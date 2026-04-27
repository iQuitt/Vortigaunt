// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_SNAPSHOTUTILS_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_BASE_H
#include "gstate_base.h"
#endif

#if GRANNY_LITTLE_ENDIAN

#define CREATE_WRITE_DWORD(var)                             \
    {                                                       \
        if (!GrannyWriteBytes(Writer, sizeof(var), &(var))) \
            return false;                                   \
    } typedef int __require_semi

#define RESET_READ_DWORD(var)                                                           \
    {                                                                                   \
        if (GrannyReadExactly(Reader, StartPosition + ReaderOffset, sizeof(var), &var)) \
            ReaderOffset += sizeof(var);                                                \
        else                                                                            \
            return false;                                                               \
    } typedef int __require_semi

#elif GRANNY_BIG_ENDIAN

#define SWAP_UINT32(val)                        \
    (((val) << 24)               |              \
     (((val) & 0x0000FF00) << 8) |              \
     (((val) & 0x00FF0000) >> 8) |              \
     ((val) >> 24))

#define CREATE_WRITE_DWORD(var)                                     \
    {                                                               \
        granny_uint32 Temp = *((granny_uint32*)(char*)&(var));      \
        granny_uint32 SwapTemp = SWAP_UINT32(Temp);                 \
        if (!GrannyWriteBytes(Writer, sizeof(SwapTemp), &SwapTemp)) \
            return false;                                           \
    } typedef int __require_semi

#define RESET_READ_DWORD(var)                                                               \
    {                                                                                       \
        granny_uint32 Temp;                                                                 \
        if (GrannyReadExactly(Reader, StartPosition + ReaderOffset, sizeof(Temp), &Temp))   \
            ReaderOffset += sizeof(Temp);                                                   \
        else                                                                                \
            return false;                                                                   \
        *((granny_uint32*)(char*)&(var)) = SWAP_UINT32(Temp);                                      \
    } typedef int __require_semi

#else
#error "Must set GRANNY_LITTLE_ENDIAN or GRANNY_BIG_ENDIAN"
#endif

#define CREATE_SNAPSHOT(Typename)                           \
    bool GSTATE                                             \
    Typename :: CreateSnapshot(granny_file_writer* Writer)

#define RESET_FROMSNAPSHOT(Typename)                                    \
    bool GSTATE                                                         \
    Typename :: ResetFromSnapshot(granny_file_reader* Reader,           \
                                  granny_int32        StartPosition)



#define CREATE_PASS_TO_PARENT() return parent::CreateSnapshot(Writer)
#define CREATE_WRITE_BOOL(var)                                      \
    {                                                               \
        granny_uint8 BoolVal = 0;                                   \
        if (var)                                                    \
            BoolVal = 1;                                            \
        if (!GrannyWriteBytes(Writer, sizeof(BoolVal), &BoolVal))   \
            return false;                                           \
    } typedef int __require_semi

#define CREATE_WRITE_INT32(var)  CREATE_WRITE_DWORD(var)
#define CREATE_WRITE_REAL32(var) CREATE_WRITE_DWORD(var)

#define CREATE_WRITE_INT32_ARRAY(var, cnt)      \
    {                                           \
        for (int Idx = 0; Idx < (cnt); ++Idx) { \
            CREATE_WRITE_DWORD((var)[Idx]);     \
        }                                       \
    } typedef int __require_semi

#define CREATE_WRITE_REAL32_ARRAY(var, cnt)     \
    {                                           \
        for (int Idx = 0; Idx < (cnt); ++Idx) { \
            CREATE_WRITE_REAL32((var)[Idx]);    \
        }                                       \
    } typedef int __require_semi


#define RESET_OFFSET_TRACKING() granny_int32x ReaderOffset = 0
#define RESET_PASS_TO_PARENT() return parent::ResetFromSnapshot(Reader, StartPosition + ReaderOffset)

#define RESET_READ_BOOL(var)                                                                    \
    {                                                                                           \
        granny_uint8 BoolVal = 0;                                                               \
        if (GrannyReadExactly(Reader, StartPosition + ReaderOffset, sizeof(BoolVal), &BoolVal)) \
            ReaderOffset += sizeof(BoolVal);                                                    \
        else                                                                                    \
            return false;                                                                       \
        (var) = (BoolVal != 0);                                                                 \
    } typedef int __require_semi

#define RESET_READ_INT32(var)  RESET_READ_DWORD(var)
#define RESET_READ_REAL32(var) RESET_READ_DWORD(var)

#define RESET_READ_INT32_ARRAY(var, cnt)        \
    {                                           \
        for (int Idx = 0; Idx < (cnt); ++Idx) { \
            RESET_READ_DWORD((var)[Idx]);       \
        }                                       \
    } typedef int __require_semi

#define RESET_READ_REAL32_ARRAY(var, cnt)       \
    {                                           \
        for (int Idx = 0; Idx < (cnt); ++Idx) { \
            RESET_READ_REAL32((var)[Idx]);      \
        }                                       \
    } typedef int __require_semi

#include "gstate_header_postfix.h"
#define GSTATE_SNAPSHOTUTILS_H
#endif /* GSTATE_SNAPSHOTUTILS_H */
