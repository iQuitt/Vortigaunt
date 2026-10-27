// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <QDialog>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QProgressBar>

#include "core/SteamWorkshopDownloader.h"

// a Workshop item: its page shown first, then downloaded and opened in the tab that fits it
class SteamWorkshopDownloaderDialog : public QDialog {
    Q_OBJECT

public:
    explicit SteamWorkshopDownloaderDialog(QWidget* parent = nullptr);
    ~SteamWorkshopDownloaderDialog() override = default;

signals:
    // the downloaded files, for the main window to open where they belong
    void openFiles(const QStringList& paths);

private slots:
    void onFetchClicked();
    void onBrowseOutputClicked();
    void onDownloadClicked();
    void onOpenClicked();
    void onShowInFolderClicked();

    void onDetailsFetched(const WorkshopItemDetails& details);
    void onPreviewDownloaded(const QImage& image);
    void onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDownloadStatusMessage(const QString& statusMsg);
    void onDownloadFinished(bool success, const QString& filePath, const QString& errorMsg);

private:
    void setupUi();
    void resetCard();
    void showCard(const WorkshopItemDetails& details);
    void startDownload();
    QString targetPath() const;
    QString getAppIdName(uint32_t appId) const;

    QLineEdit* m_urlEdit = nullptr;
    QPushButton* m_fetchButton = nullptr;
    QPushButton* m_downloadButton = nullptr;
    QLineEdit* m_outputEdit = nullptr;
    QPushButton* m_browseOutputButton = nullptr;

    QWidget* m_card = nullptr;
    QLabel* m_previewLabel = nullptr;
    QLabel* m_titleLabel = nullptr;
    QLabel* m_infoLabel = nullptr;
    QLabel* m_descriptionLabel = nullptr;

    QProgressBar* m_progressBar = nullptr;
    QLabel* m_statusLabel = nullptr;
    QPushButton* m_openButton = nullptr;
    QPushButton* m_showInFolderButton = nullptr;

    SteamWorkshopDownloader* m_downloader = nullptr;
    WorkshopItemDetails m_currentDetails;
    bool m_downloadAfterFetch = false;
    QString m_lastDownloadedFile;
};
