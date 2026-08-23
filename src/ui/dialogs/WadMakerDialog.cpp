#include "WadMakerDialog.h"
#include "WadMaker.h"
#include "ui/UiUtils.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QScreen>
#include <QApplication>
#include <QInputDialog>
#include <QToolBar>
#include <QStatusBar>
#include <QFrame>

WadMakerDialog::WadMakerDialog(QWidget* parent)
    : QDialog(parent)
    , m_wadMaker(new WadArchive())
    , m_modified(false)
{
    setWindowTitle(tr("WAD Editor"));
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    setMinimumSize(600, 400);
    
    UiUtils::resizeToScreen(this, 0.7);
    
    setupUi();
    updateTextureCount();
}

WadMakerDialog::~WadMakerDialog()
{
    delete m_wadMaker;
}

void WadMakerDialog::setupUi()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    
    // === Toolbar ===
    auto* toolbarLayout = new QHBoxLayout();
    
    m_newButton = new QPushButton(tr("New"));
    m_newButton->setToolTip(tr("Create new WAD"));
    m_openButton = new QPushButton(tr("Open WAD..."));
    m_openButton->setToolTip(tr("Open existing WAD file"));
    m_saveButton = new QPushButton(tr("Save"));
    m_saveButton->setToolTip(tr("Save WAD file"));
    m_saveButton->setEnabled(false);
    m_saveAsButton = new QPushButton(tr("Save As..."));
    m_saveAsButton->setToolTip(tr("Save WAD to new file"));
    
    toolbarLayout->addWidget(m_newButton);
    toolbarLayout->addWidget(m_openButton);
    toolbarLayout->addWidget(m_saveButton);
    toolbarLayout->addWidget(m_saveAsButton);
    toolbarLayout->addStretch();
    
    // Status label
    m_statusLabel = new QLabel(tr("Textures: 0 / %1").arg(MAX_TEXTURES));
    m_statusLabel->setStyleSheet("font-weight: bold; padding: 4px;");
    toolbarLayout->addWidget(m_statusLabel);
    
    mainLayout->addLayout(toolbarLayout);
    
    // === Main splitter (grid + preview) ===
    auto* splitter = new QSplitter(Qt::Horizontal);
    
    // Left panel: Texture grid
    auto* leftPanel = new QWidget();
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    
    // Filter
    auto* filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel(tr("Filter:")));
    m_filterEdit = new QLineEdit();
    m_filterEdit->setPlaceholderText(tr("Type to filter textures..."));
    m_filterEdit->setClearButtonEnabled(true);
    filterLayout->addWidget(m_filterEdit);
    leftLayout->addLayout(filterLayout);
    
    // Texture grid
    m_textureGrid = new QListWidget();
    m_textureGrid->setViewMode(QListWidget::IconMode);
    m_textureGrid->setIconSize(QSize(THUMBNAIL_SIZE, THUMBNAIL_SIZE));
    m_textureGrid->setSpacing(8);
    m_textureGrid->setResizeMode(QListWidget::Adjust);
    m_textureGrid->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_textureGrid->setMovement(QListWidget::Static);
    m_textureGrid->setWrapping(true);
    m_textureGrid->setUniformItemSizes(true);
    m_textureGrid->setGridSize(QSize(THUMBNAIL_SIZE + 20, THUMBNAIL_SIZE + 30));
    m_textureGrid->setStyleSheet(R"(
        QListWidget {
            background-color: #2d2d2d;
            border: 1px solid #444;
        }
        QListWidget::item {
            color: #fff;
            padding: 4px;
        }
        QListWidget::item:selected {
            background-color: #3d6a99;
            border: 2px solid #5a9fd4;
        }
        QListWidget::item:hover {
            background-color: #3a3a3a;
        }
    )");
    leftLayout->addWidget(m_textureGrid);
    
    // Texture buttons
    auto* buttonLayout = new QHBoxLayout();
    m_addButton = new QPushButton(tr("Add Images..."));
    m_removeButton = new QPushButton(tr("Remove"));
    m_extractButton = new QPushButton(tr("Extract..."));
    m_renameButton = new QPushButton(tr("Rename"));
    
    m_removeButton->setEnabled(false);
    m_extractButton->setEnabled(false);
    m_renameButton->setEnabled(false);
    
    buttonLayout->addWidget(m_addButton);
    buttonLayout->addWidget(m_removeButton);
    buttonLayout->addWidget(m_extractButton);
    buttonLayout->addWidget(m_renameButton);
    buttonLayout->addStretch();
    leftLayout->addLayout(buttonLayout);
    
    splitter->addWidget(leftPanel);
    
    // Right panel: Preview
    auto* rightPanel = new QFrame();
    rightPanel->setFrameStyle(QFrame::StyledPanel);
    rightPanel->setMinimumWidth(280);
    rightPanel->setMaximumWidth(400);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    
    auto* previewTitle = new QLabel(tr("Preview"));
    previewTitle->setStyleSheet("font-weight: bold; font-size: 14px;");
    rightLayout->addWidget(previewTitle);
    
    m_previewLabel = new QLabel();
    m_previewLabel->setMinimumSize(256, 256);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setStyleSheet("background-color: #1a1a1a; border: 1px solid #444;");
    m_previewLabel->setText(tr("Select a texture\nto preview"));
    rightLayout->addWidget(m_previewLabel);
    
    m_previewInfo = new QLabel();
    m_previewInfo->setWordWrap(true);
    m_previewInfo->setStyleSheet("padding: 8px;");
    rightLayout->addWidget(m_previewInfo);
    
    rightLayout->addStretch();
    

    splitter->addWidget(rightPanel);
    splitter->setSizes({900, 480});
    
    mainLayout->addWidget(splitter, 1);
    
    // === Connections ===
    connect(m_newButton, &QPushButton::clicked, this, &WadMakerDialog::onNewWad);
    connect(m_openButton, &QPushButton::clicked, this, &WadMakerDialog::onOpenWad);
    connect(m_saveButton, &QPushButton::clicked, this, &WadMakerDialog::onSaveWad);
    connect(m_saveAsButton, &QPushButton::clicked, this, &WadMakerDialog::onSaveWadAs);
    
    connect(m_addButton, &QPushButton::clicked, this, &WadMakerDialog::onAddImages);
    connect(m_removeButton, &QPushButton::clicked, this, &WadMakerDialog::onRemoveSelected);
    connect(m_extractButton, &QPushButton::clicked, this, &WadMakerDialog::onExtractSelected);
    connect(m_renameButton, &QPushButton::clicked, this, &WadMakerDialog::onRenameSelected);
    
    connect(m_textureGrid, &QListWidget::itemSelectionChanged, this, &WadMakerDialog::onSelectionChanged);
    connect(m_textureGrid, &QListWidget::itemDoubleClicked, this, &WadMakerDialog::onItemDoubleClicked);
    connect(m_filterEdit, &QLineEdit::textChanged, this, &WadMakerDialog::onFilterChanged);
}

void WadMakerDialog::onNewWad()
{
    if (!maybeSave()) return;
    
    m_wadMaker->clear();
    m_currentFilePath.clear();
    m_modified = false;
    m_saveButton->setEnabled(false);
    setWindowTitle(tr("WAD Editor - New"));
    refreshTextureList();
}

void WadMakerDialog::onOpenWad()
{
    if (!maybeSave()) return;
    
    QString path = QFileDialog::getOpenFileName(
        this, tr("Open WAD File"), QString(),
        tr("WAD Files (*.wad);;All Files (*.*)")
    );
    
    if (path.isEmpty()) return;
    
    loadWad(path);
}

bool WadMakerDialog::loadWad(const QString& path)
{

    if (m_wadMaker->load(path.toStdString()))
    {
        m_currentFilePath = path;
        m_modified = false;
        m_saveButton->setEnabled(true);
        setWindowTitle(tr("WAD Editor - %1").arg(QFileInfo(path).fileName()));
        refreshTextureList();
        return true;
    }
    else
    {
        QMessageBox::critical(this, tr("Error"), 
            tr("Failed to load WAD file:\n%1").arg(path));
        return false;
    }
}

void WadMakerDialog::onSaveWad()
{
    if (m_currentFilePath.isEmpty())
    {
        onSaveWadAs();
        return;
    }
    

    if (m_wadMaker->save(m_currentFilePath.toStdString()))
    {
        m_modified = false;
        QMessageBox::information(this, tr("Success"),
            tr("WAD file saved successfully!"));
    }
    else
    {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to save WAD file."));
    }
}

void WadMakerDialog::onSaveWadAs()
{
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save WAD File"), QString(),
        tr("WAD Files (*.wad);;All Files (*.*)")
    );
    
    if (path.isEmpty()) return;
    
    if (!path.toLower().endsWith(".wad"))
        path += ".wad";
    

    if (m_wadMaker->save(path.toStdString()))
    {
        m_currentFilePath = path;
        m_modified = false;
        m_saveButton->setEnabled(true);
        setWindowTitle(tr("WAD Editor - %1").arg(QFileInfo(path).fileName()));
        QMessageBox::information(this, tr("Success"),
            tr("WAD file saved successfully!\n\nTextures: %1").arg(m_wadMaker->textureCount()));
    }
    else
    {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to save WAD file."));
    }
}

void WadMakerDialog::onAddImages()
{
    if (m_wadMaker->textureCount() >= MAX_TEXTURES)
    {
        QMessageBox::warning(this, tr("Limit Reached"),
            tr("Maximum texture count (%1) reached.").arg(MAX_TEXTURES));
        return;
    }
    
    QStringList files = QFileDialog::getOpenFileNames(
        this, tr("Add Images"),
        QString(),
        tr("All Images (*.bmp *.png *.jpg *.jpeg *.tga *.dds);;"
           "BMP (*.bmp);;PNG (*.png);;JPEG (*.jpg *.jpeg);;"
           "TGA (*.tga);;DDS (*.dds);;All Files (*.*)")
    );
    
    if (files.isEmpty()) return;
    

    int added = 0;
    int failed = 0;
    
    for (const QString& file : files)
    {
        if (m_wadMaker->textureCount() >= MAX_TEXTURES)
        {
            QMessageBox::warning(this, tr("Limit Reached"),
                tr("Maximum texture count reached. Remaining files skipped."));
            break;
        }
        
        QFileInfo info(file);
        QString texName = info.baseName().left(MAX_NAME_LENGTH).toUpper();
        
        if (m_wadMaker->addTextureFromImage(texName.toStdString(), file.toStdString()))
        {
            added++;
        }
        else
        {
            failed++;
        }
    }
    
    if (added > 0)
    {
        m_modified = true;
        refreshTextureList();
        QMessageBox::information(this, tr("Images Added"),
            tr("Added: %1\nFailed: %2").arg(added).arg(failed));
    }
    else if (failed > 0)
    {
        QMessageBox::warning(this, tr("Error"),
            tr("No images could be added. Check format compatibility."));
    }
}

void WadMakerDialog::onRemoveSelected()
{
    QList<QListWidgetItem*> selected = m_textureGrid->selectedItems();
    if (selected.isEmpty()) return;
    
    int ret = QMessageBox::question(this, tr("Remove Textures"),
        tr("Remove %1 selected texture(s)?").arg(selected.count()));
    
    if (ret != QMessageBox::Yes) return;
    
    // Get indices in reverse order to remove safely
    std::vector<size_t> indices;
    for (QListWidgetItem* item : selected)
    {
        indices.push_back(item->data(Qt::UserRole).toUInt());
    }
    std::sort(indices.rbegin(), indices.rend());
    
    for (size_t idx : indices)
    {
        m_wadMaker->removeTexture(idx);
    }
    
    m_modified = true;
    refreshTextureList();
}

void WadMakerDialog::onExtractSelected()
{
    QList<QListWidgetItem*> selected = m_textureGrid->selectedItems();
    if (selected.isEmpty()) return;
    
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Directory"));
    if (dir.isEmpty()) return;
    
    int extracted = 0;
    for (QListWidgetItem* item : selected)
    {
        size_t idx = item->data(Qt::UserRole).toUInt();
        const auto& textures = m_wadMaker->textures();
        if (idx < textures.size())
        {
            QString outPath = dir + "/" + QString::fromStdString(textures[idx].name) + ".bmp";
            if (m_wadMaker->extractTextureToBmp(idx, outPath.toStdString()))
            {
                extracted++;
            }
        }
    }
    
    QMessageBox::information(this, tr("Extraction Complete"),
        tr("Extracted %1 texture(s) to:\n%2").arg(extracted).arg(dir));
}

void WadMakerDialog::onRenameSelected()
{
    QList<QListWidgetItem*> selected = m_textureGrid->selectedItems();
    if (selected.count() != 1) return;
    
    size_t idx = selected[0]->data(Qt::UserRole).toUInt();
    const auto& textures = m_wadMaker->textures();
    if (idx >= textures.size()) return;
    
    QString currentName = QString::fromStdString(textures[idx].name);
    
    bool ok;
    QString newName = QInputDialog::getText(this, tr("Rename Texture"),
        tr("New name (max %1 characters):").arg(MAX_NAME_LENGTH),
        QLineEdit::Normal, currentName, &ok);
    
    if (!ok || newName.isEmpty()) return;
    
    if (m_wadMaker->renameTexture(idx, newName.toStdString()))
    {
        m_modified = true;
        refreshTextureList();
    }
    else
    {
        QMessageBox::warning(this, tr("Error"),
            tr("Failed to rename texture. Invalid name."));
    }
}

void WadMakerDialog::onSelectionChanged()
{
    QList<QListWidgetItem*> selected = m_textureGrid->selectedItems();
    bool hasSelection = !selected.isEmpty();
    bool singleSelection = selected.count() == 1;
    
    m_removeButton->setEnabled(hasSelection);
    m_extractButton->setEnabled(hasSelection);
    m_renameButton->setEnabled(singleSelection);
    
    if (singleSelection)
    {
        size_t idx = selected[0]->data(Qt::UserRole).toUInt();
        updatePreview(static_cast<int>(idx));
    }
    else
    {
        m_previewLabel->clear();
        m_previewLabel->setText(selected.isEmpty() ? 
            tr("Select a texture\nto preview") : 
            tr("%1 textures selected").arg(selected.count()));
        m_previewInfo->clear();
    }
}

void WadMakerDialog::onFilterChanged(const QString& text)
{
    QString filter = text.toLower();
    
    for (int i = 0; i < m_textureGrid->count(); ++i)
    {
        QListWidgetItem* item = m_textureGrid->item(i);
        bool match = filter.isEmpty() || 
                     item->text().toLower().contains(filter);
        item->setHidden(!match);
    }
}

void WadMakerDialog::onItemDoubleClicked(QListWidgetItem* item)
{
    // Double-click to rename
    if (item)
    {
        m_textureGrid->setCurrentItem(item);
        onRenameSelected();
    }
}

void WadMakerDialog::refreshTextureList()
{
    m_textureGrid->clear();
    populateThumbnails();
    updateTextureCount();
}

void WadMakerDialog::populateThumbnails()
{
    const auto& textures = m_wadMaker->textures();
    
    for (size_t i = 0; i < textures.size(); ++i)
    {
        QImage thumbnail = m_wadMaker->textureThumbnail(i, THUMBNAIL_SIZE);
        
        QListWidgetItem* item = new QListWidgetItem();
        item->setText(QString::fromStdString(textures[i].name));
        item->setData(Qt::UserRole, static_cast<uint>(i));
        item->setToolTip(QString("%1\n%2×%3")
            .arg(QString::fromStdString(textures[i].name))
            .arg(textures[i].width)
            .arg(textures[i].height));
        
        if (!thumbnail.isNull())
        {
            item->setIcon(QIcon(QPixmap::fromImage(thumbnail)));
        }
        
        m_textureGrid->addItem(item);
    }
}

void WadMakerDialog::updateTextureCount()
{
    size_t count = m_wadMaker->textureCount();
    m_statusLabel->setText(tr("Textures: %1 / %2").arg(count).arg(MAX_TEXTURES));
    
    // Warning color if approaching limit
    if (count >= MAX_TEXTURES - 100)
    {
        m_statusLabel->setStyleSheet("font-weight: bold; color: #ff6b6b; padding: 4px;");
    }
    else if (count >= MAX_TEXTURES - 500)
    {
        m_statusLabel->setStyleSheet("font-weight: bold; color: #ffd93d; padding: 4px;");
    }
    else
    {
        m_statusLabel->setStyleSheet("font-weight: bold; padding: 4px;");
    }
}

void WadMakerDialog::updatePreview(int index)
{
    const auto& textures = m_wadMaker->textures();
    if (index < 0 || static_cast<size_t>(index) >= textures.size())
    {
        m_previewLabel->clear();
        m_previewInfo->clear();
        return;
    }
    
    const WadTexture& tex = textures[index];
    
    // Get full-size image for preview
    QImage fullImage = m_wadMaker->textureFullImage(index);
    if (!fullImage.isNull())
    {
        QPixmap pixmap = QPixmap::fromImage(fullImage);
        // Scale to fit preview label while maintaining aspect ratio
        pixmap = pixmap.scaled(m_previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_previewLabel->setPixmap(pixmap);
    }
    
    // Update info
    QString info = QString(
        "<b>Name:</b> %1<br>"
        "<b>Size:</b> %2 × %3<br>"
        "<b>Disk Size:</b> %4 KB"
    ).arg(QString::fromStdString(tex.name))
     .arg(tex.width)
     .arg(tex.height)
     .arg(tex.diskSize / 1024.0, 0, 'f', 1);
    
    // Show warnings
    if (tex.name.length() > MAX_NAME_LENGTH)
    {
        info += "<br><span style='color: #ff6b6b;'>⚠ Name too long!</span>";
    }
    if (tex.width > 512 || tex.height > 512)
    {
        info += "<br><span style='color: #ff6b6b;'>⚠ Size exceeds 512!</span>";
    }
    
    m_previewInfo->setText(info);
}

bool WadMakerDialog::maybeSave()
{
    if (!m_modified || m_wadMaker->textureCount() == 0)
        return true;
    
    int ret = QMessageBox::question(this, tr("Unsaved Changes"),
        tr("Save changes before proceeding?"),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
    
    if (ret == QMessageBox::Save)
    {
        onSaveWad();
        return !m_modified;
    }
    else if (ret == QMessageBox::Discard)
    {
        return true;
    }
    
    return false;
}
