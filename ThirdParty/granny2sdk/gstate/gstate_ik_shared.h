#if !defined(GSTATE_IK_SHARED_H)
#include "gstate_header_prefix.h"
// ========================================================================
// $Notice: $
// ========================================================================

#ifndef GSTATE_NODE_H
#include "gstate_node.h"
#endif

BEGIN_GSTATE_NAMESPACE;

enum EAimAxis
{
    eAimAxis_XAxis = 0,
    eAimAxis_YAxis = 1,
    eAimAxis_ZAxis = 2,

    eAimAxis_NegXAxis = 3,
    eAimAxis_NegYAxis = 4,
    eAimAxis_NegZAxis = 5,

    eAimAxis_ForceInt = 0x7fffffff
};
granny_real32 const* FloatFromAxisEnum(granny_int32x axis);

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_IK_SHARED_H
#endif /* GSTATE_IK_SHARED_H */
