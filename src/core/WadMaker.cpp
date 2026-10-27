#include "WadMaker.h"
#include "core/VortigauntLog.h"
#include "utils/Bmp.h"
#include "utils/FileIO.h"
#include "utils/ImageUtils.h"
#include "utils/util.hpp"


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
    ext = String_ToLower(ext);
    return ext;
}

std::string WadArchive::sanitizeTextureName(const std::string& name) const
{
    std::string result;
    result.reserve(15);
    
    for (char c : name)
    {
        if (result.length() >= 15) break;

        if (std::isalnum(static_cast<unsigned char>(c)) || c == '_' || c == '-' ||
            c == '{' || c == '!' || c == '+' || c == '~')
        {
            result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        }
        else if (c == ' ')
        {
            result += '_';
        }
    }
    
    // i just need delete this shit
    if (result.rfind("SKY", 0) == 0)
        for (char& c : result) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

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
    const size_t pixelCount = static_cast<size_t>(width) * height;
    std::vector<uint32_t> argb(pixelCount);
    for (size_t i = 0; i < pixelCount; ++i)
    {
        argb[i] = 0xFF000000u | (rgbData[i * 3 + 0] << 16) | (rgbData[i * 3 + 1] << 8) | rgbData[i * 3 + 2];
    }
    return BMP::quantize(argb.data(), static_cast<int>(width), static_cast<int>(height), false, palette, indexedData);
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

bool WadArchive::loadImageFile(const std::string& path, WadFit fit, WadTexture& texture)
{
    const std::string ext = getFileExtension(path);

    // an 8-bit BMP of a size GoldSrc takes is used as it is, its palette included
    if (ext == "bmp")
    {
        std::ifstream file(FileIO::toPath(path), std::ios::binary);
        uint8_t header[54] = {};
        file.read(reinterpret_cast<char*>(header), sizeof(header));
        uint32_t width = 0, height = 0;
        uint16_t bits = 0;
        std::memcpy(&width, header + 18, 4);
        std::memcpy(&height, header + 22, 4);
        std::memcpy(&bits, header + 28, 2);
        const bool exact = file && bits == 8 && width % 16 == 0 && height % 16 == 0 &&
                           width <= WadConstants::MAX_TEXTURE_SIZE && height <= WadConstants::MAX_TEXTURE_SIZE;
        if (exact || !HAS_QT_IMAGE_SUPPORT)
            return loadBmp(path, texture);
    }

#if HAS_QT_IMAGE_SUPPORT
    const QImage image = ImageUtils::loadImage(path);
    if (image.isNull())
    {
        Vortigaunt_Printf("ERROR: Cannot load image file: " + path);
        return false;
    }
    return textureFromImage(image, fit, texture);
#else
    (void)fit;
    Vortigaunt_Printf("ERROR: Image loading requires Qt (GUI build only): " + path);
    return false;
#endif
}

#if HAS_QT_IMAGE_SUPPORT
bool WadArchive::textureFromImage(const QImage& image, WadFit fit, WadTexture& texture)
{
    if (image.isNull())
        return false;
    QImage argb = image.convertToFormat(QImage::Format_ARGB32);

    // at most 512 a side, then sides that are multiples of 16
    const int limit = static_cast<int>(WadConstants::MAX_TEXTURE_SIZE);
    if (argb.width() > limit || argb.height() > limit)
    {
        LogF("Resizing texture from %dx%d to fit 512x512 limit", argb.width(), argb.height());
        argb = argb.scaled(limit, limit, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    if (fit == WadFit::Scale)
    {
        const int width = (std::max)(16, (argb.width() + 8) / 16 * 16);
        const int height = (std::max)(16, (argb.height() + 8) / 16 * 16);
        if (width != argb.width() || height != argb.height())
            argb = argb.scaled((std::min)(width, limit), (std::min)(height, limit), Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }
    else
    {
        const int width = (argb.width() + 15) / 16 * 16;
        const int height = (argb.height() + 15) / 16 * 16;
        if (width != argb.width() || height != argb.height())
        {
            QImage padded(width, height, QImage::Format_ARGB32);
            padded.fill(Qt::transparent);
            for (int y = 0; y < argb.height(); ++y)
                std::memcpy(padded.scanLine(y + (height - argb.height()) / 2) + ((width - argb.width()) / 2) * 4,
                            argb.constScanLine(y), static_cast<size_t>(argb.width()) * 4);
            argb = padded;
        }
    }

    // below half alpha is a hole (palette index 255), the rest is opaque
    const int total = argb.width() * argb.height();
    std::vector<uint32_t> pixels(static_cast<size_t>(total));
    bool transparent = false;
    for (int y = 0; y < argb.height(); ++y)
    {
        const auto* line = reinterpret_cast<const uint32_t*>(argb.constScanLine(y));
        for (int x = 0; x < argb.width(); ++x)
        {
            const uint32_t c = line[x];
            const bool hole = (c >> 24) < 128;
            transparent = transparent || hole;
            pixels[static_cast<size_t>(y) * argb.width() + x] = hole ? 0u : (c | 0xFF000000u);
        }
    }

    texture.width = static_cast<uint32_t>(argb.width());
    texture.height = static_cast<uint32_t>(argb.height());
    if (!BMP::quantize(pixels.data(), argb.width(), argb.height(), transparent, texture.palette, texture.mip0))
        return false;

    // GoldSrc only shows the holes of a { texture
    if (transparent && !texture.name.empty() && std::string("{!+~").find(texture.name[0]) == std::string::npos)
        texture.name = sanitizeTextureName("{" + texture.name);
    generateMipmaps(texture);
    return true;
}
#endif

bool WadArchive::addTextureFromImage(const std::string& name, const std::string& imagePath, WadFit fit)
{
    WadTexture texture;
    texture.name = sanitizeTextureName(name);
    if (!loadImageFile(imagePath, fit, texture))
        return false;

    Vortigaunt_Printf("Added texture '" + texture.name + "' (" +
        std::to_string(texture.width) + "x" + std::to_string(texture.height) + ") from " + imagePath);
    m_textures.push_back(std::move(texture));
    return true;
}

#if HAS_QT_IMAGE_SUPPORT
bool WadArchive::addTextureFromImage(const std::string& name, const QImage& image, WadFit fit)
{
    WadTexture texture;
    texture.name = sanitizeTextureName(name);
    if (!textureFromImage(image, fit, texture))
        return false;
    m_textures.push_back(std::move(texture));
    return true;
}
#endif

bool WadArchive::replaceTextureImage(size_t index, const std::string& imagePath, WadFit fit)
{
    if (index >= m_textures.size())
        return false;
    WadTexture texture;
    texture.name = m_textures[index].name;
    if (!loadImageFile(imagePath, fit, texture))
        return false;
    texture.name = m_textures[index].name;
    m_textures[index] = std::move(texture);
    return true;
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
    QImage fullImage = textureImage(index);
    if (fullImage.isNull())
    {
        return QImage();
    }
    
    return fullImage.scaled(maxSize, maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}

QImage WadArchive::textureImage(size_t index, int mip) const
{
    if (index >= m_textures.size() || mip < 0 || mip > 3)
    {
        return QImage();
    }

    const WadTexture& texture = m_textures[index];
    const std::vector<uint8_t>* levels[] = { &texture.mip0, &texture.mip1, &texture.mip2, &texture.mip3 };
    const std::vector<uint8_t>& data = *levels[mip];
    const uint32_t width = texture.width >> mip;
    const uint32_t height = texture.height >> mip;
    if (width == 0 || height == 0 || data.size() < static_cast<size_t>(width) * height)
    {
        return QImage();
    }

    // indexed data + palette; a masked texture shows its colour 255 as a hole, like the game
    const bool masked = !texture.name.empty() && texture.name[0] == '{';
    QImage image(static_cast<int>(width), static_cast<int>(height), QImage::Format_ARGB32);
    for (uint32_t y = 0; y < height; ++y)
    {
        auto* scanline = reinterpret_cast<QRgb*>(image.scanLine(static_cast<int>(y)));
        for (uint32_t x = 0; x < width; ++x)
        {
            const uint8_t paletteIndex = data[y * width + x];
            const uint8_t* rgb = texture.palette + paletteIndex * 3;
            scanline[x] = masked && paletteIndex == 255 ? qRgba(0, 0, 0, 0) : qRgb(rgb[0], rgb[1], rgb[2]);
        }
    }
    return image;
}
#endif // HAS_QT_IMAGE_SUPPORT
