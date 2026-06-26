#include <QCoreApplication>
#include <QImage>
#include <QFileInfo>
#include <iostream>
#include <cstdlib>
#include "core/converters/DtxConverter.h"

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    
    if (argc < 4)
    {
        std::cerr << "Usage: vortigaunt-thumbnailer <size> <input_file> <output_file>\n";
        return 1;
    }

    int targetSize = std::atoi(argv[1]);
    QString inputFile = QString::fromUtf8(argv[2]);
    QString outputFile = QString::fromUtf8(argv[3]);

    DtxConverter converter;
    std::vector<unsigned int> pixels;
    int w = 0, h = 0;
    if (!converter.DecodeDTXToRGBA(inputFile.toStdString(), pixels, w, h))
    {
        std::cerr << "Failed to decode DTX file: " << inputFile.toStdString() << "\n";
        return 2;
    }

    QImage img(reinterpret_cast<const uchar*>(pixels.data()), w, h, QImage::Format_ARGB32);
    if (img.isNull())
    {
        std::cerr << "Failed to create QImage from decoded DTX data.\n";
        return 3;
    }

    // Scale the image for the thumbnail
    QImage thumb = img.scaled(targetSize, targetSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    if (!thumb.save(outputFile, "PNG"))
    {
        std::cerr << "Failed to save thumbnail PNG to: " << outputFile.toStdString() << "\n";
        return 4;
    }

    return 0;
}
