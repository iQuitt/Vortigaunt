#pragma once


#include <cstdio>
#include <cstdint>
#include <cstring>
#include <climits>
#include <vector>
#include <algorithm>
#include <string>

#ifdef QT_WIDGETS_LIB
#include <QImage>
#include <QString>
#endif

#include "libimagequant.h" // optimize the palette using 
#include "FileIO.h"

namespace BMP
{

inline bool writeIndexed8(const char* filename, int width, int height,  const uint8_t* paletteData, const uint8_t* pixelData)
{
    FILE* f = FileIO::openFile(std::string(filename), "wb");
    if (!f) return false;

    uint32_t offBits  = 14 + 40 + 1024;
    int widthBytes    = width;
    int paddingSize   = (4 - (widthBytes % 4)) % 4;
    int stride        = widthBytes + paddingSize;
    uint32_t fileSize = offBits + (stride * height);

    // File Header (14 bytes)
    uint8_t fh[14] = { 'B','M', 0,0,0,0, 0,0, 0,0, 0,0,0,0 };
    *(uint32_t*)(fh + 2)  = fileSize;
    *(uint32_t*)(fh + 10) = offBits;
    fwrite(fh, 1, 14, f);

    // Info Header (40 bytes)
    uint8_t ih[40] = {0};
    *(uint32_t*)(ih + 0)  = 40;
    *(int32_t*) (ih + 4)  = width;
    *(int32_t*) (ih + 8)  = height;    
    *(uint16_t*)(ih + 12) = 1;         // planes
    *(uint16_t*)(ih + 14) = 8;         // bitcount
    *(uint32_t*)(ih + 32) = 256;       // ClrUsed
    *(uint32_t*)(ih + 36) = 256;       // ClrImportant
    fwrite(ih, 1, 40, f);

    fwrite(paletteData, 1, 1024, f);

    // Pixel rows - stored bottom-up in file
    uint8_t pad[3] = {0,0,0};
    for (int y = height - 1; y >= 0; --y)
    {
        fwrite(pixelData + y * width, 1, width, f);
        if (paddingSize > 0)
            fwrite(pad, 1, paddingSize, f);
    }
    fclose(f);
    return true;
}


inline bool saveAsIndexed8(const char* filename, int width, int height, const uint32_t* argbPixels)
{
    int total = width * height;
    if (total <= 0) return false;

    std::vector<uint8_t> rgbaBuffer(total * 4);
    for (int i = 0; i < total; ++i)
    {
        uint32_t c = argbPixels[i];
        rgbaBuffer[i * 4 + 0] = (c >> 16) & 0xFF; // R
        rgbaBuffer[i * 4 + 1] = (c >> 8)  & 0xFF; // G
        rgbaBuffer[i * 4 + 2] = (c)       & 0xFF; // B
        rgbaBuffer[i * 4 + 3] = (c >> 24) & 0xFF; // A
    }

    liq_attr* attr = liq_attr_create();
    if (!attr) return false;
    
    liq_set_speed(attr, 1); // Max quality i guess
    liq_set_max_colors(attr, 256);

    liq_image* img = liq_image_create_rgba(attr, rgbaBuffer.data(), width, height, 0);
    if (!img)
    {
        liq_attr_destroy(attr);
        return false;
    }

    liq_result* res = nullptr;
    if (liq_image_quantize(img, attr, &res) != LIQ_OK)
    {
        liq_image_destroy(img);
        liq_attr_destroy(attr);
        return false;
    }

    const liq_palette* pal = liq_get_palette(res);
    
    uint8_t bmpPalette[1024];
    memset(bmpPalette, 0, 1024);
    for (unsigned int i = 0; i < pal->count; ++i)
    {
        bmpPalette[i * 4 + 0] = pal->entries[i].b;
        bmpPalette[i * 4 + 1] = pal->entries[i].g;
        bmpPalette[i * 4 + 2] = pal->entries[i].r;
        bmpPalette[i * 4 + 3] = 0;
    }

    std::vector<uint8_t> indexedData(total);
    if (liq_write_remapped_image(res, img, indexedData.data(), total) != LIQ_OK)
    {
        liq_result_destroy(res);
        liq_image_destroy(img);
        liq_attr_destroy(attr);
        return false;
    }

    liq_result_destroy(res);
    liq_image_destroy(img);
    liq_attr_destroy(attr);

    return writeIndexed8(filename, width, height, bmpPalette, indexedData.data());
}

inline bool saveAsIndexed8(const std::string& filename, int width, int height, const uint32_t* argbPixels)
{
    return saveAsIndexed8(filename.c_str(), width, height, argbPixels);
}

} // namespace BMP
