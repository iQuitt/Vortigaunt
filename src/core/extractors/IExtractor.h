#pragma once

#include <functional>

#ifdef QT_CORE_LIB
#include <QString>
#include <QStringList>

/**
 * @brief Interface for all file extractors.
 * Allows uniform management via ExtractArchive.
 */
class IExtractor
{
public:
    virtual ~IExtractor() = default;

    /**
     * @brief Initialize the extractor system.
     */
    virtual void Initialize() = 0;

    /**
     * @brief Shutdown the extractor system.
     */
    virtual void Shutdown() = 0;

    /**
     * @brief Get the name of this extractor (e.g., "RezExtractor").
     */
    virtual QString GetName() const = 0;

    /**
     * @brief Get supported file extensions (e.g., "rez", "pak").
     */
    virtual QStringList GetSupportedExtensions() const = 0;

    /**
     * @brief Check if this extractor can handle the given file.
     * default implementation checks extension.
     */
    virtual bool CanHandle(const QString& filePath) const
    {
        QString ext = filePath.section('.', -1).toLower();
        return GetSupportedExtensions().contains(ext, Qt::CaseInsensitive);
    }

    // Callbacks type definitions (aligned with existing ones)
    using ProgressFunc = std::function<void(int percent)>;
    using FileProgressFunc = std::function<void(int current, int total)>;
    
    virtual bool Extract(const QString& inputFile, const QString& outputDir) = 0;
};

#endif // QT_CORE_LIB
