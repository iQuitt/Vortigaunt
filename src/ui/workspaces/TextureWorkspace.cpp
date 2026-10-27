// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "TextureWorkspace.h"

#include "core/VortigauntLog.h"
#include "core/converters/DtxConverter.h"
#include "ui/UiUtils.h"
#include "ui/widgets/TextureView.h"
#include "utils/Bmp.h"
#include "utils/FileIO.h"
#include "utils/ImageUtils.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSet>
#include <QSpinBox>
#include <QSplitter>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <cmath>
#include <fstream>
#include <mutex>
#include <thread>

namespace
{

constexpr int kIndexRole = Qt::UserRole;
constexpr int kThumbnailQueuedRole = Qt::UserRole + 1;
constexpr int kThumbnailSize = 96;

// thumbnails are not made of bigger files
constexpr qint64 kMaxThumbnailBytes = 64ll * 1024 * 1024;

QIcon placeholderIcon()
{
    static const QIcon icon = UiUtils::lineIcon(QStringLiteral("M3 3h18v18H3z M3 16l5-5 4 4 3-3 6 6"), kThumbnailSize / 2);
    return icon;
}

QIcon thumbnailOf(const QImage& image, bool alphaIsGloss)
{
    // small textures are blown up with sharp pixels, big ones smoothed down
    const bool smaller = image.width() < kThumbnailSize && image.height() < kThumbnailSize;
    QImage thumbnail = image.scaled(kThumbnailSize, kThumbnailSize, Qt::KeepAspectRatio,
                                    smaller ? Qt::FastTransformation : Qt::SmoothTransformation);
    if (alphaIsGloss)
        thumbnail = thumbnail.convertToFormat(QImage::Format_RGB32);
    return QIcon(QPixmap::fromImage(thumbnail));
}

bool hasTransparency(const QImage& image)
{
    if (!image.hasAlphaChannel())
        return false;
    const QImage argb = image.convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < argb.height(); ++y)
    {
        const auto* line = reinterpret_cast<const QRgb*>(argb.constScanLine(y));
        for (int x = 0; x < argb.width(); ++x)
        {
            if (qAlpha(line[x]) < 255)
                return true;
        }
    }
    return false;
}

bool isPowerOfTwo(int value)
{
    return value > 0 && (value & (value - 1)) == 0;
}

} // namespace

TextureWorkspace::TextureWorkspace(std::function<QString()> outputDir, QWidget* parent)
    : QWidget(parent)
    , m_outputDir(std::move(outputDir))
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    layout->addWidget(splitter);

    // left: the opened textures
    auto* left = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(4, 4, 4, 4);
    m_searchEdit = new QLineEdit(left);
    m_searchEdit->setPlaceholderText(tr("Search (name, or a pattern like *.vtf)"));
    m_searchEdit->setClearButtonEnabled(true);
    leftLayout->addWidget(m_searchEdit);
    m_list = new QListWidget(left);
    m_list->setObjectName("TextureList");
    m_list->setViewMode(QListView::IconMode);
    m_list->setIconSize(QSize(kThumbnailSize, kThumbnailSize));
    m_list->setGridSize(QSize(kThumbnailSize + 28, kThumbnailSize + 28));
    m_list->setResizeMode(QListView::Adjust);
    m_list->setMovement(QListView::Static);
    m_list->setUniformItemSizes(true);
    m_list->setWordWrap(true);
    m_list->setTextElideMode(Qt::ElideMiddle);
    m_list->setSelectionMode(QAbstractItemView::ExtendedSelection);
    leftLayout->addWidget(m_list, 1);
    auto* bottomRow = new QHBoxLayout();
    m_countLabel = new QLabel(left);
    m_countLabel->setForegroundRole(QPalette::PlaceholderText);
    bottomRow->addWidget(m_countLabel, 1);
    m_progress = new QProgressBar(left);
    m_progress->setMaximumWidth(160);
    m_progress->setVisible(false);
    bottomRow->addWidget(m_progress);
    leftLayout->addLayout(bottomRow);

    // centre: the selected texture
    auto* centre = new QWidget(splitter);
    auto* centreLayout = new QVBoxLayout(centre);
    centreLayout->setContentsMargins(4, 4, 4, 4);
    auto* viewRow = new QHBoxLayout();
    m_channelCombo = new QComboBox(centre);
    m_channelCombo->addItem(tr("Color and alpha"), static_cast<int>(TextureView::Channels::ColorAndAlpha));
    m_channelCombo->addItem(tr("Color"), static_cast<int>(TextureView::Channels::Color));
    m_channelCombo->addItem(tr("Alpha"), static_cast<int>(TextureView::Channels::Alpha));
    viewRow->addWidget(m_channelCombo);
    auto* fitButton = new QPushButton(tr("Fit"), centre);
    auto* actualButton = new QPushButton(tr("1:1"), centre);
    viewRow->addWidget(fitButton);
    viewRow->addWidget(actualButton);
    m_zoomLabel = new QLabel(centre);
    m_zoomLabel->setMinimumWidth(50);
    viewRow->addWidget(m_zoomLabel);
    viewRow->addStretch();
    auto layerSpin = [centre, viewRow](const QString& label, QLabel*& labelOut, QSpinBox*& spinOut) {
        labelOut = new QLabel(label, centre);
        spinOut = new QSpinBox(centre);
        viewRow->addWidget(labelOut);
        viewRow->addWidget(spinOut);
    };
    layerSpin(tr("Frame"), m_frameLabel, m_frameSpin);
    layerSpin(tr("Face"), m_faceLabel, m_faceSpin);
    layerSpin(tr("Mipmap"), m_mipLabel, m_mipSpin);
    centreLayout->addLayout(viewRow);
    m_view = new TextureView(centre);
    centreLayout->addWidget(m_view, 1);

    // right: about it, and saving it for GoldSrc
    auto* right = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(4, 4, 4, 4);
    m_infoLabel = new QLabel(right);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_infoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    rightLayout->addWidget(m_infoLabel);
    rightLayout->addStretch();
    auto* goldSrcNote = new QLabel(tr("BMPs are 8-bit with a palette of 256 colors, at most 512 x 512."), right);
    goldSrcNote->setWordWrap(true);
    goldSrcNote->setForegroundRole(QPalette::PlaceholderText);
    rightLayout->addWidget(goldSrcNote);
    m_maskedCheck = new QCheckBox(tr("Transparent pixels as mask (blue)"), right);
    m_maskedCheck->setToolTip(tr("For $texrendermode masked: pixels with alpha below 128 get palette index 255. "
                                 "Not used for DTX, whose alpha is a gloss mask."));
    m_powerOfTwoCheck = new QCheckBox(tr("Power of two sides"), right);
    rightLayout->addWidget(m_maskedCheck);
    rightLayout->addWidget(m_powerOfTwoCheck);
    m_saveBmpButton = new QPushButton(tr("Save as BMP..."), right);
    m_savePngButton = new QPushButton(tr("Save as PNG..."), right);
    m_exportButton = new QPushButton(tr("Export selected to output"), right);
    m_exportButton->setToolTip(tr("Write the selected textures as BMPs to the output folder"));
    rightLayout->addWidget(m_saveBmpButton);
    rightLayout->addWidget(m_savePngButton);
    rightLayout->addWidget(m_exportButton);
    m_wadButton = new QPushButton(tr("Send to WAD"), right);
    m_wadButton->setToolTip(tr("Add the selected textures to the WAD open in the WAD tab"));
    rightLayout->addWidget(m_wadButton);

    splitter->addWidget(left);
    splitter->addWidget(centre);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setStretchFactor(2, 1);
    splitter->setSizes({ 330, 760, 280 });

    m_view->zoomChanged = [this]() { m_zoomLabel->setText(QStringLiteral("%1%").arg(qRound(m_view->zoom() * 100))); };
    connect(m_searchEdit, &QLineEdit::textChanged, this, &TextureWorkspace::applyFilter);
    connect(m_list, &QListWidget::itemSelectionChanged, this, &TextureWorkspace::showSelected);
    connect(m_channelCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        m_view->setChannels(static_cast<TextureView::Channels>(m_channelCombo->currentData().toInt()));
    });
    connect(fitButton, &QPushButton::clicked, this, [this]() { m_view->fit(); });
    connect(actualButton, &QPushButton::clicked, this, [this]() { m_view->actualSize(); });
    for (QSpinBox* spin : { m_frameSpin, m_faceSpin, m_mipSpin })
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, &TextureWorkspace::showLayer);
    connect(m_saveBmpButton, &QPushButton::clicked, this, [this]() {
        if (m_decoded.image.isNull())
            return;
        const QString suggested = QDir(m_outputDir()).filePath(QFileInfo(m_textures[m_current].name).completeBaseName() + ".bmp");
        const QString path = QFileDialog::getSaveFileName(this, tr("Save as BMP"), suggested, tr("Bitmap images (*.bmp)"));
        if (path.isEmpty())
            return;
        BMP::GoldSrcTextureOptions options;
        options.masked = m_maskedCheck->isChecked() && !m_decoded.alphaIsGloss;
        options.powerOfTwo = m_powerOfTwoCheck->isChecked();
        BMP::GoldSrcTextureInfo written;
        if (BMP::saveGoldSrcTexture(path.toStdString(), m_decoded.image, options, &written))
            VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2Saved^7 %1 (%2 x %3)").arg(QDir::toNativeSeparators(path)).arg(written.width).arg(written.height));
        else
            QMessageBox::warning(this, tr("Save error"), tr("Failed to save BMP file."));
    });
    connect(m_savePngButton, &QPushButton::clicked, this, [this]() {
        if (m_decoded.image.isNull())
            return;
        const QString suggested = QDir(m_outputDir()).filePath(QFileInfo(m_textures[m_current].name).completeBaseName() + ".png");
        const QString path = QFileDialog::getSaveFileName(this, tr("Save as PNG"), suggested, tr("PNG images (*.png)"));
        if (path.isEmpty())
            return;
        const QImage image = m_decoded.alphaIsGloss ? m_decoded.image.convertToFormat(QImage::Format_RGB32) : m_decoded.image;
        if (image.save(path, "PNG"))
            VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2Saved^7 %1").arg(QDir::toNativeSeparators(path)));
        else
            QMessageBox::warning(this, tr("Save error"), tr("Failed to save PNG file."));
    });
    connect(m_exportButton, &QPushButton::clicked, this, &TextureWorkspace::exportSelectedBmp);
    connect(m_wadButton, &QPushButton::clicked, this, &TextureWorkspace::sendSelectedToWad);

    showSelected();
    applyFilter();
}

TextureWorkspace::~TextureWorkspace() = default;

bool TextureWorkspace::isTexture(const QString& path)
{
    static const QStringList extensions = { "dtx", "vtf", "dds", "tga", "bmp", "png", "jpg", "jpeg" };
    return extensions.contains(QFileInfo(path).suffix().toLower());
}

TextureWorkspace::Decoded TextureWorkspace::decode(const QString& ext, const QByteArray& data, int frame, int face, int mipmap)
{
    Decoded decoded;
    if (data.isEmpty())
        return decoded;
    if (ext == "dtx")
    {
        // the LithTech decoder is not used from two threads at once
        static std::mutex dtxMutex;
        std::lock_guard<std::mutex> lock(dtxMutex);
        DtxConverter decoder;
        std::vector<unsigned int> pixels;
        int width = 0, height = 0;
        if (decoder.DecodeDTXBufferToRGBA(reinterpret_cast<const uint8_t*>(data.constData()), static_cast<size_t>(data.size()),
                                          pixels, width, height) && !pixels.empty())
        {
            decoded.image = QImage(reinterpret_cast<const uchar*>(pixels.data()), width, height, QImage::Format_ARGB32).copy();
        }
        decoded.format = QStringLiteral("DTX");
        decoded.alphaIsGloss = true;
        return decoded;
    }
    if (ext == "vtf")
    {
        ImageUtils::VtfLayers layers;
        decoded.image = ImageUtils::loadVtfFromMemory(data.constData(), static_cast<size_t>(data.size()), frame, face, mipmap, &layers);
        decoded.format = QStringLiteral("VTF %1").arg(layers.format);
        decoded.frames = layers.frames;
        decoded.faces = layers.faces;
        decoded.mipmaps = layers.mipmaps;
        return decoded;
    }
    decoded.image = ImageUtils::loadImageFromMemory(data.constData(), static_cast<size_t>(data.size()), "." + ext.toStdString());
    decoded.format = ext.toUpper();
    return decoded;
}

void TextureWorkspace::addFiles(const QStringList& paths)
{
    for (const QString& path : paths)
    {
        const QFileInfo info(path);
        if (info.isDir())
        {
            // sub-folders are kept in the output
            const QDir root(info.absoluteFilePath());
            QDirIterator it(root.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
            QStringList found;
            while (it.hasNext())
            {
                const QString file = it.next();
                if (isTexture(file))
                    found << file;
            }
            found.sort(Qt::CaseInsensitive);
            for (const QString& file : found)
            {
                const QString relative = QFileInfo(root.relativeFilePath(file)).path();
                addTexture({ QFileInfo(file).fileName(), file, relative == "." ? QString() : relative, {} });
            }
        }
        else if (info.isFile() && isTexture(path))
        {
            addTexture({ info.fileName(), info.absoluteFilePath(), {}, {} });
        }
    }
    loadThumbnails();
    applyFilter();
}

void TextureWorkspace::addFromMemory(const QString& name, const QByteArray& data)
{
    addTexture({ name, {}, {}, data });
    loadThumbnails();
    applyFilter();
}

void TextureWorkspace::addTexture(Texture texture)
{
    for (int i = 0; i < m_list->count(); ++i)
    {
        const Texture& other = m_textures[m_list->item(i)->data(kIndexRole).toInt()];
        if (!texture.path.isEmpty() && other.path.compare(texture.path, Qt::CaseInsensitive) == 0)
            return;
    }
    auto* item = new QListWidgetItem(placeholderIcon(), texture.name);
    item->setToolTip(texture.path.isEmpty() ? texture.name : QDir::toNativeSeparators(texture.path));
    item->setData(kIndexRole, static_cast<int>(m_textures.size()));
    m_textures.push_back(std::move(texture));
    m_list->addItem(item);
    if (m_list->selectedItems().isEmpty())
        m_list->setCurrentItem(item);
}

QByteArray TextureWorkspace::read(const Texture& texture) const
{
    if (texture.path.isEmpty())
        return texture.data;
    QFile file(texture.path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

void TextureWorkspace::loadThumbnails()
{
    // the textures without a thumbnail yet, made in the background one by one
    std::vector<std::pair<int, Texture>> pending;
    for (int i = 0; i < m_list->count(); ++i)
    {
        QListWidgetItem* item = m_list->item(i);
        if (item->data(kThumbnailQueuedRole).toBool())
            continue;
        item->setData(kThumbnailQueuedRole, true);
        const int index = item->data(kIndexRole).toInt();
        pending.emplace_back(index, m_textures[index]);
    }
    if (pending.empty())
        return;

    const int generation = m_thumbnailGeneration;
    std::thread([this, pending = std::move(pending), generation]() {
        for (const auto& [index, texture] : pending)
        {
            QByteArray data;
            if (texture.path.isEmpty())
            {
                data = texture.data;
            }
            else
            {
                QFile file(texture.path);
                if (file.open(QIODevice::ReadOnly) && file.size() <= kMaxThumbnailBytes)
                    data = file.readAll();
            }
            const Decoded decoded = decode(QFileInfo(texture.name).suffix().toLower(), data);
            const QIcon icon = decoded.image.isNull() ? QIcon() : thumbnailOf(decoded.image, decoded.alphaIsGloss);
            const QString size = decoded.image.isNull() ? tr("unreadable") : QStringLiteral("%1 x %2").arg(decoded.image.width()).arg(decoded.image.height());
            QMetaObject::invokeMethod(this, [this, index, icon, size, generation]() {
                if (generation != m_thumbnailGeneration)
                    return;
                for (int i = 0; i < m_list->count(); ++i)
                {
                    QListWidgetItem* item = m_list->item(i);
                    if (item->data(kIndexRole).toInt() != index)
                        continue;
                    if (!icon.isNull())
                        item->setIcon(icon);
                    item->setToolTip(QStringLiteral("%1\n%2").arg(item->toolTip(), size));
                    break;
                }
            }, Qt::QueuedConnection);
        }
    }).detach();
}

void TextureWorkspace::applyFilter()
{
    const QString text = m_searchEdit->text().trimmed();
    QRegularExpression pattern;
    if (text.contains('*') || text.contains('?'))
        pattern = QRegularExpression(QRegularExpression::wildcardToRegularExpression(text), QRegularExpression::CaseInsensitiveOption);
    int shown = 0;
    for (int i = 0; i < m_list->count(); ++i)
    {
        QListWidgetItem* item = m_list->item(i);
        const QString& name = m_textures[item->data(kIndexRole).toInt()].name;
        const bool match = text.isEmpty() || (pattern.isValid() && !pattern.pattern().isEmpty()
                                                  ? pattern.match(name).hasMatch()
                                                  : name.contains(text, Qt::CaseInsensitive));
        item->setHidden(!match);
        shown += match ? 1 : 0;
    }
    if (m_list->count() == 0)
        m_countLabel->setText(tr("No textures open."));
    else if (shown == m_list->count())
        m_countLabel->setText(tr("%n texture(s)", "", m_list->count()));
    else
        m_countLabel->setText(tr("%1 of %2 textures").arg(shown).arg(m_list->count()));
}

std::vector<int> TextureWorkspace::shownTextures(bool selectedOnly) const
{
    std::vector<int> indices;
    for (int i = 0; i < m_list->count(); ++i)
    {
        const QListWidgetItem* item = m_list->item(i);
        if (!item->isHidden() && (!selectedOnly || item->isSelected()))
            indices.push_back(item->data(kIndexRole).toInt());
    }
    return indices;
}

void TextureWorkspace::showSelected()
{
    const QList<QListWidgetItem*> selected = m_list->selectedItems();
    QListWidgetItem* item = m_list->currentItem() && m_list->currentItem()->isSelected() ? m_list->currentItem()
                          : selected.isEmpty() ? nullptr : selected.first();
    const int index = item ? item->data(kIndexRole).toInt() : -1;
    if (index == m_current && index >= 0)
        return;
    m_current = index;
    for (QSpinBox* spin : { m_frameSpin, m_faceSpin, m_mipSpin })
    {
        spin->blockSignals(true);
        spin->setValue(0);
        spin->blockSignals(false);
    }
    showLayer();
}

void TextureWorkspace::showLayer()
{
    const bool keepView = m_current >= 0 && !m_decoded.image.isNull();
    m_decoded = Decoded();
    if (m_current >= 0)
    {
        const Texture& texture = m_textures[m_current];
        m_decoded = decode(QFileInfo(texture.name).suffix().toLower(), read(texture),
                           m_frameSpin->value(), m_faceSpin->value(), m_mipSpin->value());
    }

    auto setLayer = [](QLabel* label, QSpinBox* spin, int count) {
        spin->blockSignals(true);
        spin->setMaximum((std::max)(count - 1, 0));
        spin->blockSignals(false);
        label->setVisible(count > 1);
        spin->setVisible(count > 1);
    };
    setLayer(m_frameLabel, m_frameSpin, m_decoded.frames);
    setLayer(m_faceLabel, m_faceSpin, m_decoded.faces);
    setLayer(m_mipLabel, m_mipSpin, m_decoded.mipmaps);

    // DTX alpha is gloss: shown only when asked for on its own
    const auto channels = static_cast<TextureView::Channels>(m_channelCombo->currentData().toInt());
    m_view->setChannels(m_decoded.alphaIsGloss && channels == TextureView::Channels::ColorAndAlpha ? TextureView::Channels::Color : channels);
    m_view->placeholder = m_current >= 0 ? tr("This texture could not be decoded.") : tr("Open textures from the ribbon, or drop them here.");
    m_view->setImage(m_decoded.image, keepView);
    updateInfo();
}

void TextureWorkspace::updateInfo()
{
    const bool loaded = !m_decoded.image.isNull();
    m_saveBmpButton->setEnabled(loaded && !m_busy);
    m_savePngButton->setEnabled(loaded && !m_busy);
    m_exportButton->setEnabled(!m_textures.empty() && !m_busy);
    m_wadButton->setEnabled(loaded);
    m_maskedCheck->setEnabled(!m_decoded.alphaIsGloss);
    if (m_current < 0)
    {
        m_infoLabel->setText(tr("No texture selected."));
        return;
    }

    const Texture& texture = m_textures[m_current];
    QStringList lines;
    lines << QStringLiteral("<b>%1</b>").arg(texture.name.toHtmlEscaped());
    // a long folder breaks at its separators
    lines << (texture.path.isEmpty() ? tr("From an archive")
                                     : QDir::toNativeSeparators(QFileInfo(texture.path).absolutePath()).toHtmlEscaped().replace(QStringLiteral("\\"), QStringLiteral("\\&#8203;")));
    lines << UiUtils::formatSize(texture.path.isEmpty() ? texture.data.size() : QFileInfo(texture.path).size());
    if (!loaded)
    {
        lines << QString() << tr("This texture could not be decoded.");
        m_infoLabel->setText(lines.join("<br>"));
        return;
    }

    const QImage& image = m_decoded.image;
    lines << QString();
    lines << tr("Size: %1 x %2").arg(image.width()).arg(image.height());
    lines << tr("Format: %1").arg(m_decoded.format.toHtmlEscaped());
    if (m_decoded.frames > 1 || m_decoded.faces > 1 || m_decoded.mipmaps > 1)
        lines << tr("Frames: %1, faces: %2, mipmaps: %3").arg(m_decoded.frames).arg(m_decoded.faces).arg(m_decoded.mipmaps);
    if (m_decoded.alphaIsGloss)
        lines << tr("Alpha: gloss mask");
    else
        lines << (hasTransparency(image) ? tr("Alpha: has transparent pixels") : tr("Alpha: opaque"));

    lines << QString() << QStringLiteral("<b>%1</b>").arg(tr("GoldSrc"));
    if (image.width() > 512 || image.height() > 512)
        lines << tr("Larger than 512: scaled down to fit when saved.");
    else
        lines << tr("Fits the 512 x 512 limit.");
    if (!isPowerOfTwo(image.width()) || !isPowerOfTwo(image.height()))
        lines << tr("Sides are not powers of two.");
    m_infoLabel->setText(lines.join("<br>"));
}

void TextureWorkspace::openFilesDialog()
{
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Open Textures"), QString(),
        tr("Textures (*.dtx *.vtf *.dds *.tga *.bmp *.png *.jpg *.jpeg);;All files (*.*)"));
    addFiles(files);
}

void TextureWorkspace::openFolderDialog()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Open a Folder of Textures"));
    if (!folder.isEmpty())
        addFiles({ folder });
}

void TextureWorkspace::removeSelected()
{
    if (m_busy)
        return;
    // the entries keep their place in m_textures, as the items refer to them by index
    const QList<QListWidgetItem*> selected = m_list->selectedItems();
    for (QListWidgetItem* item : selected)
    {
        Texture& texture = m_textures[item->data(kIndexRole).toInt()];
        texture.path.clear();
        texture.data = QByteArray();
        delete item;
    }
    if (m_list->count() == 0)
    {
        clear();
        return;
    }
    m_current = -1;
    showSelected();
    applyFilter();
}

void TextureWorkspace::clear()
{
    if (m_busy)
        return;
    m_thumbnailGeneration++;
    m_list->clear();
    m_textures.clear();
    m_current = -1;
    showLayer();
    applyFilter();
}

void TextureWorkspace::exportBmp()
{
    exportTextures(shownTextures(false), true);
}

void TextureWorkspace::exportPng()
{
    exportTextures(shownTextures(false), false);
}

void TextureWorkspace::exportSelectedBmp()
{
    exportTextures(shownTextures(true), true);
}

void TextureWorkspace::sendSelectedToWad()
{
    std::vector<int> indices = shownTextures(true);
    if (indices.empty() && m_current >= 0)
        indices.push_back(m_current);
    QList<QPair<QString, QImage>> images;
    for (int index : indices)
    {
        const Texture& texture = m_textures[index];
        const Decoded decoded = decode(QFileInfo(texture.name).suffix().toLower(), read(texture));
        if (!decoded.image.isNull())
            images.append({ QFileInfo(texture.name).completeBaseName(),
                            decoded.alphaIsGloss ? decoded.image.convertToFormat(QImage::Format_RGB32) : decoded.image });
    }
    if (!images.isEmpty())
        emit sendToWad(images);
}

void TextureWorkspace::exportTextures(const std::vector<int>& indices, bool bmp)
{
    if (m_busy || indices.empty())
        return;
    std::vector<Texture> textures;
    for (int index : indices)
        textures.push_back(m_textures[index]);

    BMP::GoldSrcTextureOptions options;
    options.masked = m_maskedCheck->isChecked();
    options.powerOfTwo = m_powerOfTwoCheck->isChecked();
    const QString root = m_outputDir();
    m_progress->setRange(0, static_cast<int>(textures.size()));
    m_progress->setValue(0);
    setBusy(true);
    VortigauntLog::Vortigaunt_Printf(QStringLiteral("Saving %1 texture(s) to %2").arg(textures.size()).arg(QDir::toNativeSeparators(root)));

    std::thread([this, textures = std::move(textures), options, root, bmp]() {
        int written = 0;
        int failed = 0;
        QSet<QString> taken;
        for (size_t i = 0; i < textures.size(); ++i)
        {
            const Texture& texture = textures[i];
            QByteArray data = texture.data;
            if (!texture.path.isEmpty())
            {
                QFile file(texture.path);
                if (file.open(QIODevice::ReadOnly))
                    data = file.readAll();
            }
            const Decoded decoded = decode(QFileInfo(texture.name).suffix().toLower(), data);

            // two textures of one name in one folder do not overwrite each other
            const QString folder = QDir(root).filePath(texture.subfolder);
            const QString base = QFileInfo(texture.name).completeBaseName();
            const QString ext = bmp ? QStringLiteral(".bmp") : QStringLiteral(".png");
            QString target = QDir(folder).filePath(base + ext);
            for (int n = 2; taken.contains(target.toLower()); ++n)
                target = QDir(folder).filePath(QStringLiteral("%1_%2%3").arg(base).arg(n).arg(ext));
            taken.insert(target.toLower());

            bool ok = !decoded.image.isNull() && QDir().mkpath(folder);
            if (ok && bmp)
            {
                BMP::GoldSrcTextureOptions textureOptions = options;
                textureOptions.masked = options.masked && !decoded.alphaIsGloss;
                ok = BMP::saveGoldSrcTexture(target.toStdString(), decoded.image, textureOptions);
            }
            else if (ok)
            {
                ok = (decoded.alphaIsGloss ? decoded.image.convertToFormat(QImage::Format_RGB32) : decoded.image).save(target, "PNG");
            }
            if (ok)
            {
                written++;
            }
            else
            {
                failed++;
                VortigauntLog::Vortigaunt_Printf(QStringLiteral("ERROR: Could not save %1").arg(texture.name));
            }
            const int value = static_cast<int>(i + 1);
            QMetaObject::invokeMethod(m_progress, [this, value]() { m_progress->setValue(value); }, Qt::QueuedConnection);
        }

        QMetaObject::invokeMethod(this, [this, written, failed, root]() {
            setBusy(false);
            VortigauntLog::Vortigaunt_Printf(failed == 0
                ? QStringLiteral("^2Saved %1 texture(s) to %2^7").arg(written).arg(QDir::toNativeSeparators(root))
                : QStringLiteral("^3Saved %1 texture(s), %2 failed^7").arg(written).arg(failed));
            m_countLabel->setText(tr("Saved %1 texture(s) to %2").arg(written).arg(QDir::toNativeSeparators(root)));
        }, Qt::QueuedConnection);
    }).detach();
}

void TextureWorkspace::setBusy(bool busy)
{
    m_busy = busy;
    m_progress->setVisible(busy);
    updateInfo();
}
