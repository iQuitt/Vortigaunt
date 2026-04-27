#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

/**
 * @brief PAK file extraction utility class (Core version - no Qt dependencies)
 * 
 * Provides functionality to extract single or multiple PAK files,
 * with support for progress tracking via callbacks.
 */
#ifdef QT_CORE_LIB
#include "core/extractors/IExtractor.h"
#endif

/**
 * @brief PAK file extraction utility class (Core version - no Qt dependencies)
 * 
 * Provides functionality to extract single or multiple PAK files,
 * with support for progress tracking via callbacks.
 */
#ifdef QT_CORE_LIB
class PakExtractor : public IExtractor
#else
class PakExtractor
#endif
{
public:
    void Initialize();
    void Shutdown();

#ifdef QT_CORE_LIB
    // IExtractor Implementation
    QString GetName() const override { return "PakExtractor"; }
    QStringList GetSupportedExtensions() const override { return { "pak" }; }
    bool Extract(const QString& inputFile, const QString& outputDir) override;
#endif

    using ProgressFunc = std::function<void(int percent)>;
    using FileProgressFunc = std::function<void(size_t current, size_t total)>;

    /**
     * @brief Set custom progress callback. If not set, no progress updates are sent.
     */
    static void SetProgressFunc(const ProgressFunc& func);

    /**
     * @brief Set custom file progress callback for file count updates (current/total).
     */
    static void SetFileProgressFunc(const FileProgressFunc& func);

    static std::function<bool()> s_cancelFunc;

    // Set cancellation check callback.
    static void SetCancelFunc(std::function<bool()> func);

    /**
     * @brief Extract a single PAK file to the output directory
     * @param pakPath Path to the PAK file
     * @param outputDir Output directory path
     * @return true if extraction was successful
     */
    bool ExtractSingle(const std::string& pakPath, const std::string& outputDir);

    /**
     * @brief Extract multiple PAK files to the output directory
     * @param pakPaths List of PAK file paths
     * @param outputDir Output directory path
     * @param separateFolders If true, extract each PAK to a separate subfolder
     * @return Number of successfully extracted PAK files
     */
    size_t ExtractMultiple(const std::vector<std::string>& pakPaths, 
                           const std::string& outputDir,
                           bool separateFolders = false);

 
    /**
     * @brief Check if extraction was cancelled
     */
    bool IsCancelled() const { return m_cancelled; }

    /**
     * @brief Request cancellation of ongoing extraction
     */
    void Cancel() { m_cancelled = true; }

    /**
     * @brief Reset cancellation flag
     */
    void ResetCancel() { m_cancelled = false; }

private:
    static ProgressFunc s_progressFunc;
    static FileProgressFunc s_fileProgressFunc;
    static void ReportProgress(int percent);
    static void ReportFileProgress(size_t current, size_t total);

    bool m_cancelled = false;
};
