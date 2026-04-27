// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "preprocessor.h"

#include "platform_convert.h"

#include <stdio.h>
#include <vector>
#include <string>
#include <string.h>
using namespace std;


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


enum EFlags
{
    eOverrideCompress = 1 << 0,
    eOverridePlatform = 1 << 1
};

struct run_stage
{
    // Raw.
    string CommandLine;

    // Parsed
    char const*            CommandName;
    vector<key_value_pair> KeyValues;
};

struct run_specification
{
    vector<char> SpecBytes;
    vector<run_stage> Commands;

    granny_compression_type Compress;
    granny_grn_file_magic_value PlatformValue;

    granny_uint32 Flags;

    run_specification()
      : Compress(GrannyNoCompression),
        Flags(0) { }
};

static bool
ParseLine(char* Line, run_specification& Spec)
{
    vector<char*> Tokens;

    run_stage NewStage;
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

    if (Tokens.empty())
        return false;

    // First, let's extract the command, which is just token 0.
    NewStage.CommandName = Tokens[0];

    // Next, parse out the k/v pairs.
    if ((Tokens.size() & 1) == 0)
    {
        ErrOut("Unbalanced k/v pairs in line: %s\n", NewStage.CommandLine.c_str());
        return false;
    }

    {for (size_t Idx = 1; Idx < Tokens.size(); Idx += 2)
    {
        if (Tokens[Idx][0] != '-')
        {
            ErrOut("invalid k/v spec in line: %s\n", NewStage.CommandLine.c_str());
            return false;
        }

        // just for fun, pun this one
        key_value_pair KV = {
            Tokens[Idx] + 1,
            Tokens[Idx + 1]
        };
        NewStage.KeyValues.push_back(KV);
    }}

    // There are a few special cases we want to handle before storing this stage, however.
    // First Compress/Decompress may be entered in a batch, though they're not batchable
    // commands.  Detect and handle those.  Second PlatformConvert may also be present,
    // handle that as well.
    if ((_stricmp(NewStage.CommandName, "Compress") == 0) || 
        (_stricmp(NewStage.CommandName, "CompressBitKnit2") == 0))
    {
        Spec.Flags |= eOverrideCompress;
        Spec.Compress = GrannyBitKnit2Compression;
        return true;
    }
    else if (_stricmp(NewStage.CommandName, "CompressBitKnit") == 0)
    {
        Spec.Flags |= eOverrideCompress;
        Spec.Compress = GrannyBitKnitCompression;
        return true;
    }
    else if (_stricmp(NewStage.CommandName, "CompressOodle1") == 0)
    {
        Spec.Flags |= eOverrideCompress;
        Spec.Compress = GrannyOodle1Compression;
        return true;
    }
    else if (_stricmp(NewStage.CommandName, "Decompress") == 0)
    {
        Spec.Flags |= eOverrideCompress;
        Spec.Compress = GrannyNoCompression;
        return true;
    }
    else if (_stricmp(NewStage.CommandName, "PlatformConvert") == 0)
    {
        Spec.Flags |= eOverridePlatform;

        if (!GetPlatformMVFromKeyValues(&NewStage.KeyValues[0], int(NewStage.KeyValues.size()), Spec.PlatformValue))
        {
            return false;
        }

        return true;
    }

    // Otherwise, this is bona-fide, go ahead and add it to the list...
    Spec.Commands.push_back(NewStage);

    return true;
}

static bool
CreateRunSpec(char const*        Specfile,
              run_specification& Spec)
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

static bool
RunBatch(input_file&     InputFile,
         key_value_pair* KeyValues,
         granny_int32x   NumKeyValues,
         granny_memory_arena* TempArena)
{
    char const* Batch = FindFirstValueForKey(KeyValues, NumKeyValues, "batch");
    if (!Batch)
    {
        ErrOut("Must specify a batch file with -batch\n");
        return false;
    }

    run_specification RunSpec;
    if (!CreateRunSpec(Batch, RunSpec))
    {
        ErrOut("Unable to create run specification from: %s\n", Batch);
        return false;
    }

    // Get the file info
    granny_file_info* Info = ExtractFileInfo(InputFile);

    // Attach the history, if we're rolling that way.  Note that this is the RunBatch
    // command line, we'll attach the batch history separately
    AttachHistoryEntry(Info, TempArena);

    // Compute the output filename
    char NameBuffer[512];
    {
        strcpy(NameBuffer, InputFile.Filename);

        char const* PostFix = FindFirstValueForKey(KeyValues, NumKeyValues, "postfix");
        if (PostFix)
        {
            char* dot = strrchr(NameBuffer, '.');
            if (!dot)
                return false;

            char extbuffer[512];
            strcpy(extbuffer, dot+1);
            sprintf(dot, ".%s.%s", PostFix, extbuffer);
        }
    }

    {for (size_t Idx = 0; Idx < RunSpec.Commands.size(); ++Idx)
    {
        run_stage& Stage = RunSpec.Commands[Idx];

        int CommandIndex = FindCommandEntry(Stage.CommandName);
        if (CommandIndex == -1)
        {
            ErrOut("Misunderstood command: %s.  Use \"preprocessor ListCommands\"\n"
                   " to obtain a list of valid command strings\n",
                   Stage.CommandName);
            return false;
        }

        command_entry& CommandEntry = GlobalCommandList()[CommandIndex];
        if (CommandEntry.CommandType != eBatchable)
        {
            ErrOut("%s is not a batchable command\n", Stage.CommandName);
            return false;
        }

        key_value_pair* KVs = Stage.KeyValues.empty() ? 0 : &Stage.KeyValues[0];

        granny_file_info* NextInfo =
            (*CommandEntry.BatchableFn)(InputFile.Filename, NameBuffer,
                                        Info, KVs, int(Stage.KeyValues.size()), TempArena);

        if (NextInfo == 0)
        {
            return false;
        }

        Info = NextInfo;

        string CustomEntry = "  " + Stage.CommandLine;
        AttachHistoryEntry(Info, TempArena, &CustomEntry);
    }}

    // Attach history entries for the compression and the platform conversion, if they
    // were present...
    {
        if (RunSpec.Flags & eOverrideCompress)
        {
            string CompressEntry = "  ";
            switch( RunSpec.Compress )
            {
            case GrannyNoCompression:
                CompressEntry += "Decompress";
                break;
            case GrannyOodle1Compression:
                CompressEntry += "CompressOodle1";
                break;
            case GrannyBitKnitCompression:
                CompressEntry += "CompressBitKnit";
                break;
            case GrannyBitKnit2Compression:
                CompressEntry += "CompressBitKnit2";
                break;
            default:
                CompressEntry += "Decompress";
                break;
            }

            AttachHistoryEntry(Info, TempArena, &CompressEntry);
        }

        if (RunSpec.Flags & eOverridePlatform)
        {
            granny_int32x PointerSize;
            bool LittleEndian;
            if (GrannyGetMagicValueProperties(&RunSpec.PlatformValue, &PointerSize, &LittleEndian))
            {
                string PlatformEntry = "  PlatformConvert ";
                if (PointerSize == 32)
                    PlatformEntry += "-pointer 32 ";
                else
                    PlatformEntry += "-pointer 64 ";

                if (LittleEndian)
                    PlatformEntry += "-endian little";
                else
                    PlatformEntry += "-endian big";

                AttachHistoryEntry(Info, TempArena, &PlatformEntry);
            }
        }
    }

    // Gotta do this manually to deal with the overrides
    {
        granny_int32x FinalSectionCount = InputFile.GrannyFile->SectionCount;
        if (FinalSectionCount < GrannyStandardSectionCount)
            FinalSectionCount = GrannyStandardSectionCount;

        granny_grn_file_magic_value const* PlatformMV = InputFile.GrannyFile->SourceMagicValue;
        if (RunSpec.Flags & eOverridePlatform)
        {
            PlatformMV = &RunSpec.PlatformValue;
        }

        granny_file_builder *Builder =
            GrannyBeginFile(FinalSectionCount, GrannyCurrentGRNStandardTag,
                            PlatformMV, GrannyGetTemporaryDirectory(), "Prefix");

        granny_file_data_tree_writer *Writer = CreateTreeWriter(NameBuffer, Info);
        pp_assert(Writer);

        GrannyPreserveObjectFileSections(Writer, InputFile.GrannyFile);

        if (RunSpec.Flags & eOverrideCompress)
        {
            {for (int SectionIdx = 0; SectionIdx < FinalSectionCount; ++SectionIdx)
            {
                granny_grn_section *SectionArray =
                    GrannyGetGRNSectionArray(InputFile.GrannyFile->Header);

                GrannySetFileSectionFormat(Builder,
                                           SectionIdx,
                                           RunSpec.Compress,

                                           (SectionIdx < InputFile.GrannyFile->SectionCount ?
                                            SectionArray[SectionIdx].InternalAlignment : 4));
            }}
        }
        else
        {
            GrannyPreserveFileSectionFormats(Builder, InputFile.GrannyFile);
        }

        GrannyWriteDataTreeToFileBuilder(Writer, Builder);
        GrannyEndFileDataTreeWriting(Writer);

        bool success = GrannyEndFile(Builder, NameBuffer);
        if (!success)
        {
            fprintf(stderr, "Failed to write granny_file_info to %s\n", NameBuffer);
        }

        return success;
    }

    // Can't get here
    // return true;
}

static const char* HelpString =
    (" The preprocessor can batch together related commands in an\n"
     " internal script if you'd like to avoid writing intermediate files\n"
     " to disk. This allows you to prototype using the command line and\n"
     " batch files, and wrap everything up into a single command when\n"
     " you're done.\n"
     "\n"
     " Not every command can be part of a batch file, however. As a\n"
     " general rule, a batchable command must operate on\n"
     " granny_file_info objects, and may only take one input, and\n"
     " produce one output. So for instance, 'BonesPerMesh', which takes\n"
     " in one .gr2, and produces one .gr2 as an output is batchable,\n"
     " but 'LODCalc', which takes 2 .gr2 inputs (model + animation), is\n"
     " not. Likewise, 'MakeSPUAnimation', which operates on\n"
     " granny_spu_animation_info objects is not batchable. You can find\n"
     " out which commands support batching by using\n"
     "\n"
     "     preprocessor ListCommands\n"
     "\n"
     " If a '*' follows the command name, it can be part of a batch.\n"
     "\n"
     " There are a few special commands that are interpreted\n"
     " specially. 'Compress' (and likewise 'CompressOodle1', 'CompressBitKnit',"
     " and 'CompressBitKnit2'), 'Decompress', and 'PlatformConvert' look\n"
     " exactly like their stand-alone version, but the batching engine\n"
     " detects these, and uses them to control the output of the final\n"
     " stage, rather than running them explicitly. So you may place them\n"
     " anywhere in the script, but for clarity, it's probably best to\n"
     " put them at the end. If these commands are missing, the\n"
     " compression and platform settings will be the same as the\n"
     " original input file.\n"
     "\n"
     " Let's look at an example script! We'd like to create a batch that\n"
     " will create a model optimized for deforming on a low-spec DirectX\n"
     " 9 GPU. We'll imagine that we have constant register space for 40\n"
     " bones, and that we'd like to cache optimize the meshes after\n"
     " breaking them into managable chunks. Our script might look like\n"
     " this:\n"
     "\n"
     "     // Batches support C++-style comments\n"
     "     BonesPerMesh -bonelimit 40\n"
     "     CleanMaterials\n"
     "     VertexCacheOptimize\n"
     "\n"
     "     CompressBitKnit2\n"
     "     PlatformConvert -pointer 32 -endian big\n"
     "\n"
     "\n"
     " Note that the only thing missing for the commands is\n"
     " the '-output' flag, which is handled internally by the batching\n"
     " engine. By default, the output of the batch will be the same as\n"
     " the input file, but we can control the output filename with\n"
     " the '-postfix' argument to the RunBatch command. So we might\n"
     " create platform-optimized versions of our meshes with:\n"
     "\n"
     "    preprocessor RunBatch -batch dx9.ppb -postfix dx9 Mesh0.gr2 Mesh1.gr2\n"
     "    preprocessor RunBatch -batch 360.ppb -postfix 360 Mesh0.gr2 Mesh1.gr2\n"
     "    preprocessor RunBatch -batch ps3.ppb -postfix ps3 Mesh0.gr2 Mesh1.gr2\n");

static CommandRegistrar RegRunBatch(eSingleFile_MultiRun,
                                    RunBatch, "RunBatch",
                                    "Runs a batch of preprocessor commands (specify with -batch <file>)",
                                    HelpString);

