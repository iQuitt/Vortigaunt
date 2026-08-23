#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QSettings>

class QLabel;

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    static QString getModelViewerPath();
    static bool getDeveloperMode();
    static bool getMirrorUVY();
	static bool getDiscordRpcMode();
    static bool getDarkThemeMode();
    static void setModelViewerPath(const QString& path);
    static void setDeveloperMode(bool enabled);
    static void setMirrorUVY(bool enabled);
	static void setDiscordRpcMode(bool enabled);
    static void setDarkThemeMode(bool enabled);

    static QString getDefaultExtractPath();
    static void setDefaultExtractPath(const QString& path);

    // Output directory helpers
    static QString getDefaultOutputDir();
    static QString getExtractedOutputDir();
    static QString getLoLOutputDir(bool isDownloadFolder);
    // Always "<writable base>/VortigauntOutput", whatever the install type
    static QString getOutputRootDir();
    // Start directory for "Select Output Directory" dialogs (falls back to home if it does not exist yet)
    static QString getExtractStartDir();


private slots:
    void onBrowseModelViewer();
    void onBrowseExtractPath();
    void onSave();
#ifdef Q_OS_WIN
    void onRegisterThumbnailer();
    void onUnregisterThumbnailer();
    void onRegisterVtfThumbnailer();
    void onUnregisterVtfThumbnailer();
#endif

private:
    void setupUI();
    void loadSettings();
#ifdef Q_OS_WIN
    void updateThumbnailerStatus();
    void updateVtfThumbnailerStatus();
    bool callThumbnailerDllFunction(const QString& dllFileName, const char* functionName);
#endif

    QLineEdit* m_modelViewerEdit;
    QLineEdit* m_extractPathEdit;
    QCheckBox* m_developerModeCheck;
	QCheckBox* m_discordRpcCheck;
    QCheckBox* m_darkModeCheck;
    QPushButton* m_browseModelViewerButton;
    QPushButton* m_browseExtractPathButton;

#ifdef Q_OS_WIN
    QLabel* m_thumbnailerStatusLabel{nullptr};
    QPushButton* m_registerThumbnailerBtn{nullptr};
    QPushButton* m_unregisterThumbnailerBtn{nullptr};

    QLabel* m_vtfThumbnailerStatusLabel{nullptr};
    QPushButton* m_registerVtfThumbnailerBtn{nullptr};
    QPushButton* m_unregisterVtfThumbnailerBtn{nullptr};
#endif
};

