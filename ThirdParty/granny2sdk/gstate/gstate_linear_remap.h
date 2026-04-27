// ========================================================================
// $Notice: $
// ========================================================================
#if !defined(GSTATE_LINEAR_REMAP_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class linear_remap : public node
{
    typedef node parent;

public:
    struct pt
    {
        granny_uint16 x;
        granny_uint16 y;
    };
    
public:
    granny_int32x GetNumRemapPoints();
    bool          GetRemapPoints(pt* Buffer, granny_int32x BufferSize);
    bool          SetRemapPoints(pt const* Points, granny_int32x PointCount);

    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;

    DISALLOW_IO_MANIPULATION_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();
    DECL_CONCRETE_NODE_TOKEN(linear_remap);
};


END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_LINEAR_REMAP_H
#endif /* GSTATE_LINEAR_REMAP_H */
