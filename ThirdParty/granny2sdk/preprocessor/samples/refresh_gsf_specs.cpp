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

bool
RefreshGSFSpecs(input_file& InputFile,
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


    // granny_int32           AnimationSetCount;
    // GSTATE animation_set** AnimationSets;
    std::vector<granny_file*> AllFiles;
    for (granny_int32x SetIdx = 0; SetIdx < CharacterInfo->AnimationSetCount; ++SetIdx)
    {
        animation_set* Set = CharacterInfo->AnimationSets[SetIdx];

        std::vector<granny_file*> SetFiles(Set->SourceFileReferenceCount);
        for (granny_int32x FileIdx = 0; FileIdx < Set->SourceFileReferenceCount; ++FileIdx)
        {
            SetFiles[FileIdx] = GrannyReadEntireFile(Set->SourceFileReferences[FileIdx]->SourceFilename);
            AllFiles.push_back(SetFiles[FileIdx]);
            if (!SetFiles[FileIdx])
            {
                // just bail and leak the memory
                ErrOut("Unable to load '%s'\n", Set->SourceFileReferences[FileIdx]->SourceFilename);
                return false;
            }

            // Note that SourceInfo is an empty ref which will be zeroed on write out, so
            // we can stuff the granny_file_info there...
            Set->SourceFileReferences[FileIdx]->SourceInfo = GrannyGetFileInfo(SetFiles[FileIdx]);
            if (!Set->SourceFileReferences[FileIdx]->SourceInfo)
            {
                // really bad.
                ErrOut("Unable to get info for '%s'\n", Set->SourceFileReferences[FileIdx]->SourceFilename);
                return false;
            }

            // Valid error, more likely. We're still going to leak that memory though.
            if (Set->SourceFileReferences[FileIdx]->SourceInfo->AnimationCount !=
                Set->SourceFileReferences[FileIdx]->ExpectedAnimCount)
            {
                ErrOut("Mismatch #Animations for '%s'\n", Set->SourceFileReferences[FileIdx]->SourceFilename);
                return false;
            }

            // Recompute the AnimCRC for this Source and reassign
            Set->SourceFileReferences[FileIdx]->AnimCRC =
                ComputeAnimCRC(Set->SourceFileReferences[FileIdx]->SourceInfo);
        }

        // Run through the animation specs and update the ExpectedName fields
        for (granny_int32x SpecIdx = 0; SpecIdx < Set->AnimationSpecCount; ++SpecIdx)
        {
            animation_spec& Spec = Set->AnimationSpecs[SpecIdx];
            granny_file_info* Info = Spec.SourceReference->SourceInfo;
            assert(Info);
            assert(Spec.AnimationIndex >= 0 && Spec.AnimationIndex < Info->AnimationCount);

            Spec.ExpectedName = (char*)Info->Animations[Spec.AnimationIndex]->Name;
        }
    }

    bool Success = SimpleWriteCharacterInfo(KeyValues, NumKeyValues,
                                            CharacterInfo, TempArena);

    // Release the loaded files, after the write, we're done with them...
    for (size_t Idx = 0; Idx < AllFiles.size(); ++Idx)
        GrannyFreeFile(AllFiles[Idx]);

    return Success;
}

static const char* HelpString =
    (" RefreshGSFSpecs reads in the specified Animation Studio file, scans\n"
     " its referenced animations, and updates the ExpectedName and AnimCRC\n"
     " fields to reflect any changes in the underlying animation files.\n"
     "\n"
     " Note that there is no error checking done here other than the number\n"
     " of animations that are expected to be in the GR2 files.  (Refresh\n"
     " cannot handle source modifications that change animation index \n"
     " layout.)  Please be sure you have a valid copy of the file checked\n"
     " into source control, or that you set -output to a different file\n"
     " name to prevent accidental data loss."
     "\n"
     "    preprocessor RefreshGSFSpecs file.gsf -output file.gsf \n"
     "    preprocessor RefreshGSFSpecs file.gsf -output file_updated.gsf \n");

static CommandRegistrar RegRefreshGSFSpecs(eSingleFile_NoMultiRun,
                                           RefreshGSFSpecs,
                                           "RefreshGSFSpecs",
                                           "Updates the GR2 references in the specified GSF file.",
                                           HelpString);



