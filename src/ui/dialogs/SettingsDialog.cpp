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
#include <QFile>
#include <QLibrary>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

#include "LanguageManager.h"
#include "ThemeManager.h"
#include "core/VortigauntLog.h"

static const QString SETTINGS_ORG = "Vortigaunt";
static const QString SETTINGS_APP = "VortigauntTool";
static const QString KEY_MODEL_VIEWER = "GoldSrc/ModelViewerPath";
static const QString KEY_DEVELOPER_MODE = "General/DeveloperMode";
static const QString KEY_MIRROR_UV_Y = "GR2/MirrorUVY";
static const QString KEY_DISCORD_RPC = "General/DiscordRPC";
static const QString KEY_EXTRACT_PATH = "General/ExtractPath";
static const QString KEY_DARK_THEME = "General/DarkTheme";

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

    m_darkModeCheck = new QCheckBox(tr("Enable Dark Theme"));
    generalLayout->addWidget(m_darkModeCheck);

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
#ifdef Q_OS_WIN
    auto* shellGroup = new QGroupBox(tr("Shells"));
    auto* shellLayout = new QVBoxLayout();

    m_thumbnailerStatusLabel = new QLabel();
    shellLayout->addWidget(m_thumbnailerStatusLabel);

    auto* shellButtonsLayout = new QHBoxLayout();
    m_registerThumbnailerBtn = new QPushButton(tr("Register DTX Thumbnailer"));
    m_unregisterThumbnailerBtn = new QPushButton(tr("Unregister DTX Thumbnailer"));
    shellButtonsLayout->addWidget(m_registerThumbnailerBtn);
    shellButtonsLayout->addWidget(m_unregisterThumbnailerBtn);
    shellLayout->addLayout(shellButtonsLayout);

    m_vtfThumbnailerStatusLabel = new QLabel();
    shellLayout->addWidget(m_vtfThumbnailerStatusLabel);

    auto* vtfShellButtonsLayout = new QHBoxLayout();
    m_registerVtfThumbnailerBtn = new QPushButton(tr("Register VTF Thumbnailer"));
    m_unregisterVtfThumbnailerBtn = new QPushButton(tr("Unregister VTF Thumbnailer"));
    vtfShellButtonsLayout->addWidget(m_registerVtfThumbnailerBtn);
    vtfShellButtonsLayout->addWidget(m_unregisterVtfThumbnailerBtn);
    shellLayout->addLayout(vtfShellButtonsLayout);

    shellGroup->setLayout(shellLayout);
    mainLayout->addWidget(shellGroup);
#endif
    mainLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    // Connections
    connect(m_browseModelViewerButton, &QPushButton::clicked, this, &SettingsDialog::onBrowseModelViewer);
    connect(m_browseExtractPathButton, &QPushButton::clicked, this, &SettingsDialog::onBrowseExtractPath);
    connect(saveButton, &QPushButton::clicked, this, &SettingsDialog::onSave);
    connect(cancelButton, &QPushButton::clicked, this, &QDialog::reject);
#ifdef Q_OS_WIN
    connect(m_registerThumbnailerBtn, &QPushButton::clicked, this, &SettingsDialog::onRegisterThumbnailer);
    connect(m_unregisterThumbnailerBtn, &QPushButton::clicked, this, &SettingsDialog::onUnregisterThumbnailer);
    connect(m_registerVtfThumbnailerBtn, &QPushButton::clicked, this, &SettingsDialog::onRegisterVtfThumbnailer);
    connect(m_unregisterVtfThumbnailerBtn, &QPushButton::clicked, this, &SettingsDialog::onUnregisterVtfThumbnailer);
#endif
}

void SettingsDialog::loadSettings()
{
    m_modelViewerEdit->setText(getModelViewerPath());
    m_extractPathEdit->setText(getDefaultExtractPath());
    m_developerModeCheck->setChecked(getDeveloperMode());
	m_discordRpcCheck->setChecked(getDiscordRpcMode());
    m_darkModeCheck->setChecked(getDarkThemeMode());
#ifdef Q_OS_WIN
    updateThumbnailerStatus();
    updateVtfThumbnailerStatus();
#endif
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

    if (m_darkModeCheck->isChecked() != getDarkThemeMode())
    {
        setDarkThemeMode(m_darkModeCheck->isChecked());
        ThemeManager::apply(m_darkModeCheck->isChecked());
    }

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

bool SettingsDialog::getDarkThemeMode()
{
    QSettings settings(SETTINGS_ORG, SETTINGS_APP);
    return settings.value(KEY_DARK_THEME, true).toBool();
}

void SettingsDialog::setDarkThemeMode(bool enabled)
{
    QSettings settings(SETTINGS_ORG, SETTINGS_APP);
    settings.setValue(KEY_DARK_THEME, enabled);
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
        return QDir(appDir).filePath("VortigauntLoL");
    }
    
    QString customPath = getDefaultExtractPath();
    if (!customPath.isEmpty()) {
        return QDir(customPath).filePath("VortigauntExtracted/VortigauntLoL");
    }
    
    return QDir(QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)).filePath("VortigauntExtracted/VortigauntLoL");
}

#ifdef Q_OS_WIN
void SettingsDialog::updateThumbnailerStatus()
{
    if (!m_thumbnailerStatusLabel || !m_registerThumbnailerBtn || !m_unregisterThumbnailerBtn)
        return;

    HKEY hKey;
    LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{C4066FE0-59CE-452D-B9B2-E7B6C8A604EA}", 0, KEY_READ, &hKey);
    bool isRegistered = (status == ERROR_SUCCESS);
    if (isRegistered)
    {
        RegCloseKey(hKey);
        m_thumbnailerStatusLabel->setText(tr("DTX Thumbnailer Status: Active"));
        m_thumbnailerStatusLabel->setStyleSheet("color: #4CAF50; font-weight: bold;");
        m_registerThumbnailerBtn->setEnabled(false);
        m_unregisterThumbnailerBtn->setEnabled(true);
    }
    else
    {
        m_thumbnailerStatusLabel->setText(tr("DTX Thumbnailer Status: Inactive"));
        m_thumbnailerStatusLabel->setStyleSheet("color: gray;");
        m_registerThumbnailerBtn->setEnabled(true);
        m_unregisterThumbnailerBtn->setEnabled(false);
    }
}

// Loads a shell-extension DLL from the application directory and calls
// DllRegisterServer / DllUnregisterServer on it. Returns true on success.
bool SettingsDialog::callThumbnailerDllFunction(const QString& dllFileName, const char* functionName)
{
    QString dllPath = QDir(QCoreApplication::applicationDirPath()).filePath(dllFileName);
    if (!QFile::exists(dllPath))
    {
        QMessageBox::critical(this, tr("Error"), tr("%1 not found in application directory:\n%2").arg(dllFileName, dllPath));
        return false;
    }

    QLibrary lib(dllPath);
    if (!lib.load())
    {
        QMessageBox::critical(this, tr("Error"), tr("Failed to load %1:\n%2").arg(dllFileName, lib.errorString()));
        return false;
    }

    typedef HRESULT(STDAPICALLTYPE* PFN_DllServerFunc)();
    auto fn = (PFN_DllServerFunc)lib.resolve(functionName);
    if (!fn)
    {
        QMessageBox::critical(this, tr("Error"), tr("%1 export not found in %2.").arg(QString::fromLatin1(functionName), dllFileName));
        return false;
    }

    HRESULT hr = fn();
    lib.unload();

    if (!SUCCEEDED(hr))
    {
        QMessageBox::critical(this, tr("Error"), tr("%1 failed for %2. HRESULT: 0x%3").arg(QString::fromLatin1(functionName), dllFileName, QString::number(hr, 16)));
        return false;
    }

    return true;
}

void SettingsDialog::onRegisterThumbnailer()
{
    if (callThumbnailerDllFunction(QStringLiteral("DtxThumbnailProvider.dll"), "DllRegisterServer"))
    {
        QMessageBox::information(this, tr("Success"), tr("DTX Thumbnailer has been successfully registered."));
    }
    updateThumbnailerStatus();
}

void SettingsDialog::onUnregisterThumbnailer()
{
    if (callThumbnailerDllFunction(QStringLiteral("DtxThumbnailProvider.dll"), "DllUnregisterServer"))
    {
        QMessageBox::information(this, tr("Success"), tr("DTX Thumbnailer has been successfully unregistered."));
    }
    updateThumbnailerStatus();
}

void SettingsDialog::updateVtfThumbnailerStatus()
{
    if (!m_vtfThumbnailerStatusLabel || !m_registerVtfThumbnailerBtn || !m_unregisterVtfThumbnailerBtn)
        return;

    HKEY hKey;
    LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER, L"Software\\Classes\\CLSID\\{8D2F7A31-4B6C-4E95-9A47-C1E52D80B6F3}", 0, KEY_READ, &hKey);
    bool isRegistered = (status == ERROR_SUCCESS);
    if (isRegistered)
    {
        RegCloseKey(hKey);
        m_vtfThumbnailerStatusLabel->setText(tr("VTF Thumbnailer Status: Active"));
        m_vtfThumbnailerStatusLabel->setStyleSheet("color: #4CAF50; font-weight: bold;");
        m_registerVtfThumbnailerBtn->setEnabled(false);
        m_unregisterVtfThumbnailerBtn->setEnabled(true);
    }
    else
    {
        m_vtfThumbnailerStatusLabel->setText(tr("VTF Thumbnailer Status: Inactive"));
        m_vtfThumbnailerStatusLabel->setStyleSheet("color: gray;");
        m_registerVtfThumbnailerBtn->setEnabled(true);
        m_unregisterVtfThumbnailerBtn->setEnabled(false);
    }
}

void SettingsDialog::onRegisterVtfThumbnailer()
{
    if (callThumbnailerDllFunction(QStringLiteral("VtfThumbnailProvider.dll"), "DllRegisterServer"))
    {
        QMessageBox::information(this, tr("Success"), tr("VTF Thumbnailer has been successfully registered."));
    }
    updateVtfThumbnailerStatus();
}

void SettingsDialog::onUnregisterVtfThumbnailer()
{
    if (callThumbnailerDllFunction(QStringLiteral("VtfThumbnailProvider.dll"), "DllUnregisterServer"))
    {
        QMessageBox::information(this, tr("Success"), tr("VTF Thumbnailer has been successfully unregistered."));
    }
    updateVtfThumbnailerStatus();
}
#endif



