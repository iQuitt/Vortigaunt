// ========================================================================
// $Notice: $
// ========================================================================
#include "gstate_two_param_blend.h"

#include "gstate_anim_utils.h"
#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_quick_vecs.h"
#include "gstate_snapshotutils.h"
#include "gstate_telemetry.h"
#include "gstate_token_context.h"

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include <string.h>
#include <math.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

static const float cNotSetup = -1e38f;

struct two_param_pt
{
    granny_real32 x;
    granny_real32 y;
};

static granny_data_type_definition TwoParamPtType[] =
{
    { GrannyReal32Member, "x" },
    { GrannyReal32Member, "y" },
    { GrannyEndMember }
};

struct GSTATE two_param_blend::two_param_blendImpl
{
    granny_int32     AnimationSlotCount;
    animation_slot** AnimationSlots;

    granny_int32     ParamPointCount; // must match animation slot count
    two_param_pt*    ParamPoints;

    granny_int32     IndexCount;  // may be 0 for no triangulation
    granny_int16*    Indices;
};

granny_data_type_definition GSTATE
two_param_blend::two_param_blendImplType[] =
{
    { GrannyArrayOfReferencesMember, "AnimationSlots", AnimationSlotType },
    { GrannyReferenceToArrayMember,  "ParamPoints",    TwoParamPtType },
    { GrannyReferenceToArrayMember,  "Indices",        GrannyInt16Type },
    { GrannyEndMember },
};

// two_param_blend is a concrete class, so we must create a slotted container
struct two_param_blend_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(two_param_blend);
};

granny_data_type_definition two_param_blend::two_param_blendTokenType[] =
{
    DECL_UID_MEMBER(two_param_blend),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(two_param_blend),

    { GrannyEndMember }
};

void GSTATE
two_param_blend::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(two_param_blend);

    GStateCloneArray(m_two_param_blendToken->AnimationSlots,
                     OldToken->AnimationSlots,
                     m_two_param_blendToken->AnimationSlotCount);

    GStateCloneArray(m_two_param_blendToken->ParamPoints,
                     OldToken->ParamPoints,
                     m_two_param_blendToken->ParamPointCount);

    GStateCloneArray(m_two_param_blendToken->Indices,
                     OldToken->Indices,
                     m_two_param_blendToken->IndexCount);
}

void GSTATE
two_param_blend::ReleaseOwnedToken_two_param_blend()
{
    GStateDeallocate(m_two_param_blendToken->AnimationSlots);
    GStateDeallocate(m_two_param_blendToken->ParamPoints);
    GStateDeallocate(m_two_param_blendToken->Indices);
    m_two_param_blendToken->AnimationSlotCount = 0;
    m_two_param_blendToken->ParamPointCount    = 0;
    m_two_param_blendToken->IndexCount         = 0;
}

IMPL_CREATE_NODE_BOILERPLATE(two_param_blend);

GSTATE
two_param_blend::two_param_blend(token_context*               Context,
                                 granny_data_type_definition* TokenType,
                                 void*                        TokenObject,
                                 token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_two_param_blendToken(0),
    m_Animations(0),
    m_LocalOffset(0),
    m_LastTime(-1)
{
    m_Indices[0] = m_Indices[1] = m_Indices[2] = -1;
    m_Weights[0] = m_Weights[1] = m_Weights[2] = 0;

    IMPL_INIT_FROM_TOKEN(two_param_blend);
    {
        // Not setup yet if editor created...
        if (m_two_param_blendToken->AnimationSlotCount != 0)
        {
            GStateAllocZeroedArray(m_Animations, m_two_param_blendToken->AnimationSlotCount);
            for (int Idx = 0; Idx < m_two_param_blendToken->AnimationSlotCount; ++Idx)
            {
                GStateAllocZeroedStruct(m_Animations[Idx]);
            }
        }
    }

    if (EditorCreated())
    {
        // Add our default output
        AddInputImpl(ScalarEdge, "ParamX");
        AddInputImpl(ScalarEdge, "ParamY");
        AddOutputImpl(PoseEdge,  "Animation");
    }
}


GSTATE
two_param_blend::~two_param_blend()
{
    // Get rid of this first, we need the slot count...
    for (int Idx = 0; Idx < m_two_param_blendToken->AnimationSlotCount; ++Idx)
    {
        GStateDeallocate(m_Animations[Idx]);
    }
    GStateDeallocate(m_Animations);

    DTOR_RELEASE_TOKEN(two_param_blend);
}

bool GSTATE
two_param_blend::FillDefaultToken(granny_data_type_definition* TokenType,
                                  void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares two_param_blendImpl*& Slot = // member
    GET_TOKEN_SLOT(two_param_blend);

    // Our slot in this token should be empty.
    // Create a new anim source Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    // All arrays zeroed

    return true;
}


bool GSTATE
two_param_blend::BindToCharacter(gstate_character_instance* Instance)
{
    if (!parent::BindToCharacter(Instance))
        return false;

    // Only one output allowed
    GStateAssert(GetNumOutputs() == 1);
    GStateAssert(GetOutputType(0) == PoseEdge);
    GStateAssert(IsOutputExternal(0));

    // Todo: should be returning true in these cases?
    if (m_two_param_blendToken->AnimationSlots == 0)
        return true;

    granny_model* Model = GetSourceModelForCharacter(Instance);
    GStateAssert(Model);

    // We'll use this below in the event bind...
    for (int Idx = 0; Idx < m_two_param_blendToken->AnimationSlotCount; ++Idx)
    {
        GStateAssert(m_Animations && m_Animations[Idx]->Binding == 0);

        animation_slot* AnimSlot = m_two_param_blendToken->AnimationSlots[Idx];
        if (AnimSlot == 0)
            continue;

        animation_spec* AnimSpec = GetSpecForCharacterSlot(Instance, AnimSlot);
        if (AnimSpec == 0)
            continue;

        source_file_ref* SourceFileRef = AnimSpec->SourceReference;
        if (SourceFileRef == 0)
            continue;

        granny_file_info* Info = SourceFileRef->SourceInfo;
        if (Info == 0)
            continue;

        // Check the animation as best we can.
        if (GS_InRange(AnimSpec->AnimationIndex, Info->AnimationCount) == false)
        {
            GStateError("Out of range animation index in gstate_two_param_blend.cpp");
            continue;
        }

        granny_animation* Animation = Info->Animations[AnimSpec->AnimationIndex];
        if (AnimSpec->ExpectedName && _stricmp(AnimSpec->ExpectedName, Animation->Name) != 0)
        {
            GStateError("Found incorrect name in BindCharacter\n");
            GStateError("  expected ('%s', found '%s')\n", AnimSpec->ExpectedName, Animation->Name);
            return false;
        }

        granny_int32x TrackGroupIndex = -1;
        if (GrannyFindTrackGroupForModel(Animation, Model->Name, &TrackGroupIndex))
        {
            GStateAssert(GS_InRange(TrackGroupIndex, Animation->TrackGroupCount));
            granny_track_group* TrackGroup = Animation->TrackGroups[TrackGroupIndex];

            granny_animation_binding_identifier ID = { 0 };
            ID.Animation = Animation;
            ID.SourceTrackGroupIndex = TrackGroupIndex;
            ID.OnModel = Model;

            m_Animations[Idx]->Binding          = GrannyAcquireAnimationBindingFromID(&ID);
            m_Animations[Idx]->AccumulationMode = GrannyAccumulationModeFromTrackGroup(TrackGroup);
        }
    }

    return true;
}

void GSTATE
two_param_blend::NoteAnimationSlotDeleted(animation_slot* Slot)
{
    GStateAssert(Slot != 0);

    parent::NoteAnimationSlotDeleted(Slot);

    TakeTokenOwnership();
    for (int Idx = 0; Idx < m_two_param_blendToken->AnimationSlotCount; ++Idx)
    {
        if (m_two_param_blendToken->AnimationSlots[Idx] == Slot)
            m_two_param_blendToken->AnimationSlots[Idx] = 0;
    }
}

void GSTATE
two_param_blend::RemapAnimationSlots(animation_slot** NewSlots)
{
    TakeTokenOwnership();
    
    for (int Idx = 0; Idx < m_two_param_blendToken->AnimationSlotCount; ++Idx)
    {
        if (m_two_param_blendToken->AnimationSlots[Idx])
        {
            m_two_param_blendToken->AnimationSlots[Idx] =
                NewSlots[m_two_param_blendToken->AnimationSlots[Idx]->Index];
        }
    }
}


void GSTATE
two_param_blend::UnbindFromCharacter()
{
    parent::UnbindFromCharacter();

    // Only output 0 can be an animation
    GStateAssert(GetNumOutputs() == 1);
    GStateAssert(GetOutputType(0) == PoseEdge);

    for (int Idx = 0; Idx < m_two_param_blendToken->AnimationSlotCount; ++Idx)
    {
        GStateAssert(IsOutputExternal(0));
        GStateAssert(GetOutputType(0) == PoseEdge);

        if (m_Animations[Idx]->Binding != 0)
        {
            GrannyReleaseAnimationBinding(m_Animations[Idx]->Binding);
            m_Animations[Idx]->Binding = 0;
        }
    }
}


granny_real32 GSTATE
two_param_blend::GetDuration(granny_int32x OnOutput, granny_real32 AtT)
{
    UpdateSampling(AtT);
    
    // Will be set to neutral if the computation fails
    return m_ComputedDuration;
}

bool GSTATE
two_param_blend::DidLoopOccur(granny_int32x OnOutput, granny_real32 AtT, granny_real32 DeltaT)
{
    UpdateSampling(AtT);
    



    
    GS_InvalidCodePath("nyi");
    // todo
    return false;
}

static bool
ComputeBary(two_param_pt const& p0,
            two_param_pt const& p1,
            two_param_pt const& p2,
            two_param_pt const& p,
            granny_real32& b0,
            granny_real32& b1,
            granny_real32& b2)
{
    granny_real32 const det = ((p1.y - p2.y)*(p0.x - p2.x) +
                               (p2.x - p1.x)*(p0.y - p2.y));
    if (det == 0)
        return false;

    b0 = ((p1.y-p2.y)*(p.x - p2.x) + (p2.x-p1.x)*(p.y - p2.y)) / det;
    b1 = ((p2.y-p0.y)*(p.x - p2.x) + (p0.x-p2.x)*(p.y - p2.y)) / det;
    b2 = 1 - b0 - b1;
    return true;
}

bool GSTATE
two_param_blend::GetSampleWeights(granny_real32 AtT,
                                  granny_int32x Indices[3],
                                  granny_real32 Weights[3],
                                  bool          ClipToNearest)
{
    // Only if we have a valid triangulation...
    if (m_two_param_blendToken->IndexCount == 0)
        return false;

    granny_real32 XVal = 0;
    granny_real32 YVal = 0;
    INPUT_CONNECTION(0, X);
    if (XNode)
        XVal = XNode->SampleScalarOutput(XEdge, AtT, NULL);

    INPUT_CONNECTION(1, Y);
    if (YNode)
        YVal = YNode->SampleScalarOutput(YEdge, AtT, NULL);


    granny_real32 NearestDist = 1e38f;
    int           NearestIdx  = -1;
    granny_real32 NearestWeights[3];

    two_param_pt pt = { XVal, YVal };
    for (int Idx = 0; Idx < m_two_param_blendToken->IndexCount; Idx += 3)
    {
        granny_real32 b[3];
        if (ComputeBary(m_two_param_blendToken->ParamPoints[m_two_param_blendToken->Indices[Idx + 0]],
                        m_two_param_blendToken->ParamPoints[m_two_param_blendToken->Indices[Idx + 1]],
                        m_two_param_blendToken->ParamPoints[m_two_param_blendToken->Indices[Idx + 2]],
                        pt, b[0], b[1], b[2]))
        {
            // All pos = inthe triangle, just bail...
            if (b[0] >= 0.0f &&
                b[1] >= 0.0f &&
                b[2] >= 0.0f)
            {
                for (int PIdx = 0; PIdx < 3; ++PIdx)
                {
                    Indices[PIdx] = m_two_param_blendToken->Indices[Idx + PIdx];
                    Weights[PIdx] = b[PIdx];
                }

                return true;
            }

            // Loop the edges and compute nearest feature.
            for (int Idx0 = 2, Idx1 = 0; Idx1 < 3; Idx0 = Idx1++)
            {
                two_param_pt const& p1 = m_two_param_blendToken->ParamPoints[m_two_param_blendToken->Indices[Idx + Idx0]];
                two_param_pt const& p2 = m_two_param_blendToken->ParamPoints[m_two_param_blendToken->Indices[Idx + Idx1]];

                float d2 = (p1.x-p2.x)*(p1.x-p2.x) + (p1.y-p2.y)*(p1.y-p2.y);
                float  t = ((pt.x - p1.x) * (p2.x - p1.x) + (pt.y - p1.y) * (p2.y - p1.y)) / d2;

                // Clamp to the edge, which also picks the vert if appropriate
                t = Clamp(0, t, 1);

                two_param_pt p = {
                    p1.x*(1-t)+p2.x*t,
                    p1.y*(1-t)+p2.y*t
                };

                granny_real32 Dist = (pt.x-p.x)*(pt.x-p.x) + (pt.y-p.y)*(pt.y-p.y);
                if (Dist < NearestDist)
                {
                    NearestDist       = Dist;
                    NearestIdx        = Idx;
                    if (ClipToNearest)
                    {
                        NearestWeights[0] =
                            NearestWeights[1] =
                            NearestWeights[2] = 0;
                        NearestWeights[Idx0] = 1-t;
                        NearestWeights[Idx1] = t;
                    }
                    else
                    {
                        memcpy(NearestWeights, b, sizeof(granny_real32)*3);
                    }
                }
            }
        }
    }

    if (NearestIdx != 1)
    {
        for (int PIdx = 0; PIdx < 3; ++PIdx)
        {
            Indices[PIdx] = m_two_param_blendToken->Indices[NearestIdx + PIdx];
            Weights[PIdx] = NearestWeights[PIdx];
        }
        return true;
    }
    
    return false;
}


granny_local_pose* GSTATE
two_param_blend::SamplePoseOutput(granny_int32x      OutputIdx,
                                  granny_real32      AtT,
                                  granny_real32      AllowedError,
                                  granny_pose_cache* PoseCache,
                                  granny_real32*     PoseWeight)
{
    GSTATE_AUTO_ZONE_FN();
    GStateAssert(OutputIdx == 0);
    GStateAssert(PoseCache);

    UpdateSampling(AtT);

    // Do we have a valid sampling after the update?
    if (m_Indices[0] == -1)
        return 0;

    // Compute our relative looped position
    granny_real32 RelativePosition = (AtT + m_LocalOffset) / m_ComputedDuration;
    granny_real32 LoopedPosition = (granny_real32)fmod(RelativePosition, 1);
    GStateAssert(LoopedPosition >= 0 && LoopedPosition <= 1.0f);

    granny_model* Model = GetSourceModelForCharacter(GetBoundCharacter());
    if (!Model)
        return 0;

    granny_skeleton* Skeleton = Model->Skeleton;

    granny_int32x LODBoneCount = GrannyGetBoneCountForLOD(Skeleton, AllowedError);
    granny_local_pose* IntoPose = GrannyGetCacheLocalPose(PoseCache, Skeleton->BoneCount);

    GrannyBeginLocalPoseAccumulation(IntoPose, 0, Skeleton->BoneCount, 0);
    for (int Idx = 0; Idx < 3; ++Idx)
    {
        GStateAssert(GS_InRange(m_Indices[Idx], m_two_param_blendToken->AnimationSlotCount));
        GStateAssert(m_Animations[m_Indices[Idx]]->Binding); // or update sampling would have invalidated

        granny_animation_binding* Binding = m_Animations[m_Indices[Idx]]->Binding;

        if (m_Weights[Idx] == 0)
            continue;
        
        granny_sample_context Context;
        Context.UnderflowLoop = true;
        Context.OverflowLoop  = true;
        Context.LocalDuration = Binding->ID.Animation->Duration;
        Context.LocalClock    = Clamp(0.0f, LoopedPosition * Binding->ID.Animation->Duration, Context.LocalDuration);

        GrannySetContextFrameIndex(&Context,
                                   Context.LocalClock,
                                   Binding->ID.Animation->Duration,
                                   Binding->ID.Animation->TimeStep);

        GrannyAccumulateControlledAnimation(m_Animations[m_Indices[Idx]],
                                            &Context, m_Weights[Idx], Skeleton,
                                            0, LODBoneCount,
                                            IntoPose, AllowedError, 0);
    }
    GrannyEndLocalPoseAccumulationLOD(IntoPose,
                                      0, Skeleton->BoneCount,
                                      0, LODBoneCount,
                                      Skeleton, AllowedError, 0);

    return IntoPose;
}

bool GSTATE
two_param_blend::GetRootMotionVectors(granny_int32x  OutputIdx,
                                      granny_real32  AtT,
                                      granny_real32  DeltaT,
                                      granny_real32* ResultTranslation,
                                      granny_real32* ResultRotation,
                                      bool           Inverse)
{
    GSTATE_AUTO_ZONE_FN();
    GStateAssert(OutputIdx == 0);

    UpdateSampling(AtT);

    // Unlike the controlled animation version of this, we /do/ need to ensure that the
    // vectors are zero filled before we start
    memset(ResultTranslation, 0, sizeof(granny_real32) * 3);
    memset(ResultRotation,    0, sizeof(granny_real32) * 3);
    
    granny_model* Model = GetSourceModelForCharacter(GetBoundCharacter());
    
    // Do we have a valid sampling after the update?
    if (m_Indices[0] == -1 || !Model)
    {
        return false;
    }

    // Compute our relative looped position
    granny_real32 RelativePosition = (AtT + m_LocalOffset) / m_ComputedDuration;
    granny_real32 LoopedPosition = (granny_real32)fmod(RelativePosition, 1);
    GStateAssert(LoopedPosition >= 0 && LoopedPosition <= 1.0f);
	GStateUnused(LoopedPosition);

    for (int Idx = 0; Idx < 3; ++Idx)
    {
        granny_animation_binding* Binding = m_Animations[m_Indices[Idx]]->Binding;

        if (m_Weights[Idx] == 0)
            continue;

        granny_real32 const LocalDuration = Binding->ID.Animation->Duration;
        granny_real32 const DurationScale = LocalDuration / m_ComputedDuration;

        granny_real32 LocalEndTime = RelativePosition * LocalDuration;
        granny_real32 LocalStartTime = LocalEndTime - DeltaT * DurationScale;

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
        GrannyAccumulateControlledAnimationMotionVectors(m_Animations[m_Indices[Idx]],
                                                         LocalStartTime, LocalEndTime, LocalDuration,
                                                         m_Weights[Idx], FlipResult,
                                                         BoundTrack, SourceTrack,
                                                         &IgnoreTotalWeight,
                                                         (granny_real32*)ResultTranslation,
                                                         (granny_real32*)ResultRotation);
    }

    return true;
}

bool GSTATE
two_param_blend::GetPhaseBoundaries(granny_int32x  OnOutput,
                                    granny_real32  AtT,
                                    granny_real32* PhaseStart,
                                    granny_real32* PhaseEnd)
{
    UpdateSampling(AtT);

    GS_InvalidCodePath("nyi");
    // todo
    return false;
}

void GSTATE
two_param_blend::Activate(granny_real32 AtT)
{
    parent::Activate(AtT);

    m_LocalOffset = -AtT;
}

granny_real32 GSTATE
two_param_blend::GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx)
{
    return m_LocalOffset;
}

// void GSTATE
// two_param_blend::Synchronize(granny_int32x OnOutput,
//                              granny_real32 AtT,
//                              granny_real32 ReferenceStart,
//                              granny_real32 ReferenceEnd,
//                              granny_real32 LocalStart,
//                              granny_real32 LocalEnd)
// {
//     // todo
// }



//struct p3 { float x, y, z; };
//
//bool GRANNY
//ComputeTriangulation(param_point const* Points,
//                     int32x const       NumPoints,
//                     vector<int32x>&    Triangulation)
//{
//    Triangulation.clear();
//
//    vector<p3> QuadPoints;
//    for (int Idx = 0; Idx < NumPoints; ++Idx)
//    {
//        p3 p = { Points[Idx].x,
//                 Points[Idx].y,
//                 (Points[Idx].x*Points[Idx].x +
//                  Points[Idx].y*Points[Idx].y) };
//        QuadPoints.push_back(p);
//    }
//
//    for (int i = 0; i < NumPoints-2; ++i)
//    {
//        for (int j = i + 1; j < NumPoints; ++j)
//        {
//            for (int k = i + 1; k < NumPoints; ++k)
//            {
//                if (j == k)
//                    continue;
//
//                /* Compute normal to triangle (i,j,k). */
//                real32 xn = ((QuadPoints[j].y-QuadPoints[i].y)*(QuadPoints[k].z-QuadPoints[i].z) -
//                             (QuadPoints[k].y-QuadPoints[i].y)*(QuadPoints[j].z-QuadPoints[i].z));
//                real32 yn = ((QuadPoints[k].x-QuadPoints[i].x)*(QuadPoints[j].z-QuadPoints[i].z) -
//                             (QuadPoints[j].x-QuadPoints[i].x)*(QuadPoints[k].z-QuadPoints[i].z));
//                real32 zn = ((QuadPoints[j].x-QuadPoints[i].x)*(QuadPoints[k].y-QuadPoints[i].y) -
//                             (QuadPoints[k].x-QuadPoints[i].x)*(QuadPoints[j].y-QuadPoints[i].y));
//
//                if (zn < 0)
//                {
//                    bool Flag = true;
//                    for (int m = 0; m < NumPoints && Flag; ++m)
//                    {
//                        if (m == i || m == j || m == k)
//                            continue;
//
//                        Flag = Flag &&
//                            ((QuadPoints[m].x - QuadPoints[i].x) * xn +
//                             (QuadPoints[m].y - QuadPoints[i].y) * yn +
//                             (QuadPoints[m].z - QuadPoints[i].z) * zn) <= 0;
//                    }
//
//                    if (Flag)
//                    {
//                        // Face is included in the lower face of the bounding hull
//
//                        // winding order
//                        Triangulation.push_back(i);
//                        Triangulation.push_back(j);
//                        Triangulation.push_back(k);
//                    }
//                }
//            }
//        }
//    }
//
//    return true;
//}
//

void GSTATE
two_param_blend::RecomputeTriangulation()
{
    TakeTokenOwnership();

    // Remove the old triangulation so we can just exit on error
    GStateDeallocate(m_two_param_blendToken->Indices);
    m_two_param_blendToken->IndexCount = 0;

    int NumRawPoints = m_two_param_blendToken->ParamPointCount;
    if (NumRawPoints < 3)
    {
        // Easy.
        return;
    }

    two_param_pt const* Pts = m_two_param_blendToken->ParamPoints;

    // Grab the indices of only the unique parameter locations.
    int* UniquePoints = GStateAllocArray(int, NumRawPoints);
    int  UniquePointCount = 0;
    for (int Idx = 0; Idx < NumRawPoints; ++Idx)
    {
        bool Identical = false;
        for (int InIdx = 0; InIdx < UniquePointCount; ++InIdx)
        {
            // Look only for exact duplicates for the moment.
            if (memcmp(&Pts[Idx], &Pts[InIdx], sizeof(two_param_pt)) == 0)
            {
                Identical = true;
                break;
            }
        }

        if (!Identical)
        {
            // Take the point
            UniquePoints[UniquePointCount++] = Idx;
        }
    }

    if (UniquePointCount < 3)
    {
        // Can't construct a triangulation out of this.
        GStateDeallocate(UniquePoints);
        return;
    }

    // This does a delaunay triangulation by grabbing the bottom hull of [x,y,x*x+y+y].
    // Very slow, but N is very small.
#define PTZ(g) (Pts[g].x*Pts[g].x + Pts[g].y*Pts[g].y)

    granny_int16* TempIndices = GStateAllocArray(granny_int16, UniquePointCount*9);
    int NumIndices = 0;
    for (int i = 0; i < UniquePointCount-2; ++i)
    {
        int IdxI = UniquePoints[i];
        granny_real32 iz = PTZ(IdxI); iz = iz;
        for (int j = i + 1; j < UniquePointCount; ++j)
        {
            int IdxJ = UniquePoints[j];
            granny_real32 jz = PTZ(IdxJ); jz = jz;
            for (int k = i + 1; k < UniquePointCount; ++k)
            {
                if (j == k)
                    continue;
                int IdxK = UniquePoints[k];
                granny_real32 kz = PTZ(IdxK); kz = kz;

                /* Compute normal to triangle (i,j,k). */
                granny_real32 zn = ((Pts[IdxJ].x-Pts[IdxI].x)*(Pts[IdxK].y-Pts[IdxI].y) -
                                    (Pts[IdxK].x-Pts[IdxI].x)*(Pts[IdxJ].y-Pts[IdxI].y));

                if (zn < 0)
                {
                    // Check also that we don't share an edge with any existing triangle
                    bool Match = false;
                    for (int CheckIdx = 0; CheckIdx < NumIndices; CheckIdx += 3)
                    {
                        int i0 = TempIndices[CheckIdx + 0];
                        int i1 = TempIndices[CheckIdx + 1];
                        int i2 = TempIndices[CheckIdx + 2];


                        // We always insert the indices in sorted order
                        GStateAssert(IdxI < IdxJ && IdxI < IdxK);
                        GStateAssert(i0 < i1 && i0 < i2);

                        // lame
                        if (((IdxI == i0 && IdxJ == i1) ||
                             (IdxI == i1 && IdxJ == i2) ||
                             (IdxI == i2 && IdxJ == i0)) ||
                            ((IdxJ == i0 && IdxK == i1) ||
                             (IdxJ == i1 && IdxK == i2) ||
                             (IdxJ == i2 && IdxK == i0)) ||
                            ((IdxK == i0 && IdxI == i1) ||
                             (IdxK == i1 && IdxI == i2) ||
                             (IdxK == i2 && IdxI == i0)))
                        {
                            Match = true;
                            break;
                        }
                    }
                    if (Match)
                        continue;

                    granny_real32 xn = ((Pts[IdxJ].y-Pts[IdxI].y)*(PTZ(IdxK)-PTZ(IdxI)) -
                                        (Pts[IdxK].y-Pts[IdxI].y)*(PTZ(IdxJ)-PTZ(IdxI)));
                    granny_real32 yn = ((Pts[IdxK].x-Pts[IdxI].x)*(PTZ(IdxJ)-PTZ(IdxI)) -
                                        (Pts[IdxJ].x-Pts[IdxI].x)*(PTZ(IdxK)-PTZ(IdxI)));

                    bool Flag = true;
                    for (int m = 0; m < UniquePointCount && Flag; ++m)
                    {
                        if (m == i || m == j || m == k)
                            continue;
                        int IdxM = UniquePoints[m];
                        granny_real32 mz = PTZ(IdxM); mz = mz;

                        Flag = Flag &&
                            ((Pts[IdxM].x - Pts[IdxI].x) * xn +
                             (Pts[IdxM].y - Pts[IdxI].y) * yn +
                             (PTZ(IdxM)   - PTZ(IdxI))   * zn) <= 0;
                    }

                    if (Flag)
                    {
                        // Face is included in the lower face of the bounding hull
                        TempIndices[NumIndices++] = (granny_int16)IdxI;
                        TempIndices[NumIndices++] = (granny_int16)IdxJ;
                        TempIndices[NumIndices++] = (granny_int16)IdxK;
                    }
                }
            }
        }
    }

    if (NumIndices != 0)
    {
        // Slightly more space than needed, but it will get truncated.
        m_two_param_blendToken->IndexCount = NumIndices;
        m_two_param_blendToken->Indices    = TempIndices;
        TempIndices = 0;
    }

    // Clean the temps
    GStateDeallocate(TempIndices);
    GStateDeallocate(UniquePoints);
}


granny_real32 GSTATE
two_param_blend::ComputeLocalT(granny_real32 AtT)
{
    GS_InvalidCodePath("nyi");
    return 0;
}

void GSTATE
two_param_blend::UpdateSampling(granny_real32 AtT)
{
    // No work to do!
    if (AtT == m_LastTime)
        return;
    m_LastTime = AtT;

    granny_int32 NewIndices[3];
    granny_real32 NewWeights[3];
    if (!GetSampleWeights(AtT, NewIndices, NewWeights, true))
    {
        // todo: macro-ize this zero-out
        m_ComputedDuration = 1;
        m_Indices[0]       = m_Indices[1] = m_Indices[2] = -1;
        m_Weights[0]       = m_Weights[1] = m_Weights[2] = 0;
        return;
    }

    // For now, all of the vertices must be valid for the sample to take place
    if (m_Animations[NewIndices[0]]->Binding == 0 ||
        m_Animations[NewIndices[1]]->Binding == 0 ||
        m_Animations[NewIndices[2]]->Binding == 0)
    {
        m_ComputedDuration = 1;
        m_Indices[0]       = m_Indices[1] = m_Indices[2] = -1;
        m_Weights[0]       = m_Weights[1] = m_Weights[2] = 0;
        return;
    }

    granny_real32 NewDuration =
        (NewWeights[0] * m_Animations[NewIndices[0]]->Binding->ID.Animation->Duration + 
         NewWeights[1] * m_Animations[NewIndices[1]]->Binding->ID.Animation->Duration + 
         NewWeights[2] * m_Animations[NewIndices[2]]->Binding->ID.Animation->Duration);

	// Account for old animation position
	if (m_Indices[0] != -1)
    {
		granny_real32 RelativePosition = (AtT + m_LocalOffset) / m_ComputedDuration;
		m_LocalOffset = RelativePosition * NewDuration - AtT;
    }

    m_ComputedDuration = NewDuration;
    for (int Idx = 0; Idx < 3; ++Idx)
    {
        m_Indices[Idx] = NewIndices[Idx];
        m_Weights[Idx] = NewWeights[Idx];
    }
}



granny_int32x GSTATE
two_param_blend::GetNumSources()
{
    return m_two_param_blendToken->AnimationSlotCount;
}

bool GSTATE
two_param_blend::AddSource()
{
    TakeTokenOwnership();

    granny_controlled_animation*& Anim =
        QVecPushNewElementNoCount(m_two_param_blendToken->AnimationSlotCount,
                                  m_Animations);
    GStateAllocZeroedStruct(Anim);

    // We always add the new source as 0, so there's no need (yet) to unbind
    animation_slot*& Slot = QVecPushNewElement(m_two_param_blendToken->AnimationSlotCount,
                                               m_two_param_blendToken->AnimationSlots);
    Slot = 0;

    two_param_pt& Point = QVecPushNewElement(m_two_param_blendToken->ParamPointCount,
                                             m_two_param_blendToken->ParamPoints);
    Point.x = 0;
    Point.y = 0;

    // Redo the triangulation
    RecomputeTriangulation();
    m_LastTime = cNotSetup;

    
    return true;
}

bool GSTATE
two_param_blend::DeleteSource(granny_int32x SourceIdx)
{
    GStateCheck(GS_InRange(SourceIdx, m_two_param_blendToken->AnimationSlotCount), return false);

    TakeTokenOwnership();

    // In this case, we *do* need to rebind
    gstate_character_instance* Instance = GetBoundCharacter();
    if (Instance)
        UnbindFromCharacter();

    // Unbind takes care of the ->Binding
    GStateDeallocate(m_Animations[SourceIdx]);
    QVecRemoveElementNoCount(SourceIdx,
                             m_two_param_blendToken->AnimationSlotCount,
                             m_Animations);

    QVecRemoveElement(SourceIdx,
                      m_two_param_blendToken->AnimationSlotCount,
                      m_two_param_blendToken->AnimationSlots);

    QVecRemoveElement(SourceIdx,
                      m_two_param_blendToken->ParamPointCount,
                      m_two_param_blendToken->ParamPoints);

    // Redo the indices...
    RecomputeTriangulation();

    if (Instance)
        BindToCharacter(Instance);
    m_LastTime = cNotSetup;

    return true;
}


void GSTATE
two_param_blend::GetSourceCoords(granny_int32x SourceIdx,
                                 granny_real32* OutX,
                                 granny_real32* OutY)
{
    GStateAssert(GS_InRange(SourceIdx, m_two_param_blendToken->ParamPointCount));

    *OutX = m_two_param_blendToken->ParamPoints[SourceIdx].x;
    *OutY = m_two_param_blendToken->ParamPoints[SourceIdx].y;
}

void GSTATE
two_param_blend::SetSourceCoords(granny_int32x SourceIdx,
                                 granny_real32 InX,
                                 granny_real32 InY)
{
    GStateAssert(GS_InRange(SourceIdx, m_two_param_blendToken->ParamPointCount));

    TakeTokenOwnership();

    m_two_param_blendToken->ParamPoints[SourceIdx].x = InX;
    m_two_param_blendToken->ParamPoints[SourceIdx].y = InY;

    RecomputeTriangulation();
}

granny_int32x GSTATE
two_param_blend::GetNumTriIndices()
{
    GStateAssert((m_two_param_blendToken->IndexCount % 3) == 0);

    return m_two_param_blendToken->IndexCount;
}

bool GSTATE
two_param_blend::GetTriangulation(granny_int16* Indices,
                                  granny_int32  NumIndices)
{
    if (NumIndices < m_two_param_blendToken->IndexCount)
        return false;

    if (m_two_param_blendToken->IndexCount != 0)
    {
        memcpy(Indices, m_two_param_blendToken->Indices,
               m_two_param_blendToken->IndexCount * sizeof(granny_int16));
    }

    return true;
}

animation_slot* GSTATE
two_param_blend::GetAnimationSlot(granny_int32x SourceIdx)
{
    GStateAssert(GS_InRange(SourceIdx, m_two_param_blendToken->AnimationSlotCount));

    return m_two_param_blendToken->AnimationSlots[SourceIdx];
}

void GSTATE
two_param_blend::SetAnimationSlot(granny_int32x   SourceIdx,
                                  animation_slot* Slot)
{
    TakeTokenOwnership();

    // We need to unbind, and the rebind once the spec has been reset so we can grab our
    // animation resources, etc.
    gstate_character_instance* Instance = GetBoundCharacter();
    if (Instance)
        UnbindFromCharacter();

    m_two_param_blendToken->AnimationSlots[SourceIdx] = Slot;

    if (Instance)
        BindToCharacter(Instance);
}

CREATE_SNAPSHOT(two_param_blend)
{
    CREATE_WRITE_REAL32(m_LocalOffset);
    CREATE_WRITE_REAL32(m_LastTime);
    CREATE_WRITE_REAL32(m_ComputedDuration);
    CREATE_WRITE_INT32_ARRAY(m_Indices, 3);
    CREATE_WRITE_REAL32_ARRAY(m_Weights, 3);
    CREATE_PASS_TO_PARENT();
}

RESET_FROMSNAPSHOT(two_param_blend)
{
    RESET_OFFSET_TRACKING();
    RESET_READ_REAL32(m_LocalOffset);
    RESET_READ_REAL32(m_LastTime);
    RESET_READ_REAL32(m_ComputedDuration);
    RESET_READ_INT32_ARRAY(m_Indices, 3);
    RESET_READ_REAL32_ARRAY(m_Weights, 3);
    RESET_PASS_TO_PARENT();
}
