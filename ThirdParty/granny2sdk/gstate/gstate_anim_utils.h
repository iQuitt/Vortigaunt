// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_ANIM_UTILS_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_BASE_H
#include "gstate_base.h"
#endif

inline granny_real32
Clamp(granny_real32 Min,
      granny_real32 Val,
      granny_real32 Max)
{
    if (Val <= Min) // catches -0 problems.
        return Min;
    else if (Val >= Max)
        return Max;
    return Val;
}

granny_real32
ComputeRawTime(granny_real32 AtTime,
               granny_real32 Speed,
               granny_real32 Offset,
               granny_int32x LoopCount,
               granny_real32 AnimDuration);

granny_real32
ComputeLoopedTime(granny_real32 AtTime,
                  granny_real32 Speed,
                  granny_real32 Offset,
                  granny_int32x LoopCount,
                  granny_real32 AnimDuration);

granny_real32
ComputeModTime(granny_real32 AtTime,
               granny_real32 Speed,
               granny_real32 Offset,
               granny_int32x LoopCount,
               granny_real32 AnimDuration);

void
SortEventsByTimestamp(gstate_text_track_entry* Events,
                      granny_int32x NumEvents);

granny_int32x
FilterDuplicateEvents(gstate_text_track_entry* Events,
                      granny_int32x KnownUnique,
                      granny_int32x NumEvents);


inline granny_real32
HermiteCurve(granny_real32 t)
{
    return -2.0f * t * t * t + 3.0f * t * t;
}

#include "gstate_header_postfix.h"
#define GSTATE_ANIM_UTILS_H
#endif /* GSTATE_ANIM_UTILS_H */
