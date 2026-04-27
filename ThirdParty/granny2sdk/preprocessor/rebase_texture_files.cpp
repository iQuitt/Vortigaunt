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
#include <string.h>
#include <ctype.h>


static char const*
RebasePath(char const* BasePath,
           char const* TexPath)
{
    size_t const BasePathLen = strlen(BasePath);
    if (_strnicmp(BasePath, TexPath, BasePathLen) == 0)
    {
        // Can rebase this one, but account for any path separator at the beginning, since
        // the basepath might not have included it...
        char const* RetPath = TexPath + BasePathLen;
        while (*RetPath == '\\' || *RetPath == '/')
            ++RetPath;

        return RetPath;
    }
    else
    {
        // Path doesn't match, return unchanged...
        return TexPath;
    }
}


granny_file_info*
RebaseTextureFiles(char const*          OriginalFilename,
                   char const*          OutputFilename,
                   granny_file_info*    Info,
                   key_value_pair*      KeyValues,
                   granny_int32x        NumKeyValues,
                   granny_memory_arena* TempArena)
{
    char const* BasePath = FindFirstValueForKey(KeyValues, NumKeyValues, "basepath");
    if (BasePath == 0)
    {
        ErrOut("must specify an base path for rebasing with \"-basepath path\"\n");
        return false;
    }

    {for(granny_int32x TexIdx = 0; TexIdx < Info->TextureCount; ++TexIdx)
    {
        if (!Info->Textures[TexIdx])
            continue;
        Info->Textures[TexIdx]->FromFileName =
            RebasePath(BasePath, Info->Textures[TexIdx]->FromFileName);
    }}

    return Info;
}

static const char* HelpString =
    (" RebaseTextureFiles is intended to change the filestrings reported\n"
     " for textures from absolute paths (used by Max and Maya), to paths\n"
     " relative to the root of your game data directory, the character\n"
     " file itself, or any other root you want to specify.\n"
     "\n"
     "    preprocessor RebaseTextureFiles file.gr2 -output file_relative.gr2 \\\n"
     "        -basepath c:/game/data/root\n");
     

static CommandRegistrar RegRebaseTextureFiles(RebaseTextureFiles,
                                              "RebaseTextureFiles",
                                              "Makes any absolute textures relative to a basepath, if possible",
                                              HelpString);
