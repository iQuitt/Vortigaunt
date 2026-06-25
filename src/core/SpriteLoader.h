#pragma once

#include <vector>
#include <string>
#include <memory>
#include <cstdint>

#ifdef QT_WIDGETS_LIB
#include <QImage>
#endif

enum class SpriteVersion : int32_t {
    QUAKE = 1,
    GOLDSRC = 2,
    CSO = 3,
    LITHTECH = 100  // Lithtech engine sprite (DTX references)
};

struct SpriteHeader
{
    uint32_t id;              // "IDSP"
    SpriteVersion version;    // Sprite version
    int32_t type;             // Sprite type
    int32_t texture_format;   // Texture format
    float bounding_radius;
    int32_t max_width;
    int32_t max_height;
    int32_t num_frames;
    float beam_len;
    int32_t synch_type;
};

struct SpriteFrame
{
    int32_t origin_x;
    int32_t origin_y;
#ifdef QT_WIDGETS_LIB
    QImage image;
#endif
    std::vector<uint8_t> pixel_data;  // Raw pixel data for V2
    int32_t width;
    int32_t height;
    int32_t group;
    std::string dtxPath;  // DTX texture path (Lithtech sprites only)
};

class SpriteLoader
{
public:
    class UnsupportedFileException : public std::exception
    {
    public:
        UnsupportedFileException(const std::string& msg) : m_msg(msg) {}
        const char* what() const noexcept override { return m_msg.c_str(); }
    private:
        std::string m_msg;
    };

    SpriteLoader();
    ~SpriteLoader() = default;

    // Load SPR file
    bool loadFile(const std::string& filePath, bool transparent = false);
    bool loadFile(const std::vector<uint8_t>& data, bool transparent = false);
    //
    //// Save loaded sprite to file
    bool saveFile(const std::string& outputPath);

    // Get loaded sprite data
    const SpriteHeader& getHeader() const { return m_header; }
    SpriteHeader& getHeader() { return m_header; }
    const std::vector<SpriteFrame>& getFrames() const { return m_frames; }
    std::vector<SpriteFrame>& getFrames() { return m_frames; }
    size_t getFrameCount() const { return m_frames.size(); }

    // Extract frames to BMP files
    bool extractFramesToBmp(const std::string& filePath, const std::string& outputDir, const std::string& prefix = "frame");

    // Create sprite from frames
    bool createSpriteV2(const std::string& outputPath,
                       const std::vector<std::string>& framePaths,
                       int32_t spriteType,
                       int32_t textureFormat,
                       const std::vector<uint8_t>& bgColor = {0, 0, 0},
                       float contrast = 1.5f);

    // Convert V3 sprite to V2
    bool convertV3ToV2(const std::string& inputPath,
                      const std::string& outputPath,
                      int32_t spriteType,
                      int32_t textureFormat,
                      const std::vector<uint8_t>& bgColor = {0, 0, 0},
                      float contrast = 1.5f);

    // Lithtech sprite support
    bool loadLithtechTextures(const std::string& baseDir);
    const std::vector<std::string>& getDtxPaths() const { return m_dtxPaths; }
    uint32_t getLithtechFrameRate() const { return m_lithtechFrameRate; }
    float getLithtechFrameDurationMs() const;
    bool exportLithtechToGoldSrc(const std::string& outputPath,
                                 int32_t spriteType = 2,
                                 int32_t textureFormat = 2);
    bool exportLithtechFramesToBmp(const std::string& outputDir, const std::string& prefix = "frame");
    bool isLithtech() const { return m_header.version == SpriteVersion::LITHTECH; }
    const std::string& getFilePath() const { return m_filePath; }

    // Clear loaded data
    void clear();

private:
    bool loadVersion2(const std::vector<uint8_t>& data, size_t offset, bool transparent);
    bool loadVersion3(const std::vector<uint8_t>& data, size_t offset, bool transparent);
    bool loadLithtech(const std::vector<uint8_t>& data);
    
    bool isDdsFile(const std::vector<uint8_t>& data, size_t startPos, size_t& ddsPos);
    bool loadDdsFrame(const std::vector<uint8_t>& ddsData, SpriteFrame& frame, bool transparent = false);
    bool loadDtxTexture(const std::string& dtxPath, SpriteFrame& frame);
    
    void createSharedPalette(const std::vector<SpriteFrame>& frames, std::vector<uint8_t>& palette);

#ifdef QT_WIDGETS_LIB
    // Helper: alpha-composite src onto a solid background colour
    static QImage alphaCompositeOnto(const QImage& src, uint8_t bgR, uint8_t bgG, uint8_t bgB);

    // Helper: stitch a list of RGB images side-by-side on a black canvas
    static QImage buildCompositeImage(const std::vector<QImage>& images);

    // Helper: quantize compositeRgba to maxColors using libimagequant, return QVector<QRgb>
    static QVector<QRgb> buildPaletteWithLiq(const QImage& compositeRgba, int maxColors);

    // Helper: map every pixel in src to the nearest entry in colorTable (with cache)
    static std::vector<uint8_t> mapPixelsToNearestPalette(const QImage& src, const QVector<QRgb>& colorTable);
#endif

    SpriteHeader m_header;
    std::vector<SpriteFrame> m_frames;
    std::vector<uint8_t> m_palette; // 768 bytes (256 * RGB) for V2 sprites
    
    // Datas for lithtech
    std::vector<std::string> m_dtxPaths;
    uint32_t m_lithtechFrameRate = 0;
    std::string m_filePath;
    
public:
    const std::vector<uint8_t>& getPalette() const { return m_palette; }
    std::vector<uint8_t>& getPalette() { return m_palette; }
    
    // Rebuild frame images from palette (call after modifying palette)
    void rebuildFramesFromPalette();
};

