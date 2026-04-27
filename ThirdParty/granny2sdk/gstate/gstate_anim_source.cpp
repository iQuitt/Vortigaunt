// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_anim_source.h"
#include "gstate_anim_utils.h"

#include "gstate_character_instance.h"
#include "gstate_container.h"
#include "gstate_node_visitor.h"
#include "gstate_quick_vecs.h"
#include "gstate_snapshotutils.h"
#include "gstate_telemetry.h"
#include "gstate_token_context.h"
#include "gstate_state_machine.h"
#include "gstate_sample_recorder.h"

#include <string.h>
#include <math.h>
#include <stdio.h>

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

static char const* s_AnimEventTrackName = "<Anim. events>";
static char const* s_LoopText           = "Looped";
static char const* s_StartedText           = "Started";


struct event_edge
{
    granny_bool32 UseAnimEvents;
    char*         TrackName;  // 0 = No connection if not "AnimationEvents"
};

struct scalar_edge
{
    char*         TrackName;  // 0 = no connection
    granny_uint32 TrackKey;
};

struct morph_edge
{
    char* MeshName;  // 0 = no connection
};

static void DefaultEventEdge(event_edge& Edge)
{
    Edge.UseAnimEvents = 0;
    Edge.TrackName = 0;
}

static void DefaultMorphEdge(morph_edge& Edge)
{
    Edge.MeshName = 0;
}

static void DefaultScalarEdge(scalar_edge& Edge)
{
    Edge.TrackName = 0;
    Edge.TrackKey = 0;
}

struct GSTATE anim_source::anim_sourceImpl
{
    animation_slot* AnimationSlot;

    granny_bool32   ClampedLooping;
    granny_real32   Speed;

    granny_int32 EventEdgeCount;
    event_edge*  EventEdges;

    granny_int32 ScalarEdgeCount;
    scalar_edge* ScalarEdges;

    granny_int32 MorphEdgeCount;
    morph_edge* MorphEdges;

    granny_bool32   AllowAnyTransition;
    granny_bool32   AllowTransitionExit;

    granny_real32   AnyTransitionDuration;
};

granny_data_type_definition EventEdgeType[] =
{
    { GrannyBool32Member, "UseAnimEvents" },
    { GrannyStringMember, "TrackName" },
    { GrannyEndMember },
};

granny_data_type_definition ScalarEdgeType[] =
{
    { GrannyStringMember, "TrackName" },
    { GrannyUInt32Member, "TrackKey" },
    { GrannyEndMember },
};

granny_data_type_definition MorphEdgeType[] =
{
    { GrannyStringMember, "MeshName" },
    { GrannyEndMember },
};

granny_data_type_definition GSTATE
anim_source::anim_sourceImplType[] =
{
    { GrannyReferenceMember, "AnimationSlot", AnimationSlotType },
    { GrannyBool32Member,    "ClampedLooping" },
    { GrannyReal32Member,    "Speed" },

    { GrannyReferenceToArrayMember, "EventEdges",  EventEdgeType  },
    { GrannyReferenceToArrayMember, "ScalarEdges", ScalarEdgeType },
    { GrannyReferenceToArrayMember, "MorphEdges",  MorphEdgeType  },

    { GrannyBool32Member,    "AllowAnyTransition" },
    { GrannyBool32Member,    "AllowTransitionExit" },

    { GrannyReal32Member,    "AnyTransitionDuration" },

    { GrannyEndMember },
};

// anim_source is a concrete class, so we must create a slotted container
struct GSTATE anim_source_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(anim_source);
};

granny_data_type_definition anim_source::anim_sourceTokenType[] =
{
    DECL_UID_MEMBER(anim_source),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(anim_source),

    { GrannyEndMember }
};

void GSTATE
anim_source::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(anim_source);

    // When we take token ownership, future code will assume that it needs to free our
    // owned pointers when they change.  Don't disappoint them.
    GStateCloneArray(m_anim_sourceToken->EventEdges,
                     OldToken->EventEdges,
                     m_anim_sourceToken->EventEdgeCount);
    {for (int Idx = 0; Idx < m_anim_sourceToken->EventEdgeCount; ++Idx)
    {
        GStateCloneString(m_anim_sourceToken->EventEdges[Idx].TrackName,
                          OldToken->EventEdges[Idx].TrackName);
    }}

    GStateCloneArray(m_anim_sourceToken->ScalarEdges,
                     OldToken->ScalarEdges,
                     m_anim_sourceToken->ScalarEdgeCount);
    {for (int Idx = 0; Idx < m_anim_sourceToken->ScalarEdgeCount; ++Idx)
    {
        GStateCloneString(m_anim_sourceToken->ScalarEdges[Idx].TrackName,
                          OldToken->ScalarEdges[Idx].TrackName);
    }}

    GStateCloneArray(m_anim_sourceToken->MorphEdges,
                     OldToken->MorphEdges,
                     m_anim_sourceToken->MorphEdgeCount);
    {for (int Idx = 0; Idx < m_anim_sourceToken->MorphEdgeCount; ++Idx)
    {
        GStateCloneString(m_anim_sourceToken->MorphEdges[Idx].MeshName,
                          OldToken->MorphEdges[Idx].MeshName);
    }}
}

void GSTATE
anim_source::ReleaseOwnedToken_anim_source()
{
    {for (int Idx = 0; Idx < m_anim_sourceToken->EventEdgeCount; ++Idx)
    {
        GStateDeallocate(m_anim_sourceToken->EventEdges[Idx].TrackName);
    }}
    GStateDeallocate(m_anim_sourceToken->EventEdges);
    {for (int Idx = 0; Idx < m_anim_sourceToken->ScalarEdgeCount; ++Idx)
    {
        GStateDeallocate(m_anim_sourceToken->ScalarEdges[Idx].TrackName);
    }}
    GStateDeallocate(m_anim_sourceToken->ScalarEdges);
    {for (int Idx = 0; Idx < m_anim_sourceToken->MorphEdgeCount; ++Idx)
    {
        GStateDeallocate(m_anim_sourceToken->MorphEdges[Idx].MeshName);
    }}
    GStateDeallocate(m_anim_sourceToken->MorphEdges);
}

IMPL_CREATE_NODE_BOILERPLATE(anim_source);

GSTATE
anim_source::anim_source(token_context*               Context,
                         granny_data_type_definition* TokenType,
                         void*                        TokenObject,
                         token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_anim_sourceToken(0),
    m_Animation(0),
    m_EventTracks(0),
    m_ScalarTracks(0),
    m_ScalarDefaults(0),
    m_ScalarBoneIndex(0),
    m_MorphTracks(0),
    m_MorphBindings(0),
    m_MorphBindingCounts(0),
    m_LocalOffset(0),
    m_ActivationEdge(0)
{
    IMPL_INIT_FROM_TOKEN(anim_source);
    {
        // Not setup yet if editor created...
        if (GetNumOutputs() >= 1)
        {
            GStateAllocZeroedStruct(m_Animation);
        }
        if (m_anim_sourceToken->EventEdgeCount != 0)
        {
            GStateAllocZeroedArray(m_EventTracks, m_anim_sourceToken->EventEdgeCount);
        }
        if (m_anim_sourceToken->ScalarEdgeCount != 0)
        {
            GStateAllocZeroedArray(m_ScalarTracks, m_anim_sourceToken->ScalarEdgeCount);
            GStateAllocZeroedArray(m_ScalarDefaults, m_anim_sourceToken->ScalarEdgeCount);
            GStateAllocZeroedArray(m_ScalarBoneIndex, m_anim_sourceToken->ScalarEdgeCount);
        }
        if (m_anim_sourceToken->MorphEdgeCount != 0)
        {
            GStateAllocZeroedArray(m_MorphTracks, m_anim_sourceToken->MorphEdgeCount);
            GStateAllocZeroedArray(m_MorphBindings, m_anim_sourceToken->MorphEdgeCount);
            GStateAllocZeroedArray(m_MorphBindingCounts, m_anim_sourceToken->MorphEdgeCount);
        }
    }

    if (EditorCreated())
    {
        // Add our default output
        AddOutput(PoseEdge, "Animation");
    }
}


GSTATE
anim_source::~anim_source()
{
    DTOR_RELEASE_TOKEN(anim_source);

    GStateDeallocate(m_Animation);
    GStateDeallocate(m_EventTracks);
    GStateDeallocate(m_MorphTracks);
    GStateDeallocate(m_MorphBindings);
    GStateDeallocate(m_MorphBindingCounts);
    GStateDeallocate(m_ScalarTracks);
    GStateDeallocate(m_ScalarDefaults);
    GStateDeallocate(m_ScalarBoneIndex);
}

bool GSTATE
anim_source::FillDefaultToken(granny_data_type_definition* TokenType,
                              void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares anim_sourceImpl*& Slot = // member
    GET_TOKEN_SLOT(anim_source);

    // Our slot in this token should be empty.
    // Create a new anim source Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->AnimationSlot  = 0;
    Slot->ClampedLooping = 0;
    Slot->Speed          = 1.0f;

    Slot->EventEdgeCount = 0;
    Slot->EventEdges     = 0;

    Slot->ScalarEdgeCount = 0;
    Slot->ScalarEdges     = 0;

    Slot->MorphEdgeCount = 0;
    Slot->MorphEdges     = 0;
    Slot->AnyTransitionDuration = 0.25f;

    return true;
}


bool GSTATE
anim_source::BindToCharacter(gstate_character_instance* Instance)
{
    if (!parent::BindToCharacter(Instance))
        return false;

    // Only output 0 can be an animation
    GStateAssert(GetNumOutputs() >= 1);
    GStateAssert(GetOutputType(0) == PoseEdge);
    GStateAssert(IsOutputExternal(0));

    // Todo: should be returning true in these cases?

    if (m_anim_sourceToken->AnimationSlot == 0)
        return true;

    granny_model* Model = GetSourceModelForCharacter(Instance);
    GStateAssert(Model);

    // We'll use this below in the event bind...

    animation_slot* AnimSlot = m_anim_sourceToken->AnimationSlot;
    if (AnimSlot == 0)
        return true;

    animation_spec* AnimSpec = GetSpecForCharacterSlot(Instance, AnimSlot);
    if (AnimSpec == 0)
    {
        return true;
    }

    source_file_ref* SourceFileRef = AnimSpec->SourceReference;
    if (SourceFileRef == 0)
        return true;

    granny_file_info* Info = SourceFileRef->SourceInfo;
    if (Info == 0)
        return true;

    // Check the animation as best we can.
    if (GS_InRange(AnimSpec->AnimationIndex, Info->AnimationCount) == false)
    {
        GStateError("[SCREENERROR]Cannot bind [%s] to node [%s]. Animation does not exist in [%s]", AnimSpec->ExpectedName, GetName(), AnimSpec->SourceReference->SourceFilename);
        return true; //Return true so it doesn't fail the whole state load just because we can't find an animation. 
    }

    const char * AnimationName = "";
    bool AnimationIndexInRange = false;
    if (GS_InRange(AnimSpec->AnimationIndex, Info->AnimationCount) == true)
    {
        granny_animation* Animation = Info->Animations[AnimSpec->AnimationIndex];
        AnimationName = Animation->Name;
        AnimationIndexInRange = true;
    }

    if (!AnimationIndexInRange || AnimSpec->ExpectedName && _stricmp(AnimSpec->ExpectedName, AnimationName) != 0)
    {
        GStateError("[SCREENERROR]Found incorrect animation index for [%s] stored in gsf. Anims in [%s] changed order since the gsf was last saved.\n", AnimSpec->ExpectedName, SourceFileRef->SourceFilename);
        GStateError("[SCREENERROR]Attempting slow search to fixup index. Re-save the gsf in the Animation Studio to avoid this.\n");

        bool FoundAnimIndex = false;
        for(int AnimIdx=0; AnimIdx<Info->AnimationCount; AnimIdx++)
        {
            if( _stricmp(Info->Animations[AnimIdx]->Name,AnimSpec->ExpectedName) == 0 )
            {
                AnimSpec->AnimationIndex = AnimIdx;
                FoundAnimIndex = true;
                break;
            }
        }

        if( !FoundAnimIndex )
        {
            GStateError("Could not find anim [%s] in [%s]. Unable to bind.",AnimSpec->ExpectedName, SourceFileRef->SourceFilename);
            return false;
        }
    }

    if (GS_InRange(AnimSpec->AnimationIndex, Info->AnimationCount) == false)
    {
        GStateError("Out of range animation index in gstate_anim_source.cpp");
        return false;
    }

    granny_animation* Animation = Info->Animations[AnimSpec->AnimationIndex];

    GStateAssert(m_Animation->Binding == 0);
    granny_int32x TrackGroupIndex = -1;
    if (GrannyFindTrackGroupForModel(Animation, Model->Name, &TrackGroupIndex))
    {
        GStateAssert(GS_InRange(TrackGroupIndex, Animation->TrackGroupCount));
        granny_track_group* TrackGroup = Animation->TrackGroups[TrackGroupIndex];

        granny_animation_binding_identifier ID = { 0 };
        ID.Animation = Animation;
        ID.SourceTrackGroupIndex = TrackGroupIndex;
        ID.OnModel = Model;

        m_Animation->Binding          = GrannyAcquireAnimationBindingFromID(&ID);
        m_Animation->AccumulationMode = GrannyAccumulationModeFromTrackGroup(TrackGroup);
    }

    // MorphTracks
    if (Animation)
    {
        for (int EdgeIdx = 1; EdgeIdx < GetNumOutputs(); ++EdgeIdx)
        {
            int const TrackSlotIdx = EdgeIdx - 1;
            GStateAssert(m_MorphTracks[TrackSlotIdx] == 0);

            if (m_anim_sourceToken->MorphEdges[TrackSlotIdx].MeshName)
            {
                GStateAssert(GetOutputType(EdgeIdx) == MorphEdge);
                GStateAssert(m_anim_sourceToken->EventEdges[TrackSlotIdx].TrackName == 0);
                GStateAssert(m_anim_sourceToken->ScalarEdges[TrackSlotIdx].TrackName == 0);

                granny_int32x TrackIdx;
                if (GrannyFindMorphTrackGroupForMesh(Animation,
                                                     m_anim_sourceToken->MorphEdges[TrackSlotIdx].MeshName,
                                                     &TrackIdx))
                {
                    m_MorphTracks[TrackSlotIdx] = Animation->TrackGroups[TrackIdx];

                    // We've found a track_group for this morph edge, let's look in the
                    // character to see if we can find a matching mesh
                    granny_mesh* FoundMesh = 0;
                    if (Instance->CharacterMeshCount != 0)
                    {
                        GStateAssert(Instance->CharacterMeshes);

                        for (int MeshIdx = 0; MeshIdx < Instance->CharacterMeshCount; ++MeshIdx)
                        {
                            if (!Instance->CharacterMeshes[MeshIdx])
                                continue;

                            if (_stricmp(Instance->CharacterMeshes[MeshIdx]->Name,
                                         m_anim_sourceToken->MorphEdges[TrackSlotIdx].MeshName) == 0)
                            {
                                if (FoundMesh != 0)
                                {
                                    GStateWarning("Duplicate mesh match found for morph edge: %s\n",
                                                  m_anim_sourceToken->MorphEdges[TrackSlotIdx].MeshName);
                                }

                                FoundMesh = Instance->CharacterMeshes[MeshIdx];
                            }
                        }
                    }

                    if (FoundMesh != 0 && FoundMesh->MorphTargetCount != 0)
                    {
                        // Ok, we need to allocate that buffer of remaps...
                        m_MorphBindingCounts[TrackSlotIdx] = FoundMesh->MorphTargetCount;
                        m_MorphBindings[TrackSlotIdx] =
                            GStateAllocArray(granny_int32, FoundMesh->MorphTargetCount);
                        for (int MorphIdx = 0; MorphIdx < FoundMesh->MorphTargetCount; ++MorphIdx)
                        {
                            m_MorphBindings[TrackSlotIdx][MorphIdx] = -1;
                            for (int VecTrackIdx = 0; VecTrackIdx < m_MorphTracks[TrackSlotIdx]->VectorTrackCount; ++VecTrackIdx)
                            {
                                if( FoundMesh->MorphTargets[MorphIdx].ScalarName == NULL )
                                {
                                    GStateError("[SCREENERROR]Morph target without a name (need to export with Morph Targets: 'look for modifier edges' option)\n");
                                    return false;
                                }
                                else if ( _stricmp(FoundMesh->MorphTargets[MorphIdx].ScalarName,
                                             m_MorphTracks[TrackSlotIdx]->VectorTracks[VecTrackIdx].Name) == 0)
                                {
                                    m_MorphBindings[TrackSlotIdx][MorphIdx] = VecTrackIdx;
                                    break;
                                }
                            }

                            /*There are valid cases where models have morph targets that aren't animated, so don't warn about it. 
                            if( m_MorphBindings[TrackSlotIdx][MorphIdx] == -1 )
                            {
                                GStateWarning("[SCREENERROR]Animation [%s] has no tracks that match morph target [%s] in model [%s %s], mesh [%s]", 
                                    m_Animation->Binding->ID.Animation->Name, 
                                    FoundMesh->MorphTargets[MorphIdx].ScalarName, 
                                    Instance->CharacterInfo->ModelNameHint,
                                    Instance->SourceModel->Name,
                                    FoundMesh->Name);
                            }
                            */
                        }
                    }
                    else if (FoundMesh == 0)
                    {
                        // Support morph tracks without mesh data embedded into GR2s
                        // Assumes each morph target track is a single, floating point vector track driving the morph channels.
                        // You must generate your own mesh/morph track binding table based on morph/mesh names to use this. 
                        m_MorphBindingCounts[TrackSlotIdx] = 1;
                        m_MorphBindings[TrackSlotIdx] = GStateAllocArray(granny_int32, 1);
                        if (m_MorphTracks[TrackSlotIdx]->VectorTrackCount == 0)
                        {
                            GStateError("[SCREENERROR] anim_source missing expected vector track for MorphTrack output.\n");
                            return false;
                        }
                        m_MorphBindings[TrackSlotIdx][0] = 0;
                    }
                }
            }
        }
    }

    // Need to bind the text tracks and scalar...
    if (TrackGroupIndex != -1)
    {
        granny_track_group* TrackGroup = Animation->TrackGroups[TrackGroupIndex];

        // Separate out the loops for these, just to keep it simple...  First text_tracks
        {for (int EdgeIdx = 1; EdgeIdx < GetNumOutputs(); ++EdgeIdx)
        {
            int const TrackSlotIdx = EdgeIdx - 1;
            GStateAssert(m_EventTracks[TrackSlotIdx] == 0);

            if (m_anim_sourceToken->EventEdges[TrackSlotIdx].TrackName)
            {
                GStateAssert(GetOutputType(EdgeIdx) == EventEdge);
                GStateAssert(m_anim_sourceToken->ScalarEdges[TrackSlotIdx].TrackName == 0);
                GStateAssert(m_anim_sourceToken->MorphEdges[TrackSlotIdx].MeshName == 0);

                granny_int32x TextTrackIdx;
                if (GrannyFindTextTrackByName(TrackGroup,
                                              m_anim_sourceToken->EventEdges[TrackSlotIdx].TrackName,
                                              &TextTrackIdx))
                {
                    m_EventTracks[TrackSlotIdx] = &TrackGroup->TextTracks[TextTrackIdx];
                }
            }
        }}

        // ...and then scalar tracks
        {for (int EdgeIdx = 1; EdgeIdx < GetNumOutputs(); ++EdgeIdx)
        {
            int const TrackSlotIdx = EdgeIdx - 1;
            GStateAssert(m_ScalarTracks[TrackSlotIdx] == 0);

            if (m_anim_sourceToken->ScalarEdges[TrackSlotIdx].TrackName)
            {
                GStateAssert(GetOutputType(EdgeIdx) == ScalarEdge);
                GStateAssert(m_anim_sourceToken->EventEdges[TrackSlotIdx].TrackName == 0);
                GStateAssert(m_anim_sourceToken->MorphEdges[TrackSlotIdx].MeshName == 0);

                granny_int32x TextTrackIdx;
                if (GrannyFindVectorTrack(TrackGroup,
                                          m_anim_sourceToken->ScalarEdges[TrackSlotIdx].TrackName,
                                          m_anim_sourceToken->ScalarEdges[TrackSlotIdx].TrackKey,
                                          &TextTrackIdx))
                {
                    m_ScalarTracks[TrackSlotIdx] = &TrackGroup->VectorTracks[TextTrackIdx].ValueCurve;

                    granny_skeleton* Skeleton = Instance->TargetModel->Skeleton;

                    m_ScalarBoneIndex[TrackSlotIdx] = -1;
                    for (int Idx = 0; Idx < Skeleton->BoneCount; ++Idx)
                    {
                        if (GrannyVectorTrackKeyForBone(Skeleton, Idx, m_anim_sourceToken->ScalarEdges[TrackSlotIdx].TrackName) == m_anim_sourceToken->ScalarEdges[TrackSlotIdx].TrackKey)
                        {
                            m_ScalarBoneIndex[TrackSlotIdx] = Idx;
                            break;
                        }
                    }
                }
            }
        }}
    }

    return true;
}


void GSTATE
anim_source::UnbindFromCharacter()
{
    parent::UnbindFromCharacter();

    // Only output 0 can be an animation
    GStateAssert(GetNumOutputs() >= 1);
    GStateAssert(GetOutputType(0) == PoseEdge);
    {
        GStateAssert(IsOutputExternal(0));
        GStateAssert(GetOutputType(0) == PoseEdge);

        if (m_Animation->Binding != 0)
        {
            GrannyReleaseAnimationBinding(m_Animation->Binding);
            m_Animation->Binding = 0;
        }
    }

    // Need to unbind the text/scalar tracks...
    {for (int EdgeIdx = 1; EdgeIdx < GetNumOutputs(); ++EdgeIdx)
    {
        // These are actually quite simple, just zero out the reference.  Note the -1 to
        // account for the fact that event edges start at 1
        m_EventTracks[EdgeIdx - 1] = 0;
        m_MorphTracks[EdgeIdx - 1] = 0;

        // Release the binding arrays...
        if (m_MorphBindings)
            GStateDeallocate(m_MorphBindings[EdgeIdx - 1]);

        m_ScalarTracks[EdgeIdx - 1] = 0;
        m_ScalarBoneIndex[EdgeIdx - 1] = -1;
    }}
}


granny_real32 GSTATE
anim_source::GetDuration(granny_int32x OnOutput,
                         granny_real32 AtT)
{
    // Duration queries can come in on any edge now.
    animation_slot* AnimSlot = m_anim_sourceToken->AnimationSlot;
    if (AnimSlot == 0)
        return -1;

    // After this point, we will return a positive duration, even if we don't have a bound
    // instance.  This is to allow proper treatment of phase-locked blends in the
    // editor...
    gstate_character_instance* Instance = GetBoundCharacter();
    if (Instance == 0)
        return 1;

    animation_spec* AnimSpec = GetSpecForCharacterSlot(Instance, AnimSlot);
    if (AnimSpec == 0 || AnimSpec->AnimationIndex < 0)
        return -1;

    if (AnimSpec->SourceReference == 0)
        return 1;

    granny_file_info* Info = AnimSpec->SourceReference->SourceInfo;
    if (Info == 0)
        return 1;

    if (AnimSpec->AnimationIndex < 0 || AnimSpec->AnimationIndex >= Info->AnimationCount)
        return 1;

    // This is validated in BindCharacter...
    return Info->Animations[AnimSpec->AnimationIndex]->Duration;
}

bool GSTATE
anim_source::GetPhaseBoundaries(granny_int32x  OnOutput,
                                granny_real32  AtT,
                                granny_real32* PhaseStart,
                                granny_real32* PhaseEnd)
{
    GStateAssert(OnOutput == 0);

    if (m_Animation == 0 || m_Animation->Binding == 0)
        return false;

    granny_real32 const Duration = m_Animation->Binding->ID.Animation->Duration;

    granny_int32x FakeLoopCount =
        (m_anim_sourceToken->ClampedLooping ? 1 : 0);
    granny_real32 LocalTime = ComputeModTime(AtT, m_anim_sourceToken->Speed,
                                             m_LocalOffset,
                                             FakeLoopCount,
                                             Duration);

    *PhaseStart = AtT - (LocalTime / m_anim_sourceToken->Speed);
    *PhaseEnd   = AtT + (Clamp(0, (Duration - LocalTime), Duration) / m_anim_sourceToken->Speed);

    return true;
}



// Input manipulation disallowed for anim_source
granny_int32x GSTATE anim_source::AddInput(node_edge_type EdgeType, char const* EdgeName)
{
    return -1;
}

bool GSTATE anim_source::DeleteInput(granny_int32x InputIndex)
{
    return false;
}

granny_int32x GSTATE
anim_source::AddOutput(node_edge_type EdgeType,
                       char const*    EdgeName)
{
    // we can only add scalar/event edges after the first one.  This can actually be an
    // assert...
    if (!(GetNumOutputs() == 0 ||
        (EdgeType == EventEdge || EdgeType == ScalarEdge || EdgeType == MorphEdge)))
    {
        return -1;
    }

    // data integrity
    GStateAssert(m_anim_sourceToken->ScalarEdgeCount == m_anim_sourceToken->EventEdgeCount);
    GStateAssert(m_anim_sourceToken->ScalarEdgeCount == m_anim_sourceToken->MorphEdgeCount);

    TakeTokenOwnership();

    int NewOutput = AddOutputImpl(EdgeType, EdgeName);
    GStateAssert(NewOutput == GetNumOutputs() - 1);
    if (NewOutput == 0)
    {
        GStateAssert(m_anim_sourceToken->AnimationSlot == 0);
        GStateAssert(m_Animation == 0);

        // This is the first and only pose edge
        m_anim_sourceToken->AnimationSlot = 0;
        m_anim_sourceToken->ClampedLooping = false;
        m_anim_sourceToken->Speed = 1.0f;

        GStateAllocZeroedStruct(m_Animation);

        return NewOutput;
    }
    else
    {
        // Todo: this is getting unwieldy
        // Need to bump the size of both the event and scalar edges, as well as the
        granny_int32 PreviousCount = m_anim_sourceToken->EventEdgeCount;

        event_edge& NewEventEdge = QVecPushNewElement(m_anim_sourceToken->EventEdgeCount,
                                                      m_anim_sourceToken->EventEdges);
        DefaultEventEdge(NewEventEdge);

        scalar_edge& NewScalarEdge = QVecPushNewElement(m_anim_sourceToken->ScalarEdgeCount,
                                                        m_anim_sourceToken->ScalarEdges);
        DefaultScalarEdge(NewScalarEdge);

        morph_edge& NewMorphEdge = QVecPushNewElement(m_anim_sourceToken->MorphEdgeCount,
                                                      m_anim_sourceToken->MorphEdges);
        DefaultMorphEdge(NewMorphEdge);

        // Dummy counts to let us use the QVec routines

        // EventTracks
        {
            granny_text_track*& NewRef = QVecPushNewElementNoCount(PreviousCount, m_EventTracks);
            NewRef = 0;
        }

        // ScalarTracks
        {
            granny_curve2*& NewRef    = QVecPushNewElementNoCount(PreviousCount, m_ScalarTracks);
            granny_real32& NewDefault = QVecPushNewElementNoCount(PreviousCount, m_ScalarDefaults);
            granny_int32& NewBoneIndex      = QVecPushNewElementNoCount(PreviousCount, m_ScalarBoneIndex);
            NewRef = 0;
            NewDefault = 0;
            NewBoneIndex = -1;
        }

        // MorphTracks
        {
            granny_track_group*& NewRef = QVecPushNewElementNoCount(PreviousCount, m_MorphTracks);
            granny_int32*& NewBinding = QVecPushNewElementNoCount(PreviousCount, m_MorphBindings);
            granny_int32& NewCount = QVecPushNewElementNoCount(PreviousCount, m_MorphBindingCounts);
            NewRef = 0;
            NewBinding = 0;
            NewCount = 0;
        }

        granny_model* Model = GetSourceModelForCharacter(GetBoundCharacter());

        //Auto-add morph track names if possible. 
        if( IsEditable() )
        {
            animation_slot*   ThisSlot   = GetAnimationSlot();
            animation_spec* ThisSpec = GetSpecForCharacterSlot(GetBoundCharacter(), ThisSlot);
            source_file_ref*  SourceRef  = (ThisSpec && ThisSpec->AnimationIndex >= 0) ? ThisSpec->SourceReference : 0;
            granny_file_info* SourceInfo = SourceRef ? SourceRef->SourceInfo     : 0;

            GStateAssert(SourceInfo == 0 ||
                (ThisSpec->AnimationIndex >= 0 &&
                ThisSpec->AnimationIndex < SourceInfo->AnimationCount));
            granny_animation * CurrentAnimation = SourceInfo ? (granny_animation*)SourceInfo->Animations[ThisSpec->AnimationIndex] : 0;

            granny_int32x TGIdx = 0;
            if (CurrentAnimation && GrannyFindTrackGroupForModel(CurrentAnimation, Model->Name, &TGIdx))
            {
                const char * MorphMeshName = GetBoundCharacter()->StateMachine->GetMorphMeshNameForOutputName(EdgeName);

                for (int CurrTGIdx = 0; CurrTGIdx < CurrentAnimation->TrackGroupCount; ++CurrTGIdx)
                {
                    if (CurrentAnimation->TrackGroups[CurrTGIdx]->Flags & GrannyTrackGroupIsMorphs)
                    {
                        const char * MorphTrackName = CurrentAnimation->TrackGroups[CurrTGIdx]->Name;
                        if( _stricmp(MorphTrackName, MorphMeshName) == 0 )
                        {
                            SetMorphTrackName(NewOutput, MorphMeshName);
                            break;
                        }
                    }
                }
            }
        }

        return NewOutput;
    }
}

bool GSTATE
anim_source::DeleteOutput(granny_int32x OutputIndex)
{
    if (OutputIndex == 0)
        return false; // cannot delete the animation

    TakeTokenOwnership();

    granny_int32x const ArrayIdx = OutputIndex - 1;

    // Subtlety: no need to rebind the character...
    GStateAssert(ArrayIdx < m_anim_sourceToken->EventEdgeCount);
    GStateAssert(ArrayIdx < m_anim_sourceToken->ScalarEdgeCount);
    GStateAssert(ArrayIdx < m_anim_sourceToken->MorphEdgeCount);

    granny_int32 PreviousCount = m_anim_sourceToken->EventEdgeCount;
    GStateAssert(PreviousCount == m_anim_sourceToken->ScalarEdgeCount);
    GStateAssert(PreviousCount == m_anim_sourceToken->MorphEdgeCount);

    // Pretty easy.  Since we don't own any of the tracks that we're pointing to, we can
    // just nuke everything.  Make sure to release the strings...
    GStateDeallocate(m_anim_sourceToken->EventEdges[ArrayIdx].TrackName);
    GStateDeallocate(m_anim_sourceToken->ScalarEdges[ArrayIdx].TrackName);
    GStateDeallocate(m_anim_sourceToken->MorphEdges[ArrayIdx].MeshName);

    QVecRemoveElement(ArrayIdx,
                      m_anim_sourceToken->EventEdgeCount,
                      m_anim_sourceToken->EventEdges);
    QVecRemoveElement(ArrayIdx,
                      m_anim_sourceToken->ScalarEdgeCount,
                      m_anim_sourceToken->ScalarEdges);
    QVecRemoveElement(ArrayIdx,
                      m_anim_sourceToken->MorphEdgeCount,
                      m_anim_sourceToken->MorphEdges);

    // events
    QVecRemoveElementNoCount(ArrayIdx, PreviousCount, m_EventTracks);

    // scalars
    QVecRemoveElementNoCount(ArrayIdx, PreviousCount, m_ScalarTracks);
    QVecRemoveElementNoCount(ArrayIdx, PreviousCount, m_ScalarDefaults);
    QVecRemoveElementNoCount(ArrayIdx, PreviousCount, m_ScalarBoneIndex);

    // morphs
    QVecRemoveElementNoCount(ArrayIdx, PreviousCount, m_MorphTracks);
    GStateDeallocate(m_MorphBindings[ArrayIdx]);
    QVecRemoveElementNoCount(ArrayIdx, PreviousCount, m_MorphBindings);
    QVecRemoveElementNoCount(ArrayIdx, PreviousCount, m_MorphBindingCounts);

    return parent::DeleteOutputImpl(OutputIndex);
}

animation_slot* GSTATE
anim_source::GetAnimationSlot()
{
    return m_anim_sourceToken->AnimationSlot;
}

bool GSTATE
anim_source::GetOutputLoopClamping() const
{
    return (m_anim_sourceToken->ClampedLooping != 0);
}

bool GSTATE
anim_source::GetAllowAnyTransition() const
{
    return (m_anim_sourceToken->AllowAnyTransition != 0);
}

granny_real32 GSTATE
anim_source::GetAnyTransitionDuration() const
{
    return m_anim_sourceToken->AnyTransitionDuration;
}

bool GSTATE
anim_source::GetAllowTransitionExit() const
{
    return (m_anim_sourceToken->AllowTransitionExit != 0);
}

granny_real32 GSTATE
anim_source::GetOutputSpeed() const
{
    return m_anim_sourceToken->Speed;
}


void GSTATE
anim_source::SetAnimationSlot(animation_slot* NewSpec)
{
    TakeTokenOwnership();

    // We need to unbind, and the rebind once the spec has been reset so we can grab our
    // animation resources, etc.
    gstate_character_instance* Instance = GetBoundCharacter();
    if (Instance)
        UnbindFromCharacter();

    m_anim_sourceToken->AnimationSlot = NewSpec;

    // We need to clear the morphs, events, and scalars for this one, they'll be invalid,
    // in general
    {
        for (int Idx = 0; Idx < m_anim_sourceToken->EventEdgeCount; ++Idx)
        {
            GStateDeallocate(m_anim_sourceToken->EventEdges[Idx].TrackName);
            m_anim_sourceToken->EventEdges[Idx].TrackName = 0;
            m_EventTracks[Idx] = 0;
        }
        for (int Idx = 0; Idx < m_anim_sourceToken->MorphEdgeCount; ++Idx)
        {
            GStateDeallocate(m_anim_sourceToken->MorphEdges[Idx].MeshName);
            m_anim_sourceToken->MorphEdges[Idx].MeshName = 0;
            m_MorphTracks[Idx] = 0;
        }
        for (int Idx = 0; Idx < m_anim_sourceToken->ScalarEdgeCount; ++Idx)
        {
            GStateDeallocate(m_anim_sourceToken->ScalarEdges[Idx].TrackName);
            m_anim_sourceToken->ScalarEdges[Idx].TrackName = 0;
            m_ScalarTracks[Idx] = 0;
            m_ScalarDefaults[Idx] = 0;
            m_ScalarBoneIndex[Idx] = -1;
        }
    }

    if (Instance)
        BindToCharacter(Instance);

    if (GetParent() && GSTATE_DYNCAST(GetParent(), blend_graph))
    {
        animation_slot*   ThisSlot   = GetAnimationSlot();
        animation_spec* ThisSpec = GetSpecForCharacterSlot(GetBoundCharacter(), ThisSlot);
        source_file_ref*  SourceRef  = (ThisSpec && ThisSpec->AnimationIndex >= 0) ? ThisSpec->SourceReference : 0;
        granny_file_info* SourceInfo = SourceRef ? SourceRef->SourceInfo     : 0;

        GStateAssert(SourceInfo == 0 ||
            (ThisSpec->AnimationIndex >= 0 &&
            ThisSpec->AnimationIndex < SourceInfo->AnimationCount));
        granny_animation * CurrentAnimation = SourceInfo ? (granny_animation*)SourceInfo->Animations[ThisSpec->AnimationIndex] : 0;

        granny_int32x TGIdx = 0;
        granny_model* Model = GetSourceModelForCharacter(GetBoundCharacter());
        
        //If the anim is just set to null, leave the outputs as-is so that we can cycle through 
        //unassigned anims without clearing them. 
        if (CurrentAnimation)
        {
            bool CheckName = Model && GrannyFindTrackGroupForModel(CurrentAnimation, Model->Name, &TGIdx);

            // Run through the edges and clear the names back to the defaults...
            for (int Idx = 1; Idx < GetNumOutputs(); ++Idx)
            {
                if( !CheckName )
                {
                    switch (GetOutputType(Idx))
                    {
                    case ScalarEdge: 
                        SetOutputName(Idx, "Scalar"); break;
                    case MorphEdge:  
                        SetOutputName(Idx, "Morph"); 
                    case EventEdge:  
                        //For morph edges, keep the input name, but just reset the morph track name. 
                        SetMorphTrackName(Idx, ""); break;
                    default: break;
                    }
                }
                else
                {
                    bool HadMatchingTrack = false;

                    switch (GetOutputType(Idx))
                    {
                    case ScalarEdge: 
                    case EventEdge: 
                        {
                            //Would be nice to preserve scalar and event edges on animation tracks too. 
                        }
                        break;
                    case MorphEdge:  
                        {
                            //Morph edges are a little different because we actually care whether the morph track
                            //name matches the track group, not whether the input name matches. 
                            const char * CurrentMorphMeshName = GetMorphTrackName(Idx);

                            if( CurrentMorphMeshName )
                            {
                                for (int CurrTGIdx = 0; CurrTGIdx < CurrentAnimation->TrackGroupCount; ++CurrTGIdx)
                                {
                                    if (CurrentAnimation->TrackGroups[CurrTGIdx]->Flags & GrannyTrackGroupIsMorphs)
                                    {
                                        const char * MorphTrackName = CurrentAnimation->TrackGroups[CurrTGIdx]->Name;
                                        if( _stricmp(MorphTrackName, CurrentMorphMeshName) == 0 )
                                        {
                                            HadMatchingTrack = true;
                                        }
                                    }
                                }
                            }

                            if( !HadMatchingTrack )
                            {
                                //Couldn't find a matching track for this animation. Try again, using the root-level canonical mesh name
                                //for this output. 
                                const char * EdgeName = GetOutputName(Idx);

                                const char * MorphMeshName = GetBoundCharacter()->StateMachine->GetMorphMeshNameForOutputName(EdgeName);

                                for (int CurrTGIdx = 0; CurrTGIdx < CurrentAnimation->TrackGroupCount; ++CurrTGIdx)
                                {
                                    if (CurrentAnimation->TrackGroups[CurrTGIdx]->Flags & GrannyTrackGroupIsMorphs)
                                    {
                                        const char * MorphTrackName = CurrentAnimation->TrackGroups[CurrTGIdx]->Name;
                                        if( _stricmp(MorphTrackName, MorphMeshName) == 0 )
                                        {
                                            SetMorphTrackName(Idx, MorphMeshName);
                                            HadMatchingTrack = true;
                                            break;
                                        }
                                    }
                                }
                            }

                            if( !HadMatchingTrack )
                            {
                                //For morph edges, keep the input name, but just reset the morph track name. 
                                SetMorphTrackName(Idx, ""); break;
                            }
                        }
                        break;
                    default: 
                        GStateAssert("Should not get here");
                        break;
                    }
                }
            }
        }
    }
}

void GSTATE
anim_source::SetOutputLoopClamping(bool Clamped)

{
    TakeTokenOwnership();

    // No need to rebind for this case...
    m_anim_sourceToken->ClampedLooping = Clamped;
}

void GSTATE
anim_source::SetAllowAnyTransition(bool Allow)
{
    TakeTokenOwnership();

    // No need to rebind for this case...
    m_anim_sourceToken->AllowAnyTransition = Allow;

    //Ensure that we have an any transition transition in our transition list. 
    if( Allow )
    {
        EnsureAnyTransition();
    }
}

void GSTATE
anim_source::SetAnyTransitionDuration(granny_real32 Duration)
{
    TakeTokenOwnership();

    // No need to rebind for this case...
    m_anim_sourceToken->AnyTransitionDuration = Duration;
}



void GSTATE
anim_source::SetAllowTransitionExit(bool Allow)
{
    TakeTokenOwnership();

    // No need to rebind for this case...
    m_anim_sourceToken->AllowTransitionExit = Allow;
}

void GSTATE
anim_source::SetOutputSpeed(granny_real32 Speed)
{
    TakeTokenOwnership();

    // No need to rebind for this case...
    m_anim_sourceToken->Speed = Speed;
}


bool GSTATE
anim_source::GetEventUseAnimEvents(granny_int32x ForOutput)
{
    GStateAssert(GetOutputType(ForOutput) == EventEdge);
    if (ForOutput < 1 || ForOutput >= GetNumOutputs())
    {
        GS_PreconditionFailed;
        return false;
    }

    // Note the -1!
    return (m_anim_sourceToken->EventEdges[ForOutput - 1].UseAnimEvents != 0) || (GetBoundCharacter() && GetBoundCharacter()->StateMachine->GetAlwaysUseAnimEvents());
}

char const* GSTATE
anim_source::GetEventTrackName(granny_int32x ForOutput)
{
    GStateAssert(GetOutputType(ForOutput) == EventEdge);
    if (ForOutput < 1 || ForOutput >= GetNumOutputs())
    {
        GS_PreconditionFailed;
        return 0;
    }

    // Note the -1!
    return m_anim_sourceToken->EventEdges[ForOutput - 1].TrackName;
}

char const* GSTATE
anim_source::GetAnimEventTrackName()
{
    return s_AnimEventTrackName;
}

bool GSTATE
anim_source::SetEventTrack(granny_int32x ForOutput,
                           bool          UseAnimEvents,
                           char const*   TrackName)
{
    GStateAssert(GetOutputType(ForOutput) == EventEdge);
    if (ForOutput < 1 || ForOutput >= GetNumOutputs())
    {
        GS_PreconditionFailed;
        return false;
    }

    TakeTokenOwnership();

    // We need to unbind, and the rebind once the spec has been reset so we can grab our
    // animation resources, etc.
    gstate_character_instance* Instance = GetBoundCharacter();
    if (Instance)
        UnbindFromCharacter();

    // Common work
    {
        int const ArrayIndex = ForOutput - 1;

        event_edge& Edge = m_anim_sourceToken->EventEdges[ArrayIndex];

        if( TrackName != 0 && strcmp(TrackName,"") == 0 )
        {
            GStateDeallocate(Edge.TrackName);
            Edge.UseAnimEvents = false;
            Edge.TrackName = 0;
        }
        else
        {
            Edge.UseAnimEvents = (UseAnimEvents ? 1 : 0);
            GStateReplaceString(Edge.TrackName, TrackName);
        }

        // Just check the track name for use anim events, the interface for this function isn't the
        // greatest for historical reasons.
        if (Edge.UseAnimEvents || (GetBoundCharacter() && GetBoundCharacter()->StateMachine->GetAlwaysUseAnimEvents()))
        {
            if (Edge.TrackName == 0 || strcmp(TrackName, GetAnimEventTrackName()) != 0)
            {
                // Log about this so we can spot errors while debugging tools.
                GStateWarning("UseAnimEvents set with suspicious track name, should be anim_source::GetAnimEventTrackName().");
            }
        }
    }

    if (Instance)
        BindToCharacter(Instance);

    return true;
}

char const* GSTATE
anim_source::GetMorphTrackName(granny_int32x  ForOutput)
{
    GStateAssert(GetOutputType(ForOutput) == MorphEdge);
    if (ForOutput < 1 || ForOutput >= GetNumOutputs())
    {
        GS_PreconditionFailed;
        return NULL;
    }

    // Note the -1!
    return m_anim_sourceToken->MorphEdges[ForOutput - 1].MeshName;
}

bool GSTATE
anim_source::SetMorphTrackName(granny_int32x ForOutput,
                               char const*   TrackName)
{
    GStateAssert(IsEditable());
    GStateAssert(GetOutputType(ForOutput) == MorphEdge);
    if (ForOutput < 1 || ForOutput >= GetNumOutputs())
    {
        GS_PreconditionFailed;
        return false;
    }

    TakeTokenOwnership();

    // We need to unbind, and the rebind once the spec has been reset so we can grab our
    // animation resources, etc.
    gstate_character_instance* Instance = GetBoundCharacter();
    if (Instance)
        UnbindFromCharacter();

    // Common work
    {
        int const ArrayIndex = ForOutput - 1;
        GStateDeallocate(m_anim_sourceToken->MorphEdges[ArrayIndex].MeshName);
        m_anim_sourceToken->MorphEdges[ArrayIndex].MeshName = 0;

        if (TrackName != 0)
            GStateCloneString(m_anim_sourceToken->MorphEdges[ArrayIndex].MeshName, TrackName);
    }

    if (Instance)
        BindToCharacter(Instance);

    return true;
}

bool GSTATE
anim_source::GetScalarTrack(granny_int32x  ForOutput,
                            char const*&   Name,
                            granny_uint32& TrackKey)
{
    GStateAssert(GetOutputType(ForOutput) == ScalarEdge);
    if (ForOutput < 1 || ForOutput >= GetNumOutputs())
    {
        GS_PreconditionFailed;
        Name = 0;
        TrackKey = 0;
        return false;
    }

    // Note the -1!
    Name = m_anim_sourceToken->ScalarEdges[ForOutput - 1].TrackName;
    TrackKey = m_anim_sourceToken->ScalarEdges[ForOutput - 1].TrackKey;
    return true;
}

bool GSTATE
anim_source::SetScalarTrack(granny_int32x       ForOutput,
                            char const*         TrackName,
                            granny_uint32 const TrackKey)
{
    GStateAssert(IsEditable());
    GStateAssert(GetOutputType(ForOutput) == ScalarEdge);
    if (ForOutput < 1 || ForOutput >= GetNumOutputs())
    {
        GS_PreconditionFailed;
        return false;
    }

    TakeTokenOwnership();

    // We need to unbind, and the rebind once the spec has been reset so we can grab our
    // animation resources, etc.
    gstate_character_instance* Instance = GetBoundCharacter();
    if (Instance)
        UnbindFromCharacter();

    // Common work
    {
        int const ArrayIndex = ForOutput - 1;
        GStateDeallocate(m_anim_sourceToken->ScalarEdges[ArrayIndex].TrackName);
        m_anim_sourceToken->ScalarEdges[ArrayIndex].TrackName = "";
        m_anim_sourceToken->ScalarEdges[ArrayIndex].TrackKey = 0;

        if (TrackName != 0)
        {
            GStateCloneString(m_anim_sourceToken->ScalarEdges[ArrayIndex].TrackName, TrackName);
            m_anim_sourceToken->ScalarEdges[ArrayIndex].TrackKey = TrackKey;
        }
    }

    if (Instance)
        BindToCharacter(Instance);

    return true;
}

granny_local_pose* GSTATE
anim_source::SamplePoseOutput(granny_int32x OutputIdx,
                              granny_real32 AtT,
                              granny_real32 AllowedError,
                              granny_pose_cache* PoseCache,
                              granny_real32* PoseWeight)
{
    GSTATE_AUTO_ZONE_FN_OR_NAME();

    GStateAssert(OutputIdx == 0);
    GStateAssert(m_Animation);
    GStateAssert(PoseCache);

    granny_model* Model = GetSourceModelForCharacter(GetBoundCharacter());
    if (!Model)
        return 0;

    granny_skeleton* Skeleton = Model->Skeleton;
    if (m_Animation->Binding == 0)
    {
        // Return null, caller is resposible for building rest pose if necessary
        return 0;
    }

    granny_local_pose* IntoPose = GrannyGetCacheLocalPose(PoseCache, Skeleton->BoneCount);
    granny_animation_binding* Binding = m_Animation->Binding;

    granny_int32x FakeLoopCount =
        (m_anim_sourceToken->ClampedLooping ? 1 : 0);
    granny_real32 const LocalSampleTime =
        ComputeModTime(AtT, m_anim_sourceToken->Speed, m_LocalOffset, FakeLoopCount,
                       Binding->ID.Animation->Duration);
    GStateAssert(LocalSampleTime >= 0);  // No negatives allowed!

    granny_sample_context Context;
    Context.UnderflowLoop = !m_anim_sourceToken->ClampedLooping;
    Context.OverflowLoop  = !m_anim_sourceToken->ClampedLooping;
    Context.LocalDuration = Binding->ID.Animation->Duration;
    Context.LocalClock    = Clamp(0.0f, LocalSampleTime, Context.LocalDuration);

    GrannySetContextFrameIndex(&Context,
                               Context.LocalClock,
                               Binding->ID.Animation->Duration,
                               Binding->ID.Animation->TimeStep);

    GrannyBeginLocalPoseAccumulation(IntoPose, 0, Skeleton->BoneCount, 0 );

    granny_int32x LODBoneCount = GrannyGetBoneCountForLOD(Skeleton, AllowedError);
    GrannyAccumulateControlledAnimation(m_Animation,
                                        &Context, 1.0f, Skeleton,
                                        0, LODBoneCount,
                                        IntoPose, AllowedError, 0);

    GrannyEndLocalPoseAccumulationLOD(IntoPose,
                                      0, Skeleton->BoneCount,
                                      0, LODBoneCount,
                                      Skeleton, AllowedError, 0);

    gstate_character_instance* CharacterInstance = GetBoundCharacter();
    if( IntoPose && CharacterInstance )
    {
        granny_retargeter * Retargeter = GetCharacterRetargeter(CharacterInstance);
        bool  RetargetAnimationSources = GrannyGetRetargetAnimationSources(CharacterInstance);
        granny_model* TargetModel = GetTargetModelForCharacter(CharacterInstance);

        if( Retargeter && RetargetAnimationSources && TargetModel )
        {
            granny_int32 BoneCount = CharacterInstance->TargetModel->Skeleton->BoneCount;
            granny_local_pose* TargPose =
                GrannyGetCacheLocalPose(PoseCache, BoneCount);

            if (GrannyRetargetPose(IntoPose, TargPose, 0, BoneCount, CharacterInstance->Retargeter))
            {
                // Success!
                GrannyReleaseCachePose(PoseCache, IntoPose);

                IntoPose = TargPose;
            }
            else
            {
                GrannyReleaseCachePose(PoseCache, TargPose);
                GrannyReleaseCachePose(PoseCache, IntoPose);
                IntoPose = 0;
            }
        }
    }

#if GSTATE_USE_SAMPLE_RECORDS
    gstate_sample_record * Record = gstate_sample_recorder::AddSampleRecord(this);
    if( Record )
    {
        GStateReplaceString(Record->Name, GetName());
        Record->Category = gstate_sample_record::eSampleRecordCategory_Pose;
        Record->Type = gstate_sample_record::eSampleRecordType_Animation;
    }
#endif //GSTATE_USE_SAMPLE_RECORDS

    return IntoPose;
}

granny_real32 GSTATE
anim_source::SampleScalarOutput(granny_int32x OutputIdx,
                                granny_real32 AtT,
                                gstate_scalar_track_entry * TrackEntry)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));
    GStateAssert(GetOutputType(OutputIdx) == ScalarEdge);

    int EdgeIdx = OutputIdx - 1;

    if( TrackEntry != NULL )
    {
        TrackEntry->TrackBoneIndex = m_ScalarBoneIndex[EdgeIdx];
        TrackEntry->Weight = 1.0f;

        GetScalarTrack(OutputIdx, TrackEntry->TrackName, TrackEntry->TrackKey);
    }

    if (m_ScalarTracks[EdgeIdx] != 0 && m_Animation && m_Animation->Binding)
    {
        granny_real32 Duration = m_Animation->Binding->ID.Animation->Duration;
        granny_int32x FakeLoopCount =
            (m_anim_sourceToken->ClampedLooping ? 1 : 0);
        granny_real32 const LocalSampleTime =
            ComputeModTime(AtT, m_anim_sourceToken->Speed, m_LocalOffset, FakeLoopCount,
                           Duration);

        granny_real32 Value;
        granny_real32 Identity = 0.0f;
        GrannyEvaluateCurveAtT(1, false,
                               !m_anim_sourceToken->ClampedLooping,
                               m_ScalarTracks[EdgeIdx],
                               !m_anim_sourceToken->ClampedLooping,
                               Duration,
                               LocalSampleTime,
                               &Value, &Identity);

#if GSTATE_USE_SAMPLE_RECORDS
        gstate_sample_record * Record = gstate_sample_recorder::AddSampleRecord(this);
        if( Record )
        {
            GStateReplaceString(Record->Name, GetName());
            GStateReplaceString(Record->Action, TrackEntry == NULL ? "NULL" : TrackEntry->TrackName);
            Record->Value = Value;
            Record->Category = gstate_sample_record::eSampleRecordCategory_Scalar;
            Record->Type = gstate_sample_record::eSampleRecordType_Animation;
        }
#endif //GSTATE_USE_SAMPLE_RECORDS

        return Value;
    }
    else
    {
        return m_ScalarDefaults[EdgeIdx];
    }
}

bool GSTATE
anim_source::GetMorphChannelBindings(granny_int32x OutputIdx,
                                     char const**  MeshNames,
                                     granny_int32x MeshNameCount)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));
    GStateAssert(GetOutputType(OutputIdx) == MorphEdge);

    if (m_MorphBindings == 0)
        return false;

    int EdgeIdx = OutputIdx - 1;
    if (m_MorphBindings[EdgeIdx] == 0)
        return false;

    if (m_MorphBindingCounts[EdgeIdx] > MeshNameCount)
    {
        GStateError("Bad size for binding name array!");
        return false;
    }

    if (m_MorphTracks[EdgeIdx] == 0)
    {
        GS_InvalidCodePath("Track should never be null if we have a binding");
        return false;
    }

    granny_int32 const* Bindings = m_MorphBindings[EdgeIdx];
    for (int Idx = 0; Idx < m_MorphBindingCounts[EdgeIdx]; ++Idx)
    {
        if (Bindings[Idx] == -1)
        {
            MeshNames[Idx] = 0;
        }
        else
        {
            GStateAssert(Bindings[Idx] >= 0 && Bindings[Idx] < m_MorphTracks[EdgeIdx]->VectorTrackCount);
            MeshNames[Idx] = m_MorphTracks[EdgeIdx]->VectorTracks[Bindings[Idx]].Name;
        }
    }

    return true;
}

granny_int32x GSTATE
anim_source::GetNumMorphChannels(granny_int32x OutputIdx)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));
    GStateAssert(GetOutputType(OutputIdx) == MorphEdge);

    int EdgeIdx = OutputIdx - 1;
    if (m_MorphBindings && m_MorphBindings[EdgeIdx])
    {
        GStateAssert(m_MorphTracks[EdgeIdx]);
        GStateAssert(m_MorphBindingCounts);
        GStateAssert(m_MorphBindingCounts[EdgeIdx] >= 0);

        return m_MorphBindingCounts[EdgeIdx];
    }

    return -1;
}

bool GSTATE
anim_source::SampleMorphOutput(granny_int32x  OutputIdx,
                               granny_real32  AtT,
                               granny_real32* MorphWeights,
                               granny_int32x NumMorphWeights)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));
    GStateAssert(GetOutputType(OutputIdx) == MorphEdge);
    GStateAssert(MorphWeights != 0);

    int EdgeIdx = OutputIdx - 1;

    if (m_MorphTracks[EdgeIdx] != 0)
    {
        if (m_MorphTracks[EdgeIdx]->VectorTrackCount > NumMorphWeights)
        {
            GStateError("Too many morph channels to sample into given buffer");
            return false;
        }

        granny_real32 Duration = m_Animation->Binding->ID.Animation->Duration;
        granny_int32x FakeLoopCount =
            (m_anim_sourceToken->ClampedLooping ? 1 : 0);
        granny_real32 const LocalSampleTime =
            ComputeModTime(AtT, m_anim_sourceToken->Speed, m_LocalOffset, FakeLoopCount,
                           Duration);

        for (int BindingIdx = 0;  BindingIdx < m_MorphBindingCounts[EdgeIdx]; ++BindingIdx)
        {
            int BoundIdx = m_MorphBindings[EdgeIdx][BindingIdx];

            if( BoundIdx >=0 )
            {
                granny_real32 Value;
                granny_real32 Identity = 0.0f;
                GrannyEvaluateCurveAtT(1, false,
                    !m_anim_sourceToken->ClampedLooping,
                    &m_MorphTracks[EdgeIdx]->VectorTracks[BoundIdx].ValueCurve,
                    !m_anim_sourceToken->ClampedLooping,
                    Duration,
                    LocalSampleTime,
                    &Value, &Identity);

                MorphWeights[BindingIdx] = Value;
            }
            else
            {
                MorphWeights[BindingIdx] = 0.0f;
            }
        }

        return true;
    }
    else
    {
        return false;
    }
}


// bool GSTATE anim_source::GetScalarOutputRange()
//   for now, use node:: default, which returns false to indicate range query is not
//   supported.

bool GSTATE
anim_source::SampleEventOutput(granny_int32x            OutputIdx,
                               granny_real32            AtT,
                               granny_real32            DeltaT,
                               gstate_text_track_entry* EventBuffer,
                               granny_int32x const      EventBufferSize,
                               granny_int32x*           NumEvents)
{
    node_edge_type t = GetOutputType(OutputIdx);
    if( t != EventEdge )
    {
        const char * name = GetOutputName(OutputIdx);
        name = name;
        int i = 0;
        i= i;
    }

    GStateCheck(GS_InRange(OutputIdx, GetNumOutputs()), return false);
    GStateCheck(GetOutputType(OutputIdx) == EventEdge, return false);

    GStateCheck(DeltaT >= 0, return false);

    int TrackIndex = OutputIdx - 1;

    bool UseAnimEvents = false;

    if (m_anim_sourceToken->EventEdges[TrackIndex].UseAnimEvents)
    {
        UseAnimEvents = true;
    }
    else if(GetBoundCharacter() && GetBoundCharacter()->StateMachine->GetAlwaysUseAnimEvents())
    {
        //Only trigger the 'always use anim events' events on the first event edge
        int FirstEventOutputIndex = -1;
        for( int TestEventIdx=0; TestEventIdx<GetNumOutputs(); TestEventIdx++ )
        {
            if( GetOutputType(TestEventIdx) == EventEdge )
            {
                FirstEventOutputIndex = TestEventIdx;
                break;
            }
        }

        if( OutputIdx == FirstEventOutputIndex )
        {
            UseAnimEvents = true;
        }
    }

    if (UseAnimEvents)
    {
        *NumEvents = 0;

        if( m_ActivationEdge )
        {
            m_ActivationEdge = false;

            if (*NumEvents < EventBufferSize)
            {
                EventBuffer[*NumEvents].TimeStamp = AtT;
                EventBuffer[*NumEvents].Text      = s_StartedText;
                EventBuffer[*NumEvents].Name      = GetName();
                EventBuffer[*NumEvents].Weight    = 1.0f;
                (*NumEvents)++;

#if GSTATE_USE_SAMPLE_RECORDS
                gstate_sample_record * Record = gstate_sample_recorder::AddSampleRecord(this);
                if( Record )
                {
                    GStateReplaceString(Record->Name, GetName());
                    GStateReplaceString(Record->Action, s_StartedText);
                    Record->Category = gstate_sample_record::eSampleRecordCategory_Event;
                    Record->Type = gstate_sample_record::eSampleRecordType_Animation;
                }
#endif //GSTATE_USE_SAMPLE_RECORDS
            }
            else
            {
                return false;
            }
        }

        if (DidLoopOccur(0, AtT, DeltaT))
        {
            if ( *NumEvents < EventBufferSize )
            {
                EventBuffer[*NumEvents].TimeStamp = AtT;
                EventBuffer[*NumEvents].Text      = s_LoopText;
                EventBuffer[*NumEvents].Name      = GetName();
                EventBuffer[*NumEvents].Weight    = 1.0f;
                (*NumEvents)++;

#if GSTATE_USE_SAMPLE_RECORDS
                gstate_sample_record * Record = gstate_sample_recorder::AddSampleRecord(this);
                if( Record )
                {
                    GStateReplaceString(Record->Name, GetName());
                    GStateReplaceString(Record->Action, s_LoopText);
                    Record->Category = gstate_sample_record::eSampleRecordCategory_Event;
                    Record->Type = gstate_sample_record::eSampleRecordType_Animation;
                }
#endif //GSTATE_USE_SAMPLE_RECORDS
            }
            else
            {
                return false;
            }
        }
    }

    if (m_EventTracks[TrackIndex] > 0)
    {
        granny_text_track* TextTrack = m_EventTracks[TrackIndex];

        // Must have this if we have a bound event track...
        GStateAssert(m_Animation->Binding != 0);
        granny_animation_binding* Binding = m_Animation->Binding;
        granny_real32 const LocalDuration = Binding->ID.Animation->Duration;

        // Braindead linear search for right now...
        granny_int32x FakeLoopCount =
            (m_anim_sourceToken->ClampedLooping ? 1 : 0);

        granny_real32 LocalEndTime = ComputeLoopedTime(AtT,
            m_anim_sourceToken->Speed,
            m_LocalOffset, FakeLoopCount,
            LocalDuration);
        granny_real32 LocalStartTime = ComputeLoopedTime(AtT - DeltaT,
            m_anim_sourceToken->Speed,
            m_LocalOffset, FakeLoopCount,
            LocalDuration);
        GStateAssert(LocalEndTime >= LocalStartTime);

        // Find the first spot
        granny_real32 Offset = int(LocalStartTime / LocalDuration) * LocalDuration;
        LocalStartTime -= Offset;
        LocalEndTime   -= Offset;
        while (LocalStartTime < 0.0f)
        {
            LocalStartTime += LocalDuration;
            LocalEndTime   += LocalDuration;
            Offset         -= LocalDuration;
        }
        GStateAssert(LocalStartTime >= 0.0f && LocalStartTime < LocalDuration);

        granny_int32x UsedEvents = 0;
        while(LocalEndTime > 0.0f)
        {
            {for (int EntryIdx = 0; EntryIdx < TextTrack->EntryCount; ++EntryIdx)
            {
                if (TextTrack->Entries[EntryIdx].TimeStamp >= LocalStartTime &&
                    TextTrack->Entries[EntryIdx].TimeStamp <= LocalEndTime)
                {
                    if (UsedEvents < EventBufferSize)
                    {
                        EventBuffer[UsedEvents].Name = GetName();
                        EventBuffer[UsedEvents].Text = TextTrack->Entries[EntryIdx].Text;
                        EventBuffer[UsedEvents].TimeStamp = TextTrack->Entries[EntryIdx].TimeStamp;
                        EventBuffer[UsedEvents].TimeStamp += (Offset - m_LocalOffset);
                        EventBuffer[UsedEvents].Weight = 1.0f;

#if GSTATE_USE_SAMPLE_RECORDS
                        gstate_sample_record * Record = gstate_sample_recorder::AddSampleRecord(this);
                        if( Record )
                        {
                            GStateReplaceString(Record->Name, GetName());
                            GStateReplaceString(Record->Action, TextTrack->Entries[EntryIdx].Text);
                            Record->Category = gstate_sample_record::eSampleRecordCategory_Event;
                            Record->Type = gstate_sample_record::eSampleRecordType_Animation;
                        }
#endif //GSTATE_USE_SAMPLE_RECORDS
                    }
                    else
                    {
                        // Bail, buffer too small.
                        return false;
                    }
                    ++UsedEvents;
                }
            }}

            LocalStartTime -= LocalDuration;
            LocalEndTime   -= LocalDuration;
            Offset         += LocalDuration;
        }

        *NumEvents += UsedEvents;
    }
    return true;
}

bool GSTATE
anim_source::GetAllEvents(granny_int32x            OutputIdx,
                          gstate_text_track_entry*  EventBuffer,
                          granny_int32x const      EventBufferSize,
                          granny_int32x*           NumEvents)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));
    GStateAssert(GetOutputType(OutputIdx) == EventEdge);

    int TrackIndex = OutputIdx - 1;

    //TODO: For now, AlwaysUseAnimEvents doesn't preclude the use of text tracks
    //for syncing like manually setting UseAnimEvents does (otherwise, you'd never be able
    //to sync animations using text tracks!
    //If GetCloseEventTimes gets fixed to handle anim events, we could make it work. 
    //This is actually kinda broken to show this here, because syncing on anim events isn't supported. 
    if (m_anim_sourceToken->EventEdges[TrackIndex].UseAnimEvents)
    {
        if (EventBufferSize > 0)
        {
            EventBuffer[0].Name = GetName();
            EventBuffer[0].TimeStamp = 0;
            EventBuffer[0].Text = s_LoopText;
            EventBuffer[0].Weight = 1.0f;
            *NumEvents = 1;
        }

        return (EventBufferSize > 0);
    }
    else
    {
        if (m_EventTracks[TrackIndex] == 0)
        {
            *NumEvents = 0;
            return true;
        }

        // Really easy at this level...
        granny_text_track* TextTrack = m_EventTracks[TrackIndex];

        // Enough space?
        if (TextTrack->EntryCount > EventBufferSize)
            return false;

        for( granny_int32x EventIdx=0; EventIdx<TextTrack->EntryCount; EventIdx++ )
        {
            EventBuffer[EventIdx].Name = GetName();
            EventBuffer[EventIdx].Text = TextTrack->Entries[EventIdx].Text;
            EventBuffer[EventIdx].TimeStamp = TextTrack->Entries[EventIdx].TimeStamp;
            EventBuffer[EventIdx].Weight = 1.0f;
        }

        *NumEvents = FilterDuplicateEvents(EventBuffer, 0, TextTrack->EntryCount);
    }

    return true;
}

bool GSTATE
anim_source::GetCloseEventTimes(granny_int32x  OutputIdx,
                                granny_real32  AtT,
                                char const*    TextToFind,
                                granny_real32* PrevTime,
                                granny_real32* NextTime)
{
    GStateAssert(GS_InRange(OutputIdx, GetNumOutputs()));
    GStateAssert(GetOutputType(OutputIdx) == EventEdge);

    int TrackIndex = OutputIdx - 1;

    //Note: we don't consider GetAlwaysUseAnimEvents here. We're just going to ignore those for syncing if turned on. 
    if (m_anim_sourceToken->EventEdges[TrackIndex].UseAnimEvents)
    {
        // todo: for sync?
        return false;
    }

    if (m_EventTracks[TrackIndex] == 0)
        return false;

    granny_text_track* TextTrack = m_EventTracks[TrackIndex];

    // Must have this if we have a bound event track...
    GStateAssert(m_Animation->Binding != 0);
    granny_animation_binding* Binding = m_Animation->Binding;

    // Braindead linear search for right now...
    granny_int32x FakeLoopCount =
        (m_anim_sourceToken->ClampedLooping ? 1 : 0);

    granny_real32 LocalTime = ComputeLoopedTime(AtT,
                                                m_anim_sourceToken->Speed,
                                                m_LocalOffset, FakeLoopCount,
                                                Binding->ID.Animation->Duration);

    // Find the first spot
    granny_real32 const LocalDuration = Binding->ID.Animation->Duration/* / m_anim_sourceToken->Speed*/;
    granny_real32 Offset = int(LocalTime / LocalDuration) * LocalDuration;
    LocalTime -= Offset;
    while (LocalTime < 0.0f)
    {
        LocalTime += LocalDuration;
        Offset    -= LocalDuration;
    }
    if (!(LocalTime < LocalDuration))
    {
        // This can happen when the localtime is something super tiny negative like -1e-8.  It basically means that t = 0;
        LocalTime = 0;
        Offset   += LocalDuration;
    }
    GStateAssert(LocalTime >= 0.0f && LocalTime < LocalDuration);

    // Move this out of bounds...
    *PrevTime = AtT - 3*LocalDuration;

    // Double loop, just for simplicity
    // todo: do this in one loop, no need for this many strcmps...
    bool FoundNext = false;
    bool FoundPrev = false;
    {for (int Idx = 0; Idx < TextTrack->EntryCount; ++Idx)
    {
        granny_text_track_entry const& Entry = TextTrack->Entries[Idx];

        granny_real32 RealStamp = (Entry.TimeStamp + Offset - m_LocalOffset) / m_anim_sourceToken->Speed;

        if (RealStamp >= AtT)
        {
            if (!FoundNext && strcmp(Entry.Text, TextToFind) == 0)
            {
                *NextTime = RealStamp;
                FoundNext = true;
            }
        }

        if (RealStamp < AtT && RealStamp > *PrevTime)
        {
            if (strcmp(Entry.Text, TextToFind) == 0)
            {
                *PrevTime = RealStamp;
                FoundPrev = true;
            }
        }
    }}

    // Bail if we have both...
    if (FoundNext && FoundPrev)
        return true;

    {for (int Idx = 0; Idx < TextTrack->EntryCount; ++Idx)
    {
        granny_text_track_entry const& Entry = TextTrack->Entries[Idx];

        granny_real32 RealNextStamp = (Entry.TimeStamp + Binding->ID.Animation->Duration + Offset - m_LocalOffset) / m_anim_sourceToken->Speed;
        granny_real32 RealPrevStamp = (Entry.TimeStamp - Binding->ID.Animation->Duration + Offset - m_LocalOffset) / m_anim_sourceToken->Speed;

        if (RealNextStamp >= AtT)
        {
            if (!FoundNext && strcmp(Entry.Text, TextToFind) == 0)
            {
                *NextTime = RealNextStamp;
                FoundNext = true;
            }
        }

        if (RealPrevStamp < AtT && RealPrevStamp > *PrevTime)
        {
            if (strcmp(Entry.Text, TextToFind) == 0)
            {
                *PrevTime = RealPrevStamp;
                FoundPrev = true;
            }
        }
    }}

    GStateAssert(FoundNext == FoundPrev);
    GStateAssert(FoundNext == false ||
                 (*PrevTime < AtT && *NextTime >= AtT));
    return FoundNext;
}


bool GSTATE
anim_source::GetRootMotionVectors(granny_int32x  OutputIdx,
                                  granny_real32  AtT,
                                  granny_real32  DeltaT,
                                  granny_real32* ResultTranslation,
                                  granny_real32* ResultRotation,
                                  bool           Inverse)
{
    GStateAssert(OutputIdx == 0);
    GStateAssert(GetBoundCharacter());

    if( DeltaT < 0 )
        DeltaT = 0.0f;

    if (!ResultTranslation || !ResultRotation)
    {
        GS_PreconditionFailed;
        return false;
    }

    // Unlike the controlled animation version of this, we /do/ need to ensure that the
    // vectors are zero filled before we start
    memset(ResultTranslation, 0, sizeof(granny_real32) * 3);
    memset(ResultRotation,    0, sizeof(granny_real32) * 3);

    GStateAssert(GetSourceModelForCharacter(GetBoundCharacter()));
    GStateAssert(GetSourceModelForCharacter(GetBoundCharacter())->Skeleton);

    if (m_Animation->Binding == 0 ||
        m_Animation->AccumulationMode == GrannyNoAccumulation)
    {
        // Nothing to accumulate here...
        return true;
    }

    granny_animation_binding* Binding = m_Animation->Binding;
    granny_real32 const LocalDuration = Binding->ID.Animation->Duration;

    granny_int32x FakeLoopCount =
        (m_anim_sourceToken->ClampedLooping ? 1 : 0);

    granny_real32 LocalEndTime = ComputeLoopedTime(AtT,
                                                   m_anim_sourceToken->Speed,
                                                   m_LocalOffset, FakeLoopCount,
                                                   LocalDuration);
    granny_real32 LocalStartTime = ComputeLoopedTime(AtT - DeltaT,
                                                     m_anim_sourceToken->Speed,
                                                     m_LocalOffset, FakeLoopCount,
                                                     LocalDuration);

    bool   FlipResult = false;
    if (LocalStartTime > LocalEndTime)
    {
        granny_real32 Temp    = LocalEndTime;
        LocalEndTime   = LocalStartTime;
        LocalStartTime = Temp;
        FlipResult = true;
    }

    granny_bound_transform_track* BoundTrack = Binding->TrackBindings;
    if(BoundTrack == 0)
        return false;

    granny_transform_track const* SourceTrack = BoundTrack->SourceTrack;
    if(SourceTrack == 0)
        return false;

    granny_real32 IgnoreTotalWeight = 0.0f;
    GrannyAccumulateControlledAnimationMotionVectors(m_Animation,
                                                     LocalStartTime, LocalEndTime, LocalDuration,
                                                     1.0f, FlipResult,
                                                     BoundTrack, SourceTrack,
                                                     &IgnoreTotalWeight,
                                                     (granny_real32*)ResultTranslation,
                                                     (granny_real32*)ResultRotation);
    return true;
}


void GSTATE
anim_source::Activate(granny_real32 AtT)
{
    parent::Activate(AtT);

    granny_animation_binding* Binding = m_Animation->Binding;
    if (Binding == 0)
        return;

    // Capture this for looping calcs...
    granny_real32 Speed       = m_anim_sourceToken->Speed;

    // Todo: this triggers false loop reports when speed != 1, because it ends up generating -epsilon values instead of 0. 
    // Make m_LocalOffset not pre-multiply by speed, and handle speed * at callers. 
    granny_real32 ImpliedTime = (AtT * Speed);
    m_LocalOffset             = -ImpliedTime;

    // Animations playing backwards need to start
    // from the beginning, but it only matters for one shots...
    if (m_anim_sourceToken->ClampedLooping && Speed < 0)
    {
        m_LocalOffset += GetDuration(0, AtT);
    }

    m_ActivationEdge = true;
}

granny_real32 GSTATE
anim_source::GetLocalTimeOffset(granny_real32 AtT, granny_int32x OnOutput)
{
    // This returns the 0 point of the animation node, not the local offset, which is
    // speed compensated.
    if (m_anim_sourceToken->Speed != 0)
        return -m_LocalOffset / m_anim_sourceToken->Speed;
    else
        return 0;
}

void GSTATE
anim_source::Synchronize(granny_int32x OnOutput,
                         granny_real32 AtT,
                         granny_real32 ReferenceStart,
                         granny_real32 ReferenceEnd,
                         granny_real32 LocalStart,
                         granny_real32 LocalEnd)
{
    GStateAssert(OnOutput == 0);

    granny_animation_binding* Binding = m_Animation->Binding;
    if (Binding == 0)
        return;

    GStateAssert(ReferenceStart <= AtT);
    GStateAssert(ReferenceEnd   >= AtT);
    GStateAssert(LocalStart     <= AtT);
    GStateAssert(LocalEnd       >= AtT);

    granny_real32 const RefDuration   = ReferenceEnd - ReferenceStart;
    granny_real32 const LocalDuration = LocalEnd - LocalStart;
    GStateAssert(RefDuration > 0);   // implied by above, but check anyways
    GStateAssert(LocalDuration > 0);

    granny_real32 const Frac = (AtT - ReferenceStart) / RefDuration;

    // Graphically, we have something like this:  (' = Local)
    //
    //       s          t       e
    //    s'            t e'
    //
    // where of course e' and s' might be in different positions w.r.t. s and e.  We need
    // to adjust the local offset so that the s' and e' fraction after adjustment exactly
    // matches the fraction from the reference.

    granny_real32 Adjustment = AtT - (LocalStart + (Frac * (LocalDuration)));
    m_LocalOffset -= Adjustment * m_anim_sourceToken->Speed;
}


bool GSTATE
anim_source::DidLoopOccur(granny_int32x OnOutput,
                          granny_real32 AtT,
                          granny_real32 DeltaT)
{
    GStateAssert(OnOutput >= 0 && OnOutput < GetNumOutputs());

    if (m_Animation->Binding == 0)
        return false;

    granny_animation_binding* Binding = m_Animation->Binding;
    granny_real32 Duration = Binding->ID.Animation->Duration;
    granny_real32 Speed = m_anim_sourceToken->Speed;

    // note the lack of clamping here...
    granny_real32 LocalStartTime = ComputeRawTime(AtT - DeltaT, Speed, m_LocalOffset, 0, Duration);

    /*This is wrong, but the origin of it is unkown. Removing it. 
    if( Speed > 0.0f && LocalStartTime < 0.0f )
    {
        LocalStartTime = 0.0f;
    }*/

    granny_real32 LocalEndTime   = ComputeRawTime(AtT,          Speed, m_LocalOffset, 0, Duration);

    // If time is running backwards, flip the times so we can use the same detection code.
    // Note that the slightly priviledged t=0 case is at t=Duration for neg speed
    // animations.
    if (LocalStartTime > LocalEndTime)
    {
        LocalStartTime = -(LocalStartTime - Duration);
        if( LocalStartTime < 0.0f )
        {
            LocalStartTime = 0.0f;
        }
        LocalEndTime   = -(LocalEndTime   - Duration);
    }

    // bring into positive range
    granny_real32 StartTime = LocalStartTime;
    granny_real32 EndTime   = LocalEndTime;
    if (StartTime < 0)
    {
        // Bring both numbers into pos range...
        granny_real32 Adjust = (int(fabs(StartTime) / Duration) + 1) * Duration;
        StartTime += Adjust;
        EndTime   += Adjust;
    }
    GStateAssert(StartTime <= EndTime);
    GStateAssert(StartTime >= 0);
    GStateAssert(EndTime   >= 0);

    StartTime -= (granny_real32)fmod(StartTime, Duration);
    EndTime   -= (granny_real32)fmod(EndTime,   Duration);
    if ((EndTime - StartTime) > Duration/2)
    {
        return true;
    }

    return false;
}

void GSTATE
anim_source::NoteAnimationSlotDeleted(animation_slot* Slot)
{
    GStateAssert(Slot != 0);

    parent::NoteAnimationSlotDeleted(Slot);

    if (m_anim_sourceToken->AnimationSlot == Slot)
    {
        TakeTokenOwnership();

        m_anim_sourceToken->AnimationSlot = 0;
    }
}

void GSTATE
anim_source::RemapAnimationSlots(animation_slot** NewSlots, int NewSlotCount)
{
    if (m_anim_sourceToken->AnimationSlot)
    {
        TakeTokenOwnership();

        animation_slot* OldSlot = m_anim_sourceToken->AnimationSlot;
		if( OldSlot != NULL )
		{
	        const char * OldSlotName = OldSlot->AnimSlotName;

	        //Try to see if the slot is where it was before. This is the common case, and it'll help with consistency if there are duplicate slot names. 
	        if( OldSlot->AnimSlotIndex >= 0 && OldSlot->AnimSlotIndex < NewSlotCount )
	        {
	            if( NewSlots[OldSlot->AnimSlotIndex] )
	            {
	                const char * NewSlotName = NewSlots[OldSlot->AnimSlotIndex]->AnimSlotName;
	                if(_stricmp(OldSlotName,NewSlotName) == 0)
	                {
	                    SetAnimationSlot(NewSlots[OldSlot->AnimSlotIndex]);
	                    return;
	                }
	            }
	        }

	        //Try to find a matching animation slot name
	        for( int NewSlotIdx=0; NewSlotIdx<NewSlotCount; ++NewSlotIdx )
	        {
	            if( NewSlots[NewSlotIdx] )
	            {
	                const char * NewSlotName = NewSlots[NewSlotIdx]->AnimSlotName;
	                if(_stricmp(OldSlotName,NewSlotName) == 0)
	                {
	                    SetAnimationSlot(NewSlots[NewSlotIdx]);
	                    return;
	                }
	            }
	        }

	        SetAnimationSlot(NULL);
		}
    }
}

void GSTATE
anim_source::FixTokenEdges(anim_source_token* OuterToken)
{
    anim_sourceImpl* Token = OuterToken->anim_sourceToken;

    // Seriously non-ideal if this allocates, issue a warning about it.
    if (Token->MorphEdgeCount != Token->ScalarEdgeCount)
    {
        GStateWarning("Detected old anim_source token, fixing, but this will LEAK MEMORY.\n"
                      "  use the preprocessor or Animation Studio to update this file\n");

        // This should only happen when we have non-zero edge counts...
        GStateAssert(Token->ScalarEdgeCount != 0);

        Token->MorphEdgeCount = Token->ScalarEdgeCount;
        GStateAllocZeroedArray(Token->MorphEdges, Token->ScalarEdgeCount);
    }
}

void GSTATE
anim_source::ResetEventEdges(anim_source_token* OuterToken)
{
    anim_sourceImpl* Token = OuterToken->anim_sourceToken;
    for( int i=0; i<Token->EventEdgeCount; i++ )
    {
        if( Token->EventEdges[i].UseAnimEvents )
        {
            Token->EventEdges[i].UseAnimEvents = false;
            Token->EventEdges[i].TrackName = 0;
        }
    }
}


CREATE_SNAPSHOT(anim_source)
{
    CREATE_WRITE_REAL32(m_LocalOffset);
    CREATE_PASS_TO_PARENT();
}

RESET_FROMSNAPSHOT(anim_source)
{
    RESET_OFFSET_TRACKING();
    RESET_READ_REAL32(m_LocalOffset);
    RESET_PASS_TO_PARENT();
}


