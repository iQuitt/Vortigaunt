#pragma once

#include <QMainWindow>
#include <atomic>
#include <filesystem>
#include <functional>
#include "core/VortigauntLog.h"
#include "core/converters/IConverter.h"

class Gr2Converter;
struct ltbConverterSetting;
struct WeaponModelOptions;

class QAction;
class QCheckBox;
class QComboBox;
class QDockWidget;
class QDoubleSpinBox;
class QGroupBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QMenu;
class QPlainTextEdit;
class QProgressBar;
class QPushButton;
class QShortcut;
class QStackedWidget;
class QTabWidget;
class QTreeWidget;
class QTreeWidgetItem;
class ArchiveWorkspace;
class GLBViewer;
class RibbonBar;
class TextureWorkspace;
class StudioModelView;
class SpriteViewerWindow;
class WadMakerDialog;
class WeaponFitWorkspace;
#ifdef ENABLE_AUTORIG
class HandsWorkspace;
#endif
class VortigauntUpdateCheck;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

    // files go in as they are, folders by every file in them that has an operation
    void addPaths(const QStringList& paths);

    // empty: the default from the settings
    void setOutputDir(const QString& folder);

    // shows the Archives tab with the archive open in it
    void openArchive(const QString& path);

    // shows the Textures tab with the files (or folders of them) open in it
    void openTextures(const QStringList& paths);

    // shows the Sprites tab with the sprite open in it
    void openSprite(const QString& path);

    // shows the WAD tab with the WAD open in it
    void openWad(const QString& path);

    // shows the P Model tab with the p_ model in the player's hands
    void openWeaponFit(const QString& path);

    // each file where it belongs: archives, textures, sprites and WADs in their tabs, the rest in the Convert list
    void openPaths(const QStringList& paths);

    // Drag & drop support
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onAddFiles();

    void onAddFolder();

    void onBrowseOutput();

    void onOpenOutputFolder();

    void onRun();

    void onStop();

    void onFileSelectionChanged();

    void onWorkspaceChanged(int index);

    void onOpenLoLModelDownloader();

    void onOpenSteamWorkshopDownloader();

#ifdef METIN2_SCRIPT_EFFECT
    void onOpenMseViewer();
#endif

#ifdef ENABLE_AUTORIG
    void onOpenAutoRigDialog();
    void onOpenPhysicsLabDialog();
    void onOpenMapPorter();
#endif

    void onOpenSettings();

    void onOpenAudioConverter();

    void onOpenHudSprite();
    void onOpenRetargetDialog();

    void onLogFilterChanged(int index);

    void onExportLog();

    void onClearLog();

    void onCheckForUpdates();

private:
    // what is done with a file in the list, picked from its name
    enum class FileOp
    {
        None,
        Ltb,
        Gr2,
        Mesh,
        Weapon,
        Dtx,
        Rez,
        Pak,
        Xfs,
        Vpk,
        Unity,
    };

    struct FileJob
    {
        QTreeWidgetItem* item;
        QString path;
        FileOp op;
        QString outputDir;
    };

    void setupRibbon();

    void setupDocks();

    void setupPreview();

    void setupStatusBar();

    // a tool in a tab of the centre instead of a window of its own
    void openToolTab(QWidget* tool, const QString& title);

    void closeToolTab(QWidget* tool);

    // one tab for the tool; opening it again shows that tab
    void openToolOnce(const char* key, const QString& title, const std::function<QWidget*()>& create);

    // a tab of the centre that is kept when it is closed, with the ribbon tab that belongs to it
    struct Workspace
    {
        QWidget* widget;
        int ribbonTab;
        QString title;
    };

    void showWorkspace(QWidget* widget);

    const Workspace* workspaceOf(QWidget* widget) const;

    void setupKeyboardShortcuts();

    static FileOp detectFileOp(const QString& path);

    static QString fileOpName(FileOp op);

    void addFileRow(const QString& path, const QString& outputSubfolder);

    // UI thread only
    void setRowStatus(QTreeWidgetItem* item, const QString& status, const QColor& color);

    // from a worker thread
    void setRowStatusLater(QTreeWidgetItem* item, const QString& status, const QColor& color);

    QString outputDir() const;

    void updatePropertiesFor(FileOp op);

    void showPreview(const QString& path, FileOp op);

    // Extract Archives
    void extractArchiveRez(const QStringList& paths, const QString& outputDir);

    void extractArchivePak(const QStringList& paths, const QString& outputDir);

    void extractArchiveXfs(const QStringList& paths, const QString& outputDir);

    void extractArchiveVpk(const QStringList& paths, const QString& outputDir);

    // Model/Texture Convert; true when every file converted
    bool convertLtb(const QString& inputPath, const QString& outputDir, const ltbConverterSetting& settings);

    bool convertGr2(const QString& inputPath, const QString& outputDir, bool withAnimations);

    bool convertFbxGlb(const QString& inputPath, const QString& outputDir, bool writeQC);

    // pQcs gets the QC files of the p_ models it wrote
    bool convertViewModel(const QString& inputPath, const QString& outputDir, const WeaponModelOptions& options, QStringList* pQcs = nullptr);

    // the p_ model converted from the selected row, compiled when it is not yet, in the hands of the player
    void showSelectedOnPlayer();

    bool convertDtx(const QString& inputPath, const QString& outputDir);

    // runs a converter over a file or folder: progress, QC compiling and the failures for the summary box
    ConvertAssets runConverter(IConverter& converter, const QString& inputPath, const QString& outputDir,
                                const std::function<void(const ConvertResult&)>& afterFile = {});

    // the animations picked in the list, and the QC, for a single GR2 model
    void exportGr2Animations(Gr2Converter& gr2Converter, const std::filesystem::path& gr2File, const std::filesystem::path& outFolder);

    // Compiles a QC that was just written, when "Compile model" is ticked.
    void compileModelIfWanted(const QString& qcPath);

    // The reference model field only when the weapon type asks for one.
    void updateWeaponModelControls();

    void loadGr2Animations(const QString& gr2Path);

    void gr2AddAnimationItem(const QString& fileName, const QString& filePath);

    // Ribbon, workspaces and panels
    RibbonBar*     m_ribbon = nullptr;
    std::vector<Workspace> m_persistentWorkspaces;
    int            m_toolsRibbonTab = -1;
    QTabWidget*    m_workspaces = nullptr;
    ArchiveWorkspace* m_archiveWorkspace = nullptr;
    TextureWorkspace* m_textureWorkspace = nullptr;
    SpriteViewerWindow* m_spriteWorkspace = nullptr;
    WadMakerDialog* m_wadWorkspace = nullptr;
    WeaponFitWorkspace* m_weaponFitWorkspace = nullptr;
#ifdef ENABLE_AUTORIG
    HandsWorkspace* m_handsWorkspace = nullptr;
#endif
    bool           m_convertDocksShown = true;
    bool           m_filesDockWasVisible = true;
    bool           m_propertiesDockWasVisible = true;
    QDockWidget*   m_filesDock = nullptr;
    QDockWidget*   m_propertiesDock = nullptr;
    QDockWidget*   m_logDock = nullptr;
    QByteArray     m_defaultLayout;

    QAction*       m_addFilesAction = nullptr;
    QAction*       m_addFolderAction = nullptr;
    QAction*       m_clearListAction = nullptr;
    QAction*       m_runAction = nullptr;
    QAction*       m_stopAction = nullptr;
    QAction*       m_writeQCAction = nullptr;
    QAction*       m_compileAction = nullptr;
    QAction*       m_outputFolderAction = nullptr;
    QAction*       m_openOutputAction = nullptr;
    QAction*       m_resetLayoutAction = nullptr;

    // Files panel
    QTreeWidget*   m_fileList = nullptr;
    QLabel*        m_fileCountLabel = nullptr;
    QString        m_outputDir;  // empty: the default from the settings

    // Properties panel
    QLabel*        m_selectedNameLabel = nullptr;
    QComboBox*     m_fileOpCombo = nullptr;
    QLabel*        m_noSelectionLabel = nullptr;
    QGroupBox*     m_weaponGroup = nullptr;
    QGroupBox*     m_modelGroup = nullptr;
    QGroupBox*     m_archiveGroup = nullptr;
    QGroupBox*     m_unityGroup = nullptr;
    QGroupBox*     m_textureGroup = nullptr;

    // Preview
    QStackedWidget* m_preview = nullptr;
    QLabel*        m_previewHint = nullptr;
    GLBViewer*     m_modelPreview = nullptr;
    QLabel*        m_imagePreview = nullptr;
    StudioModelView* m_studioPreview = nullptr;  // GoldSrc .mdl
    QLabel*        m_previewInfo = nullptr;
    QPushButton*   m_previewOpenButton = nullptr;
    QString        m_previewPath;

    QCheckBox*     m_ignoreMeshesCheck;
    QCheckBox*     m_ignoreAnimsCheck;
    QCheckBox*     m_mirrorUVYCheck;
    QCheckBox*     m_writeQCCheck;      // For generating QC file when exporting GR2 to SMD
    QCheckBox*     m_compileModelCheck = nullptr;  // run studiomdl on every QC that gets written

    // V model -> P/W model
    QDoubleSpinBox* m_weaponScaleSpin = nullptr;   // 0 = worked out from the hands
    QComboBox*      m_weaponMirrorCombo = nullptr; // auto / yes / no
    QComboBox*      m_gameDirCombo = nullptr;
    QComboBox*      m_weaponTypeCombo = nullptr;   // how the weapon is held: gun, knife, grenade, as a reference model
    QLineEdit*      m_weaponReferenceEdit = nullptr;
    QPushButton*    m_weaponReferenceBrowse = nullptr;
    QCheckBox*      m_weaponUpsideDownCheck = nullptr;
    QPushButton*    m_showOnPlayerButton = nullptr;
    QCheckBox*     m_separateFoldersCheck;  // For multiple extraction
    QCheckBox*     m_unityBmpCheck;         // Unity mode: convert textures to 8-bit BMP (unchecked = 32-bit PNG)
    QPlainTextEdit* m_logEdit;

    QGroupBox*     m_gr2AnimGroup;
    QListWidget*   m_animationListWidget;
    QString        m_animationListOwner;  // the GR2 the list was filled for

    // Tool actions
    QAction*       m_lolModelAction = nullptr; // Khada LoL Model Downloader
    QAction*       m_mseViewAction = nullptr; // Metin2 Script Effect
#ifdef ENABLE_AUTORIG
    QAction*       m_autoRigAction = nullptr; // Auto-Rig (Beta)
    QAction*       m_physicsLabAction = nullptr;
    QAction*       m_mapPortAction = nullptr;
#endif
    QAction*       m_audioConvertAction = nullptr;
    QAction*       m_hudSpriteAction = nullptr;
    QAction*       m_retargetAction = nullptr;
    QAction*       m_steamWorkshopAction = nullptr;
    QAction*       m_settingsAction = nullptr;
    QMenu*         m_settingsMenu = nullptr;
    QMenu*         m_languageMenu = nullptr;
    QAction*       m_checkUpdateAction = nullptr;
    VortigauntUpdateCheck* m_updateChecker;

    QAction* m_qtAboutAction = nullptr;
    QAction* m_vortiAboutAction = nullptr;

    // Log filtering
    QComboBox*     m_logFilterCombo;
    QPushButton*   m_saveLogButton;
    QPushButton*   m_clearLogButton;
    VortigauntLog::LogLevel m_currentLogFilter;

    // Status bar
    QLabel*        m_statusLabel;
    QPushButton*   m_outputButton = nullptr;
    QProgressBar*  m_progressBar;

    // Keyboard shortcuts
    QShortcut*     m_shortcutBrowseInput;
    QShortcut*     m_shortcutBrowseOutput;
    QShortcut*     m_shortcutRun;
    QShortcut*     m_shortcutClearLog;

    // Re-entrancy protection for processEvents
    std::atomic<bool> m_processingEvents;
    std::atomic<bool> m_cancelRequested{ false };

    // Error tracking
    struct ExtractionError {
        QString filename;
        QString errorMessage;
        size_t entryIndex;
    };
    std::vector<ExtractionError> m_extractionErrors;

    // Background Threading Support
    bool m_isProcessing{ false };
    void runInBackground(std::function<void()> task);
    void setUiProcessing(bool processing);
    void updateProgressSafe(int percent);
};
