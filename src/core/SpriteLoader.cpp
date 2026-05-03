#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "SpriteLoader.h"
#include "core/VortigauntLog.h"
#include "utils/FileIO.h"

#include <fstream>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <unordered_map>
#include <unordered_set>
#include <future>
#include <mutex>


#ifdef QT_WIDGETS_LIB
#include <QImage>
#include <QColor>
#include <QPainter>
#endif

#include "utils/Dds.h"

#include "utils/Bmp.h"

#include "libimagequant.h"

#ifdef ENABLE_LITHTECH
#include "core/converters/DtxConverter.h"
#endif

static const size_t LITHTECH_SPR_HEADER_SIZE = 20;  // 4 + 4 + 12 bytes



SpriteLoader::SpriteLoader()
{
    m_header = {};
    m_frames.clear();
}

void SpriteLoader::clear()
{
    m_header = {};
    m_frames.clear();
    m_palette.clear();
    m_dtxPaths.clear();
    m_lithtechFrameRate = 0;
    m_filePath.clear();
}

void SpriteLoader::rebuildFramesFromPalette()
{
#ifdef QT_WIDGETS_LIB
    if (m_header.version != SpriteVersion::GOLDSRC || m_palette.empty())
        return;
    
    // Build color table from current palette
    QVector<QRgb> colorTable;
    for (size_t i = 0; i < m_palette.size() / 3 && colorTable.size() < 256; i++) {
        uint8_t r = m_palette[i * 3];
        uint8_t g = m_palette[i * 3 + 1];
        uint8_t b = m_palette[i * 3 + 2];
        colorTable.append(qRgb(r, g, b));
    }
    
    // Rebuild each frame's QImage from pixel_data
    for (auto& frame : m_frames) {
        if (frame.pixel_data.empty() || frame.width <= 0 || frame.height <= 0)
            continue;
        
        QImage img(frame.width, frame.height, QImage::Format_Indexed8);
        img.setColorTable(colorTable);
        
        // Copy pixel data
        for (int y = 0; y < frame.height; y++) {
            for (int x = 0; x < frame.width; x++) {
                int idx = y * frame.width + x;
                if (idx < static_cast<int>(frame.pixel_data.size())) {
                    img.setPixel(x, y, frame.pixel_data[idx]);
                }
            }
        }
        
        // Convert to ARGB32 for display
        frame.image = img.convertToFormat(QImage::Format_ARGB32);
    }
#endif
}

bool SpriteLoader::loadFile(const std::string& filePath, bool transparent)
{
    std::ifstream file(FileIO::toPath(filePath), std::ios::binary | std::ios::ate);
    if (!file)
        return false;
    
    size_t fileSize = file.tellg();
    if (fileSize < LITHTECH_SPR_HEADER_SIZE)  // Lithtech sprites can be smaller than 40
        return false;
    
    file.seekg(0, std::ios::beg);
    std::vector<uint8_t> data(fileSize);
    file.read(reinterpret_cast<char*>(data.data()), fileSize);
    
    m_filePath = filePath;
    bool success = loadFile(data, transparent);
    if (!success)
        m_filePath.clear();
    return success;
}

bool SpriteLoader::loadFile(const std::vector<uint8_t>& data, bool transparent)
{
    if (data.size() < LITHTECH_SPR_HEADER_SIZE)
        return false;
    
    clear();
    
    // Check for IDSP magic (GoldSrc/CSO sprites)
    uint32_t id = *reinterpret_cast<const uint32_t*>(data.data());
    if (id == 0x50534449) // little-endian "IDSP"
    {
        if (data.size() < 40)
            return false;
        
        m_header.id = id;
        m_header.version = static_cast<SpriteVersion>(*reinterpret_cast<const int32_t*>(data.data() + 4));
        m_header.type = *reinterpret_cast<const int32_t*>(data.data() + 8);
        m_header.texture_format = *reinterpret_cast<const int32_t*>(data.data() + 12);
        m_header.bounding_radius = *reinterpret_cast<const float*>(data.data() + 16);
        m_header.max_width = *reinterpret_cast<const int32_t*>(data.data() + 20);
        m_header.max_height = *reinterpret_cast<const int32_t*>(data.data() + 24);
        m_header.num_frames = *reinterpret_cast<const int32_t*>(data.data() + 28);
        m_header.beam_len = *reinterpret_cast<const float*>(data.data() + 32);
        m_header.synch_type = *reinterpret_cast<const int32_t*>(data.data() + 36);
        
        if (m_header.version == SpriteVersion::CSO)
            return loadVersion3(data, 40, transparent);
        else if (m_header.version == SpriteVersion::GOLDSRC)
            return loadVersion2(data, 40, transparent);
        
        return false;
    }
    
    // No IDSP magic — try Lithtech format
    return loadLithtech(data);
}

bool SpriteLoader::loadVersion2(const std::vector<uint8_t>& data, size_t offset, bool transparent)
{
    if (offset + 2 > data.size())
        return false;
    
    // Read palette size
    uint16_t paletteSize = *reinterpret_cast<const uint16_t*>(data.data() + offset);
    offset += 2;
    
    size_t paletteBytes = static_cast<size_t>(paletteSize) * 3;
    if (offset + paletteBytes > data.size())
        return false;
    
    // Read palette
    std::vector<uint8_t> palette(data.begin() + offset, data.begin() + offset + paletteBytes);
    offset += paletteBytes;
    
    // Store palette for later access
    m_palette = palette;
    
    // Handle transparency for palette
    if (transparent && m_header.texture_format == 3 && paletteSize > 0) {
        palette[(paletteSize - 1) * 3] = 0;
        palette[(paletteSize - 1) * 3 + 1] = 0;
        palette[(paletteSize - 1) * 3 + 2] = 0;
    }
    
    // Read frames
    for (int i = 0; i < m_header.num_frames; i++) {
        if (offset + 20 > data.size())
            break;
        
        SpriteFrame frame;
        const int32_t* frameData = reinterpret_cast<const int32_t*>(data.data() + offset);
        frame.group = frameData[0];
        frame.origin_x = frameData[1];
        frame.origin_y = frameData[2];
        frame.width = frameData[3];
        frame.height = frameData[4];
        offset += 20;
        
        size_t pixelCount = static_cast<size_t>(frame.width) * static_cast<size_t>(frame.height);
        if (offset + pixelCount > data.size())
            break;
        
        frame.pixel_data.assign(data.begin() + offset, data.begin() + offset + pixelCount);
        offset += pixelCount;
        
#ifdef QT_WIDGETS_LIB
        QImage img(frame.width, frame.height, QImage::Format_Indexed8);
        QVector<QRgb> colorTable;
        for (size_t j = 0; j < static_cast<size_t>(paletteSize); j++) {
            uint8_t r = palette[j * 3];
            uint8_t g = palette[j * 3 + 1];
            uint8_t b = palette[j * 3 + 2];
            colorTable.append(qRgb(r, g, b));
        }
        img.setColorTable(colorTable);
        
        for (int y = 0; y < frame.height; y++) {
            for (int x = 0; x < frame.width; x++) {
                int idx = y * frame.width + x;
                img.setPixel(x, y, frame.pixel_data[idx]);
            }
        }
        
        if (transparent) {
            img = img.convertToFormat(QImage::Format_ARGB32);
            for (int y = 0; y < frame.height; y++) {
                for (int x = 0; x < frame.width; x++) {
                    QColor color = img.pixelColor(x, y);
                    if (color.red() == 0 && color.green() == 0 && color.blue() == 255) {
                        img.setPixelColor(x, y, QColor(0, 0, 0, 0));
                    }
                }
            }
        }
        
        frame.image = img;
#endif
        
        m_frames.push_back(frame);
    }
    
    return !m_frames.empty();
}

bool SpriteLoader::loadVersion3(const std::vector<uint8_t>& data, size_t offset, bool transparent)
{
    if (offset >= data.size())
        return false;
    
    // Create a view of the remaining data for DDS scanning
    std::vector<uint8_t> remainingData(data.begin() + offset, data.end());
    
    size_t currentPos = 0;
    while (currentPos < remainingData.size()) {
        size_t ddsPos;
        if (!isDdsFile(remainingData, currentPos, ddsPos))
            break;
        
        SpriteFrame frame;
        frame.origin_x = 0;
        frame.origin_y = 0;
        frame.group = 0;
        
        if (ddsPos >= 20) {
            const uint8_t* frameHeaderBytes = remainingData.data() + ddsPos - 20;
            frame.group = *reinterpret_cast<const int32_t*>(frameHeaderBytes);
            frame.origin_x = *reinterpret_cast<const int32_t*>(frameHeaderBytes + 4);
            frame.origin_y = *reinterpret_cast<const int32_t*>(frameHeaderBytes + 8);
            frame.width = *reinterpret_cast<const int32_t*>(frameHeaderBytes + 12);
            frame.height = *reinterpret_cast<const int32_t*>(frameHeaderBytes + 16);
        }
        
        size_t nextDdsPos;
        bool hasNext = isDdsFile(remainingData, ddsPos + 4, nextDdsPos);
        
        size_t ddsSize = hasNext ? (nextDdsPos - ddsPos) : (remainingData.size() - ddsPos);
        std::vector<uint8_t> ddsData(remainingData.begin() + ddsPos, remainingData.begin() + ddsPos + ddsSize);
        
        if (loadDdsFrame(ddsData, frame, transparent)) {
            m_frames.push_back(frame);
        }
        
        currentPos = ddsPos + 4;
    }
    
    return !m_frames.empty();
}

bool SpriteLoader::isDdsFile(const std::vector<uint8_t>& data, size_t startPos, size_t& ddsPos)
{
    if (data.size() < 4 || startPos + 4 > data.size())
        return false;
    
    const uint8_t ddsSig[] = {'D', 'D', 'S', ' '};
    for (size_t i = startPos; i + 3 < data.size(); i++) {
        if (std::memcmp(data.data() + i, ddsSig, 4) == 0) {
            ddsPos = i;
            return true;
        }
    }
    return false;
}

bool SpriteLoader::loadDdsFrame(const std::vector<uint8_t>& ddsData, SpriteFrame& frame, bool transparent)
{
#ifdef QT_WIDGETS_LIB
    QImage img = DDS::loadDdsFromMemory(ddsData);
    if (img.isNull())
        return false;
        
    if (transparent && img.format() != QImage::Format_ARGB32) {
        img = img.convertToFormat(QImage::Format_ARGB32);
    }
    
    frame.image = img;
    frame.width = img.width();
    frame.height = img.height();
    return true;
#else
    return false;
#endif
}

bool SpriteLoader::extractFramesToBmp(const std::string& filePath, const std::string& outputDir)
{
    if (!loadFile(filePath, false))
        return false;
    
    std::filesystem::create_directories(outputDir);
    
#ifdef QT_WIDGETS_LIB
    for (size_t i = 0; i < m_frames.size(); i++) {
        const auto& frame = m_frames[i];
        if (frame.image.isNull())
            continue;
        
        // Convert to RGB with black background
        QImage rgbImg = QImage(frame.image.size(), QImage::Format_RGB32);
        rgbImg.fill(QColor(0, 0, 0));
        
        QPainter painter(&rgbImg);
        painter.drawImage(0, 0, frame.image);
        painter.end();
        
        // Use octree quantisation (same as CLI) for best quality
        std::string outputPath = outputDir + "/frame_" + std::to_string(i) + ".bmp";
        QImage argb = rgbImg.convertToFormat(QImage::Format_ARGB32);
        if (!BMP::saveAsIndexed8(outputPath.c_str(),
                                      argb.width(), argb.height(),
                                      reinterpret_cast<const uint32_t*>(argb.constBits())))
            return false;
    }
    return true;
#else
    return false;
#endif
}

bool SpriteLoader::saveFile(const std::string& outputPath)
{
    if (m_header.version != SpriteVersion::GOLDSRC) {
        // Only GoldSrc (V2) sprites can be saved with this method
        return false;
    }
    
    std::ofstream outFile(FileIO::toPath(outputPath), std::ios::binary);
    if (!outFile.is_open())
        return false;
    
    // Write header
    outFile.write(reinterpret_cast<const char*>(&m_header.id), 4);
    outFile.write(reinterpret_cast<const char*>(&m_header.version), 4);
    outFile.write(reinterpret_cast<const char*>(&m_header.type), 4);
    outFile.write(reinterpret_cast<const char*>(&m_header.texture_format), 4);
    outFile.write(reinterpret_cast<const char*>(&m_header.bounding_radius), 4);
    outFile.write(reinterpret_cast<const char*>(&m_header.max_width), 4);
    outFile.write(reinterpret_cast<const char*>(&m_header.max_height), 4);
    outFile.write(reinterpret_cast<const char*>(&m_header.num_frames), 4);
    outFile.write(reinterpret_cast<const char*>(&m_header.beam_len), 4);
    outFile.write(reinterpret_cast<const char*>(&m_header.synch_type), 4);
    
    // Write palette (V2 specific)
    int16_t paletteSize = 256;
    outFile.write(reinterpret_cast<const char*>(&paletteSize), 2);
    outFile.write(reinterpret_cast<const char*>(m_palette.data()), m_palette.size());
    
    // Write frames
    for (const auto& frame : m_frames) {
        int32_t group = frame.group;
        int32_t originX = frame.origin_x;
        int32_t originY = frame.origin_y;
        int32_t width = frame.width;
        int32_t height = frame.height;
        
        outFile.write(reinterpret_cast<const char*>(&group), 4);
        outFile.write(reinterpret_cast<const char*>(&originX), 4);
        outFile.write(reinterpret_cast<const char*>(&originY), 4);
        outFile.write(reinterpret_cast<const char*>(&width), 4);
        outFile.write(reinterpret_cast<const char*>(&height), 4);
        
        outFile.write(reinterpret_cast<const char*>(frame.pixel_data.data()), frame.pixel_data.size());
    }
    
    outFile.close();
    return true;
}

bool SpriteLoader::createSpriteV2(const std::string& outputPath,const std::vector<std::string>& framePaths,int32_t spriteType,int32_t textureFormat,const std::vector<uint8_t>& bgColor,float contrast)
{
#ifdef QT_WIDGETS_LIB
    (void)contrast;
    if (framePaths.empty())
        return false;
    
    // Load all frame images
    std::vector<QImage> frameImages;
    int32_t maxWidth = 0;
    int32_t maxHeight = 0;
    
    uint8_t bgR = bgColor.size() > 0 ? bgColor[0] : 0;
    uint8_t bgG = bgColor.size() > 1 ? bgColor[1] : 0;
    uint8_t bgB = bgColor.size() > 2 ? bgColor[2] : 0;
    
    for (const auto& framePath : framePaths) {
        QImage img(QString::fromStdString(framePath));
        if (img.isNull())
            continue;
        
        QImage rgbImg = alphaCompositeOnto(img.convertToFormat(QImage::Format_ARGB32), bgR, bgG, bgB);
        
        for (int y = 0; y < rgbImg.height(); y++) {
            QRgb* scanLine = reinterpret_cast<QRgb*>(rgbImg.scanLine(y));
            for (int x = 0; x < rgbImg.width(); x++) {
                int r = qRed(scanLine[x]);
                int g = qGreen(scanLine[x]);
                int b = qBlue(scanLine[x]);
                
                if (std::abs(r - bgR) < 15 && std::abs(g - bgG) < 15 && std::abs(b - bgB) < 15) {
                    scanLine[x] = qRgb(bgR, bgG, bgB);
                }
            }
        }
        
        maxWidth = std::max(maxWidth, static_cast<int32_t>(rgbImg.width()));
        maxHeight = std::max(maxHeight, static_cast<int32_t>(rgbImg.height()));
        frameImages.push_back(rgbImg);
    }
    
    if (frameImages.empty())
        return false;
    
    // Create composite image for palette generation
    QImage compositeImage = buildCompositeImage(frameImages);
    
    // Create palette (254 colors) using libimagequant
    QVector<QRgb> colorTable = buildPaletteWithLiq(
        compositeImage.convertToFormat(QImage::Format_RGBA8888), 254);
    
    // Force exact background color into the palette to prevent Additive sprite background artifacts
    colorTable.prepend(qRgb(bgR, bgG, bgB));

    while (colorTable.size() < 255)
        colorTable.append(qRgb(0, 0, 0));
    
	colorTable.append(qRgb(0, 0, 255));// blue is transparency key
    
    // Write sprite file
    std::ofstream outFile(FileIO::toPath(outputPath), std::ios::binary);
    if (!outFile)
        return false;
    
    // Write IDSP header
    uint32_t idsp = 0x50534449; // "IDSP" in little-endian
    outFile.write(reinterpret_cast<const char*>(&idsp), 4);
    
    // Write sprite header
    SpriteVersion version = SpriteVersion::GOLDSRC;
    float boundingRadius = std::max(maxWidth, maxHeight) / 2.0f;
    int32_t numFrames = static_cast<int32_t>(frameImages.size());
    float beamLen = 0.0f;
    int32_t synchType = 0;
    
    outFile.write(reinterpret_cast<const char*>(&version), 4);
    outFile.write(reinterpret_cast<const char*>(&spriteType), 4);
    outFile.write(reinterpret_cast<const char*>(&textureFormat), 4);
    outFile.write(reinterpret_cast<const char*>(&boundingRadius), 4);
    outFile.write(reinterpret_cast<const char*>(&maxWidth), 4);
    outFile.write(reinterpret_cast<const char*>(&maxHeight), 4);
    outFile.write(reinterpret_cast<const char*>(&numFrames), 4);
    outFile.write(reinterpret_cast<const char*>(&beamLen), 4);
    outFile.write(reinterpret_cast<const char*>(&synchType), 4);
    

    uint16_t paletteSize = 256;
    outFile.write(reinterpret_cast<const char*>(&paletteSize), 2);
    
    // Write palette data (256 * 3 = 768 bytes)
    std::vector<uint8_t> paletteData(768);
    for (int i = 0; i < 256 && i < colorTable.size(); i++) {
        QRgb color = colorTable[i];
        paletteData[i * 3] = qRed(color);
        paletteData[i * 3 + 1] = qGreen(color);
        paletteData[i * 3 + 2] = qBlue(color);
    }
    outFile.write(reinterpret_cast<const char*>(paletteData.data()), 768);
    
    // Write frames
    for (size_t i = 0; i < frameImages.size(); i++) {
        const QImage& frameImg = frameImages[i];
        QImage processedImg = frameImg;
        
        // For IndexAlpha (2) and AlphaTest (3), composite onto blue background
        if (textureFormat == 2 || textureFormat == 3) {
            if (i < framePaths.size()) {
                QImage origImg(QString::fromStdString(framePaths[i]));
                origImg = origImg.convertToFormat(QImage::Format_ARGB32);
                if (origImg.hasAlphaChannel()) {
                    // Start with blue, paste RGB frame using original alpha
                    QImage result(frameImg.size(), QImage::Format_RGB32);
                    result.fill(QColor(0, 0, 255));
                    for (int y = 0; y < frameImg.height(); y++) {
                        const QRgb* alphaLine = reinterpret_cast<const QRgb*>(origImg.constScanLine(y));
                        const QRgb* rgbLine = reinterpret_cast<const QRgb*>(frameImg.constScanLine(y));
                        QRgb* dstLine = reinterpret_cast<QRgb*>(result.scanLine(y));
                        for (int x = 0; x < frameImg.width(); x++) {
                            int alpha = qAlpha(alphaLine[x]);
                            if (alpha == 255) {
                                dstLine[x] = rgbLine[x];
                            } else if (alpha > 0) {
                                float a = alpha / 255.0f;
                                int rr = static_cast<int>(qRed(rgbLine[x]) * a + 0 * (1.0f - a));
                                int gg = static_cast<int>(qGreen(rgbLine[x]) * a + 0 * (1.0f - a));
                                int bb = static_cast<int>(qBlue(rgbLine[x]) * a + 255 * (1.0f - a));
                                dstLine[x] = qRgb(rr, gg, bb);
                            }
                            // alpha==0: keep blue
                        }
                    }
                    processedImg = result;
                }
            }
        }
        
        // Convert to indexed using nearest-color mapping against shared palette
        QImage srcImg = processedImg;
        if (srcImg.format() != QImage::Format_RGB32 && srcImg.format() != QImage::Format_ARGB32)
            srcImg = srcImg.convertToFormat(QImage::Format_RGB32);
        
        int32_t width  = srcImg.width();
        int32_t height = srcImg.height();
        std::vector<uint8_t> pixelData = mapPixelsToNearestPalette(srcImg, colorTable);
        
        // Write frame header
        int32_t group = 0;

        // Calculate origin from frame dimensions (center the sprite)
        int32_t originX = -(width / 2);
        int32_t originY = height / 2;
        
        outFile.write(reinterpret_cast<const char*>(&group), 4);
        outFile.write(reinterpret_cast<const char*>(&originX), 4);
        outFile.write(reinterpret_cast<const char*>(&originY), 4);
        outFile.write(reinterpret_cast<const char*>(&width), 4);
        outFile.write(reinterpret_cast<const char*>(&height), 4);
        
        // Write pixel data directly (matches Python: f.write(indexed.tobytes()))
        outFile.write(reinterpret_cast<const char*>(pixelData.data()), width * height);
    }
    
    outFile.close();
    return true;
#else
    return false;
#endif
}



bool SpriteLoader::convertV3ToV2(const std::string& inputPath, const std::string& outputPath, int32_t spriteType, int32_t textureFormat, const std::vector<uint8_t>& bgColor, float contrast)
{
#ifdef QT_WIDGETS_LIB
    (void)contrast;
    if (!loadFile(inputPath, true))
        return false;
    
    const auto& header = getHeader();
    if (header.version != SpriteVersion::CSO)
        return false;
    
    const auto& frames = getFrames();
    if (frames.empty())
        return false;
    
    // Convert DDS frames to RGB with background color
    std::vector<QImage> rgbFrames;
    std::vector<std::pair<int32_t, int32_t>> frameOrigins;
    
    uint8_t bgR = bgColor.size() > 0 ? bgColor[0] : 0;
    uint8_t bgG = bgColor.size() > 1 ? bgColor[1] : 0;
    uint8_t bgB = bgColor.size() > 2 ? bgColor[2] : 0;
    
    for (const auto& frame : frames) {
        if (frame.image.isNull())
            continue;
        
        QImage rgbImg = alphaCompositeOnto(
            frame.image.convertToFormat(QImage::Format_ARGB32), bgR, bgG, bgB);

        for (int y = 0; y < rgbImg.height(); y++) {
            QRgb* scanLine = reinterpret_cast<QRgb*>(rgbImg.scanLine(y));
            for (int x = 0; x < rgbImg.width(); x++) {
                int r = qRed(scanLine[x]);
                int g = qGreen(scanLine[x]);
                int b = qBlue(scanLine[x]);
                
                if (std::abs(r - bgR) < 15 && std::abs(g - bgG) < 15 && std::abs(b - bgB) < 15) {
                    scanLine[x] = qRgb(bgR, bgG, bgB);
                }
            }
        }
        
        rgbFrames.push_back(rgbImg);
        frameOrigins.push_back({frame.origin_x, frame.origin_y});
    }
    
    if (rgbFrames.empty())
        return false;
    
    // Create composite image for palette generation
    QImage compositeImage = buildCompositeImage(rgbFrames);
    
    // Create palette (254 colors + 1 bg color + 1 transparency) using libimagequant
    QVector<QRgb> colorTable = buildPaletteWithLiq(
        compositeImage.convertToFormat(QImage::Format_RGBA8888), 254);
    
    // Force exact background color into the palette to prevent Additive sprite background artifacts
    colorTable.prepend(qRgb(bgR, bgG, bgB));

    while (colorTable.size() < 255)
        colorTable.append(qRgb(0, 0, 0));
    // Add transparency color at index 255 (always blue = sprite transparency key)
    colorTable.append(qRgb(0, 0, 255));
    
    // Write V2 sprite file
    std::ofstream outFile(FileIO::toPath(outputPath), std::ios::binary);
    if (!outFile)
        return false;
    
    // Write IDSP header
    uint32_t idsp = 0x50534449; // little-endian "IDSP"
    outFile.write(reinterpret_cast<const char*>(&idsp), 4);
    
    // Calculate max dimensions
    int32_t maxWidth = 0;
    int32_t maxHeight32 = 0;
    for (const auto& img : rgbFrames) {
        maxWidth = std::max(maxWidth, static_cast<int32_t>(img.width()));
        maxHeight32 = std::max(maxHeight32, static_cast<int32_t>(img.height()));
    }
    
    // Write sprite header
    SpriteVersion version = SpriteVersion::GOLDSRC;
    float boundingRadius = header.bounding_radius;
    int32_t numFrames = static_cast<int32_t>(rgbFrames.size());
    float beamLen = header.beam_len;
    int32_t synchType = header.synch_type;
    
    outFile.write(reinterpret_cast<const char*>(&version), 4);
    outFile.write(reinterpret_cast<const char*>(&spriteType), 4);
    outFile.write(reinterpret_cast<const char*>(&textureFormat), 4);
    outFile.write(reinterpret_cast<const char*>(&boundingRadius), 4);
    outFile.write(reinterpret_cast<const char*>(&maxWidth), 4);
    outFile.write(reinterpret_cast<const char*>(&maxHeight32), 4);
    outFile.write(reinterpret_cast<const char*>(&numFrames), 4);
    outFile.write(reinterpret_cast<const char*>(&beamLen), 4);
    outFile.write(reinterpret_cast<const char*>(&synchType), 4);
    
    // Write palette size (256)
    uint16_t paletteSize = 256;
    outFile.write(reinterpret_cast<const char*>(&paletteSize), 2);
    
    // Write palette data (256 * 3 = 768 bytes)
    std::vector<uint8_t> paletteData(768);
    for (int i = 0; i < 256 && i < colorTable.size(); i++) {
        QRgb color = colorTable[i];
        paletteData[i * 3] = qRed(color);
        paletteData[i * 3 + 1] = qGreen(color);
        paletteData[i * 3 + 2] = qBlue(color);
    }
    outFile.write(reinterpret_cast<const char*>(paletteData.data()), 768);
    
    // Write frames
    for (size_t i = 0; i < rgbFrames.size(); i++) {
        const QImage& rgbFrame = rgbFrames[i];
        
        // Handle texture format specific processing
        QImage processedImg = rgbFrame;
        
        // For IndexAlpha (2) and AlphaTest (3), composite onto blue background
        // Matches Python: blue_bg.paste(processed_img, mask=alpha_mask)
        if (textureFormat == 2 || textureFormat == 3) {
            if (i < frames.size() && !frames[i].image.isNull()) {
                QImage alphaFrame = frames[i].image.convertToFormat(QImage::Format_ARGB32);
                if (alphaFrame.hasAlphaChannel()) {
                    // Start with blue, paste RGB frame using original alpha
                    QImage result(rgbFrame.size(), QImage::Format_RGB32);
                    result.fill(QColor(0, 0, 255));
                    for (int y = 0; y < rgbFrame.height(); y++) {
                        const QRgb* alphaLine = reinterpret_cast<const QRgb*>(alphaFrame.constScanLine(y));
                        const QRgb* rgbLine = reinterpret_cast<const QRgb*>(rgbFrame.constScanLine(y));
                        QRgb* dstLine = reinterpret_cast<QRgb*>(result.scanLine(y));
                        for (int x = 0; x < rgbFrame.width(); x++) {
                            int alpha = qAlpha(alphaLine[x]);
                            if (alpha == 255) {
                                dstLine[x] = rgbLine[x];
                            } else if (alpha > 0) {
                                float a = alpha / 255.0f;
                                int rr = static_cast<int>(qRed(rgbLine[x]) * a + 0 * (1.0f - a));
                                int gg = static_cast<int>(qGreen(rgbLine[x]) * a + 0 * (1.0f - a));
                                int bb = static_cast<int>(qBlue(rgbLine[x]) * a + 255 * (1.0f - a));
                                dstLine[x] = qRgb(rr, gg, bb);
                            }
                            // alpha==0: keep blue
                        }
                    }
                    processedImg = result;
                }
            }
        }
        
        // Convert to indexed using the shared palette (nearest-color mapping)
        int width  = processedImg.width();
        int height = processedImg.height();
        std::vector<uint8_t> pixelData = mapPixelsToNearestPalette(processedImg, colorTable);
        
        // Calculate origin from frame dimensions (center the sprite)
        int32_t originX = -(width / 2);
        int32_t originY = height / 2;
        
        // Write frame header
        int32_t group = 0;
        outFile.write(reinterpret_cast<const char*>(&group), 4);
        outFile.write(reinterpret_cast<const char*>(&originX), 4);
        outFile.write(reinterpret_cast<const char*>(&originY), 4);
        outFile.write(reinterpret_cast<const char*>(&width), 4);
        outFile.write(reinterpret_cast<const char*>(&height), 4);
        
        outFile.write(reinterpret_cast<const char*>(pixelData.data()), width * height);
    }
    
    outFile.close();
    return true;
#else
    return false;
#endif
}

#ifdef QT_WIDGETS_LIB

QImage SpriteLoader::alphaCompositeOnto(const QImage& src, uint8_t bgR, uint8_t bgG, uint8_t bgB)
{
    QImage result(src.size(), QImage::Format_RGB32);
    result.fill(QColor(bgR, bgG, bgB));
    for (int y = 0; y < src.height(); y++) {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(src.constScanLine(y));
        QRgb* dstLine = reinterpret_cast<QRgb*>(result.scanLine(y));
        for (int x = 0; x < src.width(); x++) {
            QRgb pixel = srcLine[x];
            int alpha = qAlpha(pixel);
            if (alpha == 0) {
                // keep background
            } else if (alpha == 255) {
                dstLine[x] = qRgb(qRed(pixel), qGreen(pixel), qBlue(pixel));
            } else {
                float a = alpha / 255.0f;
                int bR = static_cast<int>(qRed(pixel)   * a + bgR * (1.0f - a));
                int bG = static_cast<int>(qGreen(pixel) * a + bgG * (1.0f - a));
                int bB = static_cast<int>(qBlue(pixel)  * a + bgB * (1.0f - a));
                dstLine[x] = qRgb(bR, bG, bB);
            }
        }
    }
    return result;
}

QImage SpriteLoader::buildCompositeImage(const std::vector<QImage>& images)
{
    int totalWidth = 0, maxHeight = 0;
    for (const auto& img : images) {
        totalWidth += img.width();
        maxHeight = std::max(maxHeight, img.height());
    }
    QImage composite(totalWidth, maxHeight, QImage::Format_RGB32);
    composite.fill(QColor(0, 0, 0));
    QPainter painter(&composite);
    int xOffset = 0;
    for (const auto& img : images) {
        painter.drawImage(xOffset, 0, img);
        xOffset += img.width();
    }
    painter.end();
    return composite;
}

QVector<QRgb> SpriteLoader::buildPaletteWithLiq(const QImage& compositeRgba, int maxColors)
{
    QVector<QRgb> colorTable;
    liq_attr* attr = liq_attr_create();
    liq_set_max_colors(attr, maxColors);
    liq_set_speed(attr, 1); 
    liq_image* liqImg = liq_image_create_rgba(attr,
        compositeRgba.constBits(),
        compositeRgba.width(),
        compositeRgba.height(), 0);
    liq_result* liqRes = nullptr;
    if (liq_image_quantize(liqImg, attr, &liqRes) == LIQ_OK) {
        const liq_palette* liqPal = liq_get_palette(liqRes);
        int palCount = std::min((int)liqPal->count, maxColors);
        for (int i = 0; i < palCount; i++)
            colorTable.append(qRgb(liqPal->entries[i].r, liqPal->entries[i].g, liqPal->entries[i].b));
        liq_result_destroy(liqRes);
    }
    liq_image_destroy(liqImg);
    liq_attr_destroy(attr);
    return colorTable;
}

std::vector<uint8_t> SpriteLoader::mapPixelsToNearestPalette(const QImage& src, const QVector<QRgb>& colorTable)
{
    QImage img = src;
    if (img.format() != QImage::Format_RGB32 && img.format() != QImage::Format_ARGB32)
        img = img.convertToFormat(QImage::Format_RGB32);
    
    int width  = img.width();
    int height = img.height();
    int palSize = std::min(static_cast<int>(colorTable.size()), 256);
    
    // Pre-extract palette RGB values for faster access
    std::vector<int> palR(palSize), palG(palSize), palB(palSize);
    for (int j = 0; j < palSize; j++) {
        palR[j] = qRed(colorTable[j]);
        palG[j] = qGreen(colorTable[j]);
        palB[j] = qBlue(colorTable[j]);
    }
    
    std::unordered_map<uint32_t, uint8_t> colorCache;
    colorCache.reserve(1024);
    
    std::vector<uint8_t> pixelData(static_cast<size_t>(width) * static_cast<size_t>(height));
    for (int y = 0; y < height; y++) {
        const QRgb* srcLine = reinterpret_cast<const QRgb*>(img.constScanLine(y));
        for (int x = 0; x < width; x++) {
            QRgb pixelColor = srcLine[x];
            uint32_t colorKey = pixelColor & 0x00FFFFFF;
            auto cacheIt = colorCache.find(colorKey);
            if (cacheIt != colorCache.end()) {
                pixelData[static_cast<size_t>(y) * width + x] = cacheIt->second;
                continue;
            }
            int pr = qRed(pixelColor);
            int pg = qGreen(pixelColor);
            int pb = qBlue(pixelColor);
            int bestIndex = 0;
            int minDist = 255 * 255 * 3 + 1;
            for (int j = 0; j < palSize; j++) {
                int dr = pr - palR[j];
                int dg = pg - palG[j];
                int db = pb - palB[j];
                int dist = dr * dr + dg * dg + db * db;
                if (dist < minDist) {
                    minDist = dist;
                    bestIndex = j;
                    if (dist == 0) break;
                }
            }
            colorCache[colorKey] = static_cast<uint8_t>(bestIndex);
            pixelData[static_cast<size_t>(y) * width + x] = static_cast<uint8_t>(bestIndex);
        }
    }
    return pixelData;
}

#endif // QT_WIDGETS_LIB

void SpriteLoader::createSharedPalette(const std::vector<SpriteFrame>& frames, std::vector<uint8_t>& palette)
{
#ifdef QT_WIDGETS_LIB
    if (frames.empty()) {
        palette.clear();
        return;
    }
    
    // Create composite image from all frames
    int totalWidth = 0;
    int maxHeight = 0;
    for (const auto& frame : frames) {
        if (!frame.image.isNull()) {
            totalWidth += frame.image.width();
            maxHeight = std::max(maxHeight, frame.image.height());
        }
    }
    
    if (totalWidth == 0 || maxHeight == 0) {
        palette.clear();
        return;
    }
    
    QImage compositeImage(totalWidth, maxHeight, QImage::Format_RGB32);
    compositeImage.fill(QColor(0, 0, 0));
    
    QPainter painter(&compositeImage);
    int xOffset = 0;
    for (const auto& frame : frames) {
        if (!frame.image.isNull()) {
            painter.drawImage(xOffset, 0, frame.image);
            xOffset += frame.image.width();
        }
    }
    painter.end();
    
    // Quantize to 255 colors
    QImage paletteImg = compositeImage.convertToFormat(QImage::Format_Indexed8, Qt::DiffuseDither);
    QVector<QRgb> colorTable = paletteImg.colorTable();
    
    // Create palette data (255 colors * 3 bytes)
    palette.resize(255 * 3);
    for (int i = 0; i < 255 && i < colorTable.size(); i++) {
        QRgb color = colorTable[i];
        palette[i * 3] = qRed(color);
        palette[i * 3 + 1] = qGreen(color);
        palette[i * 3 + 2] = qBlue(color);
    }
    
    // Fill remaining slots with black if needed
    for (int i = colorTable.size(); i < 255; i++) {
        palette[i * 3] = 0;
        palette[i * 3 + 1] = 0;
        palette[i * 3 + 2] = 0;
    }
#else
    palette.clear();
#endif
}

//  Lithtech Sprite Support

// Strip leading directories one-by-one
// "A/B/C/file.dtx" -> "B/C/file.dtx" -> "C/file.dtx" -> "file.dtx"
static std::vector<std::string> generatePathVariations(const std::string& pathStr)
{
    std::vector<std::string> variations;
    std::filesystem::path p(pathStr);
    
    variations.push_back(pathStr);
    
    if (p.has_filename())
        variations.push_back(p.filename().string());
        

    std::string current = pathStr;
    std::replace(current.begin(), current.end(), '\\', '/');
    
    size_t slashPos = current.find('/');
    while (slashPos != std::string::npos)
    {
        current = current.substr(slashPos + 1);
        if (!current.empty())
        {
            variations.push_back(current);
        }
        slashPos = current.find('/');
    }
    
    return variations;
}

bool SpriteLoader::loadLithtech(const std::vector<uint8_t>& data)
{
    if (data.size() < LITHTECH_SPR_HEADER_SIZE)
        return false;

    uint32_t numFrames = *reinterpret_cast<const uint32_t*>(data.data());
    uint32_t frameRate = *reinterpret_cast<const uint32_t*>(data.data() + 4);

    if (numFrames == 0 || numFrames > 10000)
        return false;

    // Parse frame entries: pathLength (2 bytes LE) + path (pathLength bytes)
    size_t pos = LITHTECH_SPR_HEADER_SIZE;
    
    m_frames.reserve(numFrames);
    m_dtxPaths.reserve(numFrames);

    for (uint32_t i = 0; i < numFrames; ++i)
    {
        if (pos + 2 > data.size())
            break;

        uint16_t pathLength = *reinterpret_cast<const uint16_t*>(data.data() + pos);
        pos += 2;

        if (pathLength == 0 || pos + pathLength > data.size())
            break;

        SpriteFrame frame;
        frame.dtxPath = std::string(reinterpret_cast<const char*>(data.data() + pos), pathLength);
        pos += pathLength;

        // Normalize path separators
        std::replace(frame.dtxPath.begin(), frame.dtxPath.end(), '\\', '/');

        frame.origin_x = 0;
        frame.origin_y = 0;
        frame.width = 0;
        frame.height = 0;
        frame.group = 0;

        m_dtxPaths.push_back(frame.dtxPath);
        m_frames.push_back(frame);
    }

    if (m_frames.empty())
        return false;

    // Set header for Lithtech
    m_header = {};
    m_header.version = SpriteVersion::LITHTECH;
    m_header.num_frames = static_cast<int32_t>(m_frames.size());
    m_lithtechFrameRate = frameRate;

    return true;
}

float SpriteLoader::getLithtechFrameDurationMs() const
{
    return static_cast<float>(m_lithtechFrameRate);
}

bool SpriteLoader::loadLithtechTextures(const std::string& baseDir)
{
#if defined(QT_WIDGETS_LIB) && defined(ENABLE_LITHTECH)
    if (m_frames.empty())
    {
        VortigauntLog::LogF("ERROR: No frames to load.");
        return false;
    }

    bool anyLoaded = false;
    
    // Get SPR file's directory
    std::filesystem::path sprDir;
    if (!m_filePath.empty())
    {
        sprDir = std::filesystem::path(m_filePath).parent_path();
        VortigauntLog::LogF("SPR Directory: %s", sprDir.string());
    }
    
    // Define search roots
    std::vector<std::filesystem::path> searchRoots;
    if (!sprDir.empty()) searchRoots.push_back(sprDir);
    if (!baseDir.empty()) searchRoots.push_back(std::filesystem::path(baseDir));
    
    if (!sprDir.empty() && sprDir.has_parent_path())
    {
        searchRoots.push_back(sprDir.parent_path());
    }
    
    // Add common subfolders
    std::vector<std::filesystem::path> expandedRoots = searchRoots;
    for (const auto& root : searchRoots)
    {
		// those are mostly used subfolders in Lithtech projects.
		// not at all. but they might help in some cases.
		// TODO: Find a better way
        expandedRoots.push_back(root / "textures");
        expandedRoots.push_back(root / "Textures");
        expandedRoots.push_back(root / "tex");
        expandedRoots.push_back(root / "fx");
        expandedRoots.push_back(root / "FX");
    }


    // this is just bullshit
    // TODO: remove this shit and find better way
    struct LoadTask {
        size_t frameIndex;
        std::string resolvedPath;
    };
    std::vector<LoadTask> tasks;
 
    std::unordered_map<std::string, std::filesystem::path> fallbackCache;
    std::unordered_set<std::string> scannedFallbacks;
    
    int foundTexture = 0;

    for (size_t i = 0; i < m_frames.size(); ++i)
    {
        auto& frame = m_frames[i];
        if (!frame.image.isNull()) { anyLoaded = true; continue; }

        std::string dtxFilename = std::filesystem::path(frame.dtxPath).filename().string();
        
        auto variations = generatePathVariations(frame.dtxPath);
        std::string foundPath;

        for (const auto& root : expandedRoots)
        {
            for (const auto& var : variations)
            {
                std::filesystem::path candidate = root / var;
                std::error_code ec;
                if (std::filesystem::exists(candidate, ec))
                {
                    foundPath = candidate.string();
                    break;
                }
            }
            if (!foundPath.empty()) break;
        }

        if (foundPath.empty() && !sprDir.empty())
        {
            std::string dtxFilenameLower = dtxFilename;
            std::transform(dtxFilenameLower.begin(), dtxFilenameLower.end(), dtxFilenameLower.begin(), ::tolower);
            
            {
                if (scannedFallbacks.find(sprDir.string()) == scannedFallbacks.end())
                {
                    VortigauntLog::LogF("Scanning fallback dir: %s ", sprDir.string());
                    scannedFallbacks.insert(sprDir.string());
                    try {
                        for (const auto& entry : std::filesystem::recursive_directory_iterator(sprDir, std::filesystem::directory_options::skip_permission_denied)) {
                            if (entry.is_regular_file()) {
                                std::string fname = entry.path().filename().string();
                                std::transform(fname.begin(), fname.end(), fname.begin(), ::tolower);
                                if (fallbackCache.find(fname) == fallbackCache.end())
                                    fallbackCache[fname] = entry.path();
                            }
                        }
                    } catch (...) {}
                }
                
                if (fallbackCache.count(dtxFilenameLower))
                {
                    foundPath = fallbackCache[dtxFilenameLower].string();
                }
            }
        }

        if (!foundPath.empty())
        {
            tasks.push_back({i, foundPath});
            foundTexture++;
        }
    }

    VortigauntLog::LogF("[DEBUG] Got %s textures instantly...", std::to_string(foundTexture));

    if (!tasks.empty())
    {
        std::vector<std::future<void>> futures;
        std::mutex framesMutex; 
        int loadedCount = 0;
        
        auto worker = [&](const LoadTask& task) {
            SpriteFrame tempFrame;
            if (loadDtxTexture(task.resolvedPath, tempFrame))
            {
                std::lock_guard<std::mutex> lock(framesMutex);
                m_frames[task.frameIndex].image = std::move(tempFrame.image);
                m_frames[task.frameIndex].width = tempFrame.width;
                m_frames[task.frameIndex].height = tempFrame.height;
                loadedCount++;
            }
        };

        for (const auto& task : tasks)
        {
            futures.push_back(std::async(std::launch::async, worker, task));
        }

        for (auto& f : futures)
        {
            f.wait();
        }
        
        if (loadedCount > 0) anyLoaded = true;
        VortigauntLog::LogF("[DEBUG] ^2Successfully loaded: %s", std::to_string(loadedCount));
    }
    
    return anyLoaded;
#else
    (void)baseDir;
    return false;
#endif
}



bool SpriteLoader::loadDtxTexture(const std::string& dtxPath, SpriteFrame& frame)
{
#if defined(QT_WIDGETS_LIB) && defined(ENABLE_LITHTECH)
    std::filesystem::path path(dtxPath);
    if (!std::filesystem::exists(path))
        return false;

    DtxConverter converter;
    std::vector<unsigned int> pixels;
    int width = 0, height = 0;
    
    if (!converter.DecodeDTXToRGBA(path.string(), pixels, width, height))
    {
        VortigauntLog::LogF("Found but failed to decode: %s", dtxPath);
        return false;
    }

    if (pixels.empty() || width <= 0 || height <= 0)
    {
        VortigauntLog::LogF("Decode returned empty or invalid dimensions: %s", dtxPath);
        return false;
    }

    frame.image = QImage(width, height, QImage::Format_ARGB32);
    
    for (int y = 0; y < height; ++y)
    {
        for (int x = 0; x < width; ++x)
        {
            unsigned int pixel = pixels[y * width + x];
            uint8_t b = (pixel >> 0) & 0xFF;
            uint8_t g = (pixel >> 8) & 0xFF;
            uint8_t r = (pixel >> 16) & 0xFF;
            uint8_t a = (pixel >> 24) & 0xFF;
            frame.image.setPixel(x, y, qRgba(r, g, b, a));
        }
    }
    
    frame.width = width;
    frame.height = height;
    
    return true;
#else
    (void)dtxPath;
    (void)frame;
    return false;
#endif
}

bool SpriteLoader::exportLithtechToGoldSrc(const std::string& outputPath, int32_t spriteType, int32_t textureFormat)
{
#ifdef QT_WIDGETS_LIB
    if (m_frames.empty())
        return false;

    bool anyLoaded = false;
    for (const auto& frame : m_frames)
    {
        if (!frame.image.isNull())
        {
            anyLoaded = true;
            break;
        }
    }

    if (!anyLoaded)
        return false;

    // Save each frame as a temp PNG, then use createSpriteV2
    std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "lithtech_sprite_export";
    std::filesystem::create_directories(tempDir);

    std::vector<std::string> framePaths;
    for (size_t i = 0; i < m_frames.size(); ++i)
    {
        const auto& frame = m_frames[i];
        if (frame.image.isNull())
            continue;

        std::string framePath = (tempDir / ("frame_" + std::to_string(i) + ".png")).string();
        if (frame.image.save(QString::fromStdString(framePath)))
            framePaths.push_back(framePath);
    }

    if (framePaths.empty())
        return false;

    // Use a temporary SpriteLoader to avoid clobbering our state
    SpriteLoader tempLoader;
    bool ok = tempLoader.createSpriteV2(outputPath, framePaths, spriteType, textureFormat);

    // Clean up temp files
    for (const auto& p : framePaths)
    {
        std::error_code ec;
        std::filesystem::remove(p, ec);
    }

    return ok;
#else
    return false;
#endif
}

bool SpriteLoader::exportLithtechFramesToBmp(const std::string& outputDir)
{
#ifdef QT_WIDGETS_LIB
    if (m_frames.empty())
        return false;

    std::filesystem::create_directories(outputDir);

    bool anyExported = false;

    for (size_t i = 0; i < m_frames.size(); ++i)
    {
        const auto& frame = m_frames[i];
        if (frame.image.isNull())
            continue;

        std::string filename = "frame_" + std::to_string(i) + ".bmp";
        std::string outputPath = (std::filesystem::path(outputDir) / filename).string();

        QImage argb = frame.image.convertToFormat(QImage::Format_ARGB32);
        if (BMP::saveAsIndexed8(outputPath.c_str(),
                                     argb.width(), argb.height(),
                                     reinterpret_cast<const uint32_t*>(argb.constBits())))
        {
            anyExported = true;
        }
    }

    return anyExported;
#else
    return false;
#endif
}
