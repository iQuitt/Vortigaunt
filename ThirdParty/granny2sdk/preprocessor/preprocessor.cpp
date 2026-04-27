// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
//
// Driver for the Granny Preprocessor
//
//   The idea here is to separate the process of parsing and
// validating arguments, loading source data, and dispatching the
// command from actually processing the meshes.  To register a custom
// command, simply create a new function of the PreprocessorCommand
// type, and register it with a unique string in the command table.
//
//   We recommend that you place your new commands in separate source
// files modules, rather than modifying the distribution files in-place,
// since this will ease upgrades if modifications are made to the
// preprocessor.
// =============================================================================
#include "preprocessor.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <string>
#include <algorithm>

#include "preprocessor_mod_support.h"

// one file must include with the impl macro
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// one file must include with the impl macro
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"


using namespace std;

bool        g_SaveHistory = false;
std::string g_CommandLine;

#if BUILDING_GRANNY_FOR_MODS
static char const* s_PreprocessorName = MOD_PREPROCESSOR_NAME;
#else
static char const* s_PreprocessorName = "Granny Preprocessor";
#endif



// for sorting
bool command_entry::operator<(command_entry const& Other) const
{
    return _stricmp(CommandName.c_str(), Other.CommandName.c_str()) < 0;
}

command_entry::command_entry()
  : PreservePlatform(true),
    PreserveObjectSections(true),
    PreserveSectionFormats(true)
{
}


vector<command_entry>& GlobalCommandList()
{
    static vector<command_entry> CommandList;

    return CommandList;
}


int FindCommandEntry(char const* CommandName)
{
    for (int i = 0; i < int(GlobalCommandList().size()); ++i)
    {
        if (_stricmp(GlobalCommandList()[i].CommandName.c_str(), CommandName) == 0)
        {
            return i;
        }
    }

    return -1;
}

void
AttachHistoryEntry(granny_file_info*    Info,
                   granny_memory_arena* TempArena,
                   string*              CustomEntry /* = 0 */)
{
    if (g_SaveHistory == false)
        return;

    string ToAttach = CustomEntry ? *CustomEntry : g_CommandLine;

    static const char* HistoryVarName = "PreprocessorHistory";
    struct HistEntry
    {
        granny_int32 HistoryCount;
        char const** History;
    };
    static granny_data_type_definition HistEntryType[] = {
        { GrannyReferenceToArrayMember, HistoryVarName, GrannyStringType },
        { GrannyEndMember }
    };


    if (Info->ExtendedData.Type)
    {
        pp_assert(Info->ExtendedData.Object);

        granny_variant Result;
        if (GrannyFindMatchingMember(Info->ExtendedData.Type,
                                     Info->ExtendedData.Object,
                                     HistoryVarName,
                                     &Result))
        {
            // Already present, just modify
            pp_assert(Result.Type[0].Type == GrannyReferenceToArrayMember);

            granny_int32& CountLoc = *((granny_int32*)(Result.Object));
            char const**& ArrayLoc = *((char const***)((granny_uint8*)Result.Object + sizeof(granny_int32)));

            char const** NewArray = PushArray(TempArena, CountLoc+1, char const*);
            memcpy(NewArray, ArrayLoc, CountLoc * sizeof(char const*));
            NewArray[CountLoc] = PushString(TempArena, ToAttach.c_str());
            ArrayLoc = NewArray;
            ++CountLoc;
        }
        else
        {
            // Not present, add to existing type
            int NumTypes = 0;
            while (Info->ExtendedData.Type[NumTypes].Type != GrannyEndMember)
                ++NumTypes;

            int const OldTypeSize = (NumTypes+1) * sizeof(granny_data_type_definition);
            int const NewTypeSize = OldTypeSize + sizeof(granny_data_type_definition);
            granny_data_type_definition* NewType =
                (granny_data_type_definition*)PushArray(TempArena, NewTypeSize, granny_uint8);

            memcpy(NewType, Info->ExtendedData.Type, NumTypes * sizeof(granny_data_type_definition));
            memset(&NewType[NumTypes], 0, sizeof(granny_data_type_definition));
            NewType[NumTypes].Type = GrannyReferenceToArrayMember;
            NewType[NumTypes].Name = HistoryVarName;
            NewType[NumTypes].ReferenceType = GrannyStringType;
            NewType[NumTypes].ArrayWidth = 1;

            memset(&NewType[NumTypes+1], 0, sizeof(granny_data_type_definition));
            NewType[NumTypes+1].Type = GrannyEndMember;

            // Append to the existing object...
            int const OldObjectSize = GrannyGetTotalObjectSize(Info->ExtendedData.Type);
            int const NewObjectSize = OldObjectSize + sizeof(char*) + sizeof(granny_int32);

            granny_uint8* NewObject = PushArray(TempArena, NewObjectSize, granny_uint8);
            memcpy(NewObject, Info->ExtendedData.Object, OldObjectSize);

            granny_int32& CountLoc = *((granny_int32*)(NewObject + OldObjectSize));
            char const**& ArrayLoc = *((char const***)(NewObject + OldObjectSize + sizeof(granny_int32)));

            CountLoc = 1;
            ArrayLoc = PushArray(TempArena, 1, char const*);
            ArrayLoc[0] = PushString(TempArena, ToAttach.c_str());

            Info->ExtendedData.Type = NewType;
            Info->ExtendedData.Object = NewObject;
        }
    }
    else
    {
        // Tabula rasa, go nuts

        HistEntry* Entry = PushObject(TempArena, HistEntry);
        Entry->HistoryCount = 1;
        Entry->History = PushArray(TempArena, 1, char const*);

        Entry->History[0] = PushString(TempArena, ToAttach.c_str());
        Info->ExtendedData.Type   = HistEntryType;
        Info->ExtendedData.Object = Entry;
    }
}

#if BUILDING_GRANNY_FOR_MODS

// Examine the first element of the file's root data_type_definition array to see if it
// has the correct prefix.  Note: shared code with the preprocessor
static bool
IsMODFile(granny_file* File)
{
    if (!File)
        return false;

    granny_variant Root;
    GrannyGetDataTreeFromFile(File, &Root);

    if (Root.Type == 0 || Root.Type[0].Type == GrannyEndMember)
        return false;

    return _strnicmp(Root.Type[0].Name, MOD_UNIQUE_PREFIX, strlen(MOD_UNIQUE_PREFIX)) == 0;
}


// Use the extension when writing...
static bool
IsMODFilename(char const *FileName)
{
    char const *Extension = 0;
    {for(char const *Scan = FileName;
         *Scan;
         ++Scan)
    {
        if(*Scan == '.')
        {
            Extension = (Scan + 1);
        }
    }}

    return (Extension && _stricmp(Extension, MOD_FILE_EXTENSION) == 0);
}

granny_file_info*
ExtractFileInfo(input_file& InputFile)
{
    if (InputFile.GrannyFile && IsMODFile(InputFile.GrannyFile))
        return GrannyGetModFileInfo(InputFile.GrannyFile);
    else
        return GrannyGetFileInfo(InputFile.GrannyFile);
}

#else // BUILDING_GRANNY_FOR_MODS

granny_file_info*
ExtractFileInfo(input_file& InputFile)
{
    return GrannyGetFileInfo(InputFile.GrannyFile);
}

#endif // BUILDING_GRANNY_FOR_MODS


granny_file_data_tree_writer*
CreateTreeWriter(char const*       OutputFile,
                 granny_file_info* Info)
{
#if BUILDING_GRANNY_FOR_MODS
    granny_file_data_tree_writer *Writer = 0;
    volatile bool OnlyWriteMod = PREPROCESSOR_ONLY_WRITE_MOD;  // silly dance prevents static warning when OnlyWrite is defined
    if (OnlyWriteMod || IsMODFilename(OutputFile))
    {
        Writer = GrannyBeginFileDataTreeWriting(GrannyModFileInfoType, Info,
                                                GrannyStandardDiscardableSection, 0);
    }
    else
    {
        Writer = GrannyBeginFileDataTreeWriting(GrannyFileInfoType, Info,
                                                GrannyStandardDiscardableSection, 0);
    }
#else
    granny_file_data_tree_writer *Writer =
        GrannyBeginFileDataTreeWriting(GrannyFileInfoType, Info,
                                       GrannyStandardDiscardableSection, 0);
#endif
    pp_assert(Writer);

    return Writer;
}

granny_file_data_tree_writer*
CreateTreeWriter(char const* OutputFile, gstate_character_info* Info)
{
    granny_file_data_tree_writer *Writer =
        GrannyBeginFileDataTreeWriting(GStateCharacterInfoType, Info,
                                       0, 0);
    pp_assert(Writer);
    return Writer;
}

granny_file_data_tree_writer*
CreateGenericTreeWriter(char const*                  OutputFile,
                        granny_data_type_definition* Type,
                        void*                        Root)
{
    granny_file_data_tree_writer *Writer =
        GrannyBeginFileDataTreeWriting(Type, Root,
                                       GrannyStandardDiscardableSection, 0);
    pp_assert(Writer);

    return Writer;
}

bool
SimpleWriteCharacterInfo(key_value_pair* KeyValuePairs,
                         granny_int32x   NumKeyValuePairs,
                         gstate_character_info* Info,
                         granny_memory_arena* TempArena)
{
    char const* OutputFile = FindFirstValueForKey(KeyValuePairs, NumKeyValuePairs, "output");
    if (OutputFile == 0)
    {
        ErrOut("must specify an output file with \"-output <filename>\"\n");
        return false;
    }

    granny_file_builder *Builder =
        GrannyBeginFile(1, GStateCharacterInfoTag,
                        GrannyGRNFileMV_ThisPlatform,
                        GrannyGetTemporaryDirectory(),
                        "Prefix");

    granny_file_data_tree_writer *Writer = CreateTreeWriter(OutputFile, Info);
    pp_assert(Writer);

    GrannyWriteDataTreeToFileBuilder(Writer, Builder);
    GrannyEndFileDataTreeWriting(Writer);

    bool success = GrannyEndFile(Builder, OutputFile);
    if (!success)
    {
        fprintf(stderr, "Failed to write gstate_character_info to %s\n", OutputFile);
    }

    return success;
}

bool
SimpleWriteInfo(key_value_pair* KeyValuePairs,
                granny_int32x   NumKeyValuePairs,
                granny_file_info* Info,
                granny_memory_arena* TempArena)
{
    char const* OutputFile = FindFirstValueForKey(KeyValuePairs, NumKeyValuePairs, "output");
    if (OutputFile == 0)
    {
        ErrOut("must specify an output file with \"-output <filename>\"\n");
        return false;
    }

    AttachHistoryEntry(Info, TempArena);

    granny_file_builder *Builder =
        GrannyBeginFile(GrannyStandardSectionCount, GrannyCurrentGRNStandardTag,
                        GrannyGRNFileMV_ThisPlatform,
                        GrannyGetTemporaryDirectory(),
                        "Prefix");

    granny_file_data_tree_writer *Writer = CreateTreeWriter(OutputFile, Info);
    pp_assert(Writer);

    GrannyWriteDataTreeToFileBuilder(Writer, Builder);
    GrannyEndFileDataTreeWriting(Writer);

    bool success = GrannyEndFile(Builder, OutputFile);
    if (!success)
    {
        fprintf(stderr, "Failed to write granny_file_info to %s\n", OutputFile);
    }

    return success;
}

bool
WriteInfoPreserve(key_value_pair* KeyValuePairs,
                  granny_int32x   NumKeyValuePairs,
                  granny_file_info* Info,
                  granny_file* OriginalFile,
                  bool PreservePlatform,
                  bool PreserveObjectSections,
                  bool PreserveSectionFormats,
                  granny_memory_arena* TempArena)
{
    if (OriginalFile == 0)
    {
        return SimpleWriteInfo(KeyValuePairs, NumKeyValuePairs, Info, TempArena);
    }

    char const* OutputFile = FindFirstValueForKey(KeyValuePairs, NumKeyValuePairs, "output");
    if (OutputFile == 0)
    {
        ErrOut("must specify an output file with \"-output <filename>\"\n");
        return false;
    }

    AttachHistoryEntry(Info, TempArena);

    granny_int32x FinalSectionCount = OriginalFile->SectionCount;
    if (FinalSectionCount < GrannyStandardSectionCount)
        FinalSectionCount = GrannyStandardSectionCount;


    granny_grn_file_magic_value const* PlatformMV = GrannyGRNFileMV_ThisPlatform;
    if (OriginalFile && PreservePlatform)
    {
        PlatformMV = OriginalFile->SourceMagicValue;
    }

    granny_file_builder *Builder =
        GrannyBeginFile(FinalSectionCount, GrannyCurrentGRNStandardTag,
                        PlatformMV, GrannyGetTemporaryDirectory(), "Prefix");

    granny_file_data_tree_writer *Writer = CreateTreeWriter(OutputFile, Info);
    pp_assert(Writer);

    if (PreserveObjectSections)
        GrannyPreserveObjectFileSections(Writer, OriginalFile);

    if (PreserveSectionFormats)
        GrannyPreserveFileSectionFormats(Builder, OriginalFile);

    GrannyWriteDataTreeToFileBuilder(Writer, Builder);
    GrannyEndFileDataTreeWriting(Writer);

    bool success = GrannyEndFile(Builder, OutputFile);
    if (!success)
    {
        fprintf(stderr, "Failed to write granny_file_info to %s\n", OutputFile);
    }

    return success;
}


void usage()
{
    printf(" %s (v%s)\n"
           " usage:\n"
           "   preprocessor <command> [<input0.*> <input1.*> ...] [-key value ...]\n"
           "\n"
           "  Where <command> is a supported preprocessor operation, input*.* are\n"
           "  the inputs (any referenced input files must exist).  [-key value]\n"
           "  pairs are passed to the <command> operation as supplimental arguments.\n"
           "\n"
           "  Note that you may use the special command \"ListCommands\" to get a list\n"
           "  of the commands supported by this version of the preprocessor, and the\n"
           "  command \"Help <command_name>\" for information about them.\n"
           "\n"
           "  Use the \"Version\" command to query the version of Granny this tool was"
           "  compiled against.\n"
           "\n"
           " example:\n"
           "   preprocessor BonesPerMesh mesh.gr2 -output proc_mesh.gr2 -bonelimit 50\n\n",
           s_PreprocessorName,
           GrannyProductVersion);
}


static bool
Version(input_file*, granny_int32x,
        key_value_pair*, granny_int32x,
        granny_memory_arena*)
{
#if defined(NDEBUG)
    char const* Mode = "Release";
#else
    char const* Mode = "Debug";
#endif
#if defined(BUILDING_GRANNY_STATIC)
    char const* Linkage = "Static Library";
#else
    char const* Linkage = "Dynamic (DLL)";
#endif

    printf("Granny Preprocessor\n"
           "  Compiled with: %s\n"
           "           Mode: %s\n"
           "         Linked: %s\n",
           GrannyProductVersion, Mode, Linkage);
    return true;
}

static bool
ListCommands(input_file*, granny_int32x,
             key_value_pair*, granny_int32x,
             granny_memory_arena*)
{
    printf("\n"
           "  The %s supports the following %d commands.\n"
           "  Note that a '*' following a command name indicates that it is batchable.\n\n",
           s_PreprocessorName,
           int(GlobalCommandList().size()));

    size_t max_len = 0;
    {for(int i = 0; i < int(GlobalCommandList().size()); ++i)
    {
        size_t len = GlobalCommandList()[i].CommandName.length();
        if (len > max_len)
            max_len = len;
    }}

    {for(int i = 0; i < int(GlobalCommandList().size()); ++i)
    {
        bool IsBatchable = GlobalCommandList()[i].CommandType == eBatchable;
        if (!IsBatchable)
        {
            // Check our special cases: Decompress/Compress/PlatformConvert
            if (_stricmp(GlobalCommandList()[i].CommandName.c_str(), "Compress") == 0 ||
                _stricmp(GlobalCommandList()[i].CommandName.c_str(), "CompressOld") == 0 ||
                _stricmp(GlobalCommandList()[i].CommandName.c_str(), "Decompress") == 0 ||
                _stricmp(GlobalCommandList()[i].CommandName.c_str(), "PlatformConvert") == 0)
            {
                IsBatchable = true;
            }
        }

        printf(" %-*s%s: %s\n",
               (int)max_len,
               GlobalCommandList()[i].CommandName.c_str(),
               IsBatchable ? " * " : "   ",
               GlobalCommandList()[i].ShortDocString.c_str());
    }}

    return true;
}

bool Help(char const* CommandName)
{
    int CommandIndex = FindCommandEntry(CommandName);
    if (CommandIndex == -1)
        return false;

    command_entry& rEntry = GlobalCommandList()[CommandIndex];
    if (rEntry.DocString.empty() == false)
    {
        printf("\n%s\n", rEntry.CommandName.c_str());
        for (size_t i = rEntry.CommandName.length(); i > 0; --i)
            printf("-");
        printf("\n\n");
        
	puts(rEntry.DocString.c_str());
    }
    else
    {
        printf("No help for %s\n", CommandName);
    }
    
    return true;
}

static CommandRegistrar RegListCommands(ListCommands,
                                        "ListCommands",
                                        "Lists all of the commands supported by the preprocessor");
static CommandRegistrar RegVersion(Version,
                                   "Version",
                                   "Prints the version and library information for the preprocessor");

char const* FindFirstValueForKey(key_value_pair* KeyValuePairs,
                                 granny_int32x   NumKeyValuePairs,
                                 char const*     Key)
{
    if (Key == 0)
        return 0;

    {for(granny_int32x i = 0; i < NumKeyValuePairs; ++i)
    {
        if (_stricmp(KeyValuePairs[i].Key, Key) == 0)
            return KeyValuePairs[i].Value;
    }}

    return 0;
}

bool
RunBatchableCommand(command_entry&       Command,
                    input_file&          InputFile,
                    key_value_pair*      KeyValues,
                    granny_int32x        NumKeyValues,
                    granny_memory_arena* TempArena)
{
    pp_assert(Command.CommandType == eBatchable);

    char const* OutputFilename = FindFirstValueForKey(KeyValues, NumKeyValues, "output");
    if (!OutputFilename)
    {
        ErrOutLie(Command.CommandName.c_str(), "must specify an output file with \"-output <filename>\"\n");
        return false;
    }

    granny_file_info* Info = ExtractFileInfo(InputFile);
    if (Info == 0)
    {
        ErrOutLie(Command.CommandName.c_str(), "Unable to obtain file_info structure from: %s", InputFile.Filename);
        return false;
    }

    granny_file_info* OutputInfo = (*Command.BatchableFn)(InputFile.Filename, OutputFilename,
                                                          Info, KeyValues, NumKeyValues, TempArena);
    if (OutputInfo == 0)
    {
        // Command handles logging for this case
        return false;
    }

    return WriteInfoPreserve(KeyValues, NumKeyValues, OutputInfo, InputFile.GrannyFile,
                             Command.PreservePlatform,
                             Command.PreserveObjectSections,
                             Command.PreserveSectionFormats,
                             TempArena);
}



#if !defined(COMPILING_UNITS)

int main(int argc, const char** argv)
{
    // Ensure the API version matches
    if(!GrannyVersionsMatch)
    {
        printf("Warning: the Granny DLL currently loaded doesn't match the .h file used during compilation\n");

        granny_int32x Major, Minor, Build, Customization;
        GrannyGetVersion(&Major, &Minor, &Build, &Customization);

        printf("  Expected: %d.%d.%d.%d, found: %d.%d.%d.%d\n",
               GrannyProductMajorVersion, GrannyProductMinorVersion, GrannyProductBuildNumber, GrannyProductCustomization,
               Major, Minor, Build, Customization);
        return EXIT_FAILURE;
    }

    if (argc < 2)
    {
        usage();
        return EXIT_FAILURE;
    }

    // Check for the special "Help" command
    {
        if (_stricmp(argv[1], "help") == 0)
        {
            if (argc == 3 && Help(argv[2]))
            {
                return EXIT_SUCCESS;
            }
            else
            {
                fprintf(stderr, "  Misunderstood help request\n\n");
                usage();
                return EXIT_FAILURE;
            }
        }
    }

    // Command string is the first argument
    int CommandIndex = FindCommandEntry(argv[1]);
    if (CommandIndex == -1)
    {
        fprintf(stderr, "  Misunderstood command: %s.\n\n", argv[1]);
        usage();
        return EXIT_FAILURE;
    }
    command_entry& CommandEntry = GlobalCommandList()[CommandIndex];

    // Parse the input meshes, and the key value pairs.  We're going
    // to allocate too much temp space here, but this way we don't
    // have to grow the arrays.  Maximum number of input meshes is
    // (argc-2), max key/value pairs is (argc-2)/2.
    input_file*  InputFiles     = new input_file[argc - 2];
    granny_int32x NumInputFiles = 0;

    key_value_pair* KeyValuePairs = new key_value_pair[argc - 2];
    granny_int32x   NumKeyValuePairs = 0;

    {for(int i = 2; i < argc; /*explicit increment in the loop*/)
    {
        if (argv[i][0] == '-')
        {
            if (strcmp(argv[i], "-history") == 0)
            {
                // This is the switch that turns on history storage.  Reconstruct the
                // command line, and then stuff it where Simple
                if (!g_SaveHistory)
                {
                    g_SaveHistory = true;

                    g_CommandLine = "[preprocessor.exe]";
                    {for (int Idx = 1; Idx < argc; ++Idx)
                    {
                        g_CommandLine += " ";
                        g_CommandLine += argv[Idx];
                    }}
                }

                i += 1;
            }
            else if (i <= argc-2)
            {
                // proper k/v, create a pair object
                KeyValuePairs[NumKeyValuePairs].Key   = new char[strlen(argv[i])]; // ignore the '-'
                KeyValuePairs[NumKeyValuePairs].Value = new char[strlen(argv[i+1]) + 1];

                strcpy((char*)KeyValuePairs[NumKeyValuePairs].Key, argv[i]+1);
                strcpy((char*)KeyValuePairs[NumKeyValuePairs].Value, argv[i+1]);
                ++NumKeyValuePairs;

                i += 2;
            }
            else
            {
                // improper k/v, not enough arguments
                ErrOut("Bad argument list, unmatched key, must be followed by value (%s)\n", argv[i]);
                return EXIT_FAILURE;
            }
        }
        else
        {
            // Input file
            InputFiles[NumInputFiles].GrannyFile = GrannyReadEntireFile(argv[i]);
            InputFiles[NumInputFiles].Filename   = new char[strlen(argv[i]) + 1];
            strcpy(InputFiles[NumInputFiles].Filename, argv[i]);

            if (InputFiles[NumInputFiles].GrannyFile == 0)
            {
                ErrOut("Unable to load granny file: %s\n", argv[i]);
                return EXIT_FAILURE;
            }

            ++NumInputFiles;
            i += 1;
        }
    }}

    granny_memory_arena* TempArena = GrannyNewMemoryArena();
    GrannySetArenaAlignment(TempArena, 16);

    // Dispatch to the specified function...
    bool Success;
    if (CommandEntry.CommandType == eMultiFile)
    {
        Success =
            (*CommandEntry.MultiFn)(InputFiles, NumInputFiles,
                                    KeyValuePairs, NumKeyValuePairs,
                                    TempArena);
    }
    else if (CommandEntry.CommandType == eEmptyStart)
    {
        Success = (*CommandEntry.EmptyFn)(KeyValuePairs, NumKeyValuePairs, TempArena);
    }
    else if (CommandEntry.CommandType == eBatchable)
    {
        // This is batchable, but we can run it singly as well.
        if (NumInputFiles == 1)
        {
            Success =
                RunBatchableCommand(CommandEntry,
                                    InputFiles[0],
                                    KeyValuePairs, NumKeyValuePairs,
                                    TempArena);
        }
        else
        {
            ErrOut("Must have one and only one input file for %s, found: %d\n",
                   CommandEntry.CommandName.c_str(), NumInputFiles);
            Success = false;
        }
    }
    else
    {
        pp_assert(CommandEntry.CommandType == eSingleFile_NoMultiRun ||
                  CommandEntry.CommandType == eSingleFile_MultiRun);

        if (CommandEntry.CommandType == eSingleFile_NoMultiRun &&
            NumInputFiles != 1)
        {
            ErrOut("%s is a single file command that doesn't support multiple\n"
                   "invocations.  Please specify only one input file\n",
                   CommandEntry.CommandName.c_str());
            return EXIT_FAILURE;
        }

        Success = true;
        {for(int i = 0; i < NumInputFiles; ++i)
        {
            if ((*CommandEntry.SingleFn)(InputFiles[i],
                                         KeyValuePairs,
                                         NumKeyValuePairs,
                                         TempArena) == false)
            {
                ErrOut("%s failed on input file: %s\n",
                       CommandEntry.CommandName.c_str(),
                       InputFiles[i].Filename);
                Success = false;
                break;
            }

            GrannyClearArena(TempArena);
        }}
    }
    GrannyFreeMemoryArena(TempArena);

    return Success ? EXIT_SUCCESS : EXIT_FAILURE;
}

#endif // COMPILING_PREPROCESSOR


CommandRegistrar::CommandRegistrar(CommandMulti NewCommand,
                                   char const*  CommandName,
                                   char const*  ShortDocString,
                                   char const* DocString)
{
    pp_assert(FindCommandEntry(CommandName) == -1);

    command_entry CommandEntry;
    CommandEntry.CommandName = CommandName;
    CommandEntry.ShortDocString = ShortDocString;
    CommandEntry.DocString = DocString ? DocString : "";
    CommandEntry.CommandType = eMultiFile;
    CommandEntry.MultiFn = NewCommand;
    GlobalCommandList().push_back(CommandEntry);

    // Inefficient to sort this every time, but what the hey
    sort(GlobalCommandList().begin(), GlobalCommandList().end());
}

CommandRegistrar::CommandRegistrar(CommandEmpty NewCommand,
                                   char const*  CommandName,
                                   char const*  ShortDocString,
                                   char const* DocString)
{
    pp_assert(FindCommandEntry(CommandName) == -1);

    command_entry CommandEntry;
    CommandEntry.CommandName = CommandName;
    CommandEntry.ShortDocString = ShortDocString;
    CommandEntry.DocString = DocString ? DocString : "";
    CommandEntry.CommandType = eEmptyStart;
    CommandEntry.EmptyFn = NewCommand;
    GlobalCommandList().push_back(CommandEntry);

    // Inefficient to sort this every time, but what the hey
    sort(GlobalCommandList().begin(), GlobalCommandList().end());
}

CommandRegistrar::CommandRegistrar(ECommandType  CommandType,
                                   CommandSingle NewCommand,
                                   char const*   CommandName,
                                   char const*   ShortDocString,
                                   char const* DocString)
{
    pp_assert(CommandType == eSingleFile_NoMultiRun ||
              CommandType == eSingleFile_MultiRun);
    pp_assert(FindCommandEntry(CommandName) == -1);

    command_entry CommandEntry;
    CommandEntry.CommandName = CommandName;
    CommandEntry.ShortDocString = ShortDocString;
    CommandEntry.DocString = DocString ? DocString : "";
    CommandEntry.CommandType = CommandType;
    CommandEntry.SingleFn = NewCommand;
    GlobalCommandList().push_back(CommandEntry);

    // Inefficient to sort this every time, but what the hey
    sort(GlobalCommandList().begin(), GlobalCommandList().end());
}

CommandRegistrar::CommandRegistrar(CommandBatchable NewCommand,
                                   char const*      CommandName,
                                   char const*      ShortDocString,
                                   char const* DocString,
                                   bool PreservePlatform       /*= true*/,
                                   bool PreserveObjectSections /*= true*/,
                                   bool PreserveSectionFormats /*= true*/)
{
    pp_assert(FindCommandEntry(CommandName) == -1);

    command_entry CommandEntry;
    CommandEntry.CommandName    = CommandName;
    CommandEntry.ShortDocString = ShortDocString;
    CommandEntry.DocString = DocString ? DocString : "";
    CommandEntry.CommandType    = eBatchable;
    CommandEntry.BatchableFn    = NewCommand;
    CommandEntry.PreservePlatform       = PreservePlatform;
    CommandEntry.PreserveObjectSections = PreserveObjectSections;
    CommandEntry.PreserveSectionFormats = PreserveSectionFormats;
    GlobalCommandList().push_back(CommandEntry);

    // Inefficient to sort this every time, but what the hey
    sort(GlobalCommandList().begin(), GlobalCommandList().end());
}

