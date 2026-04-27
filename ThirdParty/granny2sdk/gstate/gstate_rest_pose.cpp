// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_rest_pose.cpp $
// $DateTime: 2012/03/16 15:41:10 $
// $Change: 36794 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================
#include "gstate_rest_pose.h"

#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_telemetry.h"
#include "gstate_token_context.h"
#include "gstate_sample_recorder.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE rest_pose::rest_poseImpl
{
    granny_int32 Dummy;
};

granny_data_type_definition GSTATE
rest_pose::rest_poseImplType[] =
{
    { GrannyInt32Member, "Dummy" },
    { GrannyEndMember },
};

// rest_pose is a concrete class, so we must create a slotted container
struct rest_pose_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(rest_pose);
};

granny_data_type_definition rest_pose::rest_poseTokenType[] =
{
    DECL_UID_MEMBER(rest_pose),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(rest_pose),

    { GrannyEndMember }
};

DEFAULT_TAKE_TOKENOWNERSHIP(rest_pose);
IMPL_CREATE_NODE_BOILERPLATE(rest_pose);

GSTATE
rest_pose::rest_pose(token_context*               Context,
                           granny_data_type_definition* TokenType,
                           void*                        TokenObject,
                           token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_rest_poseToken(0)
{
    IMPL_INIT_FROM_TOKEN(rest_pose);

    if (EditorCreated())
    {
        // Add our default input/output
        AddOutputImpl(PoseEdge, "Pose");
    }
}


GSTATE
rest_pose::~rest_pose()
{
    DTOR_RELEASE_TOKEN(rest_pose);
}

bool GSTATE
rest_pose::FillDefaultToken(granny_data_type_definition* TokenType,
                               void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares rest_poseImpl*& Slot = // member
    GET_TOKEN_SLOT(rest_pose);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    return true;
}


granny_local_pose* GSTATE
rest_pose::SamplePoseOutput(granny_int32x OutputIdx,
                               granny_real32 AtT,
                               granny_real32 AllowedError,
                               granny_pose_cache* PoseCache,
                               granny_real32* PoseWeight)
{
    GSTATE_AUTO_ZONE_FN_OR_NAME();

    GStateAssert(OutputIdx == 0);
    GStateAssert(PoseCache != 0);

    gstate_character_instance* CharacterInstance = GetBoundCharacter();
    if (!CharacterInstance)
        return 0;

    granny_model* Model = GetSourceModelForCharacter(CharacterInstance);
    if (!Model)
        return 0;

    granny_local_pose * IntoPose = GrannyGetCacheRestLocalPose(PoseCache, Model);

    if( IntoPose )
    {
        granny_retargeter * Retargeter = GetCharacterRetargeter(CharacterInstance);
        bool  RetargetAnimationSources = GrannyGetRetargetAnimationSources(CharacterInstance);
        granny_model* TargetModel = GetTargetModelForCharacter(CharacterInstance);

        if( Retargeter && RetargetAnimationSources && TargetModel )
        {
            granny_int32 BoneCount = TargetModel->Skeleton->BoneCount;
            granny_local_pose* TargPose =
                GrannyGetCacheLocalPose(PoseCache, BoneCount);

            if (GrannyRetargetPose(IntoPose, TargPose, 0, BoneCount, Retargeter))
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
        Record->Type = gstate_sample_record::eSampleRecordType_RestPose;
    }
#endif //GSTATE_USE_SAMPLE_RECORDS


    return IntoPose;
}


bool GSTATE
rest_pose::GetRootMotionVectors(granny_int32x  OutputIdx,
                                   granny_real32  AtT,
                                   granny_real32  DeltaT,
                                   granny_real32* ResultTranslation,
                                   granny_real32* ResultRotation,
                                   bool           Inverse)
{
    GStateAssert(GetBoundCharacter());

    if (DeltaT < 0)
    {
        GS_PreconditionFailed;
        return false;
    }
    if (!ResultTranslation || !ResultRotation)
    {
        GS_PreconditionFailed;
        return false;
    }

    // Also easy...
    memset(ResultTranslation, 0, sizeof(granny_real32)*3);
    memset(ResultRotation,    0, sizeof(granny_real32)*3);
    return true;
}


granny_real32 GSTATE
rest_pose::SampleScalarOutput(granny_int32x OutputIdx,
                         granny_real32 AtT, 
                         gstate_scalar_track_entry * TrackEntry)
{
    return 0;
}

bool GSTATE
rest_pose::GetScalarOutputRange(granny_int32x  OutputIdx,
                           granny_real32* OutMin,
                           granny_real32* OutMax)
{
    // Allow nodes to use this as the default, i.e, no InvalidCodePath like above...
    return false;
}

bool GSTATE
rest_pose::GetMorphChannelBindings(granny_int32x OutputIdx,
                              char const**  MeshNames,
                              granny_int32x MeshNameCount)
{
    return false;
}

granny_int32x GSTATE
rest_pose::GetNumMorphChannels(granny_int32x OutputIdx)
{
    return -1;
}

bool GSTATE
rest_pose::SampleMorphOutput(granny_int32x  OutputIdx,
                        granny_real32  AtT,
                        granny_real32* MorphWeights,
                        granny_int32x NumMorphWeights)
{
    return false;
}

bool GSTATE
rest_pose::SampleEventOutput(granny_int32x            OutputIdx,
                        granny_real32            AtT,
                        granny_real32            DeltaT,
                        gstate_text_track_entry* EventBuffer,
                        granny_int32x const      EventBufferSize,
                        granny_int32x*           NumEvents)
{
    return false;
}

bool GSTATE
rest_pose::GetNextEvent(granny_int32x            OutputIdx,
                   granny_real32            AtT,
                   gstate_text_track_entry*  Event)
{
    return false;
}


bool GSTATE
rest_pose::GetAllEvents(granny_int32x            OutputIdx,
                   gstate_text_track_entry*  EventBuffer,
                   granny_int32x const      EventBufferSize,
                   granny_int32x*           NumEvents)
{
    return false;
}

bool GSTATE
rest_pose::GetCloseEventTimes(granny_int32x  OutputIdx,
                         granny_real32  AtT,
                         char const*    TextToFind,
                         granny_real32* PreviousTime,
                         granny_real32* NextTime)
{
    return 0.0f;
}