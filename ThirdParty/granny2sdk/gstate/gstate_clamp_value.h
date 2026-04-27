// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_clamp_value.h $
// $DateTime: 2012/05/11 15:03:34 $
// $Change: 37384 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_CLAMP_VALUE_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class clamp_value : public node
{
    typedef node parent;

public:
    void SetBounds(granny_real32  Min,       granny_real32 Max);
    void GetBounds(granny_real32* MinOutput, granny_real32* MaxOutput);

    virtual granny_real32 SampleScalarOutput(granny_int32x OutputIdx, granny_real32 AtT, gstate_scalar_track_entry * TrackEntry);

    virtual bool GetScalarOutputRange(granny_int32x OutputIdx,
                                      granny_real32* OutMin,
                                      granny_real32* OutMax);

    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;

    DECL_CONCRETE_NODE_TOKEN(clamp_value);
    DISALLOW_IO_MANIPULATION_INTERFACE();
};


END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_CLAMP_VALUE_H
#endif /* GSTATE_CLAMP_VALUE_H */
