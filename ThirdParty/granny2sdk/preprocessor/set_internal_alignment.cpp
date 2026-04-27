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

static bool
ValidAlignment(char const* Alignment, int* AlignVal)
{
    pp_assert(AlignVal);

    if (Alignment == 0)
        return false;

    int& Val = *AlignVal;

    Val = atoi(Alignment);
    if (Val <= 3)
        return false;

    // !pow2?
    if (Val & (Val - 1))
        return false;

    return true;
}

// Special version of this function to preserve platform and compression settings while
// setting alignment.
static bool
WriteInfoPreserveWithAligns(char const* OutputFile,
                            granny_file_info* Info,
                            granny_file* OriginalFile,
                            granny_int32x* Aligns,
                            granny_memory_arena* TempArena)
{
    pp_assert(OriginalFile);

    AttachHistoryEntry(Info, TempArena);

    granny_int32x FinalSectionCount = OriginalFile->SectionCount;
    if (FinalSectionCount < GrannyStandardSectionCount)
        FinalSectionCount = GrannyStandardSectionCount;

    granny_grn_file_magic_value const* PlatformMV = OriginalFile->SourceMagicValue;

    granny_file_builder *Builder =
        GrannyBeginFile(FinalSectionCount, GrannyCurrentGRNStandardTag,
                        PlatformMV, GrannyGetTemporaryDirectory(), "Prefix");

    granny_file_data_tree_writer* Writer = CreateTreeWriter(OutputFile, Info);
    pp_assert(Writer);

    GrannyPreserveObjectFileSections(Writer, OriginalFile);

    granny_grn_section const *SectionArray = GrannyGetGRNSectionArray(OriginalFile->Header);
    {for(granny_int32x SectionIndex = 0;
         SectionIndex < OriginalFile->SectionCount;
         ++SectionIndex)
    {
        granny_grn_section const &Section = SectionArray[SectionIndex];
        granny_compression_type CompressionType = (granny_compression_type)Section.Format;
        GrannySetFileSectionFormat(Builder, SectionIndex,
                                   CompressionType,
                                   Aligns[SectionIndex]);
    }}

    GrannyWriteDataTreeToFileBuilder(Writer, Builder);
    GrannyEndFileDataTreeWriting(Writer);

    bool success = GrannyEndFile(Builder, OutputFile);
    if (!success)
    {
        ErrOut("Failed to write granny_file_info to %s\n", OutputFile);
    }

    return success;
}


bool SetInternalAlignment(input_file&     InputFile,
                          key_value_pair* KeyValues,
                          granny_int32x   NumKeyValues,
                          granny_memory_arena* TempArena)
{
    RequireKey("output", "must specify an output file with \"-output <filename>\"\n");

    int AlignVal = -1;
    char const* Alignment = FindFirstValueForKey(KeyValues, NumKeyValues, "align");
    if (!ValidAlignment(Alignment, &AlignVal))
    {
        ErrOut("must specify an alignment with \"-align <value>\".  Value must be pow2 >= 4\n");
        return false;
    }

    granny_file* File = InputFile.GrannyFile;
    granny_file_info* Info = ExtractFileInfo(InputFile);
    if (Info == 0)
    {
        ErrOut("unable to obtain a granny_file_info from the input file\n");
        return false;
    }

    granny_int32x* Aligns = new granny_int32[File->SectionCount];
    granny_grn_section* Sections = GrannyGetGRNSectionArray(File->Header);
    for (int i = 0; i < File->SectionCount; ++i)
    {
        Aligns[i] = Sections[i].InternalAlignment;
    }

    for (int i = 0; i < NumKeyValues; ++i)
    {
        if (_stricmp(KeyValues[i].Key, "section") == 0)
        {
            int Section = atoi(KeyValues[i].Value);
            if (Section >= 0 && Section < File->SectionCount)
            {
                Aligns[Section] = AlignVal;
            }
        }
    }

    char const* OutputFile = FindFirstValueForKey(KeyValues, NumKeyValues, "output");
    pp_assert(OutputFile);

    bool Result = WriteInfoPreserveWithAligns(OutputFile, Info, File, Aligns, TempArena);
    delete [] Aligns;

    return Result;
}

static const char* HelpString =
    (" SetInternalAlignment ensures that all objects and allocations\n"
     " for the specified section are aligned to a specific granularity.\n"
     "\n"
     "    preprocessor SetInternalAlignment file.gr2 -output file_align.gr2 \\\n"
     "        -align 64 -section 5 -section 6\n"
     "\n"
     " (See SetFileAlignment to set on-disk alignment)\n");

static CommandRegistrar RegSetInternalAlignment(eSingleFile_NoMultiRun,
                                                SetInternalAlignment,
                                                "SetInternalAlignment",
                                                "Sets the file's section alignment. (Specify with -align, name sections with -section.)",
                                                HelpString);

