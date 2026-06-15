#pragma once

#include <QMainWindow>
#include <type_traits>
#include <utility>
#include <QThread>
#include <atomic>
#include <QTimer>
#include "core/VortigauntLog.h"
class LtbConverter;
struct ltbConverterSetting;

class QLineEdit;
class QPushButton;
class QComboBox;
class QCheckBox;
class QPlainTextEdit;
class QAction;
class QListWidget;
class QGroupBox;
class QLabel;
class QShortcut;
class QProgressBar;
class QMenu;
class VortigauntUpdateCheck;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    
    // Drag & drop support
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    // UI event handlers (signal-slot connections)
    void onBrowseInput();

    void onBrowseOutput();

    void onRun();

    void onOpenDtxViewer();

    void onOpenPakViewer();

    void onOpenRezViewer();

    void onOpenXfsViewer();

    void onOpenSpriteViewer();

    void onOpenLithtechSpriteViewer();

    void onOpenLoLModelDownloader();

#ifdef METIN2_SCRIPT_EFFECT
    void onOpenMseViewer();
#endif

    void onOpenAutoRigDialog();
    
    void onOpenSettings();

    void onOpenWadMaker();

    void onOpenVpkViewer();

    void onOpenAudioConverter();

    void onLogFilterChanged(int index);

    void onExportLog();

    void onClearLog();

    void onCheckForUpdates();


private:
    void setupStatusBar();

    void setupKeyboardShortcuts();

    // Extract Archives
    void extractArchiveRez(const QStringList& paths, const QString& outputDir);

    void extractArchivePak(const QStringList& paths, const QString& outputDir);

    void extractArchiveXfs(const QStringList& paths, const QString& outputDir);

    void extractArchiveVpk(const QStringList& paths, const QString& outputDir);

    // Model/Texture Convert
    void convertLtb(const QString& inputPath, const QString& outputDir, const ltbConverterSetting& settings);

    void convertGr2(const QString& inputPath, const QString& outputDir);

    // UI
    void updateOperationComboForFile(const QString& filePath);

    void loadGr2Animations();

    void gr2AddAnimationItem(const QString& fileName, const QString& filePath);

    void handleDroppedFiles(const QList<QUrl>& urls);


    QLineEdit*     m_inputPathEdit;
    QPushButton*   m_browseInputButton;
    QLineEdit*     m_outputPathEdit;
    QPushButton*   m_browseOutputButton;
    QComboBox*     m_operationCombo;
    QComboBox*     m_outputFormatCombo;
    QLabel*        m_outputFormatLabel;
    QCheckBox*     m_ignoreMeshesCheck;
    QCheckBox*     m_ignoreAnimsCheck;
    QCheckBox*     m_mirrorUVYCheck;
    QCheckBox*     m_invertAlphaCheck;  // For inverting alpha in GR2 DDS textures
    QCheckBox*     m_writeQCCheck;      // For generating QC file when exporting GR2 to SMD
    QCheckBox*     m_separateFoldersCheck;  // For multiple extraction
    QPlainTextEdit* m_logEdit;
    
    QGroupBox*     m_gr2AnimGroup;
    QListWidget*   m_animationListWidget;
    QPushButton* m_runButton;


    
    // Language menu
    QMenu*         m_languageMenu;
    
    // Menu actions for retranslation
    QAction*       m_dtxViewAction; // Texture of Lithtech Engine
    QAction*       m_pakViewAction; // Counter Strike Online PAK Archive
    QAction*       m_rezViewAction; // Lithtech Engine Archive
	QAction*       m_xfsViewAction; // Xenesis File System Archive
    QAction*       m_vpkViewAction; // Source Engine VPK Archive
    QAction*       m_spriteViewAction; // Goldsrc Sprite Viewer
	QAction*       m_lithtechSpriteAction; // Lithtech Sprite Viewer
	QAction*       m_lolModelAction; // Khada LoL Model Downloader
    QAction*       m_mseViewAction; // Metin2 Script Effect
	QAction*       m_autoRigAction; // Auto-Rig (Beta)
    QAction*       m_wadMakerAction;
    QAction*       m_audioConvertAction;
    QAction*       m_settingsAction;
    QMenu*         m_toolsMenu;
    QMenu*         m_settingsMenu;
    QAction*       m_checkUpdateAction;
    VortigauntUpdateCheck* m_updateChecker;
    

    QAction* m_qtAboutAction;
    QAction* m_vortiAboutAction;

   
    
    // Log filtering
    QComboBox*     m_logFilterCombo;
    QPushButton*   m_saveLogButton;
    QPushButton*   m_clearLogButton;
    VortigauntLog::LogLevel m_currentLogFilter;
    
    // Status bar
    QLabel*        m_statusLabel;
    QProgressBar*  m_progressBar;
    
    // Keyboard shortcuts
    QShortcut*     m_shortcutBrowseInput;
    QShortcut*     m_shortcutBrowseOutput;
    QShortcut*     m_shortcutRun;
    QShortcut*     m_shortcutClearLog;
    
    // Re-entrancy protection for processEvents
    std::atomic<bool> m_processingEvents;
    
    // Error tracking
    struct ExtractionError {
        QString filename;
        QString errorMessage;
        size_t entryIndex;
    };
    std::vector<ExtractionError> m_extractionErrors;
    
    // Multiple file selection support
    QStringList m_selectedInputFiles;

    // Background Threading Support
    bool m_isProcessing{ false };
    void runInBackground(std::function<void()> task);
    void setUiProcessing(bool processing);
    void updateProgressSafe(int percent);
        

    

    
};


