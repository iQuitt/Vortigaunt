#include "VpkViewerWindow.h"
#include "core/extractors/vpk/VpkFile.h"
#include "core/extractors/vpk/VpkExtractor.h"
#include "core/extractors/gma/GmaFile.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QApplication>
#include <QScreen>
#include <QDir>
#include <QShortcut>

#include <filesystem>
#include <fstream>

#include "LanguageManager.h"
#include "ui/dialogs/SettingsDialog.h"
#include "ui/UiUtils.h"

VpkViewerWindow::VpkViewerWindow(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
}

VpkViewerWindow::~VpkViewerWindow() = default;

void VpkViewerWindow::setupUI()
{
    setWindowTitle(tr("VPK / GMA Viewer - Source Engine"));
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    setMinimumSize(400, 300);

    UiUtils::resizeToScreen(this, 0.7);

    auto* mainLayout = new QVBoxLayout(this);

    // VPK / GMA file selection
    auto* fileGroup = new QGroupBox(tr("Archive File (VPK / GMA)"));
    auto* fileLayout = new QHBoxLayout();

    m_vpkPathEdit = new QLineEdit();
    m_vpkPathEdit->setReadOnly(true);
    m_vpkPathEdit->setPlaceholderText(tr("Select a VPK or GMA file to view..."));

    m_openButton = new QPushButton(tr("Open VPK / GMA..."));

    fileLayout->addWidget(m_vpkPathEdit, 1);
    fileLayout->addWidget(m_openButton);
    fileGroup->setLayout(fileLayout);

    // File list table
    auto* listGroup = new QGroupBox(tr("Contents"));
    auto* listLayout = new QVBoxLayout();

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

    m_extractSelectedButton = new QPushButton(tr("Extract Selected"));
    m_extractSelectedButton->setEnabled(false);

    m_extractAllButton = new QPushButton(tr("Extract All"));
    m_extractAllButton->setEnabled(false);

    m_statusLabel = new QLabel(tr("No archive loaded"));

    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);

    buttonLayout->addWidget(m_extractSelectedButton);
    buttonLayout->addWidget(m_extractAllButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(fileGroup);
    mainLayout->addWidget(listGroup, 1);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(m_progressBar);

    // Connections
    connect(m_openButton, &QPushButton::clicked, this, &VpkViewerWindow::onOpenVpk);
    connect(m_extractSelectedButton, &QPushButton::clicked, this, &VpkViewerWindow::onExtractSelected);
    connect(m_extractAllButton, &QPushButton::clicked, this, &VpkViewerWindow::onExtractAll);
    connect(m_fileTable, &QTableWidget::itemSelectionChanged, this, &VpkViewerWindow::onSelectionChanged);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &VpkViewerWindow::onSearchTextChanged);
    connect(m_selectAllMatchesButton, &QPushButton::clicked, this, &VpkViewerWindow::onSelectAllMatches);

    auto* searchShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(searchShortcut, &QShortcut::activated, this, [this]() {
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    });
}

void VpkViewerWindow::onOpenVpk()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open VPK / GMA File"),
        QString(),
        tr("Source archives (*.vpk *.gma);;VPK files (*.vpk);;GMA files (*.gma);;All files (*.*)")
    );

    if (filePath.isEmpty())
        return;

    loadVpk(filePath);
}

bool VpkViewerWindow::loadVpk(const QString& filePath)
{
    QString ext = QFileInfo(filePath).suffix().toLower();

    bool ok = (ext == "gma") ? loadGmaFile(filePath)
                             : loadVpkFile(filePath);
    if (!ok)
        return false;

    m_currentVpkPath = filePath;
    m_vpkPathEdit->setText(filePath);

    populateFileList();
    return true;
}

bool VpkViewerWindow::loadVpkFile(const QString& filePath)
{
    std::filesystem::path fsPath = filePath.toStdWString();

    // Check if it's a _NNN.vpk (data chunk) - those can't be opened directly
    std::string filename = fsPath.filename().string();
    auto nnnPos = filename.rfind('_');
    if (nnnPos != std::string::npos)
    {
        std::string after = filename.substr(nnnPos + 1);
        if (after.size() >= 3 && after.find('.') != std::string::npos)
        {
            std::string numPart = after.substr(0, after.find('.'));
            if (numPart.find_first_not_of("0123456789") == std::string::npos)
            {
                QMessageBox::warning(this, tr("Cannot Open"),
                    tr("This appears to be a data chunk file (*_NNN.vpk).\n"
                       "Please open the corresponding _dir.vpk file instead."));
                return false;
            }
        }
    }

    m_vpkFile = std::make_unique<VpkFile>();

    if (!m_vpkFile->Load(filePath.toStdString()))
    {
        QMessageBox::warning(this, tr("Error"), tr("Failed to load VPK file.\n\nThe file may be corrupted or not a valid VPK archive."));
        m_vpkFile.reset();
        return false;
    }

    m_gmaFile.reset();
    return true;
}

bool VpkViewerWindow::loadGmaFile(const QString& filePath)
{
    m_gmaFile = std::make_unique<GmaFile>();

    if (!m_gmaFile->Load(filePath.toStdString()))
    {
        QMessageBox::warning(this, tr("Error"), tr("Failed to load GMA file.\n\nThe file may be corrupted or not a valid Garry's Mod addon."));
        m_gmaFile.reset();
        return false;
    }

    m_vpkFile.reset();
    return true;
}

bool VpkViewerWindow::hasArchive() const
{
    return m_vpkFile != nullptr || m_gmaFile != nullptr;
}

void VpkViewerWindow::populateFileList()
{
    m_fileTable->setRowCount(0);

    if (!hasArchive())
        return;

    // Collect (path, size) rows from whichever archive type is loaded
    struct Row { QString path; quint64 size; };
    std::vector<Row> rows;

    if (m_vpkFile)
    {
        const auto& entries = m_vpkFile->GetEntries();
        rows.reserve(entries.size());
        for (const auto& entry : entries)
            rows.push_back({ QString::fromStdString(entry.fullPath),
                             entry.entryLength + static_cast<quint64>(entry.preloadData.size()) });
    }
    else if (m_gmaFile)
    {
        const auto& entries = m_gmaFile->GetEntries();
        rows.reserve(entries.size());
        for (const auto& entry : entries)
            rows.push_back({ QString::fromStdString(entry.fullPath), entry.size });
    }

    m_fileTable->setRowCount(static_cast<int>(rows.size()));

    for (size_t i = 0; i < rows.size(); ++i)
    {
        auto* pathItem = new QTableWidgetItem(rows[i].path);
        auto* sizeItem = new QTableWidgetItem(UiUtils::formatSize(rows[i].size));
        auto* typeItem = new QTableWidgetItem(getFileTypeInfo(rows[i].path));

        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        typeItem->setTextAlignment(Qt::AlignCenter);

        m_fileTable->setItem(static_cast<int>(i), 0, pathItem);
        m_fileTable->setItem(static_cast<int>(i), 1, sizeItem);
        m_fileTable->setItem(static_cast<int>(i), 2, typeItem);
    }

    m_fileTable->resizeColumnsToContents();
    m_fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    m_extractAllButton->setEnabled(!rows.empty());

    if (m_gmaFile && !m_gmaFile->GetName().empty())
    {
        m_statusLabel->setText(tr("%1 files — %2 (by %3)")
                                   .arg(rows.size())
                                   .arg(QString::fromStdString(m_gmaFile->GetName()),
                                        QString::fromStdString(m_gmaFile->GetAuthor())));
    }
    else
    {
        m_statusLabel->setText(tr("%1 files").arg(rows.size()));
    }
}

void VpkViewerWindow::onSelectionChanged()
{
    QList<QTableWidgetItem*> selectedItems = m_fileTable->selectedItems();
    bool hasSelection = !selectedItems.isEmpty();
    m_extractSelectedButton->setEnabled(hasSelection);
}

void VpkViewerWindow::onExtractSelected()
{
    if (!hasArchive())
        return;

    QList<QTableWidgetItem*> selectedItems = m_fileTable->selectedItems();
    if (selectedItems.isEmpty())
        return;

    QSet<int> rows;
    for (auto* item : selectedItems)
        rows.insert(item->row());

    std::vector<size_t> indices;
    for (int row : rows)
        indices.push_back(static_cast<size_t>(row));

    QString outputDir = QFileDialog::getExistingDirectory(
        this, tr("Select Output Directory"), SettingsDialog::getExtractStartDir());

    if (outputDir.isEmpty())
        return;

    extractEntries(indices, outputDir);
}

void VpkViewerWindow::onExtractAll()
{
    if (!hasArchive())
        return;

    size_t entryCount = m_vpkFile ? m_vpkFile->GetEntryCount() : m_gmaFile->GetEntryCount();
    if (entryCount == 0)
        return;

    QString outputDir = QFileDialog::getExistingDirectory(
        this, tr("Select Output Directory"), SettingsDialog::getExtractStartDir());

    if (outputDir.isEmpty())
        return;

    std::vector<size_t> indices;
    for (size_t i = 0; i < entryCount; ++i)
        indices.push_back(i);

    extractEntries(indices, outputDir);
}

void VpkViewerWindow::extractEntries(const std::vector<size_t>& indices, const QString& outputDir)
{
    if (!hasArchive() || indices.empty())
        return;

    const size_t entryCount = m_vpkFile ? m_vpkFile->GetEntryCount() : m_gmaFile->GetEntryCount();

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
        if (idx >= entryCount)
            continue;

        try
        {
            std::string entryPath;
            std::vector<uint8_t> data;
            bool allowEmpty = false;

            if (m_vpkFile)
            {
                const auto& entry = m_vpkFile->GetEntries()[idx];
                entryPath = entry.fullPath;
                data = m_vpkFile->ExtractEntry(entry);
            }
            else
            {
                const auto& entry = m_gmaFile->GetEntries()[idx];
                if (!GmaFile::IsSafeEntryPath(entry.fullPath))
                {
                    ++failCount;
                    continue;
                }
                entryPath = entry.fullPath;
                data = m_gmaFile->ExtractEntry(entry);
                allowEmpty = (entry.size == 0); // GMA may contain empty files
            }

            if (data.empty() && !allowEmpty)
            {
                ++failCount;
                continue;
            }

            std::filesystem::path relPath(entryPath);
            std::filesystem::path fullPath = outPath / relPath;

            std::filesystem::create_directories(fullPath.parent_path());

            std::ofstream os(fullPath, std::ios::binary);
            if (!os)
            {
                ++failCount;
                continue;
            }

            if (!data.empty())
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
        QMessageBox::warning(this, tr("Extraction Complete"), msg);
    else
        QMessageBox::information(this, tr("Extraction Complete"), msg);

    m_statusLabel->setText(tr("Extracted %1/%2 files").arg(okCount).arg(indices.size()));
}

QString VpkViewerWindow::getFileTypeInfo(const QString& path) const
{
    QString ext = QFileInfo(path).suffix().toLower();

    if (ext == "mdl")
        return tr("Source Model");
    if (ext == "vtx" || ext == "vvd")
        return tr("Source Model Data");
    if (ext == "vtf")
        return tr("Source Texture");
    if (ext == "vmt")
        return tr("Source Material");
    if (ext == "wav" || ext == "mp3")
        return tr("Audio");
    if (ext == "bmp" || ext == "tga" || ext == "png" || ext == "jpg" || ext == "dds")
        return tr("Image");
    if (ext == "txt" || ext == "cfg" || ext == "res" || ext == "kv")
        return tr("Text File");
    if (ext == "bsp")
        return tr("Source Map");
    if (ext == "spr")
        return tr("Sprite");
    if (ext == "pcf")
        return tr("Particle System");
    if (ext == "phy")
        return tr("Physics Mesh");
    if (ext == "lua")
        return tr("Lua Script");
    if (ext == "vcd")
        return tr("Choreography Scene");
    if (ext == "json")
        return tr("JSON Data");

    return ext.toUpper();
}


void VpkViewerWindow::onSelectAllMatches()
{
    if (!hasArchive())
        return;

    UiUtils::selectVisibleRows(m_fileTable);
    onSelectionChanged(); // Update button states
}

void VpkViewerWindow::filterTable(const QString& searchText)
{
    if (!hasArchive())
        return;

    const int totalRows = m_fileTable->rowCount();
    const int visibleCount = UiUtils::filterTableRows(m_fileTable, searchText);

    m_statusLabel->setText(tr("%1 files (%2 visible)").arg(totalRows).arg(visibleCount));
    m_selectAllMatchesButton->setEnabled(!searchText.isEmpty() && visibleCount > 0);
}

void VpkViewerWindow::onSearchTextChanged(const QString& text)
{
    filterTable(text);
}


