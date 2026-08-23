#include "PakViewerWindow.h"
#include "core/extractors/pak/PakExtractor.h"
#include "core/extractors/pak/PakFile.h"
#include "SettingsDialog.h"
#include "SpriteViewerWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QApplication>
#include <QScreen>
#include <QDir>
#include <QProcess>
#include <QTemporaryDir>
#include <QShortcut>

#include <fstream>
#include <filesystem>
#include "utils/FileIO.h"

#include "util.hpp"
#include "LanguageManager.h"
#include "ui/UiUtils.h"

PakViewerWindow::PakViewerWindow(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
}

PakViewerWindow::~PakViewerWindow() = default;

void PakViewerWindow::setupUI()
{
    setWindowTitle(tr("PAK Viewer - Counter Strike Online"));
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    setMinimumSize(400, 300);
    
    UiUtils::resizeToScreen(this, 0.7);

    auto* mainLayout = new QVBoxLayout(this);

    // PAK file selection
    auto* fileGroup = new QGroupBox(tr("PAK File"));
    auto* fileLayout = new QHBoxLayout();

    m_pakPathEdit = new QLineEdit();
    m_pakPathEdit->setReadOnly(true);
    m_pakPathEdit->setPlaceholderText(tr("Select a PAK file to view..."));

    m_openButton = new QPushButton(tr("Open PAK..."));

    fileLayout->addWidget(m_pakPathEdit, 1);
    fileLayout->addWidget(m_openButton);
    fileGroup->setLayout(fileLayout);

    // File list table - 3 columns: Path, Size, Type
    auto* listGroup = new QGroupBox(tr("Contents"));
    auto* listLayout = new QVBoxLayout();
    
    // Search box
    auto* searchLayout = new QHBoxLayout();
    searchLayout->addWidget(new QLabel(tr("Search:")));
    m_searchEdit = new QLineEdit();
    m_searchEdit->setPlaceholderText(tr("Search files... (Ctrl+F)"));
    m_searchEdit->setClearButtonEnabled(true);
    m_selectAllMatchesButton = new QPushButton(tr("Select All Matches"));
    m_selectAllMatchesButton->setEnabled(false);
    searchLayout->addWidget(m_searchEdit, 1);
    searchLayout->addWidget(m_selectAllMatchesButton);
    listLayout->addLayout(searchLayout);

    m_fileTable = new QTableWidget();
    m_fileTable->setColumnCount(3);
    m_fileTable->setHorizontalHeaderLabels({tr("Path"), tr("Size"), tr("Type")});
    m_fileTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_fileTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_fileTable->horizontalHeader()->setStretchLastSection(true);
    m_fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_fileTable->verticalHeader()->setVisible(false);
    m_fileTable->setAlternatingRowColors(true);

    listLayout->addWidget(m_fileTable);
    listGroup->setLayout(listLayout);

    // Buttons
    auto* buttonLayout = new QHBoxLayout();

    m_openInViewerButton = new QPushButton(tr("Open in Viewer"));
    m_openInViewerButton->setEnabled(false);
    m_openInViewerButton->setToolTip(tr("Open .mdl or .spr files in viewer"));

    m_extractSelectedButton = new QPushButton(tr("Extract Selected"));
    m_extractSelectedButton->setEnabled(false);

    m_extractAllButton = new QPushButton(tr("Extract All"));
    m_extractAllButton->setEnabled(false);

    m_statusLabel = new QLabel(tr("No PAK file loaded"));

    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);

    buttonLayout->addWidget(m_openInViewerButton);
    buttonLayout->addWidget(m_extractSelectedButton);
    buttonLayout->addWidget(m_extractAllButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_statusLabel);

    // Main layout
    mainLayout->addWidget(fileGroup);
    mainLayout->addWidget(listGroup, 1);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(m_progressBar);

    // Connections
    connect(m_openButton, &QPushButton::clicked, this, &PakViewerWindow::onOpenPak);
    connect(m_extractSelectedButton, &QPushButton::clicked, this, &PakViewerWindow::onExtractSelected);
    connect(m_extractAllButton, &QPushButton::clicked, this, &PakViewerWindow::onExtractAll);
    connect(m_openInViewerButton, &QPushButton::clicked, this, &PakViewerWindow::onOpenInViewer);
    connect(m_fileTable, &QTableWidget::itemSelectionChanged, this, &PakViewerWindow::onSelectionChanged);
    connect(m_fileTable, &QTableWidget::cellDoubleClicked, this, &PakViewerWindow::onDoubleClick);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &PakViewerWindow::onSearchTextChanged);
    connect(m_selectAllMatchesButton, &QPushButton::clicked, this, &PakViewerWindow::onSelectAllMatches);
    
    // Keyboard shortcut for search (Ctrl+F)
    auto* searchShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(searchShortcut, &QShortcut::activated, this, [this]() {
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    });
}

void PakViewerWindow::onOpenPak()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open PAK File"),
        QString(),
        tr("PAK files (*.pak);;All files (*.*)")
    );

    if (filePath.isEmpty())
        return;

    loadPak(filePath);
}

bool PakViewerWindow::loadPak(const QString& filePath)
{
    // Read file
    std::filesystem::path fsPath = filePath.toStdWString();
    std::ifstream is(fsPath, std::ios::binary | std::ios::ate);
    if (!is)
    {
        QMessageBox::critical(this, tr("Error"), tr("Failed to open PAK file."));
        return false;
    }

    std::streamsize size = is.tellg();
    if (size <= 0)
    {
        QMessageBox::critical(this, tr("Error"), tr("Invalid PAK file size."));
        return false;
    }

    is.seekg(0, std::ios::beg);
    m_pakBuffer.resize(static_cast<size_t>(size));

    if (!is.read(reinterpret_cast<char*>(m_pakBuffer.data()), size))
    {
        QMessageBox::critical(this, tr("Error"), tr("Failed to read PAK file data."));
        return false;
    }

    // Parse PAK
    std::u16string u16Name = fsPath.filename().generic_u16string();
    
    // Make a copy for PakFile since it takes ownership
    std::vector<uint8_t> bufferCopy = m_pakBuffer;
    m_pakFile = std::make_unique<PakFile>(std::move(bufferCopy), std::move(u16Name));

    if (!m_pakFile->ParseHeader())
    {
        QMessageBox::warning(this, tr("Error"), tr("Invalid PAK header. File may be isnt Correct."));
        m_pakFile.reset();
        return false;
    }

    if (!m_pakFile->ParseEntries())
    {
        QMessageBox::warning(this, tr("Error"), tr("Failed to parse PAK entries."));
        m_pakFile.reset();
        return false;
    }

    m_currentPakPath = filePath;
    m_pakPathEdit->setText(filePath);

    populateFileList();
    return true;
}

void PakViewerWindow::populateFileList()
{
    m_fileTable->setRowCount(0);

    if (!m_pakFile)
        return;

    const auto& entries = m_pakFile->GetEntries();

    m_fileTable->setRowCount(static_cast<int>(entries.size()));

    for (size_t i = 0; i < entries.size(); ++i)
    {
        const auto& entry = entries[i];

        std::string utf8Path = String_UTF16toUTF8(entry.FilePath);
        QString qPath = QString::fromStdString(utf8Path);

        auto* pathItem = new QTableWidgetItem(qPath);
        auto* sizeItem = new QTableWidgetItem(UiUtils::formatSize(entry.RealSize));
        auto* typeItem = new QTableWidgetItem(getFileTypeInfo(qPath));

        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        typeItem->setTextAlignment(Qt::AlignCenter);

        m_fileTable->setItem(static_cast<int>(i), 0, pathItem);
        m_fileTable->setItem(static_cast<int>(i), 1, sizeItem);
        m_fileTable->setItem(static_cast<int>(i), 2, typeItem);
    }

    m_fileTable->resizeColumnsToContents();
    m_fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    m_extractAllButton->setEnabled(!entries.empty());
    m_statusLabel->setText(tr("%1 files").arg(entries.size()));
}

void PakViewerWindow::onSelectionChanged()
{
    QList<QTableWidgetItem*> selectedItems = m_fileTable->selectedItems();
    bool hasSelection = !selectedItems.isEmpty();
    m_extractSelectedButton->setEnabled(hasSelection);

    // Check if any selected file can be opened in viewer
    bool canOpen = false;
    if (hasSelection)
    {
        QSet<int> rows;
        for (auto* item : selectedItems)
            rows.insert(item->row());

        for (int row : rows)
        {
            auto* pathItem = m_fileTable->item(row, 0);
            if (pathItem && canOpenInViewer(pathItem->text()))
            {
                canOpen = true;
                break;
            }
        }
    }

    m_openInViewerButton->setEnabled(canOpen);
}

void PakViewerWindow::onDoubleClick(int row, int column)
{
    Q_UNUSED(column);
    
    auto* pathItem = m_fileTable->item(row, 0);
    if (pathItem && canOpenInViewer(pathItem->text()))
    {
        openEntryInViewer(static_cast<size_t>(row));
    }
}

bool PakViewerWindow::canOpenInViewer(const QString& path) const
{
    QString ext = QFileInfo(path).suffix().toLower();
    
    if (ext == "mdl")
        return !SettingsDialog::getModelViewerPath().isEmpty();
    if (ext == "spr")
        return true;  // Built-in viewer always available
    
    return false;
}

void PakViewerWindow::onOpenInViewer()
{
    QList<QTableWidgetItem*> selectedItems = m_fileTable->selectedItems();
    if (selectedItems.isEmpty())
        return;

    // Get first selected row that can be opened
    QSet<int> rows;
    for (auto* item : selectedItems)
        rows.insert(item->row());

    for (int row : rows)
    {
        auto* pathItem = m_fileTable->item(row, 0);
        if (pathItem && canOpenInViewer(pathItem->text()))
        {
            openEntryInViewer(static_cast<size_t>(row));
            return;
        }
    }
}

void PakViewerWindow::openEntryInViewer(size_t index)
{
    if (!m_pakFile)
        return;

    const auto& entries = m_pakFile->GetEntries();
    if (index >= entries.size())
        return;

    const auto& entry = entries[index];
    std::string utf8Path = String_UTF16toUTF8(entry.FilePath);
    QString qPath = QString::fromStdString(utf8Path);
    QString ext = QFileInfo(qPath).suffix().toLower();

    // Extract to temp directory
    QApplication::setOverrideCursor(Qt::WaitCursor);

    try
    {
        auto [unpacked, data] = m_pakFile->UnpackEntry(entry);

        if (!unpacked)
        {
            QApplication::restoreOverrideCursor();
            QMessageBox::warning(this, tr("Error"), tr("Failed to extract file."));
            return;
        }

        // Create temp file
        QString tempDir = QDir::tempPath() + "/VortigauntPakViewer";
        QDir().mkpath(tempDir);

        QString fileName = QFileInfo(qPath).fileName();
        QString tempFilePath = tempDir + "/" + fileName;
        std::filesystem::path tempPath = tempFilePath.toStdWString();
        std::ofstream os(tempPath, std::ios::binary);
        if (!os)
        {
            QApplication::restoreOverrideCursor();
            QMessageBox::warning(this, tr("Error"), tr("Failed to create temp file."));
            return;
        }

        os.write(reinterpret_cast<const char*>(data.data()),
                 static_cast<std::streamsize>(data.size()));
        os.close();

        QApplication::restoreOverrideCursor();

        if (ext == "spr")
        {
            // Use built-in Sprite Viewer
            auto* spriteViewer = new SpriteViewerWindow(this);
            spriteViewer->setAttribute(Qt::WA_DeleteOnClose);
            spriteViewer->openSprite(tempFilePath);
            spriteViewer->show();
        }
        else if (ext == "mdl")
        {
            // Use external Model Viewer
            QString viewerPath = SettingsDialog::getModelViewerPath();
            if (viewerPath.isEmpty())
            {
                QMessageBox::warning(this, tr("Viewer Not Configured"),
                    tr("Please configure the Model Viewer path in Settings -> Configure..."));
                return;
            }
            if (!QFileInfo::exists(viewerPath))
            {
                QMessageBox::warning(this, tr("Viewer Not Found"),
                    tr("The configured viewer executable was not found:\n%1").arg(viewerPath));
                return;
            }
            QProcess::startDetached(viewerPath, {tempFilePath});
        }
    }
    catch (const std::exception& ex)
    {
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, tr("Error"), 
            tr("Failed to open file: %1").arg(ex.what()));
    }
}

void PakViewerWindow::onExtractSelected()
{
    if (!m_pakFile)
        return;

    QList<QTableWidgetItem*> selectedItems = m_fileTable->selectedItems();
    if (selectedItems.isEmpty())
        return;

    // Get unique row indices
    QSet<int> rows;
    for (auto* item : selectedItems)
    {
        rows.insert(item->row());
    }

    std::vector<size_t> indices;
    for (int row : rows)
    {
        indices.push_back(static_cast<size_t>(row));
    }

    QString outputDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Output Directory"),
        SettingsDialog::getExtractStartDir()
    );

    if (outputDir.isEmpty())
        return;

    extractEntries(indices, outputDir);
}

void PakViewerWindow::onExtractAll()
{
    if (!m_pakFile)
        return;

    const auto& entries = m_pakFile->GetEntries();
    if (entries.empty())
        return;

    QString outputDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Output Directory"),
        SettingsDialog::getExtractStartDir()
    );

    if (outputDir.isEmpty())
        return;

    std::vector<size_t> indices;
    for (size_t i = 0; i < entries.size(); ++i)
    {
        indices.push_back(i);
    }

    extractEntries(indices, outputDir);
}

void PakViewerWindow::extractEntries(const std::vector<size_t>& indices, const QString& outputDir)
{
    if (!m_pakFile || indices.empty())
        return;

    const auto& entries = m_pakFile->GetEntries();

    m_progressBar->setVisible(true);
    m_progressBar->setMaximum(static_cast<int>(indices.size()));
    m_progressBar->setValue(0);

    QApplication::setOverrideCursor(Qt::WaitCursor);

    std::filesystem::path outPath = outputDir.toStdString();
    size_t okCount = 0;
    size_t failCount = 0;

    for (size_t i = 0; i < indices.size(); ++i)
    {
        size_t idx = indices[i];
        if (idx >= entries.size())
            continue;

        const auto& entry = entries[idx];

        try
        {
            auto [unpacked, data] = m_pakFile->UnpackEntry(entry);

            if (!unpacked)
            {
                ++failCount;
                continue;
            }

            std::filesystem::path relPath(entry.FilePath);
            std::filesystem::path fullPath = outPath / relPath;

            std::filesystem::create_directories(fullPath.parent_path());

            std::ofstream os(fullPath, std::ios::binary);
            if (!os)
            {
                ++failCount;
                continue;
            }

            os.write(reinterpret_cast<const char*>(data.data()),
                     static_cast<std::streamsize>(data.size()));

            ++okCount;
        }
        catch (...)
        {
            ++failCount;
        }

        m_progressBar->setValue(static_cast<int>(i + 1));
        QApplication::processEvents();
    }

    QApplication::restoreOverrideCursor();
    m_progressBar->setVisible(false);

    QString msg = tr("Extraction complete.\n\nSuccessful: %1\nFailed: %2")
                      .arg(okCount)
                      .arg(failCount);

    if (failCount > 0)
    {
        QMessageBox::warning(this, tr("Extraction Complete"), msg);
    }
    else
    {
        QMessageBox::information(this, tr("Extraction Complete"), msg);
    }

    m_statusLabel->setText(tr("Extracted %1/%2 files").arg(okCount).arg(indices.size()));
}

QString PakViewerWindow::getFileTypeInfo(const QString& path) const
{
    QString ext = QFileInfo(path).suffix().toLower();

    if (ext == "mdl")
        return tr("Half-Life Model");
    if (ext == "spr")
        return tr("Half-Life Sprite");
    if (ext == "wav")
        return tr("Wave Audio");
    if (ext == "bmp" || ext == "tga" || ext == "png" || ext == "jpg")
        return tr("Image");
    if (ext == "txt" || ext == "cfg")
        return tr("Text File");
	if (ext == "mp3")
        return tr("MP3 Audio");
	if (ext == "res")
        return tr("Resource list file");
    if (ext == "wad")
        return tr("WAD Texture Pack");
    if (ext == "bsp")
        return tr("BSP Map");
    if (ext == "nav")
		return tr("Navigation Mesh");
    if (ext == "vxl")
		return tr("Voxel file");
    
    return ext.toUpper();
}


void PakViewerWindow::onSelectAllMatches()
{
    if (!m_pakFile)
        return;

    UiUtils::selectVisibleRows(m_fileTable);
    onSelectionChanged(); // Update button states
}

void PakViewerWindow::filterTable(const QString& searchText)
{
    if (!m_pakFile)
        return;

    const int totalRows = m_fileTable->rowCount();
    const int visibleCount = UiUtils::filterTableRows(m_fileTable, searchText);

    m_statusLabel->setText(tr("%1 files (%2 visible)").arg(totalRows).arg(visibleCount));
    m_selectAllMatchesButton->setEnabled(!searchText.isEmpty() && visibleCount > 0);
}

void PakViewerWindow::onSearchTextChanged(const QString& text)
{
    filterTable(text);
}


