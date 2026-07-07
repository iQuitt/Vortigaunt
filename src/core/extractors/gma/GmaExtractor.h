#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

#ifdef QT_CORE_LIB
#include "core/extractors/IExtractor.h"
#endif

#ifdef QT_CORE_LIB
class GmaExtractor : public IExtractor
#else
class GmaExtractor
#endif
{
public:
    void Initialize();
    void Shutdown();

#ifdef QT_CORE_LIB
    QString GetName() const override { return "GmaExtractor"; }
    QStringList GetSupportedExtensions() const override { return { "gma" }; }
    bool Extract(const QString& inputFile, const QString& outputDir) override;
#endif

    using ProgressFunc = std::function<void(int percent)>;
    using FileProgressFunc = std::function<void(size_t current, size_t total)>;

    static void SetProgressFunc(const ProgressFunc& func);
    static void SetFileProgressFunc(const FileProgressFunc& func);

    static void SetCancelFunc(std::function<bool()> func);
    static std::function<bool()> s_cancelFunc;

    bool ExtractSingle(const std::string& gmaPath, const std::string& outputDir);
    size_t ExtractMultiple(const std::vector<std::string>& gmaPaths,
                           const std::string& outputDir,
                           bool separateFolders = false);

    bool IsCancelled() const { return m_cancelled; }
    void Cancel() { m_cancelled = true; }
    void ResetCancel() { m_cancelled = false; }

private:
    static ProgressFunc s_progressFunc;
    static FileProgressFunc s_fileProgressFunc;
    static void ReportProgress(int percent);
    static void ReportFileProgress(size_t current, size_t total);

    bool m_cancelled = false;
};
