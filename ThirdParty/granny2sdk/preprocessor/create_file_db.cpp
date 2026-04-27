// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "preprocessor.h"
#include "granny_file_database.h"

#include <set>
#include <string>
#include <algorithm>
#include <string.h>

using namespace std;

// Copied from the run_batch command, Ctrl-c/Ctrl-v, oh yeah.
struct file_stage
{
    // Raw.
    string CommandLine;

    // Parsed
    char const* Typename;  // must be gr2 or gsf
    char const* Filename;  // absolute, or relative to run directory of the preprocessor
};

struct file_specification
{
    vector<char> SpecBytes;
    vector<file_stage> Commands;

    file_specification() { }
};

static bool CreateFileSpec(char const* Specfile, file_specification& Spec);


bool
CreateFileDB(key_value_pair* KeyValues,
             granny_int32x   NumKeyValues,
             granny_memory_arena* TempArena)
{
    char const* OutputName = FindFirstValueForKey(KeyValues, NumKeyValues, "output");
    if (OutputName == 0)
    {
        ErrOut("must specify a database with \"-output <filename>\"\n");
        return false;
    }

    char const* FileList = FindFirstValueForKey(KeyValues, NumKeyValues, "filelist");
    if (FileList == 0)
    {
        ErrOut("must specify a filelist with \"-filelist <filename>\"\n");
        ErrOut("   should have the format:\n");
        ErrOut("   gr2 filename.gr2\n");
        ErrOut("   gsf filename.gsf\n");
        return false;
    }

    vector<granny_file*> FilesToFree;
    vector<granny_filedb_gr2_entry> GR2Files;
    vector<granny_filedb_gsf_entry> GSFFiles;

    // Copy paste what?
    file_specification FileSpec;
    if (!CreateFileSpec(FileList, FileSpec))
    {
        ErrOut("Unable to create run specification from: %s\n", FileList);
        return false;
    }


    {for (size_t Idx = 0; Idx < FileSpec.Commands.size(); ++Idx)
    {
        file_stage& Command = FileSpec.Commands[Idx];
        granny_file* TheFile = GrannyReadEntireFile(Command.Filename);
        if (TheFile == 0)
        {
            ErrOut("Unable to read: \"%s\"\n", Command.Filename);
            return false;
        }
        FilesToFree.push_back(TheFile);

        if (_stricmp(Command.Typename, "gr2") == 0)
        {
            granny_filedb_gr2_entry NewFile;
            NewFile.Filename = Command.Filename;
            NewFile.FileInfo = GrannyGetFileInfo(TheFile);
            if (NewFile.FileInfo == 0)
            {
                ErrOut("Unable to get file_info from: \"%s\"\n", Command.Filename);
                return false;
            }
            GR2Files.push_back(NewFile);
        }
        else if (_stricmp(Command.Typename, "gsf") == 0)
        {
            granny_filedb_gsf_entry NewFile;
            NewFile.Filename = Command.Filename;
            NewFile.CharacterInfo = GStateGetCharacterInfo(TheFile);
            if (NewFile.CharacterInfo == 0)
            {
                ErrOut("Unable to get character_info from: \"%s\"\n", Command.Filename);
                return false;
            }
            GSFFiles.push_back(NewFile);
        }
        else
        {
            ErrOut("Encountered bad line in filespec: \"%s\"\n", Command.CommandLine.c_str());
            return false;
        }
    }}

    granny_file_database TheDatabase;
    {
        TheDatabase.GR2FileCount = int(GR2Files.size());
        if (TheDatabase.GR2FileCount)
            TheDatabase.GR2Files = &GR2Files[0];
        else
            TheDatabase.GR2Files = 0;

        TheDatabase.GSFFileCount = int(GSFFiles.size());
        if (TheDatabase.GSFFileCount)
            TheDatabase.GSFFiles = &GSFFiles[0];
        else
            TheDatabase.GSFFiles = 0;
    }


    // Write out the resulting database...
    granny_file_builder *Builder = GrannyBeginFile(GrannyStandardSectionCount,
                                                   GrannyCurrentFDBTag,
                                                   GrannyGRNFileMV_ThisPlatform,
                                                   GrannyGetTemporaryDirectory(),
                                                   "Prefix");
    granny_file_data_tree_writer *Writer =
        GrannyBeginFileDataTreeWriting(GrannyFileDatabaseType, &TheDatabase, GrannyStandardDiscardableSection, 0);
    GrannyWriteDataTreeToFileBuilder(Writer, Builder);
    GrannyEndFileDataTreeWriting(Writer);

    bool success = GrannyEndFile(Builder, OutputName);
    if (!success)
    {
        ErrOut("failed to write database to %s\n", OutputName);
    }

    {for (size_t Idx = 0; Idx < FilesToFree.size(); ++Idx)
    {
        GrannyFreeFile(FilesToFree[Idx]);
        FilesToFree[Idx] = 0;
    }}

    return success;
}

static char const* HelpString =
    (" CreateFileDB combines multiple GR2 and GSF files into a single file\n"
     " that loads efficiently, and shares common data across a data set.\n"
     " See the Granny blog 'Make Like Voltron!' for more information, and\n"
     " some motivation\n"
     "\n"
     " The command takes in a simple script that specifies the files to be\n"
     " combined, and their underlying type.  Your script should look like:\n"
     "\n"
     "    // Scripts can contain comments\n"
     "    GR2 models/characters/char_0.gr2\n"
     "    GR2 models/characters/char_1.gr2\n"
     "    GR2 models/characters/char_2.gr2\n"
     "    GR2 models/characters/char_3.gr2\n"
     "    GR2 models/characters/char_4.gr2\n"
     "\n"
     "    // GSF indicates a state machine, rather than a granny_file_info\n"
     "    GSF characters/BaseStateMachine.gsf\n"
     "\n"
     " You create the database with:\n"
     "\n"
     "    preprocessor -filespec thing.spc -output file.fdb\n"
     "\n"
     " Which creates a granny_file_database file object.  Use the\n"
     " granny_file_database.c/h source contained in the preprocessor\n"
     " project directory to load these objects in your game\n");
     

static CommandRegistrar RegCreateFileDB(CreateFileDB, "CreateFileDB",
                                        "Creates a database (-output) with the input files from -filespec",
                                        HelpString);







// Casey style!
static void
EatWhitespace(char*& Pointer)
{
    // Don't eat \n
    while (*Pointer && (*Pointer == ' ' ||
                        *Pointer == '\t' ||
                        *Pointer == '\r'))
    {
        ++Pointer;
    }
}

static void
EatNonWhitespace(char*& Pointer)
{
    while (*Pointer && !(*Pointer == ' ' ||
                         *Pointer == '\t' ||
                         *Pointer == '\r'))
    {
        ++Pointer;
    }
}

static bool
EatNonQuote(char*& Pointer)
{
    while (*Pointer && (*Pointer != '"'))
    {
        ++Pointer;
    }

    return *Pointer == '"';
}


static void
EatToEOL(char*& Pointer)
{
    // Don't eat \n
    while (*Pointer && *Pointer != '\n')
        ++Pointer;
}


static bool
ParseLine(char* Line, file_specification& Spec)
{
    vector<char*> Tokens;

    file_stage NewStage;
    NewStage.CommandLine = Line;

    char* Pos = Line;
    for (;;)
    {
        EatWhitespace(Pos);

        // Hit the end of the line
        if (*Pos == '\0')
            break;

        char* TokEnd = Pos;
        if (*Pos == '"')
        {
            // Quoted argument
            Pos = TokEnd = Pos + 1;
            if (EatNonQuote(TokEnd) == false)
            {
                return false;
            }
        }
        else
        {
            // Normal argument
            EatNonWhitespace(TokEnd);
        }


        if (Pos != TokEnd)
            Tokens.push_back(Pos);

        if (*TokEnd)
        {
            *TokEnd = 0;
            Pos = TokEnd+1;
        }
        else
        {
            break;
        }
    }

    if (Tokens.empty() || Tokens.size() != 2)
        return false;

    // First, let's extract the command, which is just token 0.
    NewStage.Typename = Tokens[0];

    // and the filename, just token 1.  Much simpler than the run_batch parser...
    NewStage.Filename = Tokens[1];

    // Otherwise, this is bona-fide, go ahead and add it to the list...
    Spec.Commands.push_back(NewStage);

    return true;
}

static bool
CreateFileSpec(char const*         Specfile,
               file_specification& Spec)
{
    // Slurp in the file
    {
        FILE* f = fopen(Specfile, "rb");
        if (!f)
            return false;

        fseek(f, 0, SEEK_END);
        int NumBytes = ftell(f);
        fseek(f, 0, SEEK_SET);

        Spec.SpecBytes.resize(NumBytes + 2);
        fread(&Spec.SpecBytes[0], 1, NumBytes, f);

        // Double null at the end makes the below loop easier...
        Spec.SpecBytes[NumBytes] = 0;
        Spec.SpecBytes[NumBytes+1] = 0;
        fclose(f);
    }

    // Split into lines
    vector<char*> Lines;
    {
        char* StartPos = &Spec.SpecBytes[0];

        while (*StartPos != 0)
        {
            char* LineBegin = StartPos;
            EatWhitespace(LineBegin);

            char* LineEnd = LineBegin;
            EatToEOL(LineEnd);

            *LineEnd = 0;
            if (LineBegin != LineEnd)
            {
                // Valid line, make sure it's not a comment before adding it...
                if (!(strlen(LineBegin) >= 2 && (LineBegin[0] == '/' &&
                                                 LineBegin[1] == '/')))
                    Lines.push_back(LineBegin);
            }

            // Double guard pays off here.
            StartPos = LineEnd + 1;
        }
    }

    {for (int Idx = 0; Idx < (int)Lines.size(); ++Idx)
    {
        std::string StoreLine = Lines[Idx];
        if (ParseLine(Lines[Idx], Spec) == false)
        {
            ErrOut("Parsing error in line %d:\n  %s\n", Idx, StoreLine.c_str());
            return false;
        }
    }}

    return true;
}
