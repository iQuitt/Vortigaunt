// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

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
#include <QDoubleSpinBox>
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
#include <QCloseEvent>
#include <QDirIterator>
#include <QDockWidget>
#include <QFormLayout>
#include <QHeaderView>
#include <QScrollArea>
#include <QSet>
#include <QStackedWidget>
#include <QToolButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QKeyEvent>
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
#include "core/extractors/gma/GmaExtractor.h"
#include "core/VortigauntVersion.h"
#include "core/extractors/pak/PakExtractor.h"
#include "core/converters/Gr2Converter.h"
#include "core/converters/FbxGlbConverter.h"
#include "core/converters/WeaponModelConverter.h"
#include "utils/FileIO.h"
#include "utils/ImageUtils.h"
#include "core/VortigauntLog.h"
#include "core/converters/DtxConverter.h"
#include "UiUtils.h"
#include "widgets/GLBViewer.h"
#include "widgets/RibbonBar.h"

#ifdef ENABLE_GRANNY2
#include <granny.h>
#endif
#include "workspaces/ArchiveSource.h"
#include "workspaces/ArchiveWorkspace.h"
#include "workspaces/TextureWorkspace.h"
#ifdef ENABLE_AUTORIG
#include "workspaces/HandsWorkspace.h"
#endif
#include "workspaces/WeaponFitWorkspace.h"
#include "widgets/StudioModelView.h"
#include "core/mdl/StudioModel.h"
#include "SpriteViewerWindow.h"
#include "LoLModelDownloadDialog.h"
#include "SteamWorkshopDownloaderDialog.h"
#ifdef METIN2_SCRIPT_EFFECT
#include "MseViewerWindow.h"
#endif
#ifdef ENABLE_AUTORIG
#include "PhysicsLabDialog.h"
#include "AutoRigDialog.h"
#endif
#include "WadMakerDialog.h"
#include "SettingsDialog.h"
#include "dialogs/AboutDialog.h"
#include "LanguageManager.h"

#include "WadMaker.h"
#include "AudioConvertDialog.h"
#include "dialogs/HudSpriteDialog.h"
#include "dialogs/RetargetDialog.h"
#ifdef ENABLE_AUTORIG
#include "dialogs/MapPortDialog.h"
#endif
#include "QCFile.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include "VortigauntUpdateCheck.h"

#include <QDesktopServices>
#include "Discord_Integration.h"



namespace
{

enum class StatusKind
{
    Waiting,
    Running,
    Done,
    Failed,
};

// file list item data
constexpr int kPathRole = Qt::UserRole;
constexpr int kOpRole = Qt::UserRole + 1;
constexpr int kSubfolderRole = Qt::UserRole + 2;
constexpr int kPModelRole = Qt::UserRole + 3;  // the QC of the p_ model a v_ model was converted to
constexpr int kConvertedRole = Qt::UserRole + 4;  // the row was converted, with or without a QC

// bigger models freeze the window while they load
constexpr qint64 kMaxPreviewBytes = 60ll * 1024 * 1024;

// ribbon icons

//TODO: get a fuckin better icons
constexpr const char* kIconFile = "M14 3H6v18h12V7z M14 3v4h4 M12 11v6 M9 14h6";
constexpr const char* kIconFolder = "M3 6h7l2 2h9v11H3z M12 11v6 M9 14h6";
constexpr const char* kIconClear = "M5 7h14 M9 7V4h6v3 M7 7l1 13h8l1-13";
constexpr const char* kIconPlay = "M7 4l12 8-12 8z";
constexpr const char* kIconStop = "M6 6h12v12H6z";
constexpr const char* kIconQc = "M5 3h10l4 4v14H5z M8 12h8 M8 16h6";
constexpr const char* kIconCompile = "M14 4l-4 16 M7 8l-4 4 4 4 M17 8l4 4-4 4";
constexpr const char* kIconOutput = "M3 6h7l2 2h9v11H3z M13 13h5 M16 10l3 3-3 3";
constexpr const char* kIconOpen = "M3 6h7l2 2h9v11H3z M10 16l6-6 M11 10h5v5";
constexpr const char* kIconArchive = "M3 7h18v13H3z M3 7l2-3h14l2 3 M10 12h4";
constexpr const char* kIconArchiveAdd = "M3 7h18v13H3z M3 7l2-3h14l2 3 M12 10v7 M8.5 13.5h7";
constexpr const char* kIconExtract = "M3 7h18v13H3z M3 7l2-3h14l2 3 M12 10v7 M9 14l3 3 3-3";
constexpr const char* kIconExtractAll = "M3 6h7l2 2h9v11H3z M12 11v6 M9 14l3 3 3-3";
constexpr const char* kIconClose = "M6 6l12 12 M18 6L6 18";
constexpr const char* kIconSend = "M4 12h12 M12 8l4 4-4 4 M20 5v14";
constexpr const char* kIconTextureAdd = "M14 3H3v18h18V10 M3 16l5-5 4 4 3-3 6 6 M18 2v7 M14.5 5.5h7";
constexpr const char* kIconSave = "M5 3h11l3 3v15H5z M8 3v5h7V3 M8 21v-7h8v7";
constexpr const char* kIconFrame = "M3 3h18v18H3z M3 16l5-5 4 4 3-3 6 6";
constexpr const char* kIconPrevious = "M15 5l-7 7 7 7";
constexpr const char* kIconNext = "M9 5l7 7-7 7";
constexpr const char* kIconView = "M2 12l5-5h10l5 5-5 5H7z M10 10h4v4h-4z";
constexpr const char* kIconCreate = "M3 5h7v14H3z M14 5h7v14h-7z M17.5 9v6 M14.5 12h6";
constexpr const char* kIconFix = "M14 4l6 6-9 9H5v-6z M12 6l6 6";
constexpr const char* kIconRename = "M4 20h4l11-11-4-4L4 16z M13 7l4 4";
constexpr const char* kIconPlayer = "M10 3h4v4h-4z M12 7v7 M7 10h10 M12 14l-3 7 M12 14l3 7";
#ifdef ENABLE_AUTORIG
constexpr const char* kIconHands = "M6 21l-2-8 M6 21h9l3-8 M8 13V4 M11 12V3 M14 12V4 M17 13V7";
#endif
constexpr const char* kIconReset =  "M4 4v6h6 M4 10l4-4h8v12H8";
constexpr const char* kIconSprite = "M3 5h7v14H3z M14 5h7v14h-7z";
constexpr const char* kIconWad = "M4 4h16v16H4z M4 12h16 M12 4v16";
#ifdef ENABLE_AUTORIG
constexpr const char* kIconMap = "M3 6l6-3 6 3 6-3v15l-6 3-6-3-6 3z M9 3v15 M15 6v15";
#endif
constexpr const char* kIconAudio = "M4 10v4 M8 7v10 M12 4v16 M16 8v8 M20 11v2";
constexpr const char* kIconDownload = "M12 4v11 M7 10l5 5 5-5 M4 20h16";
constexpr const char* kIconRig = "M12 3v6 M12 9l-6 5 M12 9l6 5 M12 9v8 M9 21l3-4 3 4";
constexpr const char* kIconSettings = "M4 6h9 M17 6h3 M15 4v4 M4 12h3 M11 12h9 M9 10v4 M4 18h11 M19 18h1 M17 16v4";

QString grabFileExt(const QString& filePath)
{
    int idx = filePath.lastIndexOf('.');
    if (idx < 0)
        return {};
    return filePath.mid(idx + 1).toLower();
}

// readable on the dark and on the light theme
QColor statusColor(StatusKind kind)
{
    const bool dark = QApplication::palette().color(QPalette::Base).lightness() < 128;
    switch (kind)
    {
    case StatusKind::Running: return dark ? QColor(229, 192, 123) : QColor(150, 100, 0);
    case StatusKind::Done: return dark ? QColor(111, 207, 143) : QColor(20, 120, 50);
    case StatusKind::Failed: return dark ? QColor(240, 140, 132) : QColor(180, 30, 30);
    case StatusKind::Waiting: break;
    }
    return QApplication::palette().color(QPalette::PlaceholderText);
}

} // namespace


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
    setAcceptDrops(true);
    setMinimumSize(1024, 700);
    resize(1360, 860);
    setDockNestingEnabled(true);

    QSettings settings("Vortigaunt", "VortigauntTool");
    m_outputDir = settings.value("MainWindow/outputDir").toString();

    setupPreview();
    setupDocks();
    setupRibbon();
    setupStatusBar();
    setupKeyboardShortcuts();

    m_processingEvents = false;

    m_defaultLayout = saveState();
    restoreGeometry(settings.value("MainWindow/geometry").toByteArray());
    restoreState(settings.value("MainWindow/layout").toByteArray());
    onFileSelectionChanged();

    // Check for updates on startup (with a short delay to let the UI load first)
    QTimer::singleShot(2000, this, [this]() {
        if (!m_updateChecker)
        {
            m_updateChecker = new VortigauntUpdateCheck(this);
        }
        m_updateChecker->checkForUpdates(true);
    });
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    m_workspaces->setCurrentIndex(0);
    QSettings settings("Vortigaunt", "VortigauntTool");
    settings.setValue("MainWindow/geometry", saveGeometry());
    settings.setValue("MainWindow/layout", saveState());
    settings.setValue("MainWindow/outputDir", m_outputDir);
    QMainWindow::closeEvent(event);
}

void MainWindow::setupRibbon()
{
    m_ribbon = new RibbonBar(this);
    setMenuWidget(m_ribbon);

    auto command = [this](const QString& label, const QString& tip, const char* icon) {
        auto* action = new QAction(UiUtils::lineIcon(QString::fromLatin1(icon)), tip, this);
        action->setIconText(label);
        return action;
    };
    auto tool = [](QAction* action, const QString& label, const char* icon) {
        action->setIcon(UiUtils::lineIcon(QString::fromLatin1(icon)));
        action->setIconText(label);
    };

    m_addFilesAction = command(tr("Add\nfiles"), tr("Add files (Ctrl+O)"), kIconFile);
    m_addFolderAction = command(tr("Add\nfolder"), tr("Add every file in a folder that Vortigaunt can convert"), kIconFolder);
    m_clearListAction = command(tr("Clear\nlist"), tr("Remove every file from the list"), kIconClear);
    m_runAction = command(tr("Convert"), tr("Convert every file in the list (F5)"), kIconPlay);
    m_stopAction = command(tr("Stop"), tr("Stop after the file being converted"), kIconStop);
    m_stopAction->setEnabled(false);
    m_writeQCAction = command(tr("Write\nQC"), tr("Generate QC file for studiomdl compilation"), kIconQc);
    m_compileAction = command(tr("Compile\nmodel"), tr("Run studiomdl on the QC to produce the .mdl. Set the location of studiomdl in Settings."), kIconCompile);
    m_outputFolderAction = command(tr("Output\nfolder"), tr("Choose the output folder (Ctrl+Shift+O)"), kIconOutput);
    m_openOutputAction = command(tr("Open\noutput"), tr("Open the output folder"), kIconOpen);
    connect(m_addFilesAction, &QAction::triggered, this, &MainWindow::onAddFiles);
    connect(m_addFolderAction, &QAction::triggered, this, &MainWindow::onAddFolder);
    connect(m_clearListAction, &QAction::triggered, this, [this]() {
        m_fileList->clear();
        onFileSelectionChanged();
    });
    connect(m_runAction, &QAction::triggered, this, &MainWindow::onRun);
    connect(m_stopAction, &QAction::triggered, this, &MainWindow::onStop);
    connect(m_outputFolderAction, &QAction::triggered, this, &MainWindow::onBrowseOutput);
    connect(m_openOutputAction, &QAction::triggered, this, &MainWindow::onOpenOutputFolder);
    // the ribbon toggles and the Properties checkboxes are the same setting
    m_writeQCAction->setCheckable(true);
    m_compileAction->setCheckable(true);
    m_writeQCAction->setChecked(m_writeQCCheck->isChecked());
    m_compileAction->setChecked(m_compileModelCheck->isChecked());
    m_compileAction->setEnabled(m_writeQCCheck->isChecked());
    connect(m_writeQCAction, &QAction::toggled, m_writeQCCheck, &QCheckBox::setChecked);
    connect(m_writeQCCheck, &QCheckBox::toggled, m_writeQCAction, &QAction::setChecked);
    connect(m_writeQCCheck, &QCheckBox::toggled, m_compileAction, &QAction::setEnabled);
    connect(m_compileAction, &QAction::toggled, m_compileModelCheck, &QCheckBox::setChecked);
    connect(m_compileModelCheck, &QCheckBox::toggled, m_compileAction, &QAction::setChecked);

    QAction* openArchiveAction = command(tr("Open\narchive"), tr("Open REZ, PAK, XFS, VPK or GMA archives"), kIconArchive);
    QAction* closeArchiveAction = command(tr("Close\narchive"), tr("Close the archive shown"), kIconClose);
    QAction* extractSelectedAction = command(tr("Extract\nselected"), tr("Extract the selected files to the output folder"), kIconExtract);
    QAction* extractAllAction = command(tr("Extract\nall"), tr("Extract every file of the archive to the output folder"), kIconExtractAll);
    QAction* sendToConvertAction = command(tr("Send to\nConvert"), tr("Extract the selected files and add them to the Convert list"), kIconSend);
    connect(openArchiveAction, &QAction::triggered, this, [this]() {
        showWorkspace(m_archiveWorkspace);
        m_archiveWorkspace->openArchiveDialog();
    });
    connect(closeArchiveAction, &QAction::triggered, m_archiveWorkspace, &ArchiveWorkspace::closeCurrentArchive);
    connect(extractSelectedAction, &QAction::triggered, m_archiveWorkspace, &ArchiveWorkspace::extractSelected);
    connect(extractAllAction, &QAction::triggered, m_archiveWorkspace, &ArchiveWorkspace::extractAll);
    connect(sendToConvertAction, &QAction::triggered, m_archiveWorkspace, &ArchiveWorkspace::sendSelectedToConvert);

    QAction* openTexturesAction = command(tr("Open\ntextures"), tr("Open DTX, VTF, DDS, TGA, BMP, PNG or JPG files"), kIconTextureAdd);
    QAction* openTextureFolderAction = command(tr("Open\nfolder"), tr("Open every texture in a folder and its sub-folders"), kIconFolder);
    QAction* removeTexturesAction = command(tr("Remove"), tr("Remove the selected textures from the list"), kIconClose);
    QAction* clearTexturesAction = command(tr("Clear"), tr("Remove every texture from the list"), kIconClear);
    QAction* exportBmpAction = command(tr("Export\nBMP"), tr("Save every texture in the list, or the ones the search shows, as GoldSrc BMPs to the output folder"), kIconSave);
    QAction* exportPngAction = command(tr("Export\nPNG"), tr("Save every texture in the list, or the ones the search shows, as PNGs to the output folder"), kIconSave);
    connect(openTexturesAction, &QAction::triggered, this, [this]() {
        showWorkspace(m_textureWorkspace);
        m_textureWorkspace->openFilesDialog();
    });
    connect(openTextureFolderAction, &QAction::triggered, this, [this]() {
        showWorkspace(m_textureWorkspace);
        m_textureWorkspace->openFolderDialog();
    });
    connect(removeTexturesAction, &QAction::triggered, m_textureWorkspace, &TextureWorkspace::removeSelected);
    connect(clearTexturesAction, &QAction::triggered, m_textureWorkspace, &TextureWorkspace::clear);
    connect(exportBmpAction, &QAction::triggered, m_textureWorkspace, &TextureWorkspace::exportBmp);
    connect(exportPngAction, &QAction::triggered, m_textureWorkspace, &TextureWorkspace::exportPng);

    // the sprite and WAD tabs bring their own commands; a command shows its tab first
    auto workspaceCommand = [this, &tool](QAction* action, const QString& label, const char* icon, QWidget* workspace) {
        tool(action, label, icon);
        connect(action, &QAction::triggered, this, [this, workspace]() { showWorkspace(workspace); }, Qt::DirectConnection);
        return action;
    };
    workspaceCommand(m_spriteWorkspace->openAction(), tr("Open\nsprite"), kIconOpen, m_spriteWorkspace);
    workspaceCommand(m_spriteWorkspace->saveAction(), tr("Save"), kIconSave, m_spriteWorkspace);
    workspaceCommand(m_spriteWorkspace->saveAsAction(), tr("Save\nas"), kIconSave, m_spriteWorkspace);
    workspaceCommand(m_spriteWorkspace->extractFrameAction(), tr("Extract\nframe"), kIconFrame, m_spriteWorkspace);
    workspaceCommand(m_spriteWorkspace->extractAllFramesAction(), tr("Extract\nall"), kIconExtractAll, m_spriteWorkspace);
    workspaceCommand(m_spriteWorkspace->previousAction(), tr("Previous"), kIconPrevious, m_spriteWorkspace);
    workspaceCommand(m_spriteWorkspace->nextAction(), tr("Next"), kIconNext, m_spriteWorkspace);
    workspaceCommand(m_spriteWorkspace->viewModeAction(), tr("View"), kIconView, m_spriteWorkspace);
    workspaceCommand(m_spriteWorkspace->createModeAction(), tr("Create\nfrom images"), kIconCreate, m_spriteWorkspace);
    workspaceCommand(m_spriteWorkspace->fixModeAction(), tr("Fix\nV3 to V2"), kIconFix, m_spriteWorkspace);
    workspaceCommand(m_wadWorkspace->newAction(), tr("New\nWAD"), kIconFile, m_wadWorkspace);
    workspaceCommand(m_wadWorkspace->openAction(), tr("Open\nWAD"), kIconOpen, m_wadWorkspace);
    workspaceCommand(m_wadWorkspace->saveAction(), tr("Save"), kIconSave, m_wadWorkspace);
    workspaceCommand(m_wadWorkspace->saveAsAction(), tr("Save\nas"), kIconSave, m_wadWorkspace);
    workspaceCommand(m_wadWorkspace->addImagesAction(), tr("Add\nimages"), kIconTextureAdd, m_wadWorkspace);
    workspaceCommand(m_wadWorkspace->removeAction(), tr("Remove"), kIconClose, m_wadWorkspace);
    workspaceCommand(m_wadWorkspace->extractAction(), tr("Extract"), kIconExtract, m_wadWorkspace);
    workspaceCommand(m_wadWorkspace->renameAction(), tr("Rename"), kIconRename, m_wadWorkspace);
    workspaceCommand(m_wadWorkspace->replaceAction(), tr("Replace\nimage"), kIconFrame, m_wadWorkspace);
    workspaceCommand(m_weaponFitWorkspace->openAction(), tr("Open\np_ model"), kIconOpen, m_weaponFitWorkspace);
    workspaceCommand(m_weaponFitWorkspace->playerAction(), tr("Player\nmodel"), kIconPlayer, m_weaponFitWorkspace);
    workspaceCommand(m_weaponFitWorkspace->resetAction(), tr("Reset"), kIconReset, m_weaponFitWorkspace);
    workspaceCommand(m_weaponFitWorkspace->saveAction(), tr("Save"), kIconSave, m_weaponFitWorkspace);
    workspaceCommand(m_weaponFitWorkspace->saveAsAction(), tr("Save\nas"), kIconSave, m_weaponFitWorkspace);
#ifdef ENABLE_AUTORIG
    workspaceCommand(m_handsWorkspace->openAction(), tr("Open\nv_ model"), kIconOpen, m_handsWorkspace);
    workspaceCommand(m_handsWorkspace->handsAction(), tr("New\nhands"), kIconHands, m_handsWorkspace);
    workspaceCommand(m_handsWorkspace->buildAction(), tr("Build"), kIconCompile, m_handsWorkspace);
    workspaceCommand(m_handsWorkspace->saveAsAction(), tr("Save\nas"), kIconSave, m_handsWorkspace);
#endif
    QAction* texturesToWadAction = command(tr("Send to\nWAD"), tr("Add the selected textures to the WAD open in the WAD tab"), kIconWad);
    connect(texturesToWadAction, &QAction::triggered, m_textureWorkspace, &TextureWorkspace::sendSelectedToWad);
    m_audioConvertAction = new QAction(tr("Convert WAV for Goldsrc..."), this);
    tool(m_audioConvertAction, tr("Convert\nWAV"), kIconAudio);
    connect(m_audioConvertAction, &QAction::triggered, this, &MainWindow::onOpenAudioConverter);
#ifdef ENABLE_AUTORIG
    m_mapPortAction = new QAction(QStringLiteral("Port a GR2 room to a GoldSrc map of brushes..."), this);
    tool(m_mapPortAction, QStringLiteral("GR2 to\nmap"), kIconMap);
    connect(m_mapPortAction, &QAction::triggered, this, &MainWindow::onOpenMapPorter);
#endif
    m_retargetAction = new QAction(tr("Retarget motions onto a GoldSrc model's skeleton..."), this);
    tool(m_retargetAction, tr("Retarget\nanimation"), kIconRig);
    connect(m_retargetAction, &QAction::triggered, this, &MainWindow::onOpenRetargetDialog);
    m_hudSpriteAction = new QAction(tr("Make a weapon's 640hud sprite from its model..."), this);
    tool(m_hudSpriteAction, tr("HUD\nsprite"), kIconSprite);
    connect(m_hudSpriteAction, &QAction::triggered, this, &MainWindow::onOpenHudSprite);
    m_lolModelAction = new QAction(tr("League of Legends Models..."), this);
    tool(m_lolModelAction, tr("LoL\nmodels"), kIconDownload);
    connect(m_lolModelAction, &QAction::triggered, this, &MainWindow::onOpenLoLModelDownloader);
    m_steamWorkshopAction = new QAction(tr("Steam Workshop Downloader..."), this);
    tool(m_steamWorkshopAction, tr("Steam\nWorkshop"), kIconDownload);
    connect(m_steamWorkshopAction, &QAction::triggered, this, &MainWindow::onOpenSteamWorkshopDownloader);
#ifdef ENABLE_LITHTECH
#endif
#ifdef ENABLE_AUTORIG
    m_autoRigAction = new QAction(tr("Auto-Rig (Beta)"), this);
    tool(m_autoRigAction, tr("Auto-Rig\n(beta)"), kIconRig);
    connect(m_autoRigAction, &QAction::triggered, this, &MainWindow::onOpenAutoRigDialog);
    // private, so not translated
    m_physicsLabAction = new QAction(QStringLiteral("Physics Lab"), this);
    tool(m_physicsLabAction, QStringLiteral("Physics\nlab"), kIconRig);
    connect(m_physicsLabAction, &QAction::triggered, this, &MainWindow::onOpenPhysicsLabDialog);
#endif
#ifdef METIN2_SCRIPT_EFFECT
    m_mseViewAction = new QAction(tr("Metin2 Script Effect Viewer..."), this);
    tool(m_mseViewAction, tr("MSE\nviewer"), kIconSprite);
    connect(m_mseViewAction, &QAction::triggered, this, &MainWindow::onOpenMseViewer);
#endif

    const int convertTab = m_ribbon->addTab(tr("Convert"));
    QWidget* filesGroup = m_ribbon->addGroup(convertTab, tr("Files"));
    m_ribbon->addButton(filesGroup, m_addFilesAction);
    m_ribbon->addButton(filesGroup, m_addFolderAction);
    m_ribbon->addButton(filesGroup, m_clearListAction);
    QWidget* runGroup = m_ribbon->addGroup(convertTab, tr("Run"));
    m_ribbon->addButton(runGroup, m_runAction, true);
    m_ribbon->addButton(runGroup, m_stopAction);
    QWidget* modelGroup = m_ribbon->addGroup(convertTab, tr("Model"));
    m_ribbon->addButton(modelGroup, m_writeQCAction);
    m_ribbon->addButton(modelGroup, m_compileAction);
    QWidget* outputGroup = m_ribbon->addGroup(convertTab, tr("Output"));
    m_ribbon->addButton(outputGroup, m_outputFolderAction);
    m_ribbon->addButton(outputGroup, m_openOutputAction);

    const int archiveTab = m_ribbon->addTab(tr("Archives"));
    m_persistentWorkspaces.push_back({ m_archiveWorkspace, archiveTab, tr("Archives") });
    QWidget* archiveGroup = m_ribbon->addGroup(archiveTab, tr("Archive"));
    m_ribbon->addButton(archiveGroup, openArchiveAction);
    m_ribbon->addButton(archiveGroup, closeArchiveAction);
    QWidget* extractGroup = m_ribbon->addGroup(archiveTab, tr("Extract"));
    m_ribbon->addButton(extractGroup, extractSelectedAction);
    m_ribbon->addButton(extractGroup, extractAllAction);
    m_ribbon->addButton(extractGroup, sendToConvertAction);

    const int textureTab = m_ribbon->addTab(tr("Textures"));
    m_persistentWorkspaces.push_back({ m_textureWorkspace, textureTab, tr("Textures") });
    QWidget* textureGroup = m_ribbon->addGroup(textureTab, tr("Textures"));
    m_ribbon->addButton(textureGroup, openTexturesAction);
    m_ribbon->addButton(textureGroup, openTextureFolderAction);
    m_ribbon->addButton(textureGroup, removeTexturesAction);
    m_ribbon->addButton(textureGroup, clearTexturesAction);
    QWidget* textureExportGroup = m_ribbon->addGroup(textureTab, tr("Export"));
    m_ribbon->addButton(textureExportGroup, exportBmpAction);
    m_ribbon->addButton(textureExportGroup, exportPngAction);
    m_ribbon->addButton(textureExportGroup, texturesToWadAction);

    const int spriteTab = m_ribbon->addTab(tr("Sprites"));
    m_persistentWorkspaces.push_back({ m_spriteWorkspace, spriteTab, tr("Sprites") });
    QWidget* spriteFileGroup = m_ribbon->addGroup(spriteTab, tr("Sprite"));
    m_ribbon->addButton(spriteFileGroup, m_spriteWorkspace->openAction());
    m_ribbon->addButton(spriteFileGroup, m_spriteWorkspace->saveAction());
    m_ribbon->addButton(spriteFileGroup, m_spriteWorkspace->saveAsAction());
    QWidget* spriteFramesGroup = m_ribbon->addGroup(spriteTab, tr("Frames"));
    m_ribbon->addButton(spriteFramesGroup, m_spriteWorkspace->extractFrameAction());
    m_ribbon->addButton(spriteFramesGroup, m_spriteWorkspace->extractAllFramesAction());
    QWidget* spriteBrowseGroup = m_ribbon->addGroup(spriteTab, tr("Folder"));
    m_ribbon->addButton(spriteBrowseGroup, m_spriteWorkspace->previousAction());
    m_ribbon->addButton(spriteBrowseGroup, m_spriteWorkspace->nextAction());
    QWidget* spriteModeGroup = m_ribbon->addGroup(spriteTab, tr("Mode"));
    m_ribbon->addButton(spriteModeGroup, m_spriteWorkspace->viewModeAction());
    m_ribbon->addButton(spriteModeGroup, m_spriteWorkspace->createModeAction());
    m_ribbon->addButton(spriteModeGroup, m_spriteWorkspace->fixModeAction());

    const int wadTab = m_ribbon->addTab(tr("WAD"));
    m_persistentWorkspaces.push_back({ m_wadWorkspace, wadTab, tr("WAD") });
    QWidget* wadFileGroup = m_ribbon->addGroup(wadTab, tr("WAD"));
    m_ribbon->addButton(wadFileGroup, m_wadWorkspace->newAction());
    m_ribbon->addButton(wadFileGroup, m_wadWorkspace->openAction());
    m_ribbon->addButton(wadFileGroup, m_wadWorkspace->saveAction());
    m_ribbon->addButton(wadFileGroup, m_wadWorkspace->saveAsAction());
    QWidget* wadTextureGroup = m_ribbon->addGroup(wadTab, tr("Textures"));
    m_ribbon->addButton(wadTextureGroup, m_wadWorkspace->addImagesAction());
    m_ribbon->addButton(wadTextureGroup, m_wadWorkspace->removeAction());
    m_ribbon->addButton(wadTextureGroup, m_wadWorkspace->extractAction());
    m_ribbon->addButton(wadTextureGroup, m_wadWorkspace->renameAction());
    m_ribbon->addButton(wadTextureGroup, m_wadWorkspace->replaceAction());

    const int weaponTab = m_ribbon->addTab(tr("P Model"));
    m_persistentWorkspaces.push_back({ m_weaponFitWorkspace, weaponTab, tr("P Model") });
    QWidget* weaponFileGroup = m_ribbon->addGroup(weaponTab, tr("Model"));
    m_ribbon->addButton(weaponFileGroup, m_weaponFitWorkspace->openAction());
    m_ribbon->addButton(weaponFileGroup, m_weaponFitWorkspace->playerAction());
    QWidget* weaponSaveGroup = m_ribbon->addGroup(weaponTab, tr("Placement"));
    m_ribbon->addButton(weaponSaveGroup, m_weaponFitWorkspace->resetAction());
    m_ribbon->addButton(weaponSaveGroup, m_weaponFitWorkspace->saveAction());
    m_ribbon->addButton(weaponSaveGroup, m_weaponFitWorkspace->saveAsAction());

#ifdef ENABLE_AUTORIG
    const int handsTab = m_ribbon->addTab(tr("Hands"));
    m_persistentWorkspaces.push_back({ m_handsWorkspace, handsTab, tr("Hands") });
    QWidget* handsModelGroup = m_ribbon->addGroup(handsTab, tr("Model"));
    m_ribbon->addButton(handsModelGroup, m_handsWorkspace->openAction());
    m_ribbon->addButton(handsModelGroup, m_handsWorkspace->handsAction());
    QWidget* handsBuildGroup = m_ribbon->addGroup(handsTab, tr("Build"));
    m_ribbon->addButton(handsBuildGroup, m_handsWorkspace->buildAction());
    m_ribbon->addButton(handsBuildGroup, m_handsWorkspace->saveAsAction());
#endif

    const int toolsTab = m_ribbon->addTab(tr("Tools"));
    m_toolsRibbonTab = toolsTab;
    QWidget* soundGroup = m_ribbon->addGroup(toolsTab, tr("Sound"));
    m_ribbon->addButton(soundGroup, m_audioConvertAction);
#ifdef ENABLE_AUTORIG
    QWidget* mapGroup = m_ribbon->addGroup(toolsTab, QStringLiteral("Map"));
    m_ribbon->addButton(mapGroup, m_mapPortAction);
#endif
    QWidget* animationGroup = m_ribbon->addGroup(toolsTab, tr("Animation"));
    m_ribbon->addButton(animationGroup, m_retargetAction);
    QWidget* weaponGroup = m_ribbon->addGroup(toolsTab, tr("Weapon"));
    m_ribbon->addButton(weaponGroup, m_hudSpriteAction);
    QWidget* downloadGroup = m_ribbon->addGroup(toolsTab, tr("Download"));
    m_ribbon->addButton(downloadGroup, m_lolModelAction);
    m_ribbon->addButton(downloadGroup, m_steamWorkshopAction);
#if defined(ENABLE_AUTORIG) || defined(METIN2_SCRIPT_EFFECT)
    QWidget* experimentalGroup = m_ribbon->addGroup(toolsTab, tr("Experimental"));
#endif
#ifdef ENABLE_AUTORIG
    m_ribbon->addButton(experimentalGroup, m_autoRigAction);
    m_ribbon->addButton(experimentalGroup, m_physicsLabAction);
#endif
#ifdef METIN2_SCRIPT_EFFECT
    m_ribbon->addButton(experimentalGroup, m_mseViewAction);
#endif

    // Settings, language, updates and about live in one menu at the end of the tab row
    m_settingsMenu = new QMenu(this);
    m_settingsAction = m_settingsMenu->addAction(tr("Configure..."));
    connect(m_settingsAction, &QAction::triggered, this, &MainWindow::onOpenSettings);

    m_languageMenu = m_settingsMenu->addMenu(tr("Language"));
    const QString currentLang = LanguageManager::instance().getCurrentLanguage();
    for (const QString& lang : LanguageManager::instance().getAvailableLanguages())
    {
        QAction* langAction = m_languageMenu->addAction(LanguageManager::instance().getLanguageDisplayName(lang));
        langAction->setCheckable(true);
        langAction->setChecked(lang.toLower() == currentLang.toLower());
        langAction->setData(lang);
        connect(langAction, &QAction::triggered, this, [this, lang]() {
            if (lang.toLower() == LanguageManager::instance().getCurrentLanguage().toLower())
                return;
            LanguageManager::instance().loadLanguage(lang);
            QMessageBox msgBox(this);
            msgBox.setWindowTitle(tr("Restart Required"));
            msgBox.setText(tr("The application needs to restart for the language change to take effect."));
            msgBox.setIcon(QMessageBox::Information);
            msgBox.setStandardButtons(QMessageBox::Ok);
            msgBox.exec();
            QProcess::startDetached(QCoreApplication::applicationFilePath(), QCoreApplication::arguments());
            QCoreApplication::quit();
        });
    }

    m_resetLayoutAction = m_settingsMenu->addAction(tr("Reset panel layout"));
    connect(m_resetLayoutAction, &QAction::triggered, this, [this]() {
        restoreState(m_defaultLayout);
        for (QDockWidget* dock : { m_filesDock, m_propertiesDock, m_logDock })
            dock->show();
    });
    m_settingsMenu->addSeparator();
    m_checkUpdateAction = m_settingsMenu->addAction(tr("Check for Updates..."));
    connect(m_checkUpdateAction, &QAction::triggered, this, &MainWindow::onCheckForUpdates);
    m_vortiAboutAction = m_settingsMenu->addAction(tr("About Vortigaunt"));
    connect(m_vortiAboutAction, &QAction::triggered, this, [this] { AboutDialog dlg(this); dlg.exec(); });
    m_qtAboutAction = m_settingsMenu->addAction(tr("About Qt"));
    connect(m_qtAboutAction, &QAction::triggered, this, &QApplication::aboutQt);

    auto cornerButton = [this](const QString& text, const QIcon& icon) {
        auto* button = new QToolButton(m_ribbon);
        button->setText(text);
        button->setIcon(icon);
        button->setIconSize(QSize(18, 18));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setAutoRaise(true);
        button->setCursor(Qt::PointingHandCursor);
        m_ribbon->corner()->addWidget(button);
        return button;
    };
    QToolButton* discordBtn = cornerButton(tr("Discord"), QIcon(":/discord.png"));
    discordBtn->setToolTip(tr("Join our Discord"));
    connect(discordBtn, &QToolButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://discord.gg/PZ9JzgHHKa"));
    });
    QToolButton* githubBtn = cornerButton(tr("GitHub"), QIcon(":/github.png"));
    githubBtn->setToolTip(tr("GitHub Repository of Vortigaunt"));
    connect(githubBtn, &QToolButton::clicked, this, []() {
        QDesktopServices::openUrl(QUrl("https://github.com/iQuit (https://github.com/iQuitt)/Vortigaunt"));
    });
    QToolButton* settingsBtn = cornerButton(tr("Settings"), UiUtils::lineIcon(QString::fromLatin1(kIconSettings), 18));
    settingsBtn->setMenu(m_settingsMenu);
    settingsBtn->setPopupMode(QToolButton::InstantPopup);

    connect(m_ribbon, &RibbonBar::currentTabChanged, this, [this](int tab) {
        if (tab == 0)
        {
            m_workspaces->setCurrentIndex(0);
            return;
        }
        for (const Workspace& workspace : m_persistentWorkspaces)
        {
            if (workspace.ribbonTab == tab)
                showWorkspace(workspace.widget);
        }
    });
}

void MainWindow::setupDocks()
{
    // Files
    auto* filesPanel = new QWidget(this);
    auto* filesLayout = new QVBoxLayout(filesPanel);
    filesLayout->setContentsMargins(0, 0, 0, 0);
    filesLayout->setSpacing(0);

    m_fileList = new QTreeWidget(filesPanel);
    m_fileList->setObjectName("FileList");
    m_fileList->setColumnCount(3);
    m_fileList->setHeaderLabels({ tr("File"), tr("Operation"), tr("Status") });
    m_fileList->setRootIsDecorated(false);
    m_fileList->setUniformRowHeights(true);
    m_fileList->setAlternatingRowColors(true);
    m_fileList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_fileList->header()->setStretchLastSection(false);
    m_fileList->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_fileList->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_fileList->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_fileList->setContextMenuPolicy(Qt::ActionsContextMenu);
    auto* removeAction = new QAction(tr("Remove from list"), m_fileList);
    removeAction->setShortcut(QKeySequence::Delete);
    removeAction->setShortcutContext(Qt::WidgetShortcut);
    m_fileList->addAction(removeAction);
    connect(removeAction, &QAction::triggered, this, [this]() {
        if (m_isProcessing)
            return;
        qDeleteAll(m_fileList->selectedItems());
        onFileSelectionChanged();
    });
    connect(m_fileList, &QTreeWidget::itemSelectionChanged, this, &MainWindow::onFileSelectionChanged);
    filesLayout->addWidget(m_fileList, 1);

    m_fileCountLabel = new QLabel(filesPanel);
    m_fileCountLabel->setContentsMargins(6, 4, 6, 4);
    m_fileCountLabel->setForegroundRole(QPalette::PlaceholderText);
    filesLayout->addWidget(m_fileCountLabel);

    m_filesDock = new QDockWidget(tr("Files"), this);
    m_filesDock->setObjectName("FilesDock");
    m_filesDock->setWidget(filesPanel);
    addDockWidget(Qt::LeftDockWidgetArea, m_filesDock);

    // Properties
    auto* properties = new QWidget(this);
    auto* propertiesLayout = new QVBoxLayout(properties);

    m_selectedNameLabel = new QLabel(properties);
    QFont nameFont = m_selectedNameLabel->font();
    nameFont.setBold(true);
    m_selectedNameLabel->setFont(nameFont);
    m_selectedNameLabel->setWordWrap(true);
    propertiesLayout->addWidget(m_selectedNameLabel);

    auto* opRow = new QHBoxLayout();
    opRow->addWidget(new QLabel(tr("Operation:"), properties));
    m_fileOpCombo = new QComboBox(properties);
    for (FileOp op : { FileOp::None, FileOp::Ltb, FileOp::Gr2, FileOp::Mesh, FileOp::Weapon, FileOp::Dtx,
                       FileOp::Rez, FileOp::Pak, FileOp::Xfs, FileOp::Vpk, FileOp::Unity })
    {
        m_fileOpCombo->addItem(fileOpName(op), static_cast<int>(op));
    }
    m_fileOpCombo->setToolTip(tr("Picked from the file name; change it when the guess is wrong"));
    opRow->addWidget(m_fileOpCombo, 1);
    propertiesLayout->addLayout(opRow);
    connect(m_fileOpCombo, QOverload<int>::of(&QComboBox::activated), this, [this](int index) {
        const FileOp op = static_cast<FileOp>(m_fileOpCombo->itemData(index).toInt());
        for (QTreeWidgetItem* item : m_fileList->selectedItems())
        {
            item->setData(0, kOpRole, static_cast<int>(op));
            item->setText(1, fileOpName(op));
        }
        onFileSelectionChanged();
    });

    m_noSelectionLabel = new QLabel(tr("Select a file in the list to see its settings."), properties);
    m_noSelectionLabel->setWordWrap(true);
    m_noSelectionLabel->setForegroundRole(QPalette::PlaceholderText);
    propertiesLayout->addWidget(m_noSelectionLabel);

    // V model -> P/W model
    m_weaponGroup = new QGroupBox(tr("Weapon"), properties);
    auto* weaponForm = new QFormLayout(m_weaponGroup);
    m_gameDirCombo = new QComboBox(m_weaponGroup);
    m_gameDirCombo->addItem(tr("Counter-Strike"), static_cast<int>(GameDir::cstrike));
    m_gameDirCombo->addItem(tr("Half-Life"), static_cast<int>(GameDir::valve));
    m_gameDirCombo->addItem(tr("Sven Co-op"), static_cast<int>(GameDir::svencoop));
    m_gameDirCombo->setToolTip(tr("The game the P model is for. Each has its own player skeleton and hand size, "
                                     "and holds a weapon a little differently."));
    weaponForm->addRow(tr("Game:"), m_gameDirCombo);

    m_weaponTypeCombo = new QComboBox(m_weaponGroup);
    m_weaponTypeCombo->addItem(tr("Auto"), static_cast<int>(WeaponType::Auto));
    m_weaponTypeCombo->addItem(tr("Gun"), static_cast<int>(WeaponType::Gun));
    m_weaponTypeCombo->addItem(tr("Knife / melee"), static_cast<int>(WeaponType::Knife));
    m_weaponTypeCombo->addItem(tr("Grenade"), static_cast<int>(WeaponType::Grenade));
    m_weaponTypeCombo->addItem(tr("Pack (C4, mine)"), static_cast<int>(WeaponType::Pack));
    m_weaponTypeCombo->addItem(tr("Dual (one in each hand)"), static_cast<int>(WeaponType::Dual));
    m_weaponTypeCombo->addItem(tr("Like a reference p_ model"), static_cast<int>(WeaponType::Reference));
    m_weaponTypeCombo->setToolTip(tr("How the player holds the weapon. P models of one kind all sit in the hand the same way: "
                                     "a gun points along the hand, a knife stands up out of the fist, a grenade lies in the palm. "
                                     "For anything else, pick a p_ model that is held the way this one should be."));
    weaponForm->addRow(tr("Weapon:"), m_weaponTypeCombo);

    auto* referenceRow = new QWidget(m_weaponGroup);
    auto* referenceLayout = new QHBoxLayout(referenceRow);
    referenceLayout->setContentsMargins(0, 0, 0, 0);
    m_weaponReferenceEdit = new QLineEdit(referenceRow);
    m_weaponReferenceEdit->setPlaceholderText(tr("Reference p_ model (.mdl)"));
    m_weaponReferenceBrowse = new QPushButton(tr("..."), referenceRow);
    m_weaponReferenceBrowse->setMaximumWidth(32);
    referenceLayout->addWidget(m_weaponReferenceEdit, 1);
    referenceLayout->addWidget(m_weaponReferenceBrowse);
    weaponForm->addRow(referenceRow);
    connect(m_weaponReferenceBrowse, &QPushButton::clicked, this, [this]() {
        const QString file = QFileDialog::getOpenFileName(this, tr("Select a reference p_ model"), m_weaponReferenceEdit->text(),
                                                          tr("GoldSrc model (*.mdl);;All files (*.*)"));
        if (!file.isEmpty())
            m_weaponReferenceEdit->setText(file);
    });
    connect(m_weaponTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int) { updateWeaponModelControls(); });

    m_weaponScaleSpin = new QDoubleSpinBox(m_weaponGroup);
    m_weaponScaleSpin->setRange(0.0, 20.0);
    m_weaponScaleSpin->setDecimals(2);
    m_weaponScaleSpin->setSingleStep(0.05);
    m_weaponScaleSpin->setValue(0.0);
    m_weaponScaleSpin->setSpecialValueText(tr("Auto"));
    m_weaponScaleSpin->setToolTip(tr("Size of the P/W model relative to the V model. Auto works it out from the size of the V model's hands; "
                                     "the log shows the value it used, which is a good starting point for setting it by hand."));
    weaponForm->addRow(tr("Scale:"), m_weaponScaleSpin);

    m_weaponMirrorCombo = new QComboBox(m_weaponGroup);
    m_weaponMirrorCombo->addItem(tr("Auto"), -1);
    m_weaponMirrorCombo->addItem(tr("Yes"), 1);
    m_weaponMirrorCombo->addItem(tr("No"), 0);
    m_weaponMirrorCombo->setToolTip(tr("V models are usually built left handed and flipped by the engine, so the P/W model has to be the mirror image. "
                                       "Auto mirrors when the weapon stands on the left of the view."));
    weaponForm->addRow(tr("Mirror:"), m_weaponMirrorCombo);

    m_weaponUpsideDownCheck = new QCheckBox(tr("Upside down"), m_weaponGroup);
    m_weaponUpsideDownCheck->setToolTip(tr("Turns the weapon end for end in the hand. For a grenade or knife that came out the wrong way up."));
    weaponForm->addRow(m_weaponUpsideDownCheck);
    m_showOnPlayerButton = new QPushButton(tr("Show on player"), m_weaponGroup);
    m_showOnPlayerButton->setToolTip(tr("Open the p_ model this file was converted to in the hands of a player model"));
    weaponForm->addRow(m_showOnPlayerButton);
    connect(m_showOnPlayerButton, &QPushButton::clicked, this, &MainWindow::showSelectedOnPlayer);
    propertiesLayout->addWidget(m_weaponGroup);

    // GR2 animations
    m_gr2AnimGroup = new QGroupBox(tr("GR2 Animation Files"), properties);
    auto* animLayout = new QVBoxLayout(m_gr2AnimGroup);
    auto* animHint = new QLabel(tr("Animations (will be exported to {filename}_anims folder):\nDrag and drop animation files here or use the browse button."), m_gr2AnimGroup);
    animHint->setWordWrap(true);
    animLayout->addWidget(animHint);
    m_animationListWidget = new QListWidget(m_gr2AnimGroup);
    m_animationListWidget->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_animationListWidget->setAcceptDrops(true);
    m_animationListWidget->setDragDropMode(QAbstractItemView::DropOnly);
    m_animationListWidget->viewport()->setAcceptDrops(true);
    m_animationListWidget->viewport()->installEventFilter(this);
    m_animationListWidget->setMinimumHeight(90);
    animLayout->addWidget(m_animationListWidget);
    auto* browseAnimButton = new QPushButton(tr("Browse Animation Files..."), m_gr2AnimGroup);
    animLayout->addWidget(browseAnimButton, 0, Qt::AlignLeft);
    connect(browseAnimButton, &QPushButton::clicked, this, [this]() {
        const QStringList files = QFileDialog::getOpenFileNames(this, tr("Select Animation Files"), QString(),
                                                                tr("Animation files (*.smd *.gr2);;All files (*.*)"));
        for (const QString& file : files)
        {
            const QString fileName = QFileInfo(file).fileName();
            bool found = false;
            for (int i = 0; i < m_animationListWidget->count() && !found; ++i)
            {
                QListWidgetItem* existingItem = m_animationListWidget->item(i);
                found = existingItem->text() == fileName || existingItem->data(Qt::UserRole).toString() == file;
            }
            if (!found)
                gr2AddAnimationItem(fileName, file);
        }
    });
    // When you export the GR2 to Smd or another format, the texture UV Y axis is flipped.
    // Generally you need this option enabled for correct texture orientation.
    m_mirrorUVYCheck = new QCheckBox(tr("Mirror UV Y-axis"), m_gr2AnimGroup);
    m_mirrorUVYCheck->setToolTip(tr("Mirror UV Y-axis for GR2 to SMD conversion (fixes texture orientation)"));
    m_mirrorUVYCheck->setChecked(SettingsDialog::getMirrorUVY());
    connect(m_mirrorUVYCheck, &QCheckBox::toggled, this, [](bool checked) {
        SettingsDialog::setMirrorUVY(checked);
    });
    animLayout->addWidget(m_mirrorUVYCheck);
    propertiesLayout->addWidget(m_gr2AnimGroup);

    // Model output
    m_modelGroup = new QGroupBox(tr("Model"), properties);
    auto* modelLayout = new QVBoxLayout(m_modelGroup);
    m_writeQCCheck = new QCheckBox(tr("Write QC"), m_modelGroup);
    m_writeQCCheck->setToolTip(tr("Generate QC file for studiomdl compilation"));
    modelLayout->addWidget(m_writeQCCheck);
    // Shown with Write QC; there is nothing to compile without a QC
    m_compileModelCheck = new QCheckBox(tr("Compile model"), m_modelGroup);
    m_compileModelCheck->setToolTip(tr("Run studiomdl on the QC to produce the .mdl. Set the location of studiomdl in Settings."));
    m_compileModelCheck->setEnabled(false);
    connect(m_writeQCCheck, &QCheckBox::toggled, m_compileModelCheck, &QCheckBox::setEnabled);
    modelLayout->addWidget(m_compileModelCheck);
    m_ignoreMeshesCheck = new QCheckBox(tr("Ignore meshes"), m_modelGroup);
    m_ignoreAnimsCheck = new QCheckBox(tr("Ignore animations"), m_modelGroup);
    modelLayout->addWidget(m_ignoreMeshesCheck);
    modelLayout->addWidget(m_ignoreAnimsCheck);
    propertiesLayout->addWidget(m_modelGroup);

    m_archiveGroup = new QGroupBox(tr("Extract"), properties);
    auto* archiveLayout = new QVBoxLayout(m_archiveGroup);
    m_separateFoldersCheck = new QCheckBox(tr("Extract multiple file to separate folders"), m_archiveGroup);
    m_separateFoldersCheck->setToolTip(tr("When extracting REZ/PAK/XFS files, create a separate folder for each file"));
    archiveLayout->addWidget(m_separateFoldersCheck);
    propertiesLayout->addWidget(m_archiveGroup);

    m_unityGroup = new QGroupBox(tr("Unity"), properties);
    auto* unityLayout = new QVBoxLayout(m_unityGroup);
    m_unityBmpCheck = new QCheckBox(tr("Convert textures to BMP (8-bit)"), m_unityGroup);
    m_unityBmpCheck->setChecked(true);
    m_unityBmpCheck->setToolTip(tr("When unchecked, Unity textures are exported as 32-bit PNG with alpha channel"));
    unityLayout->addWidget(m_unityBmpCheck);
    propertiesLayout->addWidget(m_unityGroup);

    m_textureGroup = new QGroupBox(tr("Texture"), properties);
    auto* textureLayout = new QVBoxLayout(m_textureGroup);
    auto* textureNote = new QLabel(tr("Written as an 8-bit BMP of at most 512 x 512. The alpha channel is a gloss mask in DTX textures and is left out."), m_textureGroup);
    textureNote->setWordWrap(true);
    textureLayout->addWidget(textureNote);
    propertiesLayout->addWidget(m_textureGroup);

    propertiesLayout->addStretch();

    auto* propertiesScroll = new QScrollArea(this);
    propertiesScroll->setWidgetResizable(true);
    propertiesScroll->setFrameShape(QFrame::NoFrame);
    propertiesScroll->setWidget(properties);

    m_propertiesDock = new QDockWidget(tr("Properties"), this);
    m_propertiesDock->setObjectName("PropertiesDock");
    m_propertiesDock->setWidget(propertiesScroll);
    addDockWidget(Qt::RightDockWidgetArea, m_propertiesDock);

    // Log with filtering and export
    auto* logPanel = new QWidget(this);
    auto* logLayout = new QVBoxLayout(logPanel);
    logLayout->setContentsMargins(4, 4, 4, 4);
    auto* logToolbarLayout = new QHBoxLayout();
    logToolbarLayout->addWidget(new QLabel(tr("Filter:"), logPanel));
    m_logFilterCombo = new QComboBox(logPanel);
    m_logFilterCombo->addItem(tr("All"), static_cast<int>(VortigauntLog::LogLevel::Info));
    m_logFilterCombo->addItem(tr("Info"), static_cast<int>(VortigauntLog::LogLevel::Info));
    m_logFilterCombo->addItem(tr("Warning"), static_cast<int>(VortigauntLog::LogLevel::Warning));
    m_logFilterCombo->addItem(tr("Error"), static_cast<int>(VortigauntLog::LogLevel::Error));
    logToolbarLayout->addWidget(m_logFilterCombo);
    logToolbarLayout->addStretch();
    m_clearLogButton = new QPushButton(tr("Clear Log"), logPanel);
    m_saveLogButton = new QPushButton(tr("Export Log..."), logPanel);
    logToolbarLayout->addWidget(m_clearLogButton);
    logToolbarLayout->addWidget(m_saveLogButton);
    logLayout->addLayout(logToolbarLayout);

    m_logEdit = new QPlainTextEdit(logPanel);
    m_logEdit->setReadOnly(true);
    m_logEdit->setAcceptDrops(false);
    m_logEdit->setFont(QFont("Consolas", 9));
    VortigauntLog::addLogWidget(m_logEdit);
    logLayout->addWidget(m_logEdit, 1);

    connect(m_logFilterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onLogFilterChanged);
    connect(m_saveLogButton, &QPushButton::clicked, this, &MainWindow::onExportLog);
    connect(m_clearLogButton, &QPushButton::clicked, this, &MainWindow::onClearLog);

    m_logDock = new QDockWidget(tr("Log"), this);
    m_logDock->setObjectName("LogDock");
    m_logDock->setWidget(logPanel);
    addDockWidget(Qt::BottomDockWidgetArea, m_logDock);

    resizeDocks({ m_filesDock, m_propertiesDock }, { 360, 320 }, Qt::Horizontal);
    resizeDocks({ m_logDock }, { 190 }, Qt::Vertical);
}

void MainWindow::setupPreview()
{
    m_preview = new QStackedWidget(this);

    m_previewHint = new QLabel(tr("Drop files or folders here\n\nThe operation is picked from the file type."), m_preview);
    m_previewHint->setAlignment(Qt::AlignCenter);
    m_previewHint->setForegroundRole(QPalette::PlaceholderText);
    QFont hintFont = m_previewHint->font();
    hintFont.setPointSizeF(hintFont.pointSizeF() + 3.0);
    m_previewHint->setFont(hintFont);
    m_preview->addWidget(m_previewHint);

    m_modelPreview = new GLBViewer(m_preview);
    // its loading steps are only of use when developing
    m_modelPreview->setLogCallback([](const QString& message) {
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("DEBUG: [Preview] %1").arg(message));
    });
    m_preview->addWidget(m_modelPreview);
    connect(m_modelPreview, &GLBViewer::modelLoaded, this, [this](bool ok) {
        if (ok || m_preview->currentWidget() != m_modelPreview)
            return;
        m_previewInfo->setText(QStringLiteral("<b>%1</b><br><br>%2")
                                   .arg(QFileInfo(m_previewPath).fileName().toHtmlEscaped(), tr("This model could not be previewed.")));
        m_preview->setCurrentIndex(3);
    });

    m_imagePreview = new QLabel(m_preview);
    m_imagePreview->setAlignment(Qt::AlignCenter);
    m_imagePreview->setBackgroundRole(QPalette::Base);
    m_imagePreview->setAutoFillBackground(true);
    m_preview->addWidget(m_imagePreview);

    auto* infoPage = new QWidget(m_preview);
    m_studioPreview = new StudioModelView(m_preview);
    auto* infoLayout = new QVBoxLayout(infoPage);
    infoLayout->addStretch();
    m_previewInfo = new QLabel(infoPage);
    m_previewInfo->setAlignment(Qt::AlignCenter);
    m_previewInfo->setWordWrap(true);
    infoLayout->addWidget(m_previewInfo);
    m_previewOpenButton = new QPushButton(infoPage);
    infoLayout->addWidget(m_previewOpenButton, 0, Qt::AlignCenter);
    infoLayout->addStretch();
    m_preview->addWidget(infoPage);
    // after the info page, which is page 3
    m_preview->addWidget(m_studioPreview);

    m_workspaces = new QTabWidget(this);
    m_workspaces->setDocumentMode(true);
    m_workspaces->setTabsClosable(true);
    m_workspaces->setMovable(true);
    m_workspaces->addTab(m_preview, tr("Convert"));
    m_workspaces->tabBar()->setTabButton(0, QTabBar::RightSide, nullptr);
    m_workspaces->tabBar()->setTabButton(0, QTabBar::LeftSide, nullptr);
    connect(m_workspaces, &QTabWidget::tabCloseRequested, this, [this](int index) { closeToolTab(m_workspaces->widget(index)); });
    connect(m_workspaces, &QTabWidget::currentChanged, this, &MainWindow::onWorkspaceChanged);
    setCentralWidget(m_workspaces);

    m_archiveWorkspace = new ArchiveWorkspace([this]() { return outputDir(); }, this);
    m_archiveWorkspace->hide();
    connect(m_archiveWorkspace, &ArchiveWorkspace::addToConvert, this, [this](const QStringList& files) {
        addPaths(files);
        m_workspaces->setCurrentIndex(0);
    });
    connect(m_archiveWorkspace, &ArchiveWorkspace::openTexture, this, [this](const QString& name, const QByteArray& data) {
        showWorkspace(m_textureWorkspace);
        m_textureWorkspace->addFromMemory(name, data);
    });
    connect(m_archiveWorkspace, &ArchiveWorkspace::openSprite, this, &MainWindow::openSprite);

    m_textureWorkspace = new TextureWorkspace([this]() { return outputDir(); }, this);
    m_textureWorkspace->hide();
    m_spriteWorkspace = new SpriteViewerWindow(this);
    m_spriteWorkspace->hide();
    m_wadWorkspace = new WadMakerDialog(this);
    m_wadWorkspace->hide();
    // textures of the other tabs go into the WAD
    auto sendToWad = [this](const QList<QPair<QString, QImage>>& images) {
        showWorkspace(m_wadWorkspace);
        m_wadWorkspace->addImages(images);
    };
    connect(m_textureWorkspace, &TextureWorkspace::sendToWad, this, sendToWad);
    connect(m_archiveWorkspace, &ArchiveWorkspace::sendToWad, this, sendToWad);
    m_weaponFitWorkspace = new WeaponFitWorkspace(this);
    m_weaponFitWorkspace->hide();
#ifdef ENABLE_AUTORIG
    m_handsWorkspace = new HandsWorkspace(this);
    m_handsWorkspace->hide();
#endif

    // what is not saved yet shows a * on its tab
    auto showModified = [this](QWidget* workspace, bool modified) {
        // a property, as setWindowModified wants a [*] in a window title
        workspace->setProperty("vortigauntUnsaved", modified);
        const int index = m_workspaces->indexOf(workspace);
        const Workspace* entry = workspaceOf(workspace);
        if (index > 0 && entry)
            m_workspaces->setTabText(index, entry->title + (modified ? QStringLiteral("*") : QString()));
    };
    connect(m_wadWorkspace, &WadMakerDialog::modifiedChanged, this, [this, showModified](bool modified) { showModified(m_wadWorkspace, modified); });
    connect(m_weaponFitWorkspace, &WeaponFitWorkspace::modifiedChanged, this,
            [this, showModified](bool modified) { showModified(m_weaponFitWorkspace, modified); });
#ifdef ENABLE_AUTORIG
    connect(m_handsWorkspace, &HandsWorkspace::modifiedChanged, this,
            [this, showModified](bool modified) { showModified(m_handsWorkspace, modified); });
#endif
}

void MainWindow::openToolTab(QWidget* tool, const QString& title)
{
    if (!tool->property("vortigauntTool").toBool() && qobject_cast<QDialog*>(tool))
    {
        tool->setWindowFlags(Qt::Widget);
        // Escape and Enter would close a dialog that now lives in a tab
        tool->installEventFilter(this);
        tool->setProperty("vortigauntTool", true);
        for (QPushButton* button : tool->findChildren<QPushButton*>())
        {
            button->setAutoDefault(false);
            button->setDefault(false);
        }
        connect(qobject_cast<QDialog*>(tool), &QDialog::finished, this, [this, tool]() { closeToolTab(tool); });
    }
    int index = m_workspaces->indexOf(tool);
    if (index < 0)
        index = m_workspaces->addTab(tool, title);
    m_workspaces->setCurrentIndex(index);
    tool->show();
}

void MainWindow::closeToolTab(QWidget* tool)
{
    const int index = m_workspaces->indexOf(tool);
    if (index <= 0)
        return;
    if ((tool == m_archiveWorkspace && m_archiveWorkspace->isBusy()) || (tool == m_textureWorkspace && m_textureWorkspace->isBusy()) ||
        tool->property("vortigauntBusy").toBool())//lol
        return;
    m_workspaces->removeTab(index);
    // a workspace keeps what is open in it for the next time
    if (workspaceOf(tool))
        tool->hide();
    else
        tool->deleteLater();
}

void MainWindow::openToolOnce(const char* key, const QString& title, const std::function<QWidget*()>& create)
{
    for (int i = 1; i < m_workspaces->count(); ++i)
    {
        if (m_workspaces->widget(i)->property("vortigauntToolKey").toByteArray() == key)
        {
            m_workspaces->setCurrentIndex(i);
            return;
        }
    }
    QWidget* tool = create();
    tool->setProperty("vortigauntToolKey", QByteArray(key));
    openToolTab(tool, title);
}

const MainWindow::Workspace* MainWindow::workspaceOf(QWidget* widget) const
{
    for (const Workspace& workspace : m_persistentWorkspaces)
    {
        if (workspace.widget == widget)
            return &workspace;
    }
    return nullptr;
}

void MainWindow::showWorkspace(QWidget* widget)
{
    if (const Workspace* workspace = workspaceOf(widget))
    {
        const bool unsaved = widget->property("vortigauntUnsaved").toBool();
        openToolTab(widget, workspace->title + (unsaved ? QStringLiteral("*") : QString()));
    }
}

void MainWindow::openArchive(const QString& path)
{
    showWorkspace(m_archiveWorkspace);
    m_archiveWorkspace->openArchive(path);
}

void MainWindow::openTextures(const QStringList& paths)
{
    showWorkspace(m_textureWorkspace);
    m_textureWorkspace->addFiles(paths);
}

void MainWindow::openSprite(const QString& path)
{
    showWorkspace(m_spriteWorkspace);
    m_spriteWorkspace->setMode(SpriteViewerWindow::Mode::View);
    m_spriteWorkspace->openSprite(path);
}

void MainWindow::openWad(const QString& path)
{
    showWorkspace(m_wadWorkspace);
    m_wadWorkspace->loadWad(path);
}

void MainWindow::openWeaponFit(const QString& path)
{
    showWorkspace(m_weaponFitWorkspace);
    m_weaponFitWorkspace->openWeapon(path);
}

void MainWindow::openPaths(const QStringList& paths)
{
    QStringList textures;
    QStringList others;
    for (const QString& path : paths)
    {
        const QString ext = QFileInfo(path).suffix().toLower();
        if (ArchiveSource::isArchive(path))
            openArchive(path);
        else if (ext == "dtx" || ext == "vtf")
            textures << path;
        else if (ext == "spr")
            openSprite(path);
        else if (ext == "wad")
            openWad(path);
        else if (ext == "mdl" && QFileInfo(path).fileName().startsWith(QStringLiteral("p_"), Qt::CaseInsensitive))
            openWeaponFit(path);
        else
            others << path;
    }
    if (!textures.isEmpty())
        openTextures(textures);
    if (!others.isEmpty())
    {
        addPaths(others);
        m_workspaces->setCurrentIndex(0);
    }
}

void MainWindow::onWorkspaceChanged(int index)
{
    // the file list and its settings belong to Convert
    const bool convert = index == 0;
    if (!convert && m_convertDocksShown)
    {
        m_filesDockWasVisible = m_filesDock->isVisible();
        m_propertiesDockWasVisible = m_propertiesDock->isVisible();
        m_filesDock->hide();
        m_propertiesDock->hide();
        m_convertDocksShown = false;
    }
    else if (convert && !m_convertDocksShown)
    {
        m_filesDock->setVisible(m_filesDockWasVisible);
        m_propertiesDock->setVisible(m_propertiesDockWasVisible);
        m_convertDocksShown = true;
    }

    if (convert)
        m_ribbon->setCurrentTab(0);
    else if (const Workspace* workspace = workspaceOf(m_workspaces->widget(index)))
        m_ribbon->setCurrentTab(workspace->ribbonTab);
    else if (index > 0)
        m_ribbon->setCurrentTab(m_toolsRibbonTab);
}

MainWindow::FileOp MainWindow::detectFileOp(const QString& path)
{
    const QString ext = QFileInfo(path).suffix().toLower();
    if (ext == "ltb") return FileOp::Ltb;
    if (ext == "gr2") return FileOp::Gr2;
    if (ext == "fbx" || ext == "glb" || ext == "gltf" || ext == "obj") return FileOp::Mesh;
    if (ext == "mdl") return WeaponModelConverter::IsViewModelFile(path.toStdString()) ? FileOp::Weapon : FileOp::None;
    if (ext == "dtx") return FileOp::Dtx;
    if (ext == "rez") return FileOp::Rez;
    if (ext == "pak") return FileOp::Pak;
    if (ext == "xfs") return FileOp::Xfs;
    if (ext == "vpk" || ext == "gma") return FileOp::Vpk;
    if (ext == "unity3d" || ext == "bundle" || ext == "assets") return FileOp::Unity;
    return FileOp::None;
}

QString MainWindow::fileOpName(FileOp op)
{
    switch (op)
    {
    case FileOp::Ltb: return tr("LTB to SMD");
    case FileOp::Gr2: return tr("GR2 to SMD");
    case FileOp::Mesh: return tr("FBX/GLB/OBJ to SMD");
    case FileOp::Weapon: return tr("v_ to p_ / w_ model");
    case FileOp::Dtx: return tr("DTX to BMP");
    case FileOp::Rez: return tr("Extract REZ");
    case FileOp::Pak: return tr("Extract PAK (CSO)");
    case FileOp::Xfs: return tr("Extract XFS");
    case FileOp::Vpk: return tr("Extract VPK / GMA");
    case FileOp::Unity: return tr("Extract Unity assets");
    case FileOp::None: break;
    }
    return tr("Nothing to do");
}

void MainWindow::addPaths(const QStringList& paths)
{
    if (m_isProcessing)
        return;

    QSet<QString> listed;
    for (int i = 0; i < m_fileList->topLevelItemCount(); ++i)
        listed.insert(m_fileList->topLevelItem(i)->data(0, kPathRole).toString());

    const int before = m_fileList->topLevelItemCount();
    for (const QString& path : paths)
    {
        const QFileInfo info(path);
        if (info.isDir())
        {
            // sub-folders are kept in the output
            const QDir root(info.absoluteFilePath());
            int found = 0;
            QDirIterator it(root.absolutePath(), QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext())
            {
                const QString file = it.next();
                if (detectFileOp(file) == FileOp::None || listed.contains(file))
                    continue;
                QString sub = root.relativeFilePath(QFileInfo(file).absolutePath());
                if (sub == ".")
                    sub.clear();
                addFileRow(file, sub);
                listed.insert(file);
                found++;
            }
            if (found == 0)
                VortigauntLog::Vortigaunt_Printf(QStringLiteral("^3Nothing Vortigaunt can convert was found in '%1'.").arg(path));
        }
        else if (info.isFile() && !listed.contains(info.absoluteFilePath()))
        {
            addFileRow(info.absoluteFilePath(), QString());
            listed.insert(info.absoluteFilePath());
        }
    }

    if (m_fileList->selectedItems().isEmpty() && m_fileList->topLevelItemCount() > before)
        m_fileList->setCurrentItem(m_fileList->topLevelItem(before));
    onFileSelectionChanged();
}

void MainWindow::addFileRow(const QString& path, const QString& outputSubfolder)
{
    const FileOp op = detectFileOp(path);
    auto* item = new QTreeWidgetItem(m_fileList);
    item->setText(0, QFileInfo(path).fileName());
    item->setToolTip(0, QDir::toNativeSeparators(path));
    item->setText(1, fileOpName(op));
    item->setData(0, kPathRole, path);
    item->setData(0, kOpRole, static_cast<int>(op));
    item->setData(0, kSubfolderRole, outputSubfolder);
    setRowStatus(item, op == FileOp::None ? tr("Pick an operation") : tr("Waiting"), statusColor(StatusKind::Waiting));
}

void MainWindow::setRowStatus(QTreeWidgetItem* item, const QString& status, const QColor& color)
{
    item->setText(2, status);
    item->setForeground(2, color);
}

void MainWindow::setRowStatusLater(QTreeWidgetItem* item, const QString& status, const QColor& color)
{
    QMetaObject::invokeMethod(this, [this, item, status, color]() { setRowStatus(item, status, color); }, Qt::QueuedConnection);
}

QString MainWindow::outputDir() const
{
    return m_outputDir.isEmpty() ? SettingsDialog::getDefaultOutputDir() : m_outputDir;
}

void MainWindow::onFileSelectionChanged()
{
    const int count = m_fileList->topLevelItemCount();
    m_fileCountLabel->setText(count == 0 ? tr("Drag files or folders here, or use Add files.")
                                         : tr("%n file(s)", "", count));

    const QList<QTreeWidgetItem*> selected = m_fileList->selectedItems();
    QTreeWidgetItem* item = selected.isEmpty() ? nullptr : selected.first();
    m_noSelectionLabel->setVisible(!item);
    m_fileOpCombo->setEnabled(item && !m_isProcessing);
    if (!item)
    {
        m_selectedNameLabel->setText(tr("No file selected"));
        updatePropertiesFor(FileOp::None);
        showPreview(QString(), FileOp::None);
        return;
    }

    const QString path = item->data(0, kPathRole).toString();
    const FileOp op = static_cast<FileOp>(item->data(0, kOpRole).toInt());
    m_selectedNameLabel->setText(selected.size() > 1 ? tr("%1 files selected").arg(selected.size()) : QFileInfo(path).fileName());
    m_fileOpCombo->setCurrentIndex(m_fileOpCombo->findData(static_cast<int>(op)));
    updatePropertiesFor(op);
    showPreview(path, op);
    if (op == FileOp::Gr2 && path != m_animationListOwner)
        loadGr2Animations(path);
}

void MainWindow::updatePropertiesFor(FileOp op)
{
    const bool model = op == FileOp::Ltb || op == FileOp::Gr2 || op == FileOp::Mesh || op == FileOp::Weapon;
    m_weaponGroup->setVisible(op == FileOp::Weapon);
    QTreeWidgetItem* current = m_fileList->currentItem();
    const bool converted = current && current->data(0, kConvertedRole).toBool();
    m_showOnPlayerButton->setEnabled(converted && !m_isProcessing);
    m_showOnPlayerButton->setToolTip(converted ? tr("Open the p_ model this file was converted to in the hands of a player model")
                                               : tr("Convert the file first; its p_ model is then shown on a player"));
    m_gr2AnimGroup->setVisible(op == FileOp::Gr2);
    m_modelGroup->setVisible(model);
    m_ignoreMeshesCheck->setVisible(op == FileOp::Ltb || op == FileOp::Gr2);
    m_ignoreAnimsCheck->setVisible(op == FileOp::Ltb || op == FileOp::Gr2);
    m_archiveGroup->setVisible(op == FileOp::Rez || op == FileOp::Pak || op == FileOp::Xfs || op == FileOp::Vpk);
    m_unityGroup->setVisible(op == FileOp::Unity);
    m_textureGroup->setVisible(op == FileOp::Dtx);
    updateWeaponModelControls();
}

void MainWindow::showPreview(const QString& path, FileOp op)
{
    if (path == m_previewPath && !path.isEmpty())
        return;
    m_previewPath = path;
    m_previewOpenButton->disconnect();
    m_previewOpenButton->hide();

    if (path.isEmpty())
    {
        m_preview->setCurrentWidget(m_previewHint);
        return;
    }

    const QFileInfo info(path);
    const QString ext = info.suffix().toLower();
    auto showInfo = [this, &info](const QString& text) {
        m_previewInfo->setText(QStringLiteral("<b>%1</b><br><br>%2<br><br>%3")
                                   .arg(info.fileName().toHtmlEscaped(), UiUtils::formatSize(info.size()), text));
        m_preview->setCurrentIndex(3);
    };

    // GoldSrc models, the view models among them, with their first animation
    if (ext == "mdl")
    {
        auto model = std::make_shared<StudioModel>();
        if (info.size() <= kMaxPreviewBytes && model->Load(path.toStdString()))
        {
            m_studioPreview->clearModels();
            m_studioPreview->setSequence(m_studioPreview->addModel(model), 0);
            m_preview->setCurrentWidget(m_studioPreview);
            return;
        }
    }

    if (op == FileOp::Mesh)
    {
        if (info.size() > kMaxPreviewBytes)
        {
            showInfo(tr("Too large to preview."));
            return;
        }
        // the viewer loads on its first paint, so it has to be on screen first
        m_preview->setCurrentWidget(m_modelPreview);
        m_modelPreview->loadGLB(path);
        return;
    }

    QImage image;
    if (op == FileOp::Dtx)
    {
        DtxConverter decoder;
        std::vector<unsigned int> pixels;
        int width = 0, height = 0;
        if (decoder.DecodeDTXToRGBA(path.toStdString(), pixels, width, height) && !pixels.empty())
        {
            image = QImage(reinterpret_cast<const uchar*>(pixels.data()), width, height, QImage::Format_ARGB32).copy();
        }
    }
    else if (op == FileOp::None && (ext == "png" || ext == "tga" || ext == "bmp" || ext == "jpg" || ext == "jpeg" || ext == "dds" || ext == "vtf"))
    {
        image = ImageUtils::loadImage(path);
    }
    if (!image.isNull())
    {
        const QSize room = m_preview->size() - QSize(24, 24);
        QPixmap pixmap = QPixmap::fromImage(image.convertToFormat(QImage::Format_RGB32));
        if (pixmap.width() > room.width() || pixmap.height() > room.height())
            pixmap = pixmap.scaled(room, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        m_imagePreview->setPixmap(pixmap);
        m_imagePreview->setToolTip(tr("%1 x %2").arg(image.width()).arg(image.height()));
        m_preview->setCurrentWidget(m_imagePreview);
        return;
    }

    auto openWith = [this](const QString& label, const std::function<void()>& open) {
        m_previewOpenButton->setText(label);
        connect(m_previewOpenButton, &QPushButton::clicked, this, open);
        m_previewOpenButton->show();
    };
    auto browseArchive = [this, path]() { openArchive(path); };
    switch (op)
    {
    case FileOp::Rez:
        showInfo(tr("REZ archive. Convert extracts it to the output folder."));
        openWith(tr("Browse in Archives"), browseArchive);
        break;
    case FileOp::Pak:
        showInfo(tr("Counter-Strike Online PAK archive. Convert extracts it to the output folder."));
        openWith(tr("Browse in Archives"), browseArchive);
        break;
    case FileOp::Xfs:
        showInfo(tr("XFS archive. Convert extracts it to the output folder."));
        openWith(tr("Browse in Archives"), browseArchive);
        break;
    case FileOp::Vpk:
        showInfo(tr("Source engine archive. Convert extracts it to the output folder."));
        openWith(tr("Browse in Archives"), browseArchive);
        break;
    case FileOp::Weapon:
        showInfo(tr("GoldSrc view model. Convert makes the p_ and w_ models from it."));
        break;
    case FileOp::Gr2:
    case FileOp::Ltb:
        showInfo(tr("There is no 3D preview for this format yet."));
        break;
    case FileOp::Unity:
        showInfo(tr("Unity assets. Convert extracts meshes, textures and animation clips."));
        break;
    default:
        showInfo(tr("Vortigaunt does not know what to do with this file. Pick an operation in Properties."));
        break;
    }
}

void MainWindow::onAddFiles()
{
    const QString filter = tr("Supported files (%1);;All files (*.*)")
        .arg("*.ltb *.gr2 *.fbx *.glb *.gltf *.obj *.mdl *.dtx *.rez *.pak *.xfs *.vpk *.gma *.unity3d *.bundle *.assets");
    addPaths(QFileDialog::getOpenFileNames(this, tr("Add Files"), QString(), filter));
}

void MainWindow::onAddFolder()
{
    const QString folder = QFileDialog::getExistingDirectory(this, tr("Add Folder"));
    if (!folder.isEmpty())
        addPaths({ folder });
}

void MainWindow::onBrowseOutput()
{
    const QString path = QFileDialog::getExistingDirectory(this, tr("Select Output Folder"), outputDir());
    if (path.isEmpty())
        return;
    setOutputDir(path);
}

void MainWindow::setOutputDir(const QString& folder)
{
    m_outputDir = folder;
    m_outputButton->setText(tr("Output: %1").arg(QDir::toNativeSeparators(outputDir())));
}

void MainWindow::onOpenOutputFolder()
{
    QDir().mkpath(outputDir());
    QDesktopServices::openUrl(QUrl::fromLocalFile(outputDir()));
}

void MainWindow::onStop()
{
    m_cancelRequested = true;
    m_stopAction->setEnabled(false);
    VortigauntLog::Vortigaunt_Printf(QStringLiteral("^3Stopping after the file being converted...^7"));
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

void MainWindow::loadGr2Animations(const QString& gr2Path)
{
    // Clear previous animations when loading a new file
    m_animationListWidget->clear();
    m_animationListOwner = gr2Path;

    const QString inputPath = gr2Path;
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
    
    // Here is the stupid story.
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
    std::string grannyPath = inputPath.toStdString();
    granny_file* grannyFile = GrannyReadEntireFile(grannyPath.c_str());
    
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

void MainWindow::onRun()
{
    if (m_isProcessing)
        return;

    std::vector<FileJob> jobs;
    bool anyModel = false, anyWeapon = false;
    int gr2Count = 0;
    for (int i = 0; i < m_fileList->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = m_fileList->topLevelItem(i);
        const FileOp op = static_cast<FileOp>(item->data(0, kOpRole).toInt());
        if (op == FileOp::None)
        {
            setRowStatus(item, tr("Skipped"), statusColor(StatusKind::Waiting));
            continue;
        }
        const QString sub = item->data(0, kSubfolderRole).toString();
        jobs.push_back({ item, item->data(0, kPathRole).toString(), op, sub.isEmpty() ? outputDir() : QDir(outputDir()).filePath(sub) });
        anyModel |= op == FileOp::Ltb || op == FileOp::Gr2 || op == FileOp::Mesh || op == FileOp::Weapon;
        anyWeapon |= op == FileOp::Weapon;
        gr2Count += op == FileOp::Gr2 ? 1 : 0;
    }

    if (jobs.empty())
    {
        QMessageBox::information(this, tr("Nothing to convert"),
                                 tr("Add files Vortigaunt knows, or pick an operation for them in Properties."));
        return;
    }

    // Better to hear about a missing compiler now than after the conversion.
    if (anyModel && m_writeQCCheck->isChecked() && m_compileModelCheck->isChecked() &&
        !QFileInfo(SettingsDialog::getStudioMdlPath()).isFile())
    {
        QMessageBox::warning(this, tr("studiomdl not set"),
                             tr("\"Compile model\" needs studiomdl. Set its location in Settings, or untick \"Compile model\"."));
        return;
    }

    WeaponModelOptions weaponOptions;
    weaponOptions.writeQC = m_writeQCCheck->isChecked();
    weaponOptions.scale = static_cast<float>(m_weaponScaleSpin->value());
    weaponOptions.mirror = m_weaponMirrorCombo->currentData().toInt();
    weaponOptions.hold = static_cast<WeaponType>(m_weaponTypeCombo->currentData().toInt());
    weaponOptions.gameDir = static_cast<GameDir>(m_gameDirCombo->currentData().toInt());
    weaponOptions.upsideDown = m_weaponUpsideDownCheck->isChecked();
    weaponOptions.referenceModel = m_weaponReferenceEdit->text().trimmed().toStdString();
    if (anyWeapon && weaponOptions.hold == WeaponType::Reference && weaponOptions.referenceModel.empty())
    {
        QMessageBox::warning(this, tr("Reference model required"),
                             tr("Select the p_ model this weapon should be held like, or choose another weapon type."));
        return;
    }

    ltbConverterSetting ltbSettings;
    ltbSettings.IgnoreMeshes = m_ignoreMeshesCheck->isChecked();
    ltbSettings.IgnoreAnimations = m_ignoreAnimsCheck->isChecked();
    ltbSettings.WriteQC = m_writeQCCheck->isChecked();
    const bool writeQC = m_writeQCCheck->isChecked();
    const bool unityTexturesToBmp = m_unityBmpCheck->isChecked();
    // the animation list belongs to one model
    const bool gr2WithAnimations = gr2Count == 1;

    m_logEdit->clear();
    QDir().mkpath(outputDir());
    VortigauntLog::Vortigaunt_Printf(QStringLiteral("Output folder: %1").arg(outputDir()));

    m_extractionErrors.clear();
    m_cancelRequested = false;
    for (const FileJob& job : jobs)
        setRowStatus(job.item, tr("Waiting"), statusColor(StatusKind::Waiting));

    runInBackground([this, jobs, weaponOptions, ltbSettings, writeQC, unityTexturesToBmp, gr2WithAnimations]() {
        const QColor running = statusColor(StatusKind::Running);
        const QColor done = statusColor(StatusKind::Done);
        const QColor failed = statusColor(StatusKind::Failed);
        std::vector<bool> finished(jobs.size(), false);
        size_t finishedCount = 0;

        for (size_t i = 0; i < jobs.size(); ++i)
        {
            if (finished[i])
                continue;
            if (m_cancelRequested)
            {
                for (size_t j = i; j < jobs.size(); ++j)
                {
                    if (!finished[j])
                        setRowStatusLater(jobs[j].item, tr("Cancelled"), statusColor(StatusKind::Waiting));
                }
                break;
            }

            const FileJob& job = jobs[i];
            const bool archive = job.op == FileOp::Rez || job.op == FileOp::Pak || job.op == FileOp::Xfs || job.op == FileOp::Vpk;
            if (archive)
            {
                // archives of one kind go together, so "separate folders" means what it says
                std::vector<size_t> group;
                QStringList paths;
                for (size_t j = i; j < jobs.size(); ++j)
                {
                    if (!finished[j] && jobs[j].op == job.op && jobs[j].outputDir == job.outputDir)
                    {
                        group.push_back(j);
                        paths << jobs[j].path;
                        setRowStatusLater(jobs[j].item, tr("Extracting..."), running);
                    }
                }
                const size_t errorsBefore = m_extractionErrors.size();
                switch (job.op)
                {
                case FileOp::Rez: extractArchiveRez(paths, job.outputDir); break;
                case FileOp::Pak: extractArchivePak(paths, job.outputDir); break;
                case FileOp::Xfs: extractArchiveXfs(paths, job.outputDir); break;
                default: extractArchiveVpk(paths, job.outputDir); break;
                }
                for (size_t j : group)
                {
                    const QString name = QFileInfo(jobs[j].path).fileName();
                    bool ok = true;
                    for (size_t e = errorsBefore; e < m_extractionErrors.size(); ++e)
                    {
                        if (m_extractionErrors[e].filename == jobs[j].path || m_extractionErrors[e].filename == name)
                            ok = false;
                    }
                    setRowStatusLater(jobs[j].item, ok ? tr("Done") : tr("Failed"), ok ? done : failed);
                    finished[j] = true;
                    finishedCount++;
                }
            }
            else
            {
                setRowStatusLater(job.item, tr("Converting..."), running);
                bool ok = false;
                switch (job.op)
                {
                case FileOp::Ltb: ok = convertLtb(job.path, job.outputDir, ltbSettings); break;
                case FileOp::Gr2: ok = convertGr2(job.path, job.outputDir, gr2WithAnimations); break;
                case FileOp::Mesh: ok = convertFbxGlb(job.path, job.outputDir, writeQC); break;
                case FileOp::Weapon:
                {
                    // the row remembers its p_ model, for Show on player
                    QStringList pQcs;
                    ok = convertViewModel(job.path, job.outputDir, weaponOptions, &pQcs);
                    if (ok)
                    {
                        QTreeWidgetItem* item = job.item;
                        const QString qc = pQcs.isEmpty() ? QString() : pQcs.front();
                        QMetaObject::invokeMethod(this, [this, item, qc]() {
                            item->setData(0, kPModelRole, qc);
                            item->setData(0, kConvertedRole, true);
                            onFileSelectionChanged();
                        }, Qt::QueuedConnection);
                    }
                    break;
                }
                case FileOp::Dtx: ok = convertDtx(job.path, job.outputDir); break;
                case FileOp::Unity:
                {
                    UnityPorter porter;
                    porter.SetConvertTexturesToBmp(unityTexturesToBmp);
                    porter.Process(job.path.toStdString(), job.outputDir.toStdString());
                    ok = true;
                    break;
                }
                default: break;
                }
                setRowStatusLater(job.item, ok ? tr("Done") : tr("Failed"), ok ? done : failed);
                finished[i] = true;
                finishedCount++;
            }
            updateProgressSafe(static_cast<int>(finishedCount * 100 / jobs.size()));
        }

        // Cleanup callbacks
        RezExtractor::SetProgressFunc(nullptr);
        PakExtractor::SetProgressFunc(nullptr);
        XfsExtractor::SetProgressFunc(nullptr);
        VpkExtractor::SetProgressFunc(nullptr);
        GmaExtractor::SetProgressFunc(nullptr);

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
                QMessageBox::warning(this, tr("Finished with Errors"), errorMsg);
                m_extractionErrors.clear();
            }
        }, Qt::QueuedConnection);
    });
}

void MainWindow::onOpenSettings()
{
    SettingsDialog dlg(this);
    dlg.exec();
}

void MainWindow::onOpenLoLModelDownloader()
{
    openToolOnce("lol", tr("LoL Models"), []() { return new LoLModelDownloadDialog(nullptr); });
}

void MainWindow::onOpenSteamWorkshopDownloader()
{
    openToolOnce("workshop", tr("Steam Workshop"), [this]() {
        auto* downloader = new SteamWorkshopDownloaderDialog(nullptr);
        connect(downloader, &SteamWorkshopDownloaderDialog::openFiles, this, &MainWindow::openPaths);
        return downloader;
    });
}

#ifdef METIN2_SCRIPT_EFFECT
void MainWindow::onOpenMseViewer()
{
    openToolTab(new MseViewerWindow(), tr("MSE Viewer"));
}
#endif

#ifdef ENABLE_AUTORIG
void MainWindow::onOpenAutoRigDialog()
{
    openToolOnce("autorig", tr("Auto-Rig"), []() { return new AutoRigDialog(); });
}

void MainWindow::onOpenPhysicsLabDialog()
{
    openToolOnce("physicslab", QStringLiteral("Physics Lab"), []() { return new PhysicsLabDialog(); });
}
#endif

void MainWindow::onOpenAudioConverter()
{
    openToolOnce("audio", tr("WAV Converter"), []() { return new AudioConvertDialog(); });
}

#ifdef ENABLE_AUTORIG
void MainWindow::onOpenMapPorter()
{
    openToolOnce("map", QStringLiteral("GR2 to Map"), []() { return new MapPortDialog(); });
}
#endif

void MainWindow::onOpenRetargetDialog()
{
    openToolOnce("retarget", tr("Animation Retargeting"), []() { return new RetargetDialog(); });
}

void MainWindow::onOpenHudSprite()
{
    openToolOnce("hudsprite", tr("HUD Sprite"), []() { return new HudSpriteDialog(); });
}

ConvertAssets MainWindow::runConverter(IConverter& converter, const QString& inputPath, const QString& outputDir,
                                       const std::function<void(const ConvertResult&)>& afterFile)
{
    const ConvertAssets summary = ConvertAll(converter, inputPath.toStdString(), outputDir.toStdString(),
        [this, &afterFile](int index, int total, const std::string&, const ConvertResult& result) {
            for (const std::string& qc : result.qcFiles)
                compileModelIfWanted(QString::fromStdString(qc));
            if (afterFile)
                afterFile(result);
            updateProgressSafe((index + 1) * 100 / total);
            return !m_cancelRequested;
        });

    if (summary.total == 0)
    {
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("^3No %1 files found in '%2'.").arg(converter.Name()).arg(inputPath));
    }
    else if (summary.total > 1)
    {
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2%1 of %2 %3 files converted.").arg(summary.converted).arg(summary.total).arg(converter.Name()));
    }
    for (const auto& [file, error] : summary.failed)
    {
        m_extractionErrors.push_back({ QFileInfo(QString::fromStdString(file)).fileName(), QString::fromStdString(error), 0 });
    }
    return summary;
}

bool MainWindow::convertFbxGlb(const QString& inputPath, const QString& outputDir, bool writeQC)
{
    FbxGlbConvertOptions options;
    options.writeQC = writeQC;
    FbxGlbConverter converter;
    converter.SetOptions(options);
    const ConvertAssets summary = runConverter(converter, inputPath, outputDir);
    return summary.total > 0 && summary.failed.empty();
}

bool MainWindow::convertDtx(const QString& inputPath, const QString& outputDir)
{
    DtxConverter converter;
    const ConvertAssets summary = runConverter(converter, inputPath, outputDir);
    return summary.total > 0 && summary.failed.empty();
}

void MainWindow::compileModelIfWanted(const QString& qcPath)
{
    if (!m_compileModelCheck || !m_compileModelCheck->isChecked() || !m_writeQCCheck->isChecked())
        return;

    VortigauntLog::Vortigaunt_Printf(QStringLiteral("  Compiling %1 ...").arg(QFileInfo(qcPath).fileName()));
    const QCCompileResult result = QCFile::Compile(SettingsDialog::getStudioMdlPath().toStdString(), qcPath.toStdString());
    if (result.Ok)
    {
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("  ^2Compiled:^7 %1").arg(QString::fromStdString(result.ModelPath)));
    }
    else
    {
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("  ^1Compile failed:^7 %1").arg(QString::fromStdString(result.Error)));
    }
}

void MainWindow::updateWeaponModelControls()
{
    if (!m_weaponTypeCombo)
        return;

    const bool needsReference = static_cast<WeaponType>(m_weaponTypeCombo->currentData().toInt()) == WeaponType::Reference;
    m_weaponReferenceEdit->parentWidget()->setVisible(needsReference);
}

bool MainWindow::convertViewModel(const QString& inputPath, const QString& outputDir, const WeaponModelOptions& options, QStringList* pQcs)
{
    WeaponModelConverter converter;
    converter.SetOptions(options);
    const ConvertAssets summary = runConverter(converter, inputPath, outputDir, [pQcs](const ConvertResult& result) {
        for (const std::string& qc : result.qcFiles)
        {
            const QString path = QString::fromStdString(qc);
            if (pQcs && QFileInfo(path).fileName().startsWith(QStringLiteral("p_"), Qt::CaseInsensitive))
                pQcs->append(path);
        }
    });
    return summary.total > 0 && summary.failed.empty();
}

void MainWindow::showSelectedOnPlayer()
{
    QTreeWidgetItem* item = m_fileList->currentItem();
    const QString qc = item ? item->data(0, kPModelRole).toString() : QString();
    if (qc.isEmpty())
    {
        const bool converted = item && item->data(0, kConvertedRole).toBool();
        QMessageBox::information(this, tr("Show on player"),
                                 converted ? tr("The p_ model needs its QC to be compiled. Turn on Write QC and convert the file again.")
                                           : tr("Convert this file first; its p_ model is shown afterwards."));
        return;
    }

    // the QC puts the model next to itself; compiled now when it is missing or older than the QC
    const QFileInfo qcInfo(qc);
    QString model = qcInfo.dir().filePath(qcInfo.completeBaseName() + QStringLiteral(".mdl"));
    const QFileInfo modelInfo(model);
    if (!modelInfo.exists() || modelInfo.lastModified() < qcInfo.lastModified())
    {
        const QString studiomdl = SettingsDialog::getStudioMdlPath();
        if (!QFileInfo(studiomdl).isFile())
        {
            QMessageBox::warning(this, tr("Show on player"),
                                 tr("The p_ model has to be compiled to be shown. Set the location of studiomdl in Settings."));
            return;
        }
        QApplication::setOverrideCursor(Qt::WaitCursor);
        const QCCompileResult result = QCFile::Compile(studiomdl.toStdString(), qc.toStdString());
        QApplication::restoreOverrideCursor();
        if (!result.Ok)
        {
            QMessageBox::warning(this, tr("Show on player"), tr("studiomdl could not compile %1:\n\n%2")
                                 .arg(qcInfo.fileName(), QString::fromStdString(result.Error)));
            return;
        }
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2Compiled:^7 %1").arg(QString::fromStdString(result.ModelPath)));
        if (!result.ModelPath.empty())
            model = QString::fromStdString(result.ModelPath);
    }
    openWeaponFit(model);
}

bool MainWindow::convertLtb(const QString& inputPath, const QString& outputDir, const ltbConverterSetting& settings)
{
    ltbConverter converter;
    converter.SetConvertSetting(settings);
    const ConvertAssets summary = runConverter(converter, inputPath, outputDir);
    return summary.total > 0 && summary.failed.empty();
}

void MainWindow::extractArchiveRez(const QStringList& paths, const QString& outputDir)
{
    if (paths.isEmpty())
    {
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("WARNING: No REZ files selected."));
        return;
    }


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
        VortigauntLog::Vortigaunt_Printf(QStringLiteral("WARNING: No VPK/GMA files selected."));
        return;
    }

    Platform::setHighPriority();

    const int totalFiles = paths.size();
    const bool isBatch = totalFiles > 1;
    int successCount = 0;

    VpkExtractor vpkExtractor;
    GmaExtractor gmaExtractor;

    VpkExtractor::SetProgressFunc([this](int percent) {
        updateProgressSafe(percent);
    });
    GmaExtractor::SetProgressFunc([this](int percent) {
        updateProgressSafe(percent);
    });

    for (int i = 0; i < totalFiles; ++i)
    {
        const QString& archivePath = paths.at(i);
        const bool isGma = QFileInfo(archivePath).suffix().compare(QStringLiteral("gma"), Qt::CaseInsensitive) == 0;
        const QString fmtName = isGma ? QStringLiteral("GMA") : QStringLiteral("VPK");

        if (isBatch)
        {
            int filePercent = ((i + 1) * 100) / totalFiles;
            QMetaObject::invokeMethod(this, [this, filePercent, i, totalFiles, fmtName]() {
                if (m_progressBar) {
                    m_progressBar->setValue(filePercent);
                    m_progressBar->setFormat(tr("%1 %2/%3: %p%").arg(fmtName).arg(i + 1).arg(totalFiles));
                }
            }, Qt::QueuedConnection);
        }

        // Determine output directory
        QString targetDir;
        if (isBatch && m_separateFoldersCheck->isChecked())
            targetDir = QDir(outputDir).filePath(QFileInfo(archivePath).completeBaseName());
        else
            targetDir = outputDir.isEmpty() ? SettingsDialog::getExtractedOutputDir() : outputDir;

        VortigauntLog::Vortigaunt_Printf(QStringLiteral("^5Extracting %1:^7 ^4%2^7 -> ^4%3^7").arg(fmtName, archivePath, targetDir));

        bool ok = isGma
            ? gmaExtractor.ExtractSingle(archivePath.toStdString(), targetDir.toStdString())
            : vpkExtractor.ExtractSingle(archivePath.toStdString(), targetDir.toStdString());

        if (ok) {
            ++successCount;
        } else {
            ExtractionError err;
            err.filename = archivePath;
            err.errorMessage = tr("%1 extraction failed or was incomplete").arg(fmtName);
            m_extractionErrors.push_back(err);
        }
    }

    VpkExtractor::SetProgressFunc(nullptr);
    GmaExtractor::SetProgressFunc(nullptr);
    Platform::restoreNormalPriority();

    VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2VPK/GMA extraction complete.^7 ^3%1^7 of ^3%2^7 files processed.").arg(successCount).arg(totalFiles));
}







bool MainWindow::convertGr2(const QString& inputPath, const QString& outputDir, bool withAnimations)
{
    Gr2ConverterSettings settings;
    settings.ExportMeshes = !m_ignoreMeshesCheck->isChecked();
    settings.ExportAnimations = !m_ignoreAnimsCheck->isChecked();
    settings.ExportSkeleton = true;
    settings.ExportMaterials = true;
    settings.MirrorUVY = SettingsDialog::getMirrorUVY();

    Gr2Converter gr2Converter;
    gr2Converter.SetConverterSettings(settings);

    // the animation list belongs to a single model
    const std::filesystem::path start = std::filesystem::path(inputPath.toStdWString());
    const bool isSingleFile = std::filesystem::is_regular_file(start) && withAnimations;
    const ConvertAssets summary = runConverter(gr2Converter, inputPath, outputDir, [&](const ConvertResult& result) {
        if (isSingleFile && result.ok)
            exportGr2Animations(gr2Converter, start, FileIO::toPath(result.outputFolder));
    });
    return summary.total > 0 && summary.failed.empty();
}

void MainWindow::exportGr2Animations(Gr2Converter& gr2Converter, const std::filesystem::path& gr2File, const std::filesystem::path& outFolder)
{
    std::filesystem::path outPath = outFolder / gr2File.filename();
    outPath.replace_extension(".smd");

    
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
        QString inputDir = QFileInfo(QString::fromStdWString(gr2File.wstring())).absolutePath();
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
                    QString mainGr2Path = QString::fromStdWString(gr2File.wstring());
                    
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
    if (m_writeQCCheck->isChecked())
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
            compileModelIfWanted(QString::fromStdWString(qcPath.wstring()));
        }
        else
        {
            VortigauntLog::Vortigaunt_Printf(QStringLiteral("  Failed to create QC file: %1").arg(QString::fromStdString(qcWriter.GetError())));
        }
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
    if (!mimeData->hasUrls())
        return;
    QStringList paths;
    for (const QUrl& url : mimeData->urls())
    {
        if (url.isLocalFile())
            paths << url.toLocalFile();
    }
    // on Convert everything goes in the list; on another tab each file goes where it belongs
    if (m_workspaces->currentIndex() == 0)
    {
        addPaths(paths);
    }
    else if (m_workspaces->currentWidget() == m_textureWorkspace)
    {
        // images and folders stay in Textures
        QStringList textures;
        QStringList others;
        for (const QString& p : paths)
            (QFileInfo(p).isDir() || TextureWorkspace::isTexture(p) ? textures : others) << p;
        m_textureWorkspace->addFiles(textures);
        openPaths(others);
    }
    else
    {
        openPaths(paths);
    }
    event->acceptProposedAction();
}

// Status bar
void MainWindow::setupStatusBar()
{
    m_statusLabel = new QLabel(QString("Version: %1").arg(VORTIGAUNT_VERSION_STRING));
    m_statusLabel->setStyleSheet("QLabel { padding: 0 6px; font-size: 11px; }");
    statusBar()->addWidget(m_statusLabel);

    m_outputButton = new QPushButton(tr("Output: %1").arg(QDir::toNativeSeparators(outputDir())));
    m_outputButton->setFlat(true);
    m_outputButton->setCursor(Qt::PointingHandCursor);
    m_outputButton->setToolTip(tr("Open the output folder"));
    m_outputButton->setStyleSheet("QPushButton { padding: 0 6px; font-size: 11px; border: none; text-align: left; }");
    connect(m_outputButton, &QPushButton::clicked, this, &MainWindow::onOpenOutputFolder);
    statusBar()->addWidget(m_outputButton, 1);
    
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
    connect(m_shortcutBrowseInput, &QShortcut::activated, this, &MainWindow::onAddFiles);
    
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
    
    VortigauntLog::Vortigaunt_Printf(QStringLiteral("^2Log exported to: ^3%1").arg(fileName));
}

// Clear log
void MainWindow::onClearLog()
{
    m_logEdit->clear();
}





bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::KeyPress && watched->property("vortigauntTool").toBool())
    {
        const int key = static_cast<QKeyEvent*>(event)->key();
        if (key == Qt::Key_Escape)
            return true;
    }

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

    if (processing && m_progressBar)
    {
        m_progressBar->setVisible(true);
        m_progressBar->setValue(0);
    }

    // nothing that changes the list while it is worked through
    m_runAction->setEnabled(!processing);
    m_stopAction->setEnabled(processing);
    m_addFilesAction->setEnabled(!processing);
    m_addFolderAction->setEnabled(!processing);
    m_clearListAction->setEnabled(!processing);
    m_outputFolderAction->setEnabled(!processing);
    m_fileOpCombo->setEnabled(!processing && !m_fileList->selectedItems().isEmpty());
    if (m_settingsAction) m_settingsAction->setDisabled(processing);
    // what the finished work brought, like a p_ model to show on the player
    if (!processing)
        onFileSelectionChanged();
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

