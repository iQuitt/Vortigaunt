#include "MainWindow.h"

#include <QWidget>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QComboBox>
#include <QCheckBox>
#include <QPlainTextEdit>
#include <QFileDialog>
#include <QMessageBox>
#include <QApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QStandardPaths>
#include <QMenuBar>
#include <QAction>
#include <QListWidget>
#include <QStatusBar>
#include <QProgressBar>
#include <QShortcut>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QUrl>
#include <QTextStream>
#include <QDateTime>
#include <QThread>
#include <QCoreApplication>
#include <QEventLoop>
#include <QTimer>
#include <QProcess>
#include <QMutex>
#include <QMenu>
#include <QSettings>
#include <QRegularExpression>
#include <QFile>
#include <cstdio>
#include <memory>

#include "Platform.h"
#include "core/extractors/unity/UnityFsExtractor.h"
#include "core/extractors/unity/UnityPorter.h"
//#include "ui/dialogs/SettingsDialog.h"

#include <filesystem>
#include <fstream>
#include <vector>
#include "utils/util.hpp"

#include "core/converters/LtbConverter.h"
#include "core/extractors/rez/RezExtractor.h"
#include "core/extractors/xfs/XfsExtractor.h"
#include "core/extractors/vpk/VpkExtractor.h"
#include "core/VortigauntVersion.h"
#include "core/extractors/pak/PakExtractor.h"
#include "core/converters/Gr2Converter.h"
#include "core/VortigauntLog.h"

#ifdef ENABLE_GRANNY2
#include <granny.h>
#endif
#include "DtxViewerDialog.h"
#include "PakViewerWindow.h"
#include "RezViewerWindow.h"
#include "XfsViewerWindow.h"
#include "VpkViewerWindow.h"
#include "SpriteViewerWindow.h"
#include "LoLModelDownloadDialog.h"
#ifdef METIN2_SCRIPT_EFFECT
#include "MseViewerWindow.h"
#endif
#include "AutoRigDialog.h"
#include "WadMakerDialog.h"
#include "SettingsDialog.h"
#include "dialogs/AboutDialog.h"
#include "LanguageManager.h"

#include "WadMaker.h"
#include "AudioConvertDialog.h"
#include "QCFile.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "VortigauntUpdateCheck.h"

#include <QDesktopServices>
#include "Discord_Integration.h"



QString grabFileExt(const QString& filePath)
{
    int idx = filePath.lastIndexOf('.');
    if (idx < 0)
        return {};
    return filePath.mid(idx + 1).toLower();
}

QString replaceFileExt(const QString& filePath, const QString& ext)
{
    int idx = filePath.lastIndexOf('.');
    QString base = (idx < 0) ? filePath : filePath.left(idx);
    return base + "." + ext;
}

void recurseAndCollectFiles(const std::filesystem::path& start, std::vector<std::filesystem::path>& out)
{
    if (!std::filesystem::exists(start))
        return;

    if (std::filesystem::is_regular_file(start))
    {
        out.push_back(start);
        return;
    }

    for (auto const& entry : std::filesystem::recursive_directory_iterator(start))
    {
        if (!entry.is_regular_file())
            continue;
        const auto& p = entry.path();
        auto ext = p.extension().string();
    if (ext == ".ltb" || ext == ".LTB" ||
        ext == ".gr2" || ext == ".GR2" ||
        ext == ".rez" || ext == ".REZ" ||
        ext == ".pak" || ext == ".PAK" ||
        ext == ".vpk" || ext == ".VPK")
    {
        out.push_back(p);
    }
    }
}


MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
    , m_currentLogFilter(VortigauntLog::LogLevel::Info)
    , m_updateChecker(nullptr)
{
    setWindowTitle("Vortigaunt");

#ifdef _WIN32
    setWindowIcon(QIcon(":/app2.ico"));
#else
    setWindowIcon(QIcon(":/app2.png")); // qt already support ico so linux can draw the ico. but somehow the execute file icon is not visible so thats why its png.
	// windows also support png but just to be sure
#endif
    resize(minimumWidth(), minimumHeight());
    setAcceptDrops(true);
    setMinimumWidth(800);
    setMinimumHeight(600);

    // Menu bar with Tools entries
    m_toolsMenu = menuBar()->addMenu(tr("Tools"));

    //Lithtech Submenu
#ifdef ENABLE_LITHTECH
    QMenu* lithtechMenu = m_toolsMenu->addMenu(tr("Lithtech"));
    
    m_rezViewAction = lithtechMenu->addAction(tr("REZ Viewer..."));
    connect(m_rezViewAction, &QAction::triggered, this, &MainWindow::onOpenRezViewer);

    m_dtxViewAction = lithtechMenu->addAction(tr("DTX Viewer..."));
    connect(m_dtxViewAction, &QAction::triggered, this, &MainWindow::onOpenDtxViewer);
    
    m_lithtechSpriteAction = lithtechMenu->addAction(tr("Lithtech Sprite Viewer..."));
    connect(m_lithtechSpriteAction, &QAction::triggered, this, &MainWindow::onOpenLithtechSpriteViewer);
#else
    
    QMenu* lithtechMenu = m_toolsMenu->addMenu(tr("Lithtech"));
    m_rezViewAction = lithtechMenu->addAction(tr("REZ Viewer..."));
    connect(m_rezViewAction, &QAction::triggered, this, &MainWindow::onOpenRezViewer);
#endif

    // GoldSource Submenu
    QMenu* goldSrcMenu = m_toolsMenu->addMenu(tr("GoldSrc"));

    m_spriteViewAction = goldSrcMenu->addAction(tr("Sprite Viewer..."));
    connect(m_spriteViewAction, &QAction::triggered, this, &MainWindow::onOpenSpriteViewer);

    m_wadMakerAction = goldSrcMenu->addAction(tr("WAD Editor..."));
    connect(m_wadMakerAction, &QAction::triggered, this, &MainWindow::onOpenWadMaker);
    
    m_pakViewAction = goldSrcMenu->addAction(tr("PAK Viewer (CSO)..."));
    connect(m_pakViewAction, &QAction::triggered, this, &MainWindow::onOpenPakViewer);

    m_vpkViewAction = goldSrcMenu->addAction(tr("VPK Viewer (Source)..."));
    connect(m_vpkViewAction, &QAction::triggered, this, &MainWindow::onOpenVpkViewer);

    m_audioConvertAction = goldSrcMenu->addAction(tr("Convert WAV for Goldsrc..."));
    connect(m_audioConvertAction, &QAction::triggered, this, &MainWindow::onOpenAudioConverter);

    m_autoRigAction = goldSrcMenu->addAction(tr("Auto-Rig (Beta)"));
    connect(m_autoRigAction, &QAction::triggered, this, &MainWindow::onOpenAutoRigDialog);
    

    // Other Submenu
    QMenu* otherMenu = m_toolsMenu->addMenu(tr("Other"));

    m_lolModelAction = otherMenu->addAction(tr("League of Legends Models..."));
    connect(m_lolModelAction, &QAction::triggered, this, &MainWindow::onOpenLoLModelDownloader);

    m_xfsViewAction = otherMenu->addAction(tr("XFS Viewer (Xenesis File System)..."));
    connect(m_xfsViewAction, &QAction::triggered, this, &MainWindow::onOpenXfsViewer);
    

#ifdef METIN2_SCRIPT_EFFECT
    m_mseViewAction = otherMenu->addAction(tr("Metin2 Script Effect Viewer..."));
    connect(m_mseViewAction, &QAction::triggered, this, &MainWindow::onOpenMseViewer);
#endif

    // Settings menu
    m_settingsMenu = menuBar()->addMenu(tr("Settings"));
    m_settingsAction = m_settingsMenu->addAction(tr("Configure..."));
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::onOpenSettings);


    // About Menu
    QMenu* m_aboutMenu = menuBar()->addMenu(tr("About"));

	m_vortiAboutAction = m_aboutMenu->addAction(tr("About Vortigaunt"));
    connect(m_vortiAboutAction, &QAction::triggered, this, [this] { AboutDialog dlg(this); dlg.exec(); });

	m_qtAboutAction = m_aboutMenu->addAction(tr("About Qt"));
	connect(m_qtAboutAction, &QAction::triggered, this, &QApplication::aboutQt);
    
    // Update menu
    QMenu* m_updateMenu = menuBar()->addMenu(tr("Update"));

    m_checkUpdateAction = m_updateMenu->addAction(tr("Check for Updates..."));
    connect(m_checkUpdateAction, &QAction::triggered, this, &MainWindow::onCheckForUpdates);
    
    // Language submenu
    m_languageMenu = m_settingsMenu->addMenu(tr("Language"));
    QStringList languages = LanguageManager::instance().getAvailableLanguages();
    QString currentLang = LanguageManager::instance().getCurrentLanguage();
    
    for (const QString& lang : languages)
    {       
        QString displayName = LanguageManager::instance().getLanguageDisplayName(lang);
        QAction* langAction = m_languageMenu->addAction(displayName);
        langAction->setCheckable(true);
        langAction->setChecked(lang.toLower() == currentLang.toLower());
        langAction->setData(lang);
        
        connect(langAction, &QAction::triggered, this, [this, lang]() {
            QString currentLang = LanguageManager::instance().getCurrentLanguage();
            if (lang.toLower() != currentLang.toLower())
            {
                LanguageManager::instance().loadLanguage(lang);
                
                // Show restart dialog
                // I did this on purpose ngl i dont want mess up directlyx load. maybe later
                QMessageBox msgBox(this);
                msgBox.setWindowTitle(tr("Restart Required"));
                msgBox.setText(tr("The application needs to restart for the language change to take effect."));
                msgBox.setIcon(QMessageBox::Information);
                msgBox.setStandardButtons(QMessageBox::Ok);
                msgBox.exec();
                
                // Restart application
                QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments());
                QCoreApplication::quit();
            }
        });
    }
    
    QWidget* cornerWidget = new QWidget(this);
    QHBoxLayout* cornerLayout = new QHBoxLayout(cornerWidget);
    cornerLayout->setContentsMargins(0, 0, 16, 0); // slight right margin
    cornerLayout->setSpacing(8);


    // i know most ppl tryna click this but they close the app 
    // so TODO: find a better place for this
    const QString menuBtnStyle = R"(
        QPushButton {
            border: none; padding: 4px 8px; background: transparent;
            color: palette(windowText);
        }
        QPushButton:hover {
            background: rgba(128, 128, 128, 0.15);
        }
        QPushButton:pressed {
            background: rgba(128, 128, 128, 0.25);
        }
    )";

    QPushButton* discordBtn = new QPushButton(tr("Discord"), cornerWidget);
    discordBtn->setIcon(QIcon(":/discord.png"));
    discordBtn->setIconSize(QSize(20, 20));
    discordBtn->setToolTip(tr("Join our Discord"));
    discordBtn->setFlat(true);
    discordBtn->setCursor(Qt::PointingHandCursor);
    discordBtn->setStyleSheet(menuBtnStyle);
    connect(discordBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://discord.gg/PZ9JzgHHKa"));
    });
    cornerLayout->addWidget(discordBtn);

    QPushButton* githubBtn = new QPushButton(tr("GitHub"), cornerWidget);
    githubBtn->setIcon(QIcon(":/github.png"));
    githubBtn->setIconSize(QSize(20, 20));
    githubBtn->setToolTip(tr("GitHub Repository of Vortigaunt"));
    githubBtn->setFlat(true);
    githubBtn->setCursor(Qt::PointingHandCursor);
    githubBtn->setStyleSheet(menuBtnStyle);
    connect(githubBtn, &QPushButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/iQuitt/Vortigaunt"));
    });
    cornerLayout->addWidget(githubBtn);

    menuBar()->setCornerWidget(cornerWidget, Qt::TopRightCorner);

    auto* central = new QWidget(this);
    auto* mainLayout = new QVBoxLayout();

    // Input group
    auto* inputGroup = new QGroupBox(tr("Input"));
    auto* inputLayout = new QVBoxLayout();

    auto* pathLayout = new QHBoxLayout();
    pathLayout->addWidget(new QLabel(tr("Path:")));
    m_inputPathEdit = new QLineEdit();
    m_inputPathEdit->setPlaceholderText(tr("Select input file or drag & drop here..."));
    m_browseInputButton = new QPushButton(tr("Browse..."));
    pathLayout->addWidget(m_inputPathEdit, 1);
    pathLayout->addWidget(m_browseInputButton);

    inputLayout->addLayout(pathLayout);
    inputGroup->setLayout(inputLayout);

    // Output group
    auto* outputGroup = new QGroupBox(tr("Output"));
    auto* outputLayout = new QHBoxLayout();
    outputLayout->addWidget(new QLabel(tr("Folder (optional):")));
    m_outputPathEdit = new QLineEdit();
    m_outputPathEdit->setPlaceholderText(tr("Leave empty for default output folder..."));
    m_browseOutputButton = new QPushButton(tr("Browse..."));
    outputLayout->addWidget(m_outputPathEdit, 1);
    outputLayout->addWidget(m_browseOutputButton);
    outputGroup->setLayout(outputLayout);

    // Operation + options
    auto* optionsGroup = new QGroupBox(tr("Operation"));
    auto* optionsLayout = new QVBoxLayout();

    auto* opLayout = new QHBoxLayout();
    opLayout->addWidget(new QLabel(tr("Mode:")));
    m_operationCombo = new QComboBox();
    m_operationCombo->addItem(tr("Convert LTB"));
    m_operationCombo->addItem(tr("Convert GR2"));
#ifdef METIN2_SCRIPT_EFFECT
    m_operationCombo->addItem(tr("Convert MSE Effect (Metin2 Effect Script)"));
#endif
    m_operationCombo->addItem(tr("Extract REZ file"));
    //m_operationCombo->addItem(tr("Multi-REZ extract (folder with  REZ)"));
    m_operationCombo->addItem(tr("PAK (Counter Strike Online)"));
    m_operationCombo->addItem(tr("Extract VPK (Source Engine)"));
    //m_operationCombo->addItem(tr("Multi-PAK extract (folder with PAK)"));
    m_operationCombo->addItem(tr("Extract UnityFS / Assets"));
    opLayout->addWidget(m_operationCombo, 1);
    optionsLayout->addLayout(opLayout);

    // Output format for LTB/GR2
    auto* fmtLayout = new QHBoxLayout();
    m_outputFormatLabel = new QLabel(tr("Convert To:"));
    fmtLayout->addWidget(m_outputFormatLabel);
    m_outputFormatCombo = new QComboBox();
    m_outputFormatCombo->addItem(tr("SMD (GoldSource)"), QStringLiteral("smd"));
    fmtLayout->addWidget(m_outputFormatCombo, 1);
    optionsLayout->addLayout(fmtLayout);



    m_ignoreMeshesCheck = new QCheckBox(tr("Ignore meshes"));
	m_ignoreMeshesCheck->setVisible(false); 

    m_ignoreAnimsCheck = new QCheckBox(tr("Ignore animations"));
    m_ignoreAnimsCheck->setVisible(false);

    m_separateFoldersCheck = new QCheckBox(tr("Extract multiple file to separate folders"));
    m_separateFoldersCheck->setChecked(false);
	m_separateFoldersCheck->setVisible(false);
    m_separateFoldersCheck->setToolTip(tr("When extracting REZ/PAK/XFS files, create a separate folder for each file"));

    optionsLayout->addWidget(m_ignoreMeshesCheck);
    optionsLayout->addWidget(m_ignoreAnimsCheck);
    optionsLayout->addWidget(m_separateFoldersCheck);
    


    optionsGroup->setLayout(optionsLayout);

    // GR2 Animation Export Group (initially hidden)
    m_gr2AnimGroup = new QGroupBox(tr("GR2 Animation Files"));
    auto* animLayout = new QVBoxLayout();
    
    animLayout->addWidget(new QLabel(tr("Animations (will be exported to {filename}_anims folder):\nDrag and drop animation files here or use the browse button.")));
    m_animationListWidget = new QListWidget();
    m_animationListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_animationListWidget->setAcceptDrops(true);
    m_animationListWidget->setDragDropMode(QAbstractItemView::DropOnly);
    m_animationListWidget->viewport()->setAcceptDrops(true);
    animLayout->addWidget(m_animationListWidget);
    
    // Install event filter for drag-drop on animation list
    m_animationListWidget->viewport()->installEventFilter(this);
    
    // Browse button to add animation files
    auto* animButtonLayout = new QHBoxLayout();
    auto* browseAnimButton = new QPushButton(tr("Browse Animation Files..."));
    animButtonLayout->addWidget(browseAnimButton);
    animButtonLayout->addStretch();
    animLayout->addLayout(animButtonLayout);
    
    m_gr2AnimGroup->setLayout(animLayout);
    m_gr2AnimGroup->setVisible(false); // Hidden by default

    // Run button with Mirror UV Y checkbox (initially hidden, shown only for GR2)
	// When you export the GR2 to Smd or another format, the texture UV Y axis is flipped.
	// Generally you need this option enabled for correct texture orientation.
    auto* runLayout = new QHBoxLayout();
    m_mirrorUVYCheck = new QCheckBox(tr("Mirror UV Y-axis"));
    m_mirrorUVYCheck->setToolTip(tr("Mirror UV Y-axis for GR2 to SMD conversion (fixes texture orientation)"));
    m_mirrorUVYCheck->setVisible(false); // Hidden by default, shown only for GR2
    runLayout->addWidget(m_mirrorUVYCheck);
    
    // Some Alpha is usually reduced on shiny, glittery items. (This is something I experienced in Metin2)
	// and this option is useful for that.
    m_invertAlphaCheck = new QCheckBox(tr("Invert Alpha"));
    m_invertAlphaCheck->setToolTip(tr("Invert alpha channel for DDS textures (fixes transparency issues)"));
    m_invertAlphaCheck->setVisible(false); // Hidden by default, shown only for GR2
    runLayout->addWidget(m_invertAlphaCheck);


	// TODO: its just for only GR2. Make for other formats too
    m_writeQCCheck = new QCheckBox(tr("Write QC"));
    m_writeQCCheck->setToolTip(tr("Generate QC file for studiomdl compilation"));
    m_writeQCCheck->setVisible(false); // Hidden by default, shown only for GR2
    runLayout->addWidget(m_writeQCCheck);
    
    runLayout->addStretch();
    
    //runLayout->addStretch();
    
    
    m_runButton = new QPushButton(tr("Convert")); 

    m_runButton->setText(tr("Run")); 
    runLayout->addWidget(m_runButton);

    // Log with filtering and export
    auto* logGroup = new QGroupBox(tr("Log"));
    auto* logLayout = new QVBoxLayout();
    
    // Log toolbar
    auto* logToolbarLayout = new QHBoxLayout();
    logToolbarLayout->addWidget(new QLabel(tr("Filter:")));
    m_logFilterCombo = new QComboBox();
    m_logFilterCombo->addItem(tr("All"), static_cast<int>(VortigauntLog::LogLevel::Info));
    m_logFilterCombo->addItem(tr("Info"), static_cast<int>(VortigauntLog::LogLevel::Info));
    m_logFilterCombo->addItem(tr("Warning"), static_cast<int>(VortigauntLog::LogLevel::Warning));
    m_logFilterCombo->addItem(tr("Error"), static_cast<int>(VortigauntLog::LogLevel::Error));
    logToolbarLayout->addWidget(m_logFilterCombo);
    logToolbarLayout->addStretch();
    
    m_clearLogButton = new QPushButton(tr("Clear Log"));
    m_saveLogButton = new QPushButton(tr("Export Log..."));
    logToolbarLayout->addWidget(m_clearLogButton);
    logToolbarLayout->addWidget(m_saveLogButton);
    
    m_logEdit = new QPlainTextEdit();
    m_logEdit->setReadOnly(true);
    m_logEdit->setFont(QFont("Consolas", 9)); 
    
    VortigauntLog::addLogWidget(m_logEdit);
    
    logLayout->addLayout(logToolbarLayout);
    logLayout->addWidget(m_logEdit);
    logGroup->setLayout(logLayout);

    mainLayout->addWidget(inputGroup);
    mainLayout->addWidget(outputGroup);
    mainLayout->addWidget(optionsGroup);
    mainLayout->addWidget(m_gr2AnimGroup);
    mainLayout->addLayout(runLayout);
    mainLayout->addWidget(logGroup, 1);

    central->setLayout(mainLayout);
    setCentralWidget(central);



    connect(m_browseInputButton, &QPushButton::clicked, this, &MainWindow::onBrowseInput);
    connect(m_browseOutputButton, &QPushButton::clicked, this, &MainWindow::onBrowseOutput);
    connect(m_runButton, &QPushButton::clicked, this, &MainWindow::onRun);
    connect(m_logFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onLogFilterChanged);
    connect(m_saveLogButton, &QPushButton::clicked, this, &MainWindow::onExportLog);
    connect(m_clearLogButton, &QPushButton::clicked, this, &MainWindow::onClearLog);
    
    // Browse animation files button
    connect(browseAnimButton, &QPushButton::clicked, this, [this]() {
        QStringList files = QFileDialog::getOpenFileNames(this,
                                                          tr("Select Animation Files"),
                                                          QString(),
                                                          tr("Animation files (*.smd *.gr2);;All files (*.*)"));
       for (const QString& file : files)
        {
            QFileInfo fileInfo(file);
            QString fileName = fileInfo.fileName();
            // Check if already in list
            bool found = false;
            for (int i = 0; i < m_animationListWidget->count(); ++i)
            {
                QListWidgetItem* existingItem = m_animationListWidget->item(i);
                QString existingPath = existingItem->data(Qt::UserRole).toString();
                if (existingItem->text() == fileName || existingPath == file)
                {
                    found = true;
                    break;
                }
            }
            if (!found)
            {
                gr2AddAnimationItem(fileName, file);
            }
        }
    });
    
    // Update operation combo when input path changes
    connect(m_inputPathEdit, &QLineEdit::textChanged, this, [this](const QString& text) {
        updateOperationComboForFile(text);
    });
    
    // Show/hide GR2 animation group and update output format based on operation selection
    connect(m_operationCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        QString currentText = m_operationCombo->itemText(index);
        bool isGr2 = currentText.contains("GR2", Qt::CaseInsensitive);
        bool isLtb = currentText.contains("LTB", Qt::CaseInsensitive);
        bool isArchiveFile = currentText.contains("REZ", Qt::CaseInsensitive) ||
			currentText.contains("PAK", Qt::CaseInsensitive) ||
			currentText.contains("VPK", Qt::CaseInsensitive);

        bool isArchiveOperation = isArchiveFile || currentText.contains("XFS", Qt::CaseInsensitive) || currentText.contains("Smart Batch", Qt::CaseInsensitive);
        bool showOutputFormat = isGr2 || isLtb;

        m_gr2AnimGroup->setVisible(isGr2);
        m_mirrorUVYCheck->setVisible(isGr2); // Show Mirror UV Y checkbox only for GR2
        m_invertAlphaCheck->setVisible(isGr2); // Show Invert Alpha checkbox only for GR2
        m_writeQCCheck->setVisible(isGr2 || isLtb); // Show Write QC checkbox  for GR2 & Ltb


        m_ignoreMeshesCheck->setVisible(isLtb);
        m_ignoreAnimsCheck->setVisible(isLtb);


		m_separateFoldersCheck->setVisible(isArchiveOperation);
        
        // Hide output format for archive files as it's not applicable
        if (m_outputFormatLabel) m_outputFormatLabel->setVisible(showOutputFormat);
        if (m_outputFormatCombo) m_outputFormatCombo->setVisible(showOutputFormat);
        
        // Update output format combo
        if (m_outputFormatCombo) {
            m_outputFormatCombo->clear();
            m_outputFormatCombo->addItem(tr("SMD (GoldSource)"), QStringLiteral("smd"));
            m_outputFormatCombo->setCurrentIndex(0);
        }
        
        if (isGr2) {
            // Load animations from GR2 file if available from special folders
            loadGr2Animations();
        }
    });
    
    // Load Mirror UV Y setting and connect to save on change
    m_mirrorUVYCheck->setChecked(SettingsDialog::getMirrorUVY());
    connect(m_mirrorUVYCheck, &QCheckBox::toggled, this, [](bool checked) {
        SettingsDialog::setMirrorUVY(checked);
    });
    
    // Initial update (if there's already text in the input field)
    updateOperationComboForFile(m_inputPathEdit->text());
    
    // Initial output format update
    QString initialText = m_operationCombo->currentText();
    bool initialGr2 = initialText.contains("GR2", Qt::CaseInsensitive);
    bool initialLtb = initialText.contains("LTB", Qt::CaseInsensitive);
    bool isModelOp = initialGr2 || initialLtb;
    
    m_mirrorUVYCheck->setVisible(initialGr2);
    m_invertAlphaCheck->setVisible(initialGr2);
    m_writeQCCheck->setVisible(initialGr2 || initialLtb);
    
    if (m_outputFormatLabel) m_outputFormatLabel->setVisible(isModelOp);
    if (m_outputFormatCombo) {
        m_outputFormatCombo->setVisible(isModelOp);
        m_outputFormatCombo->clear();
        m_outputFormatCombo->addItem(tr("SMD (GoldSource)"), QStringLiteral("smd"));
        m_outputFormatCombo->setCurrentIndex(0);
    }
    
    setupStatusBar();
    
    setupKeyboardShortcuts();

    m_inputPathEdit->setAcceptDrops(true);
    
    m_processingEvents = false;
    

    
    // Check for updates on startup (with a short delay to let the UI load first)
    QTimer::singleShot(2000, this, [this]() {
        if (!m_updateChecker)
        {
            m_updateChecker = new VortigauntUpdateCheck(this);

        }
        
        m_updateChecker->checkForUpdates(true);
    });

}



void MainWindow::gr2AddAnimationItem(const QString& fileName, const QString& filePath)
{
    // Create a widget for the list item with label and remove button
    QWidget* itemWidget = new QWidget();
    QHBoxLayout* itemLayout = new QHBoxLayout(itemWidget);
    itemLayout->setContentsMargins(4, 2, 4, 2);
    itemLayout->setSpacing(4);
    
    QLabel* label = new QLabel(fileName);
    label->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    itemLayout->addWidget(label);
    
	// This is okay for now.
    QPushButton* removeButton = new QPushButton("Remove");
    removeButton->setFixedSize(76, 24);
    removeButton->setToolTip(tr("Remove this animation"));
    itemLayout->addWidget(removeButton);
    
    // Create list item
    QListWidgetItem* item = new QListWidgetItem();
    item->setData(Qt::UserRole, filePath); // Store full path
    item->setSizeHint(itemWidget->sizeHint());
    
    m_animationListWidget->addItem(item);
    m_animationListWidget->setItemWidget(item, itemWidget);
    
    // Connect remove button
    connect(removeButton, &QPushButton::clicked, this, [this, item]() {
        int row = m_animationListWidget->row(item);
        if (row >= 0)
        {
            delete m_animationListWidget->takeItem(row);
        }
    });
}

void MainWindow::loadGr2Animations()
{
    // Clear previous animations when loading a new file
    m_animationListWidget->clear();

    
    QString inputPath = m_inputPathEdit->text().trimmed();
    if (inputPath.isEmpty() || !QFileInfo(inputPath).exists())
        return;
    
    QFileInfo inputFileInfo(inputPath);
    QString inputDir = inputFileInfo.absolutePath();
    QString baseFileName = inputFileInfo.baseName();
    
    // Only auto-load animations for specific directories under ymir work:
    // monster, monster2, npc, npc2, mount, npc_pet (and their subdirectories)
    // For all other directories, user must add animations manually via drag-drop or browse button
    QString pathLower = inputDir.toLower().replace("\\", "/");
    bool isAutoAnimDirectory = false;
    
    // Here is the stupid things.
    // The developer of Metin2 (Ymir) They forcibly placed all the models maps effect etc. in drive D.
    // Otherwise, the textures of the models are not visible in Granny Viewer. So I didn't want to be the one to mess up the layout.
	// And yes you need windows for this to work. Granny SDK does not support Linux.

    // Here, only the animations of the NPCs are located in these directories and we automatically pull them from here.
    if (pathLower.contains("d:/ymir work/monster/") || pathLower.endsWith("d:/ymir work/monster") ||
        pathLower.contains("d:/ymir work/monster2/") || pathLower.endsWith("d:/ymir work/monster2") ||
        pathLower.contains("d:/ymir work/npc/") || pathLower.endsWith("d:/ymir work/npc") ||
        pathLower.contains("d:/ymir work/npc2/") || pathLower.endsWith("d:/ymir work/npc2") ||
        pathLower.contains("d:/ymir work/mount/") || pathLower.endsWith("d:/ymir work/mount") ||
        pathLower.contains("d:/ymir work/npc_pet/") || pathLower.endsWith("d:/ymir work/npc_pet"))
    {
        isAutoAnimDirectory = true;
    }
    
    if (!isAutoAnimDirectory)
        return;
    
#ifdef ENABLE_GRANNY2
    // First, try to load animations from the GR2 file
    std::string gr2Path = inputPath.toStdString();
    granny_file* grannyFile = GrannyReadEntireFile(gr2Path.c_str());
    
    if (grannyFile)
    {
        granny_file_info* fileInfo = GrannyGetFileInfo(grannyFile);
        if (fileInfo && fileInfo->AnimationCount > 0)
        {
            // Load animations from GR2 file
            for (int i = 0; i < fileInfo->AnimationCount; ++i)
            {
                granny_animation* anim = fileInfo->Animations[i];
                if (anim && anim->Name)
                {
                    QString animName = QString::fromUtf8(anim->Name);
                    if (animName.isEmpty())
                        animName = QString("Animation_%1").arg(i);
                    gr2AddAnimationItem(animName, QString()); // Empty path means it's from GR2
                }
            }
            GrannyFreeFile(grannyFile);
            return; // Found animations in GR2 file, done
        }
        GrannyFreeFile(grannyFile);
    }
    
    // If no animations in GR2 file, look for other GR2 files in the same directory
    // These might be animation files
    QDir dir(inputDir);
    QStringList filters;
    filters << "*.gr2" << "*.GR2";
    QFileInfoList gr2Files = dir.entryInfoList(filters, QDir::Files);
    
    for (const QFileInfo& fileInfo : gr2Files)
    {
        QString fileName = fileInfo.fileName();
        if (fileName.compare(inputFileInfo.fileName(), Qt::CaseInsensitive) == 0)
            continue;
        
        // Check if this GR2 file actually contains animations (not just a mesh backup)
        std::string checkPath = fileInfo.absoluteFilePath().toStdString();
        granny_file* checkFile = GrannyReadEntireFile(checkPath.c_str());
        if (checkFile)
        {
            granny_file_info* checkInfo = GrannyGetFileInfo(checkFile);
            bool hasAnimations = (checkInfo && checkInfo->AnimationCount > 0);
            bool hasMeshes = (checkInfo && checkInfo->ModelCount > 0 && checkInfo->Models[0] && checkInfo->Models[0]->MeshBindingCount > 0);
            
            GrannyFreeFile(checkFile);
            
            if (!hasAnimations)
            {
                // skip it
                continue;
            }
            
            // If file has both mesh and animations, it's likely a backup - skip it too
            if (hasMeshes && hasAnimations)
            {
                // This might be a backup with both mesh and animations
                // Usually animation-only files have no mesh
                continue;
            }
        }
        else
        {
            // Could not open file, skip it
			VortigauntLog::LogF("ERROR: Failed to read GR2 file for animation check: %s", checkPath.c_str());
            continue;
        }
        
        gr2AddAnimationItem(fileName, fileInfo.absoluteFilePath());
    }
#endif // ENABLE_GRANNY2
}

void MainWindow::updateOperationComboForFile(const QString& filePath)
{
    int currentIndex = m_operationCombo->currentIndex();
    m_operationCombo->clear();

#ifdef ENABLE_LITHTECH
    m_operationCombo->addItem(tr("Convert LTB "));
#endif
#ifdef ENABLE_GRANNY2
    m_operationCombo->addItem(tr("Convert GR2 "));
#endif
    m_operationCombo->addItem(tr("Extract REZ File"));
    m_operationCombo->addItem(tr("Extract PAK File (Counter Strike Online)"));
    m_operationCombo->addItem(tr("Extract XFS File (Xenesis)"));
    m_operationCombo->addItem(tr("Extract UnityFS / Assets"));
    m_operationCombo->addItem(tr("Extract VPK (Source Engine)"));

    QFileInfo fileInfo(filePath);
    if (filePath.isEmpty() || fileInfo.isDir())
    {
        // Restore selection if valid
        if (currentIndex >= 0 && currentIndex < m_operationCombo->count())
            m_operationCombo->setCurrentIndex(currentIndex);
        return;
    }

    QString ext = grabFileExt(filePath).toLower();
    int selectIndex = -1;

    for (int i = 0; i < m_operationCombo->count(); ++i)
    {
        QString itemText = m_operationCombo->itemText(i).toLower();
        if (ext == "ltb" && itemText.contains("ltb")) {
            selectIndex = i;
            break;
        }
        else if (ext == "gr2" && itemText.contains("gr2")) {
            selectIndex = i;
            break;
        }
        else if (ext == "rez" && itemText.contains("rez")) {
            selectIndex = i;
            break;
        }
        else if (ext == "pak" && itemText.contains("pak")) {
            selectIndex = i;
            break;
        }
        else if (ext == "xfs" && itemText.contains("xfs")) {
            selectIndex = i;
            break;
        }
        else if ((ext == "unity3d" || ext == "bundle" || ext == "assets") && itemText.contains("unity")) {
            selectIndex = i;
            break;
        }
        else if (ext == "vpk" && itemText.contains("vpk")) {
            selectIndex = i;
            break;
        }
    }

    if (selectIndex != -1)
    {
        m_operationCombo->setCurrentIndex(selectIndex);
    }
}

void MainWindow::onBrowseInput()
{
    QString current = m_inputPathEdit->text();
    QString modeText = m_operationCombo->currentText();
    QString filterStr;

    bool isUnityMode = modeText.contains("Unity", Qt::CaseInsensitive) || modeText.contains("Extract UnityFS", Qt::CaseInsensitive);

    if (isUnityMode)
    {
        filterStr = tr("All files (*.*)");
    }
    else
    {
        QString baseExtensions = "*.ltb *.gr2 *.rez *.pak *.mse *.xfs *.unity3d *.bundle *.assets *.vpk";
        filterStr = tr("Supported files (%1);;All files (*.*)").arg(baseExtensions);
    }

    QStringList files = QFileDialog::getOpenFileNames(this, tr("Select Input File(s)"), current, filterStr);
    
    if (!files.isEmpty())
    {
        if (isUnityMode)
        {
            QStringList rejectedFiles;
            QStringList acceptedFiles;
            for (const QString& f : files)
            {
                QFileInfo fi(f);
                QString ext = fi.suffix().toLower();
                if (ext == "pak" || ext == "rez" || ext == "xfs" || ext == "gr2" || ext == "ltb")
                {
                    rejectedFiles << fi.fileName();
                }
                else
                {
                    acceptedFiles << f;
                }
            }

            if (!rejectedFiles.isEmpty())
            {
                QString warnMsg = tr("Seçtiğiniz şu dosyalar bu projenin diğer formatlarına (PAK, REZ, XFS, GR2, LTB) ait olduğu için Unity modunda seçilemez:\n\n");
                for (const QString& name : rejectedFiles)
                {
                    warnMsg += QStringLiteral("  - %1\n").arg(name);
                }
                QMessageBox::warning(this, tr("Uyumsuz Dosya Seçimi"), warnMsg);
                files = acceptedFiles;
            }
        }

        if (files.isEmpty())
        {
            return;
        }

        m_selectedInputFiles = files;
        
        if (files.size() == 1)
        {
            // Single 
            m_inputPathEdit->setText(files.first());
            updateOperationComboForFile(files.first());
        }
        else
        {
            // Multi
            m_inputPathEdit->setText(tr("%1 files selected").arg(files.size()));
            updateOperationComboForFile(files.first());
        }
    }
}

void MainWindow::onBrowseOutput()
{
    QString current = m_outputPathEdit->text();
    QString path = QFileDialog::getExistingDirectory(this, tr("Select Output Folder"), current);
    if (!path.isEmpty())
        m_outputPathEdit->setText(path);
}

void MainWindow::onRun()
{
    m_logEdit->clear();


    const QString inputPath = m_inputPathEdit->text().trimmed();
    if (inputPath.isEmpty())
    {
        QMessageBox::warning(this, tr("Input required"),
                             tr("Please select an input file or folder."));
        return;
    }

    QString outputDir = m_outputPathEdit->text().trimmed();
    if (outputDir.isEmpty())
		outputDir = SettingsDialog::getDefaultOutputDir();
   
    std::error_code ec;
    std::filesystem::create_directories(outputDir.toStdWString(), ec);

    VortigauntLog::Vortigaunt_Printf(QStringLiteral("Output folder: %1").arg(outputDir));
    
    // Capture UI state before spawning thread
    QString modeText = m_operationCombo->currentText();
    QStringList selectedFiles = m_selectedInputFiles;
    if (selectedFiles.isEmpty()) selectedFiles << inputPath;

    // Check if Unity mode is selected but some files belong to other supported formats
    if (modeText.contains("Unity", Qt::CaseInsensitive))
    {
        QStringList ignoredFiles;
        QStringList filteredFiles;
        for (const QString& f : selectedFiles)
        {
            QFileInfo fi(f);
            if (fi.isFile())
            {
                QString ext = fi.suffix().toLower();
                if (ext == "pak" || ext == "rez" || ext == "xfs" || ext == "gr2" || ext == "ltb")
                {
                    ignoredFiles << fi.fileName();
                    continue;
                }
            }
            filteredFiles << f;
        }

        if (!ignoredFiles.isEmpty())
        {
            QString warnMsg = tr("The following files were ignored because they belong to other formats supported by this project (e.g. PAK, REZ, XFS, GR2, LTB):\n\n");
            for (const QString& name : ignoredFiles)
            {
                warnMsg += QStringLiteral("  - %1\n").arg(name);
            }
            QMessageBox::warning(this, tr("Unsupported Formats Ignored for Unity Mode"), warnMsg);
            
            selectedFiles = filteredFiles;
            if (selectedFiles.isEmpty())
            {
                VortigauntLog::Vortigaunt_Printf(QStringLiteral("^1Error:^7 All input files were ignored as they are incompatible with Unity mode."));
                return;
            }
        }
    }
    
    // Capture settings for LTB converter
    ltbConverterSetting ltbEncSettings;

    if (m_ignoreMeshesCheck) ltbEncSettings.IgnoreMeshes = m_ignoreMeshesCheck->isChecked();
    if (m_ignoreAnimsCheck) ltbEncSettings.IgnoreAnimations = m_ignoreAnimsCheck->isChecked();

    // Start background task
    runInBackground([this, modeText, selectedFiles, inputPath, outputDir, ltbEncSettings]() mutable {
        


        // Helper to update progress safely
        auto updateProgress = [this](int percent, const QString& msg = QString()) {
            QMetaObject::invokeMethod(this, [this, percent, msg]() {
                if(m_progressBar) {
                    m_progressBar->setValue(percent);
                    if(!msg.isEmpty()) m_progressBar->setFormat(msg);
                }
            }, Qt::QueuedConnection);
        };
        
        RezExtractor::SetProgressFunc([updateProgress](int p){ updateProgress(p); });
        PakExtractor::SetProgressFunc([updateProgress](int p){ updateProgress(p); });
        XfsExtractor::SetProgressFunc([updateProgress](int p){ updateProgress(p); });

        // When you choose Rez pak xfs at the same time, Vortigaunt was crashing
        // so i did this
        if ((modeText.contains("Multi Process Mode", Qt::CaseInsensitive) || selectedFiles.size() > 1) && !modeText.contains("Unity", Qt::CaseInsensitive))
        {
            QStringList ltbList, gr2List, rezList, pakList, xfsList, vpkList;
            
            for (const QString& f : selectedFiles) {
                QString ext = QFileInfo(f).suffix().toLower();
                if (ext == "rez") rezList << f;
                else if (ext == "pak") pakList << f;
                else if (ext == "gr2") gr2List << f;
                else if (ext == "ltb") ltbList << f;
                else if (ext == "xfs") xfsList << f;
                else if (ext == "vpk") vpkList << f;
            }
            
            if (!ltbList.isEmpty()) {
                for (const QString& ltb : ltbList) {
                    convertLtb(ltb, outputDir, ltbEncSettings); 
                }
            }
            if (!gr2List.isEmpty()) {
                for (const QString& gr2 : gr2List) {
                    convertGr2(gr2, outputDir);
                }
            }
            if (!rezList.isEmpty()) extractArchiveRez(rezList, outputDir);
            if (!pakList.isEmpty()) extractArchivePak(pakList, outputDir);
            if (!xfsList.isEmpty()) extractArchiveXfs(xfsList, outputDir);
            if (!vpkList.isEmpty()) extractArchiveVpk(vpkList, outputDir);
        }
        else if (modeText.contains("LTB", Qt::CaseInsensitive))
        {
            convertLtb(inputPath, outputDir, ltbEncSettings);
        }
        else if (modeText.contains("GR2", Qt::CaseInsensitive))
        {
            convertGr2(inputPath, outputDir);
        }
        else if (modeText.contains("Extract REZ", Qt::CaseInsensitive))
        {
            extractArchiveRez(QStringList{inputPath}, outputDir);
        }
        else if (modeText.contains("Extract PAK", Qt::CaseInsensitive))
        {
            extractArchivePak(QStringList{inputPath}, outputDir);
        }
        else if (modeText.contains("XFS", Qt::CaseInsensitive))
        {
            extractArchiveXfs(QStringList{inputPath}, outputDir);
        }
        else if (modeText.contains("Unity", Qt::CaseInsensitive) || modeText.contains("Extract UnityFS", Qt::CaseInsensitive))
        {
            // UnityFS extraction and processing
            UnityPorter porter;
            for (const QString& f : selectedFiles)
            {
                porter.Process(f.toStdString(), outputDir.toStdString());
            }
        }
        else if (modeText.contains("VPK", Qt::CaseInsensitive))
        {
            extractArchiveVpk(QStringList{inputPath}, outputDir);
        }

        // Cleanup callbacks
        RezExtractor::SetProgressFunc(nullptr);
        PakExtractor::SetProgressFunc(nullptr);
        XfsExtractor::SetProgressFunc(nullptr);
        VpkExtractor::SetProgressFunc(nullptr);
        

        QMetaObject::invokeMethod(this, [this]() {
            if (!m_extractionErrors.empty())
            {
                QString errorMsg = tr("Operation completed with %1 error(s).\n\n").arg(m_extractionErrors.size());
                errorMsg += tr("Failed files:\n");
                for (size_t i = 0; i < std::min<size_t>(10, m_extractionErrors.size()); ++i)
                {
                    errorMsg += QStringLiteral("  - %1: %2\n")
                        .arg(m_extractionErrors[i].filename)
                        .arg(m_extractionErrors[i].errorMessage);
                }
                if (m_extractionErrors.size() > 10)
                {
                    errorMsg += tr("  ... and %1 more.\n").arg(m_extractionErrors.size() - 10);
                }
                QMessageBox::warning(this, tr("Extraction Complete with Errors"), errorMsg);
                m_extractionErrors.clear();
            }
        }, Qt::QueuedConnection);

    });
}

void MainWindow::onOpenDtxViewer()
{
    DtxViewerDialog dlg(this);
    dlg.exec();
}

void MainWindow::onOpenPakViewer()
{
    PakViewerWindow dlg(this);
    dlg.exec();
}

void MainWindow::onOpenSettings()
{
    SettingsDialog dlg(this);
    dlg.exec();
}

void MainWindow::onOpenRezViewer()
{
    RezViewerWindow dlg(this);
    dlg.exec();

}

void MainWindow::onOpenXfsViewer()
{
    XfsViewerWindow dlg(this);
    dlg.exec();
}

void MainWindow::onOpenSpriteViewer()
{
    SpriteViewerWindow dlg(this);
    dlg.exec();
}

void MainWindow::onOpenLithtechSpriteViewer()
{
    auto* sprViewer = new SpriteViewerWindow(this);
    sprViewer->setAttribute(Qt::WA_DeleteOnClose);
    sprViewer->show();
}

void MainWindow::onOpenLoLModelDownloader()
{
    LoLModelDownloadDialog* qtDialog = new LoLModelDownloadDialog(nullptr);
    qtDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    qtDialog->setModal(false); // other windows stay open cuz non-modal
    qtDialog->show();
    qtDialog->raise();
    qtDialog->activateWindow();
}
#ifdef METIN2_SCRIPT_EFFECT

void MainWindow::onOpenMseViewer()
{
    MseViewerWindow* qtWindow = new MseViewerWindow(this);
    qtWindow->setAttribute(Qt::WA_DeleteOnClose, true);
    qtWindow->setModal(false);
    qtWindow->show();
    qtWindow->raise();
    qtWindow->activateWindow();

}
#endif

void MainWindow::onOpenAutoRigDialog()
{
    AutoRigDialog* qtDialog = new AutoRigDialog(this);
    qtDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    qtDialog->setModal(false);
    qtDialog->show();
    qtDialog->raise();
    qtDialog->activateWindow();
}

void MainWindow::onOpenVpkViewer()
{
    VpkViewerWindow* qtDialog = new VpkViewerWindow(this);
    qtDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    qtDialog->setModal(false);
    qtDialog->show();
    qtDialog->raise();
    qtDialog->activateWindow();
}

void MainWindow::onOpenWadMaker()
{
    WadMakerDialog* qtDialog = new WadMakerDialog(this);
    qtDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    qtDialog->setModal(false);
    qtDialog->show();
    qtDialog->raise();
    qtDialog->activateWindow();
}

void MainWindow::onOpenAudioConverter()
{
    AudioConvertDialog* qtDialog = new AudioConvertDialog(this);
    qtDialog->setAttribute(Qt::WA_DeleteOnClose, true);
    qtDialog->setModal(false);
    qtDialog->show();
    qtDialog->raise();
    qtDialog->activateWindow();
}

void MainWindow::convertLtb(const QString& inputPath, const QString& outputDir, const ltbConverterSetting& settings)
{
    std::vector<std::filesystem::path> files;

    std::filesystem::path start = std::filesystem::path(inputPath.toStdWString());
    recurseAndCollectFiles(start, files);

    if (files.empty())
    {
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("No .ltb files found in '%1'.").arg(inputPath));
        return;
    }

    ltbConverter ltbConverter;
    ltbConverter.SetConvertSetting(settings);

    for (const auto& p : files)
    {
        const QString inFile = QString::fromStdWString(p.wstring());
        const QString ext = grabFileExt(inFile);

        if (ext == "ltb")
        {
            QString outFile = replaceFileExt(inFile, "smd");
            if (!outputDir.isEmpty())
            {
                QFileInfo fi(inFile);
                QString baseName = fi.completeBaseName();
                QString targetFolder = QDir(outputDir).filePath(baseName);
                
                std::error_code ec;
                std::filesystem::create_directories(targetFolder.toStdWString(), ec);
                
                outFile = QDir(targetFolder).filePath(baseName + ".smd");
            }

            VortigauntLog::Vortigaunt_Printf(QStringLiteral("Converting LTB..."));

            int ret = ltbConverter.ConvertSingleLTBFile(inFile.toStdString(),
                                                        outFile.toStdString());
            if (ret == CONVERT_RET_OK)
            {
                VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2OK"));

                // List the actual generated SMD files
                QFileInfo outInfo(outFile);
                QDir outDir(outInfo.absolutePath());
                const QString baseName = outInfo.completeBaseName();

                QStringList filters;
                filters << QStringLiteral("%1_*.smd").arg(baseName);
                const QFileInfoList files =
                    outDir.entryInfoList(filters, QDir::Files, QDir::Name);

                if (!files.isEmpty())
                {
                    VortigauntLog::Vortigaunt_Printf(QStringLiteral("DEBUG:   ^5SMD files:^7"));
                    for (const QFileInfo& fi : files)
                    {
                        VortigauntLog::Vortigaunt_Printf(QStringLiteral("DEBUG:     ^4%1^7").arg(fi.fileName()));
                    }
                }
                else
                {
                    VortigauntLog::Vortigaunt_Printf(QStringLiteral("  ^3No SMD files were generated."));
                }

                // Generate QC file if checkbox is checked
                if (m_writeQCCheck->isChecked())
                {
                    QCFile qcWriter;
                    QCFileSettings qcSettings;

                    qcSettings.ModelName = baseName.toStdString() + ".mdl";

                    qcSettings.MeshName = (baseName + "_reference").toStdString();
                    qcSettings.Scale = 1.0f;
                    qcSettings.Fps = 30;

                    qcWriter.SetSettings(qcSettings);

                    // Add sequences 
                    QStringList animFilters;
                    animFilters << QStringLiteral("%1_anim_*.smd").arg(baseName);
                    const QFileInfoList animFiles = outDir.entryInfoList(animFilters, QDir::Files, QDir::Name);

                    if (!animFiles.isEmpty())
                    {
                        for (const QFileInfo& fi : animFiles)
                        {
                            QString animFileName = fi.completeBaseName();
                            QString prefix = baseName + "_anim_";
                            QString seqName = animFileName;
                            if (seqName.startsWith(prefix)) {
                                seqName = seqName.mid(prefix.length());
                            }

                            qcWriter.AddSequence(seqName.toStdString(), animFileName.toStdString(), 30);
                        }
                    }
                    else
                    {
                        // No animations
                        qcWriter.AddSequence("idle", qcSettings.MeshName, 30);
                    }

                    // Write QC file
                    std::filesystem::path qcPath = outInfo.absoluteFilePath().toStdWString();
                    qcPath.replace_extension(".qc");

                    if (qcWriter.Write(String_UTF16toUTF8(qcPath.generic_u16string())))
                    {
                        VortigauntLog::Vortigaunt_Printf(QStringLiteral("  QC file created: %1").arg(QString::fromStdWString(qcPath.generic_wstring())));
                    }
                    else
                    {
                        VortigauntLog::Vortigaunt_Printf(QStringLiteral("  ^1Failed to create QC file:^7 %1").arg(QString::fromStdString(qcWriter.GetError())));
                    }
                }
            }
            else
                VortigauntLog::Vortigaunt_Printf(QStringLiteral("ERROR:   FAILED (code %1)").arg(ret));
        }
    }
}

void MainWindow::extractArchiveRez(const QStringList& paths, const QString& outputDir)
{
    if (paths.isEmpty())
    {
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("WARNING: No REZ files selected."));
        return;
    }

    // Clear previous errors
    m_extractionErrors.clear();

    // Boost process priority for faster extraction (Windows only)
    Platform::setHighPriority();

    const int totalFiles = paths.size();
    const bool isBatch = totalFiles > 1;

    if (isBatch)
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("^5Extracting^7 ^3%1^7 ^5REZ files...^7").arg(totalFiles));

    // Show initial count removed as requested

    // Set up progress callback
    int lastProcessedPercent = -1;
    RezExtractor::SetProgressFunc([this, &lastProcessedPercent](int percent) {
        if (percent != lastProcessedPercent)
        {
            lastProcessedPercent = percent;
            updateProgressSafe(percent);
        }
    });

    int successCount = 0;

    for (int i = 0; i < totalFiles; ++i)
    {
        const QString& rezPath = paths.at(i);

        // Update progress bar with file-level percentage
        if (isBatch)
        {
            int filePercent = ((i + 1) * 100) / totalFiles;
            QMetaObject::invokeMethod(this, [this, filePercent, i, totalFiles]() {
                if (m_progressBar) {
                    m_progressBar->setValue(filePercent);
                    m_progressBar->setFormat(tr("REZ %1/%2: %p%").arg(i + 1).arg(totalFiles));
                }
            }, Qt::QueuedConnection);
        }

        // Validate extension
        if (grabFileExt(rezPath) != "rez")
        {
            VortigauntLog::Vortigaunt_Printf(QStringLiteral("WARNING: Skipped (not a .rez file): %1").arg(rezPath));
            ExtractionError err;
            err.filename = rezPath;
            err.errorMessage = tr("Invalid file type");
            m_extractionErrors.push_back(err);
            continue;
        }

        if (!QFileInfo::exists(rezPath))
        {
            VortigauntLog::Vortigaunt_Printf(QStringLiteral("WARNING: Skipped (file not found): %1").arg(rezPath));
            continue;
        }

        // Determine output directory (separate folders for batch mode)
        QString targetDir;
        if (isBatch && m_separateFoldersCheck->isChecked())
        {
            QFileInfo fileInfo(rezPath);
            targetDir = QDir(outputDir).filePath(fileInfo.completeBaseName());
        }
        else
        {
            targetDir = outputDir.isEmpty() ? SettingsDialog::getExtractedOutputDir() : outputDir;
        }

        VortigauntLog::Vortigaunt_Printf(QStringLiteral("^5Extracting REZ:^7 ^4%1^7 -> ^4%2^7").arg(rezPath, targetDir));
        updateProgressSafe(0);

        RezExtractor rezExtractor;
        bool loadSuccess = rezExtractor.Load(rezPath.toStdString());

        if (loadSuccess)
        {
            bool rezExtracted = rezExtractor.ExtractAll(targetDir.toStdString());

            if (rezExtracted)
            {
                VortigauntLog::Vortigaunt_Printf(QStringLiteral("  ^2REZ extraction successful.^7"));
                ++successCount;
            }
            else
            {
                VortigauntLog::Vortigaunt_Printf(QStringLiteral("ERROR:   REZ extraction failed."));
                ExtractionError err;
                err.filename = rezPath;
                err.errorMessage = tr("Extraction failed");
                m_extractionErrors.push_back(err);
            }
        }
        else
        {
            VortigauntLog::Vortigaunt_Printf(QStringLiteral("ERROR:   Failed to load REZ file."));
        }

    }

    // Clear callbacks
    RezExtractor::SetProgressFunc(RezExtractor::ProgressFunc());

    // Restore normal priority
    Platform::restoreNormalPriority();

    if (isBatch)
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2Extraction complete.^7 ^3%1^7 of ^3%2^7 files processed.").arg(successCount).arg(totalFiles));
}

void MainWindow::extractArchivePak(const QStringList& paths, const QString& outputDir)
{
    if (paths.isEmpty())
    {
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("WARNING: No PAK files selected."));
        return;
    }

    Platform::setHighPriority();

    const int totalFiles = paths.size();
    const bool isBatch = totalFiles > 1;
    int successCount = 0;

    PakExtractor pakExtractor;

    PakExtractor::SetProgressFunc([this](int percent) {
        updateProgressSafe(percent);
    });

    for (int i = 0; i < totalFiles; ++i)
    {
        const QString& pakPath = paths.at(i);

        if (isBatch)
        {
            int filePercent = ((i + 1) * 100) / totalFiles;
            QMetaObject::invokeMethod(this, [this, filePercent, i, totalFiles]() {
                if (m_progressBar) {
                    m_progressBar->setValue(filePercent);
                    m_progressBar->setFormat(tr("PAK %1/%2: %p%").arg(i + 1).arg(totalFiles));
                }
            }, Qt::QueuedConnection);
        }

        // Determine output directory
        QString targetDir;
        if (isBatch && m_separateFoldersCheck->isChecked())
            targetDir = QDir(outputDir).filePath(QFileInfo(pakPath).completeBaseName());
        else
            targetDir = outputDir.isEmpty() ? SettingsDialog::getExtractedOutputDir() : outputDir;

        VortigauntLog::Vortigaunt_Printf(QStringLiteral("^5Extracting PAK:^7 ^4%1^7 -> ^4%2^7").arg(pakPath, targetDir));

        if (pakExtractor.ExtractSingle(pakPath.toStdString(), targetDir.toStdString())) {
            ++successCount;
        } else {
            ExtractionError err;
            err.filename = pakPath;
            err.errorMessage = tr("Pak extraction failed or was incomplete");
            m_extractionErrors.push_back(err);
        }
    }

    PakExtractor::SetProgressFunc(nullptr);
    Platform::restoreNormalPriority();

    VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2PAK extraction complete.^7 ^3%1^7 of ^3%2^7 files processed.").arg(successCount).arg(totalFiles));
}

void MainWindow::extractArchiveXfs(const QStringList& paths, const QString& outputDir)
{
    if (paths.isEmpty())
    {
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("WARNING: No XFS files selected."));
        return;
    }

    Platform::setHighPriority();

    const int totalFiles = paths.size();
    const bool isBatch = totalFiles > 1;
    int successCount = 0;

    // Set up progress callback
    int lastProcessedPercent = -1;
    XfsExtractor::SetProgressFunc([this, &lastProcessedPercent](int percent) {
        if (percent != lastProcessedPercent)
        {
            lastProcessedPercent = percent;
            updateProgressSafe(percent);

            //QMetaObject::invokeMethod(this, [this, percent]() {
            //    if (m_statusLabel) m_statusLabel->setText(tr("Extracting XFS: %1%").arg(percent));
            //}, Qt::QueuedConnection);
        }
    });

    // Removed status label update as requested

    for (int i = 0; i < totalFiles; ++i)
    {
        const QString& xfsPath = paths.at(i);

        if (isBatch)
        {
            int filePercent = ((i + 1) * 100) / totalFiles;
            QMetaObject::invokeMethod(this, [this, filePercent, i, totalFiles]() {
                if (m_progressBar) {
                    m_progressBar->setValue(filePercent);
                    m_progressBar->setFormat(tr("XFS %1/%2: %p%").arg(i + 1).arg(totalFiles));
                }
            }, Qt::QueuedConnection);

            QMetaObject::invokeMethod(this, [this, i, totalFiles, xfsPath]() {
                if (m_statusLabel)
                    m_statusLabel->setText(tr("Extracting XFS %1/%2: %3").arg(i + 1).arg(totalFiles).arg(QFileInfo(xfsPath).fileName()));
            }, Qt::QueuedConnection);
        }

        // Validate extension
        if (grabFileExt(xfsPath) != "xfs")
        {
            VortigauntLog::Vortigaunt_Printf(QStringLiteral("ERROR: Input is not a .xfs file: %1").arg(xfsPath));
            continue;
        }

        // Determine output directory
        QString targetDir;
        if (isBatch && m_separateFoldersCheck->isChecked())
            targetDir = QDir(outputDir).filePath(QFileInfo(xfsPath).completeBaseName());
        else
            targetDir = outputDir.isEmpty() ? SettingsDialog::getExtractedOutputDir() : outputDir;

        VortigauntLog::Vortigaunt_Printf(QStringLiteral("^5Extracting XFS:^7 ^4%1^7 -> ^4%2^7").arg(xfsPath, targetDir));

        XfsExtractor xfsExtractor;
        bool loadSuccess = xfsExtractor.Load(xfsPath.toStdString());

        if (loadSuccess)
        {
            VortigauntLog::Vortigaunt_Printf(QStringLiteral("  Found %1 files in archive").arg(xfsExtractor.GetEntries().size()));

            bool xfsExtracted = xfsExtractor.ExtractAll(targetDir.toStdString());

            if (xfsExtracted)
            {
                VortigauntLog::Vortigaunt_Printf(QStringLiteral("  ^2XFS extraction successful.^7"));
                ++successCount;
            }
            else
            {
                VortigauntLog::Vortigaunt_Printf(QStringLiteral("ERROR:   XFS extraction failed."));
            }
        }
        else
        {
            VortigauntLog::Vortigaunt_Printf(QStringLiteral("ERROR:   Failed to load XFS file."));
        }

    }

    // Clear callbacks
    XfsExtractor::SetProgressFunc(XfsExtractor::ProgressFunc());

    Platform::restoreNormalPriority();

    if (isBatch)
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2XFS extraction complete.^7 ^3%1^7 of ^3%2^7 files processed.").arg(successCount).arg(totalFiles));
}

void MainWindow::extractArchiveVpk(const QStringList& paths, const QString& outputDir)
{
    if (paths.isEmpty())
    {
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("WARNING: No VPK files selected."));
        return;
    }

    Platform::setHighPriority();

    const int totalFiles = paths.size();
    const bool isBatch = totalFiles > 1;
    int successCount = 0;

    VpkExtractor vpkExtractor;

    VpkExtractor::SetProgressFunc([this](int percent) {
        updateProgressSafe(percent);
    });

    for (int i = 0; i < totalFiles; ++i)
    {
        const QString& vpkPath = paths.at(i);

        if (isBatch)
        {
            int filePercent = ((i + 1) * 100) / totalFiles;
            QMetaObject::invokeMethod(this, [this, filePercent, i, totalFiles]() {
                if (m_progressBar) {
                    m_progressBar->setValue(filePercent);
                    m_progressBar->setFormat(tr("VPK %1/%2: %p%").arg(i + 1).arg(totalFiles));
                }
            }, Qt::QueuedConnection);
        }

        // Determine output directory
        QString targetDir;
        if (isBatch && m_separateFoldersCheck->isChecked())
            targetDir = QDir(outputDir).filePath(QFileInfo(vpkPath).completeBaseName());
        else
            targetDir = outputDir.isEmpty() ? SettingsDialog::getExtractedOutputDir() : outputDir;

        VortigauntLog::Vortigaunt_Printf(QStringLiteral("^5Extracting VPK:^7 ^4%1^7 -> ^4%2^7").arg(vpkPath, targetDir));

        if (vpkExtractor.ExtractSingle(vpkPath.toStdString(), targetDir.toStdString())) {
            ++successCount;
        } else {
            ExtractionError err;
            err.filename = vpkPath;
            err.errorMessage = tr("VPK extraction failed or was incomplete");
            m_extractionErrors.push_back(err);
        }
    }

    VpkExtractor::SetProgressFunc(nullptr);
    Platform::restoreNormalPriority();

    VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2VPK extraction complete.^7 ^3%1^7 of ^3%2^7 files processed.").arg(successCount).arg(totalFiles));
}







void MainWindow::convertGr2(const QString& inputPath, const QString& outputDir)
{
    std::vector<std::filesystem::path> files;

    std::filesystem::path start = std::filesystem::path(inputPath.toStdWString());
    
    // Check if input is a file or directory
    if (std::filesystem::is_regular_file(start))
    {
        files.push_back(start);
    }
    else if (std::filesystem::is_directory(start))
    {
        // Collect all .gr2 files recursively
        for (auto const& entry : std::filesystem::recursive_directory_iterator(start))
        {
            if (!entry.is_regular_file())
                continue;
            const auto& p = entry.path();
            auto ext = p.extension().string();
            if (ext == ".gr2" || ext == ".GR2")
            {
                files.push_back(p);
            }
        }
    }
    else
    {
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("^1Error:^7 Invalid input path: ^4%1^7").arg(inputPath));
        return;
    }

    if (files.empty())
    {
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("^3Warning:^7 No .gr2 files found in '^4%1^7'.").arg(inputPath));
        return;
    }

    Gr2Converter gr2Converter;
    Gr2ConverterSettings settings;
    
    // Get options from checkboxes
    settings.ExportMeshes = !m_ignoreMeshesCheck->isChecked();
    settings.ExportAnimations = !m_ignoreAnimsCheck->isChecked();
    settings.ExportSkeleton = true;
    settings.ExportMaterials = true;
    settings.MirrorUVY = SettingsDialog::getMirrorUVY();
    settings.InvertAlpha = m_invertAlphaCheck->isChecked();
    
    gr2Converter.SetConverterSettings(settings);
    


    size_t gr2OkCount = 0;
    size_t gr2FailCount = 0;

    for (const auto& gr2File : files)
    {
        // Get output file name for logging
        QString inputFileName = QString::fromStdWString(gr2File.filename().wstring());
        QString outputFileName = inputFileName;
        outputFileName.replace(".gr2", ".smd", Qt::CaseInsensitive);
        outputFileName.replace(".GR2", ".smd", Qt::CaseInsensitive);

        // Determine output file path - create model subfolder based on model name
        std::filesystem::path outPath = std::filesystem::path(outputDir.toStdWString());
        
        // Get model name (filename without extension)
        std::filesystem::path modelName = gr2File.stem();
        
        // Check if input is a single file or directory
        bool isSingleFile = std::filesystem::is_regular_file(start) && files.size() == 1;
        
        if (isSingleFile)
        {
            // Single file mode - create subfolder with model name
            // Example: hanma_boss.gr2 -> VortigauntOutput/hanma_boss/hanma_boss.smd
            outPath /= modelName;  // Create model subfolder
            outPath /= gr2File.filename();  // Add filename inside subfolder
        }
        else
        {
            // Folder mode - preserve directory structure but add model subfolder
            std::filesystem::path relPath = std::filesystem::relative(gr2File, start);
            if (!relPath.empty())
            {
                // Get relative parent path and add model subfolder
                std::filesystem::path relParent = relPath.parent_path();
                if (!relParent.empty())
                {
                    outPath /= relParent;
                }
                outPath /= modelName;  // Create model subfolder
                outPath /= gr2File.filename();  // Add filename
            }
            else
            {
                // Fallback: create model subfolder
                outPath /= modelName;
                outPath /= gr2File.filename();
            }
        }
        
        // Get output format from combo
        QString outputFormat = m_outputFormatCombo->currentData().toString();
        
        outPath.replace_extension(".smd");
        
        
        // Create parent directories (including model subfolder)
        std::error_code ec;
        std::filesystem::create_directories(outPath.parent_path(), ec);

        QString qOutFile = QString::fromStdWString(outPath.generic_wstring());
        
        // Export to SMD format
        int result = gr2Converter.ConvertSingleGr2File(
            String_UTF16toUTF8(gr2File.generic_u16string()),
            String_UTF16toUTF8(outPath.generic_u16string())
        );
        
        if (result == GR2_CONVERT_RET_OK)
        {
            VortigauntLog::Vortigaunt_Printf(QStringLiteral("Exporting SMD: ^4%1^7").arg(outputFileName));
            ++gr2OkCount;
        }
        else
        {
            QString errorMsg;
            switch (result)
            {
                case GR2_CONVERT_RET_INVALID_INPUT_FILE:
                    errorMsg = "Invalid input file";
                    break;
                case GR2_CONVERT_RET_LOAD_FAILED:
                    errorMsg = "Failed to load GR2 file";
                    break;
                case GR2_CONVERT_RET_NO_MESH:
                    errorMsg = "No mesh found in GR2 file";
                    break;
                case GR2_CONVERT_RET_EXPORT_FAILED:
                    errorMsg = "Export failed";
                    break;
                default:
                    errorMsg = QString("Unknown error (code: %1)").arg(result);
                    break;
            }
            VortigauntLog::Vortigaunt_Printf(QStringLiteral("  ^1FAILED:^7 %1").arg(errorMsg));
            ++gr2FailCount;
        }
        
        // Export animations (only for single file mode)
        if (isSingleFile && m_gr2AnimGroup->isVisible())
        {
            
            // Create animation path: {filename}_anims
            std::filesystem::path animDir = outPath.parent_path();
            QString baseName = QString::fromStdWString(gr2File.stem().generic_wstring());
            animDir /= (baseName + "_anims").toStdWString();
            
            std::error_code ec;
            std::filesystem::create_directories(animDir, ec);
            
            // Check if user has selected animation files
            QList<QListWidgetItem*> selectedItems = m_animationListWidget->selectedItems();
            
            // If nothing is selected, export all items in the list
            if (selectedItems.isEmpty() && m_animationListWidget->count() > 0)
            {
                // Select all items programmatically so the selection is visible
                m_animationListWidget->selectAll();
                selectedItems = m_animationListWidget->selectedItems();
            }
            
            bool hasSelectedFiles = !selectedItems.isEmpty();
            bool hasBrowsedFiles = false;
            
            // Check if any selected item has a file path (browsed files or loaded from directory)
            for (QListWidgetItem* item : selectedItems)
            {
                QString filePath = item->data(Qt::UserRole).toString();
                if (!filePath.isEmpty())
                {
                    hasBrowsedFiles = true;
                    break;
                }
            }
            
            // If no browsed files but items are selected, they might be from GR2 file
            // In that case, we need to find the corresponding GR2 animation files
            if (!hasBrowsedFiles && !selectedItems.isEmpty())
            {
                // Selected items are animation names from GR2, but we need to find the actual GR2 files
                // The animation names might correspond to GR2 files in the same directory
                QString inputDir = QFileInfo(m_inputPathEdit->text()).absolutePath();
                for (QListWidgetItem* item : selectedItems)
                {
                    QString animName = item->text();
                    // Try to find a GR2 file with this name in the input directory
                    QString possibleGr2File = inputDir + "/" + animName;
                    if (animName.endsWith(".gr2", Qt::CaseInsensitive))
                    {
                        // Already has .gr2 extension
                        if (QFileInfo(possibleGr2File).exists())
                        {
                            item->setData(Qt::UserRole, possibleGr2File);
                            hasSelectedFiles = true;
                        }
                    }
                    else
                    {
                        // Try with .gr2 extension
                        QString withExt = possibleGr2File + ".gr2";
                        if (QFileInfo(withExt).exists())
                        {
                            item->setData(Qt::UserRole, withExt);
                            hasSelectedFiles = true;
                        }
                    }
                }
            }
            
            if (hasSelectedFiles)
            {
                // Export selected animation files (browsed files)
                for (QListWidgetItem* item : selectedItems)
                {
                    QString filePath = item->data(Qt::UserRole).toString();
                    if (!filePath.isEmpty())
                    {
                        QFileInfo fileInfo(filePath);
                        QString fileName = fileInfo.fileName();
                        QString baseFileName = fileInfo.baseName();
                        
                        // Create output path for animation
                        std::filesystem::path animOutPath = animDir;
                        QString safeFileName = baseFileName;
                        // Sanitize filename
                        for (QChar& c : safeFileName)
                        {
                            if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
                                c = '_';
                        }
                        animOutPath /= (safeFileName + ".smd").toStdWString();
                        
                        // Convert file based on extension
                        if (fileInfo.suffix().toLower() == "smd")
                        {
                            // Copy SMD file
                            try {
                                std::filesystem::copy_file(filePath.toStdWString(), animOutPath, std::filesystem::copy_options::overwrite_existing);
                                VortigauntLog::Vortigaunt_Printf(QStringLiteral("DEBUG:   ? Animation copied -> %1").arg(QString::fromStdWString(animOutPath.generic_wstring())));
                            } catch (const std::exception& e) {
                                VortigauntLog::Vortigaunt_Printf(QStringLiteral("  ? Failed to copy animation: %1").arg(fileName));
                            }
                        }
                        else if (fileInfo.suffix().toLower() == "gr2")
                        {
                            // Convert GR2 file to SMD (animation only)
                            
                            // Create a new converter for this animation file
                            Gr2Converter animConverter;
                            Gr2ConverterSettings animSettings;
                            animSettings.ExportMeshes = false; // Only export skeleton for animation
                            animSettings.ExportAnimations = true;
                            animSettings.ExportSkeleton = true;
                            animSettings.ExportMaterials = false;
                            animSettings.MirrorUVY = m_mirrorUVYCheck->isChecked();
                            animConverter.SetConverterSettings(animSettings);
                            

                            
                            // Pass main GR2 file path as skeleton source
                            QString mainGr2Path = m_inputPathEdit->text();
                            
                            // Find the correct animation index for this animation name
                            int animIndex = 0;
                            std::vector<std::string> animNames = animConverter.GetAnimationNames(filePath.toUtf8().toStdString());
                            QString animNameFromList = item->text();
                            for (size_t i = 0; i < animNames.size(); ++i)
                            {
                                if (QString::fromStdString(animNames[i]) == animNameFromList)
                                {
                                    animIndex = static_cast<int>(i);
                                    break;
                                }
                            }

                            
                            int animResult = animConverter.ConvertGr2Animation(
                                filePath.toUtf8().toStdString(), 
                                String_UTF16toUTF8(animOutPath.generic_u16string()), 
                                animIndex,
                                mainGr2Path.toUtf8().toStdString()); // Pass skeleton source

                            if (animResult == GR2_CONVERT_RET_OK)
                            {
                                // Use the actual output filename for logging
                                QString smdFileName = QString::fromStdWString(animOutPath.filename().generic_wstring());
                                VortigauntLog::Vortigaunt_Printf(QStringLiteral("  Exporting Animation: %1").arg(smdFileName));
                            }
                            else
                            {
                                VortigauntLog::Vortigaunt_Printf(QStringLiteral("  %1 -> FAILED").arg(fileName));
                            }
                        }
                        else
                        {
                            VortigauntLog::Vortigaunt_Printf(QStringLiteral("  Skipping unsupported file format: %1").arg(fileName));
                        }
                    }
                }
            }
            else
            {
                // Export all animations from GR2 file
                std::vector<std::string> animNames = gr2Converter.GetAnimationNames(String_UTF16toUTF8(gr2File.generic_u16string()));
                
                for (size_t i = 0; i < animNames.size(); ++i)
                {
                    QString animName = QString::fromStdString(animNames[i]);
                    
                    // Create output path for animation
                    std::filesystem::path animOutPath = animDir;
                    QString safeAnimName = animName;
                    // Sanitize filename
                    for (QChar& c : safeAnimName)
                    {
                        if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
                            c = '_';
                    }
                    animOutPath /= (safeAnimName + ".smd").toStdWString();
                    
                    int animResult = gr2Converter.ConvertGr2Animation(String_UTF16toUTF8(gr2File.generic_u16string()), String_UTF16toUTF8(animOutPath.generic_u16string()), static_cast<int>(i));
                    
                    if (animResult == GR2_CONVERT_RET_OK)
                    {
                        // Use the actual output filename for logging
                        QString smdFileName = QString::fromStdWString(animOutPath.filename().generic_wstring());
                        VortigauntLog::Vortigaunt_Printf(QStringLiteral("  Exporting Animation: %1").arg(smdFileName));
                    }
                    else
                    {
                        VortigauntLog::Vortigaunt_Printf(QStringLiteral("  %1 -> FAILED").arg(animName));
                    }
                }
            }
            
            // Generate QC file if checkbox is checked
            if (m_writeQCCheck->isChecked() && result == GR2_CONVERT_RET_OK)
            {
                QCFile qcWriter;
                QCFileSettings qcSettings;
                
                // Model name: basename.mdl
                qcSettings.ModelName = String_UTF16toUTF8(gr2File.stem().generic_u16string()) + ".mdl";
                
                // Mesh SMD name (without extension)
                qcSettings.MeshName = String_UTF16toUTF8(gr2File.stem().generic_u16string());
                qcSettings.Scale = 1.0f;
                qcSettings.Fps = 30;
                
                qcWriter.SetSettings(qcSettings);
                
                // Add sequences from exported animations
                // Get animation directory relative path
                QString baseName = QString::fromStdWString(gr2File.stem().generic_wstring());
                std::string animRelDir = (baseName + "_anims").toStdString();
                
                // First, try to get animations from the animation list widget
                // These are the external animation GR2 files user has added
                bool hasAnimationsFromList = false;
                if (m_animationListWidget->count() > 0)
                {
                    for (int i = 0; i < m_animationListWidget->count(); ++i)
                    {
                        QListWidgetItem* item = m_animationListWidget->item(i);
                        if (item)
                        {
                            QString filePath = item->data(Qt::UserRole).toString();
                            QString itemText = item->text();
                            
                            // Get animation name from file path or item text
                            QString animName;
                            if (!filePath.isEmpty())
                            {
                                // From file path (external GR2 file)
                                QFileInfo fileInfo(filePath);
                                animName = fileInfo.baseName();
                            }
                            else
                            {
                                // From item text (internal animation name)
                                animName = itemText;
                            }
                            
                            // Sanitize animation name
                            std::string safeAnimName = animName.toStdString();
                            for (char& c : safeAnimName)
                            {
                                if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
                                    c = '_';
                            }
                            
                            // Build relative path to animation SMD
                            std::string animSmdPath = animRelDir + "\\" + safeAnimName;
                            qcWriter.AddSequence(safeAnimName, animSmdPath, 30);
                            hasAnimationsFromList = true;
                        }
                    }
                }
                
                // If no animations from list, try to get from GR2 file internal animations
                if (!hasAnimationsFromList)
                {
                    std::vector<std::string> animNames = gr2Converter.GetAnimationNames(String_UTF16toUTF8(gr2File.generic_u16string()));
                    
                    if (!animNames.empty())
                    {
                        for (const auto& animName : animNames)
                        {
                            // Sanitize animation name
                            std::string safeAnimName = animName;
                            for (char& c : safeAnimName)
                            {
                                if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
                                    c = '_';
                            }
                            
                            // Build relative path to animation SMD
                            std::string animSmdPath = animRelDir + "\\" + safeAnimName;
                            qcWriter.AddSequence(safeAnimName, animSmdPath, 30);
                        }
                    }
                    else
                    {
                        // No animations at all - add a default idle sequence using the mesh
                        qcWriter.AddSequence("idle", qcSettings.MeshName, 30);
                    }
                }
                
                // Write QC file
                std::filesystem::path qcPath = outPath;
                qcPath.replace_extension(".qc");
                
                if (qcWriter.Write(String_UTF16toUTF8(qcPath.generic_u16string())))
                {
                    VortigauntLog::Vortigaunt_Printf(QStringLiteral("  QC file created: %1").arg(QString::fromStdWString(qcPath.generic_wstring())));
                }
                else
                {
                    VortigauntLog::Vortigaunt_Printf(QStringLiteral("  Failed to create QC file: %1").arg(QString::fromStdString(qcWriter.GetError())));
                }
            }
        }
    }
    

	VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2Granny files Converted to %1.").arg(outputDir));
    //VortigauntLog::LogF(("Convert finished. GR2 files converted to %s."));
    if (gr2FailCount > 0)
    {
        VortigauntLog::Vortigaunt_Printf(QStringLiteral(" Granny file Convert Failed: %1").arg(gr2FailCount));
    }
}





// Drag & Drop support
void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event->mimeData()->hasUrls())
    {
        event->acceptProposedAction();
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    const QMimeData* mimeData = event->mimeData();
    if (mimeData->hasUrls())
    {
        QList<QUrl> urlList = mimeData->urls();
        handleDroppedFiles(urlList);
        event->acceptProposedAction();
    }
}

void MainWindow::handleDroppedFiles(const QList<QUrl>& urls)
{
    if (urls.isEmpty())
        return;
    
    // Take the first file/folder
    QString localPath = urls.first().toLocalFile();
    if (localPath.isEmpty())
        return;
    
    QFileInfo fileInfo(localPath);
    
    // Set input path
    m_inputPathEdit->setText(localPath);
    
    // Auto-update operation combo
    updateOperationComboForFile(localPath);
    
}

// Status bar
void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel(QString("Version: %1b").arg(VORTIGAUNT_VERSION_STRING));
    m_statusLabel->setStyleSheet("QLabel { padding: 0 6px; font-size: 11px; }");
    statusBar()->addWidget(m_statusLabel);
    
    // Progress bar
    m_progressBar = new QProgressBar();
    m_progressBar->setVisible(false);
    m_progressBar->setMinimum(0);
    m_progressBar->setMaximum(100);
    m_progressBar->setTextVisible(true);
    m_progressBar->setFormat(tr("%p%"));
    m_progressBar->setMinimumWidth(200);
    m_progressBar->setFixedHeight(16);
    statusBar()->addPermanentWidget(m_progressBar);
    
    statusBar()->setStyleSheet(
        "QStatusBar { border: none; background: transparent; }"
        "QStatusBar::item { border: none; }"
    );
}


void MainWindow::setupKeyboardShortcuts()
{
    // Ctrl+O: Browse Input
    m_shortcutBrowseInput = new QShortcut(QKeySequence("Ctrl+O"), this);
    connect(m_shortcutBrowseInput, &QShortcut::activated, this, &MainWindow::onBrowseInput);
    
    // Ctrl+Shift+O: Browse Output
    m_shortcutBrowseOutput = new QShortcut(QKeySequence("Ctrl+Shift+O"), this);
    connect(m_shortcutBrowseOutput, &QShortcut::activated, this, &MainWindow::onBrowseOutput);
    
    // F5 or Ctrl+R: Run
    m_shortcutRun = new QShortcut(QKeySequence("F5"), this);
    connect(m_shortcutRun, &QShortcut::activated, this, &MainWindow::onRun);
    QShortcut* shortcutRunAlt = new QShortcut(QKeySequence("Ctrl+R"), this);
    connect(shortcutRunAlt, &QShortcut::activated, this, &MainWindow::onRun);
    
    // Ctrl+L: Clear Log
    m_shortcutClearLog = new QShortcut(QKeySequence("Ctrl+L"), this);
    connect(m_shortcutClearLog, &QShortcut::activated, this, &MainWindow::onClearLog);
}



// Log filtering
void MainWindow::onLogFilterChanged(int index)
{
    Q_UNUSED(index);
    m_currentLogFilter = static_cast<LogLevel>(m_logFilterCombo->currentData().toInt());
    
    // Re-apply filter to existing log
    QString allText = m_logEdit->toPlainText();
    m_logEdit->clear();
    
    QStringList lines = allText.split('\n');
    for (const QString& line : lines)
    {
        LogLevel lineLevel = VortigauntLog::LogLevel::Info;
        if (line.contains("[WARNING]", Qt::CaseInsensitive))
        {
            lineLevel = VortigauntLog::LogLevel::Warning;
        }
        else if (line.contains("[ERROR]", Qt::CaseInsensitive))
        {
            lineLevel = VortigauntLog::LogLevel::Error;
        }
        
        // Show line if it matches filter
        if (m_currentLogFilter == VortigauntLog::LogLevel::Info || lineLevel == m_currentLogFilter)
        {
            m_logEdit->appendPlainText(line);
        }
    }
}

void MainWindow::onExportLog()
{
    QString fileName = QFileDialog::getSaveFileName(
        this,
        tr("Export Log"),
        QDir::home().filePath(QStringLiteral("Vortigaunt_Log_%1.txt").arg(QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss"))),
        tr("Text files (*.txt);;All files (*.*)")
    );
    
    if (fileName.isEmpty())
        return;
    
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    {
        QMessageBox::warning(this, tr("Export Failed"), tr("Could not write to file: %1").arg(fileName));
        return;
    }
    
    QTextStream out(&file);
    out << m_logEdit->toPlainText();
    
    VortigauntLog::Vortigaunt_Printf(tr("^2Log exported to: ^3%1").arg(fileName));
}

// Clear log
void MainWindow::onClearLog()
{
    m_logEdit->clear();
}





bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    // Handle drag-drop on animation list widget
    if (watched == m_animationListWidget->viewport())
    {
        if (event->type() == QEvent::DragEnter)
        {
            QDragEnterEvent* dragEvent = static_cast<QDragEnterEvent*>(event);
            if (dragEvent->mimeData()->hasUrls())
            {
                // Check if any dropped file is a supported animation format
                for (const QUrl& url : dragEvent->mimeData()->urls())
                {
                    QString filePath = url.toLocalFile();
                    QString ext = QFileInfo(filePath).suffix().toLower();
                    if (ext == "gr2" || ext == "smd" || ext == "fbx")
                    {
                        dragEvent->acceptProposedAction();
                        return true;
                    }
                }
            }
            return true; 
        }
        else if (event->type() == QEvent::Drop)
        {
            QDropEvent* dropEvent = static_cast<QDropEvent*>(event);
            if (dropEvent->mimeData()->hasUrls())
            {
                for (const QUrl& url : dropEvent->mimeData()->urls())
                {
                    QString filePath = url.toLocalFile();
                    QString ext = QFileInfo(filePath).suffix().toLower();
                    if (ext == "gr2" || ext == "smd" || ext == "fbx")
                    {
                        QFileInfo fileInfo(filePath);
                        QString fileName = fileInfo.fileName();
                        
                        // Check if already in list (avoid duplicates)
                        bool found = false;
                        for (int i = 0; i < m_animationListWidget->count(); ++i)
                        {
                            QListWidgetItem* existingItem = m_animationListWidget->item(i);
                            QString existingPath = existingItem->data(Qt::UserRole).toString();
                            if (existingPath == filePath)
                            {
                                found = true;
                                break;
                            }
                        }
                        
                        if (!found)
                        {
                            gr2AddAnimationItem(fileName, filePath);
                        }
                    }
                }
                dropEvent->acceptProposedAction();
                return true;
            }
        }
    }
    
    return QMainWindow::eventFilter(watched, event);
}


void MainWindow::onCheckForUpdates()
{
    if (!m_updateChecker)
    {
        m_updateChecker = new VortigauntUpdateCheck(this);
    }
    
    m_updateChecker->checkForUpdates(false);
}

// --- Background Threading Support ---

// Helper to safely update progress from background thread
void MainWindow::updateProgressSafe(int percent)
{
    QMetaObject::invokeMethod(this, [this, percent]() {
        if (m_progressBar)
        {
            if (!m_progressBar->isVisible()) m_progressBar->setVisible(true);
            m_progressBar->setValue(percent);
        }
    }, Qt::QueuedConnection);
}

void MainWindow::setUiProcessing(bool processing)
{
    m_isProcessing = processing;
    
    if (processing)
    {
        // Operation started: Disable Run
        if(m_runButton) m_runButton->setDisabled(true);
        
        if (m_progressBar) {
             m_progressBar->setVisible(true);
             m_progressBar->setValue(0);
        }
    }
    else
    {
        // Operation finished: Enable Run
        if(m_runButton) {
            m_runButton->setDisabled(false);
        }
    }

    // Disable/Enable other inputs to prevent re-entrancy
    if(m_browseInputButton) m_browseInputButton->setDisabled(processing);
    if(m_browseOutputButton) m_browseOutputButton->setDisabled(processing);
    if(m_inputPathEdit) m_inputPathEdit->setDisabled(processing);
    if(m_outputPathEdit) m_outputPathEdit->setDisabled(processing);
    if(m_settingsAction) m_settingsAction->setDisabled(processing);
}

void MainWindow::runInBackground(std::function<void()> task)
{
    if (m_isProcessing) return;

    setUiProcessing(true);
    // Progress bar visibility handled in setUiProcessing or callbacks

    // Run in detached thread
    std::thread([this, task]() {
        // Execute task
        task();

        // Restore UI on main thread
        QMetaObject::invokeMethod(this, [this]() {
            setUiProcessing(false);
            
            VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2Operation Completed.^7"));
            
            if(m_progressBar) {
                m_progressBar->setValue(100);
            }
        }, Qt::QueuedConnection);
    }).detach();
}

