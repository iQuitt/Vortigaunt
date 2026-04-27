// ========================================================================
// $Notice: $
// ========================================================================
#if !defined(GSTATE_CUSTOM_REMAP_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

//These enum needs to sync with the list in blendnode_edit_visitor::VisitNode
enum custom_remap_type
{
    CustomRemap_Direct   = 0,                 //Input -> Output, values in between will adjacent inputs will output the value associated with the smaller input.
    CustomRemap_DirectStrict   = 1,           //Input -> Output, but will trigger a GStateWarning if the inputs don't match exactly.
    CustomRemap_IntegerInterpolated  = 2,     //Input -> integers in between outputs.
    CustomRemap_FloatInterpolated = 3,        //Input -> continuous floats between outputs.
    CustomRemap_DirectWithFallback   = 4,     //Input -> Output, but will use the specified fallback value
    CustomRemap_COUNT
};

class custom_remap : public node
{
    typedef node parent;

public:
    struct pt
    {
        granny_real32 x;
        granny_real32 y;
    };
    
public:
    granny_int32x GetNumRemapPoints();
    bool          GetRemapPoints(pt* Buffer, granny_int32x BufferSize);
    bool          SetRemapPoints(pt const* Points, granny_int32x PointCount);
    custom_remap_type GetRemapType() const;
    void          SetRemapType(custom_remap_type Type);
    void          SortPointsByX();
    void          SortPointsByY();
    granny_real32 GetFallback();
    void          SetFallback(granny_real32 Fallback);

    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;

    DISALLOW_IO_MANIPULATION_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();
    DECL_CONCRETE_NODE_TOKEN(custom_remap);

    granny_real32 LastEvalInput;
    granny_real32 LastEvalOutput;
    granny_real32 LastEvalTime;
    granny_real32 LastStrictFailureValue;
};


END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_CUSTOM_REMAP_H
#endif /* GSTATE_CUSTOM_REMAP_H */
