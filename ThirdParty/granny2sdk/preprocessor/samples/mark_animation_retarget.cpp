// ========================================================================
// $File: //jeffr/granny_29/preprocessor/touch_version.cpp $
// $DateTime: 2012/09/05 10:21:32 $
// $Change: 39146 $
// $Revision: #2 $
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>

granny_file_info*
MarkAnimRetarget(char const*          OriginalFilename,
                 char const*          OutputFilename,
                 granny_file_info*    InputInfo,
                 key_value_pair*      KeyValuePairs,
                 granny_int32x        NumKeyValuePairs,
                 granny_memory_arena* TempArena)
{
    if (InputInfo == 0)
    {
        ErrOut("unable to obtain a granny_file_info from the input animation file\n");
        return false;
    }

    if (InputInfo->AnimationCount == 0)
    {
        ErrOut("AnimFile contains no animations\n");
        return false;
    }

    for( int i=0; i<InputInfo->AnimationCount; i++ )
    {
        granny_animation* Anim = InputInfo->Animations[i];

        granny_string_table* StringTable = GrannyNewArenaStringTable(TempArena);

        granny_variant_builder* VariantBuilder = GrannyBeginVariant(StringTable);

        GrannyAddBoolMember(VariantBuilder, "GStateRetargetIndividually", true);

        granny_int32x TypeSize = GrannyGetResultingVariantTypeSize(VariantBuilder);
        granny_int32x ObjSize  = GrannyGetResultingVariantObjectSize(VariantBuilder);

        GrannyEndVariantInPlace(VariantBuilder,
            PushArray(TempArena, TypeSize, granny_uint8),
            &Anim->ExtendedData.Type,
            PushArray(TempArena, ObjSize, granny_uint8),
            &Anim->ExtendedData.Object);

        // Since we're using an Arena-based string table, we can release it at this point, the
        // strings are allocated into TempArena, and will remain valid.
        GrannyFreeStringTable(StringTable);
    }

    //Is this enough to write it out?
    return InputInfo;
}

static const char* HelpString =
(" MarkAnimRetarget Adds info to the extended data for this anim saying "
 "\n  it should be retargeted at the leaf anim level of gstate."
 "\n"
 "    preprocessor MarkAnimRetarget file.gr2 -output file.gr2\n"
 "");

static CommandRegistrar RegMarkAnimRetarget( MarkAnimRetarget,
                                             "MarkAnimRetarget",
                                             "Adds info to the extended data for this anim saying it should be retargeted at the leaf anim level of gstate",
                                             HelpString);

