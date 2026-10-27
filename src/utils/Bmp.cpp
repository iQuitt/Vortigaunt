// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "Bmp.h"
#include "FileIO.h"

#include "libimagequant.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{

struct Tap
{
    int index;
    float weight;
};

constexpr uint8_t MaskedIndex = 255;

std::vector<std::vector<Tap>> resampleTaps(int source, int target)
{
    std::vector<std::vector<Tap>> taps(target);
    const double scale = static_cast<double>(source) / target;
    for (int i = 0; i < target; ++i)
    {
        std::vector<Tap>& list = taps[i];
        if (scale > 1.0)
        {
            const double lo = i * scale;
            const double hi = (i + 1) * scale;
            for (int j = static_cast<int>(lo); j < source && j < hi; ++j)
            {
                const double overlap = std::min(hi, j + 1.0) - std::max(lo, static_cast<double>(j));
                if (overlap > 0.0) list.push_back({ j, static_cast<float>(overlap) });
            }
        }
        else
        {
            const double center = (i + 0.5) * scale - 0.5;
            const int j = static_cast<int>(std::floor(center));
            const float t = static_cast<float>(center - j);
            list.push_back({ std::clamp(j, 0, source - 1), 1.0f - t });
            list.push_back({ std::clamp(j + 1, 0, source - 1), t });
        }

        float sum = 0.0f;
        for (const Tap& tap : list) sum += tap.weight;
        for (Tap& tap : list) tap.weight /= sum;
    }
    return taps;
}

int nearestPowerOfTwo(int value, int maximum)
{
    int p = 1;
    while (p * 2 <= value) p *= 2;
    if (static_cast<double>(value) / p > 1.41421356) p *= 2;
    return std::clamp(p, std::min(4, maximum), maximum);
}

void toBmpPalette(const uint8_t* paletteRgb, uint8_t* paletteBgra)
{
    for (int i = 0; i < 256; ++i)
    {
        paletteBgra[i * 4 + 0] = paletteRgb[i * 3 + 2];
        paletteBgra[i * 4 + 1] = paletteRgb[i * 3 + 1];
        paletteBgra[i * 4 + 2] = paletteRgb[i * 3 + 0];
        paletteBgra[i * 4 + 3] = 0;
    }
}

} // namespace

namespace BMP
{

bool writeIndexed8(const char* filename, int width, int height, const uint8_t* paletteData, const uint8_t* pixelData)
{
    FILE* f = FileIO::openFile(std::string(filename), "wb");
    if (!f) return false;

    const uint32_t offBits = 14 + 40 + 1024;
    const int paddingSize = (4 - (width % 4)) % 4;
    const int stride = width + paddingSize;
    const uint32_t fileSize = offBits + (stride * height);

    uint8_t fh[14] = { 'B','M', 0,0,0,0, 0,0, 0,0, 0,0,0,0 };
    std::memcpy(fh + 2, &fileSize, 4);
    std::memcpy(fh + 10, &offBits, 4);
    fwrite(fh, 1, 14, f);

    uint8_t ih[40] = {};
    const uint32_t headerSize = 40, colours = 256;
    const uint16_t planes = 1, bitCount = 8;
    std::memcpy(ih + 0, &headerSize, 4);
    std::memcpy(ih + 4, &width, 4);
    std::memcpy(ih + 8, &height, 4);
    std::memcpy(ih + 12, &planes, 2);
    std::memcpy(ih + 14, &bitCount, 2);
    std::memcpy(ih + 32, &colours, 4);
    std::memcpy(ih + 36, &colours, 4);
    fwrite(ih, 1, 40, f);

    fwrite(paletteData, 1, 1024, f);

    // rows are stored bottom-up
    const uint8_t pad[3] = { 0, 0, 0 };
    for (int y = height - 1; y >= 0; --y)
    {
        fwrite(pixelData + static_cast<size_t>(y) * width, 1, width, f);
        if (paddingSize > 0) fwrite(pad, 1, paddingSize, f);
    }

    const bool ok = !ferror(f);
    return fclose(f) == 0 && ok;
}

bool writeIndexed8Rgb(const std::string& filename, int width, int height, const uint8_t* paletteRgb, const uint8_t* pixels)
{
    uint8_t palette[1024];
    toBmpPalette(paletteRgb, palette);
    return writeIndexed8(filename.c_str(), width, height, palette, pixels);
}

bool quantize(const uint32_t* argbPixels, int width, int height, bool masked, uint8_t* paletteRgb, std::vector<uint8_t>& indices)
{
    if (width <= 0 || height <= 0) return false;
    const size_t total = static_cast<size_t>(width) * height;

    std::vector<uint8_t> rgba(total * 4);
    std::vector<uint8_t> importance;
    size_t holes = 0;
    for (size_t i = 0; i < total; ++i)
    {
        const uint32_t c = argbPixels[i];
        rgba[i * 4 + 0] = (c >> 16) & 0xFF;
        rgba[i * 4 + 1] = (c >> 8) & 0xFF;
        rgba[i * 4 + 2] = c & 0xFF;
        rgba[i * 4 + 3] = 255;
        if (masked && (c >> 24) < 128) holes++;
    }

    std::memset(paletteRgb, 0, 768);
    indices.assign(total, 0);
    if (holes > 0)
    {
        paletteRgb[MaskedIndex * 3 + 2] = 255;
        if (holes == total)
        {
            indices.assign(total, MaskedIndex);
            return true;
        }
        importance.resize(total);
        for (size_t i = 0; i < total; ++i)
        {
            importance[i] = (argbPixels[i] >> 24) < 128 ? 0 : 255;
        }
    }

    liq_attr* attr = liq_attr_create();
    if (!attr) return false;
    liq_set_speed(attr, 1);
    liq_set_max_colors(attr, holes > 0 ? 255 : 256);

    liq_image* img = liq_image_create_rgba(attr, rgba.data(), width, height, 0);
    liq_result* res = nullptr;
    bool ok = img != nullptr;
    if (ok && holes > 0)
    {
        ok = liq_image_set_importance_map(img, importance.data(), total, LIQ_COPY_PIXELS) == LIQ_OK;
    }
    ok = ok && liq_image_quantize(img, attr, &res) == LIQ_OK;
    ok = ok && liq_write_remapped_image(res, img, indices.data(), total) == LIQ_OK;
    if (ok)
    {
        const liq_palette* pal = liq_get_palette(res);
        for (unsigned int i = 0; i < pal->count; ++i)
        {
            paletteRgb[i * 3 + 0] = pal->entries[i].r;
            paletteRgb[i * 3 + 1] = pal->entries[i].g;
            paletteRgb[i * 3 + 2] = pal->entries[i].b;
        }
        if (holes > 0)
        {
            for (size_t i = 0; i < total; ++i)
            {
                if ((argbPixels[i] >> 24) < 128) indices[i] = MaskedIndex;
            }
        }
    }

    if (res) liq_result_destroy(res);
    if (img) liq_image_destroy(img);
    liq_attr_destroy(attr);
    return ok;
}

std::vector<uint32_t> resize(const uint32_t* argbPixels, int width, int height, int newWidth, int newHeight)
{
    const std::vector<std::vector<Tap>> tapsX = resampleTaps(width, newWidth);
    const std::vector<std::vector<Tap>> tapsY = resampleTaps(height, newHeight);

    // premultiplied, so see-through pixels do not bleed their colour
    std::vector<float> rows(static_cast<size_t>(newWidth) * height * 4);
    for (int y = 0; y < height; ++y)
    {
        const uint32_t* src = argbPixels + static_cast<size_t>(y) * width;
        for (int x = 0; x < newWidth; ++x)
        {
            float sum[4] = {};
            for (const Tap& tap : tapsX[x])
            {
                const uint32_t c = src[tap.index];
                const float a = static_cast<float>(c >> 24) * tap.weight;
                sum[0] += a;
                sum[1] += static_cast<float>((c >> 16) & 0xFF) * a;
                sum[2] += static_cast<float>((c >> 8) & 0xFF) * a;
                sum[3] += static_cast<float>(c & 0xFF) * a;
            }
            std::memcpy(&rows[(static_cast<size_t>(y) * newWidth + x) * 4], sum, sizeof(sum));
        }
    }

    std::vector<uint32_t> out(static_cast<size_t>(newWidth) * newHeight);
    for (int y = 0; y < newHeight; ++y)
    {
        for (int x = 0; x < newWidth; ++x)
        {
            float sum[4] = {};
            for (const Tap& tap : tapsY[y])
            {
                const float* p = &rows[(static_cast<size_t>(tap.index) * newWidth + x) * 4];
                for (int k = 0; k < 4; ++k) sum[k] += p[k] * tap.weight;
            }
            const auto channel = [](float v) { return static_cast<uint32_t>(std::clamp(std::lround(v), 0L, 255L)); };
            const float alpha = sum[0];
            uint32_t c = channel(alpha) << 24;
            if (alpha > 0.0f)
            {
                c |= channel(sum[1] / alpha) << 16 | channel(sum[2] / alpha) << 8 | channel(sum[3] / alpha);
            }
            out[static_cast<size_t>(y) * newWidth + x] = c;
        }
    }
    return out;
}

bool saveAsIndexed8(const std::string& filename, int width, int height, const uint32_t* argbPixels)
{
    uint8_t palette[768];
    std::vector<uint8_t> indices;
    return quantize(argbPixels, width, height, false, palette, indices) &&
           writeIndexed8Rgb(filename, width, height, palette, indices.data());
}

bool saveGoldSrcTexture(const std::string& filename, int width, int height, const uint32_t* argbPixels,
                        const GoldSrcTextureOptions& options, GoldSrcTextureInfo* info)
{
    if (width <= 0 || height <= 0 || !argbPixels) return false;
    const size_t total = static_cast<size_t>(width) * height;

    // alpha is a gloss or blend mask in most source formats; only masked textures keep it, as on/off
    std::vector<uint32_t> pixels(argbPixels, argbPixels + total);
    bool hasHoles = false;
    for (uint32_t& c : pixels)
    {
        if (options.masked && (c >> 24) < 128)
        {
            c &= 0x00FFFFFF;
            hasHoles = true;
        }
        else
        {
            c |= 0xFF000000;
        }
    }

    int newWidth = width, newHeight = height;
    if (options.powerOfTwo)
    {
        newWidth = nearestPowerOfTwo(width, options.maxSize);
        newHeight = nearestPowerOfTwo(height, options.maxSize);
    }
    else if (width > options.maxSize || height > options.maxSize)
    {
        const double scale = std::min(static_cast<double>(options.maxSize) / width, static_cast<double>(options.maxSize) / height);
        newWidth = std::max(1, static_cast<int>(std::lround(width * scale)));
        newHeight = std::max(1, static_cast<int>(std::lround(height * scale)));
    }
    if (newWidth != width || newHeight != height)
    {
        pixels = resize(pixels.data(), width, height, newWidth, newHeight);
    }

    uint8_t palette[768];
    std::vector<uint8_t> indices;
    if (!quantize(pixels.data(), newWidth, newHeight, hasHoles, palette, indices)) return false;
    if (!writeIndexed8Rgb(filename, newWidth, newHeight, palette, indices.data())) return false;

    if (info)
    {
        info->width = newWidth;
        info->height = newHeight;
        info->resized = newWidth != width || newHeight != height;
        info->masked = hasHoles && std::any_of(pixels.begin(), pixels.end(), [](uint32_t c) { return (c >> 24) < 128; });
    }
    return true;
}

} // namespace BMP
