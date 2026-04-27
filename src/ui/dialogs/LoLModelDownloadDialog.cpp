#include "LoLModelDownloadDialog.h"
#include "LoLModelDownloader.h"
#include "core/VortigauntLog.h"
#include "SettingsDialog.h"
#include "smd/SmdWriter.h"
#include "GLBViewer.h"
#include "utils/Bmp.h"
#include "utils/Platform.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <QListWidget>
#include <QPushButton>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPlainTextEdit>
#include <QComboBox>
#include <QScrollArea>
#include <QGridLayout>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSplitter>
#include <QFileDialog>
#include <QMessageBox>
#include <QCompleter>
#include <QScreen>
#include <QApplication>
#include <QStandardPaths>

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QRegularExpression>
#include <QApplication>
#include <QBuffer>
#include <QLibraryInfo>
#include <QPluginLoader>

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>

#include <QFont>
#include <QPainter>
#include <QImage>
#include <QImageReader>


// we're just get the league of legends models from Khada's CDN and view.
// Khada: https://modelviewer.lol/

SkinCardWidget::SkinCardWidget(const QString& skinName, const QString& skinId, QWidget* parent)
    : QWidget(parent)
    , m_skinName(skinName)
    , m_skinId(skinId)
    , m_selected(false)
    , m_hovered(false)
{
    setFixedSize(300, 370);  // Card size: image (310) + name (42) + spacing (18)
    setCursor(Qt::PointingHandCursor);
    
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);
    
    m_imageLabel = new QLabel(this);
    m_imageLabel->setFixedSize(300, 310);  // LINUX FIX: was 500, overflowed card (350) causing transparent ghost region
    m_imageLabel->setScaledContents(true);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setStyleSheet("background-color: #1a1a1a; border-radius: 0px;");
    m_imageLabel->setText(tr("Loading..."));
    layout->addWidget(m_imageLabel);
    
    m_nameLabel = new QLabel(skinName, this);
    m_nameLabel->setFixedHeight(42); 
    m_nameLabel->setAlignment(Qt::AlignCenter);
    m_nameLabel->setWordWrap(true);
    m_nameLabel->setStyleSheet("color: white; font-weight: bold; font-size: 12px; padding: 4px;");
    layout->addWidget(m_nameLabel);
    
    setStyleSheet("background-color: #2a2a2a; border-radius: 12px; border: 2px solid transparent;");
}

void SkinCardWidget::setSplashArt(const QPixmap& pixmap)
{
    m_splashArt = pixmap;
    if (!pixmap.isNull()) {
        QPixmap scaled = pixmap.scaled(300, 310, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        m_imageLabel->setPixmap(scaled);
        m_imageLabel->setText("");
    }
}

void SkinCardWidget::setSelected(bool selected)
{
    m_selected = selected;
    update();
}

void SkinCardWidget::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    
    QRect rect = this->rect();
    
    // Background
    QColor bgColor = m_selected ? QColor(70, 130, 180) : (m_hovered ? QColor(50, 50, 50) : QColor(42, 42, 42));
    painter.setBrush(bgColor);
    painter.setPen(m_selected ? QPen(QColor(100, 150, 200), 3) : QPen(QColor(60, 60, 60), 2));
    painter.drawRoundedRect(rect.adjusted(1, 1, -1, -1), 12, 12);
    
    // Selection indicator
    if (m_selected) {
        painter.setBrush(QColor(100, 150, 200, 100));
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(rect.adjusted(2, 2, -2, -2), 10, 10);
    }
}

void SkinCardWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked();
    }
    QWidget::mousePressEvent(event);
}

void SkinCardWidget::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void SkinCardWidget::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

LoLModelDownloadDialog::LoLModelDownloadDialog(QWidget* parent)
    : QDialog(parent)
    , m_cancelled(false)
    , m_currentChampion()
    , m_contentStack(nullptr)
    , m_viewerWidget(nullptr)
    , m_backToSkinsButton(nullptr)
    , m_previousLogWidget(nullptr)
{
    m_networkManager = new QNetworkAccessManager(this);
    m_currentReply = nullptr;
    
    setupUI();
    
    loadChampionList();
}

LoLModelDownloadDialog::~LoLModelDownloadDialog()
{
    // Clean up current temp file
    if (!m_currentTempFile.isEmpty() && QFileInfo::exists(m_currentTempFile)) {
        QFile::remove(m_currentTempFile);
        m_currentTempFile.clear();
    }
    
    // Clean up VortigauntLoLViewer temp directory (viewer preview downloads)
    QString viewerTempDir = QDir::tempPath() + "/VortigauntLoLViewer";
    QDir viewerDir(viewerTempDir);
    if (viewerDir.exists()) {
        viewerDir.removeRecursively();
    }
    
    // Clean up VortigauntTemp directory (SMD export temp downloads)
    QString exportTempDir = QDir::temp().filePath("VortigauntTemp");
    QDir exportDir(exportTempDir);
    if (exportDir.exists()) {
        exportDir.removeRecursively();
    }
}


void LoLModelDownloadDialog::logToDialog(const QString& message)
{
    if (m_logEdit) {
        VortigauntLog::Vortigaunt_Printf(m_logEdit, message);
    }
}


void LoLModelDownloadDialog::setupUI()
{
    setWindowTitle(tr("LoL Models by Khada ( modelviewer.lol )"));
    setWindowFlags(windowFlags() | Qt::WindowMaximizeButtonHint | Qt::WindowMinimizeButtonHint);
    setMinimumSize(640, 480);
    
    QScreen* screen = QGuiApplication::primaryScreen();
    QSize screenSize = screen ? screen->availableGeometry().size() : QSize(1920, 1080);
    resize(screenSize.width() * 0.8, screenSize.height() * 0.8);
    
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(5);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    
    // Champion selection
    auto* championGroup = new QGroupBox(tr("Champion"));
    championGroup->setStyleSheet("QGroupBox { font-weight: bold; }");
    auto* championLayout = new QHBoxLayout();
    
    championLayout->addWidget(new QLabel(tr("Champion:")));
    m_championCombo = new QComboBox();
    m_championCombo->setEditable(true);
    m_championCombo->setInsertPolicy(QComboBox::NoInsert);
    m_championCombo->setPlaceholderText(tr("Type champion name and press Enter..."));
    m_championCombo->setMinimumWidth(300);

    m_championCombo->setCompleter(nullptr); // Disable default completer, we'll use autocomplete
    championLayout->addWidget(m_championCombo, 1);
    
    
    championGroup->setLayout(championLayout);
    
    // Skin selection - Modern grid layout with splash arts
    auto* skinGroup = new QGroupBox(tr("Skins"));
    skinGroup->setStyleSheet("QGroupBox { font-weight: bold; }");
    auto* skinLayout = new QVBoxLayout();
    skinLayout->setSpacing(10);
    
    auto* skinButtonLayout = new QHBoxLayout();
    m_refreshSkinsButton = new QPushButton(tr("Refresh Skins"));
    m_refreshSkinsButton->setEnabled(false);
    skinButtonLayout->addWidget(m_refreshSkinsButton);
    
    skinButtonLayout->addStretch();
    skinLayout->addLayout(skinButtonLayout);
    
    // Create stacked widget for skin cards and 3D viewer
    m_contentStack = new QStackedWidget();
    
    // Page 0: Skin cards
    QWidget* skinCardsPage = new QWidget();
    auto* skinCardsLayout = new QVBoxLayout(skinCardsPage);
    skinCardsLayout->setContentsMargins(0, 0, 0, 0);
    
    // Scroll area for skin cards
    m_skinScrollArea = new QScrollArea();
    m_skinScrollArea->setWidgetResizable(true);
    m_skinScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_skinScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_skinScrollArea->setStyleSheet("QScrollArea { border: 1px solid #3a3a3a; border-radius: 8px; background-color: #1a1a1a; }");
    
    m_skinContainer = new QWidget();
    m_skinGridLayout = new QGridLayout(m_skinContainer);
    m_skinGridLayout->setSpacing(20);
    m_skinGridLayout->setContentsMargins(20, 20, 20, 20);
    
    m_skinScrollArea->setWidget(m_skinContainer);
    skinCardsLayout->addWidget(m_skinScrollArea);
    
    // Fallback list widget (hidden, for compatibility)
    m_skinListWidget = new QListWidget();
    m_skinListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_skinListWidget->setEnabled(false);
    m_skinListWidget->setVisible(false);
    skinCardsLayout->addWidget(m_skinListWidget, 0);
    
    m_contentStack->addWidget(skinCardsPage); // Index 0
    
    // Page 1: 3D Viewer
    QWidget* viewerPage = new QWidget();
    auto* viewerLayout = new QVBoxLayout(viewerPage);
    viewerLayout->setContentsMargins(0, 0, 0, 0);
    
    // Back button
    m_backToSkinsButton = new QPushButton(tr("Back to Skins"));
    m_backToSkinsButton->setStyleSheet("QPushButton { padding: 8px; font-weight: bold; }");
    connect(m_backToSkinsButton, &QPushButton::clicked, [this]() {
        
        // Clear viewed skin ID when going back
        m_viewedSkinId.clear();
        updateDownloadButton();
        
        // Cancel any pending viewer download
        if (m_viewerDownloadReply) {
            m_viewerDownloadReply->abort();
            m_viewerDownloadReply->deleteLater();
            m_viewerDownloadReply = nullptr;
        }
        
        // Cancel any pending chroma detection requests
        for (QNetworkReply* reply : m_chromaDetectionReplies) {
            if (reply && !reply->isFinished()) {
                reply->abort();
            }
            reply->deleteLater();
        }
        m_chromaDetectionReplies.clear();
        m_chromaDetectionPending = 0;
        
        // Clear model and all resources before switching back
        if (m_viewerWidget) {
            m_viewerWidget->clearModel();
            m_viewerWidget->update();  // Force repaint to clear screen
        }
        
        // Clean up temp file if exists
        if (!m_currentTempFile.isEmpty() && QFileInfo::exists(m_currentTempFile)) {
            QFile::remove(m_currentTempFile);
            logToDialog(tr("DEBUG: [Viewer] Temp file deleted: %1").arg(m_currentTempFile));
            m_currentTempFile.clear();
        }
        
        m_progressBar->setVisible(false);
        m_contentStack->setCurrentIndex(0); // Switch back to skin cards
        setWindowTitle(tr("LoL Model Downloader")); // Reset window title
    });
    viewerLayout->addWidget(m_backToSkinsButton);
    
    // Main viewer area: horizontal layout with mesh list on left, 3D view on right
    m_viewerHLayout = new QHBoxLayout();
    
    // Left panel: Mesh list with visibility checkboxes
    QGroupBox* meshGroup = new QGroupBox(tr("Meshes"));
    meshGroup->setMaximumWidth(200);
    auto* meshLayout = new QVBoxLayout(meshGroup);
    m_meshList = new QListWidget();
    m_meshList->setMaximumWidth(190);
    meshLayout->addWidget(m_meshList);
    m_viewerHLayout->addWidget(meshGroup);
    
    // EAGER INIT: Create GLBViewer directly in setupUI().
    // Creating it BEFORE the dialog is ever shown completely eliminates the "window closing and opening"
    // (Native window handle recreation) bugs. Because it is directly added to layout instead of via 
    // error-prone replaceWidget/insertWidget during runtime, it eliminates any chance of "ghost viewer" overlapping.
    m_viewerWidget = new GLBViewer(viewerPage);
    m_viewerWidget->setLogCallback([this](const QString& msg) {
        logToDialog(QString("DEBUG: [Viewer] %1").arg(msg));
    });
    
    // Connect signals immediately
    connect(m_viewerWidget, &GLBViewer::modelLoaded, [this](bool success) {
        if (m_animCombo) m_animCombo->clear();
        if (m_meshList) m_meshList->clear();
        if (m_chromaCombo) {
            m_chromaCombo->clear();
            m_chromaCombo->addItem(tr("Base"));
        }
        
        if (success && m_viewerWidget) {
            QStringList animNames = m_viewerWidget->getAnimationNames();
            if (!animNames.isEmpty() && m_animCombo) {
                m_animCombo->addItems(animNames);
                m_animCombo->setEnabled(true);
                logToDialog(tr("DEBUG: Loaded %1 animations").arg(animNames.size()));
            } else if (m_animCombo) {
                m_animCombo->setEnabled(false);
            }
            
            QStringList meshNames = m_viewerWidget->getMeshNames();
            if (m_meshList) {
                for (int i = 0; i < meshNames.size(); ++i) {
                    QListWidgetItem* item = new QListWidgetItem(meshNames[i]);
                    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
                    item->setCheckState(Qt::Checked);
                    item->setData(Qt::UserRole, i);
                    m_meshList->addItem(item);
                }
            }
            logToDialog(tr("DEBUG: Loaded %1 meshes").arg(meshNames.size()));
        } else if (m_animCombo) {
            m_animCombo->setEnabled(false);
        }
    });

    // Chroma loaded signal
    connect(m_viewerWidget, &GLBViewer::chromasLoaded, [this](int count) {
        if (count > 0 && m_viewerWidget && m_chromaCombo) {
            QStringList chromaNames = m_viewerWidget->getChromaNames();
            m_chromaCombo->clear();
            m_chromaCombo->addItems(chromaNames);
            m_chromaCombo->setEnabled(true);
            logToDialog(tr("DEBUG: Loaded %1 chroma(s)").arg(count));
        }
    });

    m_viewerHLayout->addWidget(m_viewerWidget, 1);
    
    viewerLayout->addLayout(m_viewerHLayout, 1);
    
    // Viewer controls row
    auto* viewerControlsLayout = new QHBoxLayout();
    
    // Animation selection combo box
    QLabel* animLabel = new QLabel(tr("Animation"), viewerPage);
    m_animCombo = new QComboBox(viewerPage);
    m_animCombo->setMinimumWidth(200);
    m_animCombo->setEnabled(false);
    
    // Animation controls - use lambdas with null guard (viewer doesn't exist yet)
    QPushButton* playButton = new QPushButton(tr("Play"), viewerPage);
    QPushButton* pauseButton = new QPushButton(tr("Pause"), viewerPage);
    QPushButton* stopButton = new QPushButton(tr("Stop"), viewerPage);
    
    // Chroma selection combo box
    QLabel* chromaLabel = new QLabel(tr("Chroma:"), viewerPage);
    m_chromaCombo = new QComboBox(viewerPage);
    m_chromaCombo->setMinimumWidth(120);
    m_chromaCombo->setEnabled(false);
    m_chromaCombo->addItem(tr("Base"));
    
    connect(m_animCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        if (index >= 0 && m_viewerWidget) {
            QString animName = m_animCombo->itemText(index);
            m_viewerWidget->setCurrentAnimation(animName);
            logToDialog(tr("Animation selected: %1").arg(animName));
        }
    });
    
    // Animation controls - lambda-guarded since viewer may not exist yet
    connect(playButton, &QPushButton::clicked, this, [this]() {
        if (m_viewerWidget) m_viewerWidget->playAnimation();
    });
    connect(pauseButton, &QPushButton::clicked, this, [this]() {
        if (m_viewerWidget) m_viewerWidget->pauseAnimation();
    });
    connect(stopButton, &QPushButton::clicked, this, [this]() {
        if (m_viewerWidget) m_viewerWidget->stopAnimation();
    });
    
    // Chromas
    connect(m_chromaCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), [this](int index) {
        if (m_viewerWidget) {
            m_viewerWidget->setCurrentChroma(index - 1); // -1 because index 0 is "Base"
            logToDialog(tr("Chroma selected: %1").arg(m_chromaCombo->itemText(index)));
        }
    });
    
    // Connect mesh visibility checkbox
    connect(m_meshList, &QListWidget::itemChanged, [this](QListWidgetItem* item) {
        if (m_viewerWidget) {
            int meshIndex = item->data(Qt::UserRole).toInt();
            bool visible = (item->checkState() == Qt::Checked);
            m_viewerWidget->setMeshVisible(meshIndex, visible);
        }
    });
    
    viewerControlsLayout->addWidget(animLabel);
    viewerControlsLayout->addWidget(m_animCombo);
    viewerControlsLayout->addWidget(playButton);
    viewerControlsLayout->addWidget(pauseButton);
    viewerControlsLayout->addWidget(stopButton);
    viewerControlsLayout->addSpacing(20);
    
    // Chroma controls (no export button in viewer - use main Export SMD button)
    viewerControlsLayout->addWidget(chromaLabel);
    viewerControlsLayout->addWidget(m_chromaCombo);
    viewerControlsLayout->addStretch();
    
    viewerLayout->addLayout(viewerControlsLayout);
    
    m_contentStack->addWidget(viewerPage); // Index 1
    
    skinLayout->addWidget(m_contentStack, 1);
    skinGroup->setLayout(skinLayout);
    
    
    // Output path
    auto* outputGroup = new QGroupBox(tr("Output"));
    auto* outputLayout = new QHBoxLayout();
    
    outputLayout->addWidget(new QLabel(tr("Folder:")));
    m_outputPathEdit = new QLineEdit();
    m_outputPathEdit->setPlaceholderText(tr("Leave empty for default output folder..."));
    m_browseOutputButton = new QPushButton(tr("Browse..."));
    outputLayout->addWidget(m_outputPathEdit, 1);
    outputLayout->addWidget(m_browseOutputButton);
    
    outputGroup->setLayout(outputLayout);
    
    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    m_cancelButton = new QPushButton(tr("Cancel"));
    m_cancelButton->setVisible(false);
    m_cancelButton->setEnabled(false);
    buttonLayout->addWidget(m_cancelButton);
    buttonLayout->addStretch();
    
    m_viewCharacterButton = new QPushButton(tr("View Character"));
    m_viewCharacterButton->setStyleSheet("QPushButton { padding: 10px 20px; font-weight: bold; background-color: #2196F3; color: white; } QPushButton:hover { background-color: #1976D2; } QPushButton:disabled { background-color: #666; }");
    m_viewCharacterButton->setEnabled(false);
    buttonLayout->addWidget(m_viewCharacterButton);
    
    m_downloadButton = new QPushButton(tr("Export SMD"));
    m_downloadButton->setStyleSheet("QPushButton { padding: 10px 20px; font-weight: bold; background-color: #4CAF50; color: white; } QPushButton:hover { background-color: #45a049; } QPushButton:disabled { background-color: #666; }");
    m_downloadButton->setEnabled(false);
    buttonLayout->addWidget(m_downloadButton);
    
    // Log
    QGroupBox* logGroup = new QGroupBox(tr("Log"));
    logGroup->setStyleSheet("QGroupBox { font-weight: bold; }");
    QVBoxLayout* logLayout = new QVBoxLayout();
    logLayout->setContentsMargins(0, 0, 0, 0);
    
    m_logEdit = new QPlainTextEdit();
    m_logEdit->setReadOnly(true);
    m_logEdit->setFont(QFont("Consolas", 9));
    m_logEdit->setStyleSheet("QTextEdit { background-color: #1a1a1a; color: #ffffff; }");
    m_logEdit->setMinimumHeight(100);
    // Log widget is now swapped in the constructor, after setupUI() completes.
    // This prevents log corruption if setupUI() crashes (e.g., GLBViewer fails).
    
    
    logLayout->addWidget(m_logEdit);

    logGroup->setLayout(logLayout);
    
    // Status bar
    auto* statusLayout = new QHBoxLayout();
    m_statusLabel = new QLabel(tr("Ready"));
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(100);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat("%p%");
    m_progressBar->setMinimumWidth(200);
    
    // Hide/Show Log button in status bar
    QPushButton* toggleLogButton = new QPushButton(tr("Hide Log"));
    toggleLogButton->setCheckable(false);
    toggleLogButton->setChecked(false);
    toggleLogButton->setMinimumWidth(100);
    
    connect(toggleLogButton, &QPushButton::toggled, [this, logGroup, toggleLogButton](bool checked) {
        logGroup->setVisible(checked);
        toggleLogButton->setText(checked ? tr("Hide Log") : tr("Show Log"));
    });
    
    statusLayout->addWidget(m_statusLabel, 1);
    statusLayout->addWidget(m_progressBar);
    statusLayout->addWidget(toggleLogButton);
    
    // Create splitter for resizable log area
    QSplitter* mainSplitter = new QSplitter(Qt::Vertical);
    
    // Top section (champion, skins, output, buttons)
    QWidget* topSection = new QWidget();
    auto* topLayout = new QVBoxLayout(topSection);
    topLayout->setContentsMargins(0, 0, 0, 0);
    topLayout->addWidget(championGroup);
    topLayout->addWidget(skinGroup, 1);
    topLayout->addWidget(outputGroup);
    topLayout->addLayout(buttonLayout);
    
    mainSplitter->addWidget(topSection);
    mainSplitter->addWidget(logGroup);
    
  
    mainSplitter->setSizes({700, 300});
    mainSplitter->setChildrenCollapsible(false); // Prevent log from being completely hidden
    
    // TODO: Fix it. Cant resize because of this.
    mainLayout->addWidget(mainSplitter, 1);
    mainLayout->addLayout(statusLayout);
    
    // Connections
    connect(m_championCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), 
            this, &LoLModelDownloadDialog::onChampionSelected);
    
    // Enter button - only select if exact match
    QLineEdit* lineEdit = m_championCombo->lineEdit();
    if (lineEdit) {
        connect(lineEdit, &QLineEdit::returnPressed, this, [this]() {
            QString text = m_championCombo->currentText().trimmed().toLower();
            // Only proceed if there's an exact match
            if (!text.isEmpty() && m_championList.contains(text)) {
                int index = m_championList.indexOf(text);
                if (index >= 0) {
                    // Block signals to prevent duplicate calls
                    m_championCombo->blockSignals(true);
                    m_championCombo->setCurrentIndex(index);
                    m_championCombo->blockSignals(false);
                    onChampionSelected();
                }
            }
        });
    }
    // Refresh button removed per user request
    connect(m_refreshSkinsButton, &QPushButton::clicked, this, [this]() { if (!m_currentChampion.isEmpty()) loadSkinList(m_currentChampion); });
    connect(m_skinListWidget, &QListWidget::itemSelectionChanged, this, &LoLModelDownloadDialog::onSkinSelected);
    connect(m_viewCharacterButton, &QPushButton::clicked, this, &LoLModelDownloadDialog::onViewCharacterClicked);
    connect(m_downloadButton, &QPushButton::clicked, this, &LoLModelDownloadDialog::onDownloadClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &LoLModelDownloadDialog::onCancelClicked);
    connect(m_browseOutputButton, &QPushButton::clicked, 
            this, [this]() {
                QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Folder"), 
                                                                 m_outputPathEdit->text());
                if (!dir.isEmpty()) {
                    m_outputPathEdit->setText(dir);
                }
            });
}

void LoLModelDownloadDialog::loadChampionList()
{
    logToDialog(tr("Loading champion list..."));
    m_statusLabel->setText(tr("Loading champions..."));
    m_championCombo->setEnabled(false);
    
	// i dont want to deal with fetching from khada right now, so hardcoding the champion list
    QStringList champions = {
        "aatrox", "ahri", "akali", "akshan", "alistar", "ambessa", "amumu", "anivia", "annie", "aphelios",
        "ashe", "aurelionsol", "aurora", "azir", "bard", "belveth", "blitzcrank", "brand", "braum", "briar",
        "caitlyn", "camille", "cassiopeia", "chogath", "corki", "darius", "diana", "draven",
        "drmundo", "ekko", "elise", "evelynn", "ezreal", "fiddlesticks", "fiora", "fizz",
        "galio", "gangplank", "garen", "gnar", "gragas", "graves", "gwen", "hecarim", "heimerdinger",
        "hwei", "illaoi", "irelia", "ivern", "janna", "jarvaniv", "jax", "jayce", "jhin",
        "jinx", "kaisa", "kalista", "karma", "karthus", "kassadin", "katarina", "kayle", "kayn",
        "kennen", "khazix", "kindred", "kled", "kogmaw", "ksante", "leblanc", "leesin", "leona",
        "lillia", "lissandra", "lucian", "lulu", "lux", "malphite", "malzahar", "maokai",
        "masteryi", "mel", "milio", "missfortune", "mordekaiser", "morgana", "naafiri", "nami", "nasus",
        "nautilus", "neeko", "nidalee", "nilah", "nocturne", "nunu", "olaf", "orianna", "ornn",
        "pantheon", "poppy", "pyke", "qiyana", "quinn", "rakan", "rammus", "reksai", "rell",
        "renata", "renekton", "rengar", "riven", "rumble", "ryze", "samira", "sejuani", "senna",
        "seraphine", "sett", "shaco", "shen", "shyvana", "singed", "sion", "sivir", "skarner",
        "sona", "soraka", "swain", "sylas", "syndra", "tahmkench", "taliyah", "talon", "taric",
        "teemo", "thresh", "tristana", "trundle", "tryndamere", "twistedfate", "twitch", "udyr",
        "urgot", "varus", "vayne", "veigar", "velkoz", "vex", "vi", "viego", "viktor", "vladimir",
        "volibear", "warwick", "xayah", "xerath", "xinzhao", "yasuo", "yone", "yorick", "yuumi",
        "zaahen", "zac", "zed", "zeri", "ziggs", "zilean", "zoe", "zyra"
    };
    
    m_championList = champions;
    m_championCombo->clear();
    m_championCombo->blockSignals(true);
    m_championCombo->addItems(champions);
    m_championCombo->setCurrentIndex(-1);
    m_championCombo->blockSignals(false);
    
    // Enable autocomplete for typing
    QCompleter* completer = new QCompleter(champions, this);
    completer->setCaseSensitivity(Qt::CaseInsensitive);
    completer->setCompletionMode(QCompleter::PopupCompletion);
    completer->setFilterMode(Qt::MatchContains);
    m_championCombo->setCompleter(completer);
    
    logToDialog(tr("DEBUG: ^2%1 champions loaded").arg(champions.size()));
    m_statusLabel->setText(tr("Ready"));
    m_championCombo->setEnabled(true);
}

void LoLModelDownloadDialog::onChampionSelected()
{
    QString championName = m_championCombo->currentText().trimmed().toLower();
    
    // If viewer is open, close it and go back to skin cards
    if (m_contentStack && m_contentStack->currentIndex() == 1) {
        // Clear viewed skin ID
        m_viewedSkinId.clear();
        
        // Clear model
        if (m_viewerWidget) {
            m_viewerWidget->clearModel();
            m_viewerWidget->update();
        }
        
        // Switch back to skin cards
        m_contentStack->setCurrentIndex(0);
        setWindowTitle(tr("LoL Model Download"));
        
        logToDialog(tr("^3View closed, switching to new champion"));
    }
    
    if (championName.isEmpty() || !m_championList.contains(championName)) {
        // Clear skin cards
        for (auto* card : m_skinCards) {
            m_skinGridLayout->removeWidget(card);
            card->deleteLater();
        }
        m_skinCards.clear();
        m_selectedSkinIds.clear();
        m_skinListWidget->clear();
        m_skinScrollArea->setEnabled(false);
        m_refreshSkinsButton->setEnabled(false);
        m_downloadButton->setEnabled(false);
        m_currentChampion.clear();
        return;
    }
    
    m_currentChampion = championName;
    logToDialog(tr("^2Champion selected: %1").arg(championName));
    loadSkinList(championName);
}

void LoLModelDownloadDialog::loadSkinList(const QString& championName)
{
    if (championName.isEmpty()) {
        return;
    }
    
    logToDialog(tr("^3Loading skins for %1...").arg(championName));
    m_statusLabel->setText(tr("Loading skins..."));
    
    // first clear existing skin cards
    for (auto* card : m_skinCards) {
        m_skinGridLayout->removeWidget(card);
        card->deleteLater();
    }
    m_skinCards.clear();
    m_selectedSkinIds.clear();
    m_skinListWidget->clear();
    m_skinScrollArea->setEnabled(false);
    m_refreshSkinsButton->setEnabled(false);
    
    // Send a simple request
    QUrl url(QString("https://modelviewer.lol/champions/%1").arg(championName));
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    
    QNetworkReply* reply = m_networkManager->get(request);
    connect(reply, &QNetworkReply::finished, this, &LoLModelDownloadDialog::onSkinListLoaded);
    connect(reply, &QNetworkReply::errorOccurred, this, [this, championName](QNetworkReply::NetworkError error) {
        Q_UNUSED(error);
        logToDialog(tr("ERROR: ^1Failed to load skins for %1").arg(championName));
        m_statusLabel->setText(tr("Failed to load skins"));
        m_refreshSkinsButton->setEnabled(true);
    });
}

void LoLModelDownloadDialog::onSkinListLoaded()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    
    if (reply->error() != QNetworkReply::NoError) {
        logToDialog(tr("ERROR: ^1Network error: %1").arg(reply->errorString()));
        m_statusLabel->setText(tr("Network error"));
        m_refreshSkinsButton->setEnabled(true);
        reply->deleteLater();
        return;
    }
    
    QByteArray data = reply->readAll();
    QString html = QString::fromUtf8(data);
    
    m_skinModels.clear();
    m_skinListWidget->clear();
    
    // Clear existing skin cards
    for (auto* card : m_skinCards) {
        m_skinGridLayout->removeWidget(card);
        card->deleteLater();
    }
    m_skinCards.clear();
    m_selectedSkinIds.clear();
    
    QSet<QString> seenIds; // To avoid duplicates

    // CSS Selector 
    QRegularExpression regex1("<a[^>]*href\\s*=\\s*[^>]*model-viewer\\?id=(\\d+)[^>]*>", 
                             QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption | QRegularExpression::DotMatchesEverythingOption);
    QRegularExpressionMatchIterator matches1 = regex1.globalMatch(html);
    
    int count1 = 0;
    while (matches1.hasNext()) {
        QRegularExpressionMatch match = matches1.next();
        QString modelId = match.captured(1);
        QString fullMatch = match.captured(0);
        count1++;
        
        if (modelId.isEmpty() || seenIds.contains(modelId)) {
            continue;
        }
        
        QString skinName;
        
        QRegularExpression titleRegex("title\\s*=\\s*[\"']([^\"']+)[\"']", QRegularExpression::CaseInsensitiveOption);
        QRegularExpressionMatch titleMatch = titleRegex.match(fullMatch);
        if (titleMatch.hasMatch()) {
            skinName = titleMatch.captured(1).trimmed();
        }
        
        if (skinName.isEmpty()) {
            int linkStart = match.capturedEnd(0);
            int linkEnd = html.indexOf("</a>", linkStart);
            if (linkEnd > linkStart) {
                QString linkContent = html.mid(linkStart, linkEnd - linkStart);
                linkContent.remove(QRegularExpression("<[^>]+>"));
                linkContent.replace("&nbsp;", " ");
                linkContent.replace("&amp;", "&");
                linkContent.replace("&lt;", "<");
                linkContent.replace("&gt;", ">");
                linkContent.replace("&quot;", "\"");
                skinName = linkContent.trimmed();
            }
        }
        
        if (skinName.isEmpty()) {
            int linkStart = match.capturedStart(0);
            int linkEnd = html.indexOf("</a>", match.capturedEnd(0));
            if (linkEnd > linkStart) {
                QString linkSection = html.mid(linkStart, linkEnd - linkStart);
                QRegularExpression nestedTextRegex(">([^<]{3,100}?)<");
                QRegularExpressionMatchIterator nestedMatches = nestedTextRegex.globalMatch(linkSection);
                while (nestedMatches.hasNext() && skinName.isEmpty()) {
                    QRegularExpressionMatch nestedMatch = nestedMatches.next();
                    QString candidate = nestedMatch.captured(1).trimmed();
                    candidate.replace("&nbsp;", " ");
                    candidate.replace("&amp;", "&");
                    candidate = candidate.simplified();
                    if (candidate.length() >= 2 && candidate.length() <= 100 && 
                        !candidate.contains(QRegularExpression("^\\s*\\d+\\s*$"))) {
                        skinName = candidate;
                    }
                }
            }
        }
        
        
  
        
        seenIds.insert(modelId);
        SkinModel skin;
        skin.id = modelId;
        skin.name = skinName;
        m_skinModels.append(skin);
        
        QString displayName = formatSkinDisplayName(skinName, modelId);
        m_skinListWidget->addItem(displayName);
        
        // Create skin card widget
        SkinCardWidget* card = new SkinCardWidget(skinName, modelId, m_skinContainer);
        connect(card, &SkinCardWidget::clicked, this, &LoLModelDownloadDialog::onSkinCardClicked);
        m_skinCards[modelId] = card;
        
        // Add to grid (4 columns)
        int row = m_skinModels.size() - 1;
        m_skinGridLayout->addWidget(card, row / 4, row % 4);
        
        // Load splash art
        loadSplashArt(modelId);
    }
    
    
    if (m_skinModels.isEmpty()) {
        QRegularExpression regex2("<a[^>]*href\\s*=\\s*[\"']/model-viewer\\?id=(\\d+)[\"'][^>]*>([^<]+)</a>", 
                                 QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
        QRegularExpressionMatchIterator matches2 = regex2.globalMatch(html);
        
        int count2 = 0;
        while (matches2.hasNext()) {
            QRegularExpressionMatch match = matches2.next();
            QString modelId = match.captured(1);
            QString skinName = match.captured(2).trimmed();
            count2++;
            
            QString fullMatch = match.captured(0);
            QRegularExpression titleRegex("title\\s*=\\s*[\"']([^\"']+)[\"']", QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch titleMatch = titleRegex.match(fullMatch);
            if (titleMatch.hasMatch()) {
                skinName = titleMatch.captured(1).trimmed();
            }
            
            if (skinName.isEmpty() || skinName == modelId || skinName.length() < 2) {
                skinName = QString("Unknown Skin ModelID: %1").arg(modelId);
            }
            
            if (!modelId.isEmpty() && !seenIds.contains(modelId)) {
                seenIds.insert(modelId);
                SkinModel skin;
                skin.id = modelId;
                skin.name = skinName;
                m_skinModels.append(skin);
                
                QString displayName = formatSkinDisplayName(skinName, modelId);
                m_skinListWidget->addItem(displayName);
                
                SkinCardWidget* card = new SkinCardWidget(skinName, modelId, m_skinContainer);
                connect(card, &SkinCardWidget::clicked, this, &LoLModelDownloadDialog::onSkinCardClicked);
                m_skinCards[modelId] = card;
                
                int row = m_skinModels.size() - 1;
                m_skinGridLayout->addWidget(card, row / 4, row % 4);
                
                loadSplashArt(modelId);
            }
        }
    }
    
    if (m_skinModels.isEmpty()) {
        // JSON array pattern: [{"id":123,"name":"Skin Name"}, ...]
        QRegularExpression jsRegex1("\\{\\s*[\"']id[\"']\\s*:\\s*(\\d+)\\s*,\\s*[\"']name[\"']\\s*:\\s*[\"']([^\"']+)[\"']");
        QRegularExpressionMatchIterator jsMatches1 = jsRegex1.globalMatch(html);
        
        int count3 = 0;
        while (jsMatches1.hasNext()) {
            QRegularExpressionMatch match = jsMatches1.next();
            QString modelId = match.captured(1);
            QString skinName = match.captured(2).trimmed();
            count3++;
            
            if (skinName.isEmpty() || skinName == modelId || skinName.length() < 2) {
                skinName = QString("Unkown Skin ModelID: %1").arg(modelId);
            }
            
            if (!modelId.isEmpty() && !seenIds.contains(modelId)) {
                seenIds.insert(modelId);
                SkinModel skin;
                skin.id = modelId;
                skin.name = skinName;
                m_skinModels.append(skin);
                
                QString displayName = formatSkinDisplayName(skinName, modelId);
                m_skinListWidget->addItem(displayName);
                
                // Create skin card widget
                SkinCardWidget* card = new SkinCardWidget(skinName, modelId, m_skinContainer);
                connect(card, &SkinCardWidget::clicked, this, &LoLModelDownloadDialog::onSkinCardClicked);
                m_skinCards[modelId] = card;
                
                // Add to grid (4 columns)
                int row = m_skinModels.size() - 1;
                m_skinGridLayout->addWidget(card, row / 4, row % 4);
                
                // Load splash art
                loadSplashArt(modelId);
            }
        }
        
        if (m_skinModels.isEmpty()) {
            QRegularExpression jsRegex2("[\"']id[\"']\\s*:\\s*(\\d+)\\s*,\\s*[\"']name[\"']\\s*:\\s*[\"']([^\"']+)[\"']");
            QRegularExpressionMatchIterator jsMatches2 = jsRegex2.globalMatch(html);
            
            int count4 = 0;
            while (jsMatches2.hasNext()) {
                QRegularExpressionMatch match = jsMatches2.next();
                QString modelId = match.captured(1);
                QString skinName = match.captured(2).trimmed();
                count4++;
                
                if (skinName.isEmpty() || skinName == modelId || skinName.length() < 2) {
                    skinName = QString("Unknown Skin ModelID: %1").arg(modelId);
                }
                
                if (!modelId.isEmpty() && !seenIds.contains(modelId)) {
                    seenIds.insert(modelId);
                    SkinModel skin;
                    skin.id = modelId;
                    skin.name = skinName;
                    m_skinModels.append(skin);
                    
                    QString displayName = formatSkinDisplayName(skinName, modelId);
                    m_skinListWidget->addItem(displayName);
                    
                    // Create skin card widget
                    SkinCardWidget* card = new SkinCardWidget(skinName, modelId, m_skinContainer);
                    connect(card, &SkinCardWidget::clicked, this, &LoLModelDownloadDialog::onSkinCardClicked);
                    m_skinCards[modelId] = card;
                    
                    // Add to grid (4 columns)
                    int row = m_skinModels.size() - 1;
                    m_skinGridLayout->addWidget(card, row / 4, row % 4);
                    
                    // Load splash art
                    loadSplashArt(modelId);
                }
            }
        }
    }
    

    if (m_skinModels.isEmpty()) {
        QRegularExpression scriptRegex("<script[^>]*>(.*?)</script>", 
                                      QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
        QRegularExpressionMatchIterator scriptMatches = scriptRegex.globalMatch(html);
        
        while (scriptMatches.hasNext()) {
            QRegularExpressionMatch scriptMatch = scriptMatches.next();
            QString scriptContent = scriptMatch.captured(1);
            
            // JSON array pattern: [{"id":123,"name":"Skin Name"}, ...] or could be {"id":123,"name":"Skin Name"}
            QRegularExpression jsonPattern("[\"']id[\"']\\s*:\\s*(\\d+)\\s*,\\s*[\"']name[\"']\\s*:\\s*[\"']([^\"']+)[\"']", 
                                          QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatchIterator jsonMatches = jsonPattern.globalMatch(scriptContent);
            
            int jsonCount = 0;
            while (jsonMatches.hasNext()) {
                QRegularExpressionMatch jsonMatch = jsonMatches.next();
                QString modelId = jsonMatch.captured(1);
                QString skinName = jsonMatch.captured(2).trimmed();
                
                if (!modelId.isEmpty() && !skinName.isEmpty() && !seenIds.contains(modelId)) {
                    seenIds.insert(modelId);
                    SkinModel skin;
                    skin.id = modelId;
                    skin.name = skinName;
                    m_skinModels.append(skin);
                    
                    QString displayName = formatSkinDisplayName(skinName, modelId);
                    m_skinListWidget->addItem(displayName);
                    
                    // Create skin card widget
                    SkinCardWidget* card = new SkinCardWidget(skinName, modelId, m_skinContainer);
                    connect(card, &SkinCardWidget::clicked, this, &LoLModelDownloadDialog::onSkinCardClicked);
                    m_skinCards[modelId] = card;
                    
                    // Add to grid. 4 columns
                    int row = m_skinModels.size() - 1;
                    m_skinGridLayout->addWidget(card, row / 4, row % 4);
                    
                    loadSplashArt(modelId);
                    
                    jsonCount++;
                }
            }
            if (jsonCount > 0) {
                break;
            }
        }
    }
    

    if (m_skinModels.isEmpty()) {
        QRegularExpression regex5("(?:data-href|onclick|href)\\s*=\\s*[\"'][^\"']*model-viewer\\?id=(\\d+)[^\"']*[\"']", 
                                 QRegularExpression::CaseInsensitiveOption | QRegularExpression::MultilineOption);
        QRegularExpressionMatchIterator matches5 = regex5.globalMatch(html);
        
        int count5 = 0;
        int count5WithName = 0;
        // this thing is really fucked my mind
        while (matches5.hasNext()) {
            QRegularExpressionMatch match = matches5.next();
            QString modelId = match.captured(1);
            count5++;
            
            QString skinName;
            int matchStart = static_cast<int>(match.capturedStart(0));
            
            int startPos = std::max(0, matchStart - 500);
            int endPos = std::min(html.length(), matchStart + match.capturedLength(0) + 500);
            QString context = html.mid(startPos, endPos - startPos);
            
            QRegularExpression titleRegex("title\\s*=\\s*[\"']([^\"']+)[\"']", QRegularExpression::CaseInsensitiveOption);
            QRegularExpressionMatch titleMatch = titleRegex.match(context);
            if (titleMatch.hasMatch()) {
                skinName = titleMatch.captured(1).trimmed();
            }
            
            if (skinName.isEmpty()) {
                QRegularExpression dataNameRegex("data-name\\s*=\\s*[\"']([^\"']+)[\"']", QRegularExpression::CaseInsensitiveOption);
                QRegularExpressionMatch dataNameMatch = dataNameRegex.match(context);
                if (dataNameMatch.hasMatch()) {
                    skinName = dataNameMatch.captured(1).trimmed();
                }
            }
            
            if (skinName.isEmpty()) {
                QRegularExpression ariaLabelRegex("aria-label\\s*=\\s*[\"']([^\"']+)[\"']", QRegularExpression::CaseInsensitiveOption);
                QRegularExpressionMatch ariaLabelMatch = ariaLabelRegex.match(context);
                if (ariaLabelMatch.hasMatch()) {
                    skinName = ariaLabelMatch.captured(1).trimmed();
                }
            }
            
            if (skinName.isEmpty()) {
                QRegularExpression textRegex(">\\s*([^<>{3,100}]+?)\\s*<", QRegularExpression::CaseInsensitiveOption);
                QRegularExpressionMatchIterator textMatches = textRegex.globalMatch(context);
                while (textMatches.hasNext() && skinName.isEmpty()) {
                    QRegularExpressionMatch textMatch = textMatches.next();
                    QString candidate = textMatch.captured(1).trimmed();
                    candidate.replace("&nbsp;", " ");
                    candidate.replace("&amp;", "&");
                    candidate.replace("&lt;", "<");
                    candidate.replace("&gt;", ">");
                    candidate.replace("&quot;", "\"");
                    candidate = candidate.simplified();
                    if (candidate.length() >= 3 && candidate.length() <= 100 && !candidate.contains(QRegularExpression("^\\s*\\d+\\s*$"))) {
                        skinName = candidate;
                    }
                }
            }
            
            if (skinName.isEmpty() || skinName == modelId || skinName.length() < 2) {
                skinName = QString("Skin %1").arg(modelId);
            } else {
                count5WithName++;
            }
            
      
            
            if (!modelId.isEmpty() && !seenIds.contains(modelId)) {
                seenIds.insert(modelId);
                SkinModel skin;
                skin.id = modelId;
                skin.name = skinName;
                m_skinModels.append(skin);
                
                QString displayName = formatSkinDisplayName(skinName, modelId);
                m_skinListWidget->addItem(displayName);
                
                // Create skin card widget
                SkinCardWidget* card = new SkinCardWidget(skinName, modelId, m_skinContainer);
                connect(card, &SkinCardWidget::clicked, this, &LoLModelDownloadDialog::onSkinCardClicked);
                m_skinCards[modelId] = card;
                
                // Add to grid (4 columns)
                int row = m_skinModels.size() - 1;
                m_skinGridLayout->addWidget(card, row / 4, row % 4);
                
                loadSplashArt(modelId);
            }
        }
    }
    
    reply->deleteLater();
    
    if (m_skinModels.isEmpty()) {
        logToDialog(tr("^3No skins found. Try refreshing or check champion name."));
        m_statusLabel->setText(tr("No skins found"));
    } else {
        logToDialog(tr("DEBUG: ^2%1 skins loaded").arg(m_skinModels.size()));
        m_statusLabel->setText(tr("Ready"));
    }
    
    m_skinScrollArea->setEnabled(true);
    m_refreshSkinsButton->setEnabled(true);
    updateDownloadButton();
}

void LoLModelDownloadDialog::loadSplashArt(const QString& skinId)
{
    // Qt doesn't support WEBP by default, so try PNG first, then fallback to WEBP
    // We'll handle WEBP conversion if needed
    QString urlString = QString("https://cdn.modelviewer.lol/lol/tiles/%1.png").arg(skinId);
    QUrl url(urlString);
    
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, 
                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    
    QNetworkReply* reply = m_networkManager->get(request);
    if (!reply) {
        return;
    }
    
    // Store format info with reply
    reply->setProperty("skinId", skinId);
    reply->setProperty("format", "PNG");
    reply->setProperty("webpFallback", true); // Try WEBP if PNG fails
    
    m_splashArtReplies[skinId] = reply;
    connect(reply, &QNetworkReply::finished, this, &LoLModelDownloadDialog::onSplashArtLoaded);
    connect(reply, &QNetworkReply::errorOccurred, this, [this, skinId](QNetworkReply::NetworkError) {
        QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
        if (reply && reply->property("webpFallback").toBool()) {
            loadSplashArtWebP(skinId);
        }
    });
}

void LoLModelDownloadDialog::loadSplashArtWebP(const QString& skinId)
{
    QString urlString = QString("https://cdn.modelviewer.lol/lol/tiles/%1.webp").arg(skinId);
    QUrl url(urlString);
    
    
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, 
                     "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
    
    QNetworkReply* reply = m_networkManager->get(request);
    if (!reply) {
        return;
    }
    
    reply->setProperty("skinId", skinId);
    reply->setProperty("format", "WEBP");
    reply->setProperty("webpFallback", false);
    
    m_splashArtReplies[skinId] = reply;
    connect(reply, &QNetworkReply::finished, this, &LoLModelDownloadDialog::onSplashArtLoaded);
    connect(reply, &QNetworkReply::errorOccurred, this, [this, skinId](QNetworkReply::NetworkError) {
        Q_UNUSED(skinId);
    });
}

void LoLModelDownloadDialog::onSplashArtLoaded()
{
    QNetworkReply* reply = qobject_cast<QNetworkReply*>(sender());
    if (!reply) {
        return;
    }
    
    // Get skinId from reply property (more reliable)
    QString skinId = reply->property("skinId").toString();
    QString format = reply->property("format").toString();
    
    // Fallback: Find in map if property not set
    if (skinId.isEmpty()) {
        for (auto it = m_splashArtReplies.begin(); it != m_splashArtReplies.end(); ++it) {
            if (it.value() == reply) {
                skinId = it.key();
                m_splashArtReplies.erase(it);
                break;
            }
        }
    } else {
        // Remove from map if found
        m_splashArtReplies.remove(skinId);
    }
    
    if (skinId.isEmpty() || !m_skinCards.contains(skinId)) {
        reply->deleteLater();
        return;
    }
    
    
    if (reply->error() != QNetworkReply::NoError) {
        // If PNG failed and we haven't tried WEBP yet, try WEBP
        if (format == "PNG" && reply->property("webpFallback").toBool()) {
            reply->deleteLater();
            loadSplashArtWebP(skinId);
            return;
        }
        reply->deleteLater();
        return;
    }
    
    QByteArray data = reply->readAll();
    
    if (data.isEmpty()) {
        reply->deleteLater();
        return;
    }
    
    // Check if data starts with WEBP signature
    bool isWebP = data.startsWith("RIFF") && data.mid(8, 4) == "WEBP";
    
    // Check available image formats
    QList<QByteArray> supportedFormats = QImageReader::supportedImageFormats();
    bool webpSupported = supportedFormats.contains("webp") || supportedFormats.contains("WEBP");
    
    QPixmap pixmap;
    QImage image;
    bool loaded = false;
    
    // Try QImageReader first (most reliable for WEBP)
    if (isWebP || webpSupported) {
        QBuffer buffer(&data);
        buffer.open(QIODevice::ReadOnly);
        QImageReader reader(&buffer, "WEBP");
        if (reader.canRead()) {
            image = reader.read();
            if (!image.isNull()) {
                pixmap = QPixmap::fromImage(image);
                loaded = true;
            } else {
            }
        } else {
        }
    }
    
    // If it's WEBP, Qt doesn't support it - show placeholder
    if (isWebP && !webpSupported) {
        // Create a simple placeholder image
        QPixmap placeholder(184, 184);
        placeholder.fill(QColor(50, 50, 50));
        QPainter painter(&placeholder);
        painter.setPen(QColor(150, 150, 150));
        painter.setFont(QFont("Arial", 12, QFont::Bold));
        painter.drawText(placeholder.rect(), Qt::AlignCenter, tr("WEBP\nNot\nSupported"));
        painter.end();
        m_skinCards[skinId]->setSplashArt(placeholder);
        reply->deleteLater();
        return;
    }
    
    // Fallback: Try QImage::loadFromData
    if (!loaded) {
        if (image.loadFromData(data, "WEBP")) {
            pixmap = QPixmap::fromImage(image);
            loaded = true;
        } else {
            // Try QImage without format specification
            if (image.loadFromData(data)) {
                pixmap = QPixmap::fromImage(image);
                loaded = true;
            } else {
                // Last resort: Try QPixmap
                if (pixmap.loadFromData(data)) {
                    loaded = true;
                } else {
                    // Show placeholder instead of failing
                    QPixmap placeholder(184, 184);
                    placeholder.fill(QColor(50, 50, 50));
                    QPainter painter(&placeholder);
                    painter.setPen(QColor(150, 150, 150));
                    painter.setFont(QFont("Arial", 10));
                    painter.drawText(placeholder.rect(), Qt::AlignCenter, tr("Image\nLoad\nFailed"));
                    painter.end();
                    m_skinCards[skinId]->setSplashArt(placeholder);
                    reply->deleteLater();
                    return;
                }
            }
        }
    }
    
    if (loaded && !pixmap.isNull()) {
        m_skinCards[skinId]->setSplashArt(pixmap);
    }
    
    reply->deleteLater();
}

void LoLModelDownloadDialog::onSkinCardClicked()
{
    SkinCardWidget* card = qobject_cast<SkinCardWidget*>(sender());
    if (!card) {
        return;
    }
    
    QString skinId = card->skinId();
    
    // Double click to open viewer, single click to select
    static QPoint lastClickPos;
    static qint64 lastClickTime = 0;
    static QString lastClickedSkinId;
    
    QPoint currentPos = QCursor::pos();
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    
    bool isDoubleClick = (skinId == lastClickedSkinId && 
                         (currentTime - lastClickTime) < 300 && 
                         (currentPos - lastClickPos).manhattanLength() < 10);
    
    if (isDoubleClick) {
        // Double click - open GLB viewer
        this->openGLBViewer(skinId);
        lastClickTime = 0; // Reset to prevent triple click
    } else {
        // Single click - toggle selection
        bool wasSelected = m_selectedSkinIds.contains(skinId);
        
        if (wasSelected) {
            m_selectedSkinIds.remove(skinId);
            card->setSelected(false);
        } else {
            m_selectedSkinIds.insert(skinId);
            card->setSelected(true);
        }
        
        updateDownloadButton();
    }
    
    lastClickPos = currentPos;
    lastClickTime = currentTime;
    lastClickedSkinId = skinId;
}

void LoLModelDownloadDialog::onSkinSelected()
{
    // Legacy support for list widget
    QList<QListWidgetItem*> selected = m_skinListWidget->selectedItems();
    updateDownloadButton();
}

void LoLModelDownloadDialog::onViewCharacterClicked()
{
    //  Get the first selected skin to view
    QString skinIdToView;
    
    if (!m_selectedSkinIds.isEmpty()) {
        // Use first selected skin from cards
        skinIdToView = *m_selectedSkinIds.begin();
    } else {
        // Fallback to list widget
        QList<QListWidgetItem*> selected = m_skinListWidget->selectedItems();
        if (!selected.isEmpty()) {
            int row = m_skinListWidget->row(selected.first());
            if (row >= 0 && row < m_skinModels.size()) {
                skinIdToView = m_skinModels[row].id;
            }
        }
    }
    
    if (!skinIdToView.isEmpty()) {
        openGLBViewer(skinIdToView);
    }
}

void LoLModelDownloadDialog::updateDownloadButton()
{
    bool hasSelection = !m_selectedSkinIds.isEmpty() || !m_skinListWidget->selectedItems().isEmpty();
    bool hasViewedSkin = !m_viewedSkinId.isEmpty();
    
    // Enable download if we have selection OR if we're viewing a skin
    m_downloadButton->setEnabled((hasSelection || hasViewedSkin) && !m_currentChampion.isEmpty());
    
    // Enable view button if we have selection
    m_viewCharacterButton->setEnabled(hasSelection && !m_currentChampion.isEmpty());
}

void LoLModelDownloadDialog::onDownloadClicked()
{
    // Get selected skins from cards (preferred) or list widget (fallback)
    QList<SkinModel> selectedSkins;
    
    if (!m_selectedSkinIds.isEmpty()) {
        // Use card selection
        for (const SkinModel& skin : m_skinModels) {
            if (m_selectedSkinIds.contains(skin.id)) {
                selectedSkins.append(skin);
            }
        }
    } else if (!m_skinListWidget->selectedItems().isEmpty()) {
        // Fallback to list widget
        QList<QListWidgetItem*> selected = m_skinListWidget->selectedItems();
        for (QListWidgetItem* item : selected) {
            int row = m_skinListWidget->row(item);
            if (row >= 0 && row < m_skinModels.size()) {
                selectedSkins.append(m_skinModels[row]);
            }
        }
    } else if (!m_viewedSkinId.isEmpty()) {
        // Use viewed skin if no explicit selection
        for (const SkinModel& skin : m_skinModels) {
            if (skin.id == m_viewedSkinId) {
                selectedSkins.append(skin);
                break;
            }
        }
    }
    
    if (selectedSkins.isEmpty() || m_currentChampion.isEmpty()) {
        QMessageBox::warning(this, tr("No Selection"), tr("Please select at least one skin to export."));
        return;
    }

    
    // Output directory
    QString outputDir = m_outputPathEdit->text().trimmed();
    if (outputDir.isEmpty()) {
        outputDir = SettingsDialog::getLoLOutputDir(true);
    }
    QDir().mkpath(outputDir);
    
    if (m_viewerWidget) {
        m_viewerWidget->pauseAnimation();
    }
    
    m_cancelled = false;
    m_downloadButton->setEnabled(false);
    m_cancelButton->setVisible(true);
    m_cancelButton->setEnabled(true);
    m_progressBar->setVisible(true);
    m_progressBar->setValue(0);
    
    logToDialog(tr("=").repeated(80));
    logToDialog(tr("Starting SMD export for %1 skin(s)...").arg(selectedSkins.size()));
    logToDialog(tr("DEBUG: Output folder: %1").arg(outputDir));
    
    QApplication::processEvents();
    
    int total = selectedSkins.size();
    int current = 0;
    int successCount = 0;
    
    for (const SkinModel& skin : selectedSkins) {
        if (m_cancelled) {
            break;
        }
        
        current++;
        
        m_progressBar->setValue((current - 1) * 100 / total);
        m_statusLabel->setText(tr("Exporting %1/%2: %3").arg(current).arg(total).arg(skin.name));
        QApplication::processEvents();
        
        logToDialog(tr("\n[%1/%2] %3 (ID: %4)").arg(current).arg(total).arg(skin.name).arg(skin.id));
        QApplication::processEvents();
        
        logToDialog(tr("  Downloading GLB..."));
        QApplication::processEvents();
        
        QString tempDir = QDir::temp().filePath("VortigauntTemp");
        QDir().mkpath(tempDir);
        
        LoLModelDownloader downloader;
        downloader.setLogCallback([this](const QString& msg) {
            logToDialog(QString("DEBUG:     %1").arg(msg));
            QApplication::processEvents();
        });
        downloader.setProgressCallback([this](int percent) {
            // Don't update main progress bar here
            QApplication::processEvents();
        });
        
        bool downloadSuccess = downloader.downloadModel(
            m_currentChampion,
            skin.id,
            skin.name,
            tempDir,
            true,   // autoFix
            true,   // splitPrimitives
            false   // downloadChromas - not needed for SMD export
        );
        
        if (!downloadSuccess) {
            logToDialog(tr("  Failed to download model."));
            QApplication::processEvents();
            continue;
        }
        
        // Find the downloaded GLB file
        QString championDir = QDir(tempDir).filePath(m_currentChampion);
        QString glbFile;
        
        // Look for GLB files in champion directory
        QDir dir(championDir);
        QStringList glbFiles = dir.entryList(QStringList() << "*.glb", QDir::Files);
        
        // Find the one matching this skin
        QString safeName = skin.name;
        safeName.replace(" ", "_").replace("/", "_").replace("\\", "_");
        
        for (const QString& file : glbFiles) {
            if (file.contains(safeName) || file.contains(skin.id)) {
                glbFile = dir.filePath(file);
                break;
            }
        }
        
        // If not found by name, try to find any GLB with SPLIT suffix
        if (glbFile.isEmpty() && !glbFiles.isEmpty()) {
            for (const QString& file : glbFiles) {
                if (file.contains("_SPLIT")) {
                    glbFile = dir.filePath(file);
                    break;
                }
            }
        }
        
        // Last resort - just take the first GLB
        if (glbFile.isEmpty() && !glbFiles.isEmpty()) {
            glbFile = dir.filePath(glbFiles.first());
        }
        
        if (glbFile.isEmpty() || !QFileInfo::exists(glbFile)) {
            logToDialog(tr("  Could not find downloaded GLB file."));
            QApplication::processEvents();
            continue;
        }
        
        logToDialog(tr("DEBUG:   GLB downloaded: %1").arg(QFileInfo(glbFile).fileName()));
        QApplication::processEvents();
        
        logToDialog(tr("  Exporting to SMD..."));
        QApplication::processEvents();
        
        // Create output folder structure: champion/skinName/
        QString skinSafeName = skin.name;
        skinSafeName.replace(" ", "_").replace("/", "_").replace("\\", "_");
        skinSafeName.replace(":", "_").replace("*", "_").replace("?", "_");
        skinSafeName.replace("\"", "_").replace("<", "_").replace(">", "_");
        skinSafeName.replace("|", "_");
        
        QString modelDir = QDir(outputDir).filePath(m_currentChampion + "/" + skinSafeName);
        QString animDir = QDir(modelDir).filePath("animations");
        
        QDir().mkpath(modelDir);
        QDir().mkpath(animDir);
        
        // Load scene with Assimp
        Assimp::Importer importer;
        const aiScene* scene = importer.ReadFile(glbFile.toStdString(),
            aiProcess_Triangulate | aiProcess_GenNormals | aiProcess_FlipUVs | aiProcess_CalcTangentSpace);
        
        QApplication::processEvents();
        
        if (!scene || !scene->mRootNode) {
            logToDialog(tr("  Failed to load model: %1").arg(importer.GetErrorString()));
            QApplication::processEvents();
            continue;
        }
        
        // Create SmdWriter with progress callback to keep UI responsive
        SmdWriter smdWriter;
        smdWriter.SetMirrorUVY(true); // Mirror UV Y axis like GR2 converter
        smdWriter.SetApplyRootRotation(true); // Fix +90 degree X rotation for GLB->SMD to stand upright
        smdWriter.SetUseTickBasedTiming(true); // GLB/LoL keyframes are in ticks, need conversion
        smdWriter.SetProgressCallback([]() {
            QApplication::processEvents();
        });
        
        // Get hidden mesh indices from viewer if available
        // This allows users to hide meshes in the 3D viewer and exclude them from SMD export
        if (m_viewerWidget && m_viewerWidget->getMeshCount() > 0) {
            std::set<unsigned int> hiddenMeshes = m_viewerWidget->getHiddenMeshIndices();
            if (!hiddenMeshes.empty()) {
                smdWriter.SetHiddenMeshIndices(hiddenMeshes);
                logToDialog(tr("DEBUG:   Excluding %1 hidden mesh(es) from export").arg(hiddenMeshes.size()));
                QApplication::processEvents();
            }
        }
        
        // Export reference SMD (mesh + skeleton)
        QString refPath = QDir(modelDir).filePath("reference.smd");
        
        if (!smdWriter.ExportMeshSMD(scene, refPath.toStdString())) {
            logToDialog(tr("  Failed to export reference mesh."));
            QApplication::processEvents();
            continue;
        }
        
        logToDialog(tr("DEBUG:   Reference mesh exported: reference.smd"));
        QApplication::processEvents();
        
        // Export animations using optimized function (builds node list ONCE)
        int animCount = 0;
        if (scene->mNumAnimations > 0) {
            logToDialog(tr("DEBUG:   Starting animation export..."));
            logToDialog(tr("DEBUG:   Total animations: %1, Total nodes: %2").arg(scene->mNumAnimations).arg(scene->mRootNode ? 1 : 0));
            QApplication::processEvents();
            
            logToDialog(tr("DEBUG:   Exporting %1 animation(s)...").arg(scene->mNumAnimations));
            QApplication::processEvents();
            
            // Manual export with debug logging
            logToDialog(tr("DEBUG:   Building node list..."));
            QApplication::processEvents();
            
            // Count nodes for debug
            int nodeCount = 0;
            std::function<void(aiNode*)> countNodes = [&](aiNode* node) {
                if (!node) return;
                nodeCount++;
                for (unsigned int i = 0; i < node->mNumChildren; ++i) {
                    countNodes(node->mChildren[i]);
                }
            };
            countNodes(scene->mRootNode);
            logToDialog(tr("DEBUG:   Node count: %1").arg(nodeCount));
            QApplication::processEvents();
            
            // Export each animation with detailed timing
            QElapsedTimer timer;
            for (unsigned int i = 0; i < scene->mNumAnimations; ++i) {
                if (m_cancelled) break;
                
                QString animName = QString::fromUtf8(scene->mAnimations[i]->mName.C_Str());
                if (animName.isEmpty()) {
                    animName = QString("anim_%1").arg(i);
                }
                animName.replace(" ", "_").replace("/", "_").replace("\\", "_").replace(":", "_");
                
                // Get animation info for debug
                aiAnimation* anim = scene->mAnimations[i];
                double duration = anim->mDuration;
                double ticksPerSec = anim->mTicksPerSecond;
                
                // Calculate actual frame count like SmdWriter does
                double maxTime = 0.0;
                for (unsigned int c = 0; c < anim->mNumChannels; ++c) {
                    aiNodeAnim* ch = anim->mChannels[c];
                    if (ch->mNumPositionKeys > 0) {
                        maxTime = std::max(maxTime, ch->mPositionKeys[ch->mNumPositionKeys - 1].mTime);
                    }
                    if (ch->mNumRotationKeys > 0) {
                        maxTime = std::max(maxTime, ch->mRotationKeys[ch->mNumRotationKeys - 1].mTime);
                    }
                }
                int actualFrames = (int)(std::ceil(maxTime * 30.0)) + 1;
                
                logToDialog(tr("DEBUG:     [%1/%2] %3 (ch:%4)")
                    .arg(i + 1).arg(scene->mNumAnimations)
                    .arg(animName)
                    .arg(anim->mNumChannels));
                logToDialog(tr("DEBUG:       dur=%1, tps=%2, maxTime=%3, frames=%4")
                    .arg(duration).arg(ticksPerSec).arg(maxTime).arg(actualFrames));
                QApplication::processEvents();
                
                QString animPath = QDir(animDir).filePath(animName + ".smd");
                
                timer.start();
                if (smdWriter.ExportAnimationSMD(scene, i, animPath.toStdString())) {
                    animCount++;
                }
                qint64 elapsed = timer.elapsed();
                
                logToDialog(tr("DEBUG:       Done in %1 ms").arg(elapsed));
                QApplication::processEvents();
            }
            
            logToDialog(tr("  Done: %1 animations exported").arg(animCount));
        }
        
        // Extract textures as 512x512 8-bit color BMP files
        // Texture file names match mesh part names for Blender compatibility
        logToDialog(tr("  Extracting textures..."));
        QApplication::processEvents();
        
        int textureCount = 0;
        std::set<std::string> exportedTextures; // Track exported textures to avoid duplicates
        
        for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; meshIdx++) {
            // Skip hidden meshes
            if (m_viewerWidget && !m_viewerWidget->isMeshVisible(meshIdx)) {
                continue;
            }
            
            aiMesh* mesh = scene->mMeshes[meshIdx];
            if (!mesh) continue;
            
            // Get mesh name for texture filename
            std::string meshName = mesh->mName.C_Str();
            if (meshName.empty()) {
                meshName = "default";
            }
            // Clean up for filesystem
            std::replace(meshName.begin(), meshName.end(), ' ', '_');
            std::replace(meshName.begin(), meshName.end(), '/', '_');
            std::replace(meshName.begin(), meshName.end(), '\\', '_');
            
            // Skip if already exported
            if (exportedTextures.find(meshName) != exportedTextures.end()) {
                continue;
            }
            exportedTextures.insert(meshName);
            
            // Get material and texture
            if (mesh->mMaterialIndex >= scene->mNumMaterials) continue;
            
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            if (!material) continue;
            
            aiString texturePath;
            if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) != AI_SUCCESS) {
                continue;
            }
            
            // Try to get embedded texture
            const aiTexture* aiTex = scene->GetEmbeddedTexture(texturePath.C_Str());
            if (!aiTex) continue;
            
            QImage image;
            if (aiTex->mHeight == 0) {
                // Compressed texture (PNG, JPG, etc.)
                QByteArray data(reinterpret_cast<const char*>(aiTex->pcData), aiTex->mWidth);
                if (!image.loadFromData(data)) {
                    logToDialog(tr("    Failed to load embedded texture for %1").arg(QString::fromStdString(meshName)));
                    continue;
                }
            } else {
                // Raw RGBA texture
                image = QImage(reinterpret_cast<const uchar*>(aiTex->pcData), 
                              aiTex->mWidth, aiTex->mHeight, QImage::Format_RGBA8888);
            }
            
            if (image.isNull()) continue;
            
            // Scale to 512x512
            QImage scaledImage = image.scaled(512, 512, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
            
            // Save as 8-bit indexed BMP using centralized bmp
            QString bmpPath = QDir(modelDir).filePath(QString::fromStdString(meshName) + ".bmp");
            
            QImage argb = scaledImage.convertToFormat(QImage::Format_ARGB32);
            if (BMP::saveAsIndexed8(bmpPath.toUtf8().constData(), 
                                        argb.width(), argb.height(), 
                                        reinterpret_cast<const uint32_t*>(argb.constBits()))) {
                textureCount++;
            } else {
                logToDialog(tr("    Failed to save texture: %1").arg(QString::fromStdString(meshName) + ".bmp"));
            }
        }
        
        if (textureCount > 0) {
            logToDialog(tr("  Extracted %1 texture(s) as 512x512 8-bit BMP").arg(textureCount));
        } else {
            logToDialog(tr("  No embedded textures found to extract"));
        }
        QApplication::processEvents();
        
        logToDialog(tr("  SMD export completed!"));
        QApplication::processEvents();
        
        // Clean up temp file
        QFile::remove(glbFile);
        
        successCount++;
        m_progressBar->setValue(current * 100 / total);
        QApplication::processEvents();
    }
    
    m_progressBar->setValue(100);
    m_progressBar->setVisible(false);
    m_cancelButton->setVisible(false);
    m_cancelButton->setEnabled(false);
    m_downloadButton->setEnabled(true);
    
    if (m_cancelled) {
        logToDialog(tr("\nExport cancelled."));
        m_statusLabel->setText(tr("Cancelled"));
    } else {
        logToDialog(tr("\nExport completed! %1/%2 skins exported successfully.").arg(successCount).arg(total));
        m_statusLabel->setText(tr("Export completed"));
        QMessageBox::information(this, tr("Export Complete"), 
                                QString(tr("Successfully exported %1 skin(s) to SMD format.\n\nOutput folder: %2"))
                                .arg(successCount).arg(outputDir));
    }
}

void LoLModelDownloadDialog::onCancelClicked()
{
    m_cancelled = true;
    m_cancelButton->setEnabled(false);
    logToDialog(tr("Cancelling download..."));
    m_statusLabel->setText(tr("Cancelling..."));
    
    if (m_currentReply) {
        m_currentReply->abort();
    }
}

void LoLModelDownloadDialog::downloadModel(const QString& championName, const QString& modelId, const QString& modelName, bool autoFix, bool splitPrimitives, bool downloadChromas)
{
    LoLModelDownloader downloader;
    
    QString outputDir = m_outputPathEdit->text().trimmed();
    if (outputDir.isEmpty()) {
        outputDir = SettingsDialog::getLoLOutputDir(false);
    }
    
    // Log callback
    downloader.setLogCallback([this](const QString& msg) {
        QString coloredMsg = msg;
        
        // Remove symbols and add color codes
        if (msg.contains("?")) {
            coloredMsg = msg;
            coloredMsg.replace("?", "");
            coloredMsg = coloredMsg.trimmed();
            // Success messages - Green with glow
            if (coloredMsg.contains("GLB downloaded") || 
                coloredMsg.contains("Transforms fixed") ||
                coloredMsg.contains("Mesh primitives split") ||
                (coloredMsg.contains("Found") && coloredMsg.contains("chroma")) ||
                (coloredMsg.contains("Chroma") && coloredMsg.contains("GLB created")) ||
                coloredMsg.contains("Final file") ||
                coloredMsg.contains("Binary chunk read")) {
                coloredMsg = "^2" + coloredMsg;
            } else {
                coloredMsg = "^2" + coloredMsg;
            }
        } else if (msg.contains("?")) {
            coloredMsg = msg;
            coloredMsg.replace("?", "");
            coloredMsg = coloredMsg.trimmed();
            // Error messages - Red with glow
            coloredMsg = "^1" + coloredMsg;
        } else if (msg.contains("?")) {
            coloredMsg = msg;
            coloredMsg.replace("?", "");
            coloredMsg = coloredMsg.trimmed();
            // Warning messages - Yellow without glow
            coloredMsg = "^3" + coloredMsg;
        }
        // Info messages (�) stay as normal (no color code)
        
        logToDialog(coloredMsg);
        QApplication::processEvents();
    });
    
    // Progress callback
    downloader.setProgressCallback([this](int percent) {
        m_progressBar->setValue(percent);
        QApplication::processEvents();
    });
    
    bool success = downloader.downloadModel(
        championName,
        modelId,
        modelName,
        outputDir,
        autoFix,
        splitPrimitives,
        downloadChromas
    );
    
    if (success) {
        logToDialog(tr("DEBUG:   ^2Success"));
    } else {
        logToDialog(tr("DEBUG:   ^1Failed"));
    }
}



void LoLModelDownloadDialog::updateStatus(const QString& message)
{
    m_statusLabel->setText(message);
}

QString LoLModelDownloadDialog::formatSkinDisplayName(const QString& skinName, const QString& modelId)
{
    if (SettingsDialog::getDeveloperMode()) {
        return QString("%1 (ID: %2)").arg(skinName, modelId);
    }
    return skinName;
}

void LoLModelDownloadDialog::openGLBViewer(const QString& skinId)
{
    // Find skin model
    SkinModel* skinModel = nullptr;
    for (auto& skin : m_skinModels) {
        if (skin.id == skinId) {
            skinModel = &skin;
            break;
        }
    }
    
    if (!skinModel || m_currentChampion.isEmpty()) {
        return;
    }
    
    // Cancel any previous viewer download
    if (m_viewerDownloadReply) {
        m_viewerDownloadReply->abort();
        m_viewerDownloadReply->deleteLater();
        m_viewerDownloadReply = nullptr;
    }
    
    // Cancel any pending chroma detection requests
    for (QNetworkReply* reply : m_chromaDetectionReplies) {
        if (reply && !reply->isFinished()) {
            reply->abort();
        }
        reply->deleteLater();
    }
    m_chromaDetectionReplies.clear();
    m_detectedChromaIds.clear();
    m_chromaDetectionPending = 0;
    
    // Set the viewed skin ID for export functionality
    m_viewedSkinId = skinId;
    updateDownloadButton();  // Update button states
    
    // Switch to viewer page in stacked widget
    m_contentStack->setCurrentIndex(1); // Switch to 3D viewer page
    setWindowTitle(tr("LoL Model Downloader - %1").arg(skinModel->name));
    
    // Clear previous model
    if (m_viewerWidget) {
        m_viewerWidget->clearModel();
    }
    
    // Try to find GLB file on disk
    QString outputDir = m_outputPathEdit->text().trimmed();
    if (outputDir.isEmpty()) {
        outputDir = SettingsDialog::getLoLOutputDir(true);
    }
    
    QString championDir = QDir(outputDir).filePath(m_currentChampion);
    QString safeName = skinModel->name;
    for (QChar& c : safeName) {
        if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || 
            c == '"' || c == '<' || c == '>' || c == '|') {
            c = '_';
        }
    }
    
    // Try different possible file names
    QStringList possibleFiles;
    possibleFiles << QString("%1/%2_%3.glb").arg(championDir, safeName, skinId);
    possibleFiles << QString("%1/%2_%3_FIXED.glb").arg(championDir, safeName, skinId);
    possibleFiles << QString("%1/%2_%3_SPLIT.glb").arg(championDir, safeName, skinId);
    possibleFiles << QString("%1/%2_%3_FIXED_SPLIT.glb").arg(championDir, safeName, skinId);
    
    QString glbFile;
    for (const QString& file : possibleFiles) {
        if (QFileInfo::exists(file)) {
            glbFile = file;
            break;
        }
    }
    
    if (!glbFile.isEmpty()) {
        // File found on disk — load directly (no network needed)
        openGLBViewerWithFile(skinId, glbFile);
    } else {
        // ASYNC DOWNLOAD: File not found on disk, download from CDN without blocking UI
        logToDialog(tr("^3Model not found on disk, downloading from CDN..."));
        m_statusLabel->setText(tr("Downloading model..."));
        
        // Create temp directory for GLB file
        QString tempDir = QDir::tempPath() + "/VortigauntLoLViewer";
        QDir().mkpath(tempDir);
        
        // Generate unique temp file name
        QString tempFileName = QString("model_%1_%2_%3.glb")
                                .arg(m_currentChampion)
                                .arg(skinId)
                                .arg(QDateTime::currentMSecsSinceEpoch());
        QString tempFilePath = QDir(tempDir).filePath(tempFileName);
        
        // Build CDN URL
        QString cdnUrl = QString("https://cdn.modelviewer.lol/lol/models/%1/%2/model.glb")
                            .arg(m_currentChampion.toLower(), skinId);
        
        // Start async download using QNetworkAccessManager (NO QEventLoop!)
        QUrl urlObj(cdnUrl);
        QNetworkRequest request(urlObj);
        request.setHeader(QNetworkRequest::UserAgentHeader, 
                         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
        
        m_viewerDownloadReply = m_networkManager->get(request);
        
        // Show progress bar during download
        m_progressBar->setVisible(true);
        m_progressBar->setValue(0);
        
        // Track download progress
        connect(m_viewerDownloadReply, &QNetworkReply::downloadProgress, this, 
            [this](qint64 bytesReceived, qint64 bytesTotal) {
                if (bytesTotal > 0) {
                    int percent = static_cast<int>(bytesReceived * 100 / bytesTotal);
                    m_progressBar->setValue(percent);
                    m_statusLabel->setText(tr("Downloading model... %1%").arg(percent));
                }
            });
        
        // Handle download completion — capture skinId and tempFilePath by value
        connect(m_viewerDownloadReply, &QNetworkReply::finished, this,
            [this, skinId, tempFilePath]() {
                QNetworkReply* reply = m_viewerDownloadReply;
                m_viewerDownloadReply = nullptr;
                m_progressBar->setVisible(false);
                
                if (!reply) return;
                
                if (reply->error() != QNetworkReply::NoError) {
                    reply->deleteLater();
                    logToDialog(tr("^1[Viewer] Download failed: %1").arg(reply->errorString()));
                    m_statusLabel->setText(tr("Download failed"));
                    QMessageBox::warning(this, tr("Download Failed"), 
                                        tr("Failed to download model from CDN. Please check your internet connection."));
                    m_contentStack->setCurrentIndex(0); // Switch back to skin cards
                    return;
                }
                
                QByteArray data = reply->readAll();
                reply->deleteLater();
                
                // Validate GLB data
                if (data.size() < 12 || data.left(4) != "glTF") {
                    logToDialog(tr("^1[Viewer] Invalid GLB data received"));
                    m_statusLabel->setText(tr("Download failed - invalid data"));
                    QMessageBox::warning(this, tr("Download Failed"), 
                                        tr("Downloaded data is not a valid GLB file."));
                    m_contentStack->setCurrentIndex(0);
                    return;
                }
                
                // Write to temp file
                QFile file(tempFilePath);
                if (!file.open(QIODevice::WriteOnly)) {
                    logToDialog(tr("^1[Viewer] Cannot write temp file: %1").arg(tempFilePath));
                    m_contentStack->setCurrentIndex(0);
                    return;
                }
                file.write(data);
                file.close();
                
                // Clean up previous temp file
                if (!m_currentTempFile.isEmpty() && QFileInfo::exists(m_currentTempFile)) {
                    QFile::remove(m_currentTempFile);
                }
                m_currentTempFile = tempFilePath;
                
                QFileInfo tempFileInfo(tempFilePath);
                logToDialog(tr("DEBUG: ^2Model downloaded (%1 MB)")
                    .arg(tempFileInfo.size() / 1024.0 / 1024.0, 0, 'f', 2));
                m_statusLabel->setText(tr("Loading model..."));
                
                // Now load the model (this is CPU-bound but fast)
                openGLBViewerWithFile(skinId, tempFilePath);
            });
    }
}

void LoLModelDownloadDialog::openGLBViewerWithFile(const QString& skinId, const QString& filePath)
{
    // Find skin model for display name
    SkinModel* skinModel = nullptr;
    for (auto& skin : m_skinModels) {
        if (skin.id == skinId) {
            skinModel = &skin;
            break;
        }
    }
    
    if (!m_viewerWidget) {
        logToDialog(tr("^1[Viewer] Cannot load GLB: 3D Viewer is not available on this system"));
        QMessageBox::warning(this, tr("Viewer Error"), tr("3D Viewer is not available on this system."));
        m_contentStack->setCurrentIndex(0);
        return;
    }
    
    // Disconnect any previous modelLoaded connections to this dialog
    disconnect(m_viewerWidget, &GLBViewer::modelLoaded, this, nullptr);
    
    // Connect modelLoaded signal to handle success/failure and async chroma detection
    connect(m_viewerWidget, &GLBViewer::modelLoaded, this, [this, skinModel, skinId, filePath](bool success) {
        if (success) {
            logToDialog(tr("^2[Viewer] Model loaded successfully"));
            if (skinModel) {
                logToDialog(tr("DEBUG: ^2Opening 3D viewer for %1").arg(skinModel->name));
            }
            m_viewerWidget->update();
            m_statusLabel->setText(tr("Ready"));
            
            // Start ASYNC chroma detection (no QEventLoop blocking!)
            QTimer::singleShot(100, this, [this, skinId]() {
                startAsyncChromaDetection(skinId);
            });
        } else {
            logToDialog(tr("^1[Viewer] Failed to load GLB: %1").arg(filePath));
            m_statusLabel->setText(tr("Load failed"));
            QMessageBox::warning(this, tr("Load Failed"), 
                                tr("Failed to load GLB model."));
            m_contentStack->setCurrentIndex(0); // Switch back to skin cards on failure
        }
    });
    
    // Process events to ensure QStackedWidget page transition is processed by Qt Wayland
    QCoreApplication::processEvents();

    logToDialog(tr("[Viewer] Waiting for OpenGL compositor readiness..."));
    
    // Give OpenGL time to initialize on Linux compositors, then load
    QTimer::singleShot(200, this, [this, filePath]() {
        logToDialog(tr("[Viewer] Loading GLB..."));
        m_viewerWidget->loadGLB(filePath);
    });
}

void LoLModelDownloadDialog::startAsyncChromaDetection(const QString& skinId)
{
    // Cancel any previous chroma detection
    for (QNetworkReply* reply : m_chromaDetectionReplies) {
        if (reply && !reply->isFinished()) {
            reply->abort();
        }
        reply->deleteLater();
    }
    m_chromaDetectionReplies.clear();
    m_detectedChromaIds.clear();
    m_chromaDetectionPending = 0;
    
    int baseId = skinId.toInt();
    if (baseId <= 0) {
        logToDialog(tr("No chromas detected for this skin"));
        return;
    }
    
    logToDialog(tr("[Chroma] Checking for chromas (async)..."));
    
    // Send parallel HEAD requests for chroma detection (same range as before: -30 to +30)
    // But now fully async — no QEventLoop!
    int requestCount = 0;
    for (int offset = -30; offset <= 30; ++offset) {
        if (offset == 0) continue;
        
        int testId = baseId + offset;
        QString testIdStr = QString::number(testId);
        
        // Check with Body.png as the test texture
        QString testUrl = QString("https://cdn.modelviewer.lol/lol/models/%1/%2/chromas/%3/Body.png")
                         .arg(m_currentChampion.toLower(), skinId, testIdStr);
        
        QUrl urlObj(testUrl);
        QNetworkRequest request(urlObj);
        request.setHeader(QNetworkRequest::UserAgentHeader, 
                         "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36");
        
        QNetworkReply* reply = m_networkManager->head(request);
        reply->setProperty("chromaId", testIdStr);
        reply->setProperty("skinId", skinId);
        m_chromaDetectionReplies.append(reply);
        requestCount++;
        
        connect(reply, &QNetworkReply::finished, this, [this, reply, skinId]() {
            // Verify this is still for the current skin
            QString replySkinId = reply->property("skinId").toString();
            if (replySkinId != m_viewedSkinId) {
                // Skin changed, ignore this reply
                reply->deleteLater();
                m_chromaDetectionReplies.removeOne(reply);
                return;
            }
            
            QString chromaId = reply->property("chromaId").toString();
            
            if (reply->error() == QNetworkReply::NoError && 
                reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt() == 200) {
                if (!m_detectedChromaIds.contains(chromaId)) {
                    m_detectedChromaIds.append(chromaId);
                }
            }
            
            reply->deleteLater();
            m_chromaDetectionReplies.removeOne(reply);
            m_chromaDetectionPending--;
            
            // All requests completed
            if (m_chromaDetectionPending <= 0) {
                if (!m_detectedChromaIds.isEmpty()) {
                    // Sort by numeric ID
                    std::sort(m_detectedChromaIds.begin(), m_detectedChromaIds.end(),
                        [](const QString& a, const QString& b) {
                            return a.toInt() < b.toInt();
                        });
                    
                    logToDialog(tr("[Chroma] Detected %1 chromas, loading textures...").arg(m_detectedChromaIds.size()));
                    
                    if (m_viewerWidget && replySkinId == m_viewedSkinId) {
                        m_viewerWidget->loadChromaTextures(m_currentChampion, skinId, m_detectedChromaIds);
                    }
                } else {
                    logToDialog(tr("No chromas detected for this skin"));
                }
            }
        });
    }
    
    m_chromaDetectionPending = requestCount;
    logToDialog(tr("DEBUG: [Chroma] Sent %1 async HEAD requests").arg(requestCount));
}





