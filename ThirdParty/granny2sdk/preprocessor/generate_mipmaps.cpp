// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "preprocessor.h"

granny_file_info*
GenerateMipmaps(char const*          OriginalFilename,
                char const*          OutputFilename,
                granny_file_info* Info,
                key_value_pair* KeyValues,
                granny_int32x NumKeyValues,
                granny_memory_arena* TempArena)
{
    {for (int TexIdx = 0; TexIdx < Info->TextureCount; ++TexIdx)
    {
        granny_texture* Texture = Info->Textures[TexIdx];
        if (!Texture)
            continue;

        // Can't do anything if there's nothing there
        if (Texture->ImageCount == 0)
            continue;

        if (Texture->ImageCount > 1)
        {
            WarnOut("Ignoring multi-image textures for now");
            continue;
        }

        // Ignore the texture if it *already* has mip levels
        if (Texture->Images[0].MIPLevelCount > 1)
            continue;

        // Extract the image in RGBA form, which is what we'll use to re-encode it.
        int ByteCount = Texture->Width * Texture->Height * 4;
        granny_uint8* RawImage = PushArray(TempArena, ByteCount, granny_uint8);

        if (!GrannyCopyTextureImage(Texture, 0, 0,
                                    GrannyRGBA8888PixelFormat,
                                    Texture->Width, Texture->Height,
                                    Texture->Width * 4,
                                    RawImage))
        {
            ErrOut("Error extracting texture: %d", TexIdx);
            return 0;
        }

        int MipLevelCount = 0;
        {
            int Width  = Texture->Width;
            int Height = Texture->Height;
            while (Width > 1 && Height > 1) // note, not full mip series...
            {
                ++MipLevelCount;
                if (Width > 1)  Width >>= 1;
                if (Height > 1) Height >>= 1;
            }
        }

        // Always raw for now...
        granny_texture_builder* Builder =
            GrannyBeginRawTexture(Texture->Width, Texture->Height,
                                  (GrannyTextureHasAlpha(Texture) ?
                                   GrannyRGBA8888PixelFormat : GrannyRGB888PixelFormat),
                                  4);
        GrannyEncodeImage(Builder,
                          Texture->Width, Texture->Height, Texture->Width*4,
                          MipLevelCount, RawImage);
        
        granny_uint8* TextureBuffer =
            PushArray(TempArena, GrannyGetResultingTextureSize(Builder), granny_uint8);

        granny_texture* NewTexture = GrannyEndTextureInPlace(Builder, TextureBuffer);
		
		// I know.  You're looking at this line and thinking: LOL you mean
		//
		//   Info->Textures[TexIdx] = NewTexture;
		//
		// Nope, the materials refer to this by pointer, so it's easiest
		// not to change it.  Doesn't even leak memory.  The magic of GR2.

		*Info->Textures[TexIdx] = *NewTexture;
    }}

    return Info;
}

static const char* HelpString =
    (" GenerateMipmaps will create mip series for all textures that lack\n"
     " them. Note that for the moment, all textures will be created as raw\n"
     " RGB or RGBA textures. It should be very easy to modify this to\n"
     " Bink or S3TC mip series.\n"
     "\n"
     "    preprocessor GenerateMipmaps file.gr2 -output file_w_mips.gr2\n");
     
   

// Register the commands
static CommandRegistrar RegGenerateTextures(GenerateMipmaps,
                                            "GenerateMipmaps",
                                            "Generates mip series for the textures in the specified file",
                                            HelpString);

