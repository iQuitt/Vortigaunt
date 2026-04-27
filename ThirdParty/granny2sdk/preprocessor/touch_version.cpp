// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "preprocessor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool
TouchVersion(input_file& InputFile,
             key_value_pair* KeyValues,
             granny_int32x NumKeyValues,
             granny_memory_arena* TempArena)
{
    // We need to distinguish between
    char const* ForceFormat = FindFirstValueForKey(KeyValues, NumKeyValues, "force");
    if (ForceFormat && !(_stricmp(ForceFormat, "GR2") == 0 ||
                         _stricmp(ForceFormat, "GSF") == 0))
    {
        ErrOut("Invalid format force: '%s'", ForceFormat);
        return false;
    }

    if (!ForceFormat)
    {
        granny_variant RootType;
        GrannyGetDataTreeFromFile(InputFile.GrannyFile, &RootType);
        if (RootType.Type == 0)
        {
            ErrOut("Unable to get root type from the input file");
            return false;
        }

        // Look at the string of the first type entry in the root.  This should match
        // either GrannyFileInfoType[0] or GStateCharacterInfoType[0].

        // RootType.Type[0].Name might be null if this is a RAW file.
        
        if (RootType.Type[0].Name == 0)
        {
            ErrOut("Unable to infer type from RAW file.");
            return false;
        }

        if (_stricmp(GrannyFileInfoType[0].Name, RootType.Type[0].Name) == 0)
        {
            ForceFormat = "GR2";
        }
        else if (_stricmp(GStateCharacterInfoType[0].Name, RootType.Type[0].Name) == 0)
        {
            ForceFormat = "GSF";
        }
        else
        {
            ErrOut("Unable to infer type from file.");
            return false;
        }
    }
    pp_assert(ForceFormat);


    // enumeration would probably be better, duh...
    if (_stricmp(ForceFormat, "GSF") == 0)
    {
        gstate_character_info* CharacterInfo = GStateGetCharacterInfo(InputFile.GrannyFile);
        if (!CharacterInfo)
        {
            ErrOut("Unable to get gstate_character_info from file.");
            return false;
        }
        
        return SimpleWriteCharacterInfo(KeyValues, NumKeyValues,
                                        CharacterInfo, TempArena);
    }
    else
    {
        pp_assert(_stricmp(ForceFormat, "GR2") == 0);

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
                         

}

static const char* HelpString =
    (" TouchVersion reads in the specified Granny file, performs any versioning\n"
     " operations needed to bring the file up-to-date, and writes out the\n"
     " file, unchanged, in the latest format. This allows the reading\n"
     " process at runtime to move at maximum speed.\n"
     "\n"
     "    preprocessor TouchVersion char.gr2 -output char_updated.gr2\n"
     "\n"
     " Note that TouchVersiontries to determine whether the passed in file is\n"
     " a .gr2 or .gsf by examining the type information, but you may force the\n"
     " issue with\n"
     "\n"
     "    preprocessor TouchVersion file.XXX -output file.XXX -force GR2 \n"
     "    preprocessor TouchVersion file.XXX -output file.XXX -force GSF \n"
     "\n"
     " if the auto-detection is doing the wrong thing.\n");

static CommandRegistrar RegTouchVersion(eSingleFile_NoMultiRun,
                                        TouchVersion,
                                        "TouchVersion",
                                        "Updates the specified file to the current file format (GR2 or GSF).",
                                        HelpString);

