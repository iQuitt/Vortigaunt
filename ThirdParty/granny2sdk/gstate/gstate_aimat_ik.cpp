// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_aimat_ik.h"

#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_snapshotutils.h"
#include "gstate_token_context.h"
#include "gstate_sample_recorder.h"
#include "gstate_telemetry.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;


struct GSTATE aimat_ik::aimat_ikImpl
{
    granny_real32 DefaultAimPosition[3];

    char* DefaultAimJoint; 

    char* HeadName;
    char* FirstActiveName;
    char* LastActiveName;

    granny_int32 AimAxis;
    granny_int32 EarAxis;
    granny_int32 GroundNormal;
};

granny_data_type_definition GSTATE
aimat_ik::aimat_ikImplType[] =
{
    { GrannyReal32Member, "DefaultAimPosition", 0, 3 },

    { GrannyStringMember, "DefaultAimJoint" },

    { GrannyStringMember, "HeadName" },
    { GrannyStringMember, "FirstActiveName" },
    { GrannyStringMember, "LastActiveName" },

    { GrannyInt32Member, "AimAxis" },
    { GrannyInt32Member, "EarAxis" },
    { GrannyInt32Member, "GroundNormal" },

    { GrannyEndMember },
};

// aimat_ik is a concrete class, so we must create a slotted container
struct aimat_ik_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(aimat_ik);
};

granny_data_type_definition aimat_ik::aimat_ikTokenType[] =
{
    DECL_UID_MEMBER(aimat_ik),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(aimat_ik),

    { GrannyEndMember }
};

void GSTATE
aimat_ik::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(aimat_ik);

    // When we take token ownership, future code will assume that it needs to free our
    // owned pointers when they change.  Don't disappoint them.
    GStateCloneString(m_aimat_ikToken->DefaultAimJoint,  OldToken->DefaultAimJoint);
    GStateCloneString(m_aimat_ikToken->HeadName,        OldToken->HeadName);
    GStateCloneString(m_aimat_ikToken->FirstActiveName, OldToken->FirstActiveName);
    GStateCloneString(m_aimat_ikToken->LastActiveName,  OldToken->LastActiveName);
}

void GSTATE
aimat_ik::ReleaseOwnedToken_aimat_ik()
{
    GStateDeallocate(m_aimat_ikToken->DefaultAimJoint);
    GStateDeallocate(m_aimat_ikToken->HeadName);
    GStateDeallocate(m_aimat_ikToken->FirstActiveName);
    GStateDeallocate(m_aimat_ikToken->LastActiveName);
}

IMPL_CREATE_NODE_BOILERPLATE(aimat_ik);
IMPL_PASSTHROUGH_DIDLOOPOCCUR(aimat_ik, OnOutput == 0);
IMPL_PASSTHROUGH_GETDURATION(aimat_ik, OnOutput == 0);
IMPL_PASSTHROUGH_GETROOTMOTIONVECTORS(aimat_ik, OnOutput == 0);
IMPL_PASSTHROUGH_SAMPLEMORPHINTERFACE(aimat_ik, 0, true);
IMPL_PASSTHROUGH_SAMPLESCALARINTERFACE(aimat_ik, 0, true);
IMPL_PASSTHROUGH_SAMPLEEVENTINTERFACE(aimat_ik, 0, true);

GSTATE
aimat_ik::aimat_ik(token_context*               Context,
                   granny_data_type_definition* TokenType,
                   void*                        TokenObject,
                   token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_aimat_ikToken(0),
    m_tempPose(0),
    m_HeadIndex(-1),
    m_LinkCount(-1),
    m_InactiveLinks(-1)
{
    IMPL_INIT_FROM_TOKEN(aimat_ik);
    SetAimPosition(m_aimat_ikToken->DefaultAimPosition);

    if (EditorCreated())
    {
        // Add our default input/output
        AddInputImpl(PoseEdge, "Pose");
        AddOutputImpl(PoseEdge, "Pose");
    }
}


GSTATE
aimat_ik::~aimat_ik()
{
    DTOR_RELEASE_TOKEN(aimat_ik);
}

bool GSTATE
aimat_ik::FillDefaultToken(granny_data_type_definition* TokenType,
                           void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares aimat_ikImpl*& Slot = // member
    GET_TOKEN_SLOT(aimat_ik);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->DefaultAimPosition[0] = 0;
    Slot->DefaultAimPosition[1] = 0;
    Slot->DefaultAimPosition[2] = 10;

    Slot->HeadName        = 0;
    Slot->FirstActiveName = 0;
    Slot->LastActiveName  = 0;

    Slot->AimAxis      = eAimAxis_YAxis;
    Slot->EarAxis      = eAimAxis_XAxis;
    Slot->GroundNormal = eAimAxis_ZAxis;

    return true;
}

void GSTATE
aimat_ik::RefreshBoundValues()
{
    m_HeadIndex = -1;
    m_LinkCount = -1;
    m_InactiveLinks = -1;

    gstate_character_instance* Instance = GetBoundCharacter();
    if (!Instance)
        return;

    granny_model* Model = GetModelForCharacterAutomatic(Instance);
    if (Model == 0)
        return;

    if (m_aimat_ikToken->HeadName)
    {
        GrannyFindBoneByNameLowercase(Model->Skeleton, m_aimat_ikToken->HeadName, &m_HeadIndex);
    }

    if (m_HeadIndex != -1 &&
        m_aimat_ikToken->FirstActiveName != 0 &&
        m_aimat_ikToken->LastActiveName != 0)
    {
        granny_int32x ActiveIndex = -1;
        GrannyFindBoneByNameLowercase(Model->Skeleton, m_aimat_ikToken->FirstActiveName, &ActiveIndex);

        granny_int32x LastIndex = -1;
        GrannyFindBoneByNameLowercase(Model->Skeleton, m_aimat_ikToken->LastActiveName, &LastIndex);

        if (ActiveIndex != -1 && LastIndex != -1)
        {
            granny_int32x LinkCountTemp = 0;
            m_InactiveLinks = 0;

            granny_int32x Walk = m_HeadIndex;
            while (Walk != LastIndex && Walk != GrannyNoParentBone)
            {
                ++LinkCountTemp;

                Walk = Model->Skeleton->Bones[Walk].ParentIndex;
                if(Walk == GrannyNoParentBone)
                {
                    m_LinkCount = -1;
                    m_InactiveLinks = -1;
                    LinkCountTemp = -1;
                    break;
                }

                if (Walk == ActiveIndex)
                    m_InactiveLinks = LinkCountTemp;
            }

            m_LinkCount = LinkCountTemp;
        }
    }
}

bool GSTATE
aimat_ik::BindToCharacter(gstate_character_instance* Instance)
{
    if (!parent::BindToCharacter(Instance))
        return false;

    granny_model* Model = GetModelForCharacterAutomatic(Instance);
    GStateAssert(Model);
    GStateAssert(Model->Skeleton);

    m_tempPose = GrannyNewWorldPoseNoComposite(Model->Skeleton->BoneCount);

    memcpy(m_AimPosition, m_aimat_ikToken->DefaultAimPosition, sizeof(m_AimPosition));

    RefreshBoundValues();

    return true;
}

void GSTATE
aimat_ik::UnbindFromCharacter()
{
    GrannyFreeWorldPose(m_tempPose);
    m_tempPose = 0;

    m_HeadIndex     = -1;
    m_LinkCount     = -1;
    m_InactiveLinks = -1;

    parent::UnbindFromCharacter();
}

granny_real32 GSTATE
aimat_ik::GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx)
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
aimat_ik::SamplePoseOutput(granny_int32x      OutputIdx,
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

    bool AimPositionValid = false;
    if( m_aimat_ikToken->DefaultAimJoint )
    {
        gstate_character_instance* Instance = GetBoundCharacter();
        if( GStateGetCachedPosition(Instance, m_aimat_ikToken->DefaultAimJoint, &m_AimPosition[0]) )
        {
            AimPositionValid = true;
        }
    }
    else
    {
        AimPositionValid = true;
    }

    if (Pose && IsActive() && AimPositionValid)
    {
        // Build the world pose so we can do ik
        GrannyBuildWorldPose(Model->Skeleton,
                             0, Model->Skeleton->BoneCount,
                             Pose, 0, m_tempPose);

        granny_real32 Offset[3] = { 0, 0, 0 };

        GrannyIKAimAt(m_HeadIndex, m_LinkCount, m_InactiveLinks,
                      Offset,
                      FloatFromAxisEnum(m_aimat_ikToken->AimAxis),
                      FloatFromAxisEnum(m_aimat_ikToken->EarAxis),
                      FloatFromAxisEnum(m_aimat_ikToken->GroundNormal),
                      m_AimPosition,
                      Model->Skeleton, Pose, 0, m_tempPose);
    }

#if GSTATE_USE_SAMPLE_RECORDS
    gstate_sample_record * Record = gstate_sample_recorder::AddSampleRecord(this);
    if( Record )
    {
        GStateReplaceString(Record->Name, GetName());
        Record->Category = gstate_sample_record::eSampleRecordCategory_IK;
        Record->Type = gstate_sample_record::eSampleRecordType_IKAimAt;
    }
#endif //GSTATE_USE_SAMPLE_RECORDS

    return Pose;
}


CREATE_SNAPSHOT(aimat_ik)
{
    CREATE_WRITE_REAL32_ARRAY(m_AimPosition, 3);
    CREATE_WRITE_INT32(m_HeadIndex);
    CREATE_WRITE_INT32(m_LinkCount);
    CREATE_WRITE_INT32(m_InactiveLinks);
    CREATE_PASS_TO_PARENT();
}

RESET_FROMSNAPSHOT(aimat_ik)
{
    RESET_OFFSET_TRACKING();

    RESET_READ_REAL32_ARRAY(m_AimPosition, 3);
    RESET_READ_INT32(m_HeadIndex);
    RESET_READ_INT32(m_LinkCount);
    RESET_READ_INT32(m_InactiveLinks);
    RESET_PASS_TO_PARENT();
}

void GSTATE
aimat_ik::SetAimPosition(granny_real32 const* Position)
{
    for (int i = 0; i < 3; ++i)
        m_AimPosition[i] = Position[i];
}


void GSTATE
aimat_ik::GetAimPosition(granny_real32* Position)
{
    for (int i = 0; i < 3; ++i)
        Position[i] = m_AimPosition[i];
}

void GSTATE
aimat_ik::SetDefaultAimPosition(granny_real32 const* Position)
{
    for (int i = 0; i < 3; ++i)
        m_aimat_ikToken->DefaultAimPosition[i] = Position[i];
    SetAimPosition(Position);
}

void GSTATE
aimat_ik::GetDefaultAimPosition(granny_real32* Position)
{
    if( m_aimat_ikToken->DefaultAimJoint == NULL )
    {
        for (int i = 0; i < 3; ++i)
            Position[i] = m_aimat_ikToken->DefaultAimPosition[i];
    }
}

void GSTATE
aimat_ik::SetDefaultAimJoint(const char * JointName)
{
    TakeTokenOwnership();
    GStateReplaceString(m_aimat_ikToken->DefaultAimJoint, JointName);
}

const char * GSTATE
aimat_ik::GetDefaultAimJoint() const
{
    return m_aimat_ikToken->DefaultAimJoint;
}

void GSTATE
aimat_ik::SetAimAxis(EAimAxis Axis)
{
    m_aimat_ikToken->AimAxis = Axis;
}

GSTATE EAimAxis GSTATE
aimat_ik::GetAimAxis()
{
    GStateAssert(m_aimat_ikToken->AimAxis >= eAimAxis_XAxis &&
                 m_aimat_ikToken->AimAxis <= eAimAxis_NegZAxis);

    return EAimAxis(m_aimat_ikToken->AimAxis);
}

granny_real32 const* GSTATE
aimat_ik::GetAimAxisFloat()
{
    return FloatFromAxisEnum(m_aimat_ikToken->AimAxis);
}


void GSTATE
aimat_ik::SetEarAxis(EAimAxis Axis)
{
    m_aimat_ikToken->EarAxis = Axis;
}

GSTATE EAimAxis GSTATE
aimat_ik::GetEarAxis()
{
    GStateAssert(m_aimat_ikToken->AimAxis >= eAimAxis_XAxis &&
                 m_aimat_ikToken->AimAxis <= eAimAxis_NegZAxis);

    return EAimAxis(m_aimat_ikToken->EarAxis);
}

granny_real32 const* GSTATE
aimat_ik::GetEarAxisFloat()
{
    return FloatFromAxisEnum(m_aimat_ikToken->EarAxis);
}


void GSTATE
aimat_ik::SetGroundNormal(EAimAxis Axis)
{
    m_aimat_ikToken->GroundNormal = Axis;
}

GSTATE EAimAxis GSTATE
aimat_ik::GetGroundNormal()
{
    GStateAssert(m_aimat_ikToken->AimAxis >= eAimAxis_XAxis &&
                 m_aimat_ikToken->AimAxis <= eAimAxis_NegZAxis);

    return EAimAxis(m_aimat_ikToken->GroundNormal);
}

granny_real32 const* GSTATE
aimat_ik::GetGroundNormalFloat()
{
    return FloatFromAxisEnum(m_aimat_ikToken->GroundNormal);
}

void GSTATE
aimat_ik::SetHeadName(char const* HeadName)
{
    TakeTokenOwnership();

    GStateReplaceString(m_aimat_ikToken->HeadName, HeadName);

    RefreshBoundValues();
}

void GSTATE
aimat_ik::SetFirstActiveName(char const* FirstActive)
{
    TakeTokenOwnership();

    GStateReplaceString(m_aimat_ikToken->FirstActiveName, FirstActive);

    RefreshBoundValues();
}

void GSTATE
aimat_ik::SetLastActiveName(char const* LastActive)
{
    TakeTokenOwnership();

    GStateReplaceString(m_aimat_ikToken->LastActiveName, LastActive);

    RefreshBoundValues();
}

char const* GSTATE
aimat_ik::GetHeadName()
{
    return m_aimat_ikToken->HeadName;
}

char const* GSTATE
aimat_ik::GetFirstActiveName()
{
    return m_aimat_ikToken->FirstActiveName;
}

char const* GSTATE
aimat_ik::GetLastActiveName()
{
    return m_aimat_ikToken->LastActiveName;
}

bool GSTATE
aimat_ik::IsActive() const
{
    return (m_HeadIndex > 0 &&
            m_LinkCount > 0 &&
            m_InactiveLinks >= 0 &&
            m_InactiveLinks < m_LinkCount);
}

granny_int32x GSTATE
aimat_ik::GetBoundHeadIndex()
{
    return m_HeadIndex;
}

granny_int32x GSTATE
aimat_ik::GetBoundLinkCount()
{
    return m_LinkCount;
}

granny_int32x GSTATE
aimat_ik::GetBoundInactiveLinkCount()
{
    return m_InactiveLinks;
}

granny_int32x GSTATE
aimat_ik::GetOutputPassthrough(granny_int32x OutputIdx) const
{
    return OutputIdx;
}


