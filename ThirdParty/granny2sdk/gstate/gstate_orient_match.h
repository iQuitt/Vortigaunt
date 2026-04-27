// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_orient_match.h $
// $DateTime: 2013/04/15 14:26:51 $
// $Change: 42530 $
// $Revision: #8 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_ORIENT_MATCH_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

#if !defined(GSTATE_IK_SHARED_H)
#include "gstate_ik_shared.h"
#endif


BEGIN_GSTATE_NAMESPACE;

enum orient_match_target_type
{
    OrientMatchTarget_Bone,         //Target is a cached transform name from this skeleton
    OrientMatchTarget_Name,         //Target is a user-specified cached transform name
    OrientMatchTarget_User,         //Target is user-specified (code- or slider-driven)

    OrientMatchTarget_NumTypes
};

char const* NameForOrientMatchTarget(orient_match_target_type);

class orient_match : public node
{
    typedef node parent;

public:
    virtual bool BindToCharacter(gstate_character_instance* Instance);
    virtual void UnbindFromCharacter();

    DECL_SAMPLEPOSE_INTERFACE();
    DECL_CONCRETE_NODE_TOKEN(orient_match);
    DECL_SNAPPABLE();
    DECL_SAMPLEMORPH_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();
    DECL_SAMPLEEVENT_INTERFACE();

    granny_real32 GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx);

    virtual bool AllowSyncInputsWithParentOutputs() const { return true; }
    virtual bool AllowSyncOutputsWithParentOutputs() const { return true; }

    void SetTargetBone(const char * JointName);
    char * GetTargetBone();

    bool HasTargetBone() const;

    void SetTargetType(orient_match_target_type TargetType);
    orient_match_target_type GetTargetType() const;

    void SetTargetOrientation(granny_real32 const* Orientation3x3);
    void GetTargetOrientation(granny_real32* Orientation3x3);

    void SetOrientationOffset(granny_real32 const* Orientation3); //rot about X, rot about Y, rot about Z
    void GetOrientationOffset(granny_real32* Orientation3);

    void SetOrientBoneName(char const* BoneName);
    char const* GetOrientBoneName() const;
    granny_int32 GetOrientBoneIndex();

    void GetOffsetTargetOrientation(granny_real32* Orientation3);

public:
    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;

private:
    granny_world_pose* m_tempPose;

    granny_real32 m_AimOrientation3x3[9]; 
    granny_int32  m_OrientBoneIndex;

    //FullJointChainToRoot is every joint in the skeleton between the end effector and the root. 
    granny_int32x  FullJointChainToRootLength;
    granny_int32x* FullJointChainToRoot;

    void RefreshBoundValues();
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_ORIENT_MATCH_H
#endif /* GSTATE_ORIENT_MATCH_H */
