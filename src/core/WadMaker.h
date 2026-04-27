#pragma once

#include <string>
#include <vector>
#include <functional>
#include <cstdint>

#ifdef QT_CORE_LIB
#include <QImage>
#endif

namespace WadConstants
{
    constexpr char SIGNATURE[4] = {'W', 'A', 'D', '3'};
    constexpr uint8_t TEXTURE_TYPE_MIPTEX = 0x43;  // Mipmap texture
    constexpr int MAX_TEXTURE_NAME = 16;           // Including null terminator
    constexpr int PALETTE_SIZE = 256;
    constexpr int PALETTE_BYTES = PALETTE_SIZE * 3;
    constexpr uint32_t MAX_TEXTURE_SIZE = 512;    
}

// WAD3 Header structure
#pragma pack(push, 1)
struct WadHeader
{
    char signature[4];      // "WAD3"
    uint32_t numTextures;   // Number of textures
    uint32_t dirOffset;     // Offset to directory
};

// WAD3 Directory entry
struct WadDirEntry
{
    uint32_t filePos;       // Offset to texture data
    uint32_t diskSize;      // Size in file
    uint32_t size;          // Uncompressed size
    uint8_t type;           // Texture type (0x43 = miptex)
    uint8_t compression;    // 0 = none
    uint16_t padding;       // Unused
    char name[16];          // Texture name
};

// Miptex header in WAD
struct MiptexHeader
{
    char name[16];          // Texture name
    uint32_t width;         // Width
    uint32_t height;        // Height
    uint32_t offsets[4];    // Offsets to mipmap data (relative to start of miptex)
};
#pragma pack(pop)

/**
 * @brief WAD texture entry for Half-Life WAD3 format
 * thanks to the https://twhl.info/wiki/page/Specification:_WAD3
 */
struct WadTexture
{
    std::string name;           // Max 15 chars (+ null terminator = 16)
    uint32_t width = 0;         // Must be multiple of 16
    uint32_t height = 0;        // Must be multiple of 16
    std::vector<uint8_t> mip0;  // Full resolution (indexed 8-bit)
    std::vector<uint8_t> mip1;  // 1/2 size
    std::vector<uint8_t> mip2;  // 1/4 size
    std::vector<uint8_t> mip3;  // 1/8 size
    uint8_t palette[768];       // 256 RGB colors (256 * 3 bytes)
    uint32_t diskSize = 0;      // Size in WAD file (for display)
};

/**
 * @brief Reads, writes and edits Half-Life WAD3 texture archives
 * 
 * WAD3 format stores 8-bit indexed textures with 4 mipmap levels
 * and a 256-color palette per texture.
 * 
 * Supported input formats:
 * - BMP (8-bit indexed or 24-bit RGB)
 * - PNG (via Qt)
 * - JPEG (via Qt)
 * - TGA (Targa)
 * - DDS (DirectDraw Surface)
 */
class WadArchive
{
public:

    WadArchive();
    ~WadArchive();
    

    /**
     * @brief Load an existing WAD file
     * @param path Path to .wad file
     * @return true on success
     */
    bool load(const std::string& path);

    /**
     * @brief Save WAD file to disk
     * @param outputPath Output .wad file path
     * @return true on success
     */
    bool save(const std::string& outputPath);

    /**
     * @brief Clear all textures and reset state
     */
    void clear();


    /**
     * @brief Add a texture from any supported image file
     * @param name Texture name (max 15 chars, will be uppercased)
     * @param imagePath Path to image file (BMP, PNG, JPEG, TGA, DDS)
     * @return true if texture was added successfully
     */
    bool addTextureFromImage(const std::string& name, const std::string& imagePath);

    /**
     * @brief Add a texture from raw data (24-bit RGB)
     * @param name Texture name
     * @param width Image width (must be multiple of 16)
     * @param height Image height (must be multiple of 16)
     * @param rgbData Raw 24-bit RGB pixel data
     * @return true if texture was added successfully
     */
    bool addTextureFromRgb(const std::string& name, uint32_t width, uint32_t height, 
                           const uint8_t* rgbData);


    /**
     * @brief Remove a texture by index
     * @param index Texture index
     * @return true on success
     */
    bool removeTexture(size_t index);

    /**
     * @brief Rename a texture
     * @param index Texture index
     * @param newName New texture name (max 15 chars, will be sanitized)
     * @return true on success
     */
    bool renameTexture(size_t index, const std::string& newName);

    /**
     * @brief Extract a texture to BMP file
     * @param index Texture index
     * @param outputPath Output BMP file path
     * @return true on success
     */
    bool extractTextureToBmp(size_t index, const std::string& outputPath);


    /**
     * @brief Get all loaded textures
     */
    const std::vector<WadTexture>& textures() const { return m_textures; }

    /**
     * @brief Get number of textures in the WAD
     */
    size_t textureCount() const { return m_textures.size(); }

    /**
     * @brief Get current WAD file path
     */
    const std::string& currentPath() const { return m_currentWadPath; }

#ifdef QT_CORE_LIB

    /**
     * @brief Get texture as QImage thumbnail (for UI display)
     * @param index Texture index
     * @param maxSize Maximum thumbnail size (scaled proportionally)
     * @return QImage or null image if not available
     */
    QImage textureThumbnail(size_t index, int maxSize = 64);
    QImage textureFullImage(size_t index);
#endif

private:
    // Sanitize texture name (uppercase, max 15 chars, valid chars only)
    std::string sanitizeTextureName(const std::string& name) const;

    // Generate mipmaps from full-resolution indexed data
    void generateMipmaps(WadTexture& texture);

    // Quantize 24-bit RGB to 8-bit indexed with palette
    bool quantizeToIndexed(const uint8_t* rgbData, uint32_t width, uint32_t height,
                          std::vector<uint8_t>& indexedData, uint8_t* palette);

    // Resize and validate texture dimensions
    bool resizeAndValidate(WadTexture& texture);

    // Load various image formats
    bool loadBmp(const std::string& path, WadTexture& texture);
    bool loadViaQImage(const std::string& path, WadTexture& texture); // PNG, JPEG, TGA, DDS

    // Get file extension in lowercase
    std::string getFileExtension(const std::string& path) const;

    std::vector<WadTexture> m_textures;
    std::string m_currentWadPath;  // Path of loaded WAD file
};
