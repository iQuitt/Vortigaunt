// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_MASK_INVERT_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class mask_invert : public node
{
    typedef node parent;

public:
    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;

    DECL_SAMPLEMASK_INTERFACE();
    DISALLOW_IO_MANIPULATION_INTERFACE();
    DECL_CONCRETE_NODE_TOKEN(mask_invert);
};


END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_MASK_INVERT_H
#endif /* GSTATE_MASK_INVERT_H */
