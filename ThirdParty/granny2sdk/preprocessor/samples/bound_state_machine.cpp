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


struct encoded_bounds
{
    granny_real32 AnimAABBMin[3];
    granny_real32 AnimAABBMax[3];
};

granny_data_type_definition BoundsType[] = {
    {GrannyReal32Member, "AnimAABBMin", 0, 3},
    {GrannyReal32Member, "AnimAABBMax", 0, 3},
    {GrannyEndMember},
};

bool
BoundStateMachine(input_file& InputFile,
                  key_value_pair* KeyValues,
                  granny_int32x NumKeyValues,
                  granny_memory_arena* TempArena)
{
    gstate_character_info* CharacterInfo = GStateGetCharacterInfo(InputFile.GrannyFile);
    if (!CharacterInfo)
    {
        ErrOut("Unable to get gstate_character_info from file.");
        return false;
    }

    granny_real32 MinPoint[3] = {  1e38f,  1e38f,  1e38f };
    granny_real32 MaxPoint[3] = { -1e38f, -1e38f, -1e38f };
    
    for (int AnimSetIdx = 0; AnimSetIdx < CharacterInfo->AnimationSetCount; ++AnimSetIdx)
    {
        GSTATE animation_set* Set = CharacterInfo->AnimationSets[AnimSetIdx];
        for (int SrcRef = 0; SrcRef < Set->SourceFileReferenceCount; ++SrcRef)
        {
            GSTATE source_file_ref* Ref = Set->SourceFileReferences[SrcRef];

            granny_file* File = GrannyReadEntireFile(Ref->SourceFilename);
            if (!File)
            {
                WarnOut("Unable to load: '%s'\n", Ref->SourceFilename);
                continue;
            }

            granny_file_info* Info = GrannyGetFileInfo(File);
            if (!Info)
            {
                WarnOut("Unable to get granny_file_info from: '%s'\n", Ref->SourceFilename);
                GrannyFreeFile(File);
                continue;
            }

            // Only try to extract the aabb if it's present
			for (int AnimIdx = 0; AnimIdx < Info->AnimationCount; ++AnimIdx)
			{
				granny_variant Ignored;
				if (GrannyFindMatchingMember(Info->Animations[AnimIdx]->ExtendedData.Type, 0, "AnimAABBMin", &Ignored))
				{
					encoded_bounds Bounds = {0};

					GrannyConvertSingleObject(Info->Animations[AnimIdx]->ExtendedData.Type,
											  Info->Animations[AnimIdx]->ExtendedData.Object,
											  BoundsType, &Bounds, 0);
					for (int d = 0; d < 3; ++d)
					{
						MinPoint[d] = std::min(MinPoint[d], Bounds.AnimAABBMin[d]);
						MaxPoint[d] = std::max(MaxPoint[d], Bounds.AnimAABBMax[d]);
					}
				}
			}

            GrannyFreeFile(File);
        }
    }

    // Bounds valid?
    if (MinPoint[0] <= MaxPoint[0])
    {
        granny_string_table* StringTable = GrannyNewArenaStringTable(TempArena);

        granny_variant_builder* VariantBuilder = GrannyBeginVariant(StringTable);

        GrannyAddScalarArrayMember(VariantBuilder, "CharacterAABBMin", 3, MinPoint);
        GrannyAddScalarArrayMember(VariantBuilder, "CharacterAABBMax", 3, MaxPoint);

        granny_int32x TypeSize = GrannyGetResultingVariantTypeSize(VariantBuilder);
        granny_int32x ObjSize  = GrannyGetResultingVariantObjectSize(VariantBuilder);

        GrannyEndVariantInPlace(VariantBuilder,
                                PushArray(TempArena, TypeSize, granny_uint8),
                                &CharacterInfo->ExtendedData.Type,
                                PushArray(TempArena, ObjSize, granny_uint8),
                                &CharacterInfo->ExtendedData.Object);

        // Since we're using an Arena-based string table, we can release it at this point, the
        // strings are allocated into TempArena, and will remain valid.
        GrannyFreeStringTable(StringTable);
    }

    return SimpleWriteCharacterInfo(KeyValues, NumKeyValues,
                                    CharacterInfo, TempArena);
}

static const char* HelpString =
    (" BoundStateMachine reads in the specified GSF file, loops through\n"
     " its source animations, and attempts to compute a global bound for\n"
     " the animation based on embedded animation bounds computed by\n"
     " BoundsCalc.\n"
     "\n"
     "    preprocessor BoundStateMachine char.gsf -output char_updated.gsf\n"
     "\n"
     " Note that BoundStateMachinetries to determine whether the passed in file is\n"
     " a .gr2 or .gsf by examining the type information, but you may force the\n"
     " issue with\n"
     "\n"
     "    preprocessor BoundStateMachine file.XXX -output file.XXX -force GR2 \n"
     "    preprocessor BoundStateMachine file.XXX -output file.XXX -force GSF \n"
     "\n"
     " if the auto-detection is doing the wrong thing.\n");

static CommandRegistrar RegBoundStateMachine(eSingleFile_NoMultiRun,
                                        BoundStateMachine,
                                        "BoundStateMachine",
                                        "Updates the specified file to the current file format (GR2 or GSF).",
                                        HelpString);

