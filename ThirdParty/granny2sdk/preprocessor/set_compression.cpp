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

bool CompressionDriver(input_file&     InputFile,
                       key_value_pair* KeyValues,
                       granny_int32x   NumKeyValues,
                       granny_compression_type CompressionType)
{
    RequireKey("output", "must specify an output file with \"-output <filename>\"\n");

    granny_variant variant;
    GrannyGetDataTreeFromFile(InputFile.GrannyFile, &variant);

    // Write out the converted Info
    granny_int32x FinalSectionCount = InputFile.GrannyFile->SectionCount;
    granny_file_builder *Builder = GrannyBeginFile(FinalSectionCount,
                                                   InputFile.GrannyFile->Header->TypeTag,
                                                   InputFile.GrannyFile->SourceMagicValue,
                                                   GrannyGetTemporaryDirectory(),
                                                   "Prefix");
    {for (int SectionIdx = 0; SectionIdx < FinalSectionCount; ++SectionIdx)
    {
        granny_grn_section *SectionArray =
            GrannyGetGRNSectionArray(InputFile.GrannyFile->Header);

        GrannySetFileSectionFormat(Builder,
                                   SectionIdx,
                                   CompressionType,
                                   SectionArray[SectionIdx].InternalAlignment);
    }}

    granny_file_data_tree_writer *Writer =
        GrannyBeginFileDataTreeWriting(variant.Type, variant.Object,
                                       0, 0);
    GrannyPreserveObjectFileSections(Writer, InputFile.GrannyFile);
    GrannyWriteDataTreeToFileBuilder(Writer, Builder);
    GrannyEndFileDataTreeWriting(Writer);

    char const* OutputFile = FindFirstValueForKey(KeyValues, NumKeyValues, "output");
    pp_assert(OutputFile);

    bool success = GrannyEndFile(Builder, OutputFile);
    if (!success)
    {
        ErrOut("failed to write to %s\n", OutputFile);
    }

    return success;
}

bool Decompress(input_file&     InputFile,
                key_value_pair* KeyValues,
                granny_int32x   NumKeyValues,
                granny_memory_arena*)
{
    return CompressionDriver(InputFile, KeyValues, NumKeyValues, GrannyNoCompression);
}


bool OodleCompress(input_file&     InputFile,
                   key_value_pair* KeyValues,
                   granny_int32x   NumKeyValues,
                   granny_memory_arena*)
{
    return CompressionDriver(InputFile, KeyValues, NumKeyValues, GrannyOodle1Compression);
}

bool BitKnitCompress(input_file&     InputFile,
                     key_value_pair* KeyValues,
                     granny_int32x   NumKeyValues,
                     granny_memory_arena*)
{
    return CompressionDriver(InputFile, KeyValues, NumKeyValues, GrannyBitKnitCompression);
}

bool BitKnit2Compress(input_file&     InputFile,
                      key_value_pair* KeyValues,
                      granny_int32x   NumKeyValues,
                      granny_memory_arena*)
{
    return CompressionDriver(InputFile, KeyValues, NumKeyValues, GrannyBitKnit2Compression);
}

static const char* HelpString =
    (" Compress/Decompress enables or disables the on-disk compression for a\n"
     " Granny file. This should typically be the last command in a preprocessor\n"
     " chain, since many of the processing commands disable compression for\n"
     " efficiency.\n"
     "\n"
     " CompressOodle1 produces the old, slower Granny compression format. Use \n"
     " this only if you want create files that can be loaded by Granny prior \n"
     " to 2.9.35.0. \n"
     "\n"
     " CompressBitKnit produces the BitKnit 1 format. Use this to create files\n"
     " that can be loaded by Granny 2.10.\n"
     "\n"
     " CompressBitKnit2 (or just Compress) generates BitKnit 2 format data,\n"
     " which enables compression for more parts of the Granny file and is\n"
     " generally smaller. It's preferred for Granny 2.11 and up.\n"
     "\n"
     "    preprocessor CompressBitKnit2 file.gr2 -output file_comp.gr2\n"
     "    preprocessor CompressBitKnit2 file.gr2 -output file.gr2\n"
     "\n"
     "    preprocessor CompressOodle1 file.gr2 -output file_comp.gr2\n"
     "    preprocessor CompressOodle1 file.gr2 -output file.gr2\n"
     "\n"
     "    preprocessor Decompress file_comp.gr2 -output file.gr2\n"
     "    preprocessor Decompress file.gr2 -output file.gr2\n");

static CommandRegistrar RegCompressOodle1(eSingleFile_NoMultiRun, OodleCompress, "CompressOodle1",
                                       "Compresses the input file using the old, slower Granny compression format.",
                                       HelpString);
static CommandRegistrar RegCompress(eSingleFile_NoMultiRun, BitKnit2Compress, "Compress",
                                    "Compresses the input file using BitKnit2",
                                    HelpString);
static CommandRegistrar RegCompressBitKnit(eSingleFile_NoMultiRun, BitKnitCompress, "CompressBitKnit",
                                    "Compresses the input file using BitKnit",
                                    HelpString);
static CommandRegistrar RegCompressBitKnit2(eSingleFile_NoMultiRun, BitKnit2Compress, "CompressBitKnit2",
                                    "Compresses the input file using BitKnit2",
                                    HelpString);
static CommandRegistrar RegDecompress(eSingleFile_NoMultiRun, Decompress, "Decompress",
                                      "Removes any compression on the input file",
                                      HelpString);

