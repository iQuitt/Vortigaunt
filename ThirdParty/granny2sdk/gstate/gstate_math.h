// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_quick_vecs.h $
// $DateTime: 2012/05/11 15:03:34 $
// $Change: 37384 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_MATH_H)
#include "gstate_header_prefix.h"

#if !defined(GSTATE_BASE_H)
#include "gstate_base.h"
#endif

#include <math.h>

BEGIN_GSTATE_NAMESPACE;

inline void GStateVectorScale3(granny_real32 *Dest, granny_real32 const Scalar)
{
    Dest[0] *= Scalar;
    Dest[1] *= Scalar;
    Dest[2] *= Scalar;
}

inline void GStateVectorScale3(granny_real32 *Dest, granny_real32 const Scalar,
                               granny_real32 const *Source)
{
    Dest[0] = Source[0] * Scalar;
    Dest[1] = Source[1] * Scalar;
    Dest[2] = Source[2] * Scalar;
}

inline void GStateVectorEquals3(granny_real32 *Dest, granny_real32 const *Source)
{
    Dest[0] = Source[0];
    Dest[1] = Source[1];
    Dest[2] = Source[2];
}

inline void GStateVectorEquals4(granny_real32 *Dest, granny_real32 const *Source)
{
    Dest[0] = Source[0];
    Dest[1] = Source[1];
    Dest[2] = Source[2];
    Dest[3] = Source[3];
}

inline void GStateVectorAdd3(granny_real32 *Dest, granny_real32 const *Addend)
{
    Dest[0] += Addend[0];
    Dest[1] += Addend[1];
    Dest[2] += Addend[2];
}

inline void GStateMatrix3x3Equals4x4(granny_real32 *Dest, granny_real32 const *Source)
{
    Dest[0] = Source[0];
    Dest[1] = Source[1];
    Dest[2] = Source[2];
    Dest[3] = Source[4];
    Dest[4] = Source[5];
    Dest[5] = Source[6];
    Dest[6] = Source[8];
    Dest[7] = Source[9];
    Dest[8] = Source[10];
}

inline void GStateXRotationColumns3x3(granny_triple *Result, granny_real32 const AngleInRadians)
{
    granny_real32 const AngleSin = sinf(AngleInRadians);
    granny_real32 const AngleCos = cosf(AngleInRadians);

    Result[0][0] = 1.0f;
    Result[0][1] = 0.0f;
    Result[0][2] = 0.0f;
    Result[1][0] = 0.0f;
    Result[1][1] = AngleCos;
    Result[1][2] = -AngleSin;
    Result[2][0] = 0.0f;
    Result[2][1] = AngleSin;
    Result[2][2] = AngleCos;
}

inline void
GStateYRotationColumns3x3(granny_triple *Result, granny_real32 const AngleInRadians)
{
    granny_real32 const AngleSin = sinf(AngleInRadians);
    granny_real32 const AngleCos = cosf(AngleInRadians);

    Result[0][0] = AngleCos;
    Result[0][1] = 0.0f;
    Result[0][2] = AngleSin;
    Result[1][0] = 0.0f;
    Result[1][1] = 1.0f;
    Result[1][2] = 0.0f;
    Result[2][0] = -AngleSin;
    Result[2][1] = 0.0f;
    Result[2][2] = AngleCos;
}

inline void
GStateZRotationColumns3x3(granny_triple *Result, granny_real32 const AngleInRadians)
{
    granny_real32 const AngleSin = sinf(AngleInRadians);
    granny_real32 const AngleCos = cosf(AngleInRadians);

    Result[0][0] = AngleCos;
    Result[0][1] = -AngleSin;
    Result[0][2] = 0.0f;
    Result[1][0] = AngleSin;
    Result[1][1] = AngleCos;
    Result[1][2] = 0.0f;
    Result[2][0] = 0.0f;
    Result[2][1] = 0.0f;
    Result[2][2] = 1.0f;
}

inline void GStateMatrixMultiply3x3(granny_real32 *IntoMatrix,
                        granny_real32 const *Matrix,
                        granny_real32 const *ByMatrix)
{
    GStateAssert(Matrix != IntoMatrix);
    GStateAssert(ByMatrix != IntoMatrix);

    granny_triple *Result = (granny_triple *)IntoMatrix;
    granny_triple const *A = (granny_triple const *)Matrix;
    granny_triple const *B = (granny_triple const *)ByMatrix;

    Result[0][0] = A[0][0]*B[0][0] + A[0][1]*B[1][0] + A[0][2]*B[2][0];
    Result[0][1] = A[0][0]*B[0][1] + A[0][1]*B[1][1] + A[0][2]*B[2][1];
    Result[0][2] = A[0][0]*B[0][2] + A[0][1]*B[1][2] + A[0][2]*B[2][2];

    Result[1][0] = A[1][0]*B[0][0] + A[1][1]*B[1][0] + A[1][2]*B[2][0];
    Result[1][1] = A[1][0]*B[0][1] + A[1][1]*B[1][1] + A[1][2]*B[2][1];
    Result[1][2] = A[1][0]*B[0][2] + A[1][1]*B[1][2] + A[1][2]*B[2][2];

    Result[2][0] = A[2][0]*B[0][0] + A[2][1]*B[1][0] + A[2][2]*B[2][0];
    Result[2][1] = A[2][0]*B[0][1] + A[2][1]*B[1][1] + A[2][2]*B[2][1];
    Result[2][2] = A[2][0]*B[0][2] + A[2][1]*B[1][2] + A[2][2]*B[2][2];
}

inline granny_real32 GStateVectorLengthSquared4(granny_real32 const *Source)
{
    return(Source[0]*Source[0] +
        Source[1]*Source[1] +
        Source[2]*Source[2] +
        Source[3]*Source[3]);
}

inline granny_real32 GStateVectorLength4(granny_real32 const *Source)
{
    return(sqrtf(GStateVectorLengthSquared4(Source)));
}

inline void GStateVectorScale4(granny_real32 *Dest, granny_real32 const Scalar)
{
    Dest[0] *= Scalar;
    Dest[1] *= Scalar;
    Dest[2] *= Scalar;
    Dest[3] *= Scalar;
}

inline void GStateNormalize4(granny_real32 *Dest)
{
    granny_real32 Length = GStateVectorLength4(Dest);
    GStateVectorScale4(Dest, 1.0f / Length);
}

inline void GStateSetIdentity4x4(granny_real32 *IntoMatrix)
{
    IntoMatrix[0] = 1.0f;
    IntoMatrix[1] = 0.0f;
    IntoMatrix[2] = 0.0f;
    IntoMatrix[3] = 0.0f;

    IntoMatrix[4] = 0.0f;
    IntoMatrix[5] = 1.0f;
    IntoMatrix[6] = 0.0f;
    IntoMatrix[7] = 0.0f;

    IntoMatrix[8] = 0.0f;
    IntoMatrix[9] = 0.0f;
    IntoMatrix[10] = 1.0f;
    IntoMatrix[11] = 0.0f;

    IntoMatrix[12] = 0.0f;
    IntoMatrix[13] = 0.0f;
    IntoMatrix[14] = 0.0f;
    IntoMatrix[15] = 1.0f;
}

END_GSTATE_NAMESPACE;

#include "gstate_header_postfix.h"
#define GSTATE_MATH_H
#endif /* GSTATE_MATH_H */
