#include "SteamWorkshopDownloader.h"

#include <QRegularExpression>
#include <QUrlQuery>
#include <QUrl>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QThread>
#include <QProcess>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QStandardPaths>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QSslError>
#include <QEventLoop>
#include <QTimer>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

static const char* kUserAgent = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.0.0 Safari/537.36";

static QByteArray httpReq(const QString& urlStr, const QByteArray& postData = QByteArray(), const QString& contentType = QString())
{
    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(urlStr)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kUserAgent));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    if (!contentType.isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, contentType);
    } else if (!postData.isEmpty()) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/x-www-form-urlencoded"));
    } else {
        request.setRawHeader("Cookie", "birthtime=0; wants_mature_content=1;");
    }

    QNetworkReply* reply = postData.isEmpty() ? manager.get(request) : manager.post(request, postData);
    QObject::connect(reply, &QNetworkReply::sslErrors, reply, qOverload<const QList<QSslError>&>(&QNetworkReply::ignoreSslErrors));

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
    loop.exec();

    QByteArray responseData;
    if (reply->error() == QNetworkReply::NoError) {
        responseData = reply->readAll();
    }
    reply->deleteLater();
    return responseData;
}

static bool downloadFileHttp(const QString& urlStr, const QString& destinationPath, std::function<void(qint64, qint64)> progressCb, std::atomic<bool>* cancelFlag)
{
    QFile outFile(destinationPath);
    if (!outFile.open(QIODevice::WriteOnly)) {
        return false;
    }

    QNetworkAccessManager manager;
    QNetworkRequest request{QUrl(urlStr)};
    request.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kUserAgent));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = manager.get(request);
    QObject::connect(reply, &QNetworkReply::sslErrors, reply, qOverload<const QList<QSslError>&>(&QNetworkReply::ignoreSslErrors));

    qint64 bytesDownloaded = 0;
    QObject::connect(reply, &QNetworkReply::readyRead, [&]() {
        QByteArray chunk = reply->readAll();
        bytesDownloaded += chunk.size();
        outFile.write(chunk);
    });
    if (progressCb) {
        QObject::connect(reply, &QNetworkReply::downloadProgress, [progressCb](qint64 received, qint64 total) {
            progressCb(received, total);
        });
    }

    QEventLoop loop;
    QObject::connect(reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);

    QTimer cancelTimer;
    if (cancelFlag) {
        QObject::connect(&cancelTimer, &QTimer::timeout, [&]() {
            if (cancelFlag->load()) {
                reply->abort();
            }
        });
        cancelTimer.start(200);
    }

    loop.exec();
    outFile.close();

    bool success = (reply->error() == QNetworkReply::NoError) && bytesDownloaded > 0;
    reply->deleteLater();
    return success;
}

SteamWorkshopDownloader::SteamWorkshopDownloader(QObject* parent)
    : QObject(parent)
{
}

uint64_t SteamWorkshopDownloader::extractWorkshopId(const QString& inputStr)
{
    QString trimmed = inputStr.trimmed();
    if (trimmed.isEmpty()) return 0;

    bool ok = false;
    uint64_t rawId = trimmed.toULongLong(&ok);
    if (ok && rawId > 0) {
        return rawId;
    }
    QRegularExpression rx(QStringLiteral("(?:id=|filedetails/\\?id=)(\\d+)"));
    QRegularExpressionMatch match = rx.match(trimmed);
    if (match.hasMatch()) {
        return match.captured(1).toULongLong();
    }

    return 0;
}

void SteamWorkshopDownloader::fetchItemDetails(uint64_t publishedFileId)
{
    if (publishedFileId == 0) {
        WorkshopItemDetails details;
        details.success = false;
        details.errorMessage = QStringLiteral("Invalid Workshop Item ID.");
        emit detailsFetched(details);
        return;
    }

    QThread* worker = QThread::create([this, publishedFileId]() {
        QByteArray body = "itemcount=1&publishedfileids[0]=" + QByteArray::number(publishedFileId);
        QByteArray respData = httpReq(QStringLiteral("https://api.steampowered.com/ISteamRemoteStorage/GetPublishedFileDetails/v1/"), body);

        WorkshopItemDetails details;
        if (respData.isEmpty()) {
            details.success = false;
            details.errorMessage = QStringLiteral("Connection failed or remote host closed connection.");
            QMetaObject::invokeMethod(this, [this, details]() {
                emit detailsFetched(details);
            });
            return;
        }

        QJsonDocument doc = QJsonDocument::fromJson(respData);
        if (doc.isNull() || !doc.isObject()) {
            details.success = false;
            details.errorMessage = QStringLiteral("Invalid JSON response from Steam API.");
            QMetaObject::invokeMethod(this, [this, details]() {
                emit detailsFetched(details);
            });
            return;
        }

        QJsonObject rootObj = doc.object();
        QJsonObject responseObj = rootObj[QStringLiteral("response")].toObject();
        QJsonArray detailsArr = responseObj[QStringLiteral("publishedfiledetails")].toArray();

        if (detailsArr.isEmpty()) {
            details.success = false;
            details.errorMessage = QStringLiteral("Item details not found in Steam response.");
            QMetaObject::invokeMethod(this, [this, details]() {
                emit detailsFetched(details);
            });
            return;
        }

        QJsonObject itemObj = detailsArr[0].toObject();
        int result = itemObj[QStringLiteral("result")].toInt();

        if (result != 1) {
            details.success = false;
            details.errorMessage = QString(QStringLiteral("Steam API returned result code %1 (Item may be private or removed).")).arg(result);
            QMetaObject::invokeMethod(this, [this, details]() {
                emit detailsFetched(details);
            });
            return;
        }

        details.success = true;
        details.publishedFileId = itemObj[QStringLiteral("publishedfileid")].toString().toULongLong();
        details.appId = itemObj[QStringLiteral("creator_app_id")].toInt();
        if (details.appId == 0) {
            details.appId = itemObj[QStringLiteral("consumer_app_id")].toInt();
        }
        details.title = itemObj[QStringLiteral("title")].toString();
        details.description = itemObj[QStringLiteral("description")].toString();
        details.fileUrl = itemObj[QStringLiteral("file_url")].toString();
        details.previewUrl = itemObj[QStringLiteral("preview_url")].toString();
        details.fileName = itemObj[QStringLiteral("filename")].toString();
        details.fileSize = itemObj[QStringLiteral("file_size")].toVariant().toLongLong();
        details.timeCreated = itemObj[QStringLiteral("time_created")].toInt();
        details.timeUpdated = itemObj[QStringLiteral("time_updated")].toInt();

        QMetaObject::invokeMethod(this, [this, details]() {
            emit detailsFetched(details);
        });
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void SteamWorkshopDownloader::downloadPreviewImage(const QString& previewUrl)
{
    if (previewUrl.isEmpty()) return;

    QThread* worker = QThread::create([this, previewUrl]() {
        QByteArray data = httpReq(previewUrl);
        if (!data.isEmpty()) {
            QImage image;
            image.loadFromData(data);
            if (!image.isNull()) {
                QMetaObject::invokeMethod(this, [this, image]() {
                    emit previewDownloaded(image);
                });
            }
        }
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void SteamWorkshopDownloader::downloadFile(const QString& fileUrl, const QString& destinationPath, uint32_t appId, uint64_t publishedFileId)
{
    Q_UNUSED(fileUrl);

    if (publishedFileId == 0) {
        emit downloadFinished(false, destinationPath, QStringLiteral("Invalid Published File ID."));
        return;
    }

    QThread* worker = QThread::create([this, appId, publishedFileId, destinationPath]() {
        QString appDir = QCoreApplication::applicationDirPath();
        QString steamCmdFolder = QDir(appDir).filePath(QStringLiteral("steamcmd"));
        QString steamCmdExe = QDir(steamCmdFolder).filePath(QStringLiteral("steamcmd.exe"));

        // If SteamCMD is missing in application directory, auto-download official steamcmd.zip
        if (!QFile::exists(steamCmdExe)) {
            QMetaObject::invokeMethod(this, [this]() {
                emit downloadStatusMessage(QStringLiteral("SteamCMD not found in app directory. Auto-downloading SteamCMD..."));
            });

            QDir().mkpath(steamCmdFolder);
            QString zipPath = QDir(steamCmdFolder).filePath(QStringLiteral("steamcmd.zip"));

#ifdef _WIN32
            bool downloaded = downloadFileHttp(
                QStringLiteral("https://steamcdn-a.akamaihd.net/client/installer/steamcmd.zip"), zipPath, nullptr, nullptr
            );

            if (downloaded && QFile::exists(zipPath)) {
                QMetaObject::invokeMethod(this, [this]() {
                    emit downloadStatusMessage(QStringLiteral("Extracting SteamCMD package..."));
                });

                QProcess unzipProc;
                QString cmd = QString(QStringLiteral("Expand-Archive -Path '%1' -DestinationPath '%2' -Force"))
                    .arg(zipPath)
                    .arg(steamCmdFolder);

                unzipProc.start(QStringLiteral("powershell"), QStringList() << QStringLiteral("-NoProfile") << QStringLiteral("-Command") << cmd);
                unzipProc.waitForFinished(30000);

                QFile::remove(zipPath);
            }
#endif
        }

        if (!QFile::exists(steamCmdExe)) {
            QMetaObject::invokeMethod(this, [this, destinationPath]() {
                emit downloadFinished(false, destinationPath, QStringLiteral("SteamCMD could not be found."));
            });
            return;
        }

        QMetaObject::invokeMethod(this, [this]() {
            emit downloadStatusMessage(QStringLiteral("Downloading item via SteamCMD..."));
        });

        QProcess proc;
        proc.setWorkingDirectory(QFileInfo(steamCmdExe).absolutePath());
#ifdef _WIN32
        proc.setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *args) {
            args->flags |= CREATE_NO_WINDOW;
        });
#endif

        QStringList args;
        args << QStringLiteral("+force_install_dir") << QStringLiteral("./content")
             << QStringLiteral("+login") << QStringLiteral("anonymous")
             << QStringLiteral("+workshop_download_item") << QString::number(appId) << QString::number(publishedFileId)
             << QStringLiteral("+quit");

        proc.start(steamCmdExe, args);

        QRegularExpression progRx(QStringLiteral("(\\d+)%"));
        QString lastOutput;

        while (proc.state() != QProcess::NotRunning) {
            proc.waitForReadyRead(300);
            QByteArray outData = proc.readAllStandardOutput() + proc.readAllStandardError();
            if (!outData.isEmpty()) {
                QString outStr = QString::fromUtf8(outData);
                lastOutput += outStr;

                QRegularExpressionMatch m = progRx.match(outStr);
                if (m.hasMatch()) {
                    int pct = m.captured(1).toInt();
                    QMetaObject::invokeMethod(this, [this, pct]() {
                        emit downloadProgress(pct, 100);
                    });
                }
            }
        }
        proc.waitForFinished(180000);

        QString searchBase = QDir(QFileInfo(steamCmdExe).absolutePath()).filePath(QStringLiteral("steamapps/workshop/content"));
        if (!QDir(searchBase).exists()) {
            searchBase = QDir(QFileInfo(steamCmdExe).absolutePath()).filePath(QStringLiteral("content"));
        }

        QStringList candidateFiles;
        QDirIterator it(searchBase, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            QString fp = it.next();
            if (fp.contains(QString::number(publishedFileId)) || fp.endsWith(QStringLiteral(".gma")) || fp.endsWith(QStringLiteral(".bin")) || fp.endsWith(QStringLiteral(".vpk"))) {
                candidateFiles.append(fp);
            }
        }

        if (!candidateFiles.isEmpty()) {
            bool copied = false;
            QDir().mkpath(destinationPath);
            for (const QString& srcPath : candidateFiles) {
                QFileInfo fi(srcPath);
                QString destFilePath = QDir(destinationPath).filePath(fi.fileName());
                QFile::remove(destFilePath);
                if (QFile::copy(srcPath, destFilePath)) {
                    copied = true;
                }
            }

            if (copied) {
                QMetaObject::invokeMethod(this, [this, destinationPath]() {
                    emit downloadFinished(true, destinationPath, QString());
                });
                return;
            }
        }

        QMetaObject::invokeMethod(this, [this, destinationPath]() {
            emit downloadFinished(false, destinationPath, QStringLiteral("SteamCMD completed, but no downloaded Workshop files were found."));
        });
    });

    connect(worker, &QThread::finished, worker, &QObject::deleteLater);
    worker->start();
}

void SteamWorkshopDownloader::cancelDownload()
{
}
