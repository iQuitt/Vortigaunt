#include "SpriteViewerWindow.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QFileInfo>
#include <QHeaderView>
#include <QProgressDialog>
#include <QApplication>
#include <QScreen>
#include <QInputDialog>
#include <QPainter>
#include <QScrollBar>
#include <QWheelEvent>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QToolTip>
#include <QColorDialog>
#include <QCollator>
#include <QSettings>
#include <QCloseEvent>
#include <QItemSelectionModel>
#include "SpriteFixDialog.h"
#include "LanguageManager.h"

#include <filesystem>
#include <fstream>

#include "utils/Bmp.h"

ZoomablePreviewWidget::ZoomablePreviewWidget(QWidget* parent)
    : QWidget(parent)
    , m_zoomFactor(1.0)
    , m_backgroundType(0)
{
    setMinimumSize(200, 200);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMouseTracking(true);
}

void ZoomablePreviewWidget::setImage(const QImage& image)
{
    m_image = image;
    update();
}

void ZoomablePreviewWidget::resetZoom()
{
    m_zoomFactor = 1.0;
    emit zoomChanged(m_zoomFactor);
    update();
}

void ZoomablePreviewWidget::clearImage()
{
    m_image = QImage();
    update();
}

void ZoomablePreviewWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    
    // Draw background
    QRect bgRect = rect();
    switch (m_backgroundType) {
        case 0: // Checkered
            drawCheckeredBackground(painter, bgRect);
            break;
        case 1: 
            painter.fillRect(bgRect, Qt::black);
            break;
        case 2:
            painter.fillRect(bgRect, Qt::white);
            break;
        case 3: 
            painter.fillRect(bgRect, Qt::gray);
            break;
    }
    
    if (m_image.isNull())
        return;
    
    // Calculate scaled size
    int scaledWidth = static_cast<int>(m_image.width() * m_zoomFactor);
    int scaledHeight = static_cast<int>(m_image.height() * m_zoomFactor);
    
    // Center the image
    int imgX = (width() - scaledWidth) / 2;
    int imgY = (height() - scaledHeight) / 2;
    
    // Draw image
    QImage scaled = m_image.scaled(scaledWidth, scaledHeight, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    painter.drawImage(imgX, imgY, scaled);
    
    // Draw selection rectangle if valid
    if (!m_selectionRect.isEmpty()) {
        // Convert image coordinates to widget coordinates
        int selX = imgX + static_cast<int>(m_selectionRect.x() * m_zoomFactor);
        int selY = imgY + static_cast<int>(m_selectionRect.y() * m_zoomFactor);
        int selW = static_cast<int>(m_selectionRect.width() * m_zoomFactor);
        int selH = static_cast<int>(m_selectionRect.height() * m_zoomFactor);
        
        QRect widgetSelRect(selX, selY, selW, selH);
        
        // Draw semi-transparent fill
        painter.fillRect(widgetSelRect, QColor(0, 200, 255, 50));
        
        // Draw dashed border
        QPen pen(QColor(0, 200, 255), 2, Qt::DashLine);
        painter.setPen(pen);
        painter.drawRect(widgetSelRect);
        
        // Draw corner markers
        painter.setPen(QPen(QColor(255, 100, 100), 2));
        int markerSize = 6;
        // Top-left
        painter.drawLine(selX, selY, selX + markerSize, selY);
        painter.drawLine(selX, selY, selX, selY + markerSize);
        // Top-right
        painter.drawLine(selX + selW, selY, selX + selW - markerSize, selY);
        painter.drawLine(selX + selW, selY, selX + selW, selY + markerSize);
        // Bottom-left
        painter.drawLine(selX, selY + selH, selX + markerSize, selY + selH);
        painter.drawLine(selX, selY + selH, selX, selY + selH - markerSize);
        // Bottom-right
        painter.drawLine(selX + selW, selY + selH, selX + selW - markerSize, selY + selH);
        painter.drawLine(selX + selW, selY + selH, selX + selW, selY + selH - markerSize);
    }
}

void ZoomablePreviewWidget::wheelEvent(QWheelEvent* event)
{
    double oldZoom = m_zoomFactor;
    
    if (event->angleDelta().y() > 0) {
        // Zoom in
        m_zoomFactor = qMin(m_zoomFactor * 1.2, 8.0);
    } else {
        // Zoom out
        m_zoomFactor = qMax(m_zoomFactor / 1.2, 0.25);
    }
    
    if (m_zoomFactor != oldZoom) {
        emit zoomChanged(m_zoomFactor);
        update();
    }
    
    event->accept();
}

void ZoomablePreviewWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::RightButton) {
        // Right click cycles through backgrounds
        m_backgroundType = (m_backgroundType + 1) % 4;
        emit backgroundChanged(m_backgroundType);
        update();
    } else if (event->button() == Qt::LeftButton && !m_image.isNull()) {
        // Left click starts selection
        QPoint imgCoord = widgetToImage(event->pos());
        if (imgCoord.x() >= 0) {
            m_isSelecting = true;
            m_selectionStart = imgCoord;
            m_selectionEnd = imgCoord;
            m_selectionRect = QRect();
            update();
        }
    }
    QWidget::mousePressEvent(event);
}

void ZoomablePreviewWidget::mouseMoveEvent(QMouseEvent* event)
{
    if (m_image.isNull()) {
        emit coordinatesChanged(0, 0, false);
        return;
    }
    
    QPoint imgCoord = widgetToImage(event->pos());
    
    if (imgCoord.x() >= 0) {
        emit coordinatesChanged(imgCoord.x(), imgCoord.y(), true);
        
        // Update selection if dragging
        if (m_isSelecting) {
            m_selectionEnd = imgCoord;
            
            // Create normalized rectangle
            int x1 = qMin(m_selectionStart.x(), m_selectionEnd.x());
            int y1 = qMin(m_selectionStart.y(), m_selectionEnd.y());
            int x2 = qMax(m_selectionStart.x(), m_selectionEnd.x());
            int y2 = qMax(m_selectionStart.y(), m_selectionEnd.y());
            
            m_selectionRect = QRect(x1, y1, x2 - x1 + 1, y2 - y1 + 1);
            
            emit selectionChanged(m_selectionRect.x(), m_selectionRect.y(), 
                                  m_selectionRect.width(), m_selectionRect.height(), true);
            update();
        }
    } else {
        emit coordinatesChanged(0, 0, false);
    }
    
    QWidget::mouseMoveEvent(event);
}

void ZoomablePreviewWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && m_isSelecting) {
        m_isSelecting = false;
        
        // Emit final selection
        if (!m_selectionRect.isEmpty()) {
            emit selectionChanged(m_selectionRect.x(), m_selectionRect.y(), 
                                  m_selectionRect.width(), m_selectionRect.height(), true);
        }
    }
    QWidget::mouseReleaseEvent(event);
}

void ZoomablePreviewWidget::leaveEvent(QEvent* event)
{
    emit coordinatesChanged(0, 0, false);
    QWidget::leaveEvent(event);
}

QPoint ZoomablePreviewWidget::widgetToImage(const QPoint& widgetPos) const
{
    if (m_image.isNull())
        return QPoint(-1, -1);
    
    // Calculate scaled size
    int scaledWidth = static_cast<int>(m_image.width() * m_zoomFactor);
    int scaledHeight = static_cast<int>(m_image.height() * m_zoomFactor);
    
    // Calculate image position (centered)
    int imgX = (width() - scaledWidth) / 2;
    int imgY = (height() - scaledHeight) / 2;
    
    // Check if within image bounds
    if (widgetPos.x() >= imgX && widgetPos.x() < imgX + scaledWidth &&
        widgetPos.y() >= imgY && widgetPos.y() < imgY + scaledHeight) {
        
        int imageX = static_cast<int>((widgetPos.x() - imgX) / m_zoomFactor);
        int imageY = static_cast<int>((widgetPos.y() - imgY) / m_zoomFactor);
        
        // Clamp to image bounds
        imageX = qBound(0, imageX, m_image.width() - 1);
        imageY = qBound(0, imageY, m_image.height() - 1);
        
        return QPoint(imageX, imageY);
    }
    
    return QPoint(-1, -1);
}

void ZoomablePreviewWidget::drawCheckeredBackground(QPainter& painter, const QRect& rect)
{
    const int cellSize = 16;
    QColor light(200, 200, 200);
    QColor dark(150, 150, 150);
    
    for (int y = rect.top(); y < rect.bottom(); y += cellSize) {
        for (int x = rect.left(); x < rect.right(); x += cellSize) {
            bool isLight = ((x / cellSize) + (y / cellSize)) % 2 == 0;
            painter.fillRect(x, y, cellSize, cellSize, isLight ? light : dark);
        }
    }
}

// ============================================================================
// PaletteWidget Implementation
// ============================================================================

PaletteWidget::PaletteWidget(QWidget* parent)
    : QWidget(parent)
    , m_cellSize(16)
    , m_hoveredIndex(-1)
{
    setFixedSize(16 * m_cellSize + 2, 16 * m_cellSize + 2);
    setMouseTracking(true);
}

void PaletteWidget::setPalette(const QVector<QRgb>& colors)
{
    m_colors = colors;
    update();
}

void PaletteWidget::clearPalette()
{
    m_colors.clear();
    update();
}

void PaletteWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    
    // Draw border
    painter.setPen(Qt::gray);
    painter.drawRect(0, 0, width() - 1, height() - 1);
    
    // Draw color cells
    for (int i = 0; i < 256 && i < m_colors.size(); i++) {
        int x = (i % 16) * m_cellSize + 1;
        int y = (i / 16) * m_cellSize + 1;
        
        QColor color(m_colors[i]);
        painter.fillRect(x, y, m_cellSize, m_cellSize, color);
    }
    
    // Fill empty cells with black
    for (int i = m_colors.size(); i < 256; i++) {
        int x = (i % 16) * m_cellSize + 1;
        int y = (i / 16) * m_cellSize + 1;
        painter.fillRect(x, y, m_cellSize, m_cellSize, Qt::black);
    }
}

void PaletteWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !m_colors.isEmpty()) {
        int x = (event->pos().x() - 1) / m_cellSize;
        int y = (event->pos().y() - 1) / m_cellSize;
        int index = y * 16 + x;
        
        if (index >= 0 && index < m_colors.size()) {
            emit colorClicked(index, m_colors[index]);
        }
    }
    QWidget::mousePressEvent(event);
}

void PaletteWidget::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && !m_colors.isEmpty()) {
        int x = (event->pos().x() - 1) / m_cellSize;
        int y = (event->pos().y() - 1) / m_cellSize;
        int index = y * 16 + x;
        
        if (index >= 0 && index < m_colors.size()) {
            emit colorDoubleClicked(index, m_colors[index]);
        }
    }
    QWidget::mouseDoubleClickEvent(event);
}


SpriteViewerWindow::SpriteViewerWindow(QWidget* parent)
    : QDialog(parent)
    , m_fileCurrentFrameIndex(0)
    , m_fileIsPlaying(false)
    , m_fileFrameDisplayLabel(nullptr)
{
    setAcceptDrops(true);
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    setupUI();
    
    QScreen* screen = QGuiApplication::primaryScreen();
    QSize screenSize = screen ? screen->availableGeometry().size() : QSize(1920, 1080);
    resize(screenSize.width() * 0.7, screenSize.height() * 0.7);
    
    m_animationTimer = new QTimer(this);
    connect(m_animationTimer, &QTimer::timeout, this, &SpriteViewerWindow::onAnimationTimer);
    
    // Restore last browsed path
    QSettings settings("Vortigaunt", "SpriteViewer");
    QString lastPath = settings.value("lastBrowsedPath", QDir::homePath()).toString();
    if (QDir(lastPath).exists()) {
        navigateToPath(lastPath);
    }
}

void SpriteViewerWindow::openSprite(const QString& filePath)
{
    loadSprite(filePath);
}

void SpriteViewerWindow::setupUI()
{
    setWindowTitle(tr("Sprite Viewer"));
    
    auto* mainLayout = new QHBoxLayout(this);
    
    // Left side: File browser
    auto* fileBrowserWidget = createFileBrowserWidget();
    
    // Right side: Tab widget
    auto* tabWidget = new QTabWidget(this);
    tabWidget->addTab(createFileTabWidget(), tr("File"));
    tabWidget->addTab(createCreateTabWidget(), tr("Create"));
    tabWidget->addTab(createFixTabWidget(), tr("Fix"));
    
    // Splitter to make file browser resizable
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(fileBrowserWidget);
    splitter->addWidget(tabWidget);
    splitter->setSizes({250, 1150}); // File browser 250px, rest for tabs
    
    mainLayout->addWidget(splitter);
}

QWidget* SpriteViewerWindow::createFileBrowserWidget()
{
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // Navigation bar
    auto* navLayout = new QHBoxLayout();
    m_backButton = new QPushButton(tr("<"));
    m_forwardButton = new QPushButton(tr(">"));
    m_upButton = new QPushButton(tr("^"));
    m_backButton->setFixedWidth(30);
    m_forwardButton->setFixedWidth(30);
    m_upButton->setFixedWidth(30);
    m_backButton->setEnabled(false);
    m_forwardButton->setEnabled(false);
    m_backButton->setToolTip(tr("Back"));
    m_forwardButton->setToolTip(tr("Forward"));
    m_upButton->setToolTip(tr("Up"));
    
    navLayout->addWidget(m_backButton);
    navLayout->addWidget(m_forwardButton);
    navLayout->addWidget(m_upButton);
    navLayout->addStretch();
    
    // Path edit
    m_pathEdit = new QLineEdit();
    m_pathEdit->setPlaceholderText(tr("Path..."));
    
    // File system model and tree view
    m_fileBrowserModel = new QFileSystemModel(this);
    m_fileBrowserModel->setRootPath("");
    m_fileBrowserModel->setNameFilters({"*.spr"});
    m_fileBrowserModel->setNameFilterDisables(false);
    
    m_fileBrowser = new QTreeView();
    m_fileBrowser->setModel(m_fileBrowserModel);
    m_fileBrowser->setRootIndex(m_fileBrowserModel->index(QDir::homePath()));
    m_fileBrowser->hideColumn(1); // Size
    m_fileBrowser->hideColumn(2); // Type
    m_fileBrowser->hideColumn(3); // Date Modified
    m_fileBrowser->header()->hide();
    m_fileBrowser->setAnimated(true);
    m_fileBrowser->setDragEnabled(true); // Enable dragging from file browser
    m_fileBrowser->setSelectionMode(QAbstractItemView::ExtendedSelection); // Allow multiple selection
    
    layout->addLayout(navLayout);
    layout->addWidget(m_pathEdit);
    layout->addWidget(m_fileBrowser, 1);
    
    connect(m_backButton, &QPushButton::clicked, this, &SpriteViewerWindow::onNavigateBack);
    connect(m_forwardButton, &QPushButton::clicked, this, &SpriteViewerWindow::onNavigateForward);
    connect(m_upButton, &QPushButton::clicked, this, &SpriteViewerWindow::onNavigateUp);
    connect(m_pathEdit, &QLineEdit::returnPressed, this, &SpriteViewerWindow::onPathEditReturnPressed);
    connect(m_fileBrowser->selectionModel(), &QItemSelectionModel::currentChanged, this, &SpriteViewerWindow::onFileBrowserCurrentChanged);
    connect(m_fileBrowser, &QTreeView::activated, this, &SpriteViewerWindow::onFileBrowserDoubleClicked);
    
    return widget;
}

QWidget* SpriteViewerWindow::createFileTabWidget()
{
    auto* widget = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(widget);
    
    // Top section: Horizontal layout with preview on left, controls on right
    auto* topLayout = new QHBoxLayout();
    
    // Left side: Frame Preview with zoom
    auto* previewGroup = new QGroupBox(tr("Frame Preview (Scroll=Zoom, RightClick=Background, LeftDrag=Select)"));
    auto* previewLayout = new QVBoxLayout();
    
    m_previewWidget = new ZoomablePreviewWidget();
    m_fileFrameScrollArea = new QScrollArea();
    m_fileFrameScrollArea->setWidgetResizable(true);
    m_fileFrameScrollArea->setWidget(m_previewWidget);
    m_fileFrameScrollArea->setMinimumSize(400, 400);
    
    // Zoom and background info
    auto* previewInfoLayout = new QHBoxLayout();
    m_zoomLabel = new QLabel(tr("Zoom: 100%"));
    m_resetZoomButton = new QPushButton(tr("Reset Zoom"));
    m_resetZoomButton->setFixedWidth(80);
    m_bgLabel = new QLabel(tr("Background: Checkered"));
    m_coordLabel = new QLabel(tr("X: - Y: -"));
    m_coordLabel->setMinimumWidth(100);
    m_coordLabel->setStyleSheet("font-family: monospace; font-weight: bold;");
    
    // Selection info for 640hud coordinates
    m_selectionLabel = new QLabel(tr("Selection: -"));
    m_selectionLabel->setMinimumWidth(200);
    m_selectionLabel->setStyleSheet("font-family: monospace; font-weight: bold; color: #00C8FF;");
    m_selectionLabel->setToolTip(tr("Drag to select region. Format: X, Y, Width, Height\nUse for 640hud sprite coordinates."));
    
    previewInfoLayout->addWidget(m_zoomLabel);
    previewInfoLayout->addWidget(m_resetZoomButton);
    previewInfoLayout->addStretch();
    previewInfoLayout->addWidget(m_selectionLabel);
    previewInfoLayout->addSpacing(15);
    previewInfoLayout->addWidget(m_coordLabel);
    previewInfoLayout->addSpacing(15);
    previewInfoLayout->addWidget(m_bgLabel);
    
    connect(m_resetZoomButton, &QPushButton::clicked, m_previewWidget, &ZoomablePreviewWidget::resetZoom);
    
    previewLayout->addWidget(m_fileFrameScrollArea);
    previewLayout->addLayout(previewInfoLayout);
    previewGroup->setLayout(previewLayout);
    
    // Connect preview signals
    connect(m_previewWidget, &ZoomablePreviewWidget::zoomChanged, this, &SpriteViewerWindow::onZoomChanged);
    connect(m_previewWidget, &ZoomablePreviewWidget::backgroundChanged, this, &SpriteViewerWindow::onBackgroundChanged);
    connect(m_previewWidget, &ZoomablePreviewWidget::coordinatesChanged, this, &SpriteViewerWindow::onCoordinatesChanged);
    connect(m_previewWidget, &ZoomablePreviewWidget::selectionChanged, this, &SpriteViewerWindow::onSelectionChanged);
    
    // Right side: Controls, frame list, and palette
    auto* rightLayout = new QVBoxLayout();
    
    // Sprite file selection
    auto* fileGroup = new QGroupBox(tr("Sprite File"));
    auto* fileLayout = new QHBoxLayout();
    
    m_fileSpritePathEdit = new QLineEdit();
    m_fileSpritePathEdit->setReadOnly(true);
    m_fileOpenButton = new QPushButton(tr("Open Sprite..."));
    
    fileLayout->addWidget(m_fileSpritePathEdit, 1);
    fileLayout->addWidget(m_fileOpenButton);
    fileGroup->setLayout(fileLayout);
    
    // Info label
    m_fileInfoLabel = new QLabel(tr("No sprite loaded."));
    
    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    m_extractFrameButton = new QPushButton(tr("Extract Frame..."));
    m_extractFrameButton->setEnabled(false);
    m_extractAllFramesButton = new QPushButton(tr("Extract All Frames..."));
    m_extractAllFramesButton->setEnabled(false);
    m_saveButton = new QPushButton(tr("Save (Ctrl+S)"));
    m_saveButton->setEnabled(false);
    m_saveButton->setShortcut(QKeySequence::Save);
    m_saveAsButton = new QPushButton(tr("Save As..."));
    m_saveAsButton->setEnabled(false);
    m_fixSpriteButton = new QPushButton(tr("Fix Sprite"));
    m_fixSpriteButton->setEnabled(false);
    m_fixSpriteButton->setVisible(false);
    
    buttonLayout->addWidget(m_extractFrameButton);
    buttonLayout->addWidget(m_extractAllFramesButton);
    buttonLayout->addWidget(m_saveButton);
    buttonLayout->addWidget(m_saveAsButton);
    buttonLayout->addWidget(m_fixSpriteButton);
    buttonLayout->addStretch();
    
    connect(m_saveButton, &QPushButton::clicked, this, &SpriteViewerWindow::onQuickSave);
    connect(m_saveAsButton, &QPushButton::clicked, this, &SpriteViewerWindow::onSaveAsSprite);
    
    // Lithtech-specific panel (hidden by default)
    m_lithtechPanel = new QWidget();
    auto* lithtechLayout = new QVBoxLayout(m_lithtechPanel);
    lithtechLayout->setContentsMargins(0, 0, 0, 0);
    
    auto* texDirGroup = new QGroupBox(tr("Lithtech Texture Directory"));
    auto* texDirLayout = new QHBoxLayout();
    m_lithtechTextureDirEdit = new QLineEdit();
    m_lithtechTextureDirEdit->setPlaceholderText(tr("Browse for DTX texture directory..."));
    m_lithtechBrowseDirButton = new QPushButton(tr("Browse..."));
    m_lithtechLoadTexturesButton = new QPushButton(tr("Load Textures"));
    texDirLayout->addWidget(m_lithtechTextureDirEdit, 1);
    texDirLayout->addWidget(m_lithtechBrowseDirButton);
    texDirLayout->addWidget(m_lithtechLoadTexturesButton);
    texDirGroup->setLayout(texDirLayout);
    
    auto* lithtechExportLayout = new QHBoxLayout();
    m_lithtechExportGoldSrcButton = new QPushButton(tr("Export to GoldSrc SPR"));
    m_lithtechExportGoldSrcButton->setEnabled(false);
    m_lithtechExportFramesButton = new QPushButton(tr("Export Frames as BMP"));
    m_lithtechExportFramesButton->setEnabled(false);
    lithtechExportLayout->addWidget(m_lithtechExportGoldSrcButton);
    lithtechExportLayout->addWidget(m_lithtechExportFramesButton);
    lithtechExportLayout->addStretch();
    
    lithtechLayout->addWidget(texDirGroup);
    lithtechLayout->addLayout(lithtechExportLayout);
    m_lithtechPanel->setVisible(false);
    
    connect(m_lithtechBrowseDirButton, &QPushButton::clicked, this, &SpriteViewerWindow::onBrowseLithtechTextureDir);
    connect(m_lithtechLoadTexturesButton, &QPushButton::clicked, this, &SpriteViewerWindow::onLoadLithtechTextures);
    connect(m_lithtechExportGoldSrcButton, &QPushButton::clicked, this, &SpriteViewerWindow::onExportLithtechToGoldSrc);
    connect(m_lithtechExportFramesButton, &QPushButton::clicked, this, &SpriteViewerWindow::onExportLithtechFrames);
    
    // Frame list
    auto* frameListGroup = new QGroupBox(tr("Frames"));
    auto* frameListLayout = new QVBoxLayout();
    
    m_fileFrameListWidget = new QListWidget();
    m_fileFrameListWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fileFrameListWidget->setMovement(QListView::Static);
    m_fileFrameListWidget->installEventFilter(this);
    
    frameListLayout->addWidget(m_fileFrameListWidget);
    frameListGroup->setLayout(frameListLayout);
    
    // Color Palette
    auto* paletteGroup = new QGroupBox(tr("Color Palette"));
    auto* paletteLayout = new QVBoxLayout();
    
    m_paletteWidget = new PaletteWidget();
    m_paletteInfoLabel = new QLabel(tr("Click a color to see RGB values"));
    
    // Palette manipulation toolbar
    auto* paletteToolbar = new QHBoxLayout();
    m_paletteBrightnessUp = new QPushButton(tr("B+"));
    m_paletteBrightnessDown = new QPushButton(tr("B-"));
    m_paletteGrayscale = new QPushButton(tr("Grayscale"));
    m_paletteSepia = new QPushButton(tr("Sepia"));
    m_paletteContrast = new QPushButton(tr("Contrast"));
    m_paletteInvert = new QPushButton(tr("Invert"));
    
    // Swap button with dropdown menu
    m_paletteSwap = new QPushButton(tr("Swap"));
    m_swapMenu = new QMenu(this);
    m_swapMenu->addAction(tr("R <-> B"), this, &SpriteViewerWindow::onPaletteSwapRB);
    m_swapMenu->addAction(tr("R <-> G"), this, &SpriteViewerWindow::onPaletteSwapRG);
    m_swapMenu->addAction(tr("B <-> G"), this, &SpriteViewerWindow::onPaletteSwapBG);
    m_paletteSwap->setMenu(m_swapMenu);
    
    // Set small fixed widths for compact buttons
    m_paletteBrightnessUp->setFixedWidth(30);
    m_paletteBrightnessDown->setFixedWidth(30);
    m_paletteGrayscale->setFixedWidth(65);
    m_paletteSepia->setFixedWidth(45);
    m_paletteContrast->setFixedWidth(65);
    m_paletteInvert->setFixedWidth(50);
    m_paletteSwap->setFixedWidth(55);
    
    paletteToolbar->addWidget(m_paletteBrightnessUp);
    paletteToolbar->addWidget(m_paletteBrightnessDown);
    paletteToolbar->addWidget(m_paletteGrayscale);
    paletteToolbar->addWidget(m_paletteSepia);
    paletteToolbar->addWidget(m_paletteContrast);
    paletteToolbar->addWidget(m_paletteInvert);
    paletteToolbar->addWidget(m_paletteSwap);
    paletteToolbar->addStretch();
    
    // Palette widget with undo/redo/reset buttons on the right
    auto* paletteContentLayout = new QHBoxLayout();
    paletteContentLayout->addWidget(m_paletteWidget);
    
    auto* undoRedoLayout = new QVBoxLayout();
    m_paletteUndo = new QPushButton(tr("↩"));
    m_paletteRedo = new QPushButton(tr("↪"));
    m_paletteReset = new QPushButton(tr("⟲"));
    m_paletteUndo->setFixedSize(30, 30);
    m_paletteRedo->setFixedSize(30, 30);
    m_paletteReset->setFixedSize(30, 30);
    m_paletteUndo->setToolTip(tr("Undo (Ctrl+Z)"));
    m_paletteRedo->setToolTip(tr("Redo (Ctrl+Y)"));
    m_paletteReset->setToolTip(tr("Reset to original"));
    m_paletteUndo->setShortcut(QKeySequence::Undo); // Ctrl+Z
    m_paletteRedo->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Y)); // Ctrl+Y
    m_paletteUndo->setEnabled(false);
    m_paletteRedo->setEnabled(false);
    m_paletteReset->setEnabled(false);
    undoRedoLayout->addWidget(m_paletteUndo);
    undoRedoLayout->addWidget(m_paletteRedo);
    undoRedoLayout->addWidget(m_paletteReset);
    undoRedoLayout->addStretch();
    paletteContentLayout->addLayout(undoRedoLayout);
    
    // Hue/Saturation sliders
    auto* hsLayout = new QHBoxLayout();
    m_hueLabel = new QLabel(tr("H: 0"));
    m_hueLabel->setMinimumWidth(20);
    m_hueSlider = new QSlider(Qt::Horizontal);
    m_hueSlider->setRange(-180, 180);
    m_hueSlider->setValue(0);
    m_hueSlider->setFixedWidth(100);
    m_satLabel = new QLabel(tr("S: 0"));
    m_satLabel->setMinimumWidth(20);
    m_saturationSlider = new QSlider(Qt::Horizontal);
    m_saturationSlider->setRange(-100, 100);
    m_saturationSlider->setValue(0);
    m_saturationSlider->setFixedWidth(100);

    hsLayout->addWidget(m_hueLabel);
    hsLayout->addWidget(m_hueSlider);
    hsLayout->addWidget(m_satLabel);
    hsLayout->addWidget(m_saturationSlider);

    hsLayout->addStretch();
    
    paletteLayout->addLayout(paletteToolbar);
    paletteLayout->addLayout(paletteContentLayout);
    paletteLayout->addLayout(hsLayout);
    paletteLayout->addWidget(m_paletteInfoLabel);
    paletteGroup->setLayout(paletteLayout);
    
    connect(m_paletteWidget, &PaletteWidget::colorClicked, this, &SpriteViewerWindow::onPaletteColorClicked);
    connect(m_paletteWidget, &PaletteWidget::colorDoubleClicked, this, &SpriteViewerWindow::onPaletteColorDoubleClicked);
    connect(m_paletteBrightnessUp, &QPushButton::clicked, this, &SpriteViewerWindow::onPaletteBrightnessUp);
    connect(m_paletteBrightnessDown, &QPushButton::clicked, this, &SpriteViewerWindow::onPaletteBrightnessDown);
    connect(m_paletteGrayscale, &QPushButton::clicked, this, &SpriteViewerWindow::onPaletteGrayscale);
    connect(m_paletteSepia, &QPushButton::clicked, this, &SpriteViewerWindow::onPaletteSepia);
    connect(m_paletteContrast, &QPushButton::clicked, this, &SpriteViewerWindow::onPaletteContrast);
    connect(m_paletteInvert, &QPushButton::clicked, this, &SpriteViewerWindow::onPaletteInvert);
    connect(m_paletteUndo, &QPushButton::clicked, this, &SpriteViewerWindow::onPaletteUndo);
    connect(m_paletteRedo, &QPushButton::clicked, this, &SpriteViewerWindow::onPaletteRedo);
    connect(m_paletteReset, &QPushButton::clicked, this, &SpriteViewerWindow::onPaletteReset);
    connect(m_hueSlider, &QSlider::valueChanged, this, &SpriteViewerWindow::onHueChanged);
    connect(m_saturationSlider, &QSlider::valueChanged, this, &SpriteViewerWindow::onSaturationChanged);
    
    rightLayout->addWidget(fileGroup);
    rightLayout->addWidget(m_fileInfoLabel);
    rightLayout->addLayout(buttonLayout);
    rightLayout->addWidget(m_lithtechPanel);
    rightLayout->addWidget(frameListGroup, 1);
    rightLayout->addWidget(paletteGroup);
    
    topLayout->addWidget(previewGroup, 2);
    topLayout->addLayout(rightLayout, 1);
    
    // Bottom section: Animation controls and sprite properties
    auto* bottomLayout = new QHBoxLayout();
    
    // Left: Animation controls
    auto* animGroup = new QGroupBox(tr("Animation"));
    auto* animLayout = new QHBoxLayout();
    
    m_filePlayButton = new QPushButton(tr("Play"));
    m_filePlayButton->setEnabled(false);
    m_fileStopButton = new QPushButton(tr("Stop"));
    m_fileStopButton->setEnabled(false);
    
    animLayout->addWidget(m_filePlayButton);
    animLayout->addWidget(m_fileStopButton);
    animLayout->addSpacing(20);
    
    animLayout->addWidget(new QLabel(tr("Speed (ms):")));
    m_fileAnimationSpeedSpin = new QSpinBox();
    m_fileAnimationSpeedSpin->setRange(10, 1000);
    m_fileAnimationSpeedSpin->setValue(100);
    animLayout->addWidget(m_fileAnimationSpeedSpin);
    animLayout->addStretch();
    
    animGroup->setLayout(animLayout);
    
    // Middle: Sprite properties
    auto* propsGroup = new QGroupBox(tr("Sprite Properties"));
    auto* propsLayout = new QVBoxLayout();
    
    auto* typeLayout = new QHBoxLayout();
    typeLayout->addWidget(new QLabel(tr("Type:")));
    m_spriteTypeEditCombo = new QComboBox();
    m_spriteTypeEditCombo->addItem("VP_PARALLEL_UPRIGHT", 0);
    m_spriteTypeEditCombo->addItem("FACING_UPRIGHT", 1);
    m_spriteTypeEditCombo->addItem("VP_PARALLEL", 2);
    m_spriteTypeEditCombo->addItem("ORIENTED", 3);
    m_spriteTypeEditCombo->addItem("VP_PARALLEL_ORIENTED", 4);
    m_spriteTypeLabel = new QLabel();
    typeLayout->addWidget(m_spriteTypeEditCombo);
    typeLayout->addWidget(m_spriteTypeLabel);
    typeLayout->addStretch();
    
    auto* formatLayout = new QHBoxLayout();
    formatLayout->addWidget(new QLabel(tr("Format:")));
    m_textureFormatEditCombo = new QComboBox();
    m_textureFormatEditCombo->addItem(tr("Normal"), 0);
    m_textureFormatEditCombo->addItem(tr("Additive"), 1);
    m_textureFormatEditCombo->addItem(tr("IndexAlpha"), 2);
    m_textureFormatEditCombo->addItem(tr("AlphaTest"), 3);
    m_textureFormatLabel = new QLabel();
    formatLayout->addWidget(m_textureFormatEditCombo);
    formatLayout->addWidget(m_textureFormatLabel);
    formatLayout->addStretch();
    
    propsLayout->addLayout(typeLayout);
    propsLayout->addLayout(formatLayout);
    propsGroup->setLayout(propsLayout);
    
    // Right: Next/Prev sprite
    auto* navGroup = new QGroupBox(tr("Navigate Sprites"));
    auto* navLayout = new QHBoxLayout();
    
    m_prevSpriteButton = new QPushButton(tr("Previous"));
    m_nextSpriteButton = new QPushButton(tr("Next"));
    m_prevSpriteButton->setEnabled(false);
    m_nextSpriteButton->setEnabled(false);
    
    navLayout->addWidget(m_prevSpriteButton);
    navLayout->addWidget(m_nextSpriteButton);
    navGroup->setLayout(navLayout);
    navGroup->hide();
    
    bottomLayout->addWidget(animGroup);
    bottomLayout->addWidget(propsGroup);
    bottomLayout->addWidget(navGroup);
    
    mainLayout->addLayout(topLayout, 1);
    mainLayout->addLayout(bottomLayout);
    
    connect(m_fileOpenButton, &QPushButton::clicked, this, &SpriteViewerWindow::onOpenSprite);
    connect(m_extractFrameButton, &QPushButton::clicked, this, &SpriteViewerWindow::onExtractFrame);
    connect(m_extractAllFramesButton, &QPushButton::clicked, this, &SpriteViewerWindow::onExtractAllFrames);
    connect(m_fixSpriteButton, &QPushButton::clicked, this, &SpriteViewerWindow::onFixSprite);
    connect(m_fileFrameListWidget, &QListWidget::itemSelectionChanged, this, &SpriteViewerWindow::onFileFrameListSelectionChanged);
    connect(m_filePlayButton, &QPushButton::clicked, this, &SpriteViewerWindow::onFilePlayAnimation);
    connect(m_fileStopButton, &QPushButton::clicked, this, &SpriteViewerWindow::onFileStopAnimation);
    connect(m_prevSpriteButton, &QPushButton::clicked, this, &SpriteViewerWindow::onPrevSprite);
    connect(m_nextSpriteButton, &QPushButton::clicked, this, &SpriteViewerWindow::onNextSprite);
    connect(m_spriteTypeEditCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpriteViewerWindow::onSpriteTypeChanged);
    connect(m_textureFormatEditCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &SpriteViewerWindow::onTextureFormatChanged);
    
    return widget;
}

QWidget* SpriteViewerWindow::createCreateTabWidget()
{
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    
    // Frame selection
    auto* frameGroup = new QGroupBox(tr("Frames"));
    auto* frameLayout = new QVBoxLayout();
    
    m_createFrameList = new QListWidget();
    m_createFrameList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_createFrameList->setViewMode(QListView::IconMode);
    m_createFrameList->setIconSize(QSize(64, 64));
    m_createFrameList->setSpacing(5);
    m_createFrameList->setResizeMode(QListView::Adjust);
    m_createFrameList->setMovement(QListView::Static);
    m_browseFramesButton = new QPushButton(tr("Browse Frames..."));
    
    frameLayout->addWidget(m_createFrameList);
    frameLayout->addWidget(m_browseFramesButton);
    frameGroup->setLayout(frameLayout);
    
    // Sprite settings
    auto* settingsGroup = new QGroupBox(tr("Sprite Settings"));
    auto* settingsLayout = new QVBoxLayout();
    
    auto* typeLayout = new QHBoxLayout();
    typeLayout->addWidget(new QLabel(tr("Sprite Type")));
    m_spriteTypeCombo = new QComboBox();
    m_spriteTypeCombo->addItem(tr("Parallel"), 2);
    m_spriteTypeCombo->addItem(tr("Parallel Upright"), 0);
    m_spriteTypeCombo->addItem(tr("Oriented"), 3);
    m_spriteTypeCombo->addItem(tr("Parallel Oriented"), 2);
    m_spriteTypeCombo->addItem(tr("Facing Upright"), 1);
    typeLayout->addWidget(m_spriteTypeCombo);
    typeLayout->addStretch();
    
    auto* formatLayout = new QHBoxLayout();
    formatLayout->addWidget(new QLabel(tr("Texture Format")));
    m_textureFormatCombo = new QComboBox();
    m_textureFormatCombo->addItem(tr("Normal"), 0);
    m_textureFormatCombo->addItem(tr("Additive"), 1);
    m_textureFormatCombo->addItem(tr("IndexAlpha"), 2);
    m_textureFormatCombo->addItem(tr("AlphaTest"), 3);
    formatLayout->addWidget(m_textureFormatCombo);
    formatLayout->addStretch();
    
    // Transparency color picker
    auto* transColorLayout = new QHBoxLayout();
    transColorLayout->addWidget(new QLabel(tr("Transparent Color:")));
    
    auto* transColorInfo = new QLabel(tr("(i)"));
    transColorInfo->setStyleSheet("color: blue; font-weight: bold;");
    transColorInfo->setCursor(Qt::WhatsThisCursor);
    transColorInfo->setToolTip(tr(
        "Select the color that should become transparent.\n"
        "This color will be placed at palette index 255.\n"
        "Works only with AlphaTest format.\n"
        "Common: Blue (0,0,255) or Black (0,0,0)"));
    transColorLayout->addWidget(transColorInfo);
    
    m_createTransColor = QColor(0, 0, 0); // Default black
    
    m_createTransColorPreview = new QLabel();
    m_createTransColorPreview->setFixedSize(24, 24);
    m_createTransColorPreview->setStyleSheet("background-color: rgb(0,0,0); border: 1px solid gray;");
    transColorLayout->addWidget(m_createTransColorPreview);
    
    m_createTransColorButton = new QPushButton(tr("Choose..."));
    m_createTransColorButton->setFixedWidth(70);
    connect(m_createTransColorButton, &QPushButton::clicked, this, [this]() {
        QColor color = QColorDialog::getColor(m_createTransColor, this, tr("Select Transparent Color"));
        if (color.isValid()) {
            m_createTransColor = color;
            m_createTransColorPreview->setStyleSheet(
                QString("background-color: rgb(%1,%2,%3); border: 1px solid gray;")
                .arg(color.red()).arg(color.green()).arg(color.blue()));
        }
    });
    transColorLayout->addWidget(m_createTransColorButton);
    transColorLayout->addStretch();
    
    // Contrast slider
    auto* contrastLayout = new QHBoxLayout();
    contrastLayout->addWidget(new QLabel(tr("Contrast:")));
    m_createContrastSlider = new QSlider(Qt::Horizontal);
    m_createContrastSlider->setRange(10, 30);  // 1.0 to 3.0
    m_createContrastSlider->setValue(10);      // Default 1.0
    m_createContrastSlider->setTickPosition(QSlider::TicksBelow);
    m_createContrastSlider->setTickInterval(5);
    m_createContrastLabel = new QLabel("1.0x");
    m_createContrastLabel->setMinimumWidth(40);
    contrastLayout->addWidget(m_createContrastSlider, 1);
    contrastLayout->addWidget(m_createContrastLabel);
    
    connect(m_createContrastSlider, &QSlider::valueChanged, this, [this](int value) {
        m_createContrastLabel->setText(QString("%1x").arg(value / 10.0, 0, 'f', 1));
    });
    
    settingsLayout->addLayout(typeLayout);
    settingsLayout->addLayout(formatLayout);
    settingsLayout->addLayout(transColorLayout);
    settingsLayout->addLayout(contrastLayout);
    settingsGroup->setLayout(settingsLayout);
    
    // Output
    auto* outputGroup = new QGroupBox(tr("Output"));
    auto* outputLayout = new QHBoxLayout();
    
    m_createOutputPathEdit = new QLineEdit();
    m_browseOutputButton = new QPushButton(tr("Browse..."));
    
    outputLayout->addWidget(new QLabel(tr("Output Path:")));
    outputLayout->addWidget(m_createOutputPathEdit, 1);
    outputLayout->addWidget(m_browseOutputButton);
    outputGroup->setLayout(outputLayout);
    
    // Create button
    m_createSpriteButton = new QPushButton(tr("Create Sprite"));
    m_createSpriteButton->setEnabled(false);
    
    layout->addWidget(frameGroup, 1);
    layout->addWidget(settingsGroup);
    layout->addWidget(outputGroup);
    layout->addWidget(m_createSpriteButton);
    
    connect(m_browseFramesButton, &QPushButton::clicked, this, &SpriteViewerWindow::onBrowseFrames);
    connect(m_browseOutputButton, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getSaveFileName(this, tr("Save Sprite"), QString(), tr("Sprite files (*.spr)"));
        if (!path.isEmpty())
            m_createOutputPathEdit->setText(path);
    });
    connect(m_createSpriteButton, &QPushButton::clicked, this, &SpriteViewerWindow::onCreateSprite);
    connect(m_createFrameList, &QListWidget::itemSelectionChanged, this, [this]() {
        m_createSpriteButton->setEnabled(m_createFrameList->count() > 0 && !m_createOutputPathEdit->text().isEmpty());
    });
    connect(m_createOutputPathEdit, &QLineEdit::textChanged, this, [this]() {
        m_createSpriteButton->setEnabled(m_createFrameList->count() > 0 && !m_createOutputPathEdit->text().isEmpty());
    });
    
    return widget;
}

QWidget* SpriteViewerWindow::createFixTabWidget()
{
    auto* widget = new QWidget(this);
    widget->setAcceptDrops(true);
    auto* layout = new QVBoxLayout(widget);
    
    // Info label
    auto* infoLabel = new QLabel(tr("Drag & drop V3 sprite files here or use the Add button. All sprites will be converted to V2 format."));
    infoLabel->setWordWrap(true);
    
    // Sprite list
    auto* listGroup = new QGroupBox(tr("Sprites to Fix (Drag & Drop supported)"));
    auto* listLayout = new QVBoxLayout();
    
    m_fixSpriteList = new QListWidget();
    m_fixSpriteList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fixSpriteList->setAcceptDrops(true);
    m_fixSpriteList->setDragDropMode(QAbstractItemView::DropOnly);
    
    auto* listButtonLayout = new QHBoxLayout();
    m_addSpritesButton = new QPushButton(tr("Add Sprites..."));
    m_removeSpritesButton = new QPushButton(tr("Remove Selected"));
    listButtonLayout->addWidget(m_addSpritesButton);
    listButtonLayout->addWidget(m_removeSpritesButton);
    listButtonLayout->addStretch();
    
    listLayout->addWidget(m_fixSpriteList);
    listLayout->addLayout(listButtonLayout);
    listGroup->setLayout(listLayout);
    
    // Settings
    auto* settingsGroup = new QGroupBox(tr("Conversion Settings"));
    auto* settingsLayout = new QVBoxLayout();
    
    auto* typeLayout = new QHBoxLayout();
    typeLayout->addWidget(new QLabel(tr("Sprite Type")));
    m_fixSpriteTypeCombo = new QComboBox();
    m_fixSpriteTypeCombo->addItem(tr("Parallel"), 2);
    m_fixSpriteTypeCombo->addItem(tr("Parallel Upright"), 0);
    m_fixSpriteTypeCombo->addItem(tr("Oriented"), 3);
    m_fixSpriteTypeCombo->addItem(tr("Facing Upright"), 1);
    typeLayout->addWidget(m_fixSpriteTypeCombo);
    typeLayout->addStretch();
    
    auto* formatLayout = new QHBoxLayout();
    formatLayout->addWidget(new QLabel(tr("Texture Format")));
    m_fixTextureFormatCombo = new QComboBox();
    m_fixTextureFormatCombo->addItem(tr("Normal"), 0);
    m_fixTextureFormatCombo->addItem(tr("Additive"), 1);
    m_fixTextureFormatCombo->addItem(tr("IndexAlpha"), 2);
    m_fixTextureFormatCombo->addItem(tr("AlphaTest"), 3);
    formatLayout->addWidget(m_fixTextureFormatCombo);
    formatLayout->addStretch();
    
    auto* thresholdLayout = new QHBoxLayout();
    thresholdLayout->addWidget(new QLabel(tr("Alpha Threshold:")));
    
    // Info icon with tooltip
    auto* thresholdInfoLabel = new QLabel(tr("(i)"));
    thresholdInfoLabel->setStyleSheet("color: blue; font-weight: bold;");
    thresholdInfoLabel->setCursor(Qt::WhatsThisCursor);
    thresholdInfoLabel->setToolTip(tr("Controls how semi-transparent pixels are handled.\nLow value (0-64): More pixels visible, rougher edges.\nHigh value (128-255): Fewer pixels, cleaner edges.\nDefault: 32"));
    thresholdLayout->addWidget(thresholdInfoLabel);
    
    m_alphaThresholdSlider = new QSlider(Qt::Horizontal);
    m_alphaThresholdSlider->setRange(0, 255);
    m_alphaThresholdSlider->setValue(32);
    m_alphaThresholdLabel = new QLabel("32");
    m_alphaThresholdLabel->setMinimumWidth(30);
    thresholdLayout->addWidget(m_alphaThresholdSlider);
    thresholdLayout->addWidget(m_alphaThresholdLabel);
    
    connect(m_alphaThresholdSlider, &QSlider::valueChanged, this, [this](int value) {
        m_alphaThresholdLabel->setText(QString::number(value));
    });
    
    // Transparency color picker
    auto* transColorLayout = new QHBoxLayout();
    transColorLayout->addWidget(new QLabel(tr("Transparent Color:")));
    
    auto* transColorInfo = new QLabel(tr("(i)"));
    transColorInfo->setStyleSheet("color: blue; font-weight: bold;");
    transColorInfo->setCursor(Qt::WhatsThisCursor);
    transColorInfo->setToolTip(tr("Select the color that should become transparent.\nThis color will be placed at palette index 255.\nWorks only with AlphaTest format.\nCommon: Blue (0,0,255) or Black (0,0,0)"));
    transColorLayout->addWidget(transColorInfo);
    
    m_fixTransColor = QColor(0, 0, 0); // Default black
    
    m_fixTransColorPreview = new QLabel();
    m_fixTransColorPreview->setFixedSize(24, 24);
    m_fixTransColorPreview->setStyleSheet("background-color: rgb(0,0,0); border: 1px solid gray;");
    transColorLayout->addWidget(m_fixTransColorPreview);
    
    m_fixTransColorButton = new QPushButton(tr("Choose..."));
    m_fixTransColorButton->setFixedWidth(70);
    connect(m_fixTransColorButton, &QPushButton::clicked, this, [this]() {
        QColor color = QColorDialog::getColor(m_fixTransColor, this, tr("Select Transparent Color"));
        if (color.isValid()) {
            m_fixTransColor = color;
            m_fixTransColorPreview->setStyleSheet(
                QString("background-color: rgb(%1,%2,%3); border: 1px solid gray;")
                .arg(color.red()).arg(color.green()).arg(color.blue()));
        }
    });
    transColorLayout->addWidget(m_fixTransColorButton);
    transColorLayout->addStretch();
    
    // Contrast slider
    auto* contrastLayout = new QHBoxLayout();
    contrastLayout->addWidget(new QLabel(tr("Contrast")));
    m_fixContrastSlider = new QSlider(Qt::Horizontal);
    m_fixContrastSlider->setRange(10, 30);  // 1.0 to 3.0
    m_fixContrastSlider->setValue(1.0);      // Default 1.0
    m_fixContrastSlider->setTickPosition(QSlider::TicksBelow);
    m_fixContrastSlider->setTickInterval(5);
    m_fixContrastLabel = new QLabel("1.0x");
    m_fixContrastLabel->setMinimumWidth(40);
    contrastLayout->addWidget(m_fixContrastSlider, 1);
    contrastLayout->addWidget(m_fixContrastLabel);
    
    connect(m_fixContrastSlider, &QSlider::valueChanged, this, [this](int value) {
        m_fixContrastLabel->setText(QString("%1x").arg(value / 10.0, 0, 'f', 1));
    });
    
    settingsLayout->addLayout(typeLayout);
    settingsLayout->addLayout(formatLayout);
    settingsLayout->addLayout(thresholdLayout);
    settingsLayout->addLayout(transColorLayout);
    settingsLayout->addLayout(contrastLayout);
    settingsGroup->setLayout(settingsLayout);
    
    // Output directory
    auto* outputGroup = new QGroupBox(tr("Output Directory"));
    auto* outputLayout = new QHBoxLayout();
    
    m_fixOutputDirEdit = new QLineEdit();
    m_fixOutputDirEdit->setPlaceholderText(tr("Leave empty to save alongside original files with _v2 suffix"));
    m_browseFixOutputButton = new QPushButton(tr("Browse..."));
    
    outputLayout->addWidget(m_fixOutputDirEdit, 1);
    outputLayout->addWidget(m_browseFixOutputButton);
    outputGroup->setLayout(outputLayout);
    
    connect(m_browseFixOutputButton, &QPushButton::clicked, this, [this]() {
        QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Directory"));
        if (!dir.isEmpty())
            m_fixOutputDirEdit->setText(dir);
    });
    
    // Fix button and progress
    auto* fixLayout = new QHBoxLayout();
    m_fixAllButton = new QPushButton(tr("Fix All Sprites"));
    m_fixAllButton->setEnabled(false);
    m_fixProgressBar = new QProgressBar();
    m_fixProgressBar->setVisible(false);
    
    fixLayout->addWidget(m_fixAllButton);
    fixLayout->addWidget(m_fixProgressBar, 1);
    
    layout->addWidget(infoLabel);
    layout->addWidget(listGroup, 1);
    layout->addWidget(settingsGroup);
    layout->addWidget(outputGroup);
    layout->addLayout(fixLayout);
    
    connect(m_addSpritesButton, &QPushButton::clicked, this, &SpriteViewerWindow::onAddSpritesToFix);
    connect(m_removeSpritesButton, &QPushButton::clicked, this, &SpriteViewerWindow::onRemoveSelectedSprites);
    connect(m_fixAllButton, &QPushButton::clicked, this, &SpriteViewerWindow::onFixAllSprites);
    
    connect(m_fixSpriteList, &QListWidget::itemSelectionChanged, this, [this]() {
        m_fixAllButton->setEnabled(m_fixSpriteList->count() > 0);
    });
    
    return widget;
}

void SpriteViewerWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls()) {
        bool hasSpr = false;
        for (const QUrl& url : event->mimeData()->urls()) {
            if (url.toLocalFile().toLower().endsWith(".spr")) {
                hasSpr = true;
                break;
            }
        }
        if (hasSpr) {
            event->acceptProposedAction();
        }
    }
}

void SpriteViewerWindow::dropEvent(QDropEvent* event)
{
    QStringList sprFiles;
    for (const QUrl& url : event->mimeData()->urls()) {
        QString filePath = url.toLocalFile();
        if (filePath.toLower().endsWith(".spr")) {
            sprFiles.append(filePath);
        }
    }
    
    if (!sprFiles.isEmpty()) {
        addSpritesToFixList(sprFiles);
        event->acceptProposedAction();
    }
}

void SpriteViewerWindow::closeEvent(QCloseEvent* event)
{
    // Save current browsed path
    QSettings settings("Vortigaunt", "SpriteViewer");
    QString currentPath = m_fileBrowserModel->filePath(m_fileBrowser->rootIndex());
    if (!currentPath.isEmpty()) {
        settings.setValue("lastBrowsedPath", currentPath);
    }
    QDialog::closeEvent(event);
}

void SpriteViewerWindow::onZoomChanged(double factor)
{
    m_zoomLabel->setText(QString(tr("Zoom: %1%")).arg(static_cast<int>(factor * 100)));
}

void SpriteViewerWindow::onBackgroundChanged(int bgType)
{
    static const char* bgNames[] = {"Checkered", "Black", "White", "Gray"};
    m_bgLabel->setText(QString(tr("Background: %1")).arg(bgNames[bgType]));
}

void SpriteViewerWindow::onCoordinatesChanged(int x, int y, bool valid)
{
    if (valid) {
        m_coordLabel->setText(QString("X: %1  Y: %2").arg(x).arg(y));
    } else {
        m_coordLabel->setText(tr("X: - Y: -"));
    }
}

void SpriteViewerWindow::onSelectionChanged(int x, int y, int width, int height, bool valid)
{
    if (valid && width > 0 && height > 0) {
        // Format: X, Y, Width, Height (640hud format)
        m_selectionLabel->setText(QString("Sel: %1, %2, %3, %4").arg(x).arg(y).arg(width).arg(height));
    } else {
        m_selectionLabel->setText(tr("Selection: -"));
    }
}

void SpriteViewerWindow::onPaletteColorClicked(int index, QRgb color)
{
    m_paletteInfoLabel->setText(QString(tr("Index %1: RGB(%2, %3, %4)"))
        .arg(index)
        .arg(qRed(color))
        .arg(qGreen(color))
        .arg(qBlue(color)));
}

void SpriteViewerWindow::onAddSpritesToFix()
{
    QStringList files = QFileDialog::getOpenFileNames(this,
        tr("Select Sprites to Fix"),
        QString(),
        tr("Sprite files (*.spr)"));
    
    if (!files.isEmpty()) {
        addSpritesToFixList(files);
    }
}

void SpriteViewerWindow::onRemoveSelectedSprites()
{
    QList<QListWidgetItem*> selected = m_fixSpriteList->selectedItems();
    for (QListWidgetItem* item : selected) {
        delete item;
    }
    m_fixAllButton->setEnabled(m_fixSpriteList->count() > 0);
}

void SpriteViewerWindow::onFixAllSprites()
{
    if (m_fixSpriteList->count() == 0)
        return;
    
    int32_t spriteType = m_fixSpriteTypeCombo->currentData().toInt();
    int32_t textureFormat = m_fixTextureFormatCombo->currentData().toInt();
    QString outputDir = m_fixOutputDirEdit->text().trimmed();
    
    // Use selected transparency color
    std::vector<uint8_t> transColor = {
        static_cast<uint8_t>(m_fixTransColor.red()),
        static_cast<uint8_t>(m_fixTransColor.green()),
        static_cast<uint8_t>(m_fixTransColor.blue())
    };
    
    // Auto-switch to AlphaTest format only if non-black transparency color is selected
    // Black transparency works with Additive (black = transparent in additive blending)
    bool isNonBlackTrans = (transColor[0] != 0 || transColor[1] != 0 || transColor[2] != 0);
    if (isNonBlackTrans && textureFormat != 3) {
        QMessageBox::information(this, tr("Format Changed"),
            tr("Texture format has been changed to AlphaTest.\nNon-black transparent color only works with AlphaTest format."));
        textureFormat = 3;
        m_fixTextureFormatCombo->setCurrentIndex(3);
    }
    
    m_fixProgressBar->setVisible(true);
    m_fixProgressBar->setRange(0, m_fixSpriteList->count());
    m_fixProgressBar->setValue(0);
    m_fixAllButton->setEnabled(false);
    
    int successCount = 0;
    int failCount = 0;
    int skippedCount = 0;
    
    for (int i = 0; i < m_fixSpriteList->count(); i++) {
        QListWidgetItem* item = m_fixSpriteList->item(i);
        QString inputPath = item->data(Qt::UserRole).toString();
        
        // Skip V2 sprites (already marked during add)
        if (item->data(Qt::UserRole + 1).toString() == "v2") {
            skippedCount++;
            m_fixProgressBar->setValue(i + 1);
            QApplication::processEvents();
            continue;
        }
        
        // Determine output path
        QString outputPath;
        if (outputDir.isEmpty()) {
            // Save alongside original with _v2 suffix
            QFileInfo fi(inputPath);
            outputPath = fi.absolutePath() + "/" + fi.baseName() + "_v2.spr";
        } else {
            QFileInfo fi(inputPath);
            outputPath = outputDir + "/" + fi.fileName();
        }
        
        SpriteLoader loader;
        float contrast = m_fixContrastSlider->value() / 10.0f;
        if (loader.convertV3ToV2(inputPath.toStdString(), outputPath.toStdString(), 
                                  spriteType, textureFormat, transColor, contrast)) {
            successCount++;
            item->setForeground(Qt::darkGreen);
        } else {
            failCount++;
            item->setForeground(Qt::red);
        }
        
        m_fixProgressBar->setValue(i + 1);
        QApplication::processEvents();
    }
    
    m_fixProgressBar->setVisible(false);
    m_fixAllButton->setEnabled(true);
    
    QMessageBox::information(this, tr("Sprite Fix Complete"),
        QString(tr("Successfully converted: %1\nFailed: %2\nSkipped (V2): %3")).arg(successCount).arg(failCount).arg(skippedCount));
}

void SpriteViewerWindow::addSpritesToFixList(const QStringList& files)
{
    for (const QString& file : files) {
        // Check if already in list
        bool found = false;
        for (int i = 0; i < m_fixSpriteList->count(); i++) {
            if (m_fixSpriteList->item(i)->data(Qt::UserRole).toString() == file) {
                found = true;
                break;
            }
        }
        
        if (!found) {
            QFileInfo fi(file);
            
            // Load sprite to check version and get info
            SpriteLoader tempLoader;
            QString displayText;
            bool isV2 = false;
            
            if (tempLoader.loadFile(file.toStdString())) {
                const auto& header = tempLoader.getHeader();
                
                if (header.version == SpriteVersion::GOLDSRC) {
                    // V2 sprite - cannot be fixed, show warning
                    displayText = QString("%1 (Sprite is V2. Skipping)").arg(fi.fileName());
                    isV2 = true;
                } else {
                    // V3 sprite - show size and origin info
                    displayText = QString("%1 | Size: %2x%3 | Origin: -128x128")
                        .arg(fi.fileName())
                        .arg(header.max_width)
                        .arg(header.max_height);
                }
            } else {
                // Couldn't load - show just filename
                displayText = fi.fileName();
            }
            
            QListWidgetItem* item = new QListWidgetItem(displayText);
            item->setData(Qt::UserRole, file);
            item->setToolTip(file);
            
            if (isV2) {
                item->setForeground(Qt::red);
                item->setData(Qt::UserRole + 1, "v2"); // Mark as V2
            }
            
            m_fixSpriteList->addItem(item);
        }
    }
    
    m_fixAllButton->setEnabled(m_fixSpriteList->count() > 0);
}

void SpriteViewerWindow::updatePaletteDisplay()
{
    const auto& header = m_spriteLoader.getHeader();
    const auto& palette = m_spriteLoader.getPalette();
    
    if (header.version != SpriteVersion::GOLDSRC || palette.empty()) {
        m_paletteWidget->clearPalette();
        if (header.version == SpriteVersion::LITHTECH)
        {
            m_paletteInfoLabel->setText(tr("No Palette Available for Lithtech Engine Sprite"));
        }
        else
        {
            m_paletteInfoLabel->setText(tr("No palette available for V3 Sprites"));
        }
        return;
    }
    
    // Convert raw palette bytes to QRgb colors
    QVector<QRgb> colors;
    for (size_t i = 0; i < palette.size() / 3 && colors.size() < 256; i++) {
        uint8_t r = palette[i * 3];
        uint8_t g = palette[i * 3 + 1];
        uint8_t b = palette[i * 3 + 2];
        colors.append(qRgb(r, g, b));
    }
    
    m_paletteWidget->setPalette(colors);
    m_paletteInfoLabel->setText(QString(tr("Click a color to see RGB values (%1 colors)")).arg(colors.size()));
}

void SpriteViewerWindow::onFixSprite()
{
    const auto& header = m_spriteLoader.getHeader();
    if (header.version != SpriteVersion::CSO) {
        QMessageBox::information(this, tr("Info"), tr(" The fix is only available for V3 sprites."));
        return;
    }
    
    // Open fix dialog with current values
    SpriteFixDialog dialog(header.type, header.texture_format, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    
    int32_t spriteType = dialog.getSelectedSpriteType();
    int32_t textureFormat = dialog.getSelectedTextureFormat();
    
    QString outputPath = QFileDialog::getSaveFileName(this, tr("Save Fixed Sprite"), QString(), tr("Sprite files (*.spr)"));
    if (outputPath.isEmpty())
        return;
    
    // Use selected transparency color from Fix tab
    std::vector<uint8_t> transColor = {
        static_cast<uint8_t>(m_fixTransColor.red()),
        static_cast<uint8_t>(m_fixTransColor.green()),
        static_cast<uint8_t>(m_fixTransColor.blue())
    };
    
    // Auto-switch to AlphaTest only if non-black transparency color
    bool isNonBlackTrans = (transColor[0] != 0 || transColor[1] != 0 || transColor[2] != 0);
    if (isNonBlackTrans && textureFormat != 3) {
        textureFormat = 3;
    }
    
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    float contrast = m_fixContrastSlider->value() / 10.0f;
    
    if (!m_spriteLoader.convertV3ToV2(m_currentSpritePath.toStdString(), 
                                     outputPath.toStdString(), 
                                     spriteType, 
                                     textureFormat, 
                                     transColor,
                                     contrast)) {
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, tr("Error"), tr("Failed to convert sprite."));
        return;
    }
    
    QApplication::restoreOverrideCursor();
    QMessageBox::information(this, tr("Success"), tr("Sprite converted successfully."));
}


void SpriteViewerWindow::onOpenSprite()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Open Sprite..."), QString(), tr("Sprite files (*.spr)"));
    if (!path.isEmpty())
        loadSprite(path);
}

void SpriteViewerWindow::onQuickSave()
{
    if (m_currentSpritePath.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("No sprite loaded to save."));
        return;
    }
    
    // Save directly to the current file path
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    if (!m_spriteLoader.saveFile(m_currentSpritePath.toStdString())) {
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, tr("Error"), tr("Failed to save sprite."));
        return;
    }
    
    QApplication::restoreOverrideCursor();
    setModified(false);
}

void SpriteViewerWindow::onSaveAsSprite()
{
    if (m_currentSpritePath.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("No sprite loaded to save."));
        return;
    }
    
    QString outputPath = QFileDialog::getSaveFileName(this, tr("Save Sprite As"), 
        m_currentSpritePath, tr("Sprite files (*.spr)"));
    if (outputPath.isEmpty())
        return;
    
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    if (!m_spriteLoader.saveFile(outputPath.toStdString())) {
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, tr("Error"), tr("Failed to save sprite."));
        return;
    }
    
    QApplication::restoreOverrideCursor();
    m_currentSpritePath = outputPath;
    m_fileSpritePathEdit->setText(outputPath);
    setModified(false);
    QMessageBox::information(this, tr("Success"), tr("Sprite saved successfully."));
}

void SpriteViewerWindow::loadSprite(const QString& filePath)
{
    bool signalsBlocked = false;
    if (m_fileBrowser && m_fileBrowser->selectionModel()) {
        signalsBlocked = m_fileBrowser->selectionModel()->blockSignals(true);
    }

    m_currentSpritePath = filePath;
    m_fileSpritePathEdit->setText(filePath);
    setModified(false);
    
    // Clear undo/redo stacks for new sprite
    while (!m_undoStack.empty()) m_undoStack.pop();
    while (!m_redoStack.empty()) m_redoStack.pop();
    m_originalPalette.clear();
    
    if (!m_spriteLoader.loadFile(filePath.toStdString(), true)) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to load sprite file."));
        if (m_fileBrowser && m_fileBrowser->selectionModel()) {
            m_fileBrowser->selectionModel()->blockSignals(signalsBlocked);
        }
        return;
    }
    
    const auto& header = m_spriteLoader.getHeader();
    bool isVersion3 = (header.version == SpriteVersion::CSO);
    bool isVersion2 = (header.version == SpriteVersion::GOLDSRC);
    bool isLithtech = (header.version == SpriteVersion::LITHTECH);
    
    // Store original state for reset, H/S calculations, and modified detection
    if (isVersion2) {
        m_originalPalette = m_spriteLoader.getPalette();
    }
    m_originalType = header.type;
    m_originalTextureFormat = header.texture_format;
    
    // Navigate file browser to sprite's directory and select the file
    QFileInfo fi(filePath);
    navigateToPath(fi.absolutePath());
    
    // Select the current file in the file browser
    QModelIndex fileIndex = m_fileBrowserModel->index(filePath);
    if (fileIndex.isValid()) {
        m_fileBrowser->setCurrentIndex(fileIndex);
        m_fileBrowser->scrollTo(fileIndex, QAbstractItemView::PositionAtCenter);
    }
    
    updateFileInfo();
    updateFileFrameList();
    updatePaletteDisplay();
    updateUndoRedoButtons();
    
    m_extractFrameButton->setEnabled(!isLithtech);
    m_extractAllFramesButton->setEnabled(!isLithtech);
    m_saveButton->setEnabled(isVersion2);
    m_saveAsButton->setEnabled(isVersion2);
    
    // Show/hide Lithtech panel
    if (m_lithtechPanel) {
        m_lithtechPanel->setVisible(isLithtech);
        if (isLithtech) {
            // Auto-set texture dir from sprite's parent
            QFileInfo fi2(filePath);
            if (m_lithtechTextureDirEdit && m_lithtechTextureDirEdit->text().isEmpty()) {
                m_lithtechTextureDirEdit->setText(fi2.absolutePath());
            }
            if (m_lithtechExportGoldSrcButton) m_lithtechExportGoldSrcButton->setEnabled(false);
            if (m_lithtechExportFramesButton) m_lithtechExportFramesButton->setEnabled(false);
        }
    }
    
    // Reset H/S/C sliders with blocked signals
    if (m_hueSlider) {
        m_hueSlider->blockSignals(true);
        m_hueSlider->setValue(0);
        m_hueSlider->blockSignals(false);
    }
    if (m_saturationSlider) {
        m_saturationSlider->blockSignals(true);
        m_saturationSlider->setValue(0);
        m_saturationSlider->blockSignals(false);
    }

    // Update labels
    if (m_hueLabel) m_hueLabel->setText("H: 0");
    if (m_satLabel) m_satLabel->setText("S: 0");
    if (m_paletteReset) m_paletteReset->setEnabled(isVersion2);
    
    // Update sprite type/format combo boxes and labels
    if (m_spriteTypeEditCombo) {
        m_spriteTypeEditCombo->blockSignals(true);
        m_spriteTypeEditCombo->setCurrentIndex(header.type);
        m_spriteTypeEditCombo->blockSignals(false);
    }
    if (m_textureFormatEditCombo) {
        m_textureFormatEditCombo->blockSignals(true);
        m_textureFormatEditCombo->setCurrentIndex(header.texture_format);
        m_textureFormatEditCombo->blockSignals(false);
    }
    // Show current values in labels
    if (m_spriteTypeLabel) {
        m_spriteTypeLabel->setText(QString(tr("(Current: %1)")).arg(getSpriteTypeName(header.type)));
    }
    if (m_textureFormatLabel) {
        m_textureFormatLabel->setText(QString(tr("(Current: %1)")).arg(getTextureFormatName(header.texture_format)));
    }
    
    // Enable navigation buttons
    m_prevSpriteButton->setEnabled(true);
    m_nextSpriteButton->setEnabled(true);
    
    // Show Fix Sprite button only for V3 sprites
    m_fixSpriteButton->setVisible(isVersion3);
    m_fixSpriteButton->setEnabled(isVersion3);
    
    // Hide GoldSrc-specific controls for Lithtech
    if (m_spriteTypeEditCombo) m_spriteTypeEditCombo->setVisible(!isLithtech);
    if (m_textureFormatEditCombo) m_textureFormatEditCombo->setVisible(!isLithtech);
    
    // Auto-load Lithtech textures
    if (isLithtech && m_spriteLoader.getFrameCount() > 0) {
        QApplication::setOverrideCursor(Qt::WaitCursor);
        QString baseDir = QFileInfo(filePath).absolutePath();
        bool loaded = m_spriteLoader.loadLithtechTextures(baseDir.toStdString());
        QApplication::restoreOverrideCursor();
        
        if (loaded) {
            if (m_lithtechExportGoldSrcButton) m_lithtechExportGoldSrcButton->setEnabled(true);
            if (m_lithtechExportFramesButton) m_lithtechExportFramesButton->setEnabled(true);
            m_extractFrameButton->setEnabled(true);
            m_extractAllFramesButton->setEnabled(true);
        }
        updateFileInfo();
    }
    
    if (m_spriteLoader.getFrameCount() > 0) {
        m_fileFrameListWidget->setCurrentRow(0);
        m_filePlayButton->setEnabled(true);
        m_fileStopButton->setEnabled(true);
        updateFileFrameDisplay();
    }

    if (m_fileBrowser && m_fileBrowser->selectionModel()) {
        m_fileBrowser->selectionModel()->blockSignals(signalsBlocked);
    }
}

void SpriteViewerWindow::keyPressEvent(QKeyEvent* event)
{
    if (event->key() == Qt::Key_S && event->modifiers() == Qt::ControlModifier) {
        onQuickSave();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Space) {
        // Toggle play/stop with Space
        if (m_spriteLoader.getFrameCount() > 1) {
            if (m_fileIsPlaying) {
                onFileStopAnimation();
            } else {
                onFilePlayAnimation();
            }
        }
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Up) {
        onPrevSprite();
        event->accept();
        return;
    }
    if (event->key() == Qt::Key_Down) {
        onNextSprite();
        event->accept();
        return;
    }
    QDialog::keyPressEvent(event);
}

bool SpriteViewerWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_fileFrameListWidget && event->type() == QEvent::KeyPress) {
        auto* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->key() == Qt::Key_Up) {
            onPrevSprite();
            return true; // Filter out/consume the key event so it doesn't move list selection
        }
        if (keyEvent->key() == Qt::Key_Down) {
            onNextSprite();
            return true; // Filter out/consume the key event so it doesn't move list selection
        }
    }
    return QDialog::eventFilter(watched, event);
}

void SpriteViewerWindow::setModified(bool modified)
{
    m_modified = modified;
    updateWindowTitle();
}

void SpriteViewerWindow::updateWindowTitle()
{
    QString title = tr("Sprite Viewer");
    if (!m_currentSpritePath.isEmpty()) {
        QFileInfo fi(m_currentSpritePath);
        title = fi.fileName() + " - " + tr("Sprite Viewer");
    }
    if (m_modified) {
        title = "*" + title;
    }
    setWindowTitle(title);
}

void SpriteViewerWindow::updateFileInfo()
{
    const auto& header = m_spriteLoader.getHeader();
    
    if (header.version == SpriteVersion::LITHTECH) {
        QString info = QString(tr("Format: Lithtech | Frames: %1 | Frame Rate: %2 ms"))
            .arg(m_spriteLoader.getFrameCount())
            .arg(m_spriteLoader.getLithtechFrameRate());
        m_fileInfoLabel->setText(info);
    } else {
        QString info = QString(tr("Version: %1 | Frames: %2 | Size: %3x%4"))
            .arg(static_cast<int>(header.version))
            .arg(m_spriteLoader.getFrameCount())
            .arg(header.max_width)
            .arg(header.max_height);
        m_fileInfoLabel->setText(info);
    }
}

void SpriteViewerWindow::onExtractFrame()
{
    if (m_spriteLoader.getFrameCount() == 0) {
        QMessageBox::warning(this, tr("Error"), tr("No frames loaded."));
        return;
    }
    
    // Show frame selection dialog
    bool ok;
    int frameIndex = QInputDialog::getInt(this, tr("Extract Frame..."), 
        QString(tr("Frame index (0-%1):")).arg(m_spriteLoader.getFrameCount() - 1),
        0, 0, static_cast<int>(m_spriteLoader.getFrameCount() - 1), 1, &ok);
    
    if (!ok)
        return;
    
    QString outputPath = QFileDialog::getSaveFileName(this, tr("Save Frame"), 
        QString("frame_%1.png").arg(frameIndex), tr("PNG files (*.png);;Bitmap files (*.bmp)"));
    
    if (outputPath.isEmpty())
        return;
    
#ifdef QT_WIDGETS_LIB
    const auto& frames = m_spriteLoader.getFrames();
    if (frameIndex < 0 || frameIndex >= static_cast<int>(frames.size()))
        return;
    
    const auto& frame = frames[frameIndex];
    if (frame.image.isNull())
        return;
    
    if (!frame.image.save(outputPath)) {
        QMessageBox::warning(this, tr("Error"), tr("Failed to save frame."));
        return;
    }
    
    QMessageBox::information(this, tr("Success"), tr("Frame extracted successfully."));
#endif
}

void SpriteViewerWindow::onExtractAllFrames()
{
    if (m_spriteLoader.getFrameCount() == 0) {
        QMessageBox::warning(this, tr("Error"), tr("No frames loaded."));
        return;
    }

    // --- Format selection dialog ---
    QDialog formatDialog(this);
    formatDialog.setWindowTitle(tr("Export Format"));
    formatDialog.setFixedSize(280, 120);

    auto* dlgLayout = new QVBoxLayout(&formatDialog);

    auto* formatLayout = new QHBoxLayout();
    formatLayout->addWidget(new QLabel(tr("Format:")));
    auto* formatCombo = new QComboBox();
    formatCombo->addItem(tr("BMP (8-bit Indexed, GoldSrc)"), QStringLiteral("bmp"));
    formatCombo->addItem(tr("PNG"), QStringLiteral("png"));
    formatCombo->addItem(tr("JPG"), QStringLiteral("jpg"));
    formatLayout->addWidget(formatCombo, 1);
    dlgLayout->addLayout(formatLayout);

    auto* btnLayout = new QHBoxLayout();
    auto* okBtn = new QPushButton(tr("OK"));
    auto* cancelBtn = new QPushButton(tr("Cancel"));
    btnLayout->addStretch();
    btnLayout->addWidget(okBtn);
    btnLayout->addWidget(cancelBtn);
    dlgLayout->addLayout(btnLayout);

    connect(okBtn, &QPushButton::clicked, &formatDialog, &QDialog::accept);
    connect(cancelBtn, &QPushButton::clicked, &formatDialog, &QDialog::reject);

    if (formatDialog.exec() != QDialog::Accepted)
        return;

    QString chosenFormat = formatCombo->currentData().toString(); // "bmp", "png", or "jpg"

    // --- Select output directory ---
    QString outputDir = QFileDialog::getExistingDirectory(this, tr("Select Output Directory"));
    if (outputDir.isEmpty())
        return;

    const auto& frames = m_spriteLoader.getFrames();
    int totalFrames = static_cast<int>(frames.size());

    QProgressDialog progress(tr("Extracting frames..."), tr("Cancel"), 0, totalFrames, this);
    progress.setWindowModality(Qt::WindowModal);
    progress.setMinimumDuration(0);

    int successCount = 0;

    for (int i = 0; i < totalFrames; i++) {
        progress.setValue(i);
        if (progress.wasCanceled())
            break;

        const auto& frame = frames[i];
        if (frame.image.isNull())
            continue;

        QString outputPath = outputDir + QString("/frame_%1.%2").arg(i, 4, 10, QChar('0')).arg(chosenFormat);

        bool saved = false;
        if (chosenFormat == "bmp") {
            QImage argb = frame.image.convertToFormat(QImage::Format_ARGB32);
            QImage rgb(argb.size(), QImage::Format_ARGB32);
            rgb.fill(QColor(0, 0, 0));
            QPainter painter(&rgb);
            painter.drawImage(0, 0, argb);
            painter.end();
            saved = BMP::saveAsIndexed8(outputPath.toStdString(),
                                        rgb.width(), rgb.height(),
                                        reinterpret_cast<const uint32_t*>(rgb.constBits()));
        } else {
            saved = frame.image.save(outputPath);
        }

        if (saved)
            successCount++;
    }

    progress.setValue(totalFrames);

    QMessageBox::information(this, tr("Success"),
        QString(tr("Extracted %1 of %2 frames as %3.")).arg(successCount).arg(totalFrames).arg(chosenFormat.toUpper()));
}

void SpriteViewerWindow::onBrowseFrames()
{
    QString startDir = QDir::homePath();
    if (m_createFrameList->count() > 0) {
        QListWidgetItem* firstItem = m_createFrameList->item(0);
        if (firstItem) {
            QString firstPath = firstItem->data(Qt::UserRole).toString();
            if (!firstPath.isEmpty()) {
                startDir = QFileInfo(firstPath).absolutePath();
            }
        }
    }
    
    QStringList files = QFileDialog::getOpenFileNames(this, tr("Select Frame Images"), startDir, 
        tr("Image files (*.bmp *.png *.jpg);;All files (*.*)"));
    
    if (files.isEmpty())
        return;
    
    // Clear existing list
    m_createFrameList->clear();
    
    // Sort files by name to ensure correct order (natural sort)
    QCollator collator;
    collator.setNumericMode(true);
    std::sort(files.begin(), files.end(), [&collator](const QString& a, const QString& b) {
        QFileInfo infoA(a);
        QFileInfo infoB(b);
        return collator.compare(infoA.fileName(), infoB.fileName()) < 0;
    });
    
    for (const QString& file : files) {
        QFileInfo fileInfo(file);
        
        // Load image and create thumbnail
        QImage image(file);
        QIcon icon;
        if (!image.isNull()) {
            QPixmap pixmap = QPixmap::fromImage(image.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            icon = QIcon(pixmap);
        }
        
        QListWidgetItem* item = new QListWidgetItem(icon, fileInfo.fileName());
        item->setData(Qt::UserRole, file);
        item->setToolTip(file);
        m_createFrameList->addItem(item);
    }
    
    m_createSpriteButton->setEnabled(m_createFrameList->count() > 0);
}

void SpriteViewerWindow::onCreateSprite()
{
    if (m_createFrameList->count() == 0) {
        QMessageBox::warning(this, tr("Error"), tr("No frames selected."));
        return;
    }
    
    QString outputPath = m_createOutputPathEdit->text();
    if (outputPath.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Please specify output path."));
        return;
    }
    
    // Collect frame paths
    std::vector<std::string> framePaths;
    for (int i = 0; i < m_createFrameList->count(); i++) {
        QListWidgetItem* item = m_createFrameList->item(i);
        QString filePath = item->data(Qt::UserRole).toString();
        if (!filePath.isEmpty()) {
            framePaths.push_back(filePath.toStdString());
        }
    }
    
    if (framePaths.empty()) {
        QMessageBox::warning(this, tr("Error"), tr("No valid frame paths found."));
        return;
    }
    
    // Get sprite settings
    int32_t spriteType = m_spriteTypeCombo->currentData().toInt();
    int32_t textureFormat = m_textureFormatCombo->currentData().toInt();
    
    // Use selected transparency color
    std::vector<uint8_t> transColor = {
        static_cast<uint8_t>(m_createTransColor.red()),
        static_cast<uint8_t>(m_createTransColor.green()),
        static_cast<uint8_t>(m_createTransColor.blue())
    };
    
    // Auto-switch to AlphaTest format only if non-black transparency color is selected
    // Black transparency works with Additive (black = transparent in additive blending)
    bool isNonBlackTrans = (transColor[0] != 0 || transColor[1] != 0 || transColor[2] != 0);
    if (isNonBlackTrans && textureFormat != 3) {
        QMessageBox::information(this, tr("Format Changed"),
            tr("Texture format has been changed to AlphaTest.\n"
               "Non-black transparent color only works with AlphaTest format."));
        textureFormat = 3;
        m_textureFormatCombo->setCurrentIndex(3);
    }
    
    QApplication::setOverrideCursor(Qt::WaitCursor);
    
    // Get contrast value from slider (slider value / 10 = actual contrast)
    float contrast = m_createContrastSlider->value() / 10.0f;
    
    if (!m_spriteLoader.createSpriteV2(outputPath.toStdString(), framePaths, spriteType, textureFormat, transColor, contrast)) {
        QApplication::restoreOverrideCursor();
        QMessageBox::warning(this, tr("Error"), tr("Failed to create sprite."));
        return;
    }
    
    QApplication::restoreOverrideCursor();
    QMessageBox::information(this, tr("Success"), tr("Sprite created successfully."));
}


void SpriteViewerWindow::onAnimationTimer()
{
    size_t frameCount = m_spriteLoader.getFrameCount();
    if (frameCount == 0)
        return;
    
    // Handle File tab animation
    if (m_fileIsPlaying) {
        m_fileCurrentFrameIndex = (m_fileCurrentFrameIndex + 1) % frameCount;
        // Set current row without auto-scrolling
        QListWidgetItem* item = m_fileFrameListWidget->item(static_cast<int>(m_fileCurrentFrameIndex));
        if (item) {
            // Save current scroll position
            int scrollValue = m_fileFrameListWidget->verticalScrollBar()->value();
            
            // Block signals temporarily to prevent selection change events
            m_fileFrameListWidget->blockSignals(true);
            m_fileFrameListWidget->setCurrentItem(item, QItemSelectionModel::SelectCurrent);
            m_fileFrameListWidget->blockSignals(false);
            
            // Restore scroll position to prevent auto-scrolling
            m_fileFrameListWidget->verticalScrollBar()->setValue(scrollValue);
        }
        updateFileFrameDisplay();
    }
    
    // Stop timer if not playing
    if (!m_fileIsPlaying) {
        m_animationTimer->stop();
    }
}

void SpriteViewerWindow::updateFileFrameList()
{
    m_fileFrameListWidget->clear();
    
    const auto& frames = m_spriteLoader.getFrames();
    bool isLithtech = m_spriteLoader.isLithtech();
    
    for (size_t i = 0; i < frames.size(); i++) {
        if (isLithtech && !frames[i].dtxPath.empty()) {
            m_fileFrameListWidget->addItem(QString("#%1: %2").arg(i).arg(QString::fromStdString(frames[i].dtxPath)));
        } else {
            m_fileFrameListWidget->addItem(tr("Frame #%1").arg(i));
        }
    }
}

void SpriteViewerWindow::updateFileFrameDisplay()
{
    if (!m_fileFrameListWidget || !m_previewWidget)
        return;
    
    int row = m_fileFrameListWidget->currentRow();
    if (row < 0 || row >= static_cast<int>(m_spriteLoader.getFrameCount()))
        return;
    
#ifdef QT_WIDGETS_LIB
    const auto& frames = m_spriteLoader.getFrames();
    if (row >= static_cast<int>(frames.size()))
        return;
    
    const auto& frame = frames[row];
    if (frame.image.isNull())
        return;
    
    m_previewWidget->setImage(frame.image);
#endif
}

void SpriteViewerWindow::onFileFrameListSelectionChanged()
{
    updateFileFrameDisplay();
}

void SpriteViewerWindow::onFilePlayAnimation()
{
    if (m_spriteLoader.getFrameCount() == 0)
        return;
    
    m_fileIsPlaying = true;
    m_fileCurrentFrameIndex = m_fileFrameListWidget->currentRow();
    if (m_fileCurrentFrameIndex < 0)
        m_fileCurrentFrameIndex = 0;
    
    m_animationTimer->start(m_fileAnimationSpeedSpin->value());
    m_filePlayButton->setEnabled(false);
    m_fileStopButton->setEnabled(true);
}

void SpriteViewerWindow::onFileStopAnimation()
{
    m_fileIsPlaying = false;
    m_animationTimer->stop();
    m_filePlayButton->setEnabled(true);
    m_fileStopButton->setEnabled(false);
}

// ============================================================================
// Palette Manipulation
// ============================================================================

void SpriteViewerWindow::onPaletteBrightnessUp()
{
    const auto& header = m_spriteLoader.getHeader();
    if (header.version != SpriteVersion::GOLDSRC) return;
    
    auto& palette = m_spriteLoader.getPalette();
    if (palette.empty()) return;
    
    saveUndoState();
    
    for (size_t i = 0; i < palette.size(); i++) {
        palette[i] = static_cast<uint8_t>(qMin(255, static_cast<int>(palette[i]) + 10));
    }
    
    m_spriteLoader.rebuildFramesFromPalette();
    updatePaletteDisplay();
    updateFileFrameDisplay();
    updateUndoRedoButtons();
    m_paletteInfoLabel->setText(tr("Brightness increased"));
}

void SpriteViewerWindow::onPaletteBrightnessDown()
{
    const auto& header = m_spriteLoader.getHeader();
    if (header.version != SpriteVersion::GOLDSRC) return;
    
    auto& palette = m_spriteLoader.getPalette();
    if (palette.empty()) return;
    
    saveUndoState();
    
    for (size_t i = 0; i < palette.size(); i++) {
        palette[i] = static_cast<uint8_t>(qMax(0, static_cast<int>(palette[i]) - 10));
    }
    
    m_spriteLoader.rebuildFramesFromPalette();
    updatePaletteDisplay();
    updateFileFrameDisplay();
    updateUndoRedoButtons();
    m_paletteInfoLabel->setText(tr("Brightness decreased"));
}

void SpriteViewerWindow::onPaletteGrayscale()
{
    const auto& header = m_spriteLoader.getHeader();
    if (header.version != SpriteVersion::GOLDSRC) return;
    
    auto& palette = m_spriteLoader.getPalette();
    if (palette.empty()) return;
    
    saveUndoState();
    
    for (size_t i = 0; i < palette.size() / 3; i++) {
        int r = palette[i * 3];
        int g = palette[i * 3 + 1];
        int b = palette[i * 3 + 2];
        // Luminance formula
        int gray = static_cast<int>(0.299 * r + 0.587 * g + 0.114 * b);
        palette[i * 3] = static_cast<uint8_t>(gray);
        palette[i * 3 + 1] = static_cast<uint8_t>(gray);
        palette[i * 3 + 2] = static_cast<uint8_t>(gray);
    }
    
    m_spriteLoader.rebuildFramesFromPalette();
    updatePaletteDisplay();
    updateFileFrameDisplay();
    updateUndoRedoButtons();
    m_paletteInfoLabel->setText(tr("Converted to grayscale"));
}

void SpriteViewerWindow::onPaletteSepia()
{
    const auto& header = m_spriteLoader.getHeader();
    if (header.version != SpriteVersion::GOLDSRC) return;
    
    auto& palette = m_spriteLoader.getPalette();
    if (palette.empty()) return;
    
    saveUndoState();
    
    for (size_t i = 0; i < palette.size() / 3; i++) {
        int r = palette[i * 3];
        int g = palette[i * 3 + 1];
        int b = palette[i * 3 + 2];
        
        // Sepia formula
        int newR = static_cast<int>(0.393 * r + 0.769 * g + 0.189 * b);
        int newG = static_cast<int>(0.349 * r + 0.686 * g + 0.168 * b);
        int newB = static_cast<int>(0.272 * r + 0.534 * g + 0.131 * b);
        
        palette[i * 3] = static_cast<uint8_t>(qMin(255, newR));
        palette[i * 3 + 1] = static_cast<uint8_t>(qMin(255, newG));
        palette[i * 3 + 2] = static_cast<uint8_t>(qMin(255, newB));
    }
    
    m_spriteLoader.rebuildFramesFromPalette();
    updatePaletteDisplay();
    updateFileFrameDisplay();
    updateUndoRedoButtons();
    m_paletteInfoLabel->setText(tr("Applied sepia effect"));
}

void SpriteViewerWindow::onPaletteContrast()
{
    const auto& header = m_spriteLoader.getHeader();
    if (header.version != SpriteVersion::GOLDSRC) return;
    
    auto& palette = m_spriteLoader.getPalette();
    if (palette.empty()) return;
    
    // Ask for contrast value using input dialog
    bool ok;
    double contrast = QInputDialog::getDouble(this, tr("Adjust Contrast"),
        tr("Contrast value:"),
        1.5, 0.1, 5.0, 1, &ok);
    if (!ok) return;
    
    saveUndoState();
    
    // Apply contrast to each color in palette
    for (size_t i = 0; i < palette.size() / 3; i++) {
        int r = palette[i * 3];
        int g = palette[i * 3 + 1];
        int b = palette[i * 3 + 2];
        
        // Contrast formula: (color - 128) * contrast + 128
        r = std::clamp(static_cast<int>((r - 128) * contrast + 128), 0, 255);
        g = std::clamp(static_cast<int>((g - 128) * contrast + 128), 0, 255);
        b = std::clamp(static_cast<int>((b - 128) * contrast + 128), 0, 255);
        
        palette[i * 3] = static_cast<uint8_t>(r);
        palette[i * 3 + 1] = static_cast<uint8_t>(g);
        palette[i * 3 + 2] = static_cast<uint8_t>(b);
    }
    
    m_spriteLoader.rebuildFramesFromPalette();
    updatePaletteDisplay();
    updateFileFrameDisplay();
    updateUndoRedoButtons();
    m_paletteInfoLabel->setText(tr("Contrast: %1x").arg(contrast, 0, 'f', 1));
}

void SpriteViewerWindow::onPaletteInvert()
{
    const auto& header = m_spriteLoader.getHeader();
    if (header.version != SpriteVersion::GOLDSRC) return;
    
    auto& palette = m_spriteLoader.getPalette();
    if (palette.empty()) return;
    
    saveUndoState();
    
    for (size_t i = 0; i < palette.size(); i++) {
        palette[i] = static_cast<uint8_t>(255 - palette[i]);
    }
    
    m_spriteLoader.rebuildFramesFromPalette();
    updatePaletteDisplay();
    updateFileFrameDisplay();
    updateUndoRedoButtons();
    m_paletteInfoLabel->setText(tr("Colors inverted"));
}

void SpriteViewerWindow::onPaletteSwapRB()
{
    const auto& header = m_spriteLoader.getHeader();
    if (header.version != SpriteVersion::GOLDSRC) return;
    
    auto& palette = m_spriteLoader.getPalette();
    if (palette.empty()) return;
    
    saveUndoState();
    
    // Swap R <-> B
    for (size_t i = 0; i < palette.size() / 3; i++) {
        std::swap(palette[i * 3], palette[i * 3 + 2]);
    }
    
    m_spriteLoader.rebuildFramesFromPalette();
    updatePaletteDisplay();
    updateFileFrameDisplay();
    updateUndoRedoButtons();
    m_paletteInfoLabel->setText(tr("Swapped R <-> B"));
}

void SpriteViewerWindow::onPaletteSwapRG()
{
    const auto& header = m_spriteLoader.getHeader();
    if (header.version != SpriteVersion::GOLDSRC) return;
    
    auto& palette = m_spriteLoader.getPalette();
    if (palette.empty()) return;
    
    saveUndoState();
    
    // Swap R <-> G
    for (size_t i = 0; i < palette.size() / 3; i++) {
        std::swap(palette[i * 3], palette[i * 3 + 1]);
    }
    
    m_spriteLoader.rebuildFramesFromPalette();
    updatePaletteDisplay();
    updateFileFrameDisplay();
    updateUndoRedoButtons();
    m_paletteInfoLabel->setText(tr("Swapped R <-> G"));
}

void SpriteViewerWindow::onPaletteSwapBG()
{
    const auto& header = m_spriteLoader.getHeader();
    if (header.version != SpriteVersion::GOLDSRC) return;
    
    auto& palette = m_spriteLoader.getPalette();
    if (palette.empty()) return;
    
    saveUndoState();
    
    // Swap B <-> G
    for (size_t i = 0; i < palette.size() / 3; i++) {
        std::swap(palette[i * 3 + 1], palette[i * 3 + 2]);
    }
    
    m_spriteLoader.rebuildFramesFromPalette();
    updatePaletteDisplay();
    updateFileFrameDisplay();
    updateUndoRedoButtons();
    m_paletteInfoLabel->setText(tr("Swapped B <-> G"));
}

void SpriteViewerWindow::saveUndoState()
{
    const auto& header = m_spriteLoader.getHeader();
    const auto& palette = m_spriteLoader.getPalette();
    SpriteUndoState state;
    state.palette = palette;
    state.type = header.type;
    state.texture_format = header.texture_format;
    m_undoStack.push(state);
    // Clear redo stack when new action is performed
    while (!m_redoStack.empty()) {
        m_redoStack.pop();
    }
}

bool SpriteViewerWindow::isOriginalState() const
{
    const auto& header = m_spriteLoader.getHeader();
    if (header.type != m_originalType) return false;
    if (header.texture_format != m_originalTextureFormat) return false;
    const auto& palette = m_spriteLoader.getPalette();
    if (palette != m_originalPalette && !m_originalPalette.empty()) return false;
    return true;
}

void SpriteViewerWindow::updateUndoRedoButtons()
{
    m_paletteUndo->setEnabled(!m_undoStack.empty());
    m_paletteRedo->setEnabled(!m_redoStack.empty());
}

void SpriteViewerWindow::onPaletteUndo()
{
    if (m_undoStack.empty()) return;
    
    // Save current state to redo stack
    auto& header = m_spriteLoader.getHeader();
    SpriteUndoState currentState;
    currentState.palette = m_spriteLoader.getPalette();
    currentState.type = header.type;
    currentState.texture_format = header.texture_format;
    m_redoStack.push(currentState);
    
    // Restore from undo stack
    const auto& state = m_undoStack.top();
    m_spriteLoader.getPalette() = state.palette;
    header.type = state.type;
    header.texture_format = state.texture_format;
    m_undoStack.pop();
    
    m_spriteLoader.rebuildFramesFromPalette();
    updatePaletteDisplay();
    updateFileFrameDisplay();
    updateUndoRedoButtons();
    
    // Update property UI
    if (m_spriteTypeEditCombo) {
        m_spriteTypeEditCombo->blockSignals(true);
        m_spriteTypeEditCombo->setCurrentIndex(header.type);
        m_spriteTypeEditCombo->blockSignals(false);
    }
    if (m_textureFormatEditCombo) {
        m_textureFormatEditCombo->blockSignals(true);
        m_textureFormatEditCombo->setCurrentIndex(header.texture_format);
        m_textureFormatEditCombo->blockSignals(false);
    }
    if (m_spriteTypeLabel)
        m_spriteTypeLabel->setText(QString(tr("(Current: %1)")).arg(getSpriteTypeName(header.type)));
    if (m_textureFormatLabel)
        m_textureFormatLabel->setText(QString(tr("(Current: %1)")).arg(getTextureFormatName(header.texture_format)));
    
    m_paletteInfoLabel->setText(tr("Undo"));
    setModified(!isOriginalState());
}

void SpriteViewerWindow::onPaletteRedo()
{
    if (m_redoStack.empty()) return;
    
    // Save current state to undo stack
    auto& header = m_spriteLoader.getHeader();
    SpriteUndoState currentState;
    currentState.palette = m_spriteLoader.getPalette();
    currentState.type = header.type;
    currentState.texture_format = header.texture_format;
    m_undoStack.push(currentState);
    
    // Restore from redo stack
    const auto& state = m_redoStack.top();
    m_spriteLoader.getPalette() = state.palette;
    header.type = state.type;
    header.texture_format = state.texture_format;
    m_redoStack.pop();
    
    m_spriteLoader.rebuildFramesFromPalette();
    updatePaletteDisplay();
    updateFileFrameDisplay();
    updateUndoRedoButtons();
    
    // Update property UI
    if (m_spriteTypeEditCombo) {
        m_spriteTypeEditCombo->blockSignals(true);
        m_spriteTypeEditCombo->setCurrentIndex(header.type);
        m_spriteTypeEditCombo->blockSignals(false);
    }
    if (m_textureFormatEditCombo) {
        m_textureFormatEditCombo->blockSignals(true);
        m_textureFormatEditCombo->setCurrentIndex(header.texture_format);
        m_textureFormatEditCombo->blockSignals(false);
    }
    if (m_spriteTypeLabel)
        m_spriteTypeLabel->setText(QString(tr("(Current: %1)")).arg(getSpriteTypeName(header.type)));
    if (m_textureFormatLabel)
        m_textureFormatLabel->setText(QString(tr("(Current: %1)")).arg(getTextureFormatName(header.texture_format)));
    
    m_paletteInfoLabel->setText(tr("Redo"));
    setModified(!isOriginalState());
}

// ============================================================================
// File Browser Functions
// ============================================================================

void SpriteViewerWindow::onFileBrowserCurrentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous);
    if (!current.isValid())
        return;
    
    QString path = m_fileBrowserModel->filePath(current);
    m_pathEdit->setText(path);
    
    QFileInfo fi(path);
    if (fi.isFile() && fi.suffix().toLower() == "spr") {
        loadSprite(path);
    }
}

void SpriteViewerWindow::onFileBrowserDoubleClicked(const QModelIndex& index)
{
    QString path = m_fileBrowserModel->filePath(index);
    QFileInfo fi(path);
    
    if (fi.isDir()) {
        navigateToPath(path);
    } else if (fi.suffix().toLower() == "spr") {
        loadSprite(path);
    }
}

void SpriteViewerWindow::onNavigateBack()
{
    if (m_historyIndex > 0) {
        m_historyIndex--;
        QString path = m_navigationHistory[m_historyIndex];
        m_fileBrowser->setRootIndex(m_fileBrowserModel->index(path));
        m_pathEdit->setText(path);
        m_backButton->setEnabled(m_historyIndex > 0);
        m_forwardButton->setEnabled(m_historyIndex < m_navigationHistory.size() - 1);

    }
}

void SpriteViewerWindow::onNavigateForward()
{
    if (m_historyIndex < m_navigationHistory.size() - 1) {
        m_historyIndex++;
        QString path = m_navigationHistory[m_historyIndex];
        m_fileBrowser->setRootIndex(m_fileBrowserModel->index(path));
        m_pathEdit->setText(path);
        m_backButton->setEnabled(m_historyIndex > 0);
        m_forwardButton->setEnabled(m_historyIndex < m_navigationHistory.size() - 1);
    }
}

void SpriteViewerWindow::onNavigateUp()
{
    QModelIndex current = m_fileBrowser->rootIndex();
    QString currentPath = m_fileBrowserModel->filePath(current);
    QDir dir(currentPath);
    if (dir.cdUp()) {
        navigateToPath(dir.absolutePath());
    }
}

void SpriteViewerWindow::onPathEditReturnPressed()
{
    QString path = m_pathEdit->text().trimmed();
    if (QDir(path).exists()) {
        navigateToPath(path);
    } else if (QFile::exists(path) && path.toLower().endsWith(".spr")) {
        loadSprite(path);
    }
}

void SpriteViewerWindow::navigateToPath(const QString& path)
{
    if (!QDir(path).exists()) return;
    
    // Add to history
    if (m_historyIndex < m_navigationHistory.size() - 1) {
        // Remove forward history
        while (m_navigationHistory.size() > m_historyIndex + 1) {
            m_navigationHistory.removeLast();
        }
    }
    m_navigationHistory.append(path);
    m_historyIndex = m_navigationHistory.size() - 1;
    
    m_fileBrowser->setRootIndex(m_fileBrowserModel->index(path));
    m_pathEdit->setText(path);
    
    m_backButton->setEnabled(m_historyIndex > 0);
    m_forwardButton->setEnabled(false);
}

// ============================================================================
// Palette Editing Functions
// ============================================================================

void SpriteViewerWindow::onPaletteColorDoubleClicked(int index, QRgb color)
{
    const auto& header = m_spriteLoader.getHeader();
    if (header.version != SpriteVersion::GOLDSRC) return;
    
    QColor currentColor(color);
    QColor newColor = QColorDialog::getColor(currentColor, this, tr("Edit Color %1").arg(index));
    
    if (!newColor.isValid()) return;
    
    saveUndoState();
    
    auto& palette = m_spriteLoader.getPalette();
    if (index * 3 + 2 < static_cast<int>(palette.size())) {
        palette[index * 3] = static_cast<uint8_t>(newColor.red());
        palette[index * 3 + 1] = static_cast<uint8_t>(newColor.green());
        palette[index * 3 + 2] = static_cast<uint8_t>(newColor.blue());
    }
    
    m_spriteLoader.rebuildFramesFromPalette();
    updatePaletteDisplay();
    updateFileFrameDisplay();
    updateUndoRedoButtons();
    m_paletteInfoLabel->setText(tr("Color %1 changed").arg(index));
    setModified(!isOriginalState());
}

void SpriteViewerWindow::onPaletteReset()
{
    const auto& header = m_spriteLoader.getHeader();
    if (header.version != SpriteVersion::GOLDSRC) return;
    
    if (m_originalPalette.empty()) return;
    
    saveUndoState();
    
    auto& palette = m_spriteLoader.getPalette();
    palette = m_originalPalette;
    
    // Reset H/S/C sliders to 0 (block signals to avoid triggering applyHueSaturation multiple times)
    if (m_hueSlider) {
        m_hueSlider->blockSignals(true);
        m_hueSlider->setValue(0);
        m_hueSlider->blockSignals(false);
    }
    if (m_saturationSlider) {
        m_saturationSlider->blockSignals(true);
        m_saturationSlider->setValue(0);
        m_saturationSlider->blockSignals(false);
    }

    // Update labels
    if (m_hueLabel) m_hueLabel->setText(tr("H: 0"));
    if (m_satLabel) m_satLabel->setText(tr("S: 0"));
    
    m_spriteLoader.rebuildFramesFromPalette();
    updatePaletteDisplay();
    updateFileFrameDisplay();
    updateUndoRedoButtons();
    m_paletteInfoLabel->setText(tr("Palette reset to original"));
    setModified(!isOriginalState());
}

void SpriteViewerWindow::onHueChanged(int value)
{
    if (m_hueLabel) {
        m_hueLabel->setText(tr("H: %1").arg(value));
    }
    applyHueSaturation();
    setModified(!isOriginalState());
}

void SpriteViewerWindow::onSaturationChanged(int value)
{
    if (m_satLabel) {
        m_satLabel->setText(tr("S: %1").arg(value));
    }
    applyHueSaturation();
    setModified(!isOriginalState());
}


void SpriteViewerWindow::applyHueSaturation()
{
    const auto& header = m_spriteLoader.getHeader();
    if (header.version != SpriteVersion::GOLDSRC || m_originalPalette.empty()) return;
    
    int hueShift = m_hueSlider ? m_hueSlider->value() : 0;
    int satAdjust = m_saturationSlider ? m_saturationSlider->value() : 0;

    
    auto& palette = m_spriteLoader.getPalette();
    
    for (size_t i = 0; i < m_originalPalette.size() / 3; i++) {
        int r = m_originalPalette[i * 3];
        int g = m_originalPalette[i * 3 + 1];
        int b = m_originalPalette[i * 3 + 2];
        
        QColor color(r, g, b);
        int h, s, v;
        color.getHsv(&h, &s, &v);
        
        // Apply hue shift
        h = (h + hueShift) % 360;
        if (h < 0) h += 360;
        
        // Apply saturation adjustment
        s = qBound(0, s + satAdjust, 255);
        
        color.setHsv(h, s, v);
        
        // Get RGB after H/S adjustments
        r = color.red();
        g = color.green();
        b = color.blue();
        
 
        
        palette[i * 3] = static_cast<uint8_t>(r);
        palette[i * 3 + 1] = static_cast<uint8_t>(g);
        palette[i * 3 + 2] = static_cast<uint8_t>(b);
    }
    
    m_spriteLoader.rebuildFramesFromPalette();
    updatePaletteDisplay();
    updateFileFrameDisplay();
}

// ============================================================================
// Sprite Type/Format Helpers
// ============================================================================

QString SpriteViewerWindow::getSpriteTypeName(int type) const
{
    switch (type) {
        case 0: return tr("VP_PARALLEL_UPRIGHT");
        case 1: return tr("FACING_UPRIGHT");
        case 2: return tr("VP_PARALLEL");
        case 3: return tr("ORIENTED");
        case 4: return tr("VP_PARALLEL_ORIENTED");
        default: return tr("Unknown (%1)").arg(type);
    }
}

QString SpriteViewerWindow::getTextureFormatName(int format) const
{
    switch (format) {
        case 0: return tr("Normal");
        case 1: return tr("Additive");
        case 2: return tr("IndexAlpha");
        case 3: return tr("AlphaTest");
        default: return tr("Unknown (%1)").arg(format);
    }
}

void SpriteViewerWindow::onSpriteTypeChanged(int index)
{
    if (m_currentSpritePath.isEmpty()) return;
    
    auto& header = m_spriteLoader.getHeader();
    if (header.version != SpriteVersion::GOLDSRC) return;
    
    int newType = m_spriteTypeEditCombo->currentData().toInt();
    saveUndoState();
    header.type = newType;
    
    if (m_spriteTypeLabel) {
        m_spriteTypeLabel->setText(QString(tr("(Current: %1)")).arg(getSpriteTypeName(newType)));
    }
    setModified(!isOriginalState());
}

void SpriteViewerWindow::onTextureFormatChanged(int index)
{
    if (m_currentSpritePath.isEmpty()) return;
    
    auto& header = m_spriteLoader.getHeader();
    if (header.version != SpriteVersion::GOLDSRC) return;
    
    int newFormat = m_textureFormatEditCombo->currentData().toInt();
    saveUndoState();
    header.texture_format = newFormat;
    
    if (m_textureFormatLabel) {
        m_textureFormatLabel->setText(QString(tr("(Current: %1)")).arg(getTextureFormatName(newFormat)));
    }
    setModified(!isOriginalState());
}

// ============================================================================
// Next/Previous Sprite Navigation
// ============================================================================

void SpriteViewerWindow::onPrevSprite()
{
    loadNextPrevSprite(false);
}

void SpriteViewerWindow::onNextSprite()
{
    loadNextPrevSprite(true);
}

void SpriteViewerWindow::loadNextPrevSprite(bool next)
{
    if (m_currentSpritePath.isEmpty()) return;
    
    QFileInfo currentFile(m_currentSpritePath);
    QDir dir = currentFile.absoluteDir();
    
    // Get all .spr files in directory
    QStringList sprFiles = dir.entryList({"*.spr"}, QDir::Files);
    if (sprFiles.isEmpty()) return;
    
    // Natural sort (like Windows Explorer - numbers sorted numerically)
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(sprFiles.begin(), sprFiles.end(), [&collator](const QString& a, const QString& b) {
        return collator.compare(a, b) < 0;
    });
    
    // Find current file index
    int currentIndex = sprFiles.indexOf(currentFile.fileName());
    if (currentIndex == -1) return;
    
    // Calculate next/prev index
    int newIndex;
    if (next) {
        newIndex = (currentIndex + 1) % sprFiles.size();
    } else {
        newIndex = (currentIndex - 1 + sprFiles.size()) % sprFiles.size();
    }
    
    // Load the new sprite
    QString newPath = dir.absoluteFilePath(sprFiles[newIndex]);
    loadSprite(newPath);
}

// ============================================================================
// Lithtech Sprite Slots
// ============================================================================

void SpriteViewerWindow::onBrowseLithtechTextureDir()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select DTX Texture Directory"));
    if (!dir.isEmpty() && m_lithtechTextureDirEdit) {
        m_lithtechTextureDirEdit->setText(dir);
    }
}

void SpriteViewerWindow::onLoadLithtechTextures()
{
    if (!m_spriteLoader.isLithtech()) return;
    
    QString baseDir;
    if (m_lithtechTextureDirEdit) {
        baseDir = m_lithtechTextureDirEdit->text();
    }
    
    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool loaded = m_spriteLoader.loadLithtechTextures(baseDir.toStdString());
    QApplication::restoreOverrideCursor();
    
    if (loaded) {
        updateFileFrameDisplay();
        updateFileInfo();
        
        if (m_lithtechExportGoldSrcButton) m_lithtechExportGoldSrcButton->setEnabled(true);
        if (m_lithtechExportFramesButton) m_lithtechExportFramesButton->setEnabled(true);
        m_extractFrameButton->setEnabled(true);
        m_extractAllFramesButton->setEnabled(true);
        
        QMessageBox::information(this, tr("Success"), 
            tr("Textures loaded successfully. You can now preview frames and export."));
    } else {
        QMessageBox::warning(this, tr("Warning"), 
            tr("No textures could be loaded. Check the texture directory path."));
    }
}

void SpriteViewerWindow::onExportLithtechToGoldSrc()
{
    if (!m_spriteLoader.isLithtech()) return;
    
    QString outputPath = QFileDialog::getSaveFileName(this, tr("Export to GoldSrc Sprite"), 
                                                        QString(), tr("Sprite files (*.spr)"));
    if (outputPath.isEmpty()) return;
    
    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool ok = m_spriteLoader.exportLithtechToGoldSrc(outputPath.toStdString());
    QApplication::restoreOverrideCursor();
    
    if (ok) {
        QMessageBox::information(this, tr("Success"), 
            tr("Exported to GoldSrc sprite: %1").arg(outputPath));
    } else {
        QMessageBox::warning(this, tr("Error"), tr("Failed to export sprite."));
    }
}

void SpriteViewerWindow::onExportLithtechFrames()
{
    if (!m_spriteLoader.isLithtech()) return;
    
    QString outputDir = QFileDialog::getExistingDirectory(this, tr("Select Output Directory"));
    if (outputDir.isEmpty()) return;
    
    QApplication::setOverrideCursor(Qt::WaitCursor);
    bool ok = m_spriteLoader.exportLithtechFramesToBmp(outputDir.toStdString());
    QApplication::restoreOverrideCursor();
    
    if (ok) {
        QMessageBox::information(this, tr("Success"), 
            tr("Frames exported to: %1").arg(outputDir));
    } else {
        QMessageBox::warning(this, tr("Error"), tr("Failed to export frames."));
    }
}
