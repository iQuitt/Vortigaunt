#include "SettingsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QPushButton>
#include <QMessageBox>
#include <QProcess>
#include <QApplication>
#include <QCoreApplication>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>

#include "LanguageManager.h"
#include "core/VortigauntLog.h"

static const QString SETTINGS_ORG = "Vortigaunt";
static const QString SETTINGS_APP = "VortigauntTool";
static const QString KEY_MODEL_VIEWER = "GoldSrc/ModelViewerPath";
static const QString KEY_DEVELOPER_MODE = "General/DeveloperMode";
static const QString KEY_MIRROR_UV_Y = "GR2/MirrorUVY";
static const QString KEY_DISCORD_RPC = "General/DiscordRPC";
static const QString KEY_EXTRACT_PATH = "General/ExtractPath";

SettingsDialog::SettingsDialog(QWidget* parent)
    : QDialog(parent)
{
    setupUI();
    loadSettings();
}

void SettingsDialog::setupUI()
{
    setWindowTitle(tr("Settings"));
    resize(500, 200);

    auto* mainLayout = new QVBoxLayout(this);

    auto* viewersGroup = new QGroupBox(tr("Model Viewers (Half-Life)"));
    auto* viewersLayout = new QVBoxLayout();

    // Model Viewer
    auto* modelLayout = new QHBoxLayout();
    modelLayout->addWidget(new QLabel(tr("Model Viewer (.mdl):")));
    m_modelViewerEdit = new QLineEdit();
    m_modelViewerEdit->setPlaceholderText(tr("Path your Half-Life Model Viewer..."));
    m_browseModelViewerButton = new QPushButton(tr("Browse..."));
    modelLayout->addWidget(m_modelViewerEdit, 1);
    modelLayout->addWidget(m_browseModelViewerButton);
    viewersLayout->addLayout(modelLayout);

    viewersGroup->setLayout(viewersLayout);

    // Paths Group
    auto* pathsGroup = new QGroupBox(tr("Paths"));
    auto* pathsLayout = new QVBoxLayout();

    auto* extractLayout = new QHBoxLayout();
    extractLayout->addWidget(new QLabel(tr("Default Extract Path:")));
    m_extractPathEdit = new QLineEdit();
    m_extractPathEdit->setPlaceholderText(tr("Leave empty for Desktop..."));
    m_browseExtractPathButton = new QPushButton(tr("Browse..."));
    extractLayout->addWidget(m_extractPathEdit, 1);
    extractLayout->addWidget(m_browseExtractPathButton);
    pathsLayout->addLayout(extractLayout);

    pathsGroup->setLayout(pathsLayout);

    // General Settings Group
    auto* generalGroup = new QGroupBox(tr("General"));
    auto* generalLayout = new QVBoxLayout();
    
    m_developerModeCheck = new QCheckBox(tr("Enable Developer Mode"));
    generalLayout->addWidget(m_developerModeCheck);

	m_discordRpcCheck = new QCheckBox(tr("Enable Discord Rich Presence"));
	generalLayout->addWidget(m_discordRpcCheck);
    
    generalGroup->setLayout(generalLayout);
    
    // Buttons
    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto* saveButton = new QPushButton(tr("Save"));
    auto* cancelButton = new QPushButton(tr("Cancel"));
    buttonLayout->addWidget(saveButton);
    buttonLayout->addWidget(cancelButton);

    mainLayout->addWidget(viewersGroup);
    mainLayout->addWidget(pathsGroup);
    mainLayout->addWidget(generalGroup);
    mainLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    // Connections
    connect(m_browseModelViewerButton, &QPushButton::clicked, this, &SettingsDialog::onBrowseModelViewer);
    connect(m_browseExtractPathButton, &QPushButton::clicked, this, &SettingsDialog::onBrowseExtractPath);
    connect(saveButton, &QPushButton::clicked, this, &SettingsDialog::onSave);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void SettingsDialog::loadSettings()
{
    m_modelViewerEdit->setText(getModelViewerPath());
    m_extractPathEdit->setText(getDefaultExtractPath());
    m_developerModeCheck->setChecked(getDeveloperMode());
	m_discordRpcCheck->setChecked(getDiscordRpcMode());
}

void SettingsDialog::onBrowseModelViewer()
{
    QString path = QFileDialog::getOpenFileName(
        this,
        tr("Select Model Viewer Executable"),
        m_modelViewerEdit->text(),
        tr("Executables (*.exe);;All files (*.*)")
    );

    if (!path.isEmpty())
        m_modelViewerEdit->setText(path);
}

void SettingsDialog::onBrowseExtractPath()
{
    QString path = QFileDialog::getExistingDirectory(
        this,
        tr("Select Default Extract Directory"),
        m_extractPathEdit->text()
    );

    if (!path.isEmpty())
        m_extractPathEdit->setText(path);
}

void SettingsDialog::onSave()
{
    setModelViewerPath(m_modelViewerEdit->text().trimmed());
    setDefaultExtractPath(m_extractPathEdit->text().trimmed());
    setDeveloperMode(m_developerModeCheck->isChecked());
	setDiscordRpcMode(m_discordRpcCheck->isChecked());
    accept();
}

QString SettingsDialog::getModelViewerPath()
{
    QSettings settings(SETTINGS_ORG, SETTINGS_APP);
    return settings.value(KEY_MODEL_VIEWER, "").toString();
}

void SettingsDialog::setModelViewerPath(const QString& path)
{
    QSettings settings(SETTINGS_ORG, SETTINGS_APP);
    settings.setValue(KEY_MODEL_VIEWER, path);
}

QString SettingsDialog::getDefaultExtractPath()
{
    QSettings settings(SETTINGS_ORG, SETTINGS_APP);
    return settings.value(KEY_EXTRACT_PATH, "").toString();
}

void SettingsDialog::setDefaultExtractPath(const QString& path)
{
    QSettings settings(SETTINGS_ORG, SETTINGS_APP);
    settings.setValue(KEY_EXTRACT_PATH, path);
}

bool SettingsDialog::getDeveloperMode()
{
    return VortigauntLog::isDeveloperMode();
}

void SettingsDialog::setDeveloperMode(bool enabled)
{
    VortigauntLog::setDeveloperMode(enabled);
}

bool SettingsDialog::getDiscordRpcMode()
{
    QSettings settings(SETTINGS_ORG, SETTINGS_APP);
    return settings.value(KEY_DISCORD_RPC, false).toBool();
}

void SettingsDialog::setDiscordRpcMode(bool enabled)
{
    QSettings settings(SETTINGS_ORG, SETTINGS_APP);
    settings.setValue(KEY_DISCORD_RPC, enabled);
}

bool SettingsDialog::getMirrorUVY()
{
    QSettings settings(SETTINGS_ORG, SETTINGS_APP);
	// we need enable generally for gr2. most models need it for metin2. except face textures and some map textures and others.
    return settings.value(KEY_MIRROR_UV_Y, true).toBool(); 
}

void SettingsDialog::setMirrorUVY(bool enabled)
{
    QSettings settings(SETTINGS_ORG, SETTINGS_APP);
    settings.setValue(KEY_MIRROR_UV_Y, enabled);
}

static bool isAppDirTrulyWritable()
{
    static bool checked = false;
    static bool isWritable = false;
    
    if (checked) return isWritable;
    
    QString appDir = QCoreApplication::applicationDirPath();
    QString testFile = QDir(appDir).filePath(".vortigaunt_write_test");
    QFile file(testFile);
    if (file.open(QIODevice::WriteOnly)) {
        file.close();
        file.remove();
        isWritable = true;
    } else {
        isWritable = false;
    }
    
    checked = true;
    return isWritable;
}

QString SettingsDialog::getDefaultOutputDir()
{
    QString appDir = QCoreApplication::applicationDirPath();
    
    // Check if portable (executable directory is truly writable)
    if (isAppDirTrulyWritable()) {
        return QDir(appDir).filePath("VortigauntOutput");
    }
    
    QString customPath = getDefaultExtractPath();
    if (!customPath.isEmpty()) {
        return QDir(customPath).filePath("VortigauntExtracted");
    }
    
    // MSI installed (Program Files), fallback to Desktop
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).filePath("VortigauntExtracted");
}

QString SettingsDialog::getExtractedOutputDir()
{
    QString appDir = QCoreApplication::applicationDirPath();
    
    // Check if portable
    if (isAppDirTrulyWritable()) {
        return QDir(appDir).filePath("VortigauntExtracted");
    }
    
    QString customPath = getDefaultExtractPath();
    if (!customPath.isEmpty()) {
        return QDir(customPath).filePath("VortigauntExtracted");
    }
    
    // MSI installed (Program Files)
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).filePath("VortigauntExtracted");
}

QString SettingsDialog::getLoLOutputDir(bool isDownloadFolder)
{
    QString appDir = QCoreApplication::applicationDirPath();
    
    // Check if portable
    if (isAppDirTrulyWritable()) {
        return QDir(appDir).filePath(isDownloadFolder ? "VortigauntLoLDownloads" : "VortigauntLoL");
    }
    
    QString customPath = getDefaultExtractPath();
    if (!customPath.isEmpty()) {
        return QDir(customPath).filePath("VortigauntExtracted/VortigauntLoL");
    }
    
    // MSI installed (Program Files) - unify folders as requested
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).filePath("VortigauntExtracted/VortigauntLoL");
}


