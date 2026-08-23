#pragma once

#include <QObject>
#include <QString>
#include <QImage>
#include <cstdint>
#include <atomic>

struct WorkshopItemDetails {
    bool success = false;
    uint64_t publishedFileId = 0;
    uint32_t appId = 0;
    QString title;
    QString description;
    QString fileUrl;
    QString previewUrl;
    QString fileName;
    qint64 fileSize = 0;
    int timeCreated = 0;
    int timeUpdated = 0;
    QString errorMessage;
};

class SteamWorkshopDownloader : public QObject {
    Q_OBJECT

public:
    explicit SteamWorkshopDownloader(QObject* parent = nullptr);
    ~SteamWorkshopDownloader() override = default;

    static uint64_t extractWorkshopId(const QString& inputStr);

    void fetchItemDetails(uint64_t publishedFileId);

    void downloadPreviewImage(const QString& previewUrl);

    void downloadFile(const QString& fileUrl, const QString& destinationPath, uint32_t appId = 0, uint64_t publishedFileId = 0);

    void cancelDownload();

signals:
    void detailsFetched(const WorkshopItemDetails& details);
    void previewDownloaded(const QImage& image);
    void downloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void downloadStatusMessage(const QString& statusMsg);
    void downloadFinished(bool success, const QString& filePath, const QString& errorMsg);

private:
    std::atomic<bool> m_cancelRequested{false};
};
