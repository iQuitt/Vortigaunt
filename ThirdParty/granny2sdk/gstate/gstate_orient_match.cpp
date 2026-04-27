// ========================================================================
// $Notice: $
// ========================================================================
#include "gstate_orient_match.h"

#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_quick_vecs.h"
#include "gstate_snapshotutils.h"
#include "gstate_token_context.h"
#include "gstate_math.h"
#include "gstate_sample_recorder.h"
#include "gstate_telemetry.h"

#include <string.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE orient_match::orient_matchImpl
{
    granny_int32  TargetType;
    granny_real32 OrientationOffset[3];

    char* OrientBone;
    char* TargetBone;
};

granny_data_type_definition GSTATE
orient_match::orient_matchImplType[] =
{
    { GrannyInt32Member,            "TargetType" },
    { GrannyReal32Member,           "OrientationOffset", 0, 3 },
    { GrannyStringMember,           "OrientBone" },
    { GrannyStringMember,           "TargetBone" },
    { GrannyEndMember },
};

// orient_match is a concrete class, so we must create a slotted container
struct orient_match_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(orient_match);
};

granny_data_type_definition orient_match::orient_matchTokenType[] =
{
    DECL_UID_MEMBER(orient_match),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(orient_match),

    { GrannyEndMember }
};

void GSTATE
orient_match::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(orient_match);

    // When we take token ownership, future code will assume that it needs to free our
    // owned pointers when they change.  Don't disappoint them.
    GStateCloneString(m_orient_matchToken->OrientBone,  OldToken->OrientBone);
    GStateCloneString(m_orient_matchToken->TargetBone,  OldToken->TargetBone);
}

void GSTATE
orient_match::ReleaseOwnedToken_orient_match()
{
    GStateDeallocate(m_orient_matchToken->OrientBone);
    GStateDeallocate(m_orient_matchToken->TargetBone);
}

IMPL_CREATE_NODE_BOILERPLATE(orient_match);
IMPL_PASSTHROUGH_DIDLOOPOCCUR(orient_match, OnOutput == 0);
IMPL_PASSTHROUGH_GETDURATION(orient_match, OnOutput == 0);
IMPL_PASSTHROUGH_GETROOTMOTIONVECTORS(orient_match, OnOutput == 0);
IMPL_PASSTHROUGH_SAMPLEMORPHINTERFACE(orient_match, 0, true);
IMPL_PASSTHROUGH_SAMPLESCALARINTERFACE(orient_match, 0, true);
IMPL_PASSTHROUGH_SAMPLEEVENTINTERFACE(orient_match, 0, true);

char const* GSTATE
NameForOrientMatchTarget(orient_match_target_type Type)
{
    switch (Type)
    {
    case OrientMatchTarget_User: return "User Specified";
    case OrientMatchTarget_Bone: return "Bone Name";
    case OrientMatchTarget_Name: return "Custom Name";
    default:
        GS_InvalidCodePath("Unknown orient match target type");
        return "Unknown";
    }
}

GSTATE
orient_match::orient_match(token_context*               Context,
                               granny_data_type_definition* TokenType,
                               void*                        TokenObject,
                               token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_orient_matchToken(0),
    m_tempPose(0),
    m_OrientBoneIndex(-1),
    FullJointChainToRootLength(0),
    FullJointChainToRoot(0)
{
    IMPL_INIT_FROM_TOKEN(orient_match);

    if (EditorCreated())
    {
        // Add our default input/output
        AddInputImpl(PoseEdge, "Pose");
        AddOutputImpl(PoseEdge, "Pose");
    }
}


GSTATE
orient_match::~orient_match()
{
    DTOR_RELEASE_TOKEN(orient_match);
    GStateDeallocate(FullJointChainToRoot);
    FullJointChainToRootLength = 0;
}

bool GSTATE
orient_match::FillDefaultToken(granny_data_type_definition* TokenType,
                                 void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares orient_matchImpl*& Slot = // member
    GET_TOKEN_SLOT(orient_match);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->OrientationOffset[0] = 0;
    Slot->OrientationOffset[1] = 0;
    Slot->OrientationOffset[2] = 0;

    Slot->OrientBone = 0;
    Slot->TargetBone = 0;

    return true;
}

void GSTATE
orient_match::RefreshBoundValues()
{
    gstate_character_instance* Instance = GetBoundCharacter();
    if (!Instance)
        return;

    granny_model* Model = GetModelForCharacterAutomatic(Instance);
    if (Model == 0)
        return;

    // Re-cache the values
    if (m_orient_matchToken->OrientBone && m_orient_matchToken->OrientBone[0] != 0)
    {
        if( !GrannyFindBoneByNameLowercase(Model->Skeleton, m_orient_matchToken->OrientBone, &m_OrientBoneIndex) )
        {
            m_OrientBoneIndex = -1;
        }
    }
    else
    {
        m_OrientBoneIndex = -1;
    }

    if( m_OrientBoneIndex < 0 )
    {
        return;
    }

    granny_int32x RootEffectorTempChain[2];
    RootEffectorTempChain[0] = m_OrientBoneIndex;
    RootEffectorTempChain[1] = 0; //Bone 0 is the root
    granny_int32x * FullJointChainToRootUpperBound = GStateAllocArray(granny_int32x,Model->Skeleton->BoneCount); //BoneCount is an upper bound on the length of the longest chain from end effector to root. 

    FullJointChainToRootLength = GrannyBuildFullJointChainFromJointChain(Model->Skeleton, 2, RootEffectorTempChain, Model->Skeleton->BoneCount, FullJointChainToRootUpperBound );
    FullJointChainToRoot = GStateAllocArray(granny_int32x,FullJointChainToRootLength); 

    for( int FullJointChainIdx=0; FullJointChainIdx<FullJointChainToRootLength; FullJointChainIdx++ )
    {
        FullJointChainToRoot[FullJointChainIdx] = FullJointChainToRootUpperBound[FullJointChainIdx];
    }

    GStateDeallocate(FullJointChainToRootUpperBound);
}

bool GSTATE
orient_match::BindToCharacter(gstate_character_instance* Instance)
{
    if (!parent::BindToCharacter(Instance))
        return false;

    granny_model* Model = GetModelForCharacterAutomatic(Instance);
    GStateAssert(Model);
    GStateAssert(Model->Skeleton);

    m_tempPose  = GrannyNewWorldPoseNoComposite(Model->Skeleton->BoneCount);

    m_OrientBoneIndex = -1;

    RefreshBoundValues();
    return true;
}

void GSTATE
orient_match::UnbindFromCharacter()
{
    GrannyFreeWorldPose(m_tempPose);
    m_tempPose = 0;
    GStateDeallocate(FullJointChainToRoot);

    parent::UnbindFromCharacter();
}



granny_real32 GSTATE
orient_match::GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx)
{
    GStateAssert(OutputIdx == 0);

    node* PoseNode = 0;
    granny_int32x PoseEdge = -1;
    GetInputConnection(0, &PoseNode, &PoseEdge);

    if (!PoseNode)
        return 0;

    return PoseNode->GetLocalTimeOffset(AtT, PoseEdge);
}

granny_local_pose* GSTATE
orient_match::SamplePoseOutput(granny_int32x      OutputIdx,
                                 granny_real32      AtT,
                                 granny_real32      AllowedError,
                                 granny_pose_cache* PoseCache,
                                 granny_real32*     PoseWeight)
{
    GSTATE_AUTO_ZONE_FN_OR_NAME();

    GStateAssert(OutputIdx == 0);
    GStateAssert(PoseCache != 0);
    GStateAssert(m_tempPose != 0);

    node* PoseNode = 0;
    granny_int32x PoseEdge = -1;
    GetInputConnection(0, &PoseNode, &PoseEdge);

    if (!PoseNode)
        return 0;

    granny_model* Model = GetModelForCharacterAutomatic(GetBoundCharacter());
    GStateAssert(Model);
    GStateAssert(Model->Skeleton);

    granny_local_pose* Pose = PoseNode->SamplePoseOutput(PoseEdge, AtT, AllowedError, PoseCache, PoseWeight);

    bool TargetOrientValid = false;
    if( HasTargetBone() )
    {
        gstate_character_instance* Instance = GetBoundCharacter();
        granny_real32 AimTransform[16];
        if( GStateGetCachedMat4x4(Instance, m_orient_matchToken->TargetBone, &AimTransform[0]) )
        {
            GStateMatrix3x3Equals4x4(&m_AimOrientation3x3[0],&AimTransform[0]);
            TargetOrientValid = true;
        }
    }
    else
    {
        TargetOrientValid = true;
    }

    if (Pose != NULL)
    {
        granny_triple OrientationAfterOffset[3];
        GetOffsetTargetOrientation(&OrientationAfterOffset[0][0]);        

        if (m_OrientBoneIndex >= 0)
        {
            GrannyUpdateWorldPoseChain(Model->Skeleton, FullJointChainToRootLength-1, FullJointChainToRootLength, FullJointChainToRoot, Pose, 0, m_tempPose);

            GrannyIKOrientMatch(m_OrientBoneIndex,
                &OrientationAfterOffset[0][0], 
                Model->Skeleton, Pose, 0, m_tempPose);
        }
    }    

#if GSTATE_USE_SAMPLE_RECORDS
    gstate_sample_record * Record = gstate_sample_recorder::AddSampleRecord(this);
    if( Record )
    {
        GStateReplaceString(Record->Name, GetName());
        Record->Category = gstate_sample_record::eSampleRecordCategory_IK;
        Record->Type = gstate_sample_record::eSampleRecordType_IKOrientMatch;
    }
#endif //GSTATE_USE_SAMPLE_RECORDS

    return Pose;
}

CREATE_SNAPSHOT(orient_match)
{
    CREATE_WRITE_REAL32_ARRAY(m_AimOrientation3x3, 9);
    CREATE_PASS_TO_PARENT();
}

RESET_FROMSNAPSHOT(orient_match)
{
    RESET_OFFSET_TRACKING();
    RESET_READ_REAL32_ARRAY(m_AimOrientation3x3, 9);
    RESET_PASS_TO_PARENT();
}

void GSTATE
orient_match::SetTargetOrientation(granny_real32 const* Orientation3x3)
{
    memcpy(m_AimOrientation3x3, Orientation3x3, sizeof(m_AimOrientation3x3[0]) * 9);
}

void GSTATE
orient_match::GetTargetOrientation(granny_real32* Orientation3x3)
{
    memcpy(Orientation3x3, m_AimOrientation3x3, sizeof(m_AimOrientation3x3[0]) * 9);
}

void GSTATE
orient_match::SetOrientationOffset(granny_real32 const* Orientation3)
{
    memcpy(m_orient_matchToken->OrientationOffset, Orientation3, sizeof(Orientation3[0]) * 3);
}

void GSTATE
orient_match::GetOrientationOffset(granny_real32* Orientation3)
{
    memcpy(Orientation3, m_orient_matchToken->OrientationOffset, sizeof(Orientation3[0]) * 3);
}

void GSTATE
orient_match::SetTargetBone(const char * JointName)
{
    TakeTokenOwnership();
    GStateReplaceString(m_orient_matchToken->TargetBone, JointName);
}

char * GSTATE
orient_match::GetTargetBone()
{
    return m_orient_matchToken->TargetBone;
}

bool GSTATE
orient_match::HasTargetBone() const
{
    return m_orient_matchToken->TargetBone != NULL && m_orient_matchToken->TargetBone[0] != 0;
}

void GSTATE
orient_match::SetTargetType(orient_match_target_type TargetType)
{
    GStateAssert(TargetType >= 0 && TargetType < OrientMatchTarget_NumTypes);
    m_orient_matchToken->TargetType = (granny_int32)TargetType;
}

orient_match_target_type GSTATE
orient_match::GetTargetType() const
{
    return(orient_match_target_type)m_orient_matchToken->TargetType;
}

void GSTATE
orient_match::SetOrientBoneName(char const* BoneName)
{
    TakeTokenOwnership();
    
    GStateReplaceString(m_orient_matchToken->OrientBone, BoneName);

    RefreshBoundValues();
}


char const* GSTATE
orient_match::GetOrientBoneName() const
{
    return m_orient_matchToken->OrientBone;
}

granny_int32 GSTATE
orient_match::GetOrientBoneIndex()
{
    return m_OrientBoneIndex;
}

void GSTATE
orient_match::GetOffsetTargetOrientation(granny_real32* Orientation3)
{
    granny_triple OffsetAxis[3];

    granny_triple OffsetAfterX[3];
    {
        GStateXRotationColumns3x3(&OffsetAfterX[0], m_orient_matchToken->OrientationOffset[0] * ((granny_real32)M_PI) / 180.0f);
    }

    granny_triple OffsetAfterY[3];
    {
        GStateYRotationColumns3x3(&OffsetAxis[0], m_orient_matchToken->OrientationOffset[1] * ((granny_real32)M_PI) / 180.0f);
        GStateMatrixMultiply3x3(&OffsetAfterY[0][0], &OffsetAfterX[0][0], &OffsetAxis[0][0] );
    }

    granny_triple OffsetAfterZ[3];
    {
        GStateZRotationColumns3x3(&OffsetAxis[0], m_orient_matchToken->OrientationOffset[2] * ((granny_real32)M_PI) / 180.0f);
        GStateMatrixMultiply3x3(&OffsetAfterZ[0][0], &OffsetAfterY[0][0], &OffsetAxis[0][0] );
    }

    GStateMatrixMultiply3x3(Orientation3, &OffsetAfterZ[0][0], &m_AimOrientation3x3[0] );
}

granny_int32x GSTATE
orient_match::GetOutputPassthrough(granny_int32x OutputIdx) const
{
    return OutputIdx;
}


