// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_linear_equation.h $
// $DateTime: 2012/05/11 15:03:34 $
// $Change: 37384 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_LINEAR_EQUATION_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

class linear_equation : public node
{
    typedef node parent;

public:
    void SetEquation(granny_real32 A,        granny_real32 B);
    void GetEquation(granny_real32* AOutput, granny_real32* BOutput);

    virtual granny_int32x GetOutputPassthrough(granny_int32x OutputIdx) const;

    DISALLOW_IO_MANIPULATION_INTERFACE();
    DECL_SAMPLESCALAR_INTERFACE();
    DECL_CONCRETE_NODE_TOKEN(linear_equation);
};


END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_LINEAR_EQUATION_H
#endif /* GSTATE_LINEAR_EQUATION_H */
