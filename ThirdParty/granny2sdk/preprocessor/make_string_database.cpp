// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "make_string_database.h"

#include <set>
#include <string>
#include <algorithm>

using namespace std;

struct remap_context
{
    granny_string_database* Database;
    bool                    MissingStringFound;

    remap_context(granny_string_database* Database_)
      : Database(Database_), MissingStringFound(false) {}
};

struct string_less_fn : public binary_function<char const*, char const*, bool>
{
    bool operator()(char const* One, char const* Two)
    {
        return GrannyStringDifference(One, Two) < 0;
    }
};
typedef set<char const*, string_less_fn> RemapStringSet;

granny_uint32 GatherStringsCallback(void *Data, char const *String);
granny_uint32 RemapStringsCallback(void *Data, char const *String);

void f(char const* v);

template <class T> void t(char const* v) { }
template <class T> void tf(T const* v) { }

bool ExtractStrings(input_file*     InputFiles,
                    granny_int32x   NumInputFiles,
                    key_value_pair* KeyValues,
                    granny_int32x   NumKeyValues,
                    granny_memory_arena* TempArena)
{
    char const* DatabaseName = FindFirstValueForKey(KeyValues, NumKeyValues, "database");
    if (DatabaseName == 0)
    {
        ErrOut("must specify a database with \"-database <filename>\"\n");
        return false;
    }

    for (int Idx = 0; Idx < NumInputFiles; ++Idx)
    {
        if (InputFiles[Idx].GrannyFile->Header->StringDatabaseCRC != 0)
        {
            ErrOut("file \"%s\" already has remapped strings, cannot extract!\n", InputFiles[Idx].Filename);
            return false;
        }
    }

    RemapStringSet FinalStrings;

    // Always insert the NULL and empty string...
    FinalStrings.insert((char const*)0);
    FinalStrings.insert("");

    // Pull the currents strings out of the input database
    granny_file* PrevDatabaseFile = GrannyReadEntireFile(DatabaseName);
    if (PrevDatabaseFile != NULL)
    {
        granny_string_database* PrevDatabase = GrannyGetStringDatabase(PrevDatabaseFile);
        if (PrevDatabase != NULL)
        {
            {for(granny_int32 i = 0; i < PrevDatabase->StringCount; ++i)
            {
                FinalStrings.insert(PrevDatabase->Strings[i]);
            }}
        }
        else
        {
            ErrOut("%s\n",
                    DatabaseName);
            return false;
        }
    }

    // Walk the files to extract new strings...
    {for(granny_int32x FileIdx = 0; FileIdx < NumInputFiles; ++FileIdx)
    {
        granny_variant Variant;
        GrannyGetDataTreeFromFile(InputFiles[FileIdx].GrannyFile, &Variant);

        // Pretend to write out the file, really we're just gathering strings...
        granny_file_builder *Builder = GrannyBeginFile(GrannyStandardSectionCount, InputFiles[FileIdx].GrannyFile->Header->TypeTag,
                                                       GrannyGRNFileMV_ThisPlatform,
                                                       GrannyGetTemporaryDirectory(),
                                                       "Prefix");
        granny_file_data_tree_writer *Writer =
            GrannyBeginFileDataTreeWriting(Variant.Type, Variant.Object, GrannyStandardDiscardableSection, 0);
        GrannySetFileWriterStringCallback(Writer, GatherStringsCallback,
                                          (void*)&FinalStrings);

        GrannyWriteDataTreeToFileBuilder(Writer, Builder);
        GrannyEndFileDataTreeWriting(Writer);
        GrannyAbortFile(Builder);
    }}


    // Create the final string_database object
    granny_string_database StringDatabase;
    {
        GrannyMakeEmptyDataTypeObject(GrannyStringDatabaseType, &StringDatabase);
        StringDatabase.StringCount = granny_int32(FinalStrings.size());
        StringDatabase.Strings = PushArray(TempArena, StringDatabase.StringCount, char*);

        copy(FinalStrings.begin(), FinalStrings.end(), (char const**)StringDatabase.Strings);

        // This fills in the DatabaseCRC field in the Approved Fashion...
        GrannyCreateStringDatabaseCRC(&StringDatabase);
    }

    // Write out the resulting database...
    granny_file_builder *Builder = GrannyBeginFile(GrannyStandardSectionCount, GrannyCurrentGRNStandardTag,
                                                   GrannyGRNFileMV_ThisPlatform,
                                                   GrannyGetTemporaryDirectory(),
                                                   "Prefix");
    granny_file_data_tree_writer *Writer =
        GrannyBeginFileDataTreeWriting(GrannyStringDatabaseType, &StringDatabase, GrannyStandardDiscardableSection, 0);
    GrannyWriteDataTreeToFileBuilder(Writer, Builder);
    GrannyEndFileDataTreeWriting(Writer);

    bool success = GrannyEndFile(Builder, DatabaseName);
    if (!success)
    {
        ErrOut("failed to write database to %s\n", DatabaseName);
    }

    if (PrevDatabaseFile != NULL)
    {
        GrannyFreeFile(PrevDatabaseFile);
    }

    return success;
}


bool RemapStrings(input_file& InputFile,
                  key_value_pair* KeyValues,
                  granny_int32x   NumKeyValues,
                  granny_memory_arena*)
{
    char const* DatabaseName = FindFirstValueForKey(KeyValues, NumKeyValues, "database");
    char const* OutputFile = FindFirstValueForKey(KeyValues, NumKeyValues, "output");
    {
        if (DatabaseName == 0)
        {
            ErrOut("must specify a database with \"-database <filename>\"\n");
            return false;
        }

        if (OutputFile == 0)
        {
            ErrOut("must specify an output with \"-output <filename>\"\n");
            return false;
        }
    }

    if (InputFile.GrannyFile->Header->StringDatabaseCRC != 0)
    {
        ErrOut("file already has remapped strings!\n");
        return false;
    }

    // Get the string database
    granny_file* DatabaseFile = GrannyReadEntireFile(DatabaseName);
    if (DatabaseFile == NULL)
    {
        ErrOut("couldn't open %s for reading\n", DatabaseName);
        return false;
    }

    granny_string_database* Database = GrannyGetStringDatabase(DatabaseFile);
    {
        if (Database == NULL)
        {
            ErrOut("couldn't get a granny_string_database from %s\n", DatabaseName);
            return false;
        }

        if (Database->StringCount == 0)
        {
            ErrOut("input Database '%s' doesn't appear to be valid (no strings)\n", DatabaseName);
            return false;
        }
        {for(granny_int32 Idx = 2; Idx < Database->StringCount; ++Idx)
        {
            if (string_less_fn()(Database->Strings[Idx], Database->Strings[Idx-1]))
            {
                ErrOut("input Database '%s' doesn't appear to be valid (unsorted entries)\n", DatabaseName);
                return false;
            }
        }}
    }


    // Read the input file...
    granny_variant Variant;
    GrannyGetDataTreeFromFile(InputFile.GrannyFile, &Variant);

    // Ok.  Now we need to rewrite the input file, preserving sectioning, and remapping
    // the strings as we find them.
    remap_context RemapContext(Database);

    granny_int32x FinalSectionCount = InputFile.GrannyFile->SectionCount;
    granny_file_builder *Builder = GrannyBeginFile(FinalSectionCount,
                                                   InputFile.GrannyFile->Header->TypeTag,
                                                   InputFile.GrannyFile->SourceMagicValue,
                                                   GrannyGetTemporaryDirectory(),
                                                   "Prefix");
    GrannySetFileStringDatabaseCRC(Builder, Database->DatabaseCRC);

    granny_file_data_tree_writer *Writer =
        GrannyBeginFileDataTreeWriting(Variant.Type, Variant.Object,
                                       GrannyStandardDiscardableSection, 0);
    GrannySetFileWriterStringCallback(Writer, RemapStringsCallback,
                                      (void*)&RemapContext);
    GrannyPreserveFileSectionFormats(Builder, InputFile.GrannyFile);
    GrannyPreserveObjectFileSections(Writer, InputFile.GrannyFile);

    GrannyWriteDataTreeToFileBuilder(Writer, Builder);
    GrannyEndFileDataTreeWriting(Writer);

    bool success = GrannyEndFile(Builder, OutputFile);
    if (success == false)
    {
        ErrOut("failed to write to %s\n", OutputFile);
    }
    else if (RemapContext.MissingStringFound)
    {
        ErrOut("MISSING STRINGS found in %s, was this file included in the database build?\n", OutputFile);
    }

    pp_assert(DatabaseFile);
    GrannyFreeFile(DatabaseFile);

    return true;
}


// ======================================================================
// Callbacks for gathering and remapping strings...
//
granny_uint32 GatherStringsCallback(void *Data, char const *String)
{
    RemapStringSet* FinalStrings = reinterpret_cast<RemapStringSet*>(Data);

    FinalStrings->insert(String);

    // For the gather callback, we don't actually care about the string returned...
    return 0;
}

granny_uint32 RemapStringsCallback(void *Data, char const *String)
{
    remap_context* RemapContext = reinterpret_cast<remap_context*>(Data);

    // Braindead linear search for right now.  We do ensure the database is
    // sorted, so this could be replaced with a binsearch in the future
    // if we need a speedup.
    string_less_fn Fn;
    {for (granny_int32 i = 0; i < RemapContext->Database->StringCount; ++i)
    {
        // Use the same functor for consistency...
        if (!Fn(String, RemapContext->Database->Strings[i]) &&
            !Fn(RemapContext->Database->Strings[i], String))
        {
            return granny_uint32(i);
        }
    }}

    // Oh, very bad.  We don't have that string in our table...
    RemapContext->MissingStringFound = true;
    return 0;
}


static const char* HelpString =
    (" ExtractStrings/RemapStrings: These commands allow you to create\n"
     " a granny_string_database from a collection of Granny files, and\n"
     " remap the source file strings to indices in that database.\n"
     "\n"
     " For example, consider the trivial case of 2 files, \"char1.gr2\"\n"
     " and \"char2.gr2\".\n"
     "\n"
     "    preprocessor ExtractStrings char1.gr2 char2.gr2 -database strings.gsd\n"
     "\n"
     "    preprocessor RemapStrings char1.gr2 -output char1_remap.gr2 -database strings.gsd\n"
     "    preprocessor RemapStrings char2.gr2 -output char2_remap.gr2 -database strings.gsd\n"
     "\n"
     " The first command builds the database, and the second remaps the strings. Note that\n"
     " you may add to an existing database, we could have just as easily written:\n"
     "\n"
     "    preprocessor ExtractStrings char1.gr2 -database strings.gsd\n"
     "    preprocessor ExtractStrings char2.gr2 -database strings.gsd\n"
     "\n"
     " for the extraction phase. However, every time you modify the database \n"
     " you must remap all of the files that are remapped using that set of\n"
     " strings. It's possible that in the extraction phase, strings may be\n"
     " moved to alternate indices, which would invalidate the previous mappings.\n");

// Register the commands
static CommandRegistrar RegExtractStrings(ExtractStrings, "ExtractStrings",
                                          "Creates or updates a string_database with the input files' strings (use -database to specify)",
                                          HelpString);

static CommandRegistrar RegRemapStrings(eSingleFile_NoMultiRun, RemapStrings, "RemapStrings",
                                        "Remaps the strings in the given file to the input string_database (use -database to specify)",
                                        HelpString);


