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
WriteInfoPreserveWithAlign(char const* OutputFile,
                           granny_file_info* Info,
                           granny_file* OriginalFile,
                           int AlignVal,
                           granny_memory_arena* TempArena)
{
    pp_assert(OriginalFile);
    pp_assert(AlignVal >= 4 && (AlignVal & (AlignVal-1)) == 0);

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
    GrannyPreserveFileSectionFormats(Builder, OriginalFile);
    GrannySetFileDiskAlignment(Builder, AlignVal);

    GrannyWriteDataTreeToFileBuilder(Writer, Builder);
    GrannyEndFileDataTreeWriting(Writer);

    bool success = GrannyEndFile(Builder, OutputFile);
    if (!success)
    {
        ErrOut("Failed to write granny_file_info to %s\n", OutputFile);
    }

    return success;
}


bool SetFileAlignment(input_file&     InputFile,
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

    char const* OutputFile = FindFirstValueForKey(KeyValues, NumKeyValues, "output");
    pp_assert(OutputFile);

    return WriteInfoPreserveWithAlign(OutputFile, Info, File, AlignVal, TempArena);
}

static const char* HelpString =
    (" SetFileAlignment alters a Granny file such that all reads from\n"
     " the file will be aligned to the specified byte boundary.  This\n"
     " is to support platforms like the Wii, which require that DVD\n"
     " reads be aligned to 512 bytes for maximum efficiency.\n"
     "\n"
     " Note that this is *different* than 'SetInternalAlignment', which\n"
     " guarantees that all allocations and structure will align to a\n"
     " specified granularity, but may not align disk reads\n"
     "\n"
     "    preprocessor SetFileAlignment file.gr2 file512.gr2 -align 512\n");

static CommandRegistrar RegSetFileAlignment(eSingleFile_NoMultiRun,
                                            SetFileAlignment,
                                            "SetFileAlignment",
                                            "Sets the file's on disk alignment to the value specified with \"-align\".",
                                            HelpString);

