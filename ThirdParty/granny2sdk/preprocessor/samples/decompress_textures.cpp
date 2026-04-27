// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"
#include <stdio.h>
#include <string.h>
#include <vector>

using namespace std;

bool Decompress(granny_texture*      Texture,
                granny_memory_arena* Arena)
{
    granny_pixel_layout Layout;
    if(GrannyGetRecommendedPixelLayout(Texture, &Layout))
    {
        {for (int ImageIdx = 0; ImageIdx < Texture->ImageCount; ++ImageIdx)
        {
            granny_texture_image& Image = Texture->Images[ImageIdx];

            {for (int MIPIdx = 0; MIPIdx < Image.MIPLevelCount; ++MIPIdx)
            {
                granny_texture_mip_level& MIPLevel = Image.MIPLevels[MIPIdx];

                granny_int32x MipWidth  = (Texture->Width  >> MIPIdx) != 0 ? (Texture->Width  >> MIPIdx) : 1;
                granny_int32x MipHeight = (Texture->Height >> MIPIdx) != 0 ? (Texture->Height >> MIPIdx) : 1;
                granny_int32x MipSize   = GrannyGetRawImageSize(&Layout, MipWidth*Layout.BytesPerPixel,MipWidth,MipHeight);

                granny_uint8* NewMip = PushArray(Arena, MipSize, granny_uint8);
                if (!GrannyCopyTextureImage(Texture, ImageIdx, MIPIdx,
                                            &Layout,
                                            MipWidth, MipHeight, MipWidth*Layout.BytesPerPixel,
                                            NewMip))
                {
                    ErrOut("Unable to copy Image: %d Mip: %d from Texture: %s\n",
                           ImageIdx, MIPIdx, Texture->FromFileName);
                    return false;
                }

                MIPLevel.Stride = MipWidth * Layout.BytesPerPixel;
                MIPLevel.PixelByteCount = MipSize;
                MIPLevel.PixelBytes = NewMip;
            }}
        }}

        // Markup the texture...
        Texture->Encoding = GrannyRawTextureEncoding;
        Texture->SubFormat = 0;
        memcpy(&Texture->Layout, &Layout, sizeof(Layout));

        return true;
    }
    else
    {
        ErrOut("Unable to obtain texture format for texture: %s\n", Texture->FromFileName);
        return false;
    }
}


granny_file_info*
DecompressTextures(char const*          OriginalFilename,
                   char const*          OutputFilename,
                   granny_file_info*    Info,
                   key_value_pair*      KeyValues,
                   granny_int32x        NumKeyValues,
                   granny_memory_arena* TempArena)
{
    {for(int TexIdx = 0; TexIdx < Info->TextureCount; ++TexIdx)
    {
        granny_texture* Texture = Info->Textures[TexIdx];
        if (!Texture)
            continue;

        Decompress(Texture, TempArena);
    }}

    return Info;
}

static CommandRegistrar RegRecompressAnims(DecompressTextures, "DecompressTextures",
                                           "Convert all textures to raw RGB formats");

