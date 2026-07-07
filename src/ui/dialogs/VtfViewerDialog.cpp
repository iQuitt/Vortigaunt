#include "VtfViewerDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QScrollArea>

#include <vector>

#include "VTFLib.h"

#include "LanguageManager.h"
#include "core/VortigauntLog.h"
#include "utils/Bmp.h"

VtfViewerDialog::VtfViewerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("VTF Viewer - Source Engine"));
    resize(720, 540);

    auto* mainLayout = new QVBoxLayout();

    // Top controls: buttons + info
    auto* topLayout = new QHBoxLayout();
    m_openButton = new QPushButton(tr("Open VTF..."));
    m_saveBmpButton = new QPushButton(tr("Save as 8-bit BMP..."));
    m_saveBmpButton->setEnabled(false);

    m_infoLabel = new QLabel(tr("No file loaded."));
    m_infoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    topLayout->addWidget(m_openButton);
    topLayout->addWidget(m_saveBmpButton);
    topLayout->addWidget(m_infoLabel, 1);

    // Sub-image controls (frame / face / mipmap)
    auto* subLayout = new QHBoxLayout();

    m_frameLabel = new QLabel(tr("Frame:"));
    m_frameSpin = new QSpinBox();
    m_frameSpin->setMinimum(0);
    m_faceLabel = new QLabel(tr("Face:"));
    m_faceSpin = new QSpinBox();
    m_faceSpin->setMinimum(0);
    m_mipLabel = new QLabel(tr("Mipmap:"));
    m_mipSpin = new QSpinBox();
    m_mipSpin->setMinimum(0);

    subLayout->addWidget(m_frameLabel);
    subLayout->addWidget(m_frameSpin);
    subLayout->addWidget(m_faceLabel);
    subLayout->addWidget(m_faceSpin);
    subLayout->addWidget(m_mipLabel);
    subLayout->addWidget(m_mipSpin);
    subLayout->addStretch();

    // Hidden until a multi-frame/face/mip texture is loaded
    m_frameLabel->setVisible(false);
    m_frameSpin->setVisible(false);
    m_faceLabel->setVisible(false);
    m_faceSpin->setVisible(false);
    m_mipLabel->setVisible(false);
    m_mipSpin->setVisible(false);

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
    mainLayout->addLayout(subLayout);
    mainLayout->addWidget(scroll, 1);

    setLayout(mainLayout);

    connect(m_openButton, &QPushButton::clicked, this, &VtfViewerDialog::onOpenVtf);
    connect(m_saveBmpButton, &QPushButton::clicked, this, &VtfViewerDialog::onSaveBmp);
    connect(m_frameSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &VtfViewerDialog::onSubImageChanged);
    connect(m_faceSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &VtfViewerDialog::onSubImageChanged);
    connect(m_mipSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &VtfViewerDialog::onSubImageChanged);
}

VtfViewerDialog::~VtfViewerDialog() = default;

void VtfViewerDialog::onOpenVtf()
{
    QString startDir;
    if (!m_currentVtfPath.isEmpty())
        startDir = QFileInfo(m_currentVtfPath).absolutePath();
    else
        startDir = QDir::homePath();

    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Select VTF file"),
        startDir,
        tr("VTF textures (*.vtf);;All files (*.*)"));

    if (path.isEmpty())
        return;

    loadVtfFile(path);
}

bool VtfViewerDialog::loadVtfFile(const QString& filePath)
{
    if (filePath.isEmpty())
        return false;

    // Read via QFile and load from memory — avoids 8-bit codepage issues
    // with non-ASCII paths on Windows (VTFLib uses narrow-char fopen).
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        QMessageBox::warning(this, tr("VTF error"), tr("Cannot open file:\n%1").arg(filePath));
        return false;
    }
    QByteArray data = file.readAll();
    file.close();

    auto vtf = std::make_unique<VTFLib::CVTFFile>();
    if (!vtf->Load(static_cast<const vlVoid*>(data.constData()), static_cast<vlUInt>(data.size())))
    {
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("^1VTF load failed:^7 %1")
                                             .arg(QString::fromUtf8(VTFLib::LastError.Get())));
        QMessageBox::warning(this, tr("VTF error"),
                             tr("Failed to load VTF file.\n\n%1")
                                 .arg(QString::fromUtf8(VTFLib::LastError.Get())));
        return false;
    }

    m_vtfFile = std::move(vtf);
    m_currentVtfPath = filePath;

    // Configure sub-image controls without triggering decode storms
    const int frames = static_cast<int>(m_vtfFile->GetFrameCount());
    const int faces = static_cast<int>(m_vtfFile->GetFaceCount());
    const int mips = static_cast<int>(m_vtfFile->GetMipmapCount());

    m_frameSpin->blockSignals(true);
    m_faceSpin->blockSignals(true);
    m_mipSpin->blockSignals(true);

    m_frameSpin->setMaximum(frames > 0 ? frames - 1 : 0);
    m_faceSpin->setMaximum(faces > 0 ? faces - 1 : 0);
    m_mipSpin->setMaximum(mips > 0 ? mips - 1 : 0);
    m_frameSpin->setValue(0);
    m_faceSpin->setValue(0);
    m_mipSpin->setValue(0);

    m_frameSpin->blockSignals(false);
    m_faceSpin->blockSignals(false);
    m_mipSpin->blockSignals(false);

    m_frameLabel->setVisible(frames > 1);
    m_frameSpin->setVisible(frames > 1);
    m_faceLabel->setVisible(faces > 1);
    m_faceSpin->setVisible(faces > 1);
    m_mipLabel->setVisible(mips > 1);
    m_mipSpin->setVisible(mips > 1);

    if (!decodeCurrentImage())
        return false;

    updateInfoLabel();
    return true;
}

void VtfViewerDialog::onSubImageChanged()
{
    if (!m_vtfFile)
        return;

    if (decodeCurrentImage())
        updateInfoLabel();
}

bool VtfViewerDialog::decodeCurrentImage()
{
    if (!m_vtfFile)
        return false;

    const vlUInt uiFrame = static_cast<vlUInt>(m_frameSpin->value());
    const vlUInt uiFace = static_cast<vlUInt>(m_faceSpin->value());
    const vlUInt uiMip = static_cast<vlUInt>(m_mipSpin->value());

    vlUInt uiWidth = 0, uiHeight = 0, uiDepth = 0;
    VTFLib::CVTFFile::ComputeMipmapDimensions(
        m_vtfFile->GetWidth(), m_vtfFile->GetHeight(), m_vtfFile->GetDepth(),
        uiMip, uiWidth, uiHeight, uiDepth);

    if (uiWidth == 0 || uiHeight == 0)
        return false;

    vlByte* lpData = m_vtfFile->GetData(uiFrame, uiFace, 0, uiMip);
    if (lpData == nullptr)
    {
        QMessageBox::warning(this, tr("VTF error"), tr("No image data for the selected frame/face/mipmap."));
        return false;
    }

    std::vector<vlByte> rgba(static_cast<size_t>(uiWidth) * uiHeight * 4);
    if (!VTFLib::CVTFFile::ConvertToRGBA8888(lpData, rgba.data(), uiWidth, uiHeight, m_vtfFile->GetFormat()))
    {
        QMessageBox::warning(this, tr("VTF error"),
                             tr("Failed to decode image data.\n\n%1")
                                 .arg(QString::fromUtf8(VTFLib::LastError.Get())));
        return false;
    }

    QImage img(rgba.data(), static_cast<int>(uiWidth), static_cast<int>(uiHeight),
               static_cast<int>(uiWidth) * 4, QImage::Format_RGBA8888);
    if (img.isNull())
    {
        QMessageBox::warning(this, tr("Image error"), tr("Failed to construct image from decoded data."));
        return false;
    }

    m_currentImage = img.copy(); // detach from the temporary buffer
    m_saveBmpButton->setEnabled(true);

    updatePreview();
    return true;
}

void VtfViewerDialog::updateInfoLabel()
{
    if (!m_vtfFile)
    {
        m_infoLabel->setText(tr("No file loaded."));
        return;
    }

    const auto& formatInfo = VTFLib::CVTFFile::GetImageFormatInfo(m_vtfFile->GetFormat());
    const QString formatName = formatInfo.lpName ? QString::fromUtf8(formatInfo.lpName) : tr("Unknown");

    m_infoLabel->setText(
        tr("%1  |  %2 x %3  |  %4  |  v%5.%6  |  %7 frame(s), %8 mipmap(s)")
            .arg(QFileInfo(m_currentVtfPath).fileName())
            .arg(m_currentImage.width())
            .arg(m_currentImage.height())
            .arg(formatName)
            .arg(m_vtfFile->GetMajorVersion())
            .arg(m_vtfFile->GetMinorVersion())
            .arg(m_vtfFile->GetFrameCount())
            .arg(m_vtfFile->GetMipmapCount()));
}

void VtfViewerDialog::updatePreview()
{
    if (m_currentImage.isNull())
    {
        m_imageLabel->setPixmap(QPixmap());
        return;
    }

    // Fit into the scroll area while keeping aspect ratio; never upscale
    const QSize areaSize = m_imageLabel->parentWidget()->size();
    QPixmap pix = QPixmap::fromImage(m_currentImage);
    if (!areaSize.isEmpty() &&
        (pix.width() > areaSize.width() - 20 || pix.height() > areaSize.height() - 20))
    {
        pix = pix.scaled(areaSize - QSize(20, 20),
                         Qt::KeepAspectRatio,
                         Qt::SmoothTransformation);
    }
    m_imageLabel->setPixmap(pix);
}

void VtfViewerDialog::onSaveBmp()
{
    if (m_currentImage.isNull())
        return;

    QString baseName = m_currentVtfPath.isEmpty()
                           ? QStringLiteral("texture")
                           : QFileInfo(m_currentVtfPath).completeBaseName();

    QString suggested = baseName + QStringLiteral(".bmp");

    const QString outPath = QFileDialog::getSaveFileName(
        this,
        tr("Save as 8-bit BMP"),
        QDir::home().filePath(suggested),
        tr("Bitmap images (*.bmp)"));

    if (outPath.isEmpty())
        return;

    QImage imageToSave = m_currentImage;

    if (imageToSave.width() > 512 || imageToSave.height() > 512)
    {
        const auto answer = QMessageBox::question(
            this,
            tr("GoldSrc Size Limit"),
            tr("The image size (%1 x %2) does not fit GoldSrc texture limits (max 512 x 512).\n\n"
               "Do you want to save it resized to 512 x 512?")
                .arg(imageToSave.width())
                .arg(imageToSave.height()),
            QMessageBox::Yes | QMessageBox::No,
            QMessageBox::Yes);

        if (answer == QMessageBox::Yes)
        {
            imageToSave = imageToSave.scaled(512, 512, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        }
    }

    QImage argb = imageToSave.convertToFormat(QImage::Format_ARGB32);

    if (imageToSave.hasAlphaChannel()) {
        for (int y = 0; y < argb.height(); ++y) {
            QRgb* scanLine = reinterpret_cast<QRgb*>(argb.scanLine(y));
            for (int x = 0; x < argb.width(); ++x) {
                if (qAlpha(scanLine[x]) < 5) {
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
        QMessageBox::warning(this, tr("Save error"), tr("Failed to save BMP file."));
        return;
    }

    QMessageBox::information(this, tr("Saved"), tr("Saved 8-bit BMP:\n%1").arg(outPath));
}
