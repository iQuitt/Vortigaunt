#pragma once

#include <cstdint>
#include <vector>
#include <string>

#ifdef QT_WIDGETS_LIB
#include <QImage>
#include <QString>
#endif

namespace DDS {

// ============================================================================
// DDS Header Structures
// ============================================================================

struct DDS_PIXELFORMAT {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwFourCC;
    uint32_t dwRGBBitCount;
    uint32_t dwRBitMask;
    uint32_t dwGBitMask;
    uint32_t dwBBitMask;
    uint32_t dwABitMask;
};

struct DDS_HEADER {
    uint32_t dwSize;
    uint32_t dwFlags;
    uint32_t dwHeight;
    uint32_t dwWidth;
    uint32_t dwPitchOrLinearSize;
    uint32_t dwDepth;
    uint32_t dwMipMapCount;
    uint32_t dwReserved1[11];
    DDS_PIXELFORMAT ddspf;
    uint32_t dwCaps;
    uint32_t dwCaps2;
    uint32_t dwCaps3;
    uint32_t dwCaps4;
    uint32_t dwReserved2;
};

struct DDS_HEADER_DXT10 {
    uint32_t dxgiFormat;
    uint32_t resourceDimension;
    uint32_t miscFlag;
    uint32_t arraySize;
    uint32_t miscFlags2;
};

enum DXGI_FORMAT_SUPPORTED {
    DXGI_FORMAT_BC1_UNORM = 71,
    DXGI_FORMAT_BC1_UNORM_SRGB = 72,
    DXGI_FORMAT_BC2_UNORM = 74,
    DXGI_FORMAT_BC2_UNORM_SRGB = 75,
    DXGI_FORMAT_BC3_UNORM = 77,
    DXGI_FORMAT_BC3_UNORM_SRGB = 78,
};

constexpr uint32_t DDS_MAGIC = 0x20534444;  // "DDS "
constexpr uint32_t FOURCC_DXT1 = 0x31545844; // "DXT1"
constexpr uint32_t FOURCC_DXT2 = 0x32545844; // "DXT2"
constexpr uint32_t FOURCC_DXT3 = 0x33545844; // "DXT3"
constexpr uint32_t FOURCC_DXT4 = 0x34545844; // "DXT4"
constexpr uint32_t FOURCC_DXT5 = 0x35545844; // "DXT5"
constexpr uint32_t FOURCC_DX10 = 0x30315844; // "DX10"
constexpr uint32_t FOURCC_ATI1 = 0x31495441; // "ATI1"
constexpr uint32_t FOURCC_ATI2 = 0x32495441; // "ATI2"

constexpr uint32_t DDPF_ALPHAPIXELS = 0x01;
constexpr uint32_t DDPF_RGB = 0x40;

// ============================================================================
// Decoders and Utility Functions
// ============================================================================

void decodeDXT1Block(const uint8_t* block, uint32_t* output, int x, int y, int width, int height);
void decodeDXT5Block(const uint8_t* block, uint32_t* output, int x, int y, int width, int height);
void decodeDXT1(const uint8_t* src, uint32_t* output, int width, int height);
void decodeDXT5(const uint8_t* src, uint32_t* output, int width, int height);
bool isDdsFile(const std::string& filePath);
std::vector<uint32_t> decodeDdsToPixels(const std::vector<uint8_t>& data, uint32_t& width, uint32_t& height);

#ifdef QT_WIDGETS_LIB
QImage loadDdsFromMemory(const std::vector<uint8_t>& ddsData);
QImage loadDdsToQImage(const QString& filePath);
#endif

} // namespace DDS
