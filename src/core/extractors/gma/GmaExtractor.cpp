#include "GmaExtractor.h"
#include "GmaFile.h"
#include "core/VortigauntLog.h"

#include <filesystem>
#include <fstream>
#include <algorithm>

GmaExtractor::ProgressFunc GmaExtractor::s_progressFunc;
GmaExtractor::FileProgressFunc GmaExtractor::s_fileProgressFunc;
std::function<bool()> GmaExtractor::s_cancelFunc;

void GmaExtractor::Initialize()
{
    s_progressFunc = nullptr;
    s_fileProgressFunc = nullptr;
    s_cancelFunc = nullptr;
}

void GmaExtractor::Shutdown()
{
    s_progressFunc = nullptr;
    s_fileProgressFunc = nullptr;
    s_cancelFunc = nullptr;
}

#ifdef QT_CORE_LIB
bool GmaExtractor::Extract(const QString& inputFile, const QString& outputDir)
{
    return ExtractSingle(inputFile.toStdString(), outputDir.toStdString());
}
#endif

void GmaExtractor::SetProgressFunc(const ProgressFunc& func)
{
    s_progressFunc = func;
}

void GmaExtractor::SetFileProgressFunc(const FileProgressFunc& func)
{
    s_fileProgressFunc = func;
}

void GmaExtractor::SetCancelFunc(std::function<bool()> func)
{
    s_cancelFunc = func;
}

void GmaExtractor::ReportProgress(int percent)
{
    if (s_progressFunc)
        s_progressFunc(percent);
}

void GmaExtractor::ReportFileProgress(size_t current, size_t total)
{
    if (s_fileProgressFunc)
        s_fileProgressFunc(current, total);
}

bool GmaExtractor::ExtractSingle(const std::string& gmaPath, const std::string& outputDir)
{
    namespace fs = std::filesystem;

    if (!fs::exists(gmaPath))
    {
        VortigauntLog::LogF("Error: Input path does not exist: %s", gmaPath.c_str());
        return false;
    }

    std::string ext = fs::path(gmaPath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".gma")
    {
        VortigauntLog::LogF("Error: Input is not a .gma file: %s", gmaPath.c_str());
        return false;
    }

    std::string baseOut = outputDir;
    if (baseOut.empty())
        baseOut = "VortigauntOutput";

    fs::path outputPath = fs::path(baseOut);
    std::error_code ec;
    fs::create_directories(outputPath, ec);
    if (ec)
    {
        VortigauntLog::LogF("Error: Failed to create output directory: %s", ec.message().c_str());
        return false;
    }

    VortigauntLog::LogF("GMA extract: %s", gmaPath.c_str());
    VortigauntLog::LogF("Output folder: %s", baseOut.c_str());

    GmaFile gma;
    if (!gma.Load(gmaPath))
    {
        VortigauntLog::LogF("Error: Failed to load GMA file.");
        return false;
    }

    if (!gma.GetName().empty())
        VortigauntLog::LogF("Addon: %s (author: %s)", gma.GetName().c_str(),
                            gma.GetAuthor().empty() ? "unknown" : gma.GetAuthor().c_str());

    const auto& entries = gma.GetEntries();
    if (entries.empty())
    {
        VortigauntLog::LogF("Warning: GMA has no entries.");
        return true;
    }

    size_t okCount = 0;
    size_t totalEntries = entries.size();

    for (size_t i = 0; i < entries.size(); ++i)
    {
        if (s_cancelFunc && s_cancelFunc())
        {
            VortigauntLog::LogF("Extraction cancelled by user.");
            break;
        }

        const auto& entry = entries[i];

        VortigauntLog::LogF("  [%zu/%zu] %s", i + 1, entries.size(), entry.fullPath.c_str());

        if (!GmaFile::IsSafeEntryPath(entry.fullPath))
        {
            VortigauntLog::LogF("    -> SKIPPED (unsafe path)");
            continue;
        }

        fs::path outFilePath = outputPath / fs::path(entry.fullPath);
        std::error_code dirEc;
        fs::create_directories(outFilePath.parent_path(), dirEc);

        std::vector<uint8_t> data = gma.ExtractEntry(entry);
        if (data.empty() && entry.size != 0)
        {
            VortigauntLog::LogF("    -> FAILED (read error)");
            continue;
        }

        std::ofstream os(outFilePath, std::ios::binary);
        if (!os)
        {
            VortigauntLog::LogF("    -> FAILED (cannot create output file)");
            continue;
        }

        if (!data.empty())
            os.write(reinterpret_cast<const char*>(data.data()),
                     static_cast<std::streamsize>(data.size()));
        ++okCount;

        int percent = static_cast<int>((i + 1) * 100 / totalEntries);
        ReportProgress(percent);
    }

    VortigauntLog::LogF("GMA extraction finished. %zu/%zu files written to %s.",
                        okCount, entries.size(), baseOut.c_str());

    return okCount > 0;
}

size_t GmaExtractor::ExtractMultiple(const std::vector<std::string>& gmaPaths,
                                     const std::string& outputDir,
                                     bool separateFolders)
{
    namespace fs = std::filesystem;

    if (gmaPaths.empty())
    {
        VortigauntLog::LogF("No GMA files selected.");
        return 0;
    }

    VortigauntLog::LogF("Extracting %zu GMA files...", gmaPaths.size());

    size_t successCount = 0;
    size_t totalFiles = gmaPaths.size();

    for (size_t i = 0; i < gmaPaths.size(); ++i)
    {
        const std::string& gmaPath = gmaPaths[i];
        ReportFileProgress(i + 1, totalFiles);

        fs::path gmaFsPath = fs::path(gmaPath);
        VortigauntLog::LogF("--- Extracting: %s ---", gmaFsPath.filename().string().c_str());

        std::string ext = gmaFsPath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".gma")
        {
            VortigauntLog::LogF("  Skipped (not a .gma file)");
            continue;
        }

        if (!fs::exists(gmaFsPath))
        {
            VortigauntLog::LogF("  Skipped (file not found)");
            continue;
        }

        std::string targetDir;
        if (separateFolders)
            targetDir = (fs::path(outputDir) / gmaFsPath.stem()).string();
        else
            targetDir = outputDir;

        if (ExtractSingle(gmaPath, targetDir))
            ++successCount;
    }

    VortigauntLog::LogF("GMA extraction complete. %zu of %zu files processed.", successCount, gmaPaths.size());
    return successCount;
}
