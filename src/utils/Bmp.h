// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <cstdint>
#include <string>
#include <vector>

#ifdef QT_WIDGETS_LIB
#include <QImage>
#endif

namespace BMP
{

struct GoldSrcTextureOptions
{
    int maxSize = 512;
    bool powerOfTwo = false;  // snap each side to the nearest power of two
    bool masked = false;      // alpha below 128 becomes index 255 (blue), for $texrendermode masked
};

// what saveGoldSrcTexture wrote
struct GoldSrcTextureInfo
{
    int width = 0;
    int height = 0;
    bool resized = false;
    bool masked = false;
};

bool writeIndexed8(const char* filename, int width, int height, const uint8_t* paletteData, const uint8_t* pixelData);
// the same with a palette of 256 RGB entries, as GoldSrc keeps them
bool writeIndexed8Rgb(const std::string& filename, int width, int height, const uint8_t* paletteRgb, const uint8_t* pixels);

// paletteRgb gets 256 RGB entries; with masked, index 255 is blue and marks pixels with alpha below 128
bool quantize(const uint32_t* argbPixels, int width, int height, bool masked, uint8_t* paletteRgb, std::vector<uint8_t>& indices);

// box filter when shrinking, bilinear when growing; colours are weighted by alpha
std::vector<uint32_t> resize(const uint32_t* argbPixels, int width, int height, int newWidth, int newHeight);

bool saveAsIndexed8(const std::string& filename, int width, int height, const uint32_t* argbPixels);

// the one way textures go to GoldSrc: alpha dropped (or masked), fitted into maxSize, 256 colours
bool saveGoldSrcTexture(const std::string& filename, int width, int height, const uint32_t* argbPixels,
                        const GoldSrcTextureOptions& options = {}, GoldSrcTextureInfo* info = nullptr);

#ifdef QT_WIDGETS_LIB
inline bool saveGoldSrcTexture(const std::string& filename, const QImage& image,
                               const GoldSrcTextureOptions& options = {}, GoldSrcTextureInfo* info = nullptr)
{
    if (image.isNull()) return false;
    const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    return saveGoldSrcTexture(filename, argb.width(), argb.height(),
                              reinterpret_cast<const uint32_t*>(argb.constBits()), options, info);
}
#endif

} // namespace BMP
