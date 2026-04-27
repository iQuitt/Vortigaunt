#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QSettings>

class SettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SettingsDialog(QWidget* parent = nullptr);

    static QString getModelViewerPath();
    static bool getDeveloperMode();
    static bool getMirrorUVY();
	static bool getDiscordRpcMode();
    static void setModelViewerPath(const QString& path);
    static void setDeveloperMode(bool enabled);
    static void setMirrorUVY(bool enabled);
	static void setDiscordRpcMode(bool enabled);

    static QString getDefaultExtractPath();
    static void setDefaultExtractPath(const QString& path);

    // Output directory helpers
    static QString getDefaultOutputDir();
    static QString getExtractedOutputDir();
    static QString getLoLOutputDir(bool isDownloadFolder);


private slots:
    void onBrowseModelViewer();
    void onBrowseExtractPath();
    void onSave();

private:
    void setupUI();
    void loadSettings();

    QLineEdit* m_modelViewerEdit;
    QLineEdit* m_extractPathEdit;
    QCheckBox* m_developerModeCheck;
	QCheckBox* m_discordRpcCheck;
    QCheckBox* m_darkModeCheck;
    QPushButton* m_browseModelViewerButton;
    QPushButton* m_browseExtractPathButton;
};
