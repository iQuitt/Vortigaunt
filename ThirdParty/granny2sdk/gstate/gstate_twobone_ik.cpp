// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_twobone_ik.h"

#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_token_context.h"
#include "gstate_sample_recorder.h"
#include "gstate_telemetry.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE twobone_ik::twobone_ikImpl
{
    granny_real32 DefaultFootPosition[3];

    char* FootName;
    char* KneeName;
    char* HipName;

    granny_int32 KneePlane;
};

granny_data_type_definition GSTATE
twobone_ik::twobone_ikImplType[] =
{
    { GrannyReal32Member, "DefaultFootPosition", 0, 3 },

    { GrannyStringMember, "FootName" },
    { GrannyStringMember, "KneeName" },
    { GrannyStringMember, "HipName"  },

    { GrannyInt32Member, "KneePlane" },
    { GrannyEndMember },
};

// twobone_ik is a concrete class, so we must create a slotted container
struct twobone_ik_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(twobone_ik);
};

granny_data_type_definition twobone_ik::twobone_ikTokenType[] =
{
    DECL_UID_MEMBER(twobone_ik),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(twobone_ik),

    { GrannyEndMember }
};

void GSTATE
twobone_ik::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(twobone_ik);

    // When we take token ownership, future code will assume that it needs to free our
    // owned pointers when they change.  Don't disappoint them.
    GStateCloneString(m_twobone_ikToken->FootName, OldToken->FootName);
    GStateCloneString(m_twobone_ikToken->KneeName, OldToken->KneeName);
    GStateCloneString(m_twobone_ikToken->HipName,  OldToken->HipName);
}

void GSTATE
twobone_ik::ReleaseOwnedToken_twobone_ik()
{
    GStateDeallocate(m_twobone_ikToken->FootName);
    GStateDeallocate(m_twobone_ikToken->KneeName);
    GStateDeallocate(m_twobone_ikToken->HipName);
}

GSTATE
twobone_ik::twobone_ik(token_context*               Context,
                       granny_data_type_definition* TokenType,
                       void*                        TokenObject,
                       token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_twobone_ikToken(0),
    m_tempPose(0),
    m_FootIndex(-1),
    m_KneeIndex(-1),
    m_HipIndex(-1)
{
    IMPL_INIT_FROM_TOKEN(twobone_ik);

    SetFootPosition(m_twobone_ikToken->DefaultFootPosition);

    if (EditorCreated())
    {
        // Add our default input/output
        AddInputImpl(PoseEdge, "Pose");
        AddOutputImpl(PoseEdge, "Pose");
    }
}


GSTATE
twobone_ik::~twobone_ik()
{
    DTOR_RELEASE_TOKEN(twobone_ik);
}


IMPL_CREATE_NODE_BOILERPLATE(twobone_ik);
IMPL_PASSTHROUGH_DIDLOOPOCCUR(twobone_ik, OnOutput == 0);
IMPL_PASSTHROUGH_GETDURATION(twobone_ik, OnOutput == 0);
IMPL_PASSTHROUGH_GETROOTMOTIONVECTORS(twobone_ik, OnOutput == 0);
IMPL_PASSTHROUGH_SAMPLEMORPHINTERFACE(twobone_ik, 0, true);
IMPL_PASSTHROUGH_SAMPLESCALARINTERFACE(twobone_ik, 0, true);
IMPL_PASSTHROUGH_SAMPLEEVENTINTERFACE(twobone_ik, 0, true);


bool GSTATE
twobone_ik::FillDefaultToken(granny_data_type_definition* TokenType,
                                 void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares twobone_ikImpl*& Slot = // member
    GET_TOKEN_SLOT(twobone_ik);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->DefaultFootPosition[0] = 0;
    Slot->DefaultFootPosition[1] = 0;
    Slot->DefaultFootPosition[2] = 10;

    Slot->FootName = 0;
    Slot->KneeName = 0;
    Slot->HipName  = 0;

    Slot->KneePlane = eAimAxis_XAxis;

    return true;
}

bool GSTATE
twobone_ik::BindToCharacter(gstate_character_instance* Instance)
{
    if (!parent::BindToCharacter(Instance))
        return false;

    granny_model* Model = GetModelForCharacterAutomatic(Instance);
    GStateAssert(Model);
    GStateAssert(Model->Skeleton);

    m_tempPose = GrannyNewWorldPoseNoComposite(Model->Skeleton->BoneCount);

    memcpy(m_FootPosition, m_twobone_ikToken->DefaultFootPosition, sizeof(m_FootPosition));
    RefreshBoundValues();

    return true;
}

void GSTATE
twobone_ik::UnbindFromCharacter()
{
    GrannyFreeWorldPose(m_tempPose);
    m_tempPose = 0;

    m_FootIndex = -1;
    m_KneeIndex = -1;
    m_HipIndex  = -1;

    parent::UnbindFromCharacter();
}

granny_real32 GSTATE
twobone_ik::GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx)
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
twobone_ik::SamplePoseOutput(granny_int32x      OutputIdx,
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

    granny_local_pose* Pose = PoseNode->SamplePoseOutput(PoseEdge, AtT, AllowedError, PoseCache, PoseWeight);

    if (Pose && IsActive())
    {
        granny_model* Model = GetModelForCharacterAutomatic(GetBoundCharacter());
        GStateAssert(Model);
        GStateAssert(Model->Skeleton);

        // Build the world pose so we can
        GrannyBuildWorldPose(Model->Skeleton,
                             0, Model->Skeleton->BoneCount,
                             Pose, 0, m_tempPose);

        GrannyIKUpdate2BoneDetailed(m_FootIndex,
                                    m_KneeIndex,
                                    m_HipIndex,
                                    m_FootPosition, GetKneePlaneFloat(),
                                    Model->Skeleton, 0,
                                    Pose, m_tempPose,
                                    0.1f, 0.5f);
    }


#if GSTATE_USE_SAMPLE_RECORDS
    gstate_sample_record * Record = gstate_sample_recorder::AddSampleRecord(this);
    if( Record )
    {
        GStateReplaceString(Record->Name, GetName());
        Record->Category = gstate_sample_record::eSampleRecordCategory_IK;
        Record->Type = gstate_sample_record::eSampleRecordType_IKTwoBone;
    }
#endif //GSTATE_USE_SAMPLE_RECORDS

    return Pose;
}


void GSTATE
twobone_ik::SetFootPosition(granny_real32 const* Position)
{
    for (int i = 0; i < 3; ++i)
        m_FootPosition[i] = Position[i];
}

void GSTATE
twobone_ik::GetFootPosition(granny_real32* Position)
{
    for (int i = 0; i < 3; ++i)
        Position[i] = m_FootPosition[i];
}

void GSTATE
twobone_ik::SetDefaultFootPosition(granny_real32 const* Position)
{
    for (int i = 0; i < 3; ++i)
        m_twobone_ikToken->DefaultFootPosition[i] = Position[i];

    SetFootPosition(Position);
}

void GSTATE
twobone_ik::GetDefaultFootPosition(granny_real32* Position)
{
    for (int i = 0; i < 3; ++i)
        Position[i] = m_twobone_ikToken->DefaultFootPosition[i];
}

void GSTATE
twobone_ik::SetKneePlane(EAimAxis Axis)
{
    m_twobone_ikToken->KneePlane = Axis;
}

EAimAxis GSTATE
twobone_ik::GetKneePlane()
{
    return EAimAxis(m_twobone_ikToken->KneePlane);
}

granny_real32 const* GSTATE
twobone_ik::GetKneePlaneFloat()
{
    return FloatFromAxisEnum(m_twobone_ikToken->KneePlane);
}


void GSTATE
twobone_ik::SetFootName(char const* FootName)
{
    TakeTokenOwnership();

    GStateReplaceString(m_twobone_ikToken->FootName, FootName);

    RefreshBoundValues();
}

void GSTATE
twobone_ik::SetKneeName(char const* KneeName)
{
    TakeTokenOwnership();

    GStateReplaceString(m_twobone_ikToken->KneeName, KneeName);

    RefreshBoundValues();
}

void GSTATE
twobone_ik::SetHipName(char const* HipName)
{
    TakeTokenOwnership();

    GStateReplaceString(m_twobone_ikToken->HipName, HipName);

    RefreshBoundValues();
}

char const* GSTATE
twobone_ik::GetFootName()
{
    return m_twobone_ikToken->FootName;
}

char const* GSTATE
twobone_ik::GetKneeName()
{
    return m_twobone_ikToken->KneeName;
}

char const* GSTATE
twobone_ik::GetHipName()
{
    return m_twobone_ikToken->HipName;
}

bool GSTATE
twobone_ik::IsActive() const
{
    return (m_FootIndex > 0 &&
            m_KneeIndex > 0 &&
            m_HipIndex > 0 &&
            m_FootIndex > m_KneeIndex &&
            m_KneeIndex > m_HipIndex);
}

granny_int32x GSTATE
twobone_ik::GetBoundFootIndex()
{
    return m_FootIndex;
}

granny_int32x GSTATE
twobone_ik::GetBoundKneeIndex()
{
    return m_KneeIndex;
}

granny_int32x GSTATE
twobone_ik::GetBoundHipIndex()
{
    return m_HipIndex;
}


void GSTATE
twobone_ik::RefreshBoundValues()
{
    m_FootIndex = -1;
    m_KneeIndex = -1;
    m_HipIndex = -1;

    gstate_character_instance* Instance = GetBoundCharacter();
    if (!Instance)
        return;

    granny_model* Model = GetModelForCharacterAutomatic(Instance);
    if (Model == 0)
        return;

    if (m_twobone_ikToken->FootName)
    {
        GrannyFindBoneByNameLowercase(Model->Skeleton, m_twobone_ikToken->FootName, &m_FootIndex);
    }
    if (m_twobone_ikToken->KneeName)
    {
        GrannyFindBoneByNameLowercase(Model->Skeleton, m_twobone_ikToken->KneeName, &m_KneeIndex);
    }
    if (m_twobone_ikToken->HipName)
    {
        GrannyFindBoneByNameLowercase(Model->Skeleton, m_twobone_ikToken->HipName, &m_HipIndex);
    }

    if (m_FootIndex == -1 ||
        m_KneeIndex == -1 ||
        m_HipIndex == -1 ||
        m_HipIndex >= m_KneeIndex ||
        m_KneeIndex >= m_FootIndex)
    {
        return;
    }
}

granny_int32x GSTATE
twobone_ik::GetOutputPassthrough(granny_int32x OutputIdx) const
{
    return OutputIdx;
}

