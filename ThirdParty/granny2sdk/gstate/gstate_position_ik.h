// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_position_ik.h $
// $DateTime: 2016/04/04 12:42:49 $
// $Change: 57386 $
// $Revision: #11 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_chain_ik_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

#if !defined(GSTATE_IK_SHARED_H)
#include "gstate_ik_shared.h"
#endif


BEGIN_GSTATE_NAMESPACE;

struct position_ik_token;

enum position_ik_offset_type
{
    PositionIKOffset_Target,       //Offset is in the local space of the target
    PositionIKOffset_Effector,     //Offset is in the local space of the end effector joint

    PositionIKOffset_NumTypes
};

char const* NameForPositionIKOffset(position_ik_offset_type);

enum position_ik_target_type
{
    PositionIKTarget_Bone,         //Target is a cached transform name from this skeleton
    PositionIKTarget_Name,         //Target is a user-specified cached transform name
    PositionIKTarget_User,         //Target is user-specified (code- or slider-driven)

    PositionIKTarget_NumTypes
};

char const* NameForPositionIKTarget(position_ik_target_type);

class position_ik : public node
{
    typedef node parent;

public:
    virtual bool BindToCharacter(gstate_character_instance* Instance);
    virtual void UnbindFromCharacter();

    DECL_SAMPLEPOSE_INTERFACE();
    DECL_CONCRETE_NODE_TOKEN(position_ik);
    DECL_SNAPPABLE();
    DECL_SAMPLEMORPH_INTERFACE();
    DECL_SAMPLEEVENT_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();

    granny_real32 GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx);

    // For updating tokens on load. Converts target matrices that get the token initializer will create as 
    //zero into identity matrices
    static void UpgradePositionIKNode(position_ik_token* Token);

    virtual bool AllowSyncInputsWithParentOutputs() const { return true; }
    virtual bool AllowSyncOutputsWithParentOutputs() const { return true; }


    bool GetTargetPose4x4(granny_real32* Pose4x4);
    void GetOffsetFromTargetPose(granny_real32* TargetPose4x4, granny_real32*  OffsetPosition);

    bool GetOffsetTargetPosition(granny_real32*);

    // All of these affect the token
    void SetTargetPosition(granny_real32 const* Position);
    void GetTargetPosition(granny_real32*);

    void SetDefaultTargetMat4x4(granny_real32 const* Mat4x4);
    void GetDefaultTargetMat4x4(granny_real32*) const;

    void SetOffset(granny_real32 const* Position);
    void GetOffset(granny_real32*) const;

    void SetPositionSliderRange(granny_real32 SliderRange);
    granny_real32 GetPositionSliderRange() const;

    void SetOffsetSliderRange(granny_real32 SliderRange);
    granny_real32 GetOffsetSliderRange() const;

    void SetTargetJoint(const char * JointName);
    char * GetTargetJoint();

    void SetTargetType(position_ik_target_type TargetType);
    position_ik_target_type GetTargetType() const;

    void SetOffsetType(position_ik_offset_type OffsetType);
    position_ik_offset_type GetOffsetType() const;

    bool HasTargetJoint() const;

    void SetEndEffectorName(char const* EndEffectorName);

    char const* GetEndEffectorName();

    bool IsActive() const;

    // These will be valid when IsActive is true
    granny_int32x GetBoundEndEffectorIndex() const;
    granny_int32x GetJointChainLength() const;
    granny_int32x GetJointChainEntry(granny_int32x JointIndex) const;

    granny_int32 AddAffectedJointEntry();
    void RemoveAffectedJointEntry(granny_int32 AffectedJointIndex);
    granny_int32 GetNumAffectedJoints() const;
    const char * GetAffectedJointName(granny_int32 AffectedJointIndex) const;
    void SetAffectedJointName(granny_int32 AffectedJointIndex, const char * JointName);

    granny_int32x GetIterations() const;
    void SetIterations(granny_int32x Iterations);

    granny_bool32 GetIterateTargetPosition() const;
    void SetIterateTargetPosition(granny_bool32 IterateTargetPosition);

    void AutoSizeSliderRange();

public:
    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;

private:
    granny_world_pose* TempPose;

    //JointChain is a list of joints, possibly sparse, starting with the end effector at JointChain[0] and with the start joint at JointChain[JointChainLength-1]
    //0 entry is the end effector, subsequent entries are ancestor joints. 
    granny_int32x  JointChainLength;
    granny_int32x* JointChain; 

    //FullJointChainToRoot is every joint in the skeleton between JointChain[0] and the root. JointChain[JointChainLength-1] appears at FullJointChain[FullJointChainToStartLength-1]
    granny_int32x  FullJointChainToRootLength;
    granny_int32x  FullJointChainToStartLength;
    granny_int32x* FullJointChainToRoot;

    void RefreshBoundValues();
};

END_GSTATE_NAMESPACE;


#include "gstate_header_postfix.h"
#define GSTATE_chain_ik_H
#endif /* GSTATE_chain_ik_H */
