#include "WadMaker.h"
#include "core/VortigauntLog.h"
#include "utils/Bmp.h"
#include "utils/FileIO.h"
#include "utils/ImageUtils.h"
#include "libimagequant.h"


#ifdef QT_CORE_LIB
#include <QImage>
#include <QFileInfo>
#define HAS_QT_IMAGE_SUPPORT 1
#else
#define HAS_QT_IMAGE_SUPPORT 0
#endif

#include <fstream>
#include <algorithm>
#include <cstring>
#include <cmath>


using namespace VortigauntLog;



WadArchive::WadArchive()
{
}

WadArchive::~WadArchive()
{
}

void WadArchive::clear()
{
    m_textures.clear();
    m_currentWadPath.clear();
}

std::string WadArchive::getFileExtension(const std::string& path) const
{
    size_t dotPos = path.rfind('.');
    if (dotPos == std::string::npos || dotPos == path.length() - 1)
        return "";
    
    std::string ext = path.substr(dotPos + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    return ext;
}

std::string WadArchive::sanitizeTextureName(const std::string& name) const
{
    std::string result;
    result.reserve(15);
    
    for (char c : name)
    {
        if (result.length() >= 15) break;
        
        // Convert to uppercase and keep only valid characters
        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-')
        {
            result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        else if (c == ' ')
        {
            result += '_';
        }
    }
    
    if (result.empty())
    {
        result = "UNNAMED";
    }
    
    return result;
}

void WadArchive::generateMipmaps(WadTexture& texture)
{
    // We need RGB data to generate high-quality mipmaps via averaging.
    // If we only have indexed data, we'll convert it back to RGB using the palette,
    // average the colors, and then find the nearest palette match.
    
    auto generateMip = [&](const std::vector<uint8_t>& src, uint32_t sw, uint32_t sh, 
                           std::vector<uint8_t>& dst, uint32_t dw, uint32_t dh) {
        dst.resize(dw * dh);
        for (uint32_t y = 0; y < dh; ++y)
        {
            for (uint32_t x = 0; x < dw; ++x)
            {
                // Box filter: average 2x2 pixels
                uint32_t r = 0, g = 0, b = 0;
                int count = 0;
                
                for (uint32_t sy = y * 2; sy < (y * 2 + 2) && sy < sh; ++sy)
                {
                    for (uint32_t sx = x * 2; sx < (x * 2 + 2) && sx < sw; ++sx)
                    {
                        uint8_t idx = src[sy * sw + sx];
                        r += texture.palette[idx * 3 + 0];
                        g += texture.palette[idx * 3 + 1];
                        b += texture.palette[idx * 3 + 2];
                        count++;
                    }
                }
                
                if (count > 0)
                {
                    r /= count;
                    g /= count;
                    b /= count;
                }
                
                // Find nearest color in palette
                int bestIdx = 0;
                int minDist = 1000000;
                
                // For GoldSrc, index 255 might be transparent ({ textures), 
                // but here we just find the closest match.
                for (int i = 0; i < 256; ++i)
                {
                    int dr = (int)r - texture.palette[i * 3 + 0];
                    int dg = (int)g - texture.palette[i * 3 + 1];
                    int db = (int)b - texture.palette[i * 3 + 2];
                    int dist = dr*dr + dg*dg + db*db;
                    if (dist < minDist)
                    {
                        minDist = dist;
                        bestIdx = i;
                    }
                }
                dst[y * dw + x] = (uint8_t)bestIdx;
            }
        }
    };

    uint32_t w1 = std::max(1u, texture.width / 2);
    uint32_t h1 = std::max(1u, texture.height / 2);
    generateMip(texture.mip0, texture.width, texture.height, texture.mip1, w1, h1);

    uint32_t w2 = std::max(1u, texture.width / 4);
    uint32_t h2 = std::max(1u, texture.height / 4);
    generateMip(texture.mip1, w1, h1, texture.mip2, w2, h2);

    uint32_t w3 = std::max(1u, texture.width / 8);
    uint32_t h3 = std::max(1u, texture.height / 8);
    generateMip(texture.mip2, w2, h2, texture.mip3, w3, h3);
}

bool WadArchive::quantizeToIndexed(const uint8_t* rgbData, uint32_t width, uint32_t height,
                                 std::vector<uint8_t>& indexedData, uint8_t* palette)
{
    // Use libimagequant for high-quality palette generation (same as BMP::saveAsIndexed8 / SpriteLoader)
    uint32_t pixelCount = width * height;

    // Convert RGB -> RGBA (libimagequant requires 4 channels)
    std::vector<uint8_t> rgbaBuffer(pixelCount * 4);
    for (uint32_t i = 0; i < pixelCount; ++i)
    {
        rgbaBuffer[i * 4 + 0] = rgbData[i * 3 + 0]; // R
        rgbaBuffer[i * 4 + 1] = rgbData[i * 3 + 1]; // G
        rgbaBuffer[i * 4 + 2] = rgbData[i * 3 + 2]; // B
        rgbaBuffer[i * 4 + 3] = 255;                 // A (fully opaque)
    }

    liq_attr* attr = liq_attr_create();
    if (!attr) return false;

    liq_set_speed(attr, 1); // Max quality
    liq_set_max_colors(attr, 256);

    liq_image* img = liq_image_create_rgba(attr, rgbaBuffer.data(),
                                           static_cast<int>(width),
                                           static_cast<int>(height), 0);
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

    // Extract palette entries as RGB (WadTexture stores 3 bytes/color)
    const liq_palette* pal = liq_get_palette(res);
    std::memset(palette, 0, 768);
    for (unsigned int i = 0; i < pal->count; ++i)
    {
        palette[i * 3 + 0] = pal->entries[i].r;
        palette[i * 3 + 1] = pal->entries[i].g;
        palette[i * 3 + 2] = pal->entries[i].b;
    }

    // Write remapped (indexed) pixel data
    indexedData.resize(pixelCount);
    if (liq_write_remapped_image(res, img, indexedData.data(), pixelCount) != LIQ_OK)
    {
        liq_result_destroy(res);
        liq_image_destroy(img);
        liq_attr_destroy(attr);
        return false;
    }

    liq_result_destroy(res);
    liq_image_destroy(img);
    liq_attr_destroy(attr);
    return true;
}

bool WadArchive::resizeAndValidate(WadTexture& texture)
{
    // Enforce maximum texture size (512x512 for GoldSrc)
    if (texture.width > WadConstants::MAX_TEXTURE_SIZE || 
        texture.height > WadConstants::MAX_TEXTURE_SIZE)
    {

		LogF("Resizing texture from %ux%u to fit 512x512 limit", texture.width, texture.height);
        
        float scaleX = static_cast<float>(WadConstants::MAX_TEXTURE_SIZE) / texture.width;
        float scaleY = static_cast<float>(WadConstants::MAX_TEXTURE_SIZE) / texture.height;
        float scale = std::min(scaleX, scaleY);
        
        uint32_t newWidth = static_cast<uint32_t>(texture.width * scale);
        uint32_t newHeight = static_cast<uint32_t>(texture.height * scale);
        
        // Round to multiples of 16
        newWidth = ((newWidth + 15) / 16) * 16;
        newHeight = ((newHeight + 15) / 16) * 16;
        
        if (newWidth < 16) newWidth = 16;
        if (newHeight < 16) newHeight = 16;
        
        std::vector<uint8_t> newMip0(newWidth * newHeight);
        for (uint32_t y = 0; y < newHeight; ++y)
        {
            for (uint32_t x = 0; x < newWidth; ++x)
            {
                uint32_t srcX = std::min(x * texture.width / newWidth, texture.width - 1);
                uint32_t srcY = std::min(y * texture.height / newHeight, texture.height - 1);
                newMip0[y * newWidth + x] = texture.mip0[srcY * texture.width + srcX];
            }
        }
        
        texture.width = newWidth;
        texture.height = newHeight;
        texture.mip0 = std::move(newMip0);
        
        Vortigaunt_Printf("Resized to " + std::to_string(texture.width) + "x" + std::to_string(texture.height));
    }
    
    // Generate mipmaps
    generateMipmaps(texture);
    
    return true;
}


bool WadArchive::loadBmp(const std::string& path, WadTexture& texture)
{
    std::ifstream file(FileIO::toPath(path), std::ios::binary);
    if (!file)
    {
        Vortigaunt_Printf("ERROR: Cannot open BMP file: " + path);
        return false;
    }
    
    uint8_t header[54];
    file.read(reinterpret_cast<char*>(header), 54);
    
    if (header[0] != 'B' || header[1] != 'M')
    {
        Vortigaunt_Printf("ERROR: Invalid BMP signature: " + path);
        return false;
    }
    
    uint32_t dataOffset = *reinterpret_cast<uint32_t*>(&header[10]);
    uint32_t width = *reinterpret_cast<uint32_t*>(&header[18]);
    uint32_t height = *reinterpret_cast<uint32_t*>(&header[22]);
    uint16_t bitsPerPixel = *reinterpret_cast<uint16_t*>(&header[28]);
    
    // Round up to nearest 16
    uint32_t origWidth = width;
    uint32_t origHeight = height;
    if (width % 16 != 0) width = ((width + 15) / 16) * 16;
    if (height % 16 != 0) height = ((height + 15) / 16) * 16;
    
    texture.width = width;
    texture.height = height;
    
    if (bitsPerPixel == 8)
    {
        file.seekg(54);
        uint8_t paletteBgra[1024];
        file.read(reinterpret_cast<char*>(paletteBgra), 1024);
        
        for (int i = 0; i < 256; ++i)
        {
            texture.palette[i * 3 + 0] = paletteBgra[i * 4 + 2];
            texture.palette[i * 3 + 1] = paletteBgra[i * 4 + 1];
            texture.palette[i * 3 + 2] = paletteBgra[i * 4 + 0];
        }
        
        file.seekg(dataOffset);
        uint32_t rowSize = ((origWidth + 3) / 4) * 4;
        texture.mip0.resize(width * height, 0);
        
        for (uint32_t y = 0; y < origHeight; ++y)
        {
            std::vector<uint8_t> row(rowSize);
            file.read(reinterpret_cast<char*>(row.data()), rowSize);
            
            uint32_t destY = height - 1 - y;
            if (destY < height)
            {
                for (uint32_t x = 0; x < origWidth && x < width; ++x)
                {
                    texture.mip0[destY * width + x] = row[x];
                }
            }
        }
    }
    else if (bitsPerPixel == 24 || bitsPerPixel == 32)
    {
        file.seekg(dataOffset);
        int bytesPerPixel = bitsPerPixel / 8;
        uint32_t rowSize = ((origWidth * bytesPerPixel + 3) / 4) * 4;
        
        std::vector<uint8_t> rgbData(width * height * 3, 0);
        
        for (uint32_t y = 0; y < origHeight; ++y)
        {
            std::vector<uint8_t> row(rowSize);
            file.read(reinterpret_cast<char*>(row.data()), rowSize);
            
            uint32_t destY = height - 1 - y;
            if (destY < height)
            {
                for (uint32_t x = 0; x < origWidth && x < width; ++x)
                {
                    rgbData[(destY * width + x) * 3 + 0] = row[x * bytesPerPixel + 2];
                    rgbData[(destY * width + x) * 3 + 1] = row[x * bytesPerPixel + 1];
                    rgbData[(destY * width + x) * 3 + 2] = row[x * bytesPerPixel + 0];
                }
            }
        }
        
        if (!quantizeToIndexed(rgbData.data(), width, height, texture.mip0, texture.palette))
        {
            Vortigaunt_Printf("ERROR: Failed to quantize texture to 256 colors");
            return false;
        }
    }
    else
    {
        Vortigaunt_Printf("ERROR: Unsupported BMP bit depth: " + std::to_string(bitsPerPixel));
        return false;
    }
    
    return resizeAndValidate(texture);
}

bool WadArchive::loadViaQImage(const std::string& path, WadTexture& texture)
{
#if HAS_QT_IMAGE_SUPPORT
    QImage image = ImageUtils::loadImage(path);
    if (image.isNull())
    {
        Vortigaunt_Printf("ERROR: Cannot load image file: " + path);
        return false;
    }

    // Convert to ARGB32 — QImage handles all formats (PNG, JPEG, TGA, DDS, BMP)
    image = image.convertToFormat(QImage::Format_ARGB32);

    int origWidth  = image.width();
    int origHeight = image.height();

    // Round up to nearest 16 (WAD3 requirement)
    uint32_t width  = static_cast<uint32_t>(((origWidth  + 15) / 16) * 16);
    uint32_t height = static_cast<uint32_t>(((origHeight + 15) / 16) * 16);
    if (width  < 16) width  = 16;
    if (height < 16) height = 16;

    texture.width  = width;
    texture.height = height;

    int total = static_cast<int>(width * height);

    // Build flat ARGB pixel array (padded canvas, default = fully transparent)
    // Padding pixels on right/bottom (from rounding up to 16) must be transparent,
    // not opaque black, so they get mapped to index 255
    std::vector<uint32_t> argbPixels(total, 0x00000000u);

    // Center the source image within the padded canvas
    int offsetX = (static_cast<int>(width)  - origWidth)  / 2;
    int offsetY = (static_cast<int>(height) - origHeight) / 2;

    for (int y = 0; y < origHeight; ++y)
    {
        const QRgb* scanline = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < origWidth; ++x)
            argbPixels[(y + offsetY) * width + (x + offsetX)] = static_cast<uint32_t>(scanline[x]);
    }

    // Detect whether the image has any fully-transparent pixels
    // (semi-transparent pixels will be alpha-blended, not treated as transparent)
    bool hasTransparency = false;
    for (int i = 0; i < total; ++i)
    {
        if (((argbPixels[i] >> 24) & 0xFF) == 0)
        {
            hasTransparency = true;
            break;
        }
    }



    std::vector<uint8_t> rgbaBuffer(total * 4);
    for (int i = 0; i < total; ++i)
    {
        uint32_t c = argbPixels[i];
        uint8_t  a = (c >> 24) & 0xFF;
        uint8_t  r = (c >> 16) & 0xFF;
        uint8_t  g = (c >>  8) & 0xFF;
        uint8_t  b = (c      ) & 0xFF;

        if (a == 0)
        {
            // Fully transparent → blue key placeholder (overridden after quantization)
            rgbaBuffer[i * 4 + 0] = 0;
            rgbaBuffer[i * 4 + 1] = 0;
            rgbaBuffer[i * 4 + 2] = 255;
            rgbaBuffer[i * 4 + 3] = 255;
        }
        else if (a < 255)
        {
            // Semi-transparent → blend onto black background to preserve anti-aliased edges
            // result = src * alpha/255  (black bg contributes 0)
            float fa = a / 255.0f;
            rgbaBuffer[i * 4 + 0] = static_cast<uint8_t>(r * fa + 0.5f);
            rgbaBuffer[i * 4 + 1] = static_cast<uint8_t>(g * fa + 0.5f);
            rgbaBuffer[i * 4 + 2] = static_cast<uint8_t>(b * fa + 0.5f);
            rgbaBuffer[i * 4 + 3] = 255; // treat as opaque for quantizer
        }
        else
        {
            rgbaBuffer[i * 4 + 0] = r;
            rgbaBuffer[i * 4 + 1] = g;
            rgbaBuffer[i * 4 + 2] = b;
            rgbaBuffer[i * 4 + 3] = 255;
        }
    }

    // Quantize: use max 255 colors when transparency is present (slot 255 reserved)
    int maxColors = hasTransparency ? 255 : 256;

    liq_attr* attr = liq_attr_create();
    if (!attr) return false;
    liq_set_speed(attr, 1);
    liq_set_max_colors(attr, maxColors);

    liq_image* img = liq_image_create_rgba(attr, rgbaBuffer.data(),
                                           static_cast<int>(width),
                                           static_cast<int>(height), 0);
    if (!img) { liq_attr_destroy(attr); return false; }

    liq_result* res = nullptr;
    if (liq_image_quantize(img, attr, &res) != LIQ_OK)
    {
        liq_image_destroy(img);
        liq_attr_destroy(attr);
        return false;
    }

    // Write remapped indexed data
    texture.mip0.resize(total);
    if (liq_write_remapped_image(res, img, texture.mip0.data(), total) != LIQ_OK)
    {
        liq_result_destroy(res);
        liq_image_destroy(img);
        liq_attr_destroy(attr);
        return false;
    }

    // Build palette (WAD3 stores 3 bytes/color, 256 entries = 768 bytes)
    const liq_palette* pal = liq_get_palette(res);
    std::memset(texture.palette, 0, 768);
    for (unsigned int i = 0; i < pal->count; ++i)
    {
        texture.palette[i * 3 + 0] = pal->entries[i].r;
        texture.palette[i * 3 + 1] = pal->entries[i].g;
        texture.palette[i * 3 + 2] = pal->entries[i].b;
    }

    liq_result_destroy(res);
    liq_image_destroy(img);
    liq_attr_destroy(attr);

    if (hasTransparency)
    {
        // Force palette slot 255 = GoldSrc blue key {0, 0, 255}
        texture.palette[255 * 3 + 0] = 0;
        texture.palette[255 * 3 + 1] = 0;
        texture.palette[255 * 3 + 2] = 255;

        // Remap all fully-transparent pixels to index 255
        for (int i = 0; i < total; ++i)
        {
            if (((argbPixels[i] >> 24) & 0xFF) == 0)
                texture.mip0[i] = 255;
        }
    }

    return resizeAndValidate(texture);
#else
    Vortigaunt_Printf("ERROR: Image loading requires Qt (GUI build only): " + path);
    return false;
#endif
}



bool WadArchive::addTextureFromImage(const std::string& name, const std::string& imagePath)
{
    std::string ext = getFileExtension(imagePath);
    
    WadTexture texture;
    texture.name = sanitizeTextureName(name);
    
    bool success = false;
    
    if (ext == "bmp")
    {
        success = loadBmp(imagePath, texture);
    }
	else if (ext == "png" || ext == "jpg" || ext == "jpeg" || ext == "tga" /* || ext == "dds"*/)// TODO: add native DDS support
    {
        success = loadViaQImage(imagePath, texture);
    }
    else
    {
        Vortigaunt_Printf("ERROR: Unsupported image format: " + ext);
        return false;
    }
    
    if (success)
    {
        Vortigaunt_Printf("Added texture '" + texture.name + "' (" + 
            std::to_string(texture.width) + "x" + std::to_string(texture.height) + 
            ") from " + ext);
        m_textures.push_back(std::move(texture));
    }
    
    return success;
}



bool WadArchive::addTextureFromRgb(const std::string& name, uint32_t width, uint32_t height,
                                 const uint8_t* rgbData)
{
    WadTexture texture;
    texture.name = sanitizeTextureName(name);
    
    if (width % 16 != 0 || height % 16 != 0)
    {
        Vortigaunt_Printf("ERROR: Texture dimensions must be multiples of 16");
        return false;
    }
    
    texture.width = width;
    texture.height = height;
    
    if (!quantizeToIndexed(rgbData, width, height, texture.mip0, texture.palette))
    {
        return false;
    }
    
    generateMipmaps(texture);
    m_textures.push_back(std::move(texture));
    return true;
}

bool WadArchive::save(const std::string& outputPath)
{
    if (m_textures.empty())
    {
        Vortigaunt_Printf("ERROR: No textures to save");
        return false;
    }
    
    Vortigaunt_Printf("Saving WAD file: " + outputPath);
    Vortigaunt_Printf("  Textures: " + std::to_string(m_textures.size()));
    
    std::ofstream file(FileIO::toPath(outputPath), std::ios::binary);
    if (!file)
    {
        Vortigaunt_Printf("ERROR: Cannot create WAD file: " + outputPath);
        return false;
    }
    
    WadHeader header;
    std::memcpy(header.signature, WadConstants::SIGNATURE, 4);
    header.numTextures = static_cast<uint32_t>(m_textures.size());
    header.dirOffset = 0;
    file.write(reinterpret_cast<char*>(&header), sizeof(header));
    
    std::vector<WadDirEntry> directory;
    
    for (auto& texture : m_textures)
    {
        // Special handling for transparency ({ prefix)
        bool isTransparent = !texture.name.empty() && texture.name[0] == '{';
        uint8_t finalPalette[768];
        std::memcpy(finalPalette, texture.palette, 768);
        
        if (isTransparent)
        {
            // { textures is transparent so it should be blue
            finalPalette[255 * 3 + 0] = 0;
            finalPalette[255 * 3 + 1] = 0;
            finalPalette[255 * 3 + 2] = 255;
         
        }

        WadDirEntry entry = {};
        entry.filePos = static_cast<uint32_t>(file.tellp());
        entry.type = WadConstants::TEXTURE_TYPE_MIPTEX;
        entry.compression = 0;
        
        std::memset(entry.name, 0, 16);
        std::memcpy(entry.name, texture.name.c_str(), 
                    std::min(texture.name.length(), size_t(15)));
        
        uint32_t mip0Size = texture.width * texture.height;
        uint32_t mip1Size = (texture.width / 2) * (texture.height / 2);
        uint32_t mip2Size = (texture.width / 4) * (texture.height / 4);
        uint32_t mip3Size = static_cast<uint32_t>(texture.mip3.size());
        
        MiptexHeader miptex = {};
        std::memset(miptex.name, 0, 16);
        std::memcpy(miptex.name, texture.name.c_str(),
                    std::min(texture.name.length(), size_t(15)));
        miptex.width = texture.width;
        miptex.height = texture.height;
        
        uint32_t offset = sizeof(MiptexHeader);
        miptex.offsets[0] = offset;
        offset += mip0Size;
        miptex.offsets[1] = offset;
        offset += mip1Size;
        miptex.offsets[2] = offset;
        offset += mip2Size;
        miptex.offsets[3] = offset;
        
        file.write(reinterpret_cast<char*>(&miptex), sizeof(miptex));
        file.write(reinterpret_cast<const char*>(texture.mip0.data()), mip0Size);
        file.write(reinterpret_cast<const char*>(texture.mip1.data()), mip1Size);
        file.write(reinterpret_cast<const char*>(texture.mip2.data()), mip2Size);
        file.write(reinterpret_cast<const char*>(texture.mip3.data()), mip3Size);
        
        uint16_t colorCount = 256;
        file.write(reinterpret_cast<char*>(&colorCount), 2);
        file.write(reinterpret_cast<const char*>(finalPalette), WadConstants::PALETTE_BYTES);
        
        entry.diskSize = static_cast<uint32_t>(file.tellp()) - entry.filePos;
        entry.size = entry.diskSize;
        texture.diskSize = entry.diskSize;
        
        directory.push_back(entry);
    }
    
    uint32_t dirOffset = static_cast<uint32_t>(file.tellp());
    for (const auto& entry : directory)
    {
        file.write(reinterpret_cast<const char*>(&entry), sizeof(entry));
    }
    
    file.seekp(8);
    file.write(reinterpret_cast<char*>(&dirOffset), 4);
    
    file.close();
    
    m_currentWadPath = outputPath;
    Vortigaunt_Printf("^2WAD file saved successfully");
    return true;
}


bool WadArchive::load(const std::string& path)
{
    std::ifstream file(FileIO::toPath(path), std::ios::binary);
    if (!file)
    {
        Vortigaunt_Printf("ERROR: Cannot open WAD file: " + path);
        return false;
    }
    
    // Read header
    WadHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    if (std::memcmp(header.signature, WadConstants::SIGNATURE, 4) != 0)
    {
        Vortigaunt_Printf("ERROR: Invalid WAD3");
        return false;
    }
    
    Vortigaunt_Printf("Loading WAD: " + path);
    Vortigaunt_Printf("Textures: " + std::to_string(header.numTextures));
    
    file.seekg(header.dirOffset);
    std::vector<WadDirEntry> directory(header.numTextures);
    file.read(reinterpret_cast<char*>(directory.data()), 
              header.numTextures * sizeof(WadDirEntry));
    
    // Clear existing textures
    m_textures.clear();
    
    // Read each texture
    for (const auto& entry : directory)
    {
        if (entry.type != WadConstants::TEXTURE_TYPE_MIPTEX)
            continue;
        
        file.seekg(entry.filePos);
        
        MiptexHeader miptex;
        file.read(reinterpret_cast<char*>(&miptex), sizeof(miptex));
        
        WadTexture texture;
        texture.name = std::string(entry.name, strnlen(entry.name, 16));
        texture.width = miptex.width;
        texture.height = miptex.height;
        texture.diskSize = entry.diskSize;
        
        // Read mip0
        uint32_t mip0Size = miptex.width * miptex.height;
        texture.mip0.resize(mip0Size);
        file.seekg(entry.filePos + miptex.offsets[0]);
        file.read(reinterpret_cast<char*>(texture.mip0.data()), mip0Size);
        
        // Read other mipmaps
        uint32_t mip1Size = (miptex.width / 2) * (miptex.height / 2);
        texture.mip1.resize(mip1Size);
        file.seekg(entry.filePos + miptex.offsets[1]);
        file.read(reinterpret_cast<char*>(texture.mip1.data()), mip1Size);
        
        uint32_t mip2Size = (miptex.width / 4) * (miptex.height / 4);
        texture.mip2.resize(mip2Size);
        file.seekg(entry.filePos + miptex.offsets[2]);
        file.read(reinterpret_cast<char*>(texture.mip2.data()), mip2Size);
        
        uint32_t mip3Size = (miptex.width / 8) * (miptex.height / 8);
        if (mip3Size < 1) mip3Size = 1;
        texture.mip3.resize(mip3Size);
        file.seekg(entry.filePos + miptex.offsets[3]);
        file.read(reinterpret_cast<char*>(texture.mip3.data()), mip3Size);
        
        file.seekg(entry.filePos + miptex.offsets[3] + mip3Size + 2);
        file.read(reinterpret_cast<char*>(texture.palette), WadConstants::PALETTE_BYTES);
        
        m_textures.push_back(std::move(texture));
    }
    
    m_currentWadPath = path;
    Vortigaunt_Printf("WAD loaded successfully");
    return true;
}

bool WadArchive::extractTextureToBmp(size_t index, const std::string& outputPath)
{
    if (index >= m_textures.size())
    {
        Vortigaunt_Printf("ERROR: Invalid texture index");
        return false;
    }

    const WadTexture& texture = m_textures[index];


    uint8_t bgraPalette[1024];
    std::memset(bgraPalette, 0, sizeof(bgraPalette));
    for (int i = 0; i < 256; ++i)
    {
        bgraPalette[i * 4 + 0] = texture.palette[i * 3 + 2]; // B
        bgraPalette[i * 4 + 1] = texture.palette[i * 3 + 1]; // G
        bgraPalette[i * 4 + 2] = texture.palette[i * 3 + 0]; // R
        bgraPalette[i * 4 + 3] = 0;                           // A
    }

    if (!BMP::writeIndexed8(outputPath.c_str(), static_cast<int>(texture.width), static_cast<int>(texture.height), bgraPalette, texture.mip0.data()))
    {
        Vortigaunt_Printf("ERROR: Cannot create BMP file: " + outputPath);
        return false;
    }

    Vortigaunt_Printf("Extracted texture '" + texture.name + "' to " + outputPath);
    return true;
}

bool WadArchive::removeTexture(size_t index)
{
    if (index >= m_textures.size())
    {
        return false;
    }
    
    m_textures.erase(m_textures.begin() + index);
    return true;
}

bool WadArchive::renameTexture(size_t index, const std::string& newName)
{
    if (index >= m_textures.size())
    {
        return false;
    }
    
    std::string sanitized = sanitizeTextureName(newName);
    if (sanitized.empty())
    {
        return false;
    }
    
    m_textures[index].name = sanitized;
    return true;
}

#if HAS_QT_IMAGE_SUPPORT
QImage WadArchive::textureThumbnail(size_t index, int maxSize)
{
    QImage fullImage = textureFullImage(index);
    if (fullImage.isNull())
    {
        return QImage();
    }
    
    return fullImage.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QImage WadArchive::textureFullImage(size_t index)
{
    if (index >= m_textures.size())
    {
        return QImage();
    }
    
    const WadTexture& texture = m_textures[index];
    if (texture.mip0.empty() || texture.width == 0 || texture.height == 0)
    {
        return QImage();
    }
    
    // Convert indexed data + palette to RGB QImage
    QImage image(static_cast<int>(texture.width), static_cast<int>(texture.height), QImage::Format_RGB888);
    
    for (uint32_t y = 0; y < texture.height; ++y)
    {
        uchar* scanline = image.scanLine(y);
        for (uint32_t x = 0; x < texture.width; ++x)
        {
            uint8_t paletteIndex = texture.mip0[y * texture.width + x];
            scanline[x * 3 + 0] = texture.palette[paletteIndex * 3 + 0];  // R
            scanline[x * 3 + 1] = texture.palette[paletteIndex * 3 + 1];  // G
            scanline[x * 3 + 2] = texture.palette[paletteIndex * 3 + 2];  // B
        }
    }
    
    return image;
}
#endif // HAS_QT_IMAGE_SUPPORT
