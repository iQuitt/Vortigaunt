#pragma once

#include <QString>
#include <QStringList>
#include <QObject>
#include <QTranslator>
#include <QMap>

/**
 * LanguageManager - Qt Linguist based translation manager
 * 
 * Uses Qt's QTranslator system with .qm files for translations.
 * Supports runtime language switching with UI refresh.
 */
class LanguageManager : public QObject
{
    Q_OBJECT

public:
    static LanguageManager& instance();
    
    // Load a language by code (e.g., "en", "tr", "pl")
    bool loadLanguage(const QString& languageCode);
    
    // Get list of available languages (scanned from translations folder)
    QStringList getAvailableLanguages() const;
    
    // Get current language code
    QString getCurrentLanguage() const;
    
    // Get display name for a language code (e.g., "en" -> "English")
    QString getLanguageDisplayName(const QString& languageCode) const;

signals:
    // Emitted when language changes - connect to retranslateUi() slots
    void languageChanged();

private:
    LanguageManager();
    ~LanguageManager() = default;
    LanguageManager(const LanguageManager&) = delete;
    LanguageManager& operator=(const LanguageManager&) = delete;
    
    void scanAvailableLanguages();
    void saveLanguagePreference();
    void loadLanguagePreference();
    
    QTranslator m_appTranslator;       // App translations
    QTranslator m_qtTranslator;        // Qt built-in translations
    QString m_currentLanguage;         // Current language code (e.g., "en")
    QStringList m_availableLanguages;  // List of available language codes
    
  
    static inline QMap<QString, QString> s_displayNames = {
        {"en", "English"},
        {"tr", "Türkçe"},
        // for now english and turkish available
        {"de", "Deutsch"},
        {"fr", "Français"},
        {"es", "Español"},
        {"ru", "Русский"},
        {"zh", "中文"},
        {"ja", "日本語"},
        {"ko", "한국어"},
        {"pt", "Português"},
        {"it", "Italiano"},
        {"pl", "Polski"}
    };
};
