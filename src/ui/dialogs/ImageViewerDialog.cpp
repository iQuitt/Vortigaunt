#include "ImageViewerDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QScrollArea>

#include "LanguageManager.h"

ImageViewerDialog::ImageViewerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Image Viewer"));
    resize(640, 480);

    auto* mainLayout = new QVBoxLayout();

    auto* topLayout = new QHBoxLayout();
    m_openButton = new QPushButton(tr("Open Image..."));
    m_saveAsButton = new QPushButton(tr("Save As..."));
    m_saveAsButton->setEnabled(false);

    m_infoLabel = new QLabel(tr("No file loaded."));
    m_infoLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    topLayout->addWidget(m_openButton);
    topLayout->addWidget(m_saveAsButton);
    topLayout->addWidget(m_infoLabel, 1);

    m_imageLabel = new QLabel();
    m_imageLabel->setAlignment(Qt::AlignHCenter);
    m_imageLabel->setBackgroundRole(QPalette::Base);
    m_imageLabel->setScaledContents(false);

    auto* scroll = new QScrollArea();
    scroll->setWidget(m_imageLabel);
    scroll->setAlignment(Qt::AlignCenter);

    mainLayout->addLayout(topLayout);
    mainLayout->addWidget(scroll, 1);

    setLayout(mainLayout);

    connect(m_openButton, &QPushButton::clicked, this, &ImageViewerDialog::onOpenImage);
    connect(m_saveAsButton, &QPushButton::clicked, this, &ImageViewerDialog::onSaveAs);
}

void ImageViewerDialog::onOpenImage()
{
    QString startDir;
    if (!m_currentImagePath.isEmpty())
        startDir = QFileInfo(m_currentImagePath).absolutePath();
    else
        startDir = QDir::homePath();

    const QString path = QFileDialog::getOpenFileName(
        this,
        tr("Select Image file"),
        startDir,
        tr("Images (*.png *.bmp *.tga *.jpg *.jpeg);;All files (*.*)"));

    if (path.isEmpty())
        return;

    loadImageFile(path);
}

bool ImageViewerDialog::loadImageFile(const QString& filePath)
{
    if (filePath.isEmpty())
        return false;

    QImage img(filePath);
    if (img.isNull())
    {
        QMessageBox::warning(this,
                             tr("Image error"),
                             tr("Failed to load image:\n%1").arg(filePath));
        return false;
    }

    m_currentImagePath = filePath;
    m_currentDisplayName = QFileInfo(filePath).fileName();
    m_currentImage = img;

    m_infoLabel->setText(
        tr("%1  (%2 x %3)")
            .arg(m_currentDisplayName)
            .arg(m_currentImage.width())
            .arg(m_currentImage.height()));
    m_saveAsButton->setEnabled(true);

    updatePreview();
    return true;
}

bool ImageViewerDialog::loadImageFromMemory(const QByteArray& data, const QString& displayName)
{
    if (data.isEmpty())
        return false;

    QImage img;
    if (!img.loadFromData(data))
    {
        QMessageBox::warning(this,
                             tr("Image error"),
                             tr("Failed to decode in-memory image."));
        return false;
    }

    m_currentImagePath.clear();
    m_currentDisplayName = displayName.isEmpty() ? tr("In-memory image") : displayName;
    m_currentImage = img;

    m_infoLabel->setText(
        tr("%1  (%2 x %3)")
            .arg(m_currentDisplayName)
            .arg(m_currentImage.width())
            .arg(m_currentImage.height()));
    m_saveAsButton->setEnabled(true);

    updatePreview();
    return true;
}

void ImageViewerDialog::updatePreview()
{
    if (m_currentImage.isNull())
    {
        m_imageLabel->setPixmap(QPixmap());
        return;
    }

    QPixmap pix = QPixmap::fromImage(m_currentImage);

    auto* scroll = qobject_cast<QScrollArea*>(m_imageLabel->parentWidget());
    if (scroll)
    {
        QSize viewport = scroll->viewport()->size();
        if (viewport.isValid() && !viewport.isEmpty() &&
            (pix.width() > viewport.width() || pix.height() > viewport.height()))
        {
            pix = pix.scaled(viewport - QSize(50, 50),
                             Qt::KeepAspectRatio,
                             Qt::SmoothTransformation);
        }
    }

    m_imageLabel->setPixmap(pix);
    m_imageLabel->resize(pix.size());
}

void ImageViewerDialog::onSaveAs()
{
    if (m_currentImage.isNull())
        return;

    QString baseName;
    if (!m_currentImagePath.isEmpty())
        baseName = QFileInfo(m_currentImagePath).completeBaseName();
    else if (!m_currentDisplayName.isEmpty())
        baseName = QFileInfo(m_currentDisplayName).completeBaseName();
    else
        baseName = QStringLiteral("image");

    const QString outPath = QFileDialog::getSaveFileName(
        this,
        tr("Save Image"),
        QDir::home().filePath(baseName + ".png"),
        tr("PNG images (*.png);;BMP images (*.bmp);;TGA images (*.tga);;JPEG images (*.jpg)"));

    if (outPath.isEmpty())
        return;

    if (!m_currentImage.save(outPath))
    {
        QMessageBox::warning(this,
                             tr("Save error"),
                             tr("Failed to save image to:\n%1").arg(outPath));
        return;
    }

    QMessageBox::information(this,
                             tr("Saved"),
                             tr("Image saved to:\n%1").arg(outPath));
}
