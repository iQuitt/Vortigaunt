#include "gcl/utilities/devilimageutility.h"

#include <IL/ilu.h>
namespace GCL::Utilities {

void initializeDevilImageLibrary()
{
    ilInit();
}

void shutdownDevilImageLibrary()
{
    ilShutDown();
}

void convertImage(string sourceFilePath, string targetFilePath, bool flipImage)
{
    ILuint ImgId;
    ilGenImages(1, &ImgId);
    ilBindImage(ImgId);
    ilLoadImage(sourceFilePath.c_str());

    ilConvertImage(IL_RGBA, IL_UNSIGNED_BYTE);

    if (flipImage) {
        iluFlipImage();
    }

    ilSaveImage(targetFilePath.c_str());
    ilDeleteImage(ImgId);
}

} // namespace GCL::Utilities
