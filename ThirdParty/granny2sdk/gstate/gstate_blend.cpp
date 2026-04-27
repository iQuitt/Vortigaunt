// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_blend.h"
#include "gstate_anim_utils.h"

#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_quick_vecs.h"
#include "gstate_snapshotutils.h"
#include "gstate_token_context.h"
#include "gstate_container.h"
#include "gstate_input_map.h"
#include "gstate_parameters.h"
#include "gstate_telemetry.h"

#include <string.h>
#include <float.h>
#include <limits>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE blend::blendImpl
{
    granny_bool32 PhaseLocked;
    granny_bool32 UseNeighborhood;
    granny_bool32 ChooseOnActivate;
    granny_int32  InputMapCount;
    granny_int32* InputMap;
    granny_int32  NumPoseInputs;
    granny_bool32 CircularBlend;
};

granny_data_type_definition GSTATE
blend::blendImplType[] =
{
    { GrannyBool32Member, "PhaseLocked" },
    { GrannyBool32Member, "UseNeighborhood" },
    { GrannyBool32Member, "ChooseOnActivate" }, 
    { GrannyReferenceToArrayMember, "InputMap",  GrannyInt32Type },
    { GrannyInt32Member, "NumPoseInputs" }, 
    { GrannyBool32Member, "CircularBlend" },

    { GrannyEndMember },
};

// blend is a concrete class, so we must create a slotted container
struct blend_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(blend);
};

granny_data_type_definition blend::blendTokenType[] =
{
    DECL_UID_MEMBER(blend),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(blend),

    { GrannyEndMember }
};

IMPL_CREATE_NODE_BOILERPLATE(blend);

bool GSTATE
blend::DidLoopOccur(granny_int32x OutputIdx,
                   granny_real32 AtT,
                   granny_real32 DeltaT)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    granny_real32 Param;
    granny_int32x IndexFrom;
    granny_int32x IndexTo;
    if (!ComputeBlend(AtT, OutputIdx, Param, IndexFrom, IndexTo))
        return false;

    GStateAssert(IndexFrom > 0 && IndexFrom < GetNumInputs());
    GStateAssert(IndexTo   > 0 && IndexTo   < GetNumInputs());

    INPUT_CONNECTION(IndexFrom, From);
    INPUT_CONNECTION(IndexTo, To);

    if (!FromNode || !ToNode)
    {
        // Is one of these NULL?  That simplifies things...
        if (FromNode)
        {
            return FromNode->DidLoopOccur(FromEdge, AtT, DeltaT);
        }
        else if (ToNode)
        {
            return ToNode->DidLoopOccur(FromEdge, AtT, DeltaT);
        }
        else
        {
            return false;
        }
    }
    else
    {
        if( Param < 0.5f )            
        {
            return FromNode->DidLoopOccur(FromEdge, AtT, DeltaT);
        }
        else
        {
            return ToNode->DidLoopOccur(FromEdge, AtT, DeltaT);
        }  
    }
}


void GSTATE
blend::CacheNumPoseInputs()
{
    InputMap_CacheNumPoseInputs(this, m_blendToken->NumPoseInputs);
}

GSTATE
blend::blend(token_context*               Context,
             granny_data_type_definition* TokenType,
             void*                        TokenObject,
             token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_Durations(0),
    m_LocalOffset(0),
    m_LastObservedParam(0),  // default to  "from"
    m_LastDurationFrom(-1),
    m_LastDurationTo(-1),
    m_ActivationParam(-1),
    m_blendToken(0)
{
    IMPL_INIT_FROM_TOKEN(blend);

    //If this object is created in the editor, everything is done in SetParent()
    if (EditorCreated())
    {
        AddInputImpl(ScalarEdge, "Parameter");
        granny_int32& NewInputMapping = QVecPushNewElement(m_blendToken->InputMapCount, m_blendToken->InputMap);
        NewInputMapping = 0;
    }
    else
    {
		// Create the duration caches, we'll fill it in later.
		GStateAssert(GetNumInputs() >= 3);
		m_Durations = GStateAllocArray(granny_real32, GetNumInputs()-1);
		for (int Idx = 0; Idx < GetNumInputs()-1; ++Idx)
		{
			m_Durations[Idx] = -1;
		}
    }    

    //Backwards Compatibility
    if( IsEditable() )
    {
        if( m_blendToken->InputMap == NULL )
        {
            TakeTokenOwnership();
            for( int InputIdx=0; InputIdx<GetNumInputs(); InputIdx++ )
            {
                granny_int32& NewInputMapping = QVecPushNewElement(m_blendToken->InputMapCount, m_blendToken->InputMap);
                NewInputMapping = InputIdx;
            }
        }

        if( m_blendToken->NumPoseInputs == 0 )
        {
            CacheNumPoseInputs();
        }
    }
}


GSTATE
blend::~blend()
{
    DTOR_RELEASE_TOKEN(blend);

	GStateDeallocate(m_Durations);
}

void GSTATE
blend::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(blend);

    InputMap_TakeTokenOwnership(m_blendToken->InputMap, OldToken->InputMap, m_blendToken->InputMapCount);
}

void GSTATE
blend::ReleaseOwnedToken_blend()
{
    InputMap_ReleaseOwnedToken(m_blendToken->InputMap, m_blendToken->InputMapCount);
}

void GSTATE
blend::NoteInputChange(granny_int32x InputIndex)
{
    GStateAssert(InputIndex >= 0 && InputIndex < GetNumInputs());

    parent::NoteInputChange(InputIndex);

    if (InputIndex == 0) // param
        return;

    granny_real32 Duration = -1;

    node* Node = 0;
    granny_int32x Edge  = -1;
    GetInputConnection(InputIndex, &Node, &Edge);
    if (Node != 0)
    {
        // < 0 for unsupported
        Duration = Node->GetDuration(Edge, 0);
    }

    m_Durations[InputIndex-1] = Duration;
}

bool GSTATE
blend::FillDefaultToken(granny_data_type_definition* TokenType,
                        void*                        TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares blendImpl*& Slot = // member
    GET_TOKEN_SLOT(blend);

    // Our slot in this token should be empty.
    // Create a new blend Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->PhaseLocked = false;
    Slot->UseNeighborhood = false;

    return true;
}

bool GSTATE
blend::ComputeBlend(granny_real32  AtT,
                    granny_int32x  OutputIndex,
                    granny_real32& Factor,
                    granny_int32x& IndexFrom,
                    granny_int32x& IndexTo)
{
    // We absolutely require a parameter input to do anything sensible here
    node* ParamNode = 0;
    granny_int32x ParamEdge = -1;
    GetInputConnection(0, &ParamNode, &ParamEdge);

    if (ParamNode == 0)
    {
        return false;
    }

    if( m_blendToken->NumPoseInputs == 0 )
    {
        Factor = 0;
        IndexFrom = 0 + OutputIndex;
        IndexTo = -1;
        return false;
    }
    else if( m_blendToken->NumPoseInputs == 1 )
    {
        Factor = 0;
        IndexFrom = ObtainSampleIndex(0,OutputIndex);
        IndexTo = IndexFrom;
        return true;
    }

    // By default, this goes from [0,1].  If the scalar connection supports range queries,
    // we can grab the min and max from it directly
    granny_real32 MinVal, MaxVal;
    if (ParamNode->GetScalarOutputRange(ParamEdge, &MinVal, &MaxVal) == false)
    {
        MinVal = 0;
        MaxVal = 1;
    }
    GStateAssert(MinVal <= MaxVal);

    // Sample the parameters, and clamp
    granny_real32 Value = 0.0f;
    
    if( GetChooseOnActivate() )
    {
        Value = m_ActivationParam; 
    }
    else
    {
        Value = ParamNode->SampleScalarOutput(ParamEdge, AtT, NULL);
    }

    {
        if (Value < MinVal)
            Value = MinVal;
        else if (Value > MaxVal)
            Value = MaxVal;
    }

    granny_real32 ZeroToOne;
    if (MinVal != MaxVal)
        ZeroToOne = (Value - MinVal) / (MaxVal - MinVal);
    else
        ZeroToOne = 0;

    // Handle two cases specially, 0 && 1.
    if (ZeroToOne == 0)
    {
        Factor    = 0;
        IndexFrom = ObtainSampleIndex(0,OutputIndex);
        IndexTo   = ObtainSampleIndex(1,OutputIndex);
        return true;
    }
    else if (ZeroToOne == 1)
    {
        Factor    = 1;
        if( m_blendToken->CircularBlend )
        {
            IndexFrom = ObtainSampleIndex(m_blendToken->NumPoseInputs-1,OutputIndex);
            IndexTo   = ObtainSampleIndex(0,OutputIndex);
        }
        else
        {
            IndexFrom = ObtainSampleIndex(m_blendToken->NumPoseInputs-2,OutputIndex);
            IndexTo   = ObtainSampleIndex(m_blendToken->NumPoseInputs-1,OutputIndex);
        }
        return true;
    }

    // Ok, we have legitimate work to do here...

    // The 0th is the parameter, of course
    GStateAssert(m_blendToken->NumPoseInputs >= 2);

    // Because we handled the == 1 case above, we know these should be safe indices
    granny_int32x PoseIndex = 0;
    if( m_blendToken->CircularBlend )
    {
        PoseIndex = int(ZeroToOne * m_blendToken->NumPoseInputs);
        Factor = (ZeroToOne - (PoseIndex / float(m_blendToken->NumPoseInputs))) / (1.0f / (m_blendToken->NumPoseInputs));
    }
    else
    {
        PoseIndex = int(ZeroToOne * (m_blendToken->NumPoseInputs-1));
        Factor = (ZeroToOne - (PoseIndex / float(m_blendToken->NumPoseInputs-1))) / (1.0f / (m_blendToken->NumPoseInputs-1));
    }

    IndexFrom = ObtainSampleIndex(PoseIndex,OutputIndex);
    if( m_blendToken->CircularBlend )
    {
        if( PoseIndex == m_blendToken->NumPoseInputs-1 )
        {
            IndexTo   = ObtainSampleIndex(0,OutputIndex);
        }
        else
        {
            IndexTo   = ObtainSampleIndex(PoseIndex + 1,OutputIndex);
        }
    }
    else
    {
        IndexTo   = ObtainSampleIndex(PoseIndex + 1,OutputIndex);
    }

    GStateAssert(IndexFrom < GetNumInputs());
    GStateAssert(IndexTo < GetNumInputs());

    return true;
}


granny_local_pose* GSTATE
blend::SamplePoseOutput(granny_int32x      OutputIdx,
                        granny_real32      AtT,
                        granny_real32      AllowedError,
                        granny_pose_cache* PoseCache,
                        granny_real32*     PoseWeight)
{
    GSTATE_AUTO_ZONE_FN_OR_NAME();

    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());
    GStateAssert(PoseCache);

    granny_real32 FromPoseWeight = 1.0f;
    granny_real32 ToPoseWeight = 1.0f;

    granny_real32 Param;
    granny_int32x IndexFrom;
    granny_int32x IndexTo;
    if (!ComputeBlend(AtT, OutputIdx, Param, IndexFrom, IndexTo))
        return 0;

    GStateAssert(IndexFrom > 0 && IndexFrom < GetNumInputs());
    GStateAssert(IndexTo   > 0 && IndexTo   < GetNumInputs());

    INPUT_CONNECTION(IndexFrom, From);
    INPUT_CONNECTION(IndexTo, To);

    granny_local_pose* FromPose = 0;
    granny_local_pose* ToPose   = 0;

    granny_real32 DurationFrom = m_Durations[IndexFrom - 1];
    granny_real32 DurationTo   = m_Durations[IndexTo   - 1];

    if (!FromNode || !ToNode)
    {
        // Is one of these NULL?  That simplifies things...
        if (FromNode)
            FromPose = FromNode->SamplePoseOutput(FromEdge, AtT, AllowedError, PoseCache, PoseWeight);
        else if (ToNode)
            ToPose   = ToNode->SamplePoseOutput(ToEdge, AtT, AllowedError, PoseCache, PoseWeight);

        goto HandleReturn;
    }

    // Ok, we have some real work to do.
    granny_real32 FromT, ToT;
    if (m_blendToken->PhaseLocked == false || CanPhaseLock(false) == false)
    {
        // No phase locking to do, From and To times are the same.
        FromT = AtT;
        ToT   = AtT;
    }
    else
    {
        GStateAssert(DurationTo > 0 && DurationFrom > 0);

        // Otherwise, would have early outed above in the if()
        GStateAssert(FromNode && ToNode);

        // Note the negative, we'll add these to the time to get the sample positions...
        granny_real32 FromOffset = FromNode->GetLocalTimeOffset(AtT, FromEdge);
        granny_real32 ToOffset   = ToNode->GetLocalTimeOffset(AtT, ToEdge);

        // Compute the phase lock.
        granny_real32 LastDuration = m_LastDurationFrom + m_LastObservedParam * (m_LastDurationTo - m_LastDurationFrom);
        granny_real32 Pos          = (AtT - m_LocalOffset) / LastDuration;

        if (Param != m_LastObservedParam)
        {
            // Readjust the phase offset.
            //
            // Without laying out the math too much, we know that:
            // Pos    = (AtT - OffOld) / [m_DurationFrom + OldParam(m_DurationTo - m_DurationFrom)]   (1)
            // OffNew = -(Pos * (DurationFrom + Param(To - From)) - AtT)                              (2)
            //
            // Note that you can derive (2) from replacing the Old with New in (1), and
            // holding Pos constant.

            granny_real32 NewDuration = DurationFrom + Param * (DurationTo - DurationFrom);
            granny_real32 NewOff      = -((Pos * NewDuration) - AtT);
            m_LocalOffset = NewOff;
        }

        FromT = FromOffset + Pos * DurationFrom;
        ToT   = ToOffset   + Pos * DurationTo;
    }

    // Make sure to cull one side if the param is locked to the edge
    if (Param < 1.0f)
        FromPose = FromNode->SamplePoseOutput(FromEdge, FromT, AllowedError, PoseCache, &FromPoseWeight);

    if (Param > 0.0f)
        ToPose = ToNode->SamplePoseOutput(ToEdge, ToT, AllowedError, PoseCache, &ToPoseWeight);

    // These can still be NULL, if they are connected to a blend graph that's not
    // internally wired.  Check for that here...
HandleReturn:
    m_LastObservedParam = Param;
    m_LastDurationFrom  = DurationFrom;
    m_LastDurationTo    = DurationTo;

    if (!FromPose || !ToPose)
    {
        if (FromPose)
        {
            *PoseWeight = FromPoseWeight;
            return FromPose;
        }
        else if (ToPose)
        {
            *PoseWeight = ToPoseWeight;
            return ToPose;
        }
        else
        {
            *PoseWeight = 1.0f;
            return 0;
        }
    }

    *PoseWeight = Clamp(0.0f, ((1.0f - Param) * FromPoseWeight + Param * ToPoseWeight), 1.0f);

    // Blend, and release endpose before we return
    if (m_blendToken->UseNeighborhood)
    {
        granny_model* Model = GetModelForCharacterAutomatic(GetBoundCharacter());
        GStateAssert(Model->Skeleton);
        GrannyLinearBlendNeighborhood(FromPose, FromPose, ToPose, Param, Model->Skeleton);
    }
    else
    {
        GrannyLinearBlend(FromPose, FromPose, ToPose, Param);
    }
    
    GrannyReleaseCachePose(PoseCache, ToPose);
    return FromPose;
}

bool GSTATE
blend::GetMorphChannelBindings(granny_int32x OutputIdx,
                              char const**  MeshNames,
                              granny_int32x MeshNameCount)
{
    //Inputs are: Param, Pose, Morph *, ... Pose, Morph *, so do + 1 to skip over the Parameter input and get to the 1st pose input. 
    granny_int32x InputIdx = GetInputIndexFromVisualIndex(OutputIdx + 1); 

    INPUT_CONNECTION(InputIdx, Morph);
    if (!MorphNode)
        return false;

    return MorphNode->GetMorphChannelBindings(MorphEdge, MeshNames, MeshNameCount);
}

granny_int32x GSTATE
blend::GetNumMorphChannels(granny_int32x OutputIdx)
{
    //Inputs are: Param, Pose, Morph *, ... Pose, Morph *, so do + 1 to skip over the Parameter input and get to the 1st pose input. 
    granny_int32x InputIdx = GetInputIndexFromVisualIndex(OutputIdx+1); 

    INPUT_CONNECTION(InputIdx, Morph);
    if (!MorphNode)
        return -1;

    return MorphNode->GetNumMorphChannels(MorphEdge);
}

bool GSTATE
blend::SampleMorphOutput(granny_int32x  OutputIdx,
                        granny_real32  AtT,
                        granny_real32* MorphWeights,
                        granny_int32x NumMorphWeights)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    granny_real32 Param;
    granny_int32x IndexFrom;
    granny_int32x IndexTo;

    if (!ComputeBlend(AtT, OutputIdx, Param, IndexFrom, IndexTo))
        return 0;

    INPUT_CONNECTION(IndexFrom, From);
    INPUT_CONNECTION(IndexTo, To);

    granny_real32 DurationFrom = m_Durations[IndexFrom - 1];
    granny_real32 DurationTo   = m_Durations[IndexTo   - 1];

    if (!FromNode || !ToNode)
    {
        // Is one of these NULL?  That simplifies things...
        if (FromNode)
            return FromNode->SampleMorphOutput(FromEdge, AtT, MorphWeights, NumMorphWeights);
        else if (ToNode)
            return ToNode->SampleMorphOutput(ToEdge, AtT, MorphWeights, NumMorphWeights);
        else
        {
            return 0;
        }
    }

    // Ok, we have some real work to do.
    granny_real32 FromT, ToT;
    if (m_blendToken->PhaseLocked == false || CanPhaseLock(false) == false)
    {
        // No phase locking to do, From and To times are the same.
        FromT = AtT;
        ToT   = AtT;
    }
    else
    {
        GStateAssert(DurationTo > 0 && DurationFrom > 0);

        // Otherwise, would have early outed above in the if()
        GStateAssert(FromNode && ToNode);

        // Note the negative, we'll add these to the time to get the sample positions...
        granny_real32 FromOffset = FromNode->GetLocalTimeOffset(AtT, FromEdge);
        granny_real32 ToOffset   = ToNode->GetLocalTimeOffset(AtT, ToEdge);

        // Compute the phase lock.
        granny_real32 LastDuration = m_LastDurationFrom + m_LastObservedParam * (m_LastDurationTo - m_LastDurationFrom);
        granny_real32 Pos          = (AtT - m_LocalOffset) / LastDuration;

        if (Param != m_LastObservedParam)
        {
            // Readjust the phase offset.
            //
            // Without laying out the math too much, we know that:
            // Pos    = (AtT - OffOld) / [m_DurationFrom + OldParam(m_DurationTo - m_DurationFrom)]   (1)
            // OffNew = -(Pos * (DurationFrom + Param(To - From)) - AtT)                              (2)
            //
            // Note that you can derive (2) from replacing the Old with New in (1), and
            // holding Pos constant.

            granny_real32 NewDuration = DurationFrom + Param * (DurationTo - DurationFrom);
            granny_real32 NewOff      = -((Pos * NewDuration) - AtT);
            m_LocalOffset = NewOff;
        }

        FromT = FromOffset + Pos * DurationFrom;
        ToT   = ToOffset   + Pos * DurationTo;
    }

    // Make sure to cull one side if the param is locked to the edge
    if (Param >= 1.0f)
    {
        return ToNode->SampleMorphOutput(ToEdge, ToT, MorphWeights, NumMorphWeights);
    }


    if (Param <= 0.0f)
    {
        return FromNode->SampleMorphOutput(FromEdge, FromT, MorphWeights, NumMorphWeights);
    }

    // Actually blending. Try to use the stack when possible. This isn't great. 
    // A cache mechanism like we use for poses would be better
    const int MaxChannelsForStackUsage = 8;
    granny_real32  Buffer[MaxChannelsForStackUsage] = {0};
    granny_real32* BufferPtr = &Buffer[0];
    granny_real32* DynBuffer = 0;
    if (NumMorphWeights > MaxChannelsForStackUsage)
    {
        GStateAllocZeroedArray(DynBuffer, NumMorphWeights);
        if (!DynBuffer)
            return false;

        BufferPtr = DynBuffer;
    }

    if (!FromNode->SampleMorphOutput(FromEdge, FromT, BufferPtr, NumMorphWeights) ||
        !ToNode->SampleMorphOutput(ToEdge, ToT, MorphWeights, NumMorphWeights))
    {
        GStateDeallocate(DynBuffer);
        return false;
    }

    // Blend them.
    for (int Idx = 0; Idx < NumMorphWeights; ++Idx)
    {
        MorphWeights[Idx] = MorphWeights[Idx] * Param + (BufferPtr[Idx] * (1 - Param));
    }

    GStateDeallocate(DynBuffer);
    return true;
}

granny_real32 GSTATE
blend::SampleScalarOutput(granny_int32x OutputIdx,
                              granny_real32 AtT, 
                              gstate_scalar_track_entry * TrackEntry)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    granny_real32 Param;
    granny_int32x IndexFrom;
    granny_int32x IndexTo;

    if (!ComputeBlend(AtT, OutputIdx, Param, IndexFrom, IndexTo))
        return 0;

    GStateAssert(IndexFrom > 0 && IndexFrom < GetNumInputs());
    GStateAssert(IndexTo   > 0 && IndexTo   < GetNumInputs());

    INPUT_CONNECTION(IndexFrom, From);
    INPUT_CONNECTION(IndexTo, To);

    granny_real32 DurationFrom = m_Durations[IndexFrom - 1];
    granny_real32 DurationTo   = m_Durations[IndexTo   - 1];

    if (!FromNode || !ToNode)
    {
        // Is one of these NULL?  That simplifies things...
        if (FromNode)
            return FromNode->SampleScalarOutput(FromEdge, AtT, TrackEntry);
        else if (ToNode)
            return FromNode->SampleScalarOutput(ToEdge, AtT, TrackEntry);
        else
        {
            return 0;
        }
    }

    // Ok, we have some real work to do.
    granny_real32 FromT, ToT;
    if (m_blendToken->PhaseLocked == false || CanPhaseLock(false) == false)
    {
        // No phase locking to do, From and To times are the same.
        FromT = AtT;
        ToT   = AtT;
    }
    else
    {
        GStateAssert(DurationTo > 0 && DurationFrom > 0);

        // Otherwise, would have early outed above in the if()
        GStateAssert(FromNode && ToNode);

        // Note the negative, we'll add these to the time to get the sample positions...
        granny_real32 FromOffset = FromNode->GetLocalTimeOffset(AtT, FromEdge);
        granny_real32 ToOffset   = ToNode->GetLocalTimeOffset(AtT, ToEdge);

        // Compute the phase lock.
        granny_real32 LastDuration = m_LastDurationFrom + m_LastObservedParam * (m_LastDurationTo - m_LastDurationFrom);
        granny_real32 Pos          = (AtT - m_LocalOffset) / LastDuration;

        if (Param != m_LastObservedParam)
        {
            // Readjust the phase offset.
            //
            // Without laying out the math too much, we know that:
            // Pos    = (AtT - OffOld) / [m_DurationFrom + OldParam(m_DurationTo - m_DurationFrom)]   (1)
            // OffNew = -(Pos * (DurationFrom + Param(To - From)) - AtT)                              (2)
            //
            // Note that you can derive (2) from replacing the Old with New in (1), and
            // holding Pos constant.

            granny_real32 NewDuration = DurationFrom + Param * (DurationTo - DurationFrom);
            granny_real32 NewOff      = -((Pos * NewDuration) - AtT);
            m_LocalOffset = NewOff;
        }

        FromT = FromOffset + Pos * DurationFrom;
        ToT   = ToOffset   + Pos * DurationTo;
    }

    // Make sure to cull one side if the param is locked to the edge
    if (Param >= 1.0f)
    {
        return FromNode->SampleScalarOutput(ToEdge, ToT, TrackEntry);
    }

    if (Param <= 0.0f)
    {
        return FromNode->SampleScalarOutput(FromEdge, FromT, TrackEntry);
    }

    gstate_scalar_track_entry FromTrackEntry;
    gstate_scalar_track_entry ToTrackEntry;

    if (!FromNode->SampleScalarOutput(FromEdge, FromT, &FromTrackEntry) ||
        !ToNode->SampleScalarOutput(ToEdge, ToT, &ToTrackEntry))
    {
        return false;
    }

    TrackEntry->TrackKey = 0;
    TrackEntry->TrackBoneIndex = -1;
    TrackEntry->TrackName = "";
    TrackEntry->Weight = 0.0f;

    //If blending idencal things, keep the values, otherwise let them be invalid. 
    if(FromTrackEntry.TrackKey == ToTrackEntry.TrackKey)
    {
        TrackEntry->TrackKey = FromTrackEntry.TrackKey;
    }
    if(FromTrackEntry.TrackBoneIndex == ToTrackEntry.TrackBoneIndex)
    {
        TrackEntry->TrackBoneIndex = FromTrackEntry.TrackBoneIndex;
    }
    if(_stricmp(FromTrackEntry.TrackName, ToTrackEntry.TrackName) == 0)
    {
        TrackEntry->TrackName = FromTrackEntry.TrackName;
    }

    GStateAssert(FromTrackEntry.TrackBoneIndex == ToTrackEntry.TrackBoneIndex);

    // Blend them. 
    TrackEntry->Weight = (1 - Param) * FromTrackEntry.Weight + Param * ToTrackEntry.Weight;

    return true;
}

bool GSTATE
blend::GetScalarOutputRange(granny_int32x  OutputIdx,
                                granny_real32* OutMin,
                                granny_real32* OutMax)
{
    bool HadInputs = false;
    for( int InputIdx=0; InputIdx<GetNumInputs(); InputIdx++)
    {
        node* SampleNode = 0;
        granny_int32x SampleEdge = -1;
        GetInputConnection(1, &SampleNode, &SampleEdge);

        if (SampleNode && GetInputType(InputIdx) == ScalarEdge)
        {
            HadInputs = true;

            granny_real32 MinTest = -1.0f;
            granny_real32 MaxTest = -1.0f;
            SampleNode->GetScalarOutputRange(SampleEdge, &MinTest, &MaxTest);
            if( MinTest < *OutMin )
            {
                *OutMin  = MinTest;
            }
            if( MaxTest > *OutMax )
            {
                *OutMax  = MaxTest;
            }
        }
    }

    return HadInputs;
}

bool GSTATE
blend::SampleEventOutput(granny_int32x            OutputIdx,
                             granny_real32            AtT,
                             granny_real32            DeltaT,
                             gstate_text_track_entry* EventBuffer,
                             granny_int32x const      EventBufferSize,
                             granny_int32x*           NumEvents)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    granny_real32 Param;
    granny_int32x IndexFrom;
    granny_int32x IndexTo;

    if (!ComputeBlend(AtT, OutputIdx, Param, IndexFrom, IndexTo))
        return 0;

    GStateAssert(IndexFrom > 0 && IndexFrom < GetNumInputs());
    GStateAssert(IndexTo   > 0 && IndexTo   < GetNumInputs());

    INPUT_CONNECTION(IndexFrom, From);
    INPUT_CONNECTION(IndexTo, To);

    granny_real32 DurationFrom = m_Durations[IndexFrom - 1];
    granny_real32 DurationTo   = m_Durations[IndexTo   - 1];

    if (!FromNode || !ToNode)
    {
        // Is one of these NULL?  That simplifies things...
        if (FromNode)
            return FromNode->SampleEventOutput(FromEdge, AtT, DeltaT, EventBuffer, EventBufferSize, NumEvents);
        else if (ToNode)
            return ToNode->SampleEventOutput(ToEdge, AtT, DeltaT, EventBuffer, EventBufferSize, NumEvents);
        else
        {
            return 0;
        }
    }

    // Ok, we have some real work to do.
    granny_real32 FromT, ToT;
    if (m_blendToken->PhaseLocked == false || CanPhaseLock(false) == false)
    {
        // No phase locking to do, From and To times are the same.
        FromT = AtT;
        ToT   = AtT;
    }
    else
    {
        GStateAssert(DurationTo > 0 && DurationFrom > 0);

        // Otherwise, would have early outed above in the if()
        GStateAssert(FromNode && ToNode);

        // Note the negative, we'll add these to the time to get the sample positions...
        granny_real32 FromOffset = FromNode->GetLocalTimeOffset(AtT, FromEdge);
        granny_real32 ToOffset   = ToNode->GetLocalTimeOffset(AtT, ToEdge);

        // Compute the phase lock.
        granny_real32 LastDuration = m_LastDurationFrom + m_LastObservedParam * (m_LastDurationTo - m_LastDurationFrom);
        granny_real32 Pos          = (AtT - m_LocalOffset) / LastDuration;

        if (Param != m_LastObservedParam)
        {
            // Readjust the phase offset.
            //
            // Without laying out the math too much, we know that:
            // Pos    = (AtT - OffOld) / [m_DurationFrom + OldParam(m_DurationTo - m_DurationFrom)]   (1)
            // OffNew = -(Pos * (DurationFrom + Param(To - From)) - AtT)                              (2)
            //
            // Note that you can derive (2) from replacing the Old with New in (1), and
            // holding Pos constant.

            granny_real32 NewDuration = DurationFrom + Param * (DurationTo - DurationFrom);
            granny_real32 NewOff      = -((Pos * NewDuration) - AtT);
            m_LocalOffset = NewOff;
        }

        FromT = FromOffset + Pos * DurationFrom;
        ToT   = ToOffset   + Pos * DurationTo;
    }

    // Make sure to cull one side if the param is locked to the edge
    if (Param >= 1.0f)
    {
        return ToNode->SampleEventOutput(ToEdge, ToT, DeltaT, EventBuffer, EventBufferSize, NumEvents);
    }

    if (Param <= 0.0f)
    {
        return FromNode->SampleEventOutput(FromEdge, FromT, DeltaT, EventBuffer, EventBufferSize, NumEvents);
    }

    //Maybe add a midpoint switch option for these or something too. 
    int FromEventStart = *NumEvents;
    if(!FromNode->SampleEventOutput(FromEdge, FromT, DeltaT, EventBuffer, EventBufferSize, NumEvents))
    {
        return false;
    }
    int FromEventEnd = *NumEvents;


    int ToEventStart = *NumEvents;
    if(!ToNode->SampleEventOutput(ToEdge, ToT, DeltaT, EventBuffer, EventBufferSize, NumEvents))
    {
        return false;
    }
    int ToEventEnd = *NumEvents;

    for( int FromIdx=FromEventStart; FromIdx<FromEventEnd; FromIdx++ )
    {
        EventBuffer[FromIdx].Weight *= (1.0f - Param);
    }

    for( int ToIdx=ToEventStart; ToIdx<ToEventEnd; ToIdx++ )
    {
        EventBuffer[ToIdx].Weight *= Param;
    }

    SortEventsByTimestamp(EventBuffer, *NumEvents);

    return true;
}

bool GSTATE
blend::GetNextEvent(granny_int32x            OutputIdx,
                        granny_real32            AtT,
                        gstate_text_track_entry*  Event)
{
    GS_InvalidCodePath("GetNextEvent not supported on blend nodes");
    return false;
}


bool GSTATE
blend::GetAllEvents(granny_int32x            OutputIdx,
                        gstate_text_track_entry*  EventBuffer,
                        granny_int32x const      EventBufferSize,
                        granny_int32x*           NumEvents)
{
    GS_InvalidCodePath("GetAllEvents not supported on blend nodes");
    return false;
}

bool GSTATE
blend::GetCloseEventTimes(granny_int32x  OutputIdx,
                              granny_real32  AtT,
                              char const*    TextToFind,
                              granny_real32* PreviousTime,
                              granny_real32* NextTime)
{
    GS_InvalidCodePath("GetCloseEventTimes not supported on blend nodes");
    return false;
}

bool GSTATE
blend::GetRootMotionVectors(granny_int32x  OutputIdx,
                            granny_real32  AtT,
                            granny_real32  DeltaT,
                            granny_real32* Translation,
                            granny_real32* Rotation,
                            bool Inverse)
{
    GStateAssert(OutputIdx >= 0 && OutputIdx < GetNumOutputs());

    bool RetVal = false;

    granny_real32 Param;
    granny_int32x IndexFrom;
    granny_int32x IndexTo;
    if (!ComputeBlend(AtT, OutputIdx, Param, IndexFrom, IndexTo))
        return 0;
    GStateAssert(IndexFrom > 0 && IndexFrom < GetNumInputs());
    GStateAssert(IndexTo   > 0 && IndexTo   < GetNumInputs());

    INPUT_CONNECTION(IndexFrom, From);
    INPUT_CONNECTION(IndexTo, To);

    granny_real32 DurationFrom = m_Durations[IndexFrom - 1];
    granny_real32 DurationTo   = m_Durations[IndexTo   - 1];

    if (!FromNode || !ToNode)
    {
        // Is one of these NULL?  That simplifies things...
        if (FromNode)
        {
            m_LastObservedParam = Param;
            return FromNode->GetRootMotionVectors(FromEdge, AtT, DeltaT, Translation, Rotation, Inverse);
        }
        else if (ToNode)
        {
            m_LastObservedParam = Param;
            return ToNode->GetRootMotionVectors(ToEdge, AtT, DeltaT, Translation, Rotation, Inverse);
        }

        memset(Translation, 0, sizeof(granny_real32)*3);
        memset(Rotation,    0, sizeof(granny_real32)*3);
        return false;
    }

    // TODO: !!!FACTOR THIS OUT!!!
    // Ok, we have some real work to do.
    granny_real32 FromT, FromDelta;
    granny_real32 ToT,   ToDelta;
    if (m_blendToken->PhaseLocked == false || CanPhaseLock(false) == false)
    {
        // No phase locking to do, From and To times are the same.
        FromT     = AtT;
        FromDelta = DeltaT;

        ToT     = AtT;
        ToDelta = DeltaT;
    }
    else
    {
        GStateAssert(DurationTo > 0 && DurationFrom > 0);

		// Compute the phase lock.
		granny_real32 LastDuration = m_LastDurationFrom + m_LastObservedParam * (m_LastDurationTo - m_LastDurationFrom);

        granny_real32 Pos      = (AtT - m_LocalOffset)            / LastDuration;
        granny_real32 DeltaPos = ((AtT - DeltaT) - m_LocalOffset) / LastDuration;

        if (Param != m_LastObservedParam)
        {
            // Readjust the phase offset.  (See note in SamplePoseOutput)
            granny_real32 NewDuration = DurationFrom + Param * (DurationTo - DurationFrom);
			granny_real32 NewOff      = -(Pos * NewDuration - AtT);

			m_LocalOffset = NewOff;
        }

        FromT     = Pos * DurationFrom;
        FromDelta = (Pos - DeltaPos) * DurationFrom;

        ToT     = Pos * DurationTo;
        ToDelta = (Pos - DeltaPos) * DurationTo;
    }

    granny_real32 FromTrans[3], FromRot[3];
    granny_real32 ToTrans[3], ToRot[3];

    bool FromValid = FromNode->GetRootMotionVectors(FromEdge, FromT, FromDelta, FromTrans, FromRot, Inverse);
    bool ToValid   = ToNode->GetRootMotionVectors(ToEdge, ToT, ToDelta, ToTrans, ToRot, Inverse);

    m_LastObservedParam = Param;

    if (!FromValid || !ToValid)
    {
        RetVal = FromValid || ToValid;

        if (FromValid)
        {
            memcpy(Translation, FromTrans, sizeof(granny_real32)*3);
            memcpy(Rotation, FromRot, sizeof(granny_real32)*3);
            return true;
        }
        else if (ToValid)
        {
            memcpy(Translation, ToTrans, sizeof(granny_real32)*3);
            memcpy(Rotation, ToRot, sizeof(granny_real32)*3);
            return true;
        }
    }
    else
    {
        {for (int Idx = 0; Idx < 3; ++Idx)
        {
            Translation[Idx] = FromTrans[Idx] * (1 - Param) + ToTrans[Idx] * Param;
            Rotation[Idx]    = FromRot[Idx]   * (1 - Param) + ToRot[Idx]   * Param;
        }}
        RetVal = true;
    }

    return RetVal;
}

granny_real32 GSTATE
blend::GetDuration(granny_int32x OnOutput, granny_real32 AtT)
{
    // Only return a duration if we are phase locked.  Note that we know
    // CanPhaseLock(false) doesn't modify our state....
    if (m_blendToken->PhaseLocked == false || const_cast<blend*>(this)->CanPhaseLock(false) == false)
        return -1;

    // Compute from last observed parameter
    // todo: hm, this can lag a frame, correctable?
	if (m_LastDurationTo < 0 || m_LastDurationFrom < 0)
	{
		// Bogus duration until a sample occurs...
		return 1;
	}

    return (m_LastDurationFrom + m_LastObservedParam * (m_LastDurationTo - m_LastDurationFrom));
}

void GSTATE
blend::Activate(granny_real32 AtT)
{
    parent::Activate(AtT);

    m_LocalOffset = AtT;

    // We absolutely require a parameter input to do anything sensible here
    node* ParamNode = 0;
    granny_int32x ParamEdge = -1;
    GetInputConnection(0, &ParamNode, &ParamEdge);

    container * ParamNodeContainer = GSTATE_DYNCAST(ParamNode,container);

    container * Parent = GetParent();
    if( ParamNodeContainer == Parent )
    {
        node * ScalarNode = 0;
        granny_int32x ScalarEdge = -1;
        ParamNode->GetInputConnection(ParamEdge,&ScalarNode,&ScalarEdge);

        ParamNode = ScalarNode;
        ParamEdge = ScalarEdge;
    }

    if (ParamNode != 0)
    {
        granny_int32x ParamTestEdge = ParamEdge;
        node * ParamTestNode = ParamNode;

        parameters * ParamTestNodeAsParameters = GSTATE_DYNCAST(ParamTestNode,parameters);
        while(ParamTestNode != NULL && ParamTestNodeAsParameters == NULL)
        {
            node * ScalarNode = 0;
            granny_int32x ScalarEdge = -1;
            ParamTestNode->GetInputConnection(ParamTestEdge,&ScalarNode,&ScalarEdge);
            ParamTestNode = ScalarNode;
            ParamTestEdge = ScalarEdge;
            ParamTestNodeAsParameters = GSTATE_DYNCAST(ParamTestNode,parameters);
        }

        if( ParamTestNodeAsParameters )
        {
            ParamTestNodeAsParameters->Activate(AtT);
        }
        m_ActivationParam = ParamNode->SampleScalarOutput(ParamEdge, AtT, NULL);
    }
}

bool GSTATE
blend:: CanPhaseLock(bool RefreshCache)
{
    if (RefreshCache)
    {
        for (int Idx = 1; Idx < GetNumInputs(); ++Idx)
            NoteInputChange(Idx);
    }

    for (int Idx = 1; Idx < GetNumInputs(); ++Idx)
    {
        if (m_Durations[Idx-1] < 0)
            return false;
    }

    return true;
}

void GSTATE
blend::CaptureSiblingData()
{
    parent::CaptureSiblingData();

    // Refresh the duration cache.  All of our inputs are flushed at this point.
    CanPhaseLock(true);
}


bool GSTATE
blend::GetPhaseLocked() const
{
    return m_blendToken->PhaseLocked != 0;
}

void GSTATE
blend::SetPhaseLocked(bool Lock)
{
    TakeTokenOwnership();

    m_blendToken->PhaseLocked = Lock;

    // Should reactivate sub nodes?  For now, make the caller do it...
}

bool GSTATE
blend::GetNeighborhooded() const
{
    return m_blendToken->UseNeighborhood != 0;
}

void GSTATE
blend::SetNeighborhooded(bool Neighboorhood)
{
    TakeTokenOwnership();

    m_blendToken->UseNeighborhood = Neighboorhood;
}


bool GSTATE
blend::GetChooseOnActivate() const
{
    return m_blendToken->ChooseOnActivate != 0;
}

void GSTATE
blend::SetChooseOnActivate(bool ChooseOnActivate)
{
    TakeTokenOwnership();

    m_blendToken->ChooseOnActivate = ChooseOnActivate;
}

bool GSTATE
blend::GetCircularBlend() const
{
    return m_blendToken->CircularBlend != 0;
}

void GSTATE
blend::SetCircularBlend(bool CircularBlend)
{
    TakeTokenOwnership();

    m_blendToken->CircularBlend = CircularBlend;
}

granny_int32 GSTATE
blend::GetInputIndexFromVisualIndex(granny_int32 Index) const
{
    int RemappedIndex = InputMap_GetInputIndex(Index, m_blendToken->InputMap, m_blendToken->InputMapCount);
    if( RemappedIndex >= 0 )
    {
        return RemappedIndex;
    }
    else
    {
        return Index;
    }
}

granny_int32 GSTATE
blend::ObtainSampleIndex(granny_int32 N, granny_int32 OutputIndex)
{
    return InputMap_GetNthInputIndexForOutput(N, this, m_blendToken->InputMap, m_blendToken->InputMapCount, OutputIndex);
}

granny_int32x GSTATE
blend::AddInput(node_edge_type EdgeType, char const* EdgeName)
{
    // Remember that m_Durations is 1 shorter than NumInputs
    granny_real32& NewElem = QVecPushNewElementNoCount(GetNumInputs()-1, m_Durations);
    NewElem = -1;

	granny_int32x NewInput = parent::AddInputImpl(EdgeType, EdgeName);
    GStateAssert(NewInput == (GetNumInputs()-1));

    return NewInput;
}

bool GSTATE
blend::DeleteInput(granny_int32x InputIndex)
{
    if (InputIndex == 0)
        return false;

    node_edge_type RemovedInputType = GetInputType(InputIndex);

    // We have to have at least two pose inputs
    if (RemovedInputType == PoseEdge )
    {
        if( m_blendToken->NumPoseInputs <= 2 )
        {
            return false;
        }
        m_blendToken->NumPoseInputs--;
    }

    // Remove the duration.
    QVecRemoveElementNoCount(InputIndex-1, GetNumInputs()-1, m_Durations);

    // Remove the input
    bool Result = DeleteInputImpl(InputIndex);

    // If it's a pose edge, auto-remove the corresponding morph edges
    if( RemovedInputType == PoseEdge )
    {
        if( !InputMap_DeleteInputSet(this, InputIndex, m_blendToken->InputMap, m_blendToken->InputMapCount, m_blendToken->NumPoseInputs) )
            return false;
    }

    return Result;
}

void GSTATE
blend::AddInputSet()
{
    TakeTokenOwnership();

    InputMap_AddInputSet(this, m_blendToken->InputMap, m_blendToken->InputMapCount, m_blendToken->NumPoseInputs);
}

void GSTATE
blend::AddInputsForOutput(granny_int32 OutputIdx)
{
    TakeTokenOwnership();

    InputMap_AddInputChannelToEachInputSet(this, OutputIdx, m_blendToken->InputMap, m_blendToken->InputMapCount);
}

void GSTATE
blend::DeleteInputsForOutput(granny_int32 OutputIdx)
{
    TakeTokenOwnership();

    InputMap_RemoveInputChannelFromEachInputSet(this, OutputIdx, m_blendToken->InputMap, m_blendToken->InputMapCount);
}

void GSTATE
blend::SetInputNamesForOutput(granny_int32x OutputIdx, char const* NewEdgeName)
{
    TakeTokenOwnership();

    InputMap_SetNameForEachInputInInputSet(this, OutputIdx, NewEdgeName, m_blendToken->InputMap, m_blendToken->InputMapCount);
}

void GSTATE
blend::AddInputsForParentOutputs()
{
    //Build the outputs first. Blend node bases its inputs on its outputs. 
    parent::AddOutputsForParentOutputs();

    AddInputSet();
    AddInputSet();
}

granny_int32x GSTATE
blend::AddOutput(node_edge_type EdgeType, char const* EdgeName)
{
    TakeTokenOwnership();

    int NewOutput = AddOutputImpl(EdgeType, EdgeName);
    GStateAssert(NewOutput == GetNumOutputs() - 1);

    return NewOutput;
}

bool GSTATE
blend::DeleteOutput(granny_int32x OutputIdx)
{
    if( GetOutputType(OutputIdx) == PoseEdge )
    {
        GStateAssert(false); //This shouldn't be happening. 
        return false;
    }

    return parent::DeleteOutput(OutputIdx);
}


CREATE_SNAPSHOT(blend)
{
    CREATE_WRITE_REAL32(m_LastDurationFrom);
    CREATE_WRITE_REAL32(m_LastDurationTo);
    CREATE_WRITE_REAL32(m_LocalOffset);
    CREATE_WRITE_REAL32(m_LastObservedParam);

    CREATE_PASS_TO_PARENT();
}

RESET_FROMSNAPSHOT(blend)
{
    RESET_OFFSET_TRACKING();

    RESET_READ_REAL32(m_LastDurationFrom);
    RESET_READ_REAL32(m_LastDurationTo);
    RESET_READ_REAL32(m_LocalOffset);
    RESET_READ_REAL32(m_LastObservedParam);

    RESET_PASS_TO_PARENT();
}


