#if !defined(PREPROCESSOR_H)
// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "granny.h"
#include "gstate.h"
#include <stdio.h>
#include <vector>
#include <string>

#if _MSC_VER
  #pragma warning(disable:4100)  // unreferenced formal parameter
  #pragma warning(disable:4702)  // unreachable code in <xtree>. I hate turning this off, btw.
  #pragma warning(disable:4366)  // The result of the unary 'operator' operator may be unaligned
#endif

// Assertion
#ifndef NDEBUG
#include <assert.h>
#define pp_assert(x) assert(x)
#else
#define pp_assert(x) (void)sizeof(x)
#endif

#define pp_unused(x) x = x

// ------------------------------------------------------------
// Structures to hold arguments to the commands
struct input_file
{
    granny_file* GrannyFile;
    char*        Filename;   // as passed to the command line, may
                             // contain path elements
};

struct key_value_pair
{
    char const* Key;
    char const* Value;
};

// ------------------------------------------------------------
// Function signature for processing functions
enum ECommandType
{
    eMultiFile,
    eSingleFile_NoMultiRun,
    eSingleFile_MultiRun,
    eEmptyStart,
    eBatchable,
};

typedef bool (*CommandMulti)(input_file*     InputFiles,
                             granny_int32x   NumInputFiles,
                             key_value_pair* KeyValuePairs,
                             granny_int32x   NumKeyValuePairs,
                             granny_memory_arena* TempArena);

typedef bool (*CommandSingle)(input_file&     InputFile,
                              key_value_pair* KeyValuePairs,
                              granny_int32x   NumKeyValuePairs,
                              granny_memory_arena* TempArena);

typedef granny_file_info* (*CommandBatchable)(char const*          OriginalFilename,
                                              char const*          OutputFilename,
                                              granny_file_info*    InputInfo,
                                              key_value_pair*      KeyValuePairs,
                                              granny_int32x        NumKeyValuePairs,
                                              granny_memory_arena* TempArena);

typedef bool (*CommandEmpty)(key_value_pair* KeyValuePairs,
                             granny_int32x   NumKeyValuePairs,
                             granny_memory_arena* TempArena);

// Error output helpers
#if _MSC_VER
  #define ErrOut(fmt, ...) fprintf(stderr, __FUNCTION__": " fmt,##__VA_ARGS__)
  #define ErrOutLie(fnname, fmt, ...) fprintf(stderr, "%s", fnname); fprintf(stderr, ": " fmt,##__VA_ARGS__)
  #define WarnOut(fmt, ...) fprintf(stdout, "__FUNCTION__ (warning): " fmt,##__VA_ARGS__)
#else
  #define ErrOut(fmt, ...) fprintf(stderr, fmt,##__VA_ARGS__)
  #define ErrOutLie(fnname, fmt, ...) fprintf(stderr, "%s", fnname); fprintf(stderr, ": " fmt,##__VA_ARGS__)
  #define WarnOut(fmt, ...) fprintf(stdout, "(warning): " fmt,##__VA_ARGS__)
#endif

// ------------------------------------------------------------
// Rename these to prevent collisions when compiling the preprocessor into the unit
// testing framework.
//
#define PushObject(Arena, Type)       ((Type*)GrannyMemoryArenaPush(Arena, sizeof(Type)))
#define PushArray(Arena, Count, Type) ((Type*)GrannyMemoryArenaPush(Arena, sizeof(Type) * (Count)))
#define PushString(Arena, String)     GrannyMemoryArenaPushString(Arena, String)


// ------------------------------------------------------------
// Helper structure to register a preprocessor command.  Use as:
//  static CommandRegistrar RegMyCommand(eSingleFile_MultiRun,  /* or eMultiFile */
//                                       MyCommand,
//                                       "CommandString",
//                                       "ShortDocString",
//                                       "Help String");
// at /file/ level, where MyCommand is a pointer to your function.
class CommandRegistrar
{
public:

public:
    CommandRegistrar(CommandMulti NewCommand,
                     char const*  CommandName,
                     char const*  ShortDocString,
                     char const*  DocString = 0);
    CommandRegistrar(CommandEmpty NewCommand,
                     char const*  CommandName,
                     char const*  ShortDocString,
                     char const*  DocString = 0);
    CommandRegistrar(ECommandType  CommandType,
                     CommandSingle NewCommand,
                     char const*   CommandName,
                     char const*   ShortDocString,
                     char const*   DocString = 0);
    CommandRegistrar(CommandBatchable NewCommand,
                     char const*      CommandName,
                     char const*      ShortDocString,
                     char const*      DocString = 0,
                     bool PreservePlatform       = true,
                     bool PreserveObjectSections = true,
                     bool PreserveSectionFormats = true);

    // disallow standard c++ fns
private:
    CommandRegistrar();
    CommandRegistrar(CommandRegistrar const&);
    void operator=(CommandRegistrar const&);
};


// The current table of commands.  These are created by the CommandRegistrar helper class.
struct command_entry
{
    std::string  CommandName;
    std::string  ShortDocString;
    std::string  DocString;
    ECommandType CommandType;

    // For batchable commands, these are settable.
    bool PreservePlatform;
    bool PreserveObjectSections;
    bool PreserveSectionFormats;

    union
    {
        CommandMulti     MultiFn;
        CommandSingle    SingleFn;
        CommandEmpty     EmptyFn;
        CommandBatchable BatchableFn;
    };

    // for sorting
    bool operator<(command_entry const& Other) const;
    command_entry();
};

int FindCommandEntry(char const* CommandName);
std::vector<command_entry>& GlobalCommandList();

extern std::string g_CommandLine;


#define RequireKey(keyname,...)						\
  {									\
    if (!FindFirstValueForKey(KeyValues, NumKeyValues, keyname))	\
      {									\
	ErrOut(__VA_ARGS__);						\
	return false;							\
      }									\
  } typedef int __require_semicolon


// Necessary to support mods
granny_file_info* ExtractFileInfo(input_file& InputFiles);

granny_file_data_tree_writer* CreateTreeWriter(char const*       OutputFile,
                                               granny_file_info* Info);
granny_file_data_tree_writer* CreateGenericTreeWriter(char const*       OutputFile,
                                                      granny_data_type_definition* Type,
                                                      void* Root);

bool SimpleWriteCharacterInfo(key_value_pair* KeyValuePairs,
                              granny_int32x   NumKeyValuePairs,
                              gstate_character_info* Info,
                              granny_memory_arena* TempArena);
bool SimpleWriteInfo(key_value_pair* KeyValuePairs,
                     granny_int32x   NumKeyValuePairs,
                     granny_file_info* Info,
                     granny_memory_arena* TempArena);
bool WriteInfoPreserve(key_value_pair* KeyValuePairs,
                       granny_int32x   NumKeyValuePairs,
                       granny_file_info* Info,
                       granny_file* OriginalFile,
                       bool PreservePlatform,
                       bool PreserveObjectSections,
                       bool PreserveSectionFormats,
                       granny_memory_arena* TempArena);

// Only operates if the -history flag was scanned...
void AttachHistoryEntry(granny_file_info*    Info,
                        granny_memory_arena* TempArena,
                        std::string*         CustomEntry = 0);


// Utility function for searching K/V pairs.  Returns NULL for missing
// key/value
char const* FindFirstValueForKey(key_value_pair* KeyValuePairs,
                                 granny_int32x   NumKeyValuePairs,
                                 char const*     Key);




// Macosx has strcasecmp rather than stricmp
#if defined(_MACOSX) || defined(linux)
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#endif


#define PREPROCESSOR_H
#endif
