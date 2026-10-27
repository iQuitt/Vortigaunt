// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include <QApplication>
#include <QSurfaceFormat>
#include <type_traits>
#include <utility>
#include <QIcon>
#include <QFileInfo>
#include <QFile>
#include <QStyleFactory>
#include <QPalette>
#include <QColor>
#include <QDir>
#include <QTimer>
#include <QStandardPaths>
#include "ThemeManager.h"
#include "Discord_Integration.h"

// CRT Debug Heap - Memory Leak Detection (Windows Debug builds only)
#if defined(_WIN32) && defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>
#endif

#include "MainWindow.h"

#ifdef METIN2_SCRIPT_EFFECT
#include "windows/MseViewerWindow.h"
#endif
#include "dialogs/SettingsDialog.h"

#include "LanguageManager.h"
#include "utils/Platform.h"

#include "core/VortigauntLog.h"
#include "core/extractors/ExtractArchive.h"
#include "core/extractors/rez/RezExtractor.h"
#include "core/extractors/pak/PakExtractor.h"
#include "core/extractors/xfs/XfsExtractor.h"
#include "core/extractors/unity/UnityFsExtractor.h"

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    ThemeManager::apply(SettingsDialog::getDarkThemeMode());


    bool isInstalled = false;
#ifdef _WIN32
    QString appDir = QCoreApplication::applicationDirPath().toLower();
    QString programFiles = QString::fromLocal8Bit(qgetenv("ProgramFiles")).toLower();
    QString programFilesX86 = QString::fromLocal8Bit(qgetenv("ProgramFiles(x86)")).toLower();
    
    if ((!programFiles.isEmpty() && appDir.startsWith(programFiles)) || (!programFilesX86.isEmpty() && appDir.startsWith(programFilesX86))) 
    {
        isInstalled = true;
    }
#endif

    QString logDir;
    bool usePortable = false;
    
    if (!isInstalled) {
        QString testPath = QDir(QCoreApplication::applicationDirPath()).filePath(".write_test");
        QFile testFile(testPath);
        if (testFile.open(QIODevice::WriteOnly)) {
            testFile.close();
            testFile.remove();
            usePortable = true;
        }
    }
    
    if (usePortable) {
        logDir = QCoreApplication::applicationDirPath();
    } else {
        logDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(logDir); 
    }
    
    QString logPath = QDir(logDir).filePath("VortigauntTool.log");
    VortigauntLog::Initialize(logPath.toStdString());
    
    auto& registry = ExtractArchive::Instance();
    
    // TODO: Next time i need implement this.
    registry.Register(std::make_shared<RezExtractor>());
    registry.Register(std::make_shared<PakExtractor>());
    registry.Register(std::make_shared<XfsExtractor>());
    registry.Register(std::make_shared<UnityFsExtractor>());
    registry.Init();


    app.setWindowIcon(QIcon(":/app2.ico"));
    
    LanguageManager::instance();

    static Discord_Integration discord;
    static QTimer discordTimer;

    QTimer::singleShot(0, [&app]() {
        bool discordEnabled = SettingsDialog::getDiscordRpcMode();

        if (discordEnabled) {
            discord.Initialize();

            QObject::connect(&discordTimer, &QTimer::timeout, []() {
                discord.RunCallbacks();
            });
            discordTimer.start(16);

            QObject::connect(&app, &QCoreApplication::aboutToQuit, []() {
                discord.Shutdown();
            });
        }
        else
        {
            VortigauntLog::Vortigaunt_Printf("Discord Rich Presence is disabled.");
        }
    });

    QStringList args = app.arguments();
    if (args.size() > 1) {
        QString filePath = args.at(1);
        QFileInfo fi(filePath);
        
        if (fi.exists()) {
            QString ext = fi.suffix().toLower();
            
            // a Metin2 effect opens in its viewer alone, without the main window
#ifdef METIN2_SCRIPT_EFFECT
            // Metin2 Script Effect Viewer
            if (ext == "mse") {
                MseViewerWindow* mseViewer = new MseViewerWindow();
                mseViewer->setAttribute(Qt::WA_DeleteOnClose);
                mseViewer->show();
                
                QMetaObject::invokeMethod(mseViewer, [mseViewer, filePath]() {
                    mseViewer->loadMse(filePath);
                }, Qt::QueuedConnection);
                
                return app.exec();
            }
#endif
            
            
        }
    }





    MainWindow wndw;
    wndw.show();
    if (args.size() > 1)
        wndw.openPaths(args.mid(1));

    return app.exec();
}
