// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

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
void decodeDXT3Block(const uint8_t* block, uint32_t* output, int x, int y, int width, int height);
void decodeDXT5Block(const uint8_t* block, uint32_t* output, int x, int y, int width, int height);
void decodeDXT1(const uint8_t* src, uint32_t* output, int width, int height);
void decodeDXT3(const uint8_t* src, uint32_t* output, int width, int height);
void decodeDXT5(const uint8_t* src, uint32_t* output, int width, int height);
bool isDdsFile(const std::string& filePath);
std::vector<uint32_t> decodeDdsToPixels(const std::vector<uint8_t>& data, uint32_t& width, uint32_t& height);

// ============================================================================
// Encoder
// ============================================================================
//
// CSO sprites (SPR version 3) carry one complete DDS file per frame, and they
// are remarkably uniform: of the 1199 such sprites shipped with the game, every
// one of their 30195 frames is DXT5 with no mipmaps. So that is the one thing
// written here, with a header field for field identical to theirs.

// One 4x4 block of RGBA8 pixels (16 pixels, 4 bytes each, row major) into the
// 16 bytes DXT5 stores it as: an interpolated alpha block then a colour block.
void encodeDxt5Block(const uint8_t* rgbaBlock, uint8_t out[16]);

// rgba: width * height * 4 bytes, row major, 8 bits per channel, not
// premultiplied. Width and height must both be multiples of 4 - DXT5 works in
// 4x4 blocks and there is nothing sensible to do with a partial one. Returns an
// empty vector if they are not.
std::vector<uint8_t> encodeDxt5Dds(const uint8_t* rgba, int width, int height);

// The size encodeDxt5Dds will produce: the 128 byte header plus one 16 byte
// block per 4x4 pixels.
size_t dxt5DdsSize(int width, int height);

#ifdef QT_WIDGETS_LIB
QImage loadDdsFromMemory(const std::vector<uint8_t>& ddsData);
QImage loadDdsToQImage(const QString& filePath);
#endif

} // namespace DDS
