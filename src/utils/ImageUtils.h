#pragma once

#ifdef QT_WIDGETS_LIB
#include <QImage>
#include <QString>
#include <string>

namespace ImageUtils {
    // Loads an image file (PNG, BMP, JPG, TGA, DDS, GIF, TIFF, etc.) into a QImage.
    QImage loadImage(const std::string& filePath);
    QImage loadImage(const QString& filePath);
}
#endif
