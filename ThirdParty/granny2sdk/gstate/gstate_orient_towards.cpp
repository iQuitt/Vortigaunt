// ========================================================================
// $Notice: $
// ========================================================================
#include "gstate_orient_towards.h"

#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_quick_vecs.h"
#include "gstate_snapshotutils.h"
#include "gstate_token_context.h"
#include "gstate_sample_recorder.h"
#include "gstate_telemetry.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

struct GSTATE orient_towards::orient_towardsImpl
{
    granny_real32 DefaultAimPosition[3];

    granny_int32 BoneSlotCount;
    char**       BoneSlots;

    granny_int32 AimAxis;
};

granny_data_type_definition GSTATE
orient_towards::orient_towardsImplType[] =
{
    { GrannyReal32Member,           "DefaultAimPosition", 0, 3 },
    { GrannyReferenceToArrayMember, "BoneSlots", GrannyStringType },
    { GrannyInt32Member,            "AimAxis" },
    { GrannyEndMember },
};

// orient_towards is a concrete class, so we must create a slotted container
struct orient_towards_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(orient_towards);
};

granny_data_type_definition orient_towards::orient_towardsTokenType[] =
{
    DECL_UID_MEMBER(orient_towards),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(orient_towards),

    { GrannyEndMember }
};

void GSTATE
orient_towards::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(orient_towards);

    // When we take token ownership, future code will assume that it needs to free our
    // owned pointers when they change.  Don't disappoint them.
    GStateCloneArray(m_orient_towardsToken->BoneSlots,
                     OldToken->BoneSlots,
                     m_orient_towardsToken->BoneSlotCount);
    for (int Idx = 0; Idx < m_orient_towardsToken->BoneSlotCount; ++Idx)
    {
        GStateCloneString(m_orient_towardsToken->BoneSlots[Idx], OldToken->BoneSlots[Idx]);
    }
}

void GSTATE
orient_towards::ReleaseOwnedToken_orient_towards()
{
    for (int Idx = 0; Idx < m_orient_towardsToken->BoneSlotCount; ++Idx)
    {
        GStateDeallocate(m_orient_towardsToken->BoneSlots[Idx]);
    }
    GStateDeallocate(m_orient_towardsToken->BoneSlots);
}

IMPL_CREATE_NODE_BOILERPLATE(orient_towards);
IMPL_PASSTHROUGH_DIDLOOPOCCUR(orient_towards, OnOutput == 0);
IMPL_PASSTHROUGH_GETDURATION(orient_towards, OnOutput == 0);
IMPL_PASSTHROUGH_GETROOTMOTIONVECTORS(orient_towards, OnOutput == 0);
IMPL_PASSTHROUGH_SAMPLEMORPHINTERFACE(orient_towards, 0, true);
IMPL_PASSTHROUGH_SAMPLESCALARINTERFACE(orient_towards, 0, true);
IMPL_PASSTHROUGH_SAMPLEEVENTINTERFACE(orient_towards, 0, true);

GSTATE
orient_towards::orient_towards(token_context*               Context,
                               granny_data_type_definition* TokenType,
                               void*                        TokenObject,
                               token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_orient_towardsToken(0),
    m_tempPose(0),
    m_BoneSlots(0)
{
    IMPL_INIT_FROM_TOKEN(orient_towards);

    SetAimPosition(m_orient_towardsToken->DefaultAimPosition);

    if (EditorCreated())
    {
        // Add our default input/output
        AddInputImpl(PoseEdge, "Pose");
        AddOutputImpl(PoseEdge, "Pose");
    }
}


GSTATE
orient_towards::~orient_towards()
{
    DTOR_RELEASE_TOKEN(orient_towards);
}

bool GSTATE
orient_towards::FillDefaultToken(granny_data_type_definition* TokenType,
                                 void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares orient_towardsImpl*& Slot = // member
    GET_TOKEN_SLOT(orient_towards);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->DefaultAimPosition[0] = 0;
    Slot->DefaultAimPosition[1] = 0;
    Slot->DefaultAimPosition[2] = 10;

    Slot->BoneSlotCount = 0;
    Slot->BoneSlots = 0;

    return true;
}

void GSTATE
orient_towards::RefreshBoundValues()
{
    gstate_character_instance* Instance = GetBoundCharacter();
    if (!Instance)
        return;

    granny_model* Model = GetModelForCharacterAutomatic(Instance);
    if (Model == 0)
        return;

    // Re-cache the values
    for (int Idx = 0; Idx < m_orient_towardsToken->BoneSlotCount; ++Idx)
    {
        GStateCheckPtrNotNULL(m_BoneSlots, return);

        m_BoneSlots[Idx] = -1;
        if (m_orient_towardsToken->BoneSlots[Idx])
            GrannyFindBoneByNameLowercase(Model->Skeleton, m_orient_towardsToken->BoneSlots[Idx], &m_BoneSlots[Idx]);
    }
}

bool GSTATE
orient_towards::BindToCharacter(gstate_character_instance* Instance)
{
    if (!parent::BindToCharacter(Instance))
        return false;

    granny_model* Model = GetModelForCharacterAutomatic(Instance);
    GStateAssert(Model);
    GStateAssert(Model->Skeleton);

    m_tempPose  = GrannyNewWorldPoseNoComposite(Model->Skeleton->BoneCount);

    m_BoneSlots = 0;
    if (m_orient_towardsToken->BoneSlotCount != 0)
        m_BoneSlots = GStateAllocArray(granny_int32, m_orient_towardsToken->BoneSlotCount);

    RefreshBoundValues();
    return true;
}

void GSTATE
orient_towards::UnbindFromCharacter()
{
    GrannyFreeWorldPose(m_tempPose);
    m_tempPose = 0;

    GStateDeallocate(m_BoneSlots);
    m_BoneSlots = 0;

    parent::UnbindFromCharacter();
}

granny_real32 GSTATE
orient_towards::GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx)
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
orient_towards::SamplePoseOutput(granny_int32x      OutputIdx,
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

    if (m_orient_towardsToken->BoneSlotCount != 0 && Pose != NULL)
    {
        // Build the world pose so we can do ik
        GrannyBuildWorldPose(Model->Skeleton,
                             0, Model->Skeleton->BoneCount,
                             Pose, 0, m_tempPose);
        for (int Idx = 0; Idx < m_orient_towardsToken->BoneSlotCount; ++Idx)
        {
            if (m_BoneSlots[Idx] == -1)
                continue;

            GrannyIKOrientTowards(m_BoneSlots[Idx],
                                  FloatFromAxisEnum(m_orient_towardsToken->AimAxis),
                                  m_AimPosition,
                                  Model->Skeleton, Pose, 0, m_tempPose);
        }
    }    

#if GSTATE_USE_SAMPLE_RECORDS
    gstate_sample_record * Record = gstate_sample_recorder::AddSampleRecord(this);
    if( Record )
    {
        GStateReplaceString(Record->Name, GetName());
        Record->Category = gstate_sample_record::eSampleRecordCategory_IK;
        Record->Type = gstate_sample_record::eSampleRecordType_IKOrientTowards;
    }
#endif //GSTATE_USE_SAMPLE_RECORDS

    return Pose;
}

CREATE_SNAPSHOT(orient_towards)
{
    CREATE_WRITE_REAL32_ARRAY(m_AimPosition, 3);
    CREATE_PASS_TO_PARENT();
}

RESET_FROMSNAPSHOT(orient_towards)
{
    RESET_OFFSET_TRACKING();
    RESET_READ_REAL32_ARRAY(m_AimPosition, 3);
    RESET_PASS_TO_PARENT();
}

void GSTATE
orient_towards::SetAimPosition(granny_real32 const* Position)
{
    for (int i = 0; i < 3; ++i)
        m_AimPosition[i] = Position[i];
}

void GSTATE
orient_towards::GetAimPosition(granny_real32* Position)
{
    for (int i = 0; i < 3; ++i)
        Position[i] = m_AimPosition[i];
}

void GSTATE
orient_towards::SetDefaultAimPosition(granny_real32 const* Position)
{
    TakeTokenOwnership();

    for (int i = 0; i < 3; ++i)
        m_orient_towardsToken->DefaultAimPosition[i] = Position[i];

    SetAimPosition(Position);
}

void GSTATE
orient_towards::GetDefaultAimPosition(granny_real32* Position)
{
    for (int i = 0; i < 3; ++i)
        Position[i] = m_orient_towardsToken->DefaultAimPosition[i];
}

void GSTATE
orient_towards::SetAimAxis(EAimAxis Axis)
{
    TakeTokenOwnership();

    m_orient_towardsToken->AimAxis = Axis;
}

GSTATE EAimAxis GSTATE
orient_towards::GetAimAxis()
{
    GStateAssert(m_orient_towardsToken->AimAxis >= eAimAxis_XAxis &&
                 m_orient_towardsToken->AimAxis <= eAimAxis_NegZAxis);

    return EAimAxis(m_orient_towardsToken->AimAxis);
}

granny_real32 const* GSTATE
orient_towards::GetAimAxisFloat()
{
    return FloatFromAxisEnum(m_orient_towardsToken->AimAxis);
}


void GSTATE
orient_towards::AddBoneSlot()
{
    TakeTokenOwnership();

    // Update the cache first so we can use that count...
    granny_int32& NewBoneIndex = QVecPushNewElementNoCount(m_orient_towardsToken->BoneSlotCount, m_BoneSlots);
    char*& NewSlot = QVecPushNewElement(m_orient_towardsToken->BoneSlotCount, m_orient_towardsToken->BoneSlots);
    NewBoneIndex = -1;
    NewSlot = 0;
}

bool GSTATE
orient_towards::DeleteBoneSlot(int DeleteSlot)
{
    GStateCheckIndex(DeleteSlot, m_orient_towardsToken->BoneSlotCount, return false);

    TakeTokenOwnership();

    // We need to free the name pointer of the deleted input first
    {
        GStateDeallocate(m_orient_towardsToken->BoneSlots[DeleteSlot]);
        m_orient_towardsToken->BoneSlots[DeleteSlot] = 0;
    }

    QVecRemoveElementNoCount(DeleteSlot, m_orient_towardsToken->BoneSlotCount, m_BoneSlots);
    QVecRemoveElement(DeleteSlot, m_orient_towardsToken->BoneSlotCount, m_orient_towardsToken->BoneSlots);

    return true;
}

int GSTATE
orient_towards::GetNumBoneSlots()
{
    return m_orient_towardsToken->BoneSlotCount;
}

void GSTATE
orient_towards::SetBoneName(int SlotIdx, char const* BoneName)
{
    GStateCheckIndex(SlotIdx, m_orient_towardsToken->BoneSlotCount, return);

    TakeTokenOwnership();
    
    GStateReplaceString(m_orient_towardsToken->BoneSlots[SlotIdx], BoneName);

    RefreshBoundValues();
}


char const* GSTATE
orient_towards::GetBoneName(int SlotIdx)
{
    GStateCheckIndex(SlotIdx, m_orient_towardsToken->BoneSlotCount, return 0);
    return m_orient_towardsToken->BoneSlots[SlotIdx];
}

granny_int32 GSTATE
orient_towards::GetBoneIndex(int SlotIdx)
{
    GStateCheckIndex(SlotIdx, m_orient_towardsToken->BoneSlotCount, return 0);
    return m_BoneSlots[SlotIdx];
}


granny_int32x GSTATE
orient_towards::GetOutputPassthrough(granny_int32x OutputIdx) const
{
    return OutputIdx;
}


