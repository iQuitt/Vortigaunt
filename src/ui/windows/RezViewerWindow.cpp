#include "core/extractors/rez/RezExtractor.h"
#include "DtxViewerDialog.h"
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
#include <QTemporaryDir>
#include <QByteArray>
#include <QShortcut>

#include <fstream>
#include <filesystem>
#include <vector>

#include "LanguageManager.h"
#include "RezViewerWindow.h"

RezViewerWindow::RezViewerWindow(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
}

RezViewerWindow::~RezViewerWindow() = default;

void RezViewerWindow::setupUI()
{
    setWindowTitle(tr("REZ Viewer - LithTech Engine"));
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    setMinimumSize(400, 300);
    
    QScreen* screen = QGuiApplication::primaryScreen();
    QSize screenSize = screen ? screen->availableGeometry().size() : QSize(1920, 1080);
    resize(screenSize.width() * 0.7, screenSize.height() * 0.7);

    auto* mainLayout = new QVBoxLayout(this);

    auto* fileGroup = new QGroupBox(tr("REZ File"));
    auto* fileLayout = new QHBoxLayout();

    m_rezPathEdit = new QLineEdit();
    m_rezPathEdit->setReadOnly(true);
    m_rezPathEdit->setPlaceholderText(tr("Select a REZ file to view..."));

    m_openButton = new QPushButton(tr("Open REZ..."));

    fileLayout->addWidget(m_rezPathEdit, 1);
    fileLayout->addWidget(m_openButton);
    fileGroup->setLayout(fileLayout);

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

    m_viewDtxButton = new QPushButton(tr("View DTX"));
    m_viewDtxButton->setEnabled(false);
    m_viewDtxButton->setToolTip(tr("View selected DTX file in DTX Viewer"));

    m_viewSprButton = new QPushButton(tr("View SPR"));
    m_viewSprButton->setEnabled(false);
    m_viewSprButton->setToolTip(tr("View selected SPR file in Lithtech Sprite Viewer"));

    m_extractSelectedButton = new QPushButton(tr("Extract Selected"));
    m_extractSelectedButton->setEnabled(false);

    m_extractAllButton = new QPushButton(tr("Extract All"));
    m_extractAllButton->setEnabled(false);

    m_statusLabel = new QLabel(tr("No REZ file loaded"));

    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);

    buttonLayout->addWidget(m_viewDtxButton);
    buttonLayout->addWidget(m_viewSprButton);
    buttonLayout->addWidget(m_extractSelectedButton);
    buttonLayout->addWidget(m_extractAllButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_statusLabel);

    mainLayout->addWidget(fileGroup);
    mainLayout->addWidget(listGroup, 1);
    mainLayout->addLayout(buttonLayout);
    mainLayout->addWidget(m_progressBar);

    // Connections
    connect(m_openButton, &QPushButton::clicked, this, &RezViewerWindow::onOpenRez);
    connect(m_extractSelectedButton, &QPushButton::clicked, this, &RezViewerWindow::onExtractSelected);
    connect(m_extractAllButton, &QPushButton::clicked, this, &RezViewerWindow::onExtractAll);
    connect(m_viewDtxButton, &QPushButton::clicked, this, &RezViewerWindow::onViewDtx);
    connect(m_viewSprButton, &QPushButton::clicked, this, &RezViewerWindow::onViewSpr);
    connect(m_fileTable, &QTableWidget::itemSelectionChanged, this, &RezViewerWindow::onSelectionChanged);
    connect(m_fileTable, &QTableWidget::cellDoubleClicked, this, &RezViewerWindow::onDoubleClick);
    connect(m_fileTable, &QTableWidget::itemSelectionChanged, this, [this]() {
        QList<QTableWidgetItem*> selected = m_fileTable->selectedItems();
    });
    connect(m_searchEdit, &QLineEdit::textChanged, this, &RezViewerWindow::onSearchTextChanged);
    connect(m_selectAllMatchesButton, &QPushButton::clicked, this, &RezViewerWindow::onSelectAllMatches);
    
    
    auto* searchShortcut = new QShortcut(QKeySequence("Ctrl+F"), this);
    connect(searchShortcut, &QShortcut::activated, this, [this]() {
        m_searchEdit->setFocus();
        m_searchEdit->selectAll();
    });
}

void RezViewerWindow::onOpenRez()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open REZ..."),
        QString(),
        tr("REZ files (*.rez);;All files (*.*)")
    );

    if (filePath.isEmpty())
        return;

    loadRez(filePath);
}

bool RezViewerWindow::loadRez(const QString& filePath)
{
    // Show progress bar with purple color
    m_progressBar->setVisible(true);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setFormat(tr("Loading REZ: %p%"));
    
    // Set purple color for progress bar
    QString styleSheet = QString(
        "QProgressBar {"
        "    border: 1px solid #7B2CBF;"
        "    border-radius: 3px;"
        "    text-align: center;"
        "    background-color: palette(base);"
        "}"
        "QProgressBar::chunk {"
        "    background-color: #7B2CBF;"
        "    border-radius: 2px;"
        "}"
    );
    m_progressBar->setStyleSheet(styleSheet);
    
    m_statusLabel->setText(tr("Loading REZ file..."));
    QApplication::processEvents();

    m_rezExtractor = std::make_unique<RezExtractor>();
    
    int lastProcessedPercent = -1;
    RezExtractor::SetProgressFunc([this, &lastProcessedPercent](int percent) {
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
    
    if (!m_rezExtractor->Load(filePath.toStdString()))
    {
        m_progressBar->setVisible(false);
        RezExtractor::SetProgressFunc(RezExtractor::ProgressFunc()); // Clear callback
        QMessageBox::critical(this, tr("Error"), tr("Failed to load REZ file."));
        m_rezExtractor.reset();
        return false;
    }

    RezExtractor::SetProgressFunc(RezExtractor::ProgressFunc());
    m_progressBar->setVisible(false);
    m_currentRezPath = filePath;
    m_rezPathEdit->setText(filePath);

    populateFileList();
    return true;
}

void RezViewerWindow::populateFileList()
{
    m_fileTable->setRowCount(0);

    if (!m_rezExtractor)
        return;

    const auto& entries = m_rezExtractor->GetEntries();
    const size_t entryCount = entries.size();

    if (entryCount == 0)
    {
        m_extractAllButton->setEnabled(false);
        m_statusLabel->setText(tr("No files"));
        return;
    }

    m_fileTable->setSortingEnabled(false);
    m_fileTable->setUpdatesEnabled(false);

    m_fileTable->setRowCount(static_cast<int>(entryCount));


    constexpr size_t BATCH_SIZE = 5000; // Process 5000 items at a time
    
    for (size_t i = 0; i < entryCount; ++i)
    {
        const auto& entry = entries[i];

        QString qPath = QString::fromStdString(entry.filename);

        auto* pathItem = new QTableWidgetItem(qPath);
        auto* sizeItem = new QTableWidgetItem(formatSize(entry.size));
        auto* typeItem = new QTableWidgetItem(getFileTypeInfo(qPath));

        sizeItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        typeItem->setTextAlignment(Qt::AlignCenter);

        m_fileTable->setItem(static_cast<int>(i), 0, pathItem);
        m_fileTable->setItem(static_cast<int>(i), 1, sizeItem);
        m_fileTable->setItem(static_cast<int>(i), 2, typeItem);

        // Update UI less frequently for better performance
        if ((i + 1) % BATCH_SIZE == 0 || i == entryCount - 1)
        {
            m_fileTable->setUpdatesEnabled(true);
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents, 0);
            m_fileTable->setUpdatesEnabled(false);
        }
    }

    m_fileTable->setUpdatesEnabled(true);
    m_fileTable->setSortingEnabled(true);

    m_fileTable->resizeColumnsToContents();
    m_fileTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);

    m_extractAllButton->setEnabled(true);
    m_statusLabel->setText(QString(tr("%1 files")).arg(entryCount));
}

void RezViewerWindow::onSelectionChanged()
{
    QList<QTableWidgetItem*> selectedItems = m_fileTable->selectedItems();
    bool hasSelection = !selectedItems.isEmpty();
    m_extractSelectedButton->setEnabled(hasSelection);

    // Check if any selected file is a DTX or SPR
    bool hasDtx = false;
    bool hasSpr = false;
    if (hasSelection)
    {
        QSet<int> rows;
        for (auto* item : selectedItems)
            rows.insert(item->row());

        for (int row : rows)
        {
            auto* pathItem = m_fileTable->item(row, 0);
            if (pathItem)
            {
                if (canViewAsDtx(pathItem->text()))
                    hasDtx = true;
                if (canViewAsSpr(pathItem->text()))
                    hasSpr = true;
            }
        }
    }

    m_viewDtxButton->setEnabled(hasDtx);
    m_viewSprButton->setEnabled(hasSpr);
}

void RezViewerWindow::onDoubleClick(int row, int column)
{
    Q_UNUSED(column);
    
    auto* pathItem = m_fileTable->item(row, 0);
    if (pathItem)
    {
        if (canViewAsDtx(pathItem->text()))
            viewDtxEntry(static_cast<size_t>(row));
        else if (canViewAsSpr(pathItem->text()))
            viewSprEntry(static_cast<size_t>(row));
    }
}

bool RezViewerWindow::canViewAsDtx(const QString& path) const
{
    QString ext = QFileInfo(path).suffix().toLower();
    return (ext == "dtx");
}

bool RezViewerWindow::canViewAsSpr(const QString& path) const
{
    QString ext = QFileInfo(path).suffix().toLower();
    return (ext == "spr");
}

void RezViewerWindow::onViewDtx()
{
    QList<QTableWidgetItem*> selectedItems = m_fileTable->selectedItems();
    if (selectedItems.isEmpty())
        return;

    // Get first selected DTX row
    QSet<int> rows;
    for (auto* item : selectedItems)
        rows.insert(item->row());

    for (int row : rows)
    {
        auto* pathItem = m_fileTable->item(row, 0);
        if (pathItem && canViewAsDtx(pathItem->text()))
        {
            viewDtxEntry(static_cast<size_t>(row));
            return;
        }
    }
}

void RezViewerWindow::onViewSpr()
{
    QList<QTableWidgetItem*> selectedItems = m_fileTable->selectedItems();
    if (selectedItems.isEmpty())
        return;

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

void RezViewerWindow::viewDtxEntry(size_t index)
{
    if (!m_rezExtractor)
        return;

    const auto& entries = m_rezExtractor->GetEntries();
    if (index >= entries.size())
        return;

    const auto& entry = entries[index];
    QString qPath = QString::fromStdString(entry.filename);

    QApplication::setOverrideCursor(Qt::WaitCursor);

    std::vector<char> rawData;
    const bool extracted = m_rezExtractor->ExtractEntryToMemory(entry, rawData);

    QApplication::restoreOverrideCursor();

    if (!extracted || rawData.empty())
    {
        QMessageBox::warning(this, tr("Error"), tr("Failed to read DTX data from REZ file."));
        return;
    }

    QByteArray byteArray(rawData.data(), static_cast<int>(rawData.size()));

    // Open DTX Viewer dialog and load directly from memory
    DtxViewerDialog* dlg = new DtxViewerDialog(this);
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    if (!dlg->loadDtxFromMemory(byteArray, QFileInfo(qPath).fileName()))
    {
        dlg->deleteLater();
        return;
    }
    dlg->show();
}

void RezViewerWindow::viewSprEntry(size_t index)
{
    if (!m_rezExtractor)
        return;

    const auto& entries = m_rezExtractor->GetEntries();
    if (index >= entries.size())
        return;

    const auto& entry = entries[index];
    QString qPath = QString::fromStdString(entry.filename);

    QApplication::setOverrideCursor(Qt::WaitCursor);

    // Extract SPR to temp directory
    QString tempDir = QDir::tempPath() + "/VortigauntRezViewer";
    QDir().mkpath(tempDir);

    QString fileName = QFileInfo(qPath).fileName();
    QString tempFilePath = tempDir + "/" + fileName;

    bool extracted = m_rezExtractor->ExtractEntry(entry, tempFilePath.toStdString());

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

void RezViewerWindow::onExtractSelected()
{
    if (!m_rezExtractor)
        return;

    QList<QTableWidgetItem*> selectedItems = m_fileTable->selectedItems();
    if (selectedItems.isEmpty())
        return;

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
        tr("Select Output Folder"),
        QDir::homePath()
    );

    if (outputDir.isEmpty())
        return;

    extractEntries(indices, outputDir);
}

void RezViewerWindow::onExtractAll()
{
    if (!m_rezExtractor)
        return;

    const auto& entries = m_rezExtractor->GetEntries();
    if (entries.empty())
        return;

    QString outputDir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Output Folder"),
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

void RezViewerWindow::extractEntries(const std::vector<size_t>& indices, const QString& outputDir)
{
    if (!m_rezExtractor || indices.empty())
        return;

    const auto& entries = m_rezExtractor->GetEntries();

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

        std::filesystem::path relPath(entry.filename);
        std::filesystem::path fullPath = outPath / relPath;

        if (m_rezExtractor->ExtractEntry(entry, fullPath.string()))
        {
            ++okCount;
        }
        else
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

QString RezViewerWindow::getFileTypeInfo(const QString& path) const
{
    QString ext = QFileInfo(path).suffix().toLower();

    if (ext == "dtx")
        return tr("LithTech Texture");
    if (ext == "ltb")
        return tr("LithTech Model");
    if (ext == "lta")
        return tr("LithTech Animation");
    if (ext == "wav")
        return tr("Wave Audio");
    if (ext == "bmp" || ext == "tga" || ext == "png" || ext == "jpg")
        return tr("Image");
    if (ext == "txt" || ext == "cfg" || ext == "res")
        return tr("Text File");
    if (ext == "spr")
        return tr("Lithtech Sprite");
    if (ext == "dat")
        return tr("Map Format");
    
    return ext.toUpper();
}

QString RezViewerWindow::formatSize(uint32_t size) const
{
    if (size < 1024)
        return QString::number(size) + " B";
    else if (size < 1024 * 1024)
        return QString::number(size / 1024.0, 'f', 1) + " KB";
    else
        return QString::number(size / (1024.0 * 1024.0), 'f', 2) + " MB";
}

void RezViewerWindow::onSearchTextChanged(const QString& text)
{
    filterTable(text);
}

void RezViewerWindow::onSelectAllMatches()
{
    if (!m_rezExtractor)
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

void RezViewerWindow::filterTable(const QString& searchText)
{
    if (!m_rezExtractor)
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

