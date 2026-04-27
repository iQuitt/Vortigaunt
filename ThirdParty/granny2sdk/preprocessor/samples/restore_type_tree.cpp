// ========================================================================
// $File: //jeffr/granny_29/preprocessor/touch_version.cpp $
// $DateTime: 2012/09/05 10:21:32 $
// $Change: 39146 $
// $Revision: #2 $
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool
RestoreTypeTree(input_file& InputFile,
                key_value_pair* KeyValues,
                granny_int32x NumKeyValues,
                granny_memory_arena* TempArena)
{
    // We need to check that we can actually correctly access this file, since we're
    // assuming the type tree is missing.
    granny_variant RootType;
    GrannyGetDataTreeFromFile(InputFile.GrannyFile, &RootType);

    if (RootType.Type == 0 || RootType.Type[0].Type == GrannyEndMember)
    {
        // missing type tree, expected

        // FileTypeTag must match current value for the GetFileInfo call below to succeed.
        if (InputFile.GrannyFile->Header->TypeTag != GrannyCurrentGRNStandardTag)
        {
            ErrOut("File '%s' has type tag 0x%x, required: 0x%x\n",
                   InputFile.Filename, InputFile.GrannyFile->Header->TypeTag,
                   GrannyCurrentGRNStandardTag);

            // Ok, that's not recoverable
            return false;
        }
    }
    else
    {
        // Type tree present.  Warn that something unexpected has occurred, but continue,
        // GetFileInfo should work as expected.
        WarnOut("'%s' contains type information\n", InputFile.Filename);
    }
    

    granny_file_info* Info = GrannyGetFileInfo(InputFile.GrannyFile);
    if (!Info)
    {
        ErrOut("Unable to get granny_file_info from file.");
        return false;
    }

    return WriteInfoPreserve(KeyValues, NumKeyValues,
                             Info, InputFile.GrannyFile,
                             true, true, true, TempArena);
}

static const char* HelpString =
    (" RestoreTypeTree reads in the specified Granny file, and attempts to\n"
     " put back a missing tree.\n"
     "\n"
     "    preprocessor RestoreTypeTree char.gr2 -output char.gr2\n");

static CommandRegistrar RegRestoreTypeTree(eSingleFile_NoMultiRun,
                                           RestoreTypeTree,
                                           "RestoreTypeTree",
                                           "Puts a type tree back in a GR2 file",
                                           HelpString);

