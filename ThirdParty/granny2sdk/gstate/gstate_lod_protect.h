// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_LOD_PROTECT_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class lod_protect : public node
{
    typedef node parent;

public:
    DECL_SAMPLEPOSE_INTERFACE();
    DECL_CONCRETE_NODE_TOKEN(lod_protect);
    DECL_SAMPLEMORPH_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();
    DECL_SAMPLEEVENT_INTERFACE();

    granny_real32 GetLocalTimeOffset(granny_real32 AtT, granny_int32x OutputIdx);

    virtual bool AllowSyncInputsWithParentOutputs() const { return true; }
    virtual bool AllowSyncOutputsWithParentOutputs() const { return true; }

    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;
};

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_LOD_PROTECT_H
#endif /* GSTATE_LOD_PROTECT_H */
