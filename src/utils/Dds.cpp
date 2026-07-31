#include "utils/Dds.h"
#include <fstream>
#include <cstring>
#include <filesystem>

namespace DDS {

// ============================================================================
// Block Decoders
// ============================================================================

void decodeDXT1Block(const uint8_t* block, uint32_t* output, int x, int y, int width, int height)
{
    uint16_t color0 = *reinterpret_cast<const uint16_t*>(block);
    uint16_t color1 = *reinterpret_cast<const uint16_t*>(block + 2);
    uint32_t indices = *reinterpret_cast<const uint32_t*>(block + 4);
    
    // Extract RGB565 colors with proper bit replication (matches PIL's decode_565)
    // This ensures full 0-255 range: e.g. 5-bit max (31) -> 255, not 248
    uint8_t r0 = ((color0 >> 11) & 0x1F);
    r0 = (r0 << 3) | (r0 >> 2);
    uint8_t g0 = ((color0 >> 5) & 0x3F);
    g0 = (g0 << 2) | (g0 >> 4);
    uint8_t b0 = (color0 & 0x1F);
    b0 = (b0 << 3) | (b0 >> 2);
    
    uint8_t r1 = ((color1 >> 11) & 0x1F);
    r1 = (r1 << 3) | (r1 >> 2);
    uint8_t g1 = ((color1 >> 5) & 0x3F);
    g1 = (g1 << 2) | (g1 >> 4);
    uint8_t b1 = (color1 & 0x1F);
    b1 = (b1 << 3) | (b1 >> 2);
    
    uint32_t colors[4];
    colors[0] = 0xFF000000 | (r0 << 16) | (g0 << 8) | b0;
    colors[1] = 0xFF000000 | (r1 << 16) | (g1 << 8) | b1;
    
    if (color0 > color1) {
        colors[2] = 0xFF000000 | (((r0 * 2 + r1) / 3) << 16) | (((g0 * 2 + g1) / 3) << 8) | ((b0 * 2 + b1) / 3);
        colors[3] = 0xFF000000 | (((r0 + r1 * 2) / 3) << 16) | (((g0 + g1 * 2) / 3) << 8) | ((b0 + b1 * 2) / 3);
    } else {
        colors[2] = 0xFF000000 | (((r0 + r1) / 2) << 16) | (((g0 + g1) / 2) << 8) | ((b0 + b1) / 2);
        colors[3] = 0x00000000; // Transparent
    }
    
    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            int idx = (j * 4 + i);
            int bitOffset = idx * 2;
            uint32_t colorIdx = (indices >> bitOffset) & 0x3;
            
            int px = x + i;
            int py = y + j;
            if (px < width && py < height) {
                output[py * width + px] = colors[colorIdx];
            }
        }
    }
}

void decodeDXT3Block(const uint8_t* block, uint32_t* output, int x, int y, int width, int height)
{
    const uint8_t* colorBlock = block + 8;
    uint16_t color0 = *reinterpret_cast<const uint16_t*>(colorBlock);
    uint16_t color1 = *reinterpret_cast<const uint16_t*>(colorBlock + 2);
    uint32_t indices = *reinterpret_cast<const uint32_t*>(colorBlock + 4);

    uint8_t r0 = ((color0 >> 11) & 0x1F);
    r0 = (r0 << 3) | (r0 >> 2);
    uint8_t g0 = ((color0 >> 5) & 0x3F);
    g0 = (g0 << 2) | (g0 >> 4);
    uint8_t b0 = (color0 & 0x1F);
    b0 = (b0 << 3) | (b0 >> 2);

    uint8_t r1 = ((color1 >> 11) & 0x1F);
    r1 = (r1 << 3) | (r1 >> 2);
    uint8_t g1 = ((color1 >> 5) & 0x3F);
    g1 = (g1 << 2) | (g1 >> 4);
    uint8_t b1 = (color1 & 0x1F);
    b1 = (b1 << 3) | (b1 >> 2);

    uint32_t colors[4];
    colors[0] = (r0 << 16) | (g0 << 8) | b0;
    colors[1] = (r1 << 16) | (g1 << 8) | b1;
    colors[2] = (((r0 * 2 + r1) / 3) << 16) | (((g0 * 2 + g1) / 3) << 8) | ((b0 * 2 + b1) / 3);
    colors[3] = (((r0 + r1 * 2) / 3) << 16) | (((g0 + g1 * 2) / 3) << 8) | ((b0 + b1 * 2) / 3);

    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            int idx = (j * 4 + i);

            uint8_t byteVal = block[idx / 2];
            uint8_t nibble = (idx & 1) ? (byteVal >> 4) : (byteVal & 0x0F);
            uint8_t alpha = nibble * 17;

            int colorBitOffset = idx * 2;
            uint32_t colorIdx = (indices >> colorBitOffset) & 0x3;

            int px = x + i;
            int py = y + j;
            if (px < width && py < height) {
                output[py * width + px] = (static_cast<uint32_t>(alpha) << 24) | colors[colorIdx];
            }
        }
    }
}

void decodeDXT5Block(const uint8_t* block, uint32_t* output, int x, int y, int width, int height)
{
    uint8_t alpha0 = block[0];
    uint8_t alpha1 = block[1];
    uint64_t alphaIndices = *reinterpret_cast<const uint64_t*>(block) >> 16;
    
    uint8_t alphaTable[8];
    alphaTable[0] = alpha0;
    alphaTable[1] = alpha1;
    if (alpha0 > alpha1) {
        alphaTable[2] = (6 * alpha0 + 1 * alpha1) / 7;
        alphaTable[3] = (5 * alpha0 + 2 * alpha1) / 7;
        alphaTable[4] = (4 * alpha0 + 3 * alpha1) / 7;
        alphaTable[5] = (3 * alpha0 + 4 * alpha1) / 7;
        alphaTable[6] = (2 * alpha0 + 5 * alpha1) / 7;
        alphaTable[7] = (1 * alpha0 + 6 * alpha1) / 7;
    } else {
        alphaTable[2] = (4 * alpha0 + 1 * alpha1) / 5;
        alphaTable[3] = (3 * alpha0 + 2 * alpha1) / 5;
        alphaTable[4] = (2 * alpha0 + 3 * alpha1) / 5;
        alphaTable[5] = (1 * alpha0 + 4 * alpha1) / 5;
        alphaTable[6] = 0;
        alphaTable[7] = 255;
    }
    
    // Decode color part (same as DXT1)
    const uint8_t* colorBlock = block + 8;
    uint16_t color0 = *reinterpret_cast<const uint16_t*>(colorBlock);
    uint16_t color1 = *reinterpret_cast<const uint16_t*>(colorBlock + 2);
    uint32_t indices = *reinterpret_cast<const uint32_t*>(colorBlock + 4);
    
    // Extract RGB565 colors with proper bit replication (matches PIL's decode_565)
    uint8_t r0 = ((color0 >> 11) & 0x1F);
    r0 = (r0 << 3) | (r0 >> 2);
    uint8_t g0 = ((color0 >> 5) & 0x3F);
    g0 = (g0 << 2) | (g0 >> 4);
    uint8_t b0 = (color0 & 0x1F);
    b0 = (b0 << 3) | (b0 >> 2);
    
    uint8_t r1 = ((color1 >> 11) & 0x1F);
    r1 = (r1 << 3) | (r1 >> 2);
    uint8_t g1 = ((color1 >> 5) & 0x3F);
    g1 = (g1 << 2) | (g1 >> 4);
    uint8_t b1 = (color1 & 0x1F);
    b1 = (b1 << 3) | (b1 >> 2);
    
    uint32_t colors[4];
    colors[0] = (r0 << 16) | (g0 << 8) | b0;
    colors[1] = (r1 << 16) | (g1 << 8) | b1;
    colors[2] = (((r0 * 2 + r1) / 3) << 16) | (((g0 * 2 + g1) / 3) << 8) | ((b0 * 2 + b1) / 3);
    colors[3] = (((r0 + r1 * 2) / 3) << 16) | (((g0 + g1 * 2) / 3) << 8) | ((b0 + b1 * 2) / 3);
    
    for (int j = 0; j < 4; j++) {
        for (int i = 0; i < 4; i++) {
            int idx = (j * 4 + i);
            
            // Alpha
            int alphaBitOffset = idx * 3;
            uint8_t alphaIdx = (alphaIndices >> alphaBitOffset) & 0x7;
            uint8_t alpha = alphaTable[alphaIdx];
            
            // Color
            int colorBitOffset = idx * 2;
            uint32_t colorIdx = (indices >> colorBitOffset) & 0x3;
            
            int px = x + i;
            int py = y + j;
            if (px < width && py < height) {
                output[py * width + px] = (alpha << 24) | colors[colorIdx];
            }
        }
    }
}

void decodeDXT1(const uint8_t* src, uint32_t* output, int width, int height)
{
    int blockWidth = (width + 3) / 4;
    int blockHeight = (height + 3) / 4;
    for (int by = 0; by < blockHeight; ++by) {
        for (int bx = 0; bx < blockWidth; ++bx) {
            size_t blockOffset = (by * blockWidth + bx) * 8;
            const uint8_t* block = src + blockOffset;
            int x = bx * 4;
            int y = by * 4;
            decodeDXT1Block(block, output, x, y, width, height);
        }
    }
}

void decodeDXT3(const uint8_t* src, uint32_t* output, int width, int height)
{
    int blockWidth = (width + 3) / 4;
    int blockHeight = (height + 3) / 4;
    for (int by = 0; by < blockHeight; ++by) {
        for (int bx = 0; bx < blockWidth; ++bx) {
            size_t blockOffset = (by * blockWidth + bx) * 16;
            const uint8_t* block = src + blockOffset;
            int x = bx * 4;
            int y = by * 4;
            decodeDXT3Block(block, output, x, y, width, height);
        }
    }
}

void decodeDXT5(const uint8_t* src, uint32_t* output, int width, int height)
{
    int blockWidth = (width + 3) / 4;
    int blockHeight = (height + 3) / 4;
    for (int by = 0; by < blockHeight; ++by) {
        for (int bx = 0; bx < blockWidth; ++bx) {
            size_t blockOffset = (by * blockWidth + bx) * 16;
            const uint8_t* block = src + blockOffset;
            int x = bx * 4;
            int y = by * 4;
            decodeDXT5Block(block, output, x, y, width, height);
        }
    }
}

// ============================================================================
// Utility Functions
// ============================================================================

bool isDdsFile(const std::string& filePath)
{
    std::ifstream file(filePath, std::ios::binary);
    if (!file)
        return false;
    
    uint32_t magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    return magic == DDS_MAGIC;
}

std::vector<uint32_t> decodeDdsToPixels(const std::vector<uint8_t>& data, uint32_t& width, uint32_t& height)
{
    if (data.size() < 128) // Minimum DDS header size
        return {};
    
    // Read magic
    uint32_t magic = *reinterpret_cast<const uint32_t*>(data.data());
    if (magic != DDS_MAGIC)
        return {};
    
    // Read header
    DDS_HEADER header;
    std::memcpy(&header, data.data() + 4, sizeof(header));
    
    if (header.dwSize != 124)
        return {};
    
    width = header.dwWidth;
    height = header.dwHeight;
    
    uint32_t fourCC = header.ddspf.dwFourCC;
    bool isDXT1 = (fourCC == FOURCC_DXT1);
    bool isDXT3 = (fourCC == FOURCC_DXT3);
    bool isDXT5 = (fourCC == FOURCC_DXT5);
    bool isDX10 = (fourCC == FOURCC_DX10);
    
    size_t dataOffset = 4 + sizeof(DDS_HEADER);
    
    // Handle DX10 extended header
    if (isDX10) {
        if (data.size() < dataOffset + sizeof(DDS_HEADER_DXT10))
            return {};
        
        DDS_HEADER_DXT10 dx10Header;
        std::memcpy(&dx10Header, data.data() + dataOffset, sizeof(dx10Header));
        dataOffset += sizeof(DDS_HEADER_DXT10);
        
        switch (dx10Header.dxgiFormat) {
            case DXGI_FORMAT_BC1_UNORM:
            case DXGI_FORMAT_BC1_UNORM_SRGB:
                isDXT1 = true;
                break;
            case DXGI_FORMAT_BC2_UNORM:
            case DXGI_FORMAT_BC2_UNORM_SRGB:
                isDXT3 = true;
                break;
            case DXGI_FORMAT_BC3_UNORM:
            case DXGI_FORMAT_BC3_UNORM_SRGB:
                isDXT5 = true;
                break;
            default:
                return {};
        }
    }

    if (!isDXT1 && !isDXT3 && !isDXT5)
        return {};
    
    uint32_t blockWidth = (width + 3) / 4;
    uint32_t blockHeight = (height + 3) / 4;
    uint32_t blockSize = isDXT1 ? 8 : 16;
    
    size_t compressedSize = blockWidth * blockHeight * blockSize;
    
    if (data.size() < dataOffset + compressedSize)
        return {};
    
    std::vector<uint32_t> pixels(width * height, 0);
    const uint8_t* compressedData = data.data() + dataOffset;
    
    if (isDXT1) {
        decodeDXT1(compressedData, pixels.data(), static_cast<int>(width), static_cast<int>(height));
    } else if (isDXT3) {
        decodeDXT3(compressedData, pixels.data(), static_cast<int>(width), static_cast<int>(height));
    } else {
        decodeDXT5(compressedData, pixels.data(), static_cast<int>(width), static_cast<int>(height));
    }

    return pixels;
}

// ============================================================================
// Qt-specific Functions (only available when Qt is linked)
// ============================================================================

#ifdef QT_WIDGETS_LIB

QImage loadDdsFromMemory(const std::vector<uint8_t>& ddsData)
{
    uint32_t width, height;
    std::vector<uint32_t> pixels = decodeDdsToPixels(ddsData, width, height);
    
    if (pixels.empty())
        return QImage();
    
    QImage img(width, height, QImage::Format_ARGB32);
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            uint32_t pixel = pixels[y * width + x];
            img.setPixel(x, y, pixel);
        }
    }
    
    return img;
}

QImage loadDdsToQImage(const QString& filePath)
{
    std::ifstream file(std::filesystem::path(filePath.toStdWString()), std::ios::binary);
    if (!file)
        return QImage();
    
    // Read magic
    uint32_t magic;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != DDS_MAGIC)
        return QImage();
    
    // Read header
    DDS_HEADER header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (header.dwSize != 124)
        return QImage();
    
    uint32_t width = header.dwWidth;
    uint32_t height = header.dwHeight;
    
    uint32_t fourCC = header.ddspf.dwFourCC;
    bool isDXT1 = (fourCC == FOURCC_DXT1);
    bool isDXT2 = (fourCC == FOURCC_DXT2);
    bool isDXT3 = (fourCC == FOURCC_DXT3);
    bool isDXT4 = (fourCC == FOURCC_DXT4);
    bool isDXT5 = (fourCC == FOURCC_DXT5);
    bool isDX10 = (fourCC == FOURCC_DX10);
    
    // Handle DX10 extended header
    if (isDX10) {
        DDS_HEADER_DXT10 dx10Header;
        file.read(reinterpret_cast<char*>(&dx10Header), sizeof(dx10Header));
        
        switch (dx10Header.dxgiFormat) {
            case DXGI_FORMAT_BC1_UNORM:
            case DXGI_FORMAT_BC1_UNORM_SRGB:
                isDXT1 = true;
                break;
            case DXGI_FORMAT_BC2_UNORM:
            case DXGI_FORMAT_BC2_UNORM_SRGB:
                isDXT3 = true;
                break;
            case DXGI_FORMAT_BC3_UNORM:
            case DXGI_FORMAT_BC3_UNORM_SRGB:
                isDXT5 = true;
                break;
            default:
                return QImage();
        }
    }
    
    // Check for uncompressed formats
    bool isUncompressed = (header.ddspf.dwFlags & DDPF_RGB) != 0;
    
    if (isDXT1 || isDXT2 || isDXT3 || isDXT4 || isDXT5) {
        // Compressed DXT format
        uint32_t blockWidth = (width + 3) / 4;
        uint32_t blockHeight = (height + 3) / 4;
        uint32_t blockSize = isDXT1 ? 8 : 16;
        
        size_t dataSize = blockWidth * blockHeight * blockSize;
        std::vector<uint8_t> compressedData(dataSize);
        file.read(reinterpret_cast<char*>(compressedData.data()), dataSize);
        
        if (file.gcount() < static_cast<std::streamsize>(dataSize))
            return QImage();
        
        std::vector<uint32_t> pixels(width * height, 0);
        
        if (isDXT1) {
            decodeDXT1(compressedData.data(), pixels.data(), static_cast<int>(width), static_cast<int>(height));
        } else if (isDXT2 || isDXT3) {
            decodeDXT3(compressedData.data(), pixels.data(), static_cast<int>(width), static_cast<int>(height));
        } else {
            decodeDXT5(compressedData.data(), pixels.data(), static_cast<int>(width), static_cast<int>(height));
        }
        
        QImage img(width, height, QImage::Format_ARGB32);
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                uint32_t pixel = pixels[y * width + x];
                img.setPixel(x, y, pixel);
            }
        }
        return img;
    }
    else if (isUncompressed) {
        // Uncompressed format (RGB, RGBA, BGR, BGRA)
        uint32_t bpp = header.ddspf.dwRGBBitCount;
        uint32_t bytesPerPixel = bpp / 8;
        
        if (bytesPerPixel < 2 || bytesPerPixel > 4)
            return QImage();
        
        size_t rowPitch = (width * bytesPerPixel + 3) & ~3; // 4-byte aligned
        size_t dataSize = rowPitch * height;
        std::vector<uint8_t> rawData(dataSize);
        file.read(reinterpret_cast<char*>(rawData.data()), dataSize);
        
        QImage img(width, height, QImage::Format_ARGB32);
        
        uint32_t rMask = header.ddspf.dwRBitMask;
        uint32_t gMask = header.ddspf.dwGBitMask;
        uint32_t bMask = header.ddspf.dwBBitMask;
        uint32_t aMask = header.ddspf.dwABitMask;
        
        // Determine shift amounts and bit counts for each channel
        int rShift = 0, gShift = 0, bShift = 0, aShift = 0;
        int rBits = 0, gBits = 0, bBits = 0, aBits = 0;
        
        auto countBits = [](uint32_t mask) -> int {
            int count = 0;
            while (mask) { count += (mask & 1); mask >>= 1; }
            return count;
        };
        
        if (rMask) { while (((rMask >> rShift) & 1) == 0) rShift++; rBits = countBits(rMask); }
        if (gMask) { while (((gMask >> gShift) & 1) == 0) gShift++; gBits = countBits(gMask); }
        if (bMask) { while (((bMask >> bShift) & 1) == 0) bShift++; bBits = countBits(bMask); }
        if (aMask) { while (((aMask >> aShift) & 1) == 0) aShift++; aBits = countBits(aMask); }
        
        for (uint32_t y = 0; y < height; y++) {
            const uint8_t* row = rawData.data() + y * rowPitch;
            for (uint32_t x = 0; x < width; x++) {
                uint32_t pixel = 0;
                std::memcpy(&pixel, row + x * bytesPerPixel, bytesPerPixel);
                
                // Extract and scale to 8-bit
                uint8_t r = 0, g = 0, b = 0, a = 255;
                if (rMask && rBits > 0) {
                    uint32_t rv = (pixel & rMask) >> rShift;
                    r = (rv * 255) / ((1 << rBits) - 1);
                }
                if (gMask && gBits > 0) {
                    uint32_t gv = (pixel & gMask) >> gShift;
                    g = (gv * 255) / ((1 << gBits) - 1);
                }
                if (bMask && bBits > 0) {
                    uint32_t bv = (pixel & bMask) >> bShift;
                    b = (bv * 255) / ((1 << bBits) - 1);
                }
                if (aMask && aBits > 0) {
                    uint32_t av = (pixel & aMask) >> aShift;
                    a = (av * 255) / ((1 << aBits) - 1);
                }
                
                img.setPixel(x, y, qRgba(r, g, b, a));
            }
        }
        return img;
    }
    
    return QImage();
}

#endif // QT_WIDGETS_LIB

} // namespace DDS
