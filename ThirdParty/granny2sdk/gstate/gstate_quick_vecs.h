// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_QUICK_VECS_H)
#include "gstate_header_prefix.h"

#if !defined(GSTATE_BASE_H)
#include "gstate_base.h"
#endif

// Small utilities for dealing with growing arrays in GState.  This is a really common
// pattern that occurs only (very uncommonly) at editing time.  I.e, none of this code
// will be invoked at runtime.  It's designed to correspond exactly to the
// GrannyReferenceToArrayMember, which means no extra information about reserves, etc is
// preserved.  One extra element at a time!
granny_uint8* QVecPushNewElementGeneric(granny_int32& ElementCount,
                                        granny_uint8*& Elements,
                                        size_t ElementSize);
void QVecRemoveElementGeneric(granny_int32x ElementToRemove,
                              granny_int32& ElementCount,
                              granny_uint8*& Elements,
                              size_t ElementSize);

template <class T>
inline T& QVecPushNewElement(granny_int32& ElementCount, T*& Elements)
{
    return *((T*)QVecPushNewElementGeneric(ElementCount, (granny_uint8*&)Elements, sizeof(T)));
}

template <class T>
inline T& QVecPushNewElementNoCount(granny_int32 const ElementCount,
                                    T*& Elements)
{
    granny_int32 NewElementCount = ElementCount;
    return *((T*)QVecPushNewElementGeneric(NewElementCount, (granny_uint8*&)Elements, sizeof(T)));
}

template <class T>
inline void QVecRemoveElement(granny_int32 const ToRemove, granny_int32& ElementCount, T*& Elements)
{
    QVecRemoveElementGeneric(ToRemove, ElementCount, (granny_uint8*&)Elements, sizeof(T));
}

template <class T>
inline void QVecRemoveElementNoCount(granny_int32 const ToRemove,
                                     granny_int32 const ElementCount,
                                     T*& Elements)
{
    granny_int32 NewElementCount = ElementCount;
    QVecRemoveElementGeneric(ToRemove, NewElementCount, (granny_uint8*&)Elements, sizeof(T));
}


#include "gstate_header_postfix.h"
#define GSTATE_QUICK_VECS_H
#endif /* GSTATE_QUICK_VECS_H */
