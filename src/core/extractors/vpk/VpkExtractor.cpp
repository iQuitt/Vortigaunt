#include "VpkExtractor.h"
#include "VpkFile.h"
#include "core/VortigauntLog.h"

#include <filesystem>
#include <fstream>
#include <algorithm>

VpkExtractor::ProgressFunc VpkExtractor::s_progressFunc;
VpkExtractor::FileProgressFunc VpkExtractor::s_fileProgressFunc;
std::function<bool()> VpkExtractor::s_cancelFunc;

void VpkExtractor::Initialize()
{
    s_progressFunc = nullptr;
    s_fileProgressFunc = nullptr;
    s_cancelFunc = nullptr;
}

void VpkExtractor::Shutdown()
{
    s_progressFunc = nullptr;
    s_fileProgressFunc = nullptr;
    s_cancelFunc = nullptr;
}

#ifdef QT_CORE_LIB
bool VpkExtractor::Extract(const QString& inputFile, const QString& outputDir)
{
    return ExtractSingle(inputFile.toStdString(), outputDir.toStdString());
}
#endif

void VpkExtractor::SetProgressFunc(const ProgressFunc& func)
{
    s_progressFunc = func;
}

void VpkExtractor::SetFileProgressFunc(const FileProgressFunc& func)
{
    s_fileProgressFunc = func;
}

void VpkExtractor::SetCancelFunc(std::function<bool()> func)
{
    s_cancelFunc = func;
}

void VpkExtractor::ReportProgress(int percent)
{
    if (s_progressFunc)
        s_progressFunc(percent);
}

void VpkExtractor::ReportFileProgress(size_t current, size_t total)
{
    if (s_fileProgressFunc)
        s_fileProgressFunc(current, total);
}

bool VpkExtractor::ExtractSingle(const std::string& vpkPath, const std::string& outputDir)
{
    namespace fs = std::filesystem;

    if (!fs::exists(vpkPath))
    {
        VortigauntLog::LogF("Error: Input path does not exist: %s", vpkPath.c_str());
        return false;
    }

    std::string ext = fs::path(vpkPath).extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".vpk")
    {
        VortigauntLog::LogF("Error: Input is not a .vpk file: %s", vpkPath.c_str());
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

    VortigauntLog::LogF("VPK extract: %s", vpkPath.c_str());
    VortigauntLog::LogF("Output folder: %s", baseOut.c_str());

    VpkFile vpk;
    if (!vpk.Load(vpkPath))
    {
        VortigauntLog::LogF("Error: Failed to load VPK file.");
        return false;
    }

    const auto& entries = vpk.GetEntries();
    if (entries.empty())
    {
        VortigauntLog::LogF("Warning: VPK has no entries.");
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

        std::vector<uint8_t> data = vpk.ExtractEntry(entry);
        if (data.empty())
        {
            VortigauntLog::LogF("    -> FAILED (empty data)");
            continue;
        }

        fs::path outFilePath = outputPath / fs::path(entry.fullPath);
        std::error_code dirEc;
        fs::create_directories(outFilePath.parent_path(), dirEc);

        std::ofstream os(outFilePath, std::ios::binary);
        if (!os)
        {
            VortigauntLog::LogF("    -> FAILED (cannot create output file)");
            continue;
        }

        os.write(reinterpret_cast<const char*>(data.data()),
                 static_cast<std::streamsize>(data.size()));
        ++okCount;

        int percent = static_cast<int>((i + 1) * 100 / totalEntries);
        ReportProgress(percent);
    }

    VortigauntLog::LogF("VPK extraction finished. %zu/%zu files written to %s.",
                        okCount, entries.size(), baseOut.c_str());

    return okCount > 0;
}

size_t VpkExtractor::ExtractMultiple(const std::vector<std::string>& vpkPaths,
                                     const std::string& outputDir,
                                     bool separateFolders)
{
    namespace fs = std::filesystem;

    if (vpkPaths.empty())
    {
        VortigauntLog::LogF("No VPK files selected.");
        return 0;
    }

    VortigauntLog::LogF("Extracting %zu VPK files...", vpkPaths.size());

    size_t successCount = 0;
    size_t totalFiles = vpkPaths.size();

    for (size_t i = 0; i < vpkPaths.size(); ++i)
    {
        const std::string& vpkPath = vpkPaths[i];
        ReportFileProgress(i + 1, totalFiles);

        fs::path vpkFsPath = fs::path(vpkPath);
        VortigauntLog::LogF("--- Extracting: %s ---", vpkFsPath.filename().string().c_str());

        std::string ext = vpkFsPath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".vpk")
        {
            VortigauntLog::LogF("  Skipped (not a .vpk file)");
            continue;
        }

        if (!fs::exists(vpkFsPath))
        {
            VortigauntLog::LogF("  Skipped (file not found)");
            continue;
        }

        std::string targetDir;
        if (separateFolders)
            targetDir = (fs::path(outputDir) / vpkFsPath.stem()).string();
        else
            targetDir = outputDir;

        if (ExtractSingle(vpkPath, targetDir))
            ++successCount;
    }

    VortigauntLog::LogF("VPK extraction complete. %zu of %zu files processed.", successCount, vpkPaths.size());
    return successCount;
}
