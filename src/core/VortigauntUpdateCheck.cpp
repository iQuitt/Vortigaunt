#include "VortigauntUpdateCheck.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QUrl>
#include <QNetworkRequest>
#include <QStringList>
#include <QMessageBox>
#include <QDesktopServices>
#include <QWidget>

VortigauntUpdateCheck::VortigauntUpdateCheck(QObject* parent)
    : QObject(parent)
    , m_networkManager(new QNetworkAccessManager(this))
{
    connect(m_networkManager, &QNetworkAccessManager::finished,
            this, &VortigauntUpdateCheck::onNetworkReply);
}

void VortigauntUpdateCheck::checkForUpdates(bool silent)
{
    m_silentCheck = silent;
    QString urlString = QString("https://api.github.com/repos/%1/%2/releases/latest")
                      .arg(GITHUB_OWNER)
                      .arg(GITHUB_REPO);
    
    QUrl url(urlString);
    QNetworkRequest request;
    request.setUrl(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("Vortigaunt-UpdateChecker"));
    request.setRawHeader("Accept", "application/vnd.github.v3+json");
    
    m_networkManager->get(request);
}


void VortigauntUpdateCheck::onNetworkReply(QNetworkReply* reply)
{
    reply->deleteLater();
    
    QWidget* parentWidget = qobject_cast<QWidget*>(parent());
    
    if (reply->error() != QNetworkReply::NoError)
    {
        // Handle 404 specifically - might mean no releases yet
        if (reply->error() == QNetworkReply::ContentNotFoundError)
        {
            if (!m_silentCheck)
            {
                QMessageBox::information(
                    parentWidget,
                    tr("No Update Available"),
                    tr("You are running the latest version of Vortigaunt (%1).")
                        .arg(currentVersion())
                );
            }
            emit noUpdateAvailable();
            return;
        }
        
        if (!m_silentCheck)
        {
            QMessageBox::warning(
                parentWidget,
                tr("Update Check Failed"),
                tr("Failed to check for updates: %1").arg(reply->errorString())
            );
        }
        emit updateCheckFailed(reply->errorString());
        return;
    }
    
    QByteArray data = reply->readAll();
    QJsonDocument doc = QJsonDocument::fromJson(data);
    
    if (!doc.isObject())
    {
        if (!m_silentCheck)
        {
            QMessageBox::warning(
                parentWidget,
                tr("Update Check Failed"),
                tr("Invalid response from GitHub")
            );
        }
        emit updateCheckFailed(tr("Invalid response from GitHub"));
        return;
    }
    
    QJsonObject obj = doc.object();
    
    QString tagName = obj.value("tag_name").toString();
    QString htmlUrl = obj.value("html_url").toString();
    QString body = obj.value("body").toString();
    
    if (tagName.isEmpty())
    {
        if (!m_silentCheck)
        {
            QMessageBox::warning(
                parentWidget,
                tr("Update Check Failed"),
                tr("No version information in release")
            );
        }
        emit updateCheckFailed(tr("No version information in release"));
        return;
    }
    
    // Remove 'v' prefix if present
    QString latestVersion = tagName;
    if (latestVersion.startsWith('v') || latestVersion.startsWith('V'))
    {
        latestVersion = latestVersion.mid(1);
    } 
    
    QString current = currentVersion();
    
    int cmp = compareVersions(current, latestVersion);
    
    if (cmp < 0)
    {

        // TODO: Add Changelog
        QString message = tr("A new version of Vortigaunt is available!\n\n"
                             "Current version: %1\n"
                             "Latest version: %2\n\n"
                             "Would you like to visit the download page?")
                          .arg(current)
                          .arg(latestVersion);
        
        QMessageBox::StandardButton replyBtn = QMessageBox::information(
            parentWidget,
            tr("Update Available"),
            message,
            QMessageBox::Yes | QMessageBox::No
        );
        
        if (replyBtn == QMessageBox::Yes)
        {
            QDesktopServices::openUrl(QUrl(htmlUrl));
        }
        
        emit updateAvailable(latestVersion, htmlUrl, body);
    }
    else
    {
        if (!m_silentCheck)
        {
            QMessageBox::information(
                parentWidget,
                tr("No Update Available"),
                tr("You are running the latest version of Vortigaunt (%1).")
                    .arg(current)
            );
        }
        emit noUpdateAvailable();
    }
}

int VortigauntUpdateCheck::compareVersions(const QString& v1, const QString& v2)
{
    QStringList parts1 = v1.split('.');
    QStringList parts2 = v2.split('.');
    
    int maxLen = qMax(parts1.size(), parts2.size());
    
    for (int i = 0; i < maxLen; ++i)
    {
        int num1 = (i < parts1.size()) ? parts1[i].toInt() : 0;
        int num2 = (i < parts2.size()) ? parts2[i].toInt() : 0;
        
        if (num1 < num2) return -1;
        if (num1 > num2) return 1;
    }
    
    return 0; 
}
