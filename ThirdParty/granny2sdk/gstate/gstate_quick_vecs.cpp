// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_quick_vecs.h"
#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

granny_uint8*
QVecPushNewElementGeneric(granny_int32& ElementCount, granny_uint8*& Elements, size_t ElementSize)
{
    GStateAssert(ElementCount >= 0);

    granny_uint8* NewElements = GStateAllocArray(granny_uint8, (ElementCount + 1) * ElementSize);
    // Assume success!  Rawr!
    
    if (ElementCount != 0)
        memcpy(NewElements, Elements, ElementSize*ElementCount);

    GStateDeallocate(Elements);
    Elements = NewElements;
    ++ElementCount;

    return (Elements + ElementSize * (ElementCount-1));
}

void
QVecRemoveElementGeneric(granny_int32x ElementToRemove,
                     granny_int32& ElementCount,
                     granny_uint8*& Elements,
                     size_t ElementSize)
{
    // We won't realloc, just shrink element count.  We do deallocate if ElementCount
    // drops to 0.
    GStateAssert(ElementToRemove >= 0 && ElementToRemove < ElementCount);

    if (ElementCount > 1)
    {
        memmove(Elements + ElementToRemove*ElementSize,
                Elements + (ElementToRemove+1)*ElementSize,
                ElementSize * (ElementCount - (ElementToRemove+1)));
    }

    --ElementCount;
    if (ElementCount == 0)
    {
        GStateDeallocate(Elements);
        Elements = 0;
    }
}


