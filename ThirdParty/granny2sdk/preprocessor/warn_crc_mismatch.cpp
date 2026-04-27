// ========================================================================
// $File: //jeffr/granny_29/preprocessor/warn_scale_shear.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "preprocessor.h"
#include <stdio.h>
#include <vector>
using namespace std;

// Not ideal, obviously, but ok for the preprocessor
bool FoundErrors = false;
vector<granny_file*> LoadedFiles;

GRANNY_CALLBACK(void)
log_callback(granny_log_message_type Type,
             granny_log_message_origin Origin,
             char const*  File,
             granny_int32x Line,
             char const * Message,
             void * UserData)
{
    if (Type == GrannyErrorLogMessage && Origin == GrannyGStateLogMessage)
    {
        FoundErrors = true;
        ErrOut("%s\n", Message);
    }
}


static GRANNY_CALLBACK(granny_file_info*)
SourceBindingCallback(gstate_character_info* BindingInfo,
                      char const*     SourceName,
                      void*           /*UserData*/)
{
    granny_file* File = GrannyReadEntireFile(SourceName);
    if (!File)
        return 0;

    LoadedFiles.push_back(File);
    return GrannyGetFileInfo(File);
}


bool
WarnCRCMismatch(input_file&     InputFile,
                key_value_pair* KeyValues,
                granny_int32x   NumKeyValues,
                granny_memory_arena* TempArena)
{
    gstate_character_info* CharacterInfo = GStateGetCharacterInfo(InputFile.GrannyFile);
    if (!CharacterInfo)
    {
        ErrOut("Unable to get gstate_character_info from file.");
        return false;
    }

    granny_log_callback OldCallback;
    GrannyGetLogCallback(&OldCallback);


    granny_log_callback NewCallback;
    NewCallback.Function = log_callback;
    NewCallback.UserData = TempArena;
    GrannySetLogCallback(&NewCallback);

    // Load the file, let the log callback catch the errors...
    FoundErrors = false;

    if (!GStateBindCharacterFileReferences(CharacterInfo, SourceBindingCallback, 0))
    {
        FoundErrors = true;
    }

    for (size_t Idx = 0; Idx < LoadedFiles.size(); ++Idx)
    {
        GrannyFreeFile(LoadedFiles[Idx]);
    }
    LoadedFiles.clear();
    
    GrannySetLogCallback(&OldCallback);

    return !FoundErrors;
}

static const char* HelpString =
    (" WarnCRCMismatch will look for animations that have mismatched AnimCRCs,\n"
     " indicating that the GSF may have errors when played back.  Note that\n"
     " this function is for use with gstate_character_info files: .GSFs\n"
     " not GR2s\n"
     "\n"
     "   preprocessor WarnCRCMismatch file.gsf\n");

static CommandRegistrar RegWarnScaleShear(eSingleFile_MultiRun,
                                          WarnCRCMismatch,
                                          "WarnCRCMismatch",
                                          "Warns (and returns failure) if any AnimCRC mismatches are found.",
                                          HelpString);

