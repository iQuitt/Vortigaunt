#include <QApplication>
#include <QSurfaceFormat>
#include <type_traits>
#include <utility>
#include <QIcon>
#include <QFileInfo>
#include <QStyleFactory>
#include <QPalette>
#include <QColor>
#include <QDir>
#include <QTimer>
#include <QStandardPaths>
#include "Discord_Integration.h"

// CRT Debug Heap - Memory Leak Detection (Windows Debug builds only)
#if defined(_WIN32) && defined(_DEBUG)
#define _CRTDBG_MAP_ALLOC
#include <cstdlib>
#include <crtdbg.h>
#endif

#include "MainWindow.h"

#include "windows/SpriteViewerWindow.h"
#include "windows/RezViewerWindow.h"
#include "windows/XfsViewerWindow.h"
#ifdef METIN2_SCRIPT_EFFECT
#include "windows/MseViewerWindow.h"
#endif
#include "windows/PakViewerWindow.h"
#include "dialogs/DtxViewerDialog.h"
#include "dialogs/WadMakerDialog.h"
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

    
    // Use AppData directory for log file to avoid admin permission issues in Program Files
    QString appDataDir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(appDataDir); // Ensure directory exists
    QString logPath = QDir(appDataDir).filePath("VortigauntTool.log");
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
            

           // These are the you can open directly through File Explorer without manually opening  Vortigaunt.
		   // you cant see the main window of the vortigaunt if you open these file types, it will just open the viewer directly. 
   

            // Goldsrc Sprite Viewer 
            // TODO: Detect Lithtech Sprite. (DONE)
            
            if (ext == "spr") {
                SpriteViewerWindow* spriteViewer = new SpriteViewerWindow();
                spriteViewer->setAttribute(Qt::WA_DeleteOnClose);
                spriteViewer->show();
                
                QMetaObject::invokeMethod(spriteViewer, [spriteViewer, filePath]() {
                    spriteViewer->openSprite(filePath);
                }, Qt::QueuedConnection);
                
                return app.exec();
            }
            
            //  REZ Viewer
            if (ext == "rez") {
                RezViewerWindow* rezViewer = new RezViewerWindow();
                rezViewer->setAttribute(Qt::WA_DeleteOnClose);
                rezViewer->show();
                
                QMetaObject::invokeMethod(rezViewer, [rezViewer, filePath]() {
                    rezViewer->loadRez(filePath);
                }, Qt::QueuedConnection);
                
                return app.exec();
            }
            
            // Xenesis File Viewer
            if (ext == "xfs") {
                XfsViewerWindow* xfsViewer = new XfsViewerWindow();
                xfsViewer->setAttribute(Qt::WA_DeleteOnClose);
                xfsViewer->show();
                
                QMetaObject::invokeMethod(xfsViewer, [xfsViewer, filePath]() {
                    xfsViewer->loadXfs(filePath);
                }, Qt::QueuedConnection);
                
                return app.exec();
            }
            
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
            
            // DTX Viewer
            if (ext == "dtx") {
                DtxViewerDialog* dtxViewer = new DtxViewerDialog();
                dtxViewer->setAttribute(Qt::WA_DeleteOnClose);
                dtxViewer->show();
                
                QMetaObject::invokeMethod(dtxViewer, [dtxViewer, filePath]() {
                    dtxViewer->loadDtxFile(filePath);
                }, Qt::QueuedConnection);
                
                return app.exec();
            }
            
            // WAD Maker
            if (ext == "wad") {
                WadMakerDialog* wadMaker = new WadMakerDialog();
                wadMaker->setAttribute(Qt::WA_DeleteOnClose);
                wadMaker->show();
                
                QMetaObject::invokeMethod(wadMaker, [wadMaker, filePath]() {
                    wadMaker->loadWad(filePath);
                }, Qt::QueuedConnection);
                
                return app.exec();
            }
            
            // PAK Viewer
            if (ext == "pak") {
                PakViewerWindow* pakViewer = new PakViewerWindow();
                pakViewer->setAttribute(Qt::WA_DeleteOnClose);
                pakViewer->show();
                
                QMetaObject::invokeMethod(pakViewer, [pakViewer, filePath]() {
                    pakViewer->loadPak(filePath);
                }, Qt::QueuedConnection);
                
                return app.exec();
            }
        }
    }





    MainWindow wndw;
    wndw.show();

    return app.exec();
}
