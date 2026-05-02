#include "DtxViewerDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QScrollArea>

#include <filesystem>

#include "DtxConverter.h"
#include "LanguageManager.h"
#include "utils/Bmp.h"

DtxViewerDialog::DtxViewerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("DTX Viewer"));
    resize(640, 480);

    auto* mainLayout = new QVBoxLayout();

    // Top controls: buttons + info
    auto* topLayout = new QHBoxLayout();
    m_openButton = new QPushButton(tr("Open DTX..."));
    m_saveBmpButton = new QPushButton(tr("Save as 8-bit BMP..."));
    m_saveBmpButton->setEnabled(false);

    m_infoLabel = new QLabel(tr("No file loaded."));
    m_infoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    topLayout->addWidget(m_openButton);
    topLayout->addWidget(m_saveBmpButton);
    topLayout->addWidget(m_infoLabel, 1);

    // Image preview in scroll area
    m_imageLabel = new QLabel();
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setBackgroundRole(QPalette::Base);
    m_imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    m_imageLabel->setScaledContents(false);

    auto* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setWidget(m_imageLabel);

    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(scroll, 1);

    setLayout(mainLayout);

    connect(m_openButton, &QPushButton::clicked, this, &DtxViewerDialog::onOpenDtx);
    connect(m_saveBmpButton, &QPushButton::clicked, this, &DtxViewerDialog::onSaveBmp);
}

void DtxViewerDialog::onOpenDtx()
{
    QString startDir;
    if (!m_currentDtxPath.isEmpty())
        startDir = QFileInfo(m_currentDtxPath).absolutePath();
    else
        startDir = QDir::homePath();

    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Select DTX file"),
        startDir,
        tr("DTX textures (*.dtx);;All files (*.*)"));

    if (path.isEmpty())
        return;

    loadDtxFile(path);
}

bool DtxViewerDialog::loadDtxFile(const QString& filePath)
{
    if (filePath.isEmpty())
        return false;

    DtxConverter converter;
    std::vector<unsigned int> pixels;
    int w = 0;
    int h = 0;

    if (!converter.DecodeDTXToRGBA(filePath.toStdString(), pixels, w, h))
    {
        QMessageBox::warning(this,
                             tr("DTX error"),
                             tr("Failed to decode DTX texture."));
        return false;
    }

    m_currentDtxPath = filePath;
    m_currentDisplayName = QFileInfo(filePath).fileName();

    return displayDecodedImage(pixels, w, h, m_currentDisplayName);
}

bool DtxViewerDialog::loadDtxFromMemory(const QByteArray& data, const QString& displayName)
{
    if (data.isEmpty())
        return false;

    DtxConverter converter;
    std::vector<unsigned int> pixels;
    int w = 0;
    int h = 0;

    if (!converter.DecodeDTXBufferToRGBA(reinterpret_cast<const uint8_t*>(data.constData()),
                                         static_cast<size_t>(data.size()),
                                         pixels,
                                         w,
                                         h))
    {
        QMessageBox::warning(this,
                             tr("DTX error"),
                             tr("Failed to decode in-memory DTX texture."));
        return false;
    }

    m_currentDtxPath.clear();
    m_currentDisplayName = displayName.isEmpty() ? tr("In-memory DTX") : displayName;

    return displayDecodedImage(pixels, w, h, m_currentDisplayName);
}

void DtxViewerDialog::updatePreview()
{
    if (m_currentImage.isNull())
    {
        m_imageLabel->setPixmap(QPixmap());
        return;
    }

    // Fit into the scroll area while keeping aspect ratio
    const QSize areaSize = m_imageLabel->parentWidget()->size();
    QPixmap pix = QPixmap::fromImage(m_currentImage);
    if (!areaSize.isEmpty())
    {
        pix = pix.scaled(areaSize - QSize(20, 20),
                         Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
    }
    m_imageLabel->setPixmap(pix);
}

void DtxViewerDialog::onSaveBmp()
{
    if (m_currentImage.isNull())
        return;

    QString baseName;
    if (!m_currentDtxPath.isEmpty())
    {
        baseName = QFileInfo(m_currentDtxPath).completeBaseName();
    }
    else if (!m_currentDisplayName.isEmpty())
    {
        baseName = QFileInfo(m_currentDisplayName).completeBaseName();
    }
    else
    {
        baseName = QStringLiteral("texture");
    }

    QString suggested = baseName + QStringLiteral(".bmp");

    const QString outPath = QFileDialog::getSaveFileName(
        this,
        tr("Save as 8-bit BMP"),
        QDir::home().filePath(suggested),
        tr("Bitmap images (*.bmp)"));

    if (outPath.isEmpty())
        return;



    QImage argb = m_currentImage.convertToFormat(QImage::Format_ARGB32);

    if (m_currentImage.hasAlphaChannel()) {
        for (int y = 0; y < argb.height(); ++y) {
            QRgb* scanLine = reinterpret_cast<QRgb*>(argb.scanLine(y));
            for (int x = 0; x < argb.width(); ++x) {
                if (qAlpha(scanLine[x]) < 5) {// TODO: I need take a look this value or go another way, because some of textures we got still green background. not at all but some pixels are green in some textures.
                    scanLine[x] = qRgb(0, 0, 0);
                }
                else {
                    scanLine[x] = qRgb(qRed(scanLine[x]), qGreen(scanLine[x]), qBlue(scanLine[x]));
                }
            }
        }
    }

    if (!BMP::saveAsIndexed8(outPath.toStdString().c_str(),
                                  argb.width(), argb.height(),
                                  reinterpret_cast<const uint32_t*>(argb.constBits())))
    {
        QMessageBox::warning(this,
                             tr("Save error"),
                             tr("Failed to save BMP file."));
        return;
    }

    QMessageBox::information(this,
                             tr("Saved"),
                             tr("Saved 8-bit BMP:\n%1").arg(outPath));
}

bool DtxViewerDialog::displayDecodedImage(const std::vector<unsigned int>& pixels,
                                          int width,
                                          int height,
                                          const QString& label)
{
    if (pixels.empty() || width <= 0 || height <= 0)
    {
        QMessageBox::warning(this,
                             tr("DTX error"),
                             tr("Decoded texture is empty."));
        return false;
    }

    QImage img(reinterpret_cast<const uchar*>(pixels.data()),
               width,
               height,
               QImage::Format_ARGB32);
    if (img.isNull())
    {
        QMessageBox::warning(this,
                             tr("Image error"),
                             tr("Failed to construct QImage from decoded data."));
        return false;
    }

    m_currentImage = img.copy();
    m_infoLabel->setText(
        tr("%1  (%2 x %3)")
            .arg(label.isEmpty() ? tr("DTX") : label)
            .arg(m_currentImage.width())
            .arg(m_currentImage.height()));
    m_saveBmpButton->setEnabled(true);

    updatePreview();
    return true;
}


