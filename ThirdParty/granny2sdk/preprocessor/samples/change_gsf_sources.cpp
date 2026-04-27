// ========================================================================
// $File: //jeffr/granny_29/preprocessor/touch_version.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <vector>

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

USING_GSTATE_NAMESPACE;
using namespace std;

static string
AlterFilename(char const* OldFilename)
{
    // CHANGE THIS!
    return string(OldFilename) + ".foo";
}

bool
ChangeGSFSources(input_file& InputFile,
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

    for (granny_int32x SetIdx = 0; SetIdx < CharacterInfo->AnimationSetCount; ++SetIdx)
    {
        animation_set* Set = CharacterInfo->AnimationSets[SetIdx];

        for (granny_int32x FileIdx = 0; FileIdx < Set->SourceFileReferenceCount; ++FileIdx)
        {
            string NewName = AlterFilename(Set->SourceFileReferences[FileIdx]->SourceFilename);

            Set->SourceFileReferences[FileIdx]->SourceFilename =
                PushString(TempArena, NewName.c_str());
        }
    }

    return SimpleWriteCharacterInfo(KeyValues, NumKeyValues, CharacterInfo, TempArena);
}

static const char* HelpString =
    (" ChangeGSFSources reads in the specified Animation Studio file, scans\n"
     " its referenced animations, and alters the filename to reflect layout\n"
     " changes in your art tree.\n"
     "\n"
     " YOU MUST CHANGE THIS FILE!\n"
     "\n"
     " In particular, you should customize the \"AlterFilename\" function to\n"
     " return a new filename when passed an old one.\n"
     "\n"
     "    preprocessor ChangeGSFSources file.gsf -output file.gsf\n"
     "    preprocessor ChangeGSFSources file.gsf -output file_updated.gsf\n");

static CommandRegistrar RegRefreshGSFSpecs(eSingleFile_NoMultiRun,
                                           ChangeGSFSources,
                                           "ChangeGSFSources",
                                           "Alters the GR2 source references in the specified GSF file.",
                                           HelpString);



