// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_AIMAT_IK_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

#if !defined(GSTATE_IK_SHARED_H)
#include "gstate_ik_shared.h"
#endif


BEGIN_GSTATE_NAMESPACE;

class aimat_ik : public node
{
    typedef node parent;

public:
    virtual bool BindToCharacter(gstate_character_instance* Instance);
    virtual void UnbindFromCharacter();

    DECL_SAMPLEPOSE_INTERFACE();
    DECL_CONCRETE_NODE_TOKEN(aimat_ik);
    DECL_SNAPPABLE();
    DECL_SAMPLEMORPH_INTERFACE();
    DECL_SAMPLEEVENT_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();

    virtual bool AllowSyncInputsWithParentOutputs() const { return true; }
    virtual bool AllowSyncOutputsWithParentOutputs() const { return true; }

    virtual granny_real32 GetLocalTimeOffset(granny_real32 AtT, granny_int32x OnOutput);

    // This is the only per-instance piece of data...
    void SetAimPosition(granny_real32 const* Position);
    void GetAimPosition(granny_real32*);
    // All of these affect the token
    void SetDefaultAimPosition(granny_real32 const* Position);
    void GetDefaultAimPosition(granny_real32*);

    void SetDefaultAimJoint(const char * JointName);
    const char * GetDefaultAimJoint() const;

    void SetAimAxis(EAimAxis Axis);
    EAimAxis GetAimAxis();
    granny_real32 const* GetAimAxisFloat();

    void SetEarAxis(EAimAxis Axis);
    EAimAxis GetEarAxis();
    granny_real32 const* GetEarAxisFloat();

    void SetGroundNormal(EAimAxis Axis);
    EAimAxis GetGroundNormal();
    granny_real32 const* GetGroundNormalFloat();

    void SetHeadName(char const* HeadName);
    void SetFirstActiveName(char const* FirstActive);
    void SetLastActiveName(char const* LastActive);

    char const* GetHeadName();
    char const* GetFirstActiveName();
    char const* GetLastActiveName();

    bool IsActive() const;

    // These will be valid when IsActive is true
    granny_int32x GetBoundHeadIndex();
    granny_int32x GetBoundLinkCount();
    granny_int32x GetBoundInactiveLinkCount();

public:
    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;

private:
    granny_world_pose* m_tempPose;

    granny_real32 m_AimPosition[3];

    // If the node is bound to an invalid character, all of these will be -1.
    // Unless all of them are > 0, and m_InactiveLinks < m_linkCount, the node
    // is inactive
    granny_int32x m_HeadIndex;
    granny_int32x m_LinkCount;
    granny_int32x m_InactiveLinks;

    void RefreshBoundValues();
};

END_GSTATE_NAMESPACE;


#include "gstate_header_postfix.h"
#define GSTATE_AIMAT_IK_H
#endif /* GSTATE_AIMAT_IK_H */
