// Copyright (C) 2026 iQuit (https://github.com/iQuitt)
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "ShellActionDialog.h"
#include "SettingsDialog.h"
#include "core/QCFile.h"
#include "core/converters/FbxGlbConverter.h"
#include "core/mdl/StudioDecompiler.h"
#include "ui/windows/ModelViewerWindow.h"
#include "utils/FileIO.h"

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QListWidget>
#include <QLocalServer>
#include <QLocalSocket>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QSplitter>
#include <QUrl>
#include <QVBoxLayout>
#include <thread>

#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#endif

namespace
{
enum Role
{
    PathRole = Qt::UserRole,
    FolderRole,
    ModelRole,
    DetailsRole
};

QString serverName(const QString& action)
{
    return QStringLiteral("VortigauntShell-") + action;
}

// hands the files to a window already open for the command
bool sendToRunning(const QString& action, const QStringList& files)
{
    QLocalSocket socket;
    socket.connectToServer(serverName(action));
    if (!socket.waitForConnected(300)) return false;
    socket.write(files.join(QLatin1Char('\n')).toUtf8());
    socket.flush();
    socket.waitForBytesWritten(2000);
    socket.disconnectFromServer();
    return true;
}

void openViewer(const QString& model)
{
    auto* viewer = new ModelViewerWindow();
    viewer->setAttribute(Qt::WA_DeleteOnClose);
    viewer->load(model);
    viewer->show();
}
}

ShellActionDialog::ShellActionDialog(const QString& action, QWidget* parent)
    : QDialog(parent), m_action(action)
{
    if (action == QLatin1String("decompile")) setWindowTitle(tr("Decompile model"));
    else if (action == QLatin1String("convert")) setWindowTitle(tr("Convert to GoldSrc"));
    else setWindowTitle(tr("Compile model"));
    resize(640, 420);

    auto* layout = new QVBoxLayout(this);
    auto* splitter = new QSplitter(Qt::Vertical);
    m_list = new QListWidget();
    m_list->setObjectName(QStringLiteral("files"));
    splitter->addWidget(m_list);
    m_details = new QPlainTextEdit();
    m_details->setReadOnly(true);
    m_details->setPlaceholderText(tr("Details of the selected file show here"));
    splitter->addWidget(m_details);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);

    m_progress = new QProgressBar();
    m_progress->setRange(0, 0);
    m_progress->setTextVisible(false);
    m_progress->setMaximumHeight(6);
    m_progress->hide();
    layout->addWidget(m_progress);

    auto* buttons = new QHBoxLayout();
    buttons->addStretch();
    m_openButton = new QPushButton(tr("Open folder"));
    m_viewButton = new QPushButton(tr("View model"));
    auto* closeButton = new QPushButton(tr("Close"));
    m_openButton->setEnabled(false);
    m_viewButton->setEnabled(false);
    buttons->addWidget(m_openButton);
    buttons->addWidget(m_viewButton);
    buttons->addWidget(closeButton);
    layout->addLayout(buttons);

    connect(m_list, &QListWidget::currentItemChanged, this, &ShellActionDialog::showItem);
    connect(m_openButton, &QPushButton::clicked, this, [this]() {
        if (const QListWidgetItem* item = m_list->currentItem())
            QDesktopServices::openUrl(QUrl::fromLocalFile(item->data(FolderRole).toString()));
    });
    connect(m_viewButton, &QPushButton::clicked, this, [this]() {
        if (const QListWidgetItem* item = m_list->currentItem()) openViewer(item->data(ModelRole).toString());
    });
    connect(closeButton, &QPushButton::clicked, this, &ShellActionDialog::reject);
}

void ShellActionDialog::reject()
{
    if (!m_busy) QDialog::reject();
}

void ShellActionDialog::addFiles(const QStringList& files)
{
    for (const QString& file : files)
    {
        const QString path = QDir::toNativeSeparators(QFileInfo(file).absoluteFilePath());
        if (file.isEmpty() || m_queue.contains(path)) continue;
        auto* item = new QListWidgetItem(tr("%1: waiting").arg(QFileInfo(path).fileName()), m_list);
        item->setData(PathRole, path);
        m_queue << path;
    }
    if (!m_list->currentItem() && m_list->count()) m_list->setCurrentRow(0);
    processNext();
}

void ShellActionDialog::processNext()
{
    if (m_busy || m_queue.isEmpty())
    {
        m_progress->setVisible(m_busy);
        return;
    }
    const QString file = m_queue.takeFirst();
    QListWidgetItem* item = nullptr;
    for (int i = 0; i < m_list->count() && !item; ++i)
        if (m_list->item(i)->data(PathRole).toString() == file && m_list->item(i)->data(DetailsRole).isNull()) item = m_list->item(i);
    if (!item) return processNext();
    const QString name = QFileInfo(file).fileName();

    // asked here, on the window's thread
    if (m_action == QLatin1String("decompile"))
    {
        const QDir folder(QFileInfo(file).dir().filePath(QString::fromStdString(FileIO::sanitizePathComponent(QFileInfo(file).completeBaseName().toStdString()))));
        if (folder.exists() && !folder.isEmpty())
        {
            if (m_overwrite < 0)
                m_overwrite = QMessageBox::question(this, windowTitle(), tr("%1 already exists. Overwrite the files in it?").arg(QDir::toNativeSeparators(folder.path())),
                                                    QMessageBox::Yes | QMessageBox::No) == QMessageBox::Yes;
            if (!m_overwrite)
            {
                Outcome skipped;
                skipped.summary = tr("skipped, the folder already exists");
                skipped.folder = folder.path();
                finish(item, skipped);
                return processNext();
            }
        }
    }
    QString studiomdl;
    if (m_action.startsWith(QLatin1String("compile")))
    {
        studiomdl = SettingsDialog::getStudioMdlPath();
        if (!QFileInfo(studiomdl).isFile())
        {
            studiomdl = QFileDialog::getOpenFileName(this, tr("Select studiomdl"), QString(), tr("Programs (*.exe);;All files (*.*)"));
            if (!studiomdl.isEmpty()) SettingsDialog::setStudioMdlPath(studiomdl);
        }
    }

    m_busy = true;
    m_progress->show();
    item->setText(tr("%1: working...").arg(name));
    m_list->setCurrentItem(item);
    std::thread([this, file, studiomdl, item]() {
        const Outcome outcome = work(file, studiomdl);
        QMetaObject::invokeMethod(this, [this, item, outcome]() {
            m_busy = false;
            finish(item, outcome);
            if (outcome.ok && m_action == QLatin1String("compileview") && !outcome.model.isEmpty()) openViewer(outcome.model);
            processNext();
        }, Qt::QueuedConnection);
    }).detach();
}

ShellActionDialog::Outcome ShellActionDialog::work(const QString& file, const QString& studiomdl) const
{
    Outcome outcome;
    const std::string path = file.toStdString();
    const QString folder = QFileInfo(file).absolutePath();
    if (m_action == QLatin1String("decompile"))
    {
        const DecompileResult result = DecompileStudioModel(path, folder.toStdString());
        outcome.ok = result.ok;
        outcome.folder = QString::fromStdString(result.folder);
        outcome.model = file;
        outcome.summary = result.ok ? tr("%1 SMD files, %2 textures").arg(result.smdFiles).arg(result.textures) : QString::fromStdString(result.error);
        QStringList lines;
        if (result.ok) lines << tr("QC: %1").arg(QDir::toNativeSeparators(QString::fromStdString(result.qcPath)));
        for (const std::string& warning : result.warnings) lines << QString::fromStdString(warning);
        outcome.details = lines.join(QLatin1Char('\n'));
    }
    else if (m_action == QLatin1String("convert"))
    {
        FbxGlbConverter converter;
        const FbxGlbConvertReport report = converter.Convert(path, folder.toStdString());
        outcome.ok = report.ok;
        outcome.folder = report.ok ? QString::fromStdString(report.outputFolder) : folder;
        outcome.summary = report.ok ? tr("%1 bones, %2 animations").arg(report.bones).arg(report.animations.size())
                                    : QString::fromStdString(report.error);
        QStringList lines;
        if (!report.qcPath.empty()) lines << tr("QC: %1").arg(QDir::toNativeSeparators(QString::fromStdString(report.qcPath)));
        for (const std::string& warning : report.warnings) lines << QString::fromStdString(warning);
        outcome.details = lines.join(QLatin1Char('\n'));
    }
    else
    {
        const QCCompileResult result = QCFile::Compile(studiomdl.toStdString(), path);
        outcome.ok = result.Ok;
        outcome.model = result.Ok ? QString::fromStdString(result.ModelPath) : QString();
        outcome.folder = result.Ok ? QFileInfo(outcome.model).absolutePath() : folder;
        outcome.summary = result.Ok ? tr("written to %1").arg(QFileInfo(outcome.model).fileName()) : QString::fromStdString(result.Error);
        outcome.details = QString::fromStdString(result.Output);
    }
    return outcome;
}

void ShellActionDialog::finish(QListWidgetItem* item, const Outcome& outcome)
{
    item->setText(QStringLiteral("%1: %2").arg(QFileInfo(item->data(PathRole).toString()).fileName(), outcome.summary));
    item->setForeground(outcome.ok ? QColor(76, 175, 80) : QColor(229, 57, 53));
    item->setData(FolderRole, outcome.folder);
    item->setData(ModelRole, outcome.model);
    item->setData(DetailsRole, outcome.details.isEmpty() ? outcome.summary : outcome.summary + QStringLiteral("\n\n") + outcome.details);
    if (item == m_list->currentItem()) showItem();
}

void ShellActionDialog::showItem()
{
    const QListWidgetItem* item = m_list->currentItem();
    m_details->setPlainText(item ? item->data(DetailsRole).toString() : QString());
    // a failed compile's log ends with the reason
    m_details->moveCursor(QTextCursor::End);
    m_openButton->setEnabled(item && QFileInfo(item->data(FolderRole).toString()).isDir());
    m_viewButton->setEnabled(item && QFileInfo(item->data(ModelRole).toString()).isFile());
}

int ShellActionDialog::run(const QString& action, const QStringList& files)
{
    if (action == QLatin1String("view"))
    {
        for (const QString& file : files) openViewer(file);
        return QApplication::exec();
    }
    if (sendToRunning(action, files)) return 0;
    auto* server = new QLocalServer(qApp);
    // another launch may have started listening a moment ago
    if (!server->listen(serverName(action)) && sendToRunning(action, files)) return 0;

    auto* dialog = new ShellActionDialog(action);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    QObject::connect(server, &QLocalServer::newConnection, dialog, [server, dialog]() {
        while (QLocalSocket* socket = server->nextPendingConnection())
        {
            auto* received = new QByteArray();
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [socket, received]() { *received += socket->readAll(); });
            QObject::connect(socket, &QLocalSocket::disconnected, dialog, [socket, received, dialog]() {
                *received += socket->readAll();
                dialog->addFiles(QString::fromUtf8(*received).split(QLatin1Char('\n'), Qt::SkipEmptyParts));
                dialog->raise();
                delete received;
                socket->deleteLater();
            });
        }
    });
    dialog->show();
    dialog->addFiles(files);
    return QApplication::exec();
}

#ifdef Q_OS_WIN
namespace
{
const QString kMenuKey = QStringLiteral("Software\\Classes\\SystemFileAssociations\\%1\\shell\\VortigauntTool");

struct MenuItem
{
    const char* extensions;
    const char* key;
    const char* action;
};

const MenuItem kMenuItems[] = {
    { ".mdl", "1decompile", "decompile" }, { ".mdl", "2view", "view" },       { ".qc", "1compile", "compile" },
    { ".qc", "2compileview", "compileview" }, { ".fbx .glb .gltf", "1convert", "convert" },
};

bool setString(const QString& key, const wchar_t* name, const QString& value)
{
    return RegSetKeyValueW(HKEY_CURRENT_USER, reinterpret_cast<const wchar_t*>(key.utf16()), name, REG_SZ, value.utf16(),
                           static_cast<DWORD>((value.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
}

QString exePath()
{
    return QDir::toNativeSeparators(QCoreApplication::applicationFilePath());
}

QStringList menuExtensions()
{
    QStringList extensions;
    for (const MenuItem& item : kMenuItems)
        for (const QString& ext : QString::fromLatin1(item.extensions).split(QLatin1Char(' ')))
            if (!extensions.contains(ext)) extensions << ext;
    return extensions;
}
}

bool ShellActionDialog::registerContextMenu()
{
    unregisterContextMenu();
    const QString icon = exePath() + QStringLiteral(",0");
    bool ok = true;
    for (const QString& ext : menuExtensions())
    {
        const QString menu = kMenuKey.arg(ext);
        ok = setString(menu, L"MUIVerb", QStringLiteral("VortigauntTool")) && ok;
        ok = setString(menu, L"Icon", icon) && ok;
        ok = setString(menu, L"SubCommands", QString()) && ok;
    }
    for (const MenuItem& item : kMenuItems)
    {
        const QString action = QString::fromLatin1(item.action);
        QString text;
        if (action == QLatin1String("decompile")) text = tr("Decompile model");
        else if (action == QLatin1String("view")) text = tr("View model");
        else if (action == QLatin1String("compile")) text = tr("Compile model");
        else if (action == QLatin1String("compileview")) text = tr("Compile and view");
        else text = tr("Convert to GoldSrc");
        const QString command = QStringLiteral("\"%1\" --shell %2 \"%3\"").arg(exePath(), action, QStringLiteral("%1"));
        for (const QString& ext : QString::fromLatin1(item.extensions).split(QLatin1Char(' ')))
        {
            const QString verb = kMenuKey.arg(ext) + QStringLiteral("\\shell\\") + QString::fromLatin1(item.key);
            ok = setString(verb, L"MUIVerb", text) && ok;
            ok = setString(verb, L"Icon", icon) && ok;
            ok = setString(verb + QStringLiteral("\\command"), nullptr, command) && ok;
        }
    }
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
    return ok;
}

void ShellActionDialog::unregisterContextMenu()
{
    for (const QString& ext : menuExtensions())
        RegDeleteTreeW(HKEY_CURRENT_USER, reinterpret_cast<const wchar_t*>(kMenuKey.arg(ext).utf16()));
    SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, nullptr, nullptr);
}

bool ShellActionDialog::isContextMenuRegistered()
{
    // registered, and for this copy of the program
    const QString key = kMenuKey.arg(QStringLiteral(".mdl")) + QStringLiteral("\\shell\\1decompile\\command");
    wchar_t value[1024];
    DWORD size = sizeof(value);
    if (RegGetValueW(HKEY_CURRENT_USER, reinterpret_cast<const wchar_t*>(key.utf16()), nullptr, RRF_RT_REG_SZ, nullptr, value, &size) != ERROR_SUCCESS)
        return false;
    return QString::fromWCharArray(value).contains(exePath(), Qt::CaseInsensitive);
}
#endif
