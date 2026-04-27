#include "LanguageManager.h"

#include <QCoreApplication>
#include <QDir>
#include <QSettings>
#include <QDebug>
#include <QLibraryInfo>

LanguageManager& LanguageManager::instance()
{
    static LanguageManager instance;
    return instance;
}

LanguageManager::LanguageManager()
    : QObject(nullptr)
    , m_currentLanguage("en")
{
    scanAvailableLanguages();
    loadLanguagePreference();
}

void LanguageManager::scanAvailableLanguages()
{
    m_availableLanguages.clear();
    
    QStringList searchPaths;
    searchPaths << QCoreApplication::applicationDirPath() + "/translations";
    searchPaths << QCoreApplication::applicationDirPath();
    searchPaths << QCoreApplication::applicationDirPath() + "/../src/resources/translations";
    searchPaths << QCoreApplication::applicationDirPath() + "/../../src/resources/translations";
    searchPaths << QCoreApplication::applicationDirPath() + "/../../../src/resources/translations";
    
    for (const QString& searchPath : searchPaths)
    {
        QDir dir(searchPath);
        if (!dir.exists())
            continue;
        
        // Look for vortigaunt_*.qm files
        QStringList filters;
        filters << "vortigaunt_*.qm";
        
        QFileInfoList files = dir.entryInfoList(filters, QDir::Files);
        for (const QFileInfo& fileInfo : files)
        {
            QString fileName = fileInfo.baseName(); // example: "vortigaunt_tr"
            QString langCode = fileName.mid(11);    // example: "tr" after "vortigaunt_"
            
            if (!m_availableLanguages.contains(langCode))
            {
                m_availableLanguages.append(langCode);
            }
        }
        
        filters.clear();
        filters << "vortigaunt_*.ts";
        
        files = dir.entryInfoList(filters, QDir::Files);
        for (const QFileInfo& fileInfo : files)
        {
            QString fileName = fileInfo.baseName();
            QString langCode = fileName.mid(11);
            
            if (!m_availableLanguages.contains(langCode))
            {
                m_availableLanguages.append(langCode);
            }
        }
    }
    

    if (!m_availableLanguages.contains("en"))
    {
        m_availableLanguages.prepend("en");
    }
    
    std::sort(m_availableLanguages.begin(), m_availableLanguages.end(),
        [this](const QString& a, const QString& b) {
            return getLanguageDisplayName(a) < getLanguageDisplayName(b);
        });
    
    qDebug() << "Available languages:" << m_availableLanguages;
}

bool LanguageManager::loadLanguage(const QString& languageCode)
{
    QString langCode = languageCode.toLower();
    
    // Don't reload if already loaded
    if (langCode == m_currentLanguage && !m_currentLanguage.isEmpty())
    {
        return true;
    }
    
    QCoreApplication::removeTranslator(&m_appTranslator);
    QCoreApplication::removeTranslator(&m_qtTranslator);
    
    // Search for translation file
    QStringList searchPaths;
    searchPaths << QCoreApplication::applicationDirPath() + "/translations";
    searchPaths << QCoreApplication::applicationDirPath();
    searchPaths << QCoreApplication::applicationDirPath() + "/../src/resources/translations";
    searchPaths << QCoreApplication::applicationDirPath() + "/../../src/resources/translations";
    searchPaths << QCoreApplication::applicationDirPath() + "/../../../src/resources/translations";
    
    bool appTranslatorLoaded = false;
    
    for (const QString& searchPath : searchPaths)
    {
        QString qmFile = searchPath + "/vortigaunt_" + langCode + ".qm";
        if (QFile::exists(qmFile))
        {
            if (m_appTranslator.load(qmFile))
            {
                QCoreApplication::installTranslator(&m_appTranslator);
                appTranslatorLoaded = true;
                qDebug() << "Loaded app translation:" << qmFile;
                break;
            }
        }
    }
    
    // For English, no translation needed (source language)
    if (langCode == "en")
    {
        appTranslatorLoaded = true;
    }
    
    if (!appTranslatorLoaded && langCode != "en")
    {
        qWarning() << "Could not load translation for language:" << langCode;
        // Fall back to English
        langCode = "en";
    }
    
    // Load Qt's built-in translations (for standard dialogs)
    QString qtTranslationsPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
    if (m_qtTranslator.load("qt_" + langCode, qtTranslationsPath))
    {
        QCoreApplication::installTranslator(&m_qtTranslator);
        qDebug() << "Loaded Qt translation for:" << langCode;
    }
    
    m_currentLanguage = langCode;
    saveLanguagePreference();
    
    emit languageChanged();
    return appTranslatorLoaded;
}

QStringList LanguageManager::getAvailableLanguages() const
{
    return m_availableLanguages;
}

QString LanguageManager::getCurrentLanguage() const
{
    return m_currentLanguage;
}

QString LanguageManager::getLanguageDisplayName(const QString& languageCode) const
{
    QString code = languageCode.toLower();
    
    if (s_displayNames.contains(code))
    {
        return s_displayNames[code];
    }
    
    return code.left(1).toUpper() + code.mid(1);
}

void LanguageManager::saveLanguagePreference()
{
    QSettings settings("Vortigaunt", "VortigauntTool");
    settings.setValue("language", m_currentLanguage);
}

void LanguageManager::loadLanguagePreference()
{
    QSettings settings("Vortigaunt", "VortigauntTool");
    QString savedLang = settings.value("language", "en").toString();
    
    // Try to load saved language
    if (!loadLanguage(savedLang))
    {
        if (savedLang != "en")
        {
            loadLanguage("en");
        }
    }
}
