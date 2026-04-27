#pragma once

#include <QObject>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include "VortigauntVersion.h"

/**
 * VortigauntUpdateCheck - GitHub Releases based auto-update checker
 * 
 * Checks for new releases on GitHub and notifies the user if an update is available.

 */
class VortigauntUpdateCheck : public QObject
{
    Q_OBJECT

public:
    explicit VortigauntUpdateCheck(QObject* parent = nullptr);
    ~VortigauntUpdateCheck() = default;

    // Start checking for updates
    void checkForUpdates(bool silent = false);

    // Get current application version (from CMake-generated VortigauntVersion.h)
    static QString currentVersion() { return QString(VORTIGAUNT_VERSION_STRING); }
    
    // GitHub repository configuration (from CMake-generated VortigauntVersion.h)
    static constexpr const char* GITHUB_OWNER = VORTIGAUNT_GITHUB_OWNER;
    static constexpr const char* GITHUB_REPO = VORTIGAUNT_GITHUB_REPO;

signals:
    // Emitted when a new update is available
    void updateAvailable(const QString& latestVersion, const QString& releaseUrl, const QString& releaseNotes);
    
    // Emitted when no update is available (current version is latest)
    void noUpdateAvailable();
    
    // Emitted when update check fails
    void updateCheckFailed(const QString& errorMessage);

private slots:
    void onNetworkReply(QNetworkReply* reply);

private:
    // Compare two version strings (e.g., "1.2.3" vs "1.3.0")
    // Returns: -1 if v1 < v2, 0 if v1 == v2, 1 if v1 > v2
    static int compareVersions(const QString& v1, const QString& v2);

    QNetworkAccessManager* m_networkManager;
    bool m_silentCheck = false;
};
