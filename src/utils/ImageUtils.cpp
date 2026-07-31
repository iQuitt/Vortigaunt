#include "ImageUtils.h"

#ifdef QT_WIDGETS_LIB
#include "FileIO.h"
#include "Dds.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <algorithm>
#include <cctype>

#ifndef STB_IMAGE_STATIC
#define STB_IMAGE_STATIC
#endif
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#include "../../ThirdParty/assimp/contrib/stb/stb_image.h"

namespace ImageUtils {

QImage loadImage(const std::string& filePath) {
    if (filePath.empty()) {
        return QImage();
    }

    std::filesystem::path p = FileIO::toPath(filePath);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (ext == ".dds") {
        return DDS::loadDdsToQImage(QString::fromStdString(filePath));
    }

    if (ext != ".tga") {
        QImage img(QString::fromStdString(filePath));
        if (!img.isNull()) {
            return img;
        }
    }

    std::ifstream file = FileIO::openRead(filePath, std::ios::binary);
    if (!file.is_open()) {
        return QImage(QString::fromStdString(filePath));
    }

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();

    if (buffer.empty()) {
        return QImage();
    }

    int width = 0, height = 0, comp = 0;
    stbi_uc* pixels = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, &comp, 4);
    if (pixels) {
        QImage stbImg(pixels, width, height, width * 4, QImage::Format_RGBA8888);
        QImage result = stbImg.copy();
        stbi_image_free(pixels);
        return result;
    }

    return QImage(QString::fromStdString(filePath));
}

QImage loadImage(const QString& filePath) {
    return loadImage(filePath.toStdString());
}

} // namespace ImageUtils
#endif
