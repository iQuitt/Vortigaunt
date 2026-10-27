// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#ifdef QT_WIDGETS_LIB
#include <QImage>
#include <QString>
#include <string>

namespace ImageUtils {
    // what a VTF holds besides its first picture
    struct VtfLayers {
        int frames = 1;
        int faces = 1;
        int mipmaps = 1;
        QString format;
    };

    // Loads an image file (PNG, BMP, JPG, TGA, DDS, VTF, GIF, TIFF, etc.) into a QImage.
    QImage loadImage(const std::string& filePath);
    QImage loadImage(const QString& filePath);
    // the same from a file's bytes; extension like ".tga"
    QImage loadImageFromMemory(const void* data, size_t size, const std::string& extension);
    // one frame, face and mipmap of a Source engine texture
    QImage loadVtfFromMemory(const void* data, size_t size, int frame = 0, int face = 0, int mipmap = 0, VtfLayers* layers = nullptr);
}
#endif
