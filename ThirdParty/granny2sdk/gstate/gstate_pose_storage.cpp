// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_pose_storage.cpp $
// $DateTime: 2012/03/16 15:41:10 $
// $Change: 36794 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================
#include "gstate_pose_storage.h"

#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_token_context.h"
#include "gstate_sample_recorder.h"
#include "gstate_telemetry.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE pose_storage::pose_storageImpl
{
    granny_local_pose* Pose;
    granny_bool32 SampleOncePerFrame;
};

granny_data_type_definition GSTATE
pose_storage::pose_storageImplType[] =
{
    { GrannyReferenceMember, "Pose", GrannyLocalPoseType },
    { GrannyBool32Member, "SampleOncePerFrame" },
    { GrannyEndMember },
};

// pose_storage is a concrete class, so we must create a slotted container
struct pose_storage_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(pose_storage);
};

granny_data_type_definition pose_storage::pose_storageTokenType[] =
{
    DECL_UID_MEMBER(pose_storage),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(pose_storage),

    { GrannyEndMember }
};

void GSTATE
pose_storage::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(pose_storage);
    {
        if (OldToken->Pose)
        {
            int const BoneCount = GrannyGetLocalPoseBoneCount(OldToken->Pose);
            m_pose_storageToken->Pose = GrannyNewLocalPose(BoneCount);
            PoseOwned = true;
            GrannyCopyLocalPose(OldToken->Pose, m_pose_storageToken->Pose);
        }
    }
}

void GSTATE
pose_storage::ReleaseOwnedToken_pose_storage()
{
    GrannyFreeLocalPose(m_pose_storageToken->Pose);
    m_pose_storageToken->Pose = 0;
}

IMPL_CREATE_NODE_BOILERPLATE(pose_storage);

GSTATE
pose_storage::pose_storage(token_context*               Context,
                           granny_data_type_definition* TokenType,
                           void*                        TokenObject,
                           token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_pose_storageToken(0),
    LastSampleTimestamp(-1.0f),
    PoseOwned(false)
{
    IMPL_INIT_FROM_TOKEN(pose_storage);

    if (EditorCreated())
    {
        // Add our default input/output
        AddInputImpl(PoseEdge,  "PoseCapture");
        AddOutputImpl(PoseEdge, "Pose");
    }
}


GSTATE
pose_storage::~pose_storage()
{
    DTOR_RELEASE_TOKEN(pose_storage);
}

bool GSTATE
pose_storage::FillDefaultToken(granny_data_type_definition* TokenType,
                               void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares pose_storageImpl*& Slot = // member
    GET_TOKEN_SLOT(pose_storage);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->Pose = 0;

    return true;
}


granny_local_pose* GSTATE
pose_storage::SamplePoseOutput(granny_int32x OutputIdx,
                               granny_real32 AtT,
                               granny_real32 AllowedError,
                               granny_pose_cache* PoseCache,
                               granny_real32* PoseWeight)
{
    GSTATE_AUTO_ZONE_FN_OR_NAME();

    GStateAssert(OutputIdx == 0);
    GStateAssert(PoseCache != 0);

    granny_model* Model = GetModelForCharacterAutomatic(GetBoundCharacter());
    if (!Model)
        return 0;

    granny_skeleton* Skeleton = Model->Skeleton;
    granny_local_pose* IntoPose = GrannyGetCacheLocalPose(PoseCache, Skeleton->BoneCount);
    GStateAssert(GrannyGetLocalPoseBoneCount(IntoPose) == Skeleton->BoneCount);

    // Double check that the pose matches the character
    if( m_pose_storageToken->SampleOncePerFrame )
    {
        if( m_pose_storageToken->Pose != 0 && 
            GrannyGetLocalPoseBoneCount(m_pose_storageToken->Pose) != Skeleton->BoneCount )
        {
            if( PoseOwned )
            {
                GrannyFreeLocalPose(m_pose_storageToken->Pose);
            }
            m_pose_storageToken->Pose = 0;
        }

        if( m_pose_storageToken->Pose == 0 )
        {
            gstate_character_instance* Instance = GetBoundCharacter();
            if (Instance)
            {
                // If there is no pose, create one...
                granny_model* BoundModel = GetModelForCharacterAutomatic(GetBoundCharacter());
                m_pose_storageToken->Pose = GrannyNewLocalPose(BoundModel->Skeleton->BoneCount);
                PoseOwned = true;
            }
        }

        if( m_pose_storageToken->Pose == 0 )
        {
            GrannyBuildRestLocalPose(Skeleton, 0, Skeleton->BoneCount, IntoPose);
        }
        else
        {
            if( AtT != LastSampleTimestamp )
            {
                LastSampleTimestamp = AtT;
                INPUT_CONNECTION(0, Capture);
                if (CaptureNode)
                {
                    granny_real32 SamplePoseWeight = 1.0f;
                    granny_local_pose* SampledPose = CaptureNode->SamplePoseOutput(CaptureEdge, AtT, 0.0f, PoseCache, &SamplePoseWeight);
                    if( SampledPose )
                    {
                        granny_model* BoundModel = GetModelForCharacterAutomatic(GetBoundCharacter());
                        if (m_pose_storageToken->Pose == 0)
                        {
                            m_pose_storageToken->Pose = GrannyNewLocalPose(BoundModel->Skeleton->BoneCount);
                            PoseOwned = true;
                        }

                        GStateAssert(GrannyGetLocalPoseBoneCount(m_pose_storageToken->Pose) == BoundModel->Skeleton->BoneCount);
                        GrannyCopyLocalPose(SampledPose, m_pose_storageToken->Pose);
                        GrannyReleaseCachePose(PoseCache, SampledPose);
                    }
                }
            }
            GrannyCopyLocalPose(m_pose_storageToken->Pose, IntoPose);
        }
    }
    else
    {
        if (m_pose_storageToken->Pose != 0 &&
            GrannyGetLocalPoseBoneCount(m_pose_storageToken->Pose) == Skeleton->BoneCount)
        {     
            GrannyCopyLocalPose(m_pose_storageToken->Pose, IntoPose);
        }
        else
        {
            GrannyBuildRestLocalPose(Skeleton, 0, Skeleton->BoneCount, IntoPose);
        }
    }

#if GSTATE_USE_SAMPLE_RECORDS
    gstate_sample_record * Record = gstate_sample_recorder::AddSampleRecord(this);
    if( Record )
    {
        GStateReplaceString(Record->Name, GetName());
        Record->Category = gstate_sample_record::eSampleRecordCategory_Pose;
        Record->Type = gstate_sample_record::eSampleRecordType_PoseStorage;
    }
#endif //GSTATE_USE_SAMPLE_RECORDS

    return IntoPose;
}


bool GSTATE
pose_storage::GetRootMotionVectors(granny_int32x  OutputIdx,
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

bool GSTATE
pose_storage::GetSampleOncePerFrame() const
{
    return m_pose_storageToken->SampleOncePerFrame ? true : false;
}

void GSTATE
pose_storage::SetSampleOncePerFrame(bool val)
{
    TakeTokenOwnership();
    m_pose_storageToken->SampleOncePerFrame = val ? 1 : 0;
}


void GSTATE
pose_storage::ReturnToRestPose()
{
    TakeTokenOwnership();

    // If we are bound, then build a rest pose in the token, otherwise, null out for
    // default rest pose...
    gstate_character_instance* Instance = GetBoundCharacter();
    if (Instance)
    {
        // If there is no pose, create one...
        granny_model* Model = GetModelForCharacterAutomatic(GetBoundCharacter());
        if (m_pose_storageToken->Pose == 0)
        {
            m_pose_storageToken->Pose = GrannyNewLocalPose(Model->Skeleton->BoneCount);
            PoseOwned = true;
        }
        GStateAssert(GrannyGetLocalPoseBoneCount(m_pose_storageToken->Pose) == Model->Skeleton->BoneCount);

        GrannyBuildRestLocalPose(Model->Skeleton,
                                 0, Model->Skeleton->BoneCount,
                                 m_pose_storageToken->Pose);
    }
    else
    {
        if( PoseOwned )
        {
            GrannyFreeLocalPose(m_pose_storageToken->Pose);
        }
        m_pose_storageToken->Pose = 0;
    }
}

// For now, this is only runnable in the tool.  That restriction may relax over time.
void GSTATE
pose_storage::CaptureInput(granny_real32 AtT)
{
    TakeTokenOwnership();

    INPUT_CONNECTION(0, Capture);
    if (!CaptureNode)
    {
        // Equivalent to:
        ReturnToRestPose();
        return;
    }

    // First, can we even sample the thing?
    granny_pose_cache* Cache = GrannyNewPoseCache();

    granny_real32 SamplePoseWeight = 1.0f;
    granny_local_pose* SampledPose = CaptureNode->SamplePoseOutput(CaptureEdge, AtT, 0.0f, Cache, &SamplePoseWeight);
    if (SampledPose)
    {
        // If the pose returns, then there is a bound character...
        // If there is no pose, create one...
        granny_model* Model = GetModelForCharacterAutomatic(GetBoundCharacter());
        if (m_pose_storageToken->Pose == 0)
        {
            m_pose_storageToken->Pose = GrannyNewLocalPose(Model->Skeleton->BoneCount);
            PoseOwned = true;
        }

        GStateAssert(GrannyGetLocalPoseBoneCount(m_pose_storageToken->Pose) == Model->Skeleton->BoneCount);
        GrannyCopyLocalPose(SampledPose, m_pose_storageToken->Pose);
    }
    else
    {
        // Equivalent to:
        ReturnToRestPose();
    }

    // Also releases the SampledPose, if necessary...
    GrannyFreePoseCache(Cache);
}
