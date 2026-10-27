// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "SteamWorkshopDownloaderDialog.h"
#include "ui/UiUtils.h"
#include "ui/dialogs/SettingsDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QDir>
#include <QDateTime>
#include <QDesktopServices>
#include <QRegularExpression>
#include <QScrollArea>
#include <QUrl>

namespace {

constexpr int kPreviewSize = 200;

// Workshop descriptions are BBCode
QString plainDescription(const QString& bbcode)
{
    QString text = bbcode;
    text.remove(QRegularExpression(QStringLiteral("\\[/?[a-zA-Z0-9*]+(=[^\\]]*)?\\]")));
    return text.trimmed();
}

} // namespace

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
    resize(720, 420);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(8);

    // the item, and what to do with it
    auto* inputLayout = new QHBoxLayout();
    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText(tr("Enter Steam Workshop URL or ID"));
    m_urlEdit->setClearButtonEnabled(true);
    m_fetchButton = new QPushButton(tr("Show Item"), this);
    m_fetchButton->setToolTip(tr("Look the item up without downloading it"));
    m_downloadButton = new QPushButton(tr("Download Item"), this);
    inputLayout->addWidget(m_urlEdit, 1);
    inputLayout->addWidget(m_fetchButton);
    inputLayout->addWidget(m_downloadButton);
    mainLayout->addLayout(inputLayout);

    auto* outputLayout = new QHBoxLayout();
    outputLayout->addWidget(new QLabel(tr("Folder:"), this));
    m_outputEdit = new QLineEdit(this);
    m_outputEdit->setText(QDir(SettingsDialog::getOutputRootDir()).filePath(QStringLiteral("SteamWorkshop")));
    m_browseOutputButton = new QPushButton(tr("Browse..."), this);
    outputLayout->addWidget(m_outputEdit, 1);
    outputLayout->addWidget(m_browseOutputButton);
    mainLayout->addLayout(outputLayout);

    // the item's page: preview on the left, title, game, size and description on the right
    m_card = new QWidget(this);
    auto* cardLayout = new QHBoxLayout(m_card);
    cardLayout->setContentsMargins(0, 8, 0, 8);
    m_previewLabel = new QLabel(m_card);
    m_previewLabel->setFixedSize(kPreviewSize, kPreviewSize);
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setBackgroundRole(QPalette::Base);
    m_previewLabel->setAutoFillBackground(true);
    cardLayout->addWidget(m_previewLabel, 0, Qt::AlignTop);
    auto* textLayout = new QVBoxLayout();
    m_titleLabel = new QLabel(m_card);
    m_titleLabel->setWordWrap(true);
    m_titleLabel->setStyleSheet("font-weight: bold; font-size: 15px;");
    m_titleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_infoLabel = new QLabel(m_card);
    m_infoLabel->setWordWrap(true);
    m_infoLabel->setForegroundRole(QPalette::PlaceholderText);
    m_descriptionLabel = new QLabel(m_card);
    m_descriptionLabel->setWordWrap(true);
    m_descriptionLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_descriptionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    // a long description scrolls instead of pushing the buttons away
    auto* descriptionScroll = new QScrollArea(m_card);
    descriptionScroll->setWidgetResizable(true);
    descriptionScroll->setFrameShape(QFrame::NoFrame);
    descriptionScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    descriptionScroll->setWidget(m_descriptionLabel);
    textLayout->addWidget(m_titleLabel);
    textLayout->addWidget(m_infoLabel);
    textLayout->addWidget(descriptionScroll, 1);
    cardLayout->addLayout(textLayout, 1);
    mainLayout->addWidget(m_card, 1);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    m_progressBar->setVisible(false);
    m_statusLabel = new QLabel(tr("Ready."), this);
    m_statusLabel->setWordWrap(true);
    mainLayout->addWidget(m_progressBar);

    auto* actionLayout = new QHBoxLayout();
    actionLayout->addWidget(m_statusLabel, 1);
    m_openButton = new QPushButton(tr("Open"), this);
    m_openButton->setToolTip(tr("Open the downloaded file in the tab that fits it"));
    m_showInFolderButton = new QPushButton(tr("Show in Folder"), this);
    actionLayout->addWidget(m_openButton);
    actionLayout->addWidget(m_showInFolderButton);
    mainLayout->addLayout(actionLayout);

    connect(m_urlEdit, &QLineEdit::returnPressed, this, &SteamWorkshopDownloaderDialog::onDownloadClicked);
    connect(m_fetchButton, &QPushButton::clicked, this, &SteamWorkshopDownloaderDialog::onFetchClicked);
    connect(m_downloadButton, &QPushButton::clicked, this, &SteamWorkshopDownloaderDialog::onDownloadClicked);
    connect(m_browseOutputButton, &QPushButton::clicked, this, &SteamWorkshopDownloaderDialog::onBrowseOutputClicked);
    connect(m_openButton, &QPushButton::clicked, this, &SteamWorkshopDownloaderDialog::onOpenClicked);
    connect(m_showInFolderButton, &QPushButton::clicked, this, &SteamWorkshopDownloaderDialog::onShowInFolderClicked);

    resetCard();
}

void SteamWorkshopDownloaderDialog::resetCard()
{
    m_fetchButton->setEnabled(true);
    m_downloadButton->setEnabled(true);
    m_openButton->setEnabled(false);
    m_showInFolderButton->setEnabled(false);
    m_progressBar->setVisible(false);
    m_currentDetails = {};
    m_previewLabel->setPixmap(QPixmap());
    m_previewLabel->setText(QString());
    m_titleLabel->setText(tr("No item yet"));
    m_infoLabel->setText(tr("Paste a Workshop link or item ID above, then show or download it."));
    m_descriptionLabel->clear();
}

void SteamWorkshopDownloaderDialog::showCard(const WorkshopItemDetails& details)
{
    m_titleLabel->setText(details.title.isEmpty() ? QString::number(details.publishedFileId) : details.title);
    QStringList info;
    info << QStringLiteral("%1 (%2)").arg(getAppIdName(details.appId)).arg(details.appId);
    if (!details.fileName.isEmpty())
        info << QFileInfo(details.fileName).fileName();
    if (details.fileSize > 0)
        info << UiUtils::formatSize(details.fileSize);
    if (details.timeUpdated > 0)
        info << tr("updated %1").arg(QDateTime::fromSecsSinceEpoch(details.timeUpdated).date().toString(Qt::ISODate));
    m_infoLabel->setText(info.join(QStringLiteral("  ·  ")));
    m_descriptionLabel->setText(plainDescription(details.description));
    m_previewLabel->setText(details.previewUrl.isEmpty() ? tr("No preview") : tr("Loading preview..."));
    m_downloader->downloadPreviewImage(details.previewUrl);
}

QString SteamWorkshopDownloaderDialog::targetPath() const
{
    const QDir outDir(m_outputEdit->text().trimmed());
    // without a file URL SteamCMD fetches the item, into a folder of its own
    if (m_currentDetails.fileUrl.isEmpty())
        return outDir.filePath(QString::number(m_currentDetails.publishedFileId));
    QString fname = QFileInfo(m_currentDetails.fileName).fileName();
    if (fname.isEmpty())
        fname = QString("%1.bin").arg(m_currentDetails.publishedFileId);
    if (m_currentDetails.appId == 4000 && QFileInfo(fname).suffix().compare(QStringLiteral("bin"), Qt::CaseInsensitive) == 0)
        fname = QFileInfo(fname).completeBaseName() + QStringLiteral(".gma");
    return outDir.filePath(fname);
}

void SteamWorkshopDownloaderDialog::startDownload()
{
    const QString outDir = m_outputEdit->text().trimmed();
    if (outDir.isEmpty()) {
        m_fetchButton->setEnabled(true);
        m_downloadButton->setEnabled(true);
        QMessageBox::warning(this, tr("Error"), tr("Please specify a valid destination folder."));
        return;
    }
    QDir().mkpath(outDir);
    const QString target = targetPath();
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    m_statusLabel->setText(tr("Downloading %1 (AppID: %2)...").arg(QFileInfo(target).fileName()).arg(m_currentDetails.appId));
    m_downloader->downloadFile(m_currentDetails.fileUrl, target, m_currentDetails.appId, m_currentDetails.publishedFileId);
}

void SteamWorkshopDownloaderDialog::onFetchClicked()
{
    const uint64_t id = SteamWorkshopDownloader::extractWorkshopId(m_urlEdit->text().trimmed());
    if (id == 0) {
        QMessageBox::warning(this, tr("Invalid Input"), tr("Please enter a valid Steam Workshop URL or numeric Item ID."));
        return;
    }
    resetCard();
    m_fetchButton->setEnabled(false);
    m_downloadButton->setEnabled(false);
    m_downloadAfterFetch = false;
    m_statusLabel->setText(tr("Resolving Steam Item ID %1...").arg(id));
    m_downloader->fetchItemDetails(id);
}

void SteamWorkshopDownloaderDialog::onDetailsFetched(const WorkshopItemDetails& details)
{
    m_currentDetails = details;
    m_fetchButton->setEnabled(true);

    if (!details.success) {
        m_downloadButton->setEnabled(true);
        m_statusLabel->setText(tr("Error: %1").arg(details.errorMessage));
        QMessageBox::warning(this, tr("Fetch Failed"), details.errorMessage);
        return;
    }

    showCard(details);
    if (m_downloadAfterFetch)
        startDownload();
    else {
        m_downloadButton->setEnabled(true);
        m_statusLabel->setText(tr("Ready to download."));
    }
}

void SteamWorkshopDownloaderDialog::onPreviewDownloaded(const QImage& image)
{
    if (image.isNull()) {
        m_previewLabel->setText(tr("No preview"));
        return;
    }
    m_previewLabel->setPixmap(QPixmap::fromImage(image.scaled(kPreviewSize, kPreviewSize, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
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
    const uint64_t id = SteamWorkshopDownloader::extractWorkshopId(m_urlEdit->text().trimmed());
    if (id == 0) {
        QMessageBox::warning(this, tr("Invalid Input"), tr("Please enter a valid Steam Workshop URL or numeric Item ID."));
        return;
    }

    m_fetchButton->setEnabled(false);
    m_downloadButton->setEnabled(false);
    m_openButton->setEnabled(false);
    m_showInFolderButton->setEnabled(false);

    // an item already shown downloads at once, another one is looked up first
    if (m_currentDetails.publishedFileId == id && m_currentDetails.appId != 0) {
        startDownload();
    } else {
        resetCard();
        m_fetchButton->setEnabled(false);
        m_downloadButton->setEnabled(false);
        m_downloadAfterFetch = true;
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
    m_fetchButton->setEnabled(true);
    m_downloadButton->setEnabled(true);
    m_progressBar->setVisible(false);

    if (success) {
        // old Garry's Mod items are LZMA-compressed addons named .bin; as .gma the Archives tab opens them
        if (m_currentDetails.appId == 4000 && QFileInfo(filePath).isDir()) {
            QDir folder(filePath);
            for (const QString& name : folder.entryList({ QStringLiteral("*.bin") }, QDir::Files))
                QFile::rename(folder.filePath(name), folder.filePath(QFileInfo(name).completeBaseName() + QStringLiteral(".gma")));
        }
        m_lastDownloadedFile = filePath;
        m_statusLabel->setText(tr("Download completed: %1").arg(QDir::toNativeSeparators(filePath)));
        m_openButton->setEnabled(true);
        m_showInFolderButton->setEnabled(true);
    } else {
        m_statusLabel->setText(tr("Download failed: %1").arg(errorMsg));
        QMessageBox::critical(this, tr("Download Failed"), tr("Failed to download workshop item.\nError: %1").arg(errorMsg));
    }
}

void SteamWorkshopDownloaderDialog::onOpenClicked()
{
    const QFileInfo downloaded(m_lastDownloadedFile);
    if (downloaded.isFile()) {
        emit openFiles({ downloaded.absoluteFilePath() });
    } else if (downloaded.isDir()) {
        QStringList files;
        const QDir folder(downloaded.absoluteFilePath());
        for (const QString& name : folder.entryList(QDir::Files))
            files << folder.filePath(name);
        emit openFiles(files);
    }
}

void SteamWorkshopDownloaderDialog::onShowInFolderClicked()
{
    const QFileInfo downloaded(m_lastDownloadedFile);
    if (downloaded.exists())
        QDesktopServices::openUrl(QUrl::fromLocalFile(downloaded.isDir() ? downloaded.absoluteFilePath() : downloaded.absolutePath()));
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
