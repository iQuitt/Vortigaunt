#if !defined(GSTATE_TWOBONE_IK_H)
#include "gstate_header_prefix.h"
// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

#ifndef GSTATE_AIMAT_IK_H
#include "gstate_aimat_ik.h"
#endif


BEGIN_GSTATE_NAMESPACE;

class twobone_ik : public node
{
    typedef node parent;

public:
    virtual bool BindToCharacter(gstate_character_instance* Instance);
    virtual void UnbindFromCharacter();

    DECL_SAMPLEPOSE_INTERFACE();
    DECL_SAMPLEMORPH_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();
    DECL_SAMPLEEVENT_INTERFACE();

    granny_real32 GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx);

    virtual bool AllowSyncInputsWithParentOutputs() const { return true; }
    virtual bool AllowSyncOutputsWithParentOutputs() const { return true; }

    // This is the only per-instance piece of data...
    void SetFootPosition(granny_real32 const* Position);
    void GetFootPosition(granny_real32*);

    // All of these affect the token
    void SetDefaultFootPosition(granny_real32 const* Position);
    void GetDefaultFootPosition(granny_real32*);

    void SetKneePlane(EAimAxis Axis);
    EAimAxis GetKneePlane();
    granny_real32 const* GetKneePlaneFloat();

    void SetFootName(char const*);
    void SetKneeName(char const*);
    void SetHipName(char const*);
    char const* GetFootName();
    char const* GetKneeName();
    char const* GetHipName();

    bool IsActive() const;

    // These will be valid when IsActive is true
    granny_int32x GetBoundFootIndex();
    granny_int32x GetBoundKneeIndex();
    granny_int32x GetBoundHipIndex();

    DECL_CONCRETE_NODE_TOKEN(twobone_ik);

public:
    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;

private:
    granny_world_pose* m_tempPose;

    granny_real32 m_FootPosition[3];

    // If the node is bound to an invalid character, all of these will be -1.
    // Unless all of them are > 0, the node is inactive
    granny_int32x m_FootIndex;
    granny_int32x m_KneeIndex;
    granny_int32x m_HipIndex;

    void RefreshBoundValues();
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_TWOBONE_IK_H
#endif /* GSTATE_TWOBONE_IK_H */

