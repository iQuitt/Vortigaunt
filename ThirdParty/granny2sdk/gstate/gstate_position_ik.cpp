// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_position_ik.cpp $
// $DateTime: 2016/04/04 12:42:49 $
// $Change: 57386 $
// $Revision: #13 $
//
// $Notice: $
// ========================================================================
#include "gstate_position_ik.h"

#include "gstate_character_instance.h"
#include "gstate_node_visitor.h"
#include "gstate_snapshotutils.h"
#include "gstate_quick_vecs.h"
#include "gstate_token_context.h"
#include "gstate_math.h"
#include "gstate_sample_recorder.h"
#include "gstate_telemetry.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;


struct position_ik_joint
{
    char* Name;
};

granny_data_type_definition PositionIKJointType[] =
{
    { GrannyStringMember,   "JointName" },
    { GrannyEndMember }
};

struct GSTATE position_ik::position_ikImpl
{
    granny_int32  TargetType;
    granny_real32 TargetMat4x4[16];
    granny_real32 Offset[3];
    granny_int32  OffsetType;
    granny_real32 PositionSliderRange;
    granny_real32 OffsetSliderRange;

    char* TargetJoint; ///Overrides aim position if specified. 

    char* EndEffector;

    granny_int32 ActiveJointCount;
    position_ik_joint * ActiveJoints;

    granny_int32 Iterations;
    granny_bool32 IterateTargetPosition;
};

granny_data_type_definition GSTATE
position_ik::position_ikImplType[] =
{
    { GrannyInt32Member,  "TargetType" },
    { GrannyReal32Member, "TargetMat4x4", 0, 16 },
    { GrannyReal32Member, "Offset", 0, 3 },
    { GrannyInt32Member,  "OffsetType" },
    { GrannyReal32Member, "PositionSliderRange" },
    { GrannyReal32Member, "OffsetSliderRange" },

    { GrannyStringMember, "TargetJoint" },

    { GrannyStringMember, "EndEffector" },

    { GrannyReferenceToArrayMember, "ActiveJoints", PositionIKJointType },

    { GrannyInt32Member, "Iterations" },

    { GrannyBool32Member, "IterateTargetPosition" },

    { GrannyEndMember },
};

// position_ik is a concrete class, so we must create a slotted container
struct GSTATE position_ik_token
{
    DECL_UID();
    DECL_OPAQUE_TOKEN_SLOT(node);
    DECL_TOKEN_SLOT(position_ik);
};

granny_data_type_definition position_ik::position_ikTokenType[] =
{
    DECL_UID_MEMBER(position_ik),
    DECL_TOKEN_MEMBER(node),
    DECL_TOKEN_MEMBER(position_ik),

    { GrannyEndMember }
};

void GSTATE
position_ik::TakeTokenOwnership()
{
    TAKE_TOKEN_OWNERSHIP(position_ik);

    // When we take token ownership, future code will assume that it needs to free our
    // owned pointers when they change.  Don't disappoint them.
    GStateCloneString(m_position_ikToken->TargetJoint,  OldToken->TargetJoint);
    GStateCloneString(m_position_ikToken->EndEffector,        OldToken->EndEffector);

    GStateCloneArray(m_position_ikToken->ActiveJoints,
        OldToken->ActiveJoints,
        m_position_ikToken->ActiveJointCount);

    for (int Idx = 0; Idx < m_position_ikToken->ActiveJointCount; ++Idx)
    {
        GStateCloneString(m_position_ikToken->ActiveJoints[Idx].Name, OldToken->ActiveJoints[Idx].Name);
    }
}

void GSTATE
position_ik::ReleaseOwnedToken_position_ik()
{
    GStateDeallocate(m_position_ikToken->TargetJoint);
    GStateDeallocate(m_position_ikToken->EndEffector);

    for (int Idx = 0; Idx < m_position_ikToken->ActiveJointCount; ++Idx)
    {
        GStateDeallocate(m_position_ikToken->ActiveJoints[Idx].Name);
    }
    GStateDeallocate(m_position_ikToken->ActiveJoints);
}

IMPL_CREATE_NODE_BOILERPLATE(position_ik);
IMPL_PASSTHROUGH_DIDLOOPOCCUR(position_ik, OnOutput == 0);
IMPL_PASSTHROUGH_GETDURATION(position_ik, OnOutput == 0);
IMPL_PASSTHROUGH_GETROOTMOTIONVECTORS(position_ik, OnOutput == 0);
IMPL_PASSTHROUGH_SAMPLEMORPHINTERFACE(position_ik, 0, true);
IMPL_PASSTHROUGH_SAMPLESCALARINTERFACE(position_ik, 0, true);
IMPL_PASSTHROUGH_SAMPLEEVENTINTERFACE(position_ik, 0, true);

char const* GSTATE
NameForPositionIKOffset(position_ik_offset_type Type)
{
    switch (Type)
    {
    case PositionIKOffset_Target:   return "From Target";
    case PositionIKOffset_Effector: return "From Effector";
    default:
        GS_InvalidCodePath("Unknown position ik offset type");
        return "Unknown";
    }
}

char const* GSTATE
NameForPositionIKTarget(position_ik_target_type Type)
{
    switch (Type)
    {
    case PositionIKTarget_User: return "User Specified";
    case PositionIKTarget_Bone: return "Bone Name";
    case PositionIKTarget_Name: return "Custom Name";
    default:
        GS_InvalidCodePath("Unknown position ik target type");
        return "Unknown";
    }
}

/*static*/ void GSTATE
position_ik::UpgradePositionIKNode(position_ik_token* Token)
{
    //If upgrading a node that that has all-zero for the target mat, make it identity. 
    bool AllZero = true;
    for( int i=0; i<16; i++)
    {
        if( Token->position_ikToken->TargetMat4x4[i] != 0.0f )
        {
            AllZero = false;
            break;
        }
    }

    if( AllZero )
    {
        GStateSetIdentity4x4(Token->position_ikToken->TargetMat4x4);
    }
}

void EnsureLegalSliderRange( granny_real32 * Range)
{
    if( *Range <= 0.0f )
    {
        *Range = 10.0f;
    }
}

void AutoSizeSliderRange3( granny_real32 * Value3, granny_real32 * Range)
{
    if( fabs(Value3[0]) >  *Range )
    {
        *Range = ceilf(fabsf(Value3[0]));
    }
    if( fabs(Value3[1]) > *Range )
    {
        *Range = ceilf(fabsf(Value3[1]));
    }
    if( fabs(Value3[2]) > *Range )
    {
        *Range = ceilf(fabsf(Value3[2]));
    }

    EnsureLegalSliderRange(Range);
}


GSTATE
position_ik::position_ik(token_context*               Context,
                   granny_data_type_definition* TokenType,
                   void*                        TokenObject,
                   token_ownership              TokenOwnership)
  : parent(Context, TokenType, TokenObject, TokenOwnership),
    m_position_ikToken(0),
    TempPose(0),
    JointChainLength(0),
    JointChain(0),
    FullJointChainToRootLength(0),
    FullJointChainToStartLength(0),
    FullJointChainToRoot(0)
{
    IMPL_INIT_FROM_TOKEN(position_ik);

    if (EditorCreated())
    {
        // Add our default input/output
        AddInputImpl(PoseEdge, "Pose");
        AddOutputImpl(PoseEdge, "Pose");

        //Add the end effector and one joint. IK does nothing without these. 
        AddAffectedJointEntry();
        AddAffectedJointEntry();

        m_position_ikToken->Iterations = 5;
        m_position_ikToken->IterateTargetPosition = false;

        EnsureLegalSliderRange(&m_position_ikToken->PositionSliderRange);
        EnsureLegalSliderRange(&m_position_ikToken->OffsetSliderRange);

        GStateSetIdentity4x4(m_position_ikToken->TargetMat4x4);
    }
    else
    {
        AutoSizeSliderRange3( &m_position_ikToken->TargetMat4x4[12], &m_position_ikToken->PositionSliderRange );
        AutoSizeSliderRange3( &m_position_ikToken->Offset[0], &m_position_ikToken->OffsetSliderRange );
    }
}


GSTATE
position_ik::~position_ik()
{
    DTOR_RELEASE_TOKEN(position_ik);
}

bool GSTATE
position_ik::FillDefaultToken(granny_data_type_definition* TokenType,
                           void* TokenObject)
{
    if (!parent::FillDefaultToken(TokenType, TokenObject))
        return false;

    // Declares position_ikImpl*& Slot = // member
    GET_TOKEN_SLOT(position_ik);

    // Our slot in this token should be empty.
    // Create a new mask invert Token
    GStateAssert(Slot == 0);
    GStateAllocZeroedStruct(Slot);

    Slot->TargetMat4x4[0] = 1.0f;
    Slot->TargetMat4x4[1] = 0.0f;
    Slot->TargetMat4x4[2] = 0.0f;
    Slot->TargetMat4x4[3] = 0.0f;

    Slot->TargetMat4x4[4] = 0.0f;
    Slot->TargetMat4x4[5] = 1.0f;
    Slot->TargetMat4x4[6] = 0.0f;
    Slot->TargetMat4x4[7] = 0.0f;

    Slot->TargetMat4x4[8] = 0.0f;
    Slot->TargetMat4x4[9] = 0.0f;
    Slot->TargetMat4x4[10] = 1.0f;
    Slot->TargetMat4x4[11] = 0.0f;

    Slot->TargetMat4x4[12] = 0.0f;
    Slot->TargetMat4x4[13] = 0.0f;
    Slot->TargetMat4x4[14] = 10.0f;
    Slot->TargetMat4x4[15] = 1.0f;

    Slot->EndEffector = 0;

    Slot->ActiveJointCount = 0;
    Slot->ActiveJoints = NULL;

    return true;
}

void GSTATE
position_ik::RefreshBoundValues()
{
    GStateDeallocate(JointChain);    
    GStateDeallocate(FullJointChainToRoot);    
    JointChainLength = 0;
    FullJointChainToRootLength = 0;
    FullJointChainToStartLength = 0;

    gstate_character_instance* Instance = GetBoundCharacter();
    if (!Instance)
        return;

    granny_model* Model = GetModelForCharacterAutomatic(Instance);
    if (Model == 0)
        return;

    if( m_position_ikToken->ActiveJointCount == 0 )
        return;

    JointChain = GStateAllocArray(granny_int32x,m_position_ikToken->ActiveJointCount); //Maybe a little too big because some of the active joints might be invalid. But that's ok. 

    for( int ActiveJointIdx=0; ActiveJointIdx<m_position_ikToken->ActiveJointCount; ActiveJointIdx++ )
    {
        granny_int32x ActiveJointBoneIndex = -1;
        if( GrannyFindBoneByNameLowercase(Model->Skeleton, m_position_ikToken->ActiveJoints[ActiveJointIdx].Name, &ActiveJointBoneIndex) )
        {
            JointChain[JointChainLength] = ActiveJointBoneIndex;
            JointChainLength++;
        }
    }

    //No work to do until you get a joint chain with an end effector and an ancestor node. 
    if( JointChainLength < 2 )
        return;

    granny_int32x RootEffectorTempChain[2];
    RootEffectorTempChain[0] = JointChain[0]; //0 index of the joint chain is the end effector
    RootEffectorTempChain[1] = 0; //Bone 0 is the root
    granny_int32x * FullJointChainToRootUpperBound = GStateAllocArray(granny_int32x,Model->Skeleton->BoneCount); //BoneCount is an upper bound on the length of the longest chain from end effector to root. 

    FullJointChainToRootLength = GrannyBuildFullJointChainFromJointChain(Model->Skeleton, JointChainLength, RootEffectorTempChain, Model->Skeleton->BoneCount, FullJointChainToRootUpperBound );
    FullJointChainToRoot = GStateAllocArray(granny_int32x,FullJointChainToRootLength); 

    for( int FullJointChainIdx=0; FullJointChainIdx<FullJointChainToRootLength; FullJointChainIdx++ )
    {
        FullJointChainToRoot[FullJointChainIdx] = FullJointChainToRootUpperBound[FullJointChainIdx];
        if( FullJointChainToRoot[FullJointChainIdx] == JointChain[JointChainLength-1] )
        {
            FullJointChainToStartLength = FullJointChainIdx+1;
        }
    }

    GStateDeallocate(FullJointChainToRootUpperBound);
}

bool GSTATE
position_ik::BindToCharacter(gstate_character_instance* Instance)
{
    if (!parent::BindToCharacter(Instance))
        return false;

    granny_model* Model = GetModelForCharacterAutomatic(Instance);
    GStateAssert(Model);
    GStateAssert(Model->Skeleton);

    TempPose = GrannyNewWorldPoseNoComposite(Model->Skeleton->BoneCount);

    RefreshBoundValues();

    return true;
}

void GSTATE
position_ik::UnbindFromCharacter()
{
    GrannyFreeWorldPose(TempPose);
    TempPose = 0;

    GStateDeallocate(JointChain);
    GStateDeallocate(FullJointChainToRoot);
    JointChainLength = 0;
    FullJointChainToRootLength = 0;
    FullJointChainToStartLength = 0;

    parent::UnbindFromCharacter();
}

//Offset the position of the transform by a scaled amount of each axis of the transform's rotation.
inline void GrannyOffsetFromTransformByScaledRotationAxes4x3(granny_real32 * Dest, granny_real32 const * Scale, granny_real32 const * Transform4x4)
{
    //Dest and Offset can be the same memory, so use Offset before we write to Dest.
    granny_real32 Bx[3];
    granny_real32 By[3];
    granny_real32 Bz[3];

    //Scale each component axis of the world space pose basis
    GStateVectorScale3(&Bx[0], Scale[0], &Transform4x4[0]);
    GStateVectorScale3(&By[0], Scale[1], &Transform4x4[4]);
    GStateVectorScale3(&Bz[0], Scale[2], &Transform4x4[8]);

    //Start at the world pose position
    GStateVectorEquals3(Dest,&Transform4x4[12]);

    //Offset by the scaled basis
    GStateVectorAdd3(Dest,&Bx[0]);
    GStateVectorAdd3(Dest,&By[0]);
    GStateVectorAdd3(Dest,&Bz[0]);
}

granny_real32 GSTATE
position_ik::GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx)
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
position_ik::SamplePoseOutput(granny_int32x      OutputIdx,
                           granny_real32      AtT,
                           granny_real32      AllowedError,
                           granny_pose_cache* PoseCache,
                           granny_real32*     PoseWeight)
{
    GSTATE_AUTO_ZONE_FN_OR_NAME();

    GStateAssert(OutputIdx == 0);
    GStateAssert(PoseCache != 0);
    GStateAssert(TempPose != 0);

    node* PoseNode = 0;
    granny_int32x PoseEdge = -1;
    GetInputConnection(0, &PoseNode, &PoseEdge);

    if (!PoseNode)
        return 0;

    granny_model* Model = GetModelForCharacterAutomatic(GetBoundCharacter());
    GStateAssert(Model);
    GStateAssert(Model->Skeleton);

    granny_local_pose* Pose = PoseNode->SamplePoseOutput(PoseEdge, AtT, AllowedError, PoseCache, PoseWeight);

    granny_real32 TargetPosition[3];
    bool TargetPositionValid = GetOffsetTargetPosition(TargetPosition);

    if (Pose && IsActive() && TargetPositionValid)
    {
        //Build just enough of the world pose to do ik (from the root to the end effector)
        GrannyUpdateWorldPoseChain(Model->Skeleton, FullJointChainToRootLength-1, FullJointChainToRootLength, FullJointChainToRoot, Pose, 0, TempPose);

        granny_real32 OffsetPosition[3];
        granny_real32 * EffectorOffset = NULL;
        if( GetOffsetType() == PositionIKOffset_Effector )
        {
            GetOffset(&OffsetPosition[0]);
            EffectorOffset = &OffsetPosition[0];
        }

        GrannyIKUpdateChainDetailed(JointChainLength, JointChain, FullJointChainToStartLength, FullJointChainToRoot, EffectorOffset, TargetPosition, GetIterateTargetPosition(), GetIterations(), Model->Skeleton, 0, Pose, TempPose);
    }

#if GSTATE_USE_SAMPLE_RECORDS
    gstate_sample_record * Record = gstate_sample_recorder::AddSampleRecord(this);
    if( Record )
    {
        GStateReplaceString(Record->Name, GetName());
        Record->Category = gstate_sample_record::eSampleRecordCategory_IK;
        Record->Type = gstate_sample_record::eSampleRecordType_IKPosition;
    }
#endif //GSTATE_USE_SAMPLE_RECORDS

    return Pose;
}


CREATE_SNAPSHOT(position_ik)
{
    CREATE_WRITE_INT32(JointChainLength);
    CREATE_WRITE_INT32_ARRAY(JointChain, JointChainLength);
    CREATE_PASS_TO_PARENT();
}

RESET_FROMSNAPSHOT(position_ik)
{
    RESET_OFFSET_TRACKING();

    RESET_READ_INT32(JointChainLength);
    RESET_READ_REAL32_ARRAY(JointChain, JointChainLength);
    RESET_PASS_TO_PARENT();
}

bool GSTATE
position_ik::GetTargetPose4x4(granny_real32* TargetPose)
{
    bool TargetPositionValid = false;
    if( GetTargetType() != PositionIKTarget_User && HasTargetJoint() )
    {
        gstate_character_instance* Instance = GetBoundCharacter();
        if( GStateGetCachedMat4x4(Instance, m_position_ikToken->TargetJoint, &TargetPose[0] ) )
        {
            TargetPositionValid = true;
        }
    }
    else
    {
        memcpy(TargetPose,m_position_ikToken->TargetMat4x4,sizeof(TargetPose[0]) * 16);

        TargetPositionValid = true;
    }

    return TargetPositionValid;
}

void position_ik::GetOffsetFromTargetPose(granny_real32* TargetPose4x4, granny_real32* OffsetPosition)
{
    if( GetOffsetType() == PositionIKOffset_Target )
    {
        granny_real32 Offset[3];
        GetOffset(&Offset[0]);
        GrannyOffsetFromTransformByScaledRotationAxes4x3(&OffsetPosition[0], &Offset[0], &TargetPose4x4[0]);
    }
    else
    {
        OffsetPosition[0] = TargetPose4x4[12];
        OffsetPosition[1] = TargetPose4x4[13];
        OffsetPosition[2] = TargetPose4x4[14];
    }
}

bool GSTATE
position_ik::GetOffsetTargetPosition(granny_real32* TargetPosition)
{
    granny_real32 TargetPose[16];
    bool TargetPositionValid = GetTargetPose4x4(TargetPose);

    if( TargetPositionValid )
    {
        GetOffsetFromTargetPose(TargetPose, TargetPosition);
    }

    return TargetPositionValid;
}



void GSTATE
position_ik::SetTargetPosition(granny_real32 const* Position)
{
    for (int i = 0; i < 3; ++i)
        m_position_ikToken->TargetMat4x4[12 + i] = Position[i];
}

void GSTATE
position_ik::GetTargetPosition(granny_real32* Position)
{
    for (int i = 0; i < 3; ++i)
        Position[i] = m_position_ikToken->TargetMat4x4[12 + i];
}

void GSTATE
position_ik::SetDefaultTargetMat4x4(granny_real32 const* Mat4x4)
{
    memcpy(m_position_ikToken->TargetMat4x4, Mat4x4, sizeof(m_position_ikToken->TargetMat4x4[0]) * 16);
}

void GSTATE
position_ik::GetDefaultTargetMat4x4(granny_real32* Mat4x4) const
{
    memcpy(Mat4x4, m_position_ikToken->TargetMat4x4, sizeof(m_position_ikToken->TargetMat4x4[0]) * 16);
}

void GSTATE
position_ik::SetOffset(granny_real32 const* Offset)
{
    for (int i = 0; i < 3; ++i)
        m_position_ikToken->Offset[i] = Offset[i];
}

void GSTATE
position_ik::GetOffset(granny_real32* Offset) const
{
    for (int i = 0; i < 3; ++i)
        Offset[i] = m_position_ikToken->Offset[i];
}

void GSTATE
position_ik::SetTargetJoint(const char * JointName)
{
    TakeTokenOwnership();
    GStateReplaceString(m_position_ikToken->TargetJoint, JointName);
}

char * GSTATE
position_ik::GetTargetJoint()
{
    return m_position_ikToken->TargetJoint;
}

void GSTATE
position_ik::SetPositionSliderRange(granny_real32 SliderRange)
{
    TakeTokenOwnership();
    EnsureLegalSliderRange(&SliderRange);
    m_position_ikToken->PositionSliderRange = SliderRange;
}

granny_real32 GSTATE
position_ik::GetPositionSliderRange() const
{
    return m_position_ikToken->PositionSliderRange;
}

void GSTATE
position_ik::SetOffsetSliderRange(granny_real32 SliderRange)
{
    TakeTokenOwnership();
    EnsureLegalSliderRange(&SliderRange);
    m_position_ikToken->OffsetSliderRange = SliderRange;
}

granny_real32 GSTATE
position_ik::GetOffsetSliderRange() const
{
    return m_position_ikToken->OffsetSliderRange;
}


void GSTATE
position_ik::SetTargetType(position_ik_target_type TargetType)
{
    GStateAssert(TargetType >= 0 && TargetType < PositionIKTarget_NumTypes);
    m_position_ikToken->TargetType = (granny_int32)TargetType;
}

position_ik_target_type GSTATE
position_ik::GetTargetType() const
{
    return(position_ik_target_type)m_position_ikToken->TargetType;
}

void GSTATE
position_ik::SetOffsetType(position_ik_offset_type OffsetType)
{
    GStateAssert(OffsetType >= 0 && OffsetType < PositionIKOffset_NumTypes);
    m_position_ikToken->OffsetType = (granny_int32)OffsetType;
}

position_ik_offset_type GSTATE
position_ik::GetOffsetType() const
{
    return(position_ik_offset_type)m_position_ikToken->OffsetType;
}

bool GSTATE
position_ik::HasTargetJoint() const
{
    return m_position_ikToken->TargetJoint != NULL && m_position_ikToken->TargetJoint[0] != 0;
}


void GSTATE
position_ik::SetEndEffectorName(char const* EndEffector)
{
    TakeTokenOwnership();

    GStateReplaceString(m_position_ikToken->EndEffector, EndEffector);

    RefreshBoundValues();
}

char const* GSTATE
position_ik::GetEndEffectorName()
{
    return m_position_ikToken->EndEffector;
}

bool GSTATE
position_ik::IsActive() const
{
    return JointChainLength > 1; //Need an end effector (0 index) and at least one joint above it to rotate. 
}

granny_int32x GSTATE
position_ik::GetBoundEndEffectorIndex() const
{
    if( JointChainLength <= 0 )
        return -1;

    return JointChain[0];
}

granny_int32x GSTATE
position_ik::GetJointChainLength() const
{
    return JointChainLength;
}

granny_int32x GSTATE
position_ik::GetJointChainEntry(granny_int32x JointIndex) const
{
    GStateAssert( JointIndex >= 0 && JointIndex < JointChainLength );
    return JointChain[JointIndex];
}

granny_int32 GSTATE
position_ik::AddAffectedJointEntry()
{
    TakeTokenOwnership();

    position_ik_joint& NewJointEntry = QVecPushNewElement(m_position_ikToken->ActiveJointCount, m_position_ikToken->ActiveJoints);
    GStateCloneString(NewJointEntry.Name, "");

    return m_position_ikToken->ActiveJointCount-1;
}

void GSTATE
position_ik::RemoveAffectedJointEntry(granny_int32 AffectedJointIndex)
{
    TakeTokenOwnership();

    GStateDeallocate(m_position_ikToken->ActiveJoints[AffectedJointIndex].Name);

    QVecRemoveElement(AffectedJointIndex, m_position_ikToken->ActiveJointCount, m_position_ikToken->ActiveJoints);

    RefreshBoundValues();
}

granny_int32 GSTATE
position_ik::GetNumAffectedJoints() const
{
    return m_position_ikToken->ActiveJointCount;
}

const char * GSTATE
position_ik::GetAffectedJointName(granny_int32 AffectedJointIndex) const
{
    return m_position_ikToken->ActiveJoints[AffectedJointIndex].Name;
}

void GSTATE
position_ik::SetAffectedJointName(granny_int32 AffectedJointIndex, const char * JointName)
{
    TakeTokenOwnership();

    GStateReplaceString(m_position_ikToken->ActiveJoints[AffectedJointIndex].Name, JointName);

    RefreshBoundValues();
}

granny_int32x GSTATE
position_ik::GetIterations() const
{
    return m_position_ikToken->Iterations;
}

void GSTATE
position_ik::SetIterations(granny_int32x Iterations)
{
    m_position_ikToken->Iterations = Iterations;
}


granny_bool32 GSTATE
position_ik::GetIterateTargetPosition() const
{
    return m_position_ikToken->IterateTargetPosition;
}

void GSTATE
position_ik::SetIterateTargetPosition(granny_bool32 IterateTargetPosition)
{
    m_position_ikToken->IterateTargetPosition = IterateTargetPosition;
}


granny_int32x GSTATE
position_ik::GetOutputPassthrough(granny_int32x OutputIdx) const
{
    return OutputIdx;
}


