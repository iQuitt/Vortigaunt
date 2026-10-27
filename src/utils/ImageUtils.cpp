// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

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
#include "utils/util.hpp"
#include "VTFLib.h"

namespace ImageUtils {

QImage loadVtfFromMemory(const void* data, size_t size, int frame, int face, int mipmap, VtfLayers* layers) {
    VTFLib::CVTFFile vtf;
    if (!vtf.Load(data, static_cast<vlUInt>(size))) {
        return QImage();
    }
    if (layers) {
        layers->frames = static_cast<int>(vtf.GetFrameCount());
        layers->faces = static_cast<int>(vtf.GetFaceCount());
        layers->mipmaps = static_cast<int>(vtf.GetMipmapCount());
        layers->format = QString::fromLatin1(VTFLib::CVTFFile::GetImageFormatInfo(vtf.GetFormat()).lpName);
    }
    if (frame < 0 || face < 0 || mipmap < 0 || frame >= static_cast<int>(vtf.GetFrameCount()) ||
        face >= static_cast<int>(vtf.GetFaceCount()) || mipmap >= static_cast<int>(vtf.GetMipmapCount())) {
        return QImage();
    }
    vlUInt width = 0, height = 0, depth = 0;
    VTFLib::CVTFFile::ComputeMipmapDimensions(vtf.GetWidth(), vtf.GetHeight(), vtf.GetDepth(), static_cast<vlUInt>(mipmap), width, height, depth);
    vlByte* source = vtf.GetData(static_cast<vlUInt>(frame), static_cast<vlUInt>(face), 0, static_cast<vlUInt>(mipmap));
    if (width == 0 || height == 0 || !source) {
        return QImage();
    }
    QImage image(static_cast<int>(width), static_cast<int>(height), QImage::Format_RGBA8888);
    if (image.isNull() || image.bytesPerLine() != static_cast<qsizetype>(width) * 4 ||
        !VTFLib::CVTFFile::ConvertToRGBA8888(source, image.bits(), width, height, vtf.GetFormat())) {
        return QImage();
    }
    return image;
}

QImage loadImageFromMemory(const void* data, size_t size, const std::string& extension) {
    if (!data || size == 0) {
        return QImage();
    }
    const std::string ext = String_ToLower(extension);
    const auto* bytes = static_cast<const uint8_t*>(data);
    if (ext == ".dds") {
        return DDS::loadDdsFromMemory(std::vector<uint8_t>(bytes, bytes + size));
    }
    if (ext == ".vtf") {
        return loadVtfFromMemory(data, size);
    }
    if (ext != ".tga") {
        QImage img = QImage::fromData(bytes, static_cast<int>(size));
        if (!img.isNull()) {
            return img;
        }
    }

    int width = 0, height = 0, comp = 0;
    stbi_uc* pixels = stbi_load_from_memory(bytes, static_cast<int>(size), &width, &height, &comp, 4);
    if (pixels) {
        QImage stbImg(pixels, width, height, width * 4, QImage::Format_RGBA8888);
        QImage result = stbImg.copy();
        stbi_image_free(pixels);
        return result;
    }
    return QImage::fromData(bytes, static_cast<int>(size));
}

QImage loadImage(const std::string& filePath) {
    if (filePath.empty()) {
        return QImage();
    }
    std::ifstream file = FileIO::openRead(filePath, std::ios::binary);
    if (!file.is_open()) {
        return QImage();
    }
    const std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    return loadImageFromMemory(buffer.data(), buffer.size(), FileIO::fromPath(FileIO::toPath(filePath).extension()));
}

QImage loadImage(const QString& filePath) {
    return loadImage(filePath.toStdString());
}

} // namespace ImageUtils
#endif
