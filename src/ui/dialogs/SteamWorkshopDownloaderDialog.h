#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>
#include <QTextEdit>
#include <QGroupBox>

#include "core/SteamWorkshopDownloader.h"

class SteamWorkshopDownloaderDialog : public QDialog {
    Q_OBJECT

public:
    explicit SteamWorkshopDownloaderDialog(QWidget* parent = nullptr);
    ~SteamWorkshopDownloaderDialog() override = default;

private slots:
    void onFetchClicked();
    void onBrowseOutputClicked();
    void onDownloadClicked();
    void onOpenInViewerClicked();

    void onDetailsFetched(const WorkshopItemDetails& details);
    void onPreviewDownloaded(const QImage& image);
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadStatusMessage(const QString& statusMsg);
    void onDownloadFinished(bool success, const QString& filePath, const QString& errorMsg);

private:
    void setupUi();
    void resetCard();
    QString getAppIdName(uint32_t appId) const;

    QLineEdit* m_urlEdit = nullptr;
    QLineEdit* m_outputEdit = nullptr;
    QPushButton* m_browseOutputButton = nullptr;

    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_downloadButton = nullptr;
    QPushButton* m_openViewerButton = nullptr;

    SteamWorkshopDownloader* m_downloader = nullptr;
    WorkshopItemDetails m_currentDetails;
    QString m_lastDownloadedFile;
};
