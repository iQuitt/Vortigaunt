// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_orient_towards.h $
// $DateTime: 2013/04/15 14:26:51 $
// $Change: 42530 $
// $Revision: #8 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_ORIENT_TOWARDS_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

#if !defined(GSTATE_IK_SHARED_H)
#include "gstate_ik_shared.h"
#endif


BEGIN_GSTATE_NAMESPACE;


class orient_towards : public node
{
    typedef node parent;

public:
    virtual bool BindToCharacter(gstate_character_instance* Instance);
    virtual void UnbindFromCharacter();

    DECL_SAMPLEPOSE_INTERFACE();
    DECL_CONCRETE_NODE_TOKEN(orient_towards);
    DECL_SNAPPABLE();
    DECL_SAMPLEMORPH_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();
    DECL_SAMPLEEVENT_INTERFACE();

    granny_real32 GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx);

    virtual bool AllowSyncInputsWithParentOutputs() const { return true; }
    virtual bool AllowSyncOutputsWithParentOutputs() const { return true; }

    // This is the only per-instance piece of data...
    void SetAimPosition(granny_real32 const* Position);
    void GetAimPosition(granny_real32*);

    // All of these affect the token
    void SetDefaultAimPosition(granny_real32 const* Position);
    void GetDefaultAimPosition(granny_real32*);

    void SetAimAxis(EAimAxis Axis);
    EAimAxis GetAimAxis();
    granny_real32 const* GetAimAxisFloat();

    void AddBoneSlot();
    bool DeleteBoneSlot(int DeleteSlot);
    int  GetNumBoneSlots();

    void SetBoneName(int BoneSlot, char const* HeadName);
    char const* GetBoneName(int BoneSlot);

    granny_int32 GetBoneIndex(int BoneSlot);

public:
    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;

private:
    granny_world_pose* m_tempPose;

    granny_real32 m_AimPosition[3];
    granny_int32* m_BoneSlots;

    void RefreshBoundValues();
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_ORIENT_TOWARDS_H
#endif /* GSTATE_ORIENT_TOWARDS_H */
