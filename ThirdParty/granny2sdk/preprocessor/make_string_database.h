#if !defined(MAKE_STRING_DATABASE_H)
// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================

#include "preprocessor.h"

bool ExtractStrings(input_file*     InputFiles,
                    granny_int32x   NumInputFiles,
                    key_value_pair* KeyValues,
                    granny_int32x   NumKeyValues,
                    granny_memory_arena* TempArena);

bool RemapStrings(input_file& InputFile,
                  key_value_pair* KeyValues,
                  granny_int32x   NumKeyValues,
                  granny_memory_arena* TempArena);

#define MAKE_STRING_DATABASE_H
#endif /* MAKE_STRING_DATABASE_H */
