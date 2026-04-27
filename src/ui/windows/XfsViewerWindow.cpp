#include "XfsViewerWindow.h"
#include "core/extractors/xfs/XfsExtractor.h"
#include "SpriteViewerWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>
#include <QApplication>
#include <QScreen>
#include <QEventLoop>
#include <QDir>
#include <QShortcut>

#include <filesystem>

#include "LanguageManager.h"

XfsViewerWindow::XfsViewerWindow(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
}

XfsViewerWindow::~XfsViewerWindow() = default;

void XfsViewerWindow::setupUI()
{
    setWindowTitle(tr("XFS Viewer"));
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    setMinimumSize(400, 300);
    
    QScreen* screen = QGuiApplication::primaryScreen();
    QSize screenSize = screen ? screen->availableGeometry().size() : QSize(1920, 1080);
    resize(screenSize.width() * 0.7, screenSize.height() * 0.7);

    auto* mainLayout = new QVBoxLayout(this);

    // File group
    auto* fileGroup = new QGroupBox(tr("XFS Archive"));
    auto* fileLayout = new QHBoxLayout();

    m_xfsPathEdit = new QLineEdit();
    m_xfsPathEdit->setReadOnly(true);
    m_xfsPathEdit->setPlaceholderText(tr("Select an XFS file to view..."));

    m_openButton = new QPushButton(tr("Open XFS..."));

    fileLayout->addWidget(m_xfsPathEdit, 1);
    fileLayout->addWidget(m_openButton);
    fileGroup->setLayout(fileLayout);

    // Archive info label
    m_archiveInfoLabel = new QLabel();
    m_archiveInfoLabel->setStyleSheet("QLabel { color: #666; font-size: 11px; }");

    // File list table
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

    // Table with 4 columns: Path, Packed Size, Unpacked Size, Type
    m_fileTable = new QTableWidget();
    m_fileTable->setColumnCount(4);
    m_fileTable->setHorizontalHeaderLabels({tr("Path"), tr("Packed"), tr("Unpacked"), tr("Type")});
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

    m_viewSprButton = new QPushButton(tr("View SPR"));
    m_viewSprButton->setEnabled(false);
    m_viewSprButton->setToolTip(tr("View selected SPR file in Lithtech Sprite Viewer"));

    m_extractSelectedButton = new QPushButton(tr("Extract Selected"));
    m_extractSelectedButton->setEnabled(false);

    m_extractAllButton = new QPushButton(tr("Extract All"));
    m_extractAllButton->setEnabled(false);

    m_statusLabel = new QLabel(tr("No XFS file loaded"));

    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);

    buttonLayout->addWidget(m_viewSprButton);
    buttonLayout->addWidget(m_extractSelectedButton);
    buttonLayout->addWidget(m_extractAllButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_statusLabel);

    // Main layout
    mainLayout->addWidget(fileGroup);
    mainLayout->addWidget(m_archiveInfoLabel);
    mainLayout->addWidget(listGroup, 1);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(m_progressBar);

    // Connections
    connect(m_openButton, &QPushButton::clicked, this, &XfsViewerWindow::onOpenXfs);
    connect(m_extractSelectedButton, &QPushButton::clicked, this, &XfsViewerWindow::onExtractSelected);
    connect(m_extractAllButton, &QPushButton::clicked, this, &XfsViewerWindow::onExtractAll);
    connect(m_fileTable, &QTableWidget::itemSelectionChanged, this, &XfsViewerWindow::onSelectionChanged);
    connect(m_fileTable, &QTableWidget::cellDoubleClicked, this, &XfsViewerWindow::onDoubleClick);
    connect(m_viewSprButton, &QPushButton::clicked, this, &XfsViewerWindow::onViewSpr);
    connect(m_searchEdit, &QLineEdit::textChanged, this, &XfsViewerWindow::onSearchTextChanged);
    connect(m_selectAllMatchesButton, &QPushButton::clicked, this, &XfsViewerWindow::onSelectAllMatches);

    // Keyboard shortcut for search (Ctrl+F)
    auto* searchShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(searchShortcut, &QShortcut::activated, this, [this]() {
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    });
}

void XfsViewerWindow::onOpenXfs()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open XFS Archive"),
        QString(),
        tr("XFS files (*.xfs);;All files (*.*)")
    );

    if (filePath.isEmpty())
        return;

    loadXfs(filePath);
}

bool XfsViewerWindow::loadXfs(const QString& filePath)
{
    // Show progress bar
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 0); // Indeterminate mode for loading
    m_progressBar->setFormat(tr("Loading XFS..."));

    // Set green color for progress bar
    QString styleSheet = QString(
        "QProgressBar {"
        "    border: 1px solid #2E7D32;"
        "    border-radius: 3px;"
        "    text-align: center;"
        "    background-color: #E0E0E0;"
        "}"
        "QProgressBar::chunk {"
        "    background-color: #4CAF50;"
        "    border-radius: 2px;"
        "}"
    );
    m_progressBar->setStyleSheet(styleSheet);

    m_statusLabel->setText(tr("Loading XFS archive..."));
    QApplication::processEvents();

    m_xfsExtractor = std::make_unique<XfsExtractor>();

    if (!m_xfsExtractor->Load(filePath.toStdString()))
    {
        m_progressBar->setVisible(false);
        QMessageBox::critical(this, tr("Error"), tr("Failed to load XFS file. The file may be corrupted or not a valid XFS archive."));
        m_xfsExtractor.reset();
        return false;
    }

    m_progressBar->setVisible(false);
    m_currentXfsPath = filePath;
    m_xfsPathEdit->setText(filePath);

    // Update archive info
    uint64_t totalPacked = m_xfsExtractor->GetTotalPackedSize();
    uint64_t totalUnpacked = m_xfsExtractor->GetTotalUnpackedSize();
    m_archiveInfoLabel->setText(tr("Archive: %1 | Total packed: %2 | Total unpacked: %3")
        .arg(QFileInfo(filePath).fileName())
        .arg(formatSize(totalPacked))
        .arg(formatSize(totalUnpacked)));

    populateFileList();
    return true;
}

void XfsViewerWindow::populateFileList()
{
    m_fileTable->setRowCount(0);

    if (!m_xfsExtractor)
        return;

    const auto& entries = m_xfsExtractor->GetEntries();
    const size_t entryCount = entries.size();

    if (entryCount == 0)
    {
        m_extractAllButton->setEnabled(false);
        m_statusLabel->setText(tr("No files found"));
        return;
    }

    m_fileTable->setSortingEnabled(false);
    m_fileTable->setUpdatesEnabled(false);

    // Pre-allocate rows
    m_fileTable->setRowCount(static_cast<int>(entryCount));

    constexpr size_t BATCH_SIZE = 5000;

    for (size_t i = 0; i < entryCount; ++i)
    {
        const auto& entry = entries[i];

        QString qPath = QString::fromStdString(entry.filename);

        auto* pathItem = new QTableWidgetItem(qPath);
        auto* packedSizeItem = new QTableWidgetItem(formatSize(entry.packedSize));
        auto* unpackedSizeItem = new QTableWidgetItem(formatSize(entry.unpackedSize));
        auto* typeItem = new QTableWidgetItem(getFileTypeInfo(qPath));

        packedSizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        unpackedSizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        typeItem->setTextAlignment(Qt::AlignCenter);

        m_fileTable->setItem(static_cast<int>(i), 0, pathItem);
        m_fileTable->setItem(static_cast<int>(i), 1, packedSizeItem);
        m_fileTable->setItem(static_cast<int>(i), 2, unpackedSizeItem);
        m_fileTable->setItem(static_cast<int>(i), 3, typeItem);

        // Update UI periodically
        if ((i + 1) % BATCH_SIZE == 0 || i == entryCount - 1)
        {
            m_fileTable->setUpdatesEnabled(true);
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 0);
            m_fileTable->setUpdatesEnabled(false);
        }
    }

    // Re-enable updates and sorting
    m_fileTable->setUpdatesEnabled(true);
    m_fileTable->setSortingEnabled(true);

    // Resize columns
    m_fileTable->resizeColumnsToContents();
    m_fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    m_extractAllButton->setEnabled(true);
    m_statusLabel->setText(tr("%1 files").arg(entryCount));
}

void XfsViewerWindow::onSelectionChanged()
{
    QList<QTableWidgetItem*> selectedItems = m_fileTable->selectedItems();
    bool hasSelection = !selectedItems.isEmpty();
    m_extractSelectedButton->setEnabled(hasSelection);

    // Check if any selected file is a SPR
    bool hasSpr = false;
    if (hasSelection)
    {
        QSet<int> rows;
        for (auto* item : selectedItems)
            rows.insert(item->row());

        for (int row : rows)
        {
            auto* pathItem = m_fileTable->item(row, 0);
            if (pathItem && canViewAsSpr(pathItem->text()))
            {
                hasSpr = true;
                break;
            }
        }
    }

    m_viewSprButton->setEnabled(hasSpr);
}

void XfsViewerWindow::onDoubleClick(int row, int column)
{
    Q_UNUSED(column);
    
    auto* pathItem = m_fileTable->item(row, 0);
    if (pathItem && canViewAsSpr(pathItem->text()))
    {
        viewSprEntry(static_cast<size_t>(row));
    }
}

bool XfsViewerWindow::canViewAsSpr(const QString& path) const
{
    QString ext = QFileInfo(path).suffix().toLower();
    return (ext == "spr");
}

void XfsViewerWindow::onViewSpr()
{
    QList<QTableWidgetItem*> selectedItems = m_fileTable->selectedItems();
    if (selectedItems.isEmpty())
        return;

    // Get first selected SPR row
    QSet<int> rows;
    for (auto* item : selectedItems)
        rows.insert(item->row());

    for (int row : rows)
    {
        auto* pathItem = m_fileTable->item(row, 0);
        if (pathItem && canViewAsSpr(pathItem->text()))
        {
            viewSprEntry(static_cast<size_t>(row));
            return;
        }
    }
}

void XfsViewerWindow::viewSprEntry(size_t index)
{
    if (!m_xfsExtractor)
        return;

    const auto& entries = m_xfsExtractor->GetEntries();
    if (index >= entries.size())
        return;

    const auto& entry = entries[index];
    QString qPath = QString::fromStdString(entry.filename);

    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Extract SPR to temp directory
    QString tempDir = QDir::tempPath() + "/VortigauntXfsViewer";
    QDir().mkpath(tempDir);

    QString fileName = QFileInfo(qPath).fileName();
    QString tempFilePath = tempDir + "/" + fileName;

    bool extracted = m_xfsExtractor->ExtractEntry(entry, tempFilePath.toStdString());

    QApplication::restoreOverrideCursor();

    if (!extracted)
    {
        QMessageBox::warning(this, tr("Error"), tr("Failed to extract SPR file."));
        return;
    }

    // Open in unified Sprite Viewer
    auto* sprViewer = new SpriteViewerWindow(this);
    sprViewer->setAttribute(Qt::WA_DeleteOnClose);
    sprViewer->openSprite(tempFilePath);
    sprViewer->show();
}

void XfsViewerWindow::onExtractSelected()
{
    if (!m_xfsExtractor)
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
        QDir::homePath()
    );

    if (outputDir.isEmpty())
        return;

    extractEntries(indices, outputDir);
}

void XfsViewerWindow::onExtractAll()
{
    if (!m_xfsExtractor)
        return;

    const auto& entries = m_xfsExtractor->GetEntries();
    if (entries.empty())
        return;

    QString outputDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Output Directory"),
        QDir::homePath()
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

void XfsViewerWindow::extractEntries(const std::vector<size_t>& indices, const QString& outputDir)
{
    if (!m_xfsExtractor || indices.empty())
        return;

    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFormat(tr("Extracting: %p%"));

    // Set green color
    QString styleSheet = QString(
        "QProgressBar {"
        "    border: 1px solid #2E7D32;"
        "    border-radius: 3px;"
        "    text-align: center;"
        "    background-color: #E0E0E0;"
        "}"
        "QProgressBar::chunk {"
        "    background-color: #4CAF50;"
        "    border-radius: 2px;"
        "}"
    );
    m_progressBar->setStyleSheet(styleSheet);

    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Set up progress callback
    int lastProcessedPercent = -1;
    XfsExtractor::SetProgressFunc([this, &lastProcessedPercent](int percent) {
        if (percent - lastProcessedPercent >= 5 || percent == 100)
        {
            lastProcessedPercent = percent;
            m_progressBar->setValue(percent);
            if (percent % 10 == 0 || percent == 100)
            {
                QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 0);
            }
        }
    });

    const std::string outDir = outputDir.toStdString();
    bool success = m_xfsExtractor->ExtractSelectedEntries(indices, outDir);

    // Clear progress callback
    XfsExtractor::SetProgressFunc(XfsExtractor::ProgressFunc());

    QApplication::restoreOverrideCursor();
    m_progressBar->setVisible(false);

    if (success)
    {
        m_statusLabel->setText(tr("Extracted %1 files").arg(indices.size()));
        QMessageBox::information(this, tr("Extraction Complete"),
            tr("Successfully extracted %1 files to:\n%2").arg(indices.size()).arg(outputDir));
    }
    else
    {
        m_statusLabel->setText(tr("Extraction failed"));
        QMessageBox::warning(this, tr("Extraction Failed"),
            tr("Failed to extract files. The archive may be corrupted."));
    }
}

void XfsViewerWindow::onSearchTextChanged(const QString& text)
{
    filterTable(text);
}

void XfsViewerWindow::onSelectAllMatches()
{
    if (!m_xfsExtractor)
        return;

    QString searchText = m_searchEdit->text().trimmed();
    if (searchText.isEmpty())
    {
        m_fileTable->selectAll();
        return;
    }

    m_fileTable->setSelectionMode(QAbstractItemView::MultiSelection);
    m_fileTable->clearSelection();

    for (int row = 0; row < m_fileTable->rowCount(); ++row)
    {
        if (!m_fileTable->isRowHidden(row))
        {
            m_fileTable->selectRow(row);
        }
    }

    m_fileTable->setSelectionMode(QAbstractItemView::ExtendedSelection);
    onSelectionChanged();
}

void XfsViewerWindow::filterTable(const QString& searchText)
{
    if (!m_xfsExtractor)
        return;

    QString searchLower = searchText.toLower();
    int visibleCount = 0;
    int totalRows = m_fileTable->rowCount();

    for (int row = 0; row < totalRows; ++row)
    {
        auto* pathItem = m_fileTable->item(row, 0);
        if (!pathItem)
        {
            m_fileTable->setRowHidden(row, true);
            continue;
        }

        QString path = pathItem->text().toLower();
        bool matches = searchText.isEmpty() || path.contains(searchLower);

        m_fileTable->setRowHidden(row, !matches);
        if (matches)
            ++visibleCount;
    }

    m_statusLabel->setText(tr("%1 files (%2 visible)").arg(totalRows).arg(visibleCount));
    m_selectAllMatchesButton->setEnabled(!searchText.isEmpty() && visibleCount > 0);
}

QString XfsViewerWindow::getFileTypeInfo(const QString& path) const
{
    QString ext = QFileInfo(path).suffix().toLower();

    if (ext == "spr")
        return tr("Sprite");
    if (ext == "img")
        return tr("Image Data");
    if (ext == "dat")
        return tr("Data File");
    if (ext == "txt" || ext == "cfg" || ext == "ini")
        return tr("Text Config");
    if (ext == "wav" || ext == "ogg" || ext == "mp3")
        return tr("Audio");
    if (ext == "bmp" || ext == "tga" || ext == "png" || ext == "jpg" || ext == "dds")
        return tr("Texture");
    if (ext == "ani")
        return tr("Animation");
    if (ext == "mdl" || ext == "msh")
        return tr("Model");
    if (ext == "lua" || ext == "py")
        return tr("Script");

    if (ext.isEmpty())
        return tr("Unknown");

    return ext.toUpper();
}

QString XfsViewerWindow::formatSize(uint64_t size) const
{
    if (size < 1024)
        return QString::number(size) + " B";
    else if (size < 1024 * 1024)
        return QString::number(size / 1024.0, 'f', 1) + " KB";
    else if (size < 1024 * 1024 * 1024)
        return QString::number(size / (1024.0 * 1024.0), 'f', 2) + " MB";
    else
        return QString::number(size / (1024.0 * 1024.0 * 1024.0), 'f', 2) + " GB";
}
