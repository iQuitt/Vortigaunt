#include "SteamWorkshopDownloaderDialog.h"
#include "utils/FileIO.h"
#include "ui/windows/SpriteViewerWindow.h"
#include "ui/dialogs/SettingsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QDesktopServices>
#include <QUrl>

SteamWorkshopDownloaderDialog::SteamWorkshopDownloaderDialog(QWidget* parent)
    : QDialog(parent)
    , m_downloader(new SteamWorkshopDownloader(this))
{
    setupUi();

    connect(m_downloader, &SteamWorkshopDownloader::detailsFetched, this, &SteamWorkshopDownloaderDialog::onDetailsFetched);
    connect(m_downloader, &SteamWorkshopDownloader::previewDownloaded, this, &SteamWorkshopDownloaderDialog::onPreviewDownloaded);
    connect(m_downloader, &SteamWorkshopDownloader::downloadProgress, this, &SteamWorkshopDownloaderDialog::onDownloadProgress);
    connect(m_downloader, &SteamWorkshopDownloader::downloadStatusMessage, this, &SteamWorkshopDownloaderDialog::onDownloadStatusMessage);
    connect(m_downloader, &SteamWorkshopDownloader::downloadFinished, this, &SteamWorkshopDownloaderDialog::onDownloadFinished);
}

void SteamWorkshopDownloaderDialog::setupUi()
{
    setWindowTitle(tr("Steam Workshop Downloader"));
    resize(540, 220);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);

    auto* inputGroup = new QGroupBox(tr("Steam Workshop"), this);
    auto* inputLayout = new QHBoxLayout(inputGroup);

    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText(tr("Enter Steam Workshop URL or ID"));

    inputLayout->addWidget(m_urlEdit, 1);
    mainLayout->addWidget(inputGroup);

    connect(m_urlEdit, &QLineEdit::returnPressed, this, &SteamWorkshopDownloaderDialog::onDownloadClicked);

    auto* outputGroup = new QGroupBox(tr("Path"), this);
    auto* outputLayout = new QHBoxLayout(outputGroup);

    m_outputEdit = new QLineEdit(this);
    m_outputEdit->setText(QDir(SettingsDialog::getOutputRootDir()).filePath(QStringLiteral("SteamWorkshop")));
    m_browseOutputButton = new QPushButton(tr("Browse..."), this);

    outputLayout->addWidget(m_outputEdit, 1);
    outputLayout->addWidget(m_browseOutputButton);
    mainLayout->addWidget(outputGroup);

    connect(m_browseOutputButton, &QPushButton::clicked, this, &SteamWorkshopDownloaderDialog::onBrowseOutputClicked);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);

    m_statusLabel = new QLabel(tr("Ready."), this);
    m_statusLabel->setWordWrap(true);

    auto* actionLayout = new QHBoxLayout();
    m_downloadButton = new QPushButton(tr("Download Item"), this);

    m_openViewerButton = new QPushButton(tr("Open in Viewer"), this);
    m_openViewerButton->setEnabled(false);

    auto* closeButton = new QPushButton(tr("Close"), this);

    actionLayout->addWidget(m_downloadButton);
    actionLayout->addWidget(m_openViewerButton);
    actionLayout->addStretch();
    actionLayout->addWidget(closeButton);

    mainLayout->addWidget(m_progressBar);
    mainLayout->addWidget(m_statusLabel);
    mainLayout->addLayout(actionLayout);

    connect(m_downloadButton, &QPushButton::clicked, this, &SteamWorkshopDownloaderDialog::onDownloadClicked);
    connect(m_openViewerButton, &QPushButton::clicked, this, &SteamWorkshopDownloaderDialog::onOpenInViewerClicked);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::reject);
}

void SteamWorkshopDownloaderDialog::resetCard()
{
    m_downloadButton->setEnabled(true);
    m_openViewerButton->setEnabled(false);
    m_progressBar->setVisible(false);
    m_currentDetails = {};
}

void SteamWorkshopDownloaderDialog::onFetchClicked()
{
    onDownloadClicked();
}

void SteamWorkshopDownloaderDialog::onDetailsFetched(const WorkshopItemDetails& details)
{
    m_currentDetails = details;

    if (!details.success) {
        m_downloadButton->setEnabled(true);
        m_statusLabel->setText(tr("Error: %1").arg(details.errorMessage));
        QMessageBox::warning(this, tr("Fetch Failed"), details.errorMessage);
        return;
    }

    // Auto-proceed to download once details (AppID) are resolved
    QString outDir = m_outputEdit->text().trimmed();
    if (outDir.isEmpty()) {
        m_downloadButton->setEnabled(true);
        QMessageBox::warning(this, tr("Error"), tr("Please specify a valid destination folder."));
        return;
    }

    QDir().mkpath(outDir);

    QString fname = QFileInfo(details.fileName).fileName();
    if (fname.isEmpty()) fname = QString("%1.bin").arg(details.publishedFileId);
    QString targetPath = QDir(outDir).filePath(fname);

    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    m_statusLabel->setText(tr("Downloading %1 (AppID: %2)...").arg(fname).arg(details.appId));

    m_downloader->downloadFile(details.fileUrl, targetPath, details.appId, details.publishedFileId);
}

void SteamWorkshopDownloaderDialog::onPreviewDownloaded(const QImage& image)
{
    Q_UNUSED(image);
}

void SteamWorkshopDownloaderDialog::onBrowseOutputClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Select Output Directory"), m_outputEdit->text());
    if (!dir.isEmpty()) {
        m_outputEdit->setText(dir);
    }
}

void SteamWorkshopDownloaderDialog::onDownloadClicked()
{
    QString input = m_urlEdit->text().trimmed();
    uint64_t id = SteamWorkshopDownloader::extractWorkshopId(input);

    if (id == 0) {
        QMessageBox::warning(this, tr("Invalid Input"), tr("Please enter a valid Steam Workshop URL or numeric Item ID."));
        return;
    }

    QString outDir = m_outputEdit->text().trimmed();
    if (outDir.isEmpty()) {
        QMessageBox::warning(this, tr("Error"), tr("Please specify a valid destination folder."));
        return;
    }

    m_downloadButton->setEnabled(false);

    if (m_currentDetails.publishedFileId == id && m_currentDetails.appId != 0) {
        // Details already resolved, start download directly
        QDir().mkpath(outDir);
        QString fname = QFileInfo(m_currentDetails.fileName).fileName();
        if (fname.isEmpty()) fname = QString("%1.bin").arg(id);
        QString targetPath = QDir(outDir).filePath(fname);

        m_progressBar->setValue(0);
        m_progressBar->setVisible(true);
        m_statusLabel->setText(tr("Downloading %1...").arg(fname));

        m_downloader->downloadFile(m_currentDetails.fileUrl, targetPath, m_currentDetails.appId, m_currentDetails.publishedFileId);
    } else {
        resetCard();
        m_statusLabel->setText(tr("Resolving Steam Item ID %1...").arg(id));
        m_downloader->fetchItemDetails(id);
    }
}

void SteamWorkshopDownloaderDialog::onDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        int percent = static_cast<int>((bytesReceived * 100) / bytesTotal);
        m_progressBar->setValue(percent);
        double rMB = bytesReceived / (1024.0 * 1024.0);
        double tMB = bytesTotal / (1024.0 * 1024.0);
        m_statusLabel->setText(tr("Downloading: %1 MB / %2 MB (%3%)").arg(rMB, 0, 'f', 1).arg(tMB, 0, 'f', 1).arg(percent));
    }
}

void SteamWorkshopDownloaderDialog::onDownloadStatusMessage(const QString& statusMsg)
{
    m_statusLabel->setText(statusMsg);
}

void SteamWorkshopDownloaderDialog::onDownloadFinished(bool success, const QString& filePath, const QString& errorMsg)
{
    m_downloadButton->setEnabled(true);
    m_progressBar->setVisible(false);

    if (success) {
        m_lastDownloadedFile = filePath;
        m_statusLabel->setText(tr("Download completed: %1").arg(QFileInfo(filePath).fileName()));

        // Just view it 
        QString ext = QFileInfo(filePath).suffix().toLower();
        if (ext == QStringLiteral("spr") || ext == QStringLiteral("vtf") || ext == QStringLiteral("mdl") || ext == QStringLiteral("wad") || ext == QStringLiteral("bin") || ext == QStringLiteral("gma")) {
            m_openViewerButton->setEnabled(true);
        }

        QMessageBox::information(this, tr("Download Complete"), tr("Successfully downloaded workshop item to:\n%1").arg(filePath));
    } else {
        m_statusLabel->setText(tr("Download failed: %1").arg(errorMsg));
        QMessageBox::critical(this, tr("Download Failed"), tr("Failed to download workshop item.\nError: %1").arg(errorMsg));
    }
}

void SteamWorkshopDownloaderDialog::onOpenInViewerClicked()
{
    if (m_lastDownloadedFile.isEmpty() || !QFile::exists(m_lastDownloadedFile)) return;

    QString ext = QFileInfo(m_lastDownloadedFile).suffix().toLower();
    if (ext == QStringLiteral("spr") || ext == QStringLiteral("vtf") || ext == QStringLiteral("bin") || ext == QStringLiteral("gma")) {
        auto* viewer = new SpriteViewerWindow();
        viewer->setAttribute(Qt::WA_DeleteOnClose);
        viewer->show();
    } else {
        QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(m_lastDownloadedFile).absolutePath()));
    }
}

QString SteamWorkshopDownloaderDialog::getAppIdName(uint32_t appId) const
{
    switch (appId) {
		//  i'll add more source engine games 
        case 4000: return QStringLiteral("Garry's Mod");
        case 730: return QStringLiteral("Counter-Strike 2 / CS:GO");
        case 440: return QStringLiteral("Team Fortress 2");
        case 550: return QStringLiteral("Left 4 Dead 2");
        default: return QStringLiteral("Steam App");
    }
}
