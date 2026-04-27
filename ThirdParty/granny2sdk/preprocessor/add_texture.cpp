// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "add_texture.h"
#include "utilities.h"

#include "stb_image.h"

#include <stdlib.h>
#include <map>
#include <string>
#include <vector>

using namespace std;

static granny_texture_builder*
GetTextureBuilder(key_value_pair* KeyValues,
                  granny_int32x NumKeyValues,
                  int Width, int Height)
{
    char const* Format = FindFirstValueForKey(KeyValues, NumKeyValues, "format");
    if (!Format)
        Format = "Bink";

    int CompLevel = 8;
    {
        char const* Comp = FindFirstValueForKey(KeyValues, NumKeyValues, "compression");
        if (Comp)
            CompLevel = atoi(Comp);
    }

    if (_stricmp(Format, "Bink") == 0)
        return GrannyBeginBinkTexture(Width, Height, CompLevel, GrannyBinkUseBink1);
    else if (_stricmp(Format, "BinkAlpha") == 0)
        return GrannyBeginBinkTexture(Width, Height, CompLevel, GrannyBinkUseBink1|GrannyBinkEncodeAlpha);
    else if (_stricmp(Format, "S3TC") == 0)
        return GrannyBeginBestMatchS3TCTexture(Width, Height);
    else if (_stricmp(Format, "RawRGB") == 0)
        return GrannyBeginRawTexture(Width, Height, GrannyRGB888PixelFormat, 1);
    else if (_stricmp(Format, "RawRGBA") == 0)
        return GrannyBeginRawTexture(Width, Height, GrannyRGBA8888PixelFormat, 1);

    return 0;
}


bool
AddTexture(input_file*,
           granny_int32x,
           key_value_pair* KeyValues,
           granny_int32x NumKeyValues,
           granny_memory_arena* TempArena)
{
    RequireKey("database", "must specify a database with '-database <filename>'\n");
    char const* DatabaseName = FindFirstValueForKey(KeyValues, NumKeyValues, "database");

    granny_file_info Dummy;
    memset(&Dummy, 0, sizeof(Dummy));

    granny_file*      File = GrannyReadEntireFile(DatabaseName);
    granny_file_info* Info = 0;
    {
        if (File) Info = GrannyGetFileInfo(File);
        else      Info = &Dummy;
    }

    typedef map<char const*, granny_texture*, CIStrcmp> texture_map_t;
    texture_map_t TexMap;
    for (int Idx = 0; Idx < Info->TextureCount; ++Idx)
    {
        TexMap[Info->Textures[Idx]->FromFileName] = Info->Textures[Idx];
    }

    {for(int Idx = 0; Idx < NumKeyValues; ++Idx)
    {
        char const* Filename = KeyValues[Idx].Value;
        int x, y, comp;
        stbi_uc* ImageBytes = stbi_load(Filename, &x, &y, &comp, 4);
        if (!ImageBytes)
            continue;

        granny_texture_builder* Builder = GetTextureBuilder(KeyValues, NumKeyValues, x, y);
        if (!Builder)
        {
            stbi_image_free(ImageBytes);
            continue;
        }

        GrannyEncodeImage(Builder, x, y, x * 4, 1, ImageBytes);
        granny_texture *ResultingTexture =
            GrannyEndTextureInPlace(Builder,
                                    PushArray(TempArena, GrannyGetResultingTextureSize(Builder),
                                              granny_uint8));
        ResultingTexture->FromFileName = Filename;
        TexMap[Filename] = ResultingTexture;
    }}

    if (TexMap.empty())
    {
        ErrOut("no textures found in db or in added list\n");
        return false;
    }

    int NumNewTextures = (int)TexMap.size();
    granny_texture** NewTextures = PushArray(TempArena, NumNewTextures, granny_texture*);
    {
        int Idx = 0;
        for (texture_map_t::iterator Itr = TexMap.begin();
             Itr != TexMap.end(); ++Itr)
        {
            NewTextures[Idx++] = Itr->second;
        }
        pp_assert(Idx == NumNewTextures);
    }

    Info->TextureCount = NumNewTextures;
    Info->Textures = NewTextures;

    // Write out the resulting file.  Note that we have to fake the options, since
    // WriteInfoPreserve wants -database, not -output...
    key_value_pair Pair = { "output", (char*)DatabaseName };
    return WriteInfoPreserve(&Pair, 1, Info, File, true, true, true, TempArena);
}

static char const* HelpString =
    (" AddTexture manipulates a GR2 file that represents a collection of\n"
     " textures.  The idea is that for textures can be collected into one\n"
     " file for simple, efficient loading.\n"
     "\n"
     " Note that the first time you specify a file with '-database', the\n"
     " preprocessor will create an empty database.  AddTexture supports any\n"
     " format loaded by stb_image.\n"
     " \n"
     "    preprocessor AddTexture -database textures.gr2 -add tex0.bmp\n"
     "    preprocessor AddTexture -database textures.gr2 -add ..\\tex1.bmp\n"
     "    preprocessor AddTexture -database textures.gr2 -add tex2.bmp\n"
     "    preprocessor AddTexture -database textures.gr2 -add tex5.bmp -add tex6.bmp\n"
     );

// Register the commands
static CommandRegistrar RegAddTexture(AddTexture, "AddTexture",
                                      "Adds a texture to the file specified with '-database'",
                                      HelpString);

