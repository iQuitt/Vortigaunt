// ========================================================================
// $Notice: $
// ========================================================================
#include "gstate_ik_shared.h"

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

granny_real32 const* GSTATE
FloatFromAxisEnum(granny_int32x axis)
{
    static const granny_real32 XAxis[3]    = { 1, 0, 0 };
    static const granny_real32 YAxis[3]    = { 0, 1, 0 };
    static const granny_real32 ZAxis[3]    = { 0, 0, 1 };
    static const granny_real32 NegXAxis[3] = { -1,  0,  0 };
    static const granny_real32 NegYAxis[3] = {  0, -1,  0 };
    static const granny_real32 NegZAxis[3] = {  0,  0, -1 };
    switch (axis)
    {
        case eAimAxis_XAxis: return XAxis;
        case eAimAxis_YAxis: return YAxis;
        case eAimAxis_ZAxis: return ZAxis;
        case eAimAxis_NegXAxis: return NegXAxis;
        case eAimAxis_NegYAxis: return NegYAxis;
        case eAimAxis_NegZAxis: return NegZAxis;

        default:
        {
            GS_InvalidCodePath("misunderstood axis enum");
            return ZAxis;
        }
    }
}
