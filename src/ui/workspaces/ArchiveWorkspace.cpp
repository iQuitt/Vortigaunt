// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "ArchiveWorkspace.h"
#include "ArchiveSource.h"

#include "core/VortigauntLog.h"
#include "ui/UiUtils.h"
#include "ui/workspaces/TextureWorkspace.h"
#include "utils/FileIO.h"

#include <QApplication>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHash>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QSplitter>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <fstream>
#include <thread>

namespace
{

enum class EntryKind
{
    Texture,
    Sprite,
    Model,
    Sound,
    Text,
    Other,
};

// sorts the size column by bytes, not by its text
class EntryItem : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;
    bool operator<(const QTreeWidgetItem& other) const override
    {
        const int column = treeWidget() ? treeWidget()->sortColumn() : 0;
        if (column == 3)
            return data(3, Qt::UserRole).toULongLong() < other.data(3, Qt::UserRole).toULongLong();
        return text(column).compare(other.text(column), Qt::CaseInsensitive) < 0;
    }
};

constexpr int kIndexRole = Qt::UserRole;
constexpr int kFolderRole = Qt::UserRole + 1;
constexpr int kKindRole = Qt::UserRole + 2;

// bigger entries are not read just to be shown
constexpr qint64 kMaxPreviewBytes = 64ll * 1024 * 1024;
constexpr qint64 kMaxTextPreviewBytes = 256 * 1024;

EntryKind kindOf(const QString& ext)
{
	static const QStringList textures = { "dtx", "dds", "tga", "bmp", "png", "jpg", "jpeg", "vtf", "pcx" };// supported by TextureWorkspace
    static const QStringList models = { "ltb", "mdl", "gr2", "fbx", "glb", "gltf", "obj", "smd", "msh", "ltt" };
    static const QStringList sounds = { "wav", "mp3", "ogg", "flac" };
    static const QStringList texts = { "txt", "cfg", "ini", "qc", "lua", "json", "xml", "csv", "res", "vmt", "vdf", "scr", "fx", "msk" };
    if (textures.contains(ext)) return EntryKind::Texture;
    if (ext == "spr") return EntryKind::Sprite;
    if (models.contains(ext)) return EntryKind::Model;
    if (sounds.contains(ext)) return EntryKind::Sound;
    if (texts.contains(ext)) return EntryKind::Text;
    return EntryKind::Other;
}

} // namespace

ArchiveWorkspace::ArchiveWorkspace(std::function<QString()> outputDir, QWidget* parent)
    : QWidget(parent)
    , m_outputDir(std::move(outputDir))
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    layout->addWidget(splitter);

    // left: archives and their folders
    auto* left = new QWidget(splitter);
    auto* leftLayout = new QVBoxLayout(left);
    leftLayout->setContentsMargins(4, 4, 4, 4);
    leftLayout->addWidget(new QLabel(tr("Open archives"), left));
    m_archiveList = new QListWidget(left);
    m_archiveList->setMaximumHeight(140);
    leftLayout->addWidget(m_archiveList);
    leftLayout->addWidget(new QLabel(tr("Folders"), left));
    m_folderTree = new QTreeWidget(left);
    m_folderTree->setHeaderHidden(true);
    leftLayout->addWidget(m_folderTree, 1);

    // centre: the files
    auto* centre = new QWidget(splitter);
    auto* centreLayout = new QVBoxLayout(centre);
    centreLayout->setContentsMargins(4, 4, 4, 4);
    auto* filterRow = new QHBoxLayout();
    m_searchEdit = new QLineEdit(centre);
    m_searchEdit->setPlaceholderText(tr("Search (name, or a pattern like *.dtx)"));
    m_searchEdit->setClearButtonEnabled(true);
    filterRow->addWidget(m_searchEdit, 1);
    m_typeFilter = new QComboBox(centre);
    m_typeFilter->addItem(tr("All types"), -1);
    m_typeFilter->addItem(tr("Textures"), static_cast<int>(EntryKind::Texture));
    m_typeFilter->addItem(tr("Sprites"), static_cast<int>(EntryKind::Sprite));
    m_typeFilter->addItem(tr("Models"), static_cast<int>(EntryKind::Model));
    m_typeFilter->addItem(tr("Sounds"), static_cast<int>(EntryKind::Sound));
    m_typeFilter->addItem(tr("Text"), static_cast<int>(EntryKind::Text));
    m_typeFilter->addItem(tr("Other"), static_cast<int>(EntryKind::Other));
    filterRow->addWidget(m_typeFilter);
    centreLayout->addLayout(filterRow);

    m_entryList = new QTreeWidget(centre);
    m_entryList->setColumnCount(4);
    m_entryList->setHeaderLabels({ tr("Name"), tr("Folder"), tr("Type"), tr("Size") });
    m_entryList->setRootIsDecorated(false);
    m_entryList->setUniformRowHeights(true);
    m_entryList->setAlternatingRowColors(true);
    m_entryList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_entryList->setSortingEnabled(true);
    m_entryList->sortByColumn(1, Qt::AscendingOrder);
    m_entryList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_entryList->header()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_entryList->header()->resizeSection(1, 240);
    centreLayout->addWidget(m_entryList, 1);

    auto* bottomRow = new QHBoxLayout();
    m_countLabel = new QLabel(centre);
    m_countLabel->setForegroundRole(QPalette::PlaceholderText);
    bottomRow->addWidget(m_countLabel, 1);
    m_progress = new QProgressBar(centre);
    m_progress->setMaximumWidth(220);
    m_progress->setVisible(false);
    bottomRow->addWidget(m_progress);
    centreLayout->addLayout(bottomRow);

    // right: preview and what to do with the selection
    auto* right = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(right);
    rightLayout->setContentsMargins(4, 4, 4, 4);
    m_previewStack = new QStackedWidget(right);
    m_previewNote = new QLabel(right);
    m_previewNote->setAlignment(Qt::AlignCenter);
    m_previewNote->setWordWrap(true);
    m_previewNote->setForegroundRole(QPalette::PlaceholderText);
    m_previewStack->addWidget(m_previewNote);
    m_previewImage = new QLabel(right);
    m_previewImage->setAlignment(Qt::AlignCenter);
    m_previewImage->setMinimumSize(160, 160);
    m_previewImage->setBackgroundRole(QPalette::Base);
    m_previewImage->setAutoFillBackground(true);
    m_previewStack->addWidget(m_previewImage);
    m_previewText = new QPlainTextEdit(right);
    m_previewText->setReadOnly(true);
    m_previewText->setFont(QFont("Consolas", 9));
    m_previewStack->addWidget(m_previewText);
    rightLayout->addWidget(m_previewStack, 1);

    m_infoLabel = new QLabel(right);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    rightLayout->addWidget(m_infoLabel);
    m_openButton = new QPushButton(tr("Open in viewer"), right);
    m_extractButton = new QPushButton(tr("Extract selected"), right);
    m_convertButton = new QPushButton(tr("Send to Convert list"), right);
    m_convertButton->setToolTip(tr("Extract the selected files to the output folder and add them to the Convert list"));
    rightLayout->addWidget(m_openButton);
    rightLayout->addWidget(m_extractButton);
    rightLayout->addWidget(m_convertButton);
    m_wadButton = new QPushButton(tr("Send to WAD"), right);
    m_wadButton->setToolTip(tr("Add the selected textures to the WAD open in the WAD tab"));
    rightLayout->addWidget(m_wadButton);

    splitter->addWidget(left);
    splitter->addWidget(centre);
    splitter->addWidget(right);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 3);
    splitter->setStretchFactor(2, 1);
    splitter->setSizes({ 260, 760, 320 });

    connect(m_archiveList, &QListWidget::currentRowChanged, this, &ArchiveWorkspace::showArchive);
    connect(m_folderTree, &QTreeWidget::currentItemChanged, this, &ArchiveWorkspace::applyFilter);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &ArchiveWorkspace::applyFilter);
    connect(m_typeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &ArchiveWorkspace::applyFilter);
    connect(m_entryList, &QTreeWidget::itemSelectionChanged, this, &ArchiveWorkspace::updatePreview);
    connect(m_entryList, &QTreeWidget::itemDoubleClicked, this, &ArchiveWorkspace::openSelectedInViewer);
    connect(m_openButton, &QPushButton::clicked, this, &ArchiveWorkspace::openSelectedInViewer);
    connect(m_extractButton, &QPushButton::clicked, this, &ArchiveWorkspace::extractSelected);
    connect(m_convertButton, &QPushButton::clicked, this, &ArchiveWorkspace::sendSelectedToConvert);
    connect(m_wadButton, &QPushButton::clicked, this, &ArchiveWorkspace::sendSelectedToWad);

    showArchive(-1);
}

ArchiveWorkspace::~ArchiveWorkspace() = default;

ArchiveSource* ArchiveWorkspace::current() const
{
    const int row = m_archiveList->currentRow();
    return row >= 0 && row < static_cast<int>(m_archives.size()) ? m_archives[row].get() : nullptr;
}

bool ArchiveWorkspace::openArchive(const QString& path)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
    for (size_t i = 0; i < m_archives.size(); ++i)
    {
        if (m_archives[i]->filePath().compare(absolute, Qt::CaseInsensitive) == 0)
        {
            m_archiveList->setCurrentRow(static_cast<int>(i));
            return true;
        }
    }

    QString error;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    std::unique_ptr<ArchiveSource> source = ArchiveSource::open(absolute, error);
    QApplication::restoreOverrideCursor();
    if (!source)
    {
        QMessageBox::warning(this, tr("Cannot open archive"), QStringLiteral("%1\n\n%2").arg(QFileInfo(path).fileName(), error));
        return false;
    }

    VortigauntLog::Vortigaunt_Printf(QStringLiteral("Opened %1: %2 files").arg(QFileInfo(absolute).fileName()).arg(source->entries().size()));
    auto* item = new QListWidgetItem(QStringLiteral("%1\n%2").arg(QFileInfo(absolute).fileName(), source->typeName()));
    item->setToolTip(QDir::toNativeSeparators(absolute));
    m_archives.push_back(std::move(source));
    m_archiveList->addItem(item);
    m_archiveList->setCurrentRow(m_archiveList->count() - 1);
    return true;
}

void ArchiveWorkspace::openArchiveDialog()
{
    const QStringList files = QFileDialog::getOpenFileNames(this, tr("Open Archives"), QString(),
        tr("Archives (*.rez *.pak *.xfs *.vpk *.gma);;All files (*.*)"));
    for (const QString& file : files)
        openArchive(file);
}

void ArchiveWorkspace::closeCurrentArchive()
{
    const int row = m_archiveList->currentRow();
    if (m_busy || row < 0)
        return;
    m_entryList->clear();
    m_archives.erase(m_archives.begin() + row);
    delete m_archiveList->takeItem(row);
    showArchive(m_archiveList->currentRow());
}

void ArchiveWorkspace::showArchive(int index)
{
    m_entryList->clear();
    m_folderTree->clear();
    ArchiveSource* source = index >= 0 && index < static_cast<int>(m_archives.size()) ? m_archives[index].get() : nullptr;
    if (!source)
    {
        m_countLabel->setText(tr("Open an archive from the ribbon, or drop one here."));
        updatePreview();
        return;
    }

    m_entryList->setSortingEnabled(false);
    const std::vector<ArchiveSource::Entry>& entries = source->entries();
    QList<QTreeWidgetItem*> items;
    items.reserve(static_cast<qsizetype>(entries.size()));
    for (size_t i = 0; i < entries.size(); ++i)
    {
        const QString& path = entries[i].path;
        const int slash = path.lastIndexOf('/');
        const QString name = slash >= 0 ? path.mid(slash + 1) : path;
        const QString folder = slash >= 0 ? path.left(slash) : QString();
        const QString ext = QFileInfo(name).suffix().toLower();
        auto* item = new EntryItem();
        item->setText(0, name);
        item->setText(1, folder);
        item->setText(2, ext.toUpper());
        item->setText(3, UiUtils::formatSize(entries[i].size));
        item->setData(3, Qt::UserRole, static_cast<qulonglong>(entries[i].size));
        item->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
        item->setData(0, kIndexRole, static_cast<qulonglong>(i));
        item->setData(0, kFolderRole, folder);
        item->setData(0, kKindRole, static_cast<int>(kindOf(ext)));
        items << item;
    }
    m_entryList->addTopLevelItems(items);
    m_entryList->setSortingEnabled(true);

    buildFolderTree();
    applyFilter();
}

void ArchiveWorkspace::buildFolderTree()
{
    ArchiveSource* source = current();
    auto* root = new QTreeWidgetItem(m_folderTree, { QFileInfo(source->filePath()).fileName() });
    root->setData(0, Qt::UserRole, QString());
    QHash<QString, QTreeWidgetItem*> folders;
    for (const ArchiveSource::Entry& entry : source->entries())
    {
        const int slash = entry.path.lastIndexOf('/');
        if (slash <= 0)
            continue;
        QTreeWidgetItem* parent = root;
        QString sofar;
        for (const QString& part : entry.path.left(slash).split('/', Qt::SkipEmptyParts))
        {
            sofar = sofar.isEmpty() ? part : sofar + '/' + part;
            QTreeWidgetItem*& item = folders[sofar.toLower()];
            if (!item)
            {
                item = new QTreeWidgetItem(parent, { part });
                item->setData(0, Qt::UserRole, sofar);
            }
            parent = item;
        }
    }
    m_folderTree->sortItems(0, Qt::AscendingOrder);
    root->setExpanded(true);
    m_folderTree->setCurrentItem(root);
}

void ArchiveWorkspace::applyFilter()
{
    QTreeWidgetItem* folderItem = m_folderTree->currentItem();
    const QString folder = folderItem ? folderItem->data(0, Qt::UserRole).toString() : QString();
    const QString search = m_searchEdit->text().trimmed();
    const bool pattern = search.contains('*') || search.contains('?');
    const QRegularExpression wildcard(QRegularExpression::wildcardToRegularExpression(search),
                                      QRegularExpression::CaseInsensitiveOption);
    const int kind = m_typeFilter->currentData().toInt();

    int shown = 0;
    for (int i = 0; i < m_entryList->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = m_entryList->topLevelItem(i);
        const QString itemFolder = item->data(0, kFolderRole).toString();
        bool visible = folder.isEmpty() || itemFolder.compare(folder, Qt::CaseInsensitive) == 0 ||
                       itemFolder.startsWith(folder + '/', Qt::CaseInsensitive);
        if (visible && !search.isEmpty())
            visible = pattern ? wildcard.match(item->text(0)).hasMatch() : item->text(0).contains(search, Qt::CaseInsensitive);
        if (visible && kind >= 0)
            visible = item->data(0, kKindRole).toInt() == kind;
        item->setHidden(!visible);
        shown += visible ? 1 : 0;
    }
    if (current())
        m_countLabel->setText(tr("%1 of %2 files").arg(shown).arg(m_entryList->topLevelItemCount()));
}

std::vector<size_t> ArchiveWorkspace::selectedEntries() const
{
    std::vector<size_t> indices;
    for (QTreeWidgetItem* item : m_entryList->selectedItems())
    {
        if (!item->isHidden())
            indices.push_back(static_cast<size_t>(item->data(0, kIndexRole).toULongLong()));
    }
    return indices;
}

void ArchiveWorkspace::updatePreview()
{
    const std::vector<size_t> selected = selectedEntries();
    ArchiveSource* source = current();
    m_openButton->setEnabled(false);
    m_extractButton->setEnabled(source && !selected.empty() && !m_busy);
    m_convertButton->setEnabled(source && !selected.empty() && !m_busy);
    bool textures = false;
    for (size_t index : selected)
        textures = textures || (source && TextureWorkspace::isTexture(source->entries()[index].path));
    m_wadButton->setEnabled(textures && !m_busy);

    if (!source || selected.size() != 1)
    {
        m_previewNote->setText(source && selected.size() > 1 ? tr("%1 files selected").arg(selected.size())
                                                             : tr("Select a file to preview it."));
        m_infoLabel->clear();
        m_previewStack->setCurrentWidget(m_previewNote);
        return;
    }

    const ArchiveSource::Entry& entry = source->entries()[selected.front()];
    const QString ext = QFileInfo(entry.path).suffix().toLower();
    const EntryKind kind = kindOf(ext);
    m_infoLabel->setText(QStringLiteral("<b>%1</b><br>%2<br>%3")
                             .arg(QFileInfo(entry.path).fileName().toHtmlEscaped(), entry.path.toHtmlEscaped(), UiUtils::formatSize(entry.size)));
    m_openButton->setEnabled(kind == EntryKind::Texture || kind == EntryKind::Sprite);

    if ((kind != EntryKind::Texture && kind != EntryKind::Text) || entry.size > static_cast<quint64>(kMaxPreviewBytes))
    {
        m_previewNote->setText(kind == EntryKind::Sprite ? tr("Open the sprite in the viewer to see its frames.")
                                                         : tr("No preview for this type."));
        m_previewStack->setCurrentWidget(m_previewNote);
        return;
    }

    QByteArray data;
    if (!source->read(selected.front(), data))
    {
        m_previewNote->setText(tr("This file could not be read from the archive."));
        m_previewStack->setCurrentWidget(m_previewNote);
        return;
    }

    if (kind == EntryKind::Text)
    {
        m_previewText->setPlainText(QString::fromUtf8(data.left(kMaxTextPreviewBytes)));
        m_previewStack->setCurrentWidget(m_previewText);
        return;
    }

    const QImage image = TextureWorkspace::decode(ext, data).image;
    if (image.isNull())
    {
        m_previewNote->setText(tr("This texture could not be decoded."));
        m_previewStack->setCurrentWidget(m_previewNote);
        return;
    }
    QPixmap pixmap = QPixmap::fromImage(image.convertToFormat(QImage::Format_RGB32));
    const QSize room = m_previewStack->size() - QSize(8, 8);
    if (pixmap.width() > room.width() || pixmap.height() > room.height())
        pixmap = pixmap.scaled(room, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    m_previewImage->setPixmap(pixmap);
    m_infoLabel->setText(m_infoLabel->text() + QStringLiteral("<br>%1 x %2").arg(image.width()).arg(image.height()));
    m_previewStack->setCurrentWidget(m_previewImage);
}

void ArchiveWorkspace::openSelectedInViewer()
{
    const std::vector<size_t> selected = selectedEntries();
    ArchiveSource* source = current();
    if (!source || selected.empty())
        return;

    // every selected texture goes to the Textures tab
    bool textures = false;
    for (size_t index : selected)
    {
        const QString name = QFileInfo(source->entries()[index].path).fileName();
        QByteArray data;
        if (TextureWorkspace::isTexture(name) && source->read(index, data))
        {
            emit openTexture(name, data);
            textures = true;
        }
    }
    if (textures || selected.size() != 1)
        return;

    const ArchiveSource::Entry& entry = source->entries()[selected.front()];
    const QString name = QFileInfo(entry.path).fileName();
    const QString ext = QFileInfo(name).suffix().toLower();
    QByteArray data;
    if (!source->read(selected.front(), data))
        return;

    if (ext == "spr")
    {
        // the sprite viewer reads files
        const QString folder = QDir::temp().filePath("VortigauntArchive");
        QDir().mkpath(folder);
        const QString file = QDir(folder).filePath(ArchiveSource::safeRelativePath(name));
        std::ofstream out = FileIO::openWrite(file.toStdString(), std::ios::binary);
        out.write(data.constData(), data.size());
        out.close();
        emit openSprite(file);
    }
}

void ArchiveWorkspace::extractSelected()
{
    const std::vector<size_t> selected = selectedEntries();
    if (!selected.empty())
        extract(selected, {});
}

void ArchiveWorkspace::extractAll()
{
    ArchiveSource* source = current();
    if (!source)
        return;
    std::vector<size_t> all(source->entries().size());
    for (size_t i = 0; i < all.size(); ++i)
        all[i] = i;
    extract(all, {});
}

void ArchiveWorkspace::sendSelectedToConvert()
{
    const std::vector<size_t> selected = selectedEntries();
    if (!selected.empty())
        extract(selected, [this](const QStringList& files) { emit addToConvert(files); });
}

void ArchiveWorkspace::sendSelectedToWad()
{
    ArchiveSource* source = current();
    if (!source)
        return;
    QList<QPair<QString, QImage>> images;
    for (size_t index : selectedEntries())
    {
        const QFileInfo name(source->entries()[index].path);
        QByteArray data;
        if (!TextureWorkspace::isTexture(name.fileName()) || !source->read(index, data))
            continue;
        const TextureWorkspace::Decoded decoded = TextureWorkspace::decode(name.suffix().toLower(), data);
        if (!decoded.image.isNull())
            images.append({ name.completeBaseName(),
                            decoded.alphaIsGloss ? decoded.image.convertToFormat(QImage::Format_RGB32) : decoded.image });
    }
    if (!images.isEmpty())
        emit sendToWad(images);
}

void ArchiveWorkspace::setBusy(bool busy)
{
    m_busy = busy;
    m_archiveList->setEnabled(!busy);
    m_progress->setVisible(busy);
    updatePreview();
}

void ArchiveWorkspace::extract(const std::vector<size_t>& indices, const std::function<void(const QStringList&)>& done)
{
    ArchiveSource* source = current();
    if (!source || m_busy || indices.empty())
        return;

    // each archive into a folder of its own name
    const QString root = QDir(m_outputDir()).filePath(QFileInfo(source->filePath()).completeBaseName());
    m_progress->setRange(0, static_cast<int>(indices.size()));
    m_progress->setValue(0);
    setBusy(true);
    VortigauntLog::Vortigaunt_Printf(QStringLiteral("Extracting %1 file(s) from %2 to %3")
                                         .arg(indices.size()).arg(QFileInfo(source->filePath()).fileName(), QDir::toNativeSeparators(root)));

    std::thread([this, source, indices, root, done]() {
        QStringList written;
        int failed = 0;
        for (size_t i = 0; i < indices.size(); ++i)
        {
            const ArchiveSource::Entry& entry = source->entries()[indices[i]];
            const QString target = QDir(root).filePath(ArchiveSource::safeRelativePath(entry.path));
            QByteArray data;
            bool ok = source->read(indices[i], data);
            if (ok)
            {
                QDir().mkpath(QFileInfo(target).absolutePath());
                std::ofstream out = FileIO::openWrite(target.toStdString(), std::ios::binary);
                out.write(data.constData(), data.size());
                ok = out.good();
            }
            if (ok)
            {
                written << target;
            }
            else
            {
                failed++;
                VortigauntLog::Vortigaunt_Printf(QStringLiteral("ERROR: Could not extract %1").arg(entry.path));
            }
            if (i % 16 == 0 || i + 1 == indices.size())
            {
                const int value = static_cast<int>(i + 1);
                QMetaObject::invokeMethod(m_progress, [this, value]() { m_progress->setValue(value); }, Qt::QueuedConnection);
            }
        }

        QMetaObject::invokeMethod(this, [this, written, failed, root, done]() {
            setBusy(false);
            VortigauntLog::Vortigaunt_Printf(failed == 0
                ? QStringLiteral("^2Extracted %1 file(s) to %2^7").arg(written.size()).arg(QDir::toNativeSeparators(root))
                : QStringLiteral("^3Extracted %1 file(s), %2 failed^7").arg(written.size()).arg(failed));
            m_countLabel->setText(tr("Extracted %1 file(s) to %2").arg(written.size()).arg(QDir::toNativeSeparators(root)));
            if (done)
                done(written);
        }, Qt::QueuedConnection);
    }).detach();
}
