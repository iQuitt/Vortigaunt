#include "UnityTextureDecoder.h"
#include "UnityAssetParser.h"
#include "VortigauntLog.h"
#include "utils/Bmp.h"
#include "utils/Png.h"
#include "utils/BinaryUtils.h"

#define BCDEC_IMPLEMENTATION
#include "utils/bcdec.h"
#define ETCDEC_IMPLEMENTATION
#include "utils/etcdec.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace {

// Unity TextureFormat enum values.
namespace TextureFormat {
    constexpr std::int64_t Alpha8             = 1;
    constexpr std::int64_t ARGB4444           = 2;
    constexpr std::int64_t RGB24              = 3;
    constexpr std::int64_t RGBA32             = 4;
    constexpr std::int64_t ARGB32             = 5;
    constexpr std::int64_t RGB565             = 7;
    constexpr std::int64_t R16                = 9;
    constexpr std::int64_t DXT1               = 10;
    constexpr std::int64_t DXT3               = 11;
    constexpr std::int64_t DXT5               = 12;
    constexpr std::int64_t RGBA4444           = 13;
    constexpr std::int64_t BGRA32             = 14;
    constexpr std::int64_t RHalf              = 15;
    constexpr std::int64_t RGHalf             = 16;
    constexpr std::int64_t RGBAHalf           = 17;
    constexpr std::int64_t RFloat             = 18;
    constexpr std::int64_t RGFloat            = 19;
    constexpr std::int64_t RGBAFloat          = 20;
    constexpr std::int64_t YUY2               = 21;
    constexpr std::int64_t RGB9e5Float        = 22;
    constexpr std::int64_t BC6H               = 24;
    constexpr std::int64_t BC7                = 25;
    constexpr std::int64_t BC4                = 26;
    constexpr std::int64_t BC5                = 27;
    constexpr std::int64_t DXT1Crunched       = 28;
    constexpr std::int64_t DXT5Crunched       = 29;
    constexpr std::int64_t PVRTC_RGB2         = 30;
    constexpr std::int64_t PVRTC_RGBA2        = 31;
    constexpr std::int64_t PVRTC_RGB4         = 32;
    constexpr std::int64_t PVRTC_RGBA4        = 33;
    constexpr std::int64_t ETC_RGB4           = 34;
    constexpr std::int64_t EAC_R              = 41;
    constexpr std::int64_t EAC_R_SIGNED       = 42;
    constexpr std::int64_t EAC_RG             = 43;
    constexpr std::int64_t EAC_RG_SIGNED      = 44;
    constexpr std::int64_t ETC2_RGB           = 45;
    constexpr std::int64_t ETC2_RGBA1         = 46;
    constexpr std::int64_t ETC2_RGBA8         = 47;
    constexpr std::int64_t ASTC_RGB_4x4       = 48;
    constexpr std::int64_t ASTC_RGB_5x5       = 49;
    constexpr std::int64_t ASTC_RGB_6x6       = 50;
    constexpr std::int64_t ASTC_RGB_8x8       = 51;
    constexpr std::int64_t ASTC_RGB_10x10     = 52;
    constexpr std::int64_t ASTC_RGB_12x12     = 53;
    constexpr std::int64_t ASTC_RGBA_4x4      = 54;
    constexpr std::int64_t ASTC_RGBA_5x5      = 55;
    constexpr std::int64_t ASTC_RGBA_6x6      = 56;
    constexpr std::int64_t ASTC_RGBA_8x8      = 57;
    constexpr std::int64_t ASTC_RGBA_10x10    = 58;
    constexpr std::int64_t ASTC_RGBA_12x12    = 59;
    constexpr std::int64_t RG16               = 62;
    constexpr std::int64_t R8                 = 63;
    constexpr std::int64_t ETC_RGB4Crunched   = 64;
    constexpr std::int64_t ETC2_RGBA8Crunched = 65;
}

const char* FormatName(std::int64_t format) {
    switch (format) {
        case TextureFormat::Alpha8:             return "Alpha8";
        case TextureFormat::ARGB4444:           return "ARGB4444";
        case TextureFormat::RGB24:              return "RGB24";
        case TextureFormat::RGBA32:             return "RGBA32";
        case TextureFormat::ARGB32:             return "ARGB32";
        case TextureFormat::RGB565:             return "RGB565";
        case TextureFormat::R16:                return "R16";
        case TextureFormat::DXT1:               return "DXT1";
        case TextureFormat::DXT3:               return "DXT3";
        case TextureFormat::DXT5:               return "DXT5";
        case TextureFormat::RGBA4444:           return "RGBA4444";
        case TextureFormat::BGRA32:             return "BGRA32";
        case TextureFormat::RHalf:              return "RHalf";
        case TextureFormat::RGHalf:             return "RGHalf";
        case TextureFormat::RGBAHalf:           return "RGBAHalf";
        case TextureFormat::RFloat:             return "RFloat";
        case TextureFormat::RGFloat:            return "RGFloat";
        case TextureFormat::RGBAFloat:          return "RGBAFloat";
        case TextureFormat::YUY2:               return "YUY2";
        case TextureFormat::RGB9e5Float:        return "RGB9e5Float";
        case TextureFormat::BC6H:               return "BC6H";
        case TextureFormat::BC7:                return "BC7";
        case TextureFormat::BC4:                return "BC4";
        case TextureFormat::BC5:                return "BC5";
        case TextureFormat::DXT1Crunched:       return "DXT1Crunched";
        case TextureFormat::DXT5Crunched:       return "DXT5Crunched";
        case TextureFormat::PVRTC_RGB2:         return "PVRTC_RGB2";
        case TextureFormat::PVRTC_RGBA2:        return "PVRTC_RGBA2";
        case TextureFormat::PVRTC_RGB4:         return "PVRTC_RGB4";
        case TextureFormat::PVRTC_RGBA4:        return "PVRTC_RGBA4";
        case TextureFormat::ETC_RGB4:           return "ETC_RGB4";
        case TextureFormat::EAC_R:              return "EAC_R";
        case TextureFormat::EAC_R_SIGNED:       return "EAC_R_SIGNED";
        case TextureFormat::EAC_RG:             return "EAC_RG";
        case TextureFormat::EAC_RG_SIGNED:      return "EAC_RG_SIGNED";
        case TextureFormat::ETC2_RGB:           return "ETC2_RGB";
        case TextureFormat::ETC2_RGBA1:         return "ETC2_RGBA1";
        case TextureFormat::ETC2_RGBA8:         return "ETC2_RGBA8";
        case TextureFormat::ASTC_RGB_4x4:       return "ASTC_RGB_4x4";
        case TextureFormat::ASTC_RGB_5x5:       return "ASTC_RGB_5x5";
        case TextureFormat::ASTC_RGB_6x6:       return "ASTC_RGB_6x6";
        case TextureFormat::ASTC_RGB_8x8:       return "ASTC_RGB_8x8";
        case TextureFormat::ASTC_RGB_10x10:     return "ASTC_RGB_10x10";
        case TextureFormat::ASTC_RGB_12x12:     return "ASTC_RGB_12x12";
        case TextureFormat::ASTC_RGBA_4x4:      return "ASTC_RGBA_4x4";
        case TextureFormat::ASTC_RGBA_5x5:      return "ASTC_RGBA_5x5";
        case TextureFormat::ASTC_RGBA_6x6:      return "ASTC_RGBA_6x6";
        case TextureFormat::ASTC_RGBA_8x8:      return "ASTC_RGBA_8x8";
        case TextureFormat::ASTC_RGBA_10x10:    return "ASTC_RGBA_10x10";
        case TextureFormat::ASTC_RGBA_12x12:    return "ASTC_RGBA_12x12";
        case TextureFormat::RG16:               return "RG16";
        case TextureFormat::R8:                 return "R8";
        case TextureFormat::ETC_RGB4Crunched:   return "ETC_RGB4Crunched";
        case TextureFormat::ETC2_RGBA8Crunched: return "ETC2_RGBA8Crunched";
        default:                                return "Unknown";
    }
}

inline std::uint32_t PackARGB(std::uint32_t a, std::uint32_t r, std::uint32_t g, std::uint32_t b) {
    return (a << 24) | (r << 16) | (g << 8) | b;
}

constexpr std::uint32_t Expand4(std::uint32_t n) { n &= 0xF;  return (n << 4) | n; }
constexpr std::uint32_t Expand5(std::uint32_t n) { n &= 0x1F; return (n << 3) | (n >> 2); }
constexpr std::uint32_t Expand6(std::uint32_t n) { n &= 0x3F; return (n << 2) | (n >> 4); }

// Clamps to [0,1] and scales to a byte. NaN maps to 0.
std::uint8_t FloatToByte(float v) {
    if (!(v > 0.0f)) {
        return 0;
    }
    if (v >= 1.0f) {
        return 255;
    }
    return static_cast<std::uint8_t>(v * 255.0f + 0.5f);
}

float ReadFloat32(const std::uint8_t* p, bool bigEndian) {
    std::uint32_t bits = Utils::Read32(p, bigEndian);
    float f = 0.0f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

std::uint8_t HalfToByte(const std::uint8_t* p, bool bigEndian) {
    return FloatToByte(Utils::Float16ToFloat32(Utils::Read16(p, bigEndian)));
}

// Decodes tightly packed per-pixel data. Stops when either the source bytes or
// the destination pixels run out, so truncated data cannot overrun.
template <typename PixelFn>
void DecodeRaw(const std::vector<std::uint8_t>& src, std::size_t bytesPerPixel,
               std::uint32_t* dst, std::size_t pixelCount, PixelFn&& pixel)
{
    const std::size_t count = std::min(pixelCount, src.size() / bytesPerPixel);
    const std::uint8_t* p = src.data();
    for (std::size_t i = 0; i < count; ++i, p += bytesPerPixel) {
        dst[i] = pixel(p);
    }
}

// Generic driver for 4x4 block-compressed formats. blockFn decodes one compressed
// block into a 16-pixel RGBA8 staging buffer; only pixels inside the image are
// copied out, which handles non-multiple-of-4 dimensions. Returns false when the
// source data is too small for the expected block count.
template <typename BlockFn>
bool DecodeBlocks(const std::vector<std::uint8_t>& src, std::size_t blockSize,
                  int width, int height, std::uint32_t* dst, BlockFn&& blockFn)
{
    const int blocksX = (width + 3) / 4;
    const int blocksY = (height + 3) / 4;
    const std::size_t blockCount = static_cast<std::size_t>(blocksX) * static_cast<std::size_t>(blocksY);
    if (src.size() < blockCount * blockSize) {
        return false;
    }

    std::uint8_t staging[16 * 4];
    const std::uint8_t* block = src.data();
    for (int by = 0; by < blocksY; ++by) {
        const int copyH = std::min(4, height - by * 4);
        for (int bx = 0; bx < blocksX; ++bx, block += blockSize) {
            blockFn(block, staging);
            const int copyW = std::min(4, width - bx * 4);
            for (int py = 0; py < copyH; ++py) {
                const std::uint8_t* s = staging + py * 16;
                std::uint32_t* d = dst + (static_cast<std::size_t>(by) * 4 + py) * static_cast<std::size_t>(width)
                                       + static_cast<std::size_t>(bx) * 4;
                for (int px = 0; px < copyW; ++px) {
                    d[px] = PackARGB(s[px * 4 + 3], s[px * 4 + 0], s[px * 4 + 1], s[px * 4 + 2]);
                }
            }
        }
    }
    return true;
}

} // namespace

void UnityTextureDecoder::Convert(const UnityObjectInfo& objInfo, const UnityAssetParser& parser, const std::string& outputDir, bool convertToBmp)
{
    UnityValue root = parser.DeserializeObject(objInfo);

    std::int64_t width = 0, height = 0;
    std::int64_t format = 0;
    std::vector<std::uint8_t> imageData;
    try {
        width = root.GetField("m_Width").AsInt();
        height = root.GetField("m_Height").AsInt();
        format = root.GetField("m_TextureFormat").AsInt();
        const UnityValue& imgVal = root.GetField("image data");
        imageData = imgVal.AsByteArray();
    } catch (const std::exception& e) {
        VortigauntLog::LogF("ERROR: Missing texture fields for PathID %lld: %s\n", static_cast<long long>(objInfo.pathId), e.what());
        return;
    }

    // Dimensions come from untrusted data; cap each dimension before multiplying
    // (so the product cannot overflow int64), then cap at 256M pixels before any allocation.
    if (width <= 0 || height <= 0 || width > 32768 || height > 32768 || width * height > 268435456) {
        VortigauntLog::LogF("ERROR: Invalid texture dimensions %lldx%lld for PathID %lld.\n",
            static_cast<long long>(width), static_cast<long long>(height), static_cast<long long>(objInfo.pathId));
        return;
    }

    if (imageData.empty()) {
        const UnityValue& streamData = root.GetField("m_StreamData");
        if (!streamData.IsNull()) {
            std::uint64_t offset = 0;
            std::uint32_t size = 0;
            std::string path;

            const UnityValue& offsetVal = streamData.GetField("offset").IsNull() ? streamData.GetField("m_Offset") : streamData.GetField("offset");
            const UnityValue& sizeVal = streamData.GetField("size").IsNull() ? streamData.GetField("m_Size") : streamData.GetField("size");
            const UnityValue& pathVal = streamData.GetField("path").IsNull() ? streamData.GetField("m_Path") : streamData.GetField("path");

            offset = offsetVal.AsUInt();
            size = static_cast<std::uint32_t>(sizeVal.AsUInt());
            path = pathVal.AsString();

            if (size > 0 && !path.empty()) {
                std::filesystem::path p(path);
                std::string filename = p.filename().string();

                const UnityDataResolver* resolver = parser.GetDataResolver();
                if (resolver) {
                    if (!resolver->ReadData(filename, offset, size, imageData)) {
                        VortigauntLog::LogF("ERROR: Failed to read streamed companion data file %s. Offset: %llu, Size: %u\n",
                            filename.c_str(), static_cast<unsigned long long>(offset), size);
                    }
                } else {
                    VortigauntLog::LogF("ERROR: No UnityDataResolver set in parser. Cannot resolve companion files.\n");
                }
            }
        }
    }

    if (imageData.empty()) {
        VortigauntLog::LogF("ERROR: Texture pixel data is empty for PathID %lld.\n", static_cast<long long>(objInfo.pathId));
        return;
    }

    // Decode into ARGB32 pixels expected by the indexed 8-bit BMP encoder.
    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    std::vector<std::uint32_t> argbPixels(pixelCount, 0);
    std::uint32_t* dst = argbPixels.data();
    const bool bigEndian = parser.IsBigEndian();
    const int w = static_cast<int>(width);
    const int h = static_cast<int>(height);
    bool sizeOk = true;

    switch (format) {
    case TextureFormat::Alpha8:
        DecodeRaw(imageData, 1, dst, pixelCount, [](const std::uint8_t* p) {
            const std::uint32_t a = p[0];
            return PackARGB(255, a, a, a);
        });
        break;
    case TextureFormat::ARGB4444:
        DecodeRaw(imageData, 2, dst, pixelCount, [bigEndian](const std::uint8_t* p) {
            const std::uint16_t v = Utils::Read16(p, bigEndian);
            return PackARGB(Expand4(v >> 12), Expand4(v >> 8), Expand4(v >> 4), Expand4(v));
        });
        break;
    case TextureFormat::RGB24:
        DecodeRaw(imageData, 3, dst, pixelCount, [](const std::uint8_t* p) {
            return PackARGB(255, p[0], p[1], p[2]);
        });
        break;
    case TextureFormat::RGBA32:
        DecodeRaw(imageData, 4, dst, pixelCount, [](const std::uint8_t* p) {
            return PackARGB(p[3], p[0], p[1], p[2]);
        });
        break;
    case TextureFormat::ARGB32:
        DecodeRaw(imageData, 4, dst, pixelCount, [](const std::uint8_t* p) {
            return PackARGB(p[0], p[1], p[2], p[3]);
        });
        break;
    case TextureFormat::RGB565:
        DecodeRaw(imageData, 2, dst, pixelCount, [bigEndian](const std::uint8_t* p) {
            const std::uint16_t v = Utils::Read16(p, bigEndian);
            return PackARGB(255, Expand5(v >> 11), Expand6(v >> 5), Expand5(v));
        });
        break;
    case TextureFormat::R16:
        DecodeRaw(imageData, 2, dst, pixelCount, [bigEndian](const std::uint8_t* p) {
            const std::uint32_t gray = Utils::Read16(p, bigEndian) >> 8;
            return PackARGB(255, gray, gray, gray);
        });
        break;
    case TextureFormat::RGBA4444:
        DecodeRaw(imageData, 2, dst, pixelCount, [bigEndian](const std::uint8_t* p) {
            const std::uint16_t v = Utils::Read16(p, bigEndian);
            return PackARGB(Expand4(v), Expand4(v >> 12), Expand4(v >> 8), Expand4(v >> 4));
        });
        break;
    case TextureFormat::BGRA32:
        DecodeRaw(imageData, 4, dst, pixelCount, [](const std::uint8_t* p) {
            return PackARGB(p[3], p[2], p[1], p[0]);
        });
        break;
    case TextureFormat::RHalf:
        DecodeRaw(imageData, 2, dst, pixelCount, [bigEndian](const std::uint8_t* p) {
            const std::uint32_t gray = HalfToByte(p, bigEndian);
            return PackARGB(255, gray, gray, gray);
        });
        break;
    case TextureFormat::RGHalf:
        DecodeRaw(imageData, 4, dst, pixelCount, [bigEndian](const std::uint8_t* p) {
            return PackARGB(255, HalfToByte(p, bigEndian), HalfToByte(p + 2, bigEndian), 0);
        });
        break;
    case TextureFormat::RGBAHalf:
        DecodeRaw(imageData, 8, dst, pixelCount, [bigEndian](const std::uint8_t* p) {
            return PackARGB(HalfToByte(p + 6, bigEndian), HalfToByte(p, bigEndian),
                            HalfToByte(p + 2, bigEndian), HalfToByte(p + 4, bigEndian));
        });
        break;
    case TextureFormat::RFloat:
        DecodeRaw(imageData, 4, dst, pixelCount, [bigEndian](const std::uint8_t* p) {
            const std::uint32_t gray = FloatToByte(ReadFloat32(p, bigEndian));
            return PackARGB(255, gray, gray, gray);
        });
        break;
    case TextureFormat::RGFloat:
        DecodeRaw(imageData, 8, dst, pixelCount, [bigEndian](const std::uint8_t* p) {
            return PackARGB(255, FloatToByte(ReadFloat32(p, bigEndian)),
                            FloatToByte(ReadFloat32(p + 4, bigEndian)), 0);
        });
        break;
    case TextureFormat::RGBAFloat:
        DecodeRaw(imageData, 16, dst, pixelCount, [bigEndian](const std::uint8_t* p) {
            return PackARGB(FloatToByte(ReadFloat32(p + 12, bigEndian)), FloatToByte(ReadFloat32(p, bigEndian)),
                            FloatToByte(ReadFloat32(p + 4, bigEndian)), FloatToByte(ReadFloat32(p + 8, bigEndian)));
        });
        break;
    case TextureFormat::RGB9e5Float:
        DecodeRaw(imageData, 4, dst, pixelCount, [bigEndian](const std::uint8_t* p) {
            const std::uint32_t v = Utils::Read32(p, bigEndian);
            const float scale = std::ldexp(1.0f, static_cast<int>(v >> 27) - 24);
            return PackARGB(255,
                FloatToByte(static_cast<float>(v & 0x1FF) * scale),
                FloatToByte(static_cast<float>((v >> 9) & 0x1FF) * scale),
                FloatToByte(static_cast<float>((v >> 18) & 0x1FF) * scale));
        });
        break;
    case TextureFormat::RG16:
        DecodeRaw(imageData, 2, dst, pixelCount, [](const std::uint8_t* p) {
            return PackARGB(255, p[0], p[1], 0);
        });
        break;
    case TextureFormat::R8:
        DecodeRaw(imageData, 1, dst, pixelCount, [](const std::uint8_t* p) {
            return PackARGB(255, p[0], 0, 0);
        });
        break;

    case TextureFormat::DXT1:
        sizeOk = DecodeBlocks(imageData, BCDEC_BC1_BLOCK_SIZE, w, h, dst,
            [](const std::uint8_t* blk, std::uint8_t* out) { bcdec_bc1(blk, out, 16); });
        break;
    case TextureFormat::DXT3:
        sizeOk = DecodeBlocks(imageData, BCDEC_BC2_BLOCK_SIZE, w, h, dst,
            [](const std::uint8_t* blk, std::uint8_t* out) { bcdec_bc2(blk, out, 16); });
        break;
    case TextureFormat::DXT5:
        sizeOk = DecodeBlocks(imageData, BCDEC_BC3_BLOCK_SIZE, w, h, dst,
            [](const std::uint8_t* blk, std::uint8_t* out) { bcdec_bc3(blk, out, 16); });
        break;
    case TextureFormat::BC4:
        sizeOk = DecodeBlocks(imageData, BCDEC_BC4_BLOCK_SIZE, w, h, dst,
            [](const std::uint8_t* blk, std::uint8_t* out) {
                std::uint8_t r[16];
                bcdec_bc4(blk, r, 4);
                for (int i = 0; i < 16; ++i) {
                    out[i * 4 + 0] = r[i];
                    out[i * 4 + 1] = r[i];
                    out[i * 4 + 2] = r[i];
                    out[i * 4 + 3] = 255;
                }
            });
        break;
    case TextureFormat::BC5:
        sizeOk = DecodeBlocks(imageData, BCDEC_BC5_BLOCK_SIZE, w, h, dst,
            [](const std::uint8_t* blk, std::uint8_t* out) {
                std::uint8_t rg[16 * 2];
                bcdec_bc5(blk, rg, 8);
                for (int i = 0; i < 16; ++i) {
                    out[i * 4 + 0] = rg[i * 2 + 0];
                    out[i * 4 + 1] = rg[i * 2 + 1];
                    out[i * 4 + 2] = 0;
                    out[i * 4 + 3] = 255;
                }
            });
        break;
    case TextureFormat::BC6H:
        sizeOk = DecodeBlocks(imageData, BCDEC_BC6H_BLOCK_SIZE, w, h, dst,
            [](const std::uint8_t* blk, std::uint8_t* out) {
                float rgb[16 * 3];
                // bcdec_bc6h_float takes its pitch in floats (3 per pixel).
                bcdec_bc6h_float(blk, rgb, 4 * 3, 0);
                for (int i = 0; i < 16; ++i) {
                    out[i * 4 + 0] = FloatToByte(rgb[i * 3 + 0]);
                    out[i * 4 + 1] = FloatToByte(rgb[i * 3 + 1]);
                    out[i * 4 + 2] = FloatToByte(rgb[i * 3 + 2]);
                    out[i * 4 + 3] = 255;
                }
            });
        break;
    case TextureFormat::BC7:
        sizeOk = DecodeBlocks(imageData, BCDEC_BC7_BLOCK_SIZE, w, h, dst,
            [](const std::uint8_t* blk, std::uint8_t* out) { bcdec_bc7(blk, out, 16); });
        break;

    case TextureFormat::ETC_RGB4:
    case TextureFormat::ETC2_RGB:
        sizeOk = DecodeBlocks(imageData, ETCDEC_ETC_RGB_BLOCK_SIZE, w, h, dst,
            [](const std::uint8_t* blk, std::uint8_t* out) { etcdec_etc_rgb(blk, out, 16); });
        break;
    case TextureFormat::ETC2_RGBA1:
        sizeOk = DecodeBlocks(imageData, ETCDEC_ETC_RGB_A1_BLOCK_SIZE, w, h, dst,
            [](const std::uint8_t* blk, std::uint8_t* out) { etcdec_etc_rgb_a1(blk, out, 16); });
        break;
    case TextureFormat::ETC2_RGBA8:
        sizeOk = DecodeBlocks(imageData, ETCDEC_EAC_RGBA_BLOCK_SIZE, w, h, dst,
            [](const std::uint8_t* blk, std::uint8_t* out) { etcdec_eac_rgba(blk, out, 16); });
        break;
    case TextureFormat::EAC_R:
        sizeOk = DecodeBlocks(imageData, ETCDEC_EAC_R11_BLOCK_SIZE, w, h, dst,
            [](const std::uint8_t* blk, std::uint8_t* out) {
                std::uint16_t r[16];
                etcdec_eac_r11_u16(blk, r, 4 * 2);
                for (int i = 0; i < 16; ++i) {
                    const std::uint8_t gray = static_cast<std::uint8_t>(r[i] >> 8);
                    out[i * 4 + 0] = gray;
                    out[i * 4 + 1] = gray;
                    out[i * 4 + 2] = gray;
                    out[i * 4 + 3] = 255;
                }
            });
        break;
    case TextureFormat::EAC_RG:
        sizeOk = DecodeBlocks(imageData, ETCDEC_EAC_RG11_BLOCK_SIZE, w, h, dst,
            [](const std::uint8_t* blk, std::uint8_t* out) {
                std::uint16_t rg[16 * 2];
                etcdec_eac_rg11_u16(blk, rg, 4 * 4);
                for (int i = 0; i < 16; ++i) {
                    out[i * 4 + 0] = static_cast<std::uint8_t>(rg[i * 2 + 0] >> 8);
                    out[i * 4 + 1] = static_cast<std::uint8_t>(rg[i * 2 + 1] >> 8);
                    out[i * 4 + 2] = 0;
                    out[i * 4 + 3] = 255;
                }
            });
        break;

    case TextureFormat::DXT1Crunched:
    case TextureFormat::DXT5Crunched:
    case TextureFormat::ETC_RGB4Crunched:
    case TextureFormat::ETC2_RGBA8Crunched:
        VortigauntLog::LogF("WARNING: %s texture for PathID %lld: Crunched textures require the crunch library - not supported. Skipping.\n",
            FormatName(format), static_cast<long long>(objInfo.pathId));
        return;
    case TextureFormat::PVRTC_RGB2:
    case TextureFormat::PVRTC_RGBA2:
    case TextureFormat::PVRTC_RGB4:
    case TextureFormat::PVRTC_RGBA4:
        VortigauntLog::LogF("WARNING: %s texture for PathID %lld: PVRTC decoding is not supported. Skipping.\n",
            FormatName(format), static_cast<long long>(objInfo.pathId));
        return;
    case TextureFormat::ASTC_RGB_4x4:
    case TextureFormat::ASTC_RGB_5x5:
    case TextureFormat::ASTC_RGB_6x6:
    case TextureFormat::ASTC_RGB_8x8:
    case TextureFormat::ASTC_RGB_10x10:
    case TextureFormat::ASTC_RGB_12x12:
    case TextureFormat::ASTC_RGBA_4x4:
    case TextureFormat::ASTC_RGBA_5x5:
    case TextureFormat::ASTC_RGBA_6x6:
    case TextureFormat::ASTC_RGBA_8x8:
    case TextureFormat::ASTC_RGBA_10x10:
    case TextureFormat::ASTC_RGBA_12x12:
        VortigauntLog::LogF("WARNING: %s texture for PathID %lld: ASTC decoding is not supported. Skipping.\n",
            FormatName(format), static_cast<long long>(objInfo.pathId));
        return;
    case TextureFormat::YUY2:
        VortigauntLog::LogF("WARNING: %s texture for PathID %lld: YUY2 decoding is not supported. Skipping.\n",
            FormatName(format), static_cast<long long>(objInfo.pathId));
        return;
    case TextureFormat::EAC_R_SIGNED:
    case TextureFormat::EAC_RG_SIGNED:
        VortigauntLog::LogF("WARNING: %s texture for PathID %lld: signed EAC decoding is not supported. Skipping.\n",
            FormatName(format), static_cast<long long>(objInfo.pathId));
        return;
    default:
        VortigauntLog::LogF("WARNING: Unknown texture format %lld for PathID %lld. Skipping.\n",
            static_cast<long long>(format), static_cast<long long>(objInfo.pathId));
        return;
    }

    if (!sizeOk) {
        VortigauntLog::LogF("WARNING: %s image data size too small for PathID %lld. Skipping.\n",
            FormatName(format), static_cast<long long>(objInfo.pathId));
        return;
    }

    // Flip rows vertically in-place because Unity textures start from bottom-left, while BMP starts from top-left.
    if (!argbPixels.empty()) {
        for (std::size_t y = 0; y < static_cast<std::size_t>(height / 2); ++y) {
            std::swap_ranges(
                argbPixels.begin() + y * width,
                argbPixels.begin() + (y + 1) * width,
                argbPixels.begin() + (height - 1 - y) * width
            );
        }
    }

    // Prepare output path using Sanitized Object Name
    std::string baseName = objInfo.name.empty() ? std::to_string(objInfo.pathId) : Utils::SanitizeFilename(objInfo.name);
    std::filesystem::path outPath = std::filesystem::path(outputDir) / (baseName + (convertToBmp ? ".bmp" : ".png"));

    bool success = convertToBmp
        ? BMP::saveAsIndexed8(outPath.string(), w, h, argbPixels.data())
        : PNG::saveArgb(outPath.string(), w, h, argbPixels.data());

    if (!success) {
        VortigauntLog::LogF("ERROR: Failed to encode %s for PathID %lld\n",
            convertToBmp ? "BMP" : "PNG", static_cast<long long>(objInfo.pathId));
        return;
    }

    VortigauntLog::LogF("Texture converted: %s (Format: %s)\n", outPath.string().c_str(), FormatName(format));
}
