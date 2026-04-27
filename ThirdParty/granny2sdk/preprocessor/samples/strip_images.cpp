// ========================================================================
// $File: //jeffr/granny/preprocessor/samples/strip_images.cpp $
// $DateTime: 2012/07/09 09:41:34 $
// $Change: 38182 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"
#include <stdio.h>
#include <string.h>
#include <vector>

using namespace std;

granny_file_info*
StripImages(char const*          OriginalFilename,
            char const*          OutputFilename,
            granny_file_info*    Info,
            key_value_pair*      KeyValues,
            granny_int32x        NumKeyValues,
            granny_memory_arena* TempArena)
{
    // Loop through the enumerated textures...
    {for(int TexIdx = 0; TexIdx < Info->TextureCount; ++TexIdx)
    {
        granny_texture* Texture = Info->Textures[TexIdx];
        if (!Texture)
            continue;

        Texture->ImageCount = 0;
        Texture->Images = 0;
    }}

    // Whip through the materials, just in case there's a texture that isn't listed in the
    // top level array...
    {for(int MatIdx = 0; MatIdx < Info->MaterialCount; ++MatIdx)
    {
        granny_material* Material = Info->Materials[MatIdx];
        if (!Material || !Material->Texture)
            continue;

        Material->Texture->ImageCount = 0;
        Material->Texture->Images = 0;
    }}

    return Info;
}

static CommandRegistrar RegStripImages(StripImages, "StripImages",
                                       "Remove all pixel data from textures, leaving the file reference only.");

