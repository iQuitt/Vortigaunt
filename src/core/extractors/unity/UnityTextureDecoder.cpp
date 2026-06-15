#include "UnityTextureDecoder.h"
#include "UnityAssetParser.h"
#include "VortigauntLog.h"
#include "utils/Bmp.h"
#include "utils/Dds.h"
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <vector>
#include "utils/BinaryUtils.h"

// Ericsson Texture Compression (ETC1) decoding helper
static void decodeETC1Block(const uint8_t* block, uint32_t* output, int x, int y, int width, int height) {
    // Modifier tables
    static const int etc1_modifier_table[8][4] = {
        { 2, 8, -2, -8 },
        { 5, 17, -5, -17 },
        { 9, 29, -9, -29 },
        { 13, 42, -13, -42 },
        { 18, 60, -18, -60 },
        { 24, 80, -24, -80 },
        { 33, 106, -33, -106 },
        { 47, 183, -47, -183 }
    };

    uint32_t colorVal = Utils::ReadBE32(block);
    uint32_t indexVal = Utils::ReadBE32(block + 4);

    bool diffbit = (colorVal & 2) != 0;
    bool flipbit = (colorVal & 1) != 0;

    int r1 = 0, g1 = 0, b1 = 0;
    int r2 = 0, g2 = 0, b2 = 0;

    if (!diffbit) {
        // Individual mode
        r1 = ((colorVal >> 28) & 0xF); r1 = (r1 << 4) | r1;
        g1 = ((colorVal >> 24) & 0xF); g1 = (g1 << 4) | g1;
        b1 = ((colorVal >> 20) & 0xF); b1 = (b1 << 4) | b1;

        r2 = ((colorVal >> 16) & 0xF); r2 = (r2 << 4) | r2;
        g2 = ((colorVal >> 12) & 0xF); g2 = (g2 << 4) | g2;
        b2 = ((colorVal >> 8)  & 0xF); b2 = (b2 << 4) | b2;
    } else {
        // Differential mode
        int R = (colorVal >> 27) & 0x1F;
        int G = (colorVal >> 19) & 0x1F;
        int B = (colorVal >> 11) & 0x1F;

        r1 = (R << 3) | (R >> 2);
        g1 = (G << 3) | (G >> 2);
        b1 = (B << 3) | (B >> 2);

        // 3-bit signed offsets (two's complement)
        int dR = (colorVal >> 24) & 0x7; if (dR & 4) dR -= 8;
        int dG = (colorVal >> 16) & 0x7; if (dG & 4) dG -= 8;
        int dB = (colorVal >> 8)  & 0x7; if (dB & 4) dB -= 8;

        int R2 = R + dR; if (R2 < 0) R2 = 0; if (R2 > 31) R2 = 31;
        int G2 = G + dG; if (G2 < 0) G2 = 0; if (G2 > 31) G2 = 31;
        int B2 = B + dB; if (B2 < 0) B2 = 0; if (B2 > 31) B2 = 31;

        r2 = (R2 << 3) | (R2 >> 2);
        g2 = (G2 << 3) | (G2 >> 2);
        b2 = (B2 << 3) | (B2 >> 2);
    }

    int tableIndex1 = (colorVal >> 5) & 7;
    int tableIndex2 = (colorVal >> 2) & 7;

    const int* table1 = etc1_modifier_table[tableIndex1];
    const int* table2 = etc1_modifier_table[tableIndex2];

    uint32_t msb = indexVal >> 16;
    uint32_t lsb = indexVal & 0xFFFF;

    for (int py = 0; py < 4; ++py) {
        for (int px = 0; px < 4; ++px) {
            int pixelX = x + px;
            int pixelY = y + py;
            if (pixelX >= width || pixelY >= height) continue;

            // Determine which subblock this pixel belongs to
            bool isSecondSubblock = false;
            if (!flipbit) {
                // 2x4 subblocks: left half is subblock 1, right half is subblock 2
                isSecondSubblock = (px >= 2);
            } else {
                // 4x2 subblocks: top half is subblock 1, bottom half is subblock 2
                isSecondSubblock = (py >= 2);
            }

            int baseR = isSecondSubblock ? r2 : r1;
            int baseG = isSecondSubblock ? g2 : g1;
            int baseB = isSecondSubblock ? b2 : b1;
            const int* currentTable = isSecondSubblock ? table2 : table1;

            // Extract index bits
            int bitIndex = px * 4 + py;
            int msbBit = (msb >> bitIndex) & 1;
            int lsbBit = (lsb >> bitIndex) & 1;
            int code = (msbBit << 1) | lsbBit;

            int modifier = currentTable[code];

            int rOut = baseR + modifier; if (rOut < 0) rOut = 0; if (rOut > 255) rOut = 255;
            int gOut = baseG + modifier; if (gOut < 0) gOut = 0; if (gOut > 255) gOut = 255;
            int bOut = baseB + modifier; if (bOut < 0) bOut = 0; if (bOut > 255) bOut = 255;

            // Write ARGB32 pixel
            output[pixelY * width + pixelX] = (255u << 24) | (static_cast<uint32_t>(rOut) << 16) | (static_cast<uint32_t>(gOut) << 8) | static_cast<uint32_t>(bOut);
        }
    }
}

// ETC1 Texture decoding
static void decodeETC1(const uint8_t* src, uint32_t* output, int width, int height) {
    int blockWidth = (width + 3) / 4;
    int blockHeight = (height + 3) / 4;

    for (int by = 0; by < blockHeight; ++by) {
        for (int bx = 0; bx < blockWidth; ++bx) {
            const uint8_t* block = src + (by * blockWidth + bx) * 8;
            decodeETC1Block(block, output, bx * 4, by * 4, width, height);
        }
    }
}




void UnityTextureDecoder::Convert(const UnityObjectInfo& objInfo, const UnityAssetParser& parser, const std::string& outputDir)
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

    // Allocate output buffer for ARGB32 pixels expected by standard indexed 8-bit BMP encoder
    std::vector<uint32_t> argbPixels(width * height, 0);
    bool bigEndian = parser.IsBigEndian();
    
    if (format == 1) { // Alpha8
        for (std::size_t i = 0; i < imageData.size() && i < static_cast<std::size_t>(width * height); ++i) {
            uint32_t a = imageData[i];
            argbPixels[i] = (255u << 24) | (a << 16) | (a << 8) | a;
        }
    }
    else if (format == 2) { // ARGB4444
        for (std::size_t i = 0; i < static_cast<std::size_t>(width * height) && i * 2 + 1 < imageData.size(); ++i) {
            std::uint16_t val = Utils::Read16(imageData.data() + i * 2, bigEndian);
            std::uint8_t a = (val >> 12) & 0xF; a = (a << 4) | a;
            std::uint8_t r = (val >> 8) & 0xF;  r = (r << 4) | r;
            std::uint8_t g = (val >> 4) & 0xF;  g = (g << 4) | g;
            std::uint8_t b = val & 0xF;         b = (b << 4) | b;
            argbPixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    else if (format == 3) { // RGB24
        for (std::size_t i = 0; i < static_cast<std::size_t>(width * height) && i * 3 + 2 < imageData.size(); ++i) {
            uint32_t r = imageData[i * 3];
            uint32_t g = imageData[i * 3 + 1];
            uint32_t b = imageData[i * 3 + 2];
            argbPixels[i] = (255u << 24) | (r << 16) | (g << 8) | b;
        }
    }
    else if (format == 4) { // RGBA32
        for (std::size_t i = 0; i < static_cast<std::size_t>(width * height) && i * 4 + 3 < imageData.size(); ++i) {
            uint32_t r = imageData[i * 4 + 0];
            uint32_t g = imageData[i * 4 + 1];
            uint32_t b = imageData[i * 4 + 2];
            uint32_t a = imageData[i * 4 + 3];
            argbPixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    else if (format == 5) { // ARGB32
        for (std::size_t i = 0; i < static_cast<std::size_t>(width * height) && i * 4 + 3 < imageData.size(); ++i) {
            uint32_t a = imageData[i * 4 + 0];
            uint32_t r = imageData[i * 4 + 1];
            uint32_t g = imageData[i * 4 + 2];
            uint32_t b = imageData[i * 4 + 3];
            argbPixels[i] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }
    else if (format == 7) { // RGB565
        for (std::size_t i = 0; i < static_cast<std::size_t>(width * height) && i * 2 + 1 < imageData.size(); ++i) {
            std::uint16_t val = Utils::Read16(imageData.data() + i * 2, bigEndian);
            std::uint8_t r = (val >> 11) & 0x1F; r = (r << 3) | (r >> 2);
            std::uint8_t g = (val >> 5) & 0x3F;  g = (g << 2) | (g >> 4);
            std::uint8_t b = val & 0x1F;         b = (b << 3) | (b >> 2);
            argbPixels[i] = (255u << 24) | (r << 16) | (g << 8) | b;
        }
    }
    else if (format == 10) { // DXT1
        size_t needed = static_cast<size_t>(((width + 3) / 4) * ((height + 3) / 4) * 8);
        if (imageData.size() >= needed) {
            DDS::decodeDXT1(imageData.data(), argbPixels.data(), static_cast<int>(width), static_cast<int>(height));
        } else {
            VortigauntLog::LogF("WARNING: DXT1 image data size too small for PathID %lld.\n", static_cast<long long>(objInfo.pathId));
        }
    }
    else if (format == 12) { // DXT5
        size_t needed = static_cast<size_t>(((width + 3) / 4) * ((height + 3) / 4) * 16);
        if (imageData.size() >= needed) {
            DDS::decodeDXT5(imageData.data(), argbPixels.data(), static_cast<int>(width), static_cast<int>(height));
        } else {
            VortigauntLog::LogF("WARNING: DXT5 image data size too small for PathID %lld.\n", static_cast<long long>(objInfo.pathId));
        }
    }
    else if (format == 62) { // RG16
        for (std::size_t i = 0; i < static_cast<std::size_t>(width * height) && i * 2 + 1 < imageData.size(); ++i) {
            uint32_t r = imageData[i * 2];
            uint32_t g = imageData[i * 2 + 1];
            argbPixels[i] = (255u << 24) | (r << 16) | (g << 8) | 0;
        }
    }
    else if (format == 63) { // R8
        for (std::size_t i = 0; i < imageData.size() && i < static_cast<std::size_t>(width * height); ++i) {
            uint32_t r = imageData[i];
            argbPixels[i] = (255u << 24) | (r << 16) | 0 | 0;
        }
    }
    else if (format == 24) { // ETC_RGB4
        size_t needed = static_cast<size_t>(((width + 3) / 4) * ((height + 3) / 4) * 8);
        if (imageData.size() >= needed) {
            decodeETC1(imageData.data(), argbPixels.data(), static_cast<int>(width), static_cast<int>(height));
        } else {
            VortigauntLog::LogF("WARNING: ETC_RGB4 image data size too small for PathID %lld.\n", static_cast<long long>(objInfo.pathId));
        }
    }
    else {
        VortigauntLog::LogF("WARNING: Unsupported texture format %lld for PathID %lld. Skipping.\n", static_cast<long long>(format), static_cast<long long>(objInfo.pathId));
        return;
    }

    // Flip rows vertically in-place because Unity textures start from bottom-left, while BMP starts from top-left.
    if (width > 0 && height > 0 && !argbPixels.empty()) {
        for (std::size_t y = 0; y < static_cast<std::size_t>(height / 2); ++y) {
            std::swap_ranges(
                argbPixels.begin() + y * width,
                argbPixels.begin() + (y + 1) * width,
                argbPixels.begin() + (height - 1 - y) * width
            );
        }
    }

    // Prepare BMP output path using Sanitized Object Name
    std::string baseName = objInfo.name.empty() ? std::to_string(objInfo.pathId) : Utils::SanitizeFilename(objInfo.name);
    std::filesystem::path outPath = std::filesystem::path(outputDir) / (baseName + ".bmp");

    bool success = BMP::saveAsIndexed8(outPath.string(), static_cast<int>(width), static_cast<int>(height), argbPixels.data());

    if (!success) {
        VortigauntLog::LogF("ERROR: Failed to encode BMP for PathID %lld\n", static_cast<long long>(objInfo.pathId));
        return;
    }

    VortigauntLog::LogF("Texture converted: %s (Format: %lld)\n", outPath.string().c_str(), static_cast<long long>(format));
}
