// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "preprocessor.h"
#include "preprocessor_mod_support.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GRANNY_CALLBACK(granny_uint32) IdentityMapping(void*, char const* String)
{
    // 64-bit problems here...
    return (granny_uint32)(granny_intaddrx)String;
}

bool GetPlatformMVFromKeyValues(key_value_pair* KeyValues,
                                granny_int32x   NumKeyValues,
                                granny_grn_file_magic_value& MagicValue)
{
    granny_int32x PointerSize = 32;
    bool LittleEndian         = true;

    char const* PointerValue = FindFirstValueForKey(KeyValues, NumKeyValues, "pointer");
    if (PointerValue != 0)
    {
        PointerSize = atoi(PointerValue);
    }

    char const* EndianValue = FindFirstValueForKey(KeyValues, NumKeyValues, "endian");
    if (EndianValue != 0)
    {
        if (_stricmp(EndianValue, "big") == 0)
            LittleEndian = false;
        else if (_stricmp(EndianValue, "little") == 0)
            LittleEndian = true;
        else
        {
            ErrOut("unable to parse endian key, must be \"big\" or \"little\"\n");
            return false;
        }
    }

    if (GrannyGetMagicValueForPlatform(PointerSize, LittleEndian, &MagicValue) == false)
    {
        ErrOut("unable to find platform (%dbit, %s-endian)\n",
                PointerSize, LittleEndian ? "little" : "big");
        return false;
    }

    return true;
}


bool PlatformConvert(input_file&     InputFile,
                     key_value_pair* KeyValues,
                     granny_int32x   NumKeyValues,
                     granny_memory_arena* TempArena)
{
    RequireKey("output", "must specify an output file with \"-output <filename>\"\n");

    granny_file* File      = InputFile.GrannyFile;

    // Default to 32le
    bool PreserveStringMapping = false;

    char const* Preserve = FindFirstValueForKey(KeyValues, NumKeyValues, "remapped");
    if (Preserve)
    {
        if (_stricmp(Preserve, "true") == 0)
        {
            PreserveStringMapping = true;
        }
        else if (_stricmp(Preserve, "false") != 0)
        {
            ErrOut("key for -remapped invalid, only \"true\"|\"false\" recognized\n");
            return false;
        }
    }

    granny_grn_file_magic_value MagicValue;
    if (!GetPlatformMVFromKeyValues(KeyValues, NumKeyValues, MagicValue))
    {
        return false;
    }

    granny_int32x FinalSectionCount = File->SectionCount;

    // Write out the modified Info
    granny_file_builder *Builder =
        GrannyBeginFile(FinalSectionCount,
                        File->Header->TypeTag,  // we haven't changed this at all, actually.
                        &MagicValue,
                        GrannyGetTemporaryDirectory(),
                        "Prefix");

    // Get this from the utility to account for mod support
    char const* OutputFile = FindFirstValueForKey(KeyValues, NumKeyValues, "output");
    pp_assert(OutputFile);

    granny_variant RootType;
    GrannyGetDataTreeFromFile(File, &RootType);

    granny_file_data_tree_writer *Writer = CreateGenericTreeWriter(OutputFile, RootType.Type, RootType.Object);
    pp_assert(Writer);

    GrannyPreserveFileSectionFormats(Builder, InputFile.GrannyFile);
    GrannyPreserveObjectFileSections(Writer, File);
    if (PreserveStringMapping)
    {
        GrannySetFileStringDatabaseCRC(Builder, File->Header->StringDatabaseCRC);
        GrannySetFileWriterStringCallback(Writer, IdentityMapping, 0);
    }

    GrannyWriteDataTreeToFileBuilder(Writer, Builder);
    GrannyEndFileDataTreeWriting(Writer);

    bool success = GrannyEndFile(Builder, OutputFile);
    if (!success)
    {
        ErrOut("failed to write to %s\n", OutputFile);
    }

    return success;
}


static const char* HelpString =
    (" PlatformConvert alters the on-disk endian and pointer-size setting\n"
     " for a Granny file.  Files will always load on any platform, but they\n"
     " are most efficient when the file byte ordering matches the target\n"
     " platform.\n"
     "\n"
     "     preprocessor PlatformConvert file.gr2 -output file_32_be.gr2 \\\n"
     "         -pointer 32 -endian big\n"
     "\n"
     "     preprocessor PlatformConvert file.gr2 -output file_64_le.gr2 \\\n"
     "         -pointer 64 -endian little\n");

static CommandRegistrar RegPlatformConvert(eSingleFile_NoMultiRun,
                                           PlatformConvert,
                                           "PlatformConvert",
                                           "Converts the file to the specified platform.",
                                           HelpString);

