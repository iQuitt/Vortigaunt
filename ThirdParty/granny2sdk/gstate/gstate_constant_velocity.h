// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_CONSTANT_VELOCITY_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class constant_velocity : public node
{
    typedef node parent;

public:
    void          GetAxis(granny_real32* Axis);
    granny_real32 GetSpeed();

    void SetAxis(granny_real32 const* Axis);
    void SetSpeed(granny_real32 Speed);

    DECL_CONCRETE_NODE_TOKEN(constant_velocity);
    DECL_SAMPLEPOSE_INTERFACE();
    DECL_SAMPLEMORPH_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();
    DECL_SAMPLEEVENT_INTERFACE();

    granny_real32 GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx);
    virtual bool AllowSyncInputsWithParentOutputs() const { return true; }
    virtual bool AllowSyncOutputsWithParentOutputs() const { return true; }

public:
    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_CONSTANT_VELOCITY_H
#endif /* GSTATE_CONSTANT_VELOCITY_H */
