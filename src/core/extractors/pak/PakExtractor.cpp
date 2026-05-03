#include "PakExtractor.h"
#include "PakFile.h"
#include "utils/util.hpp"
#include "core/VortigauntLog.h"
#include "utils/fsutils.hpp"

#include <filesystem>
#include <fstream>
#include "utils/FileIO.h"
#include <cstdarg>
#include <cstdio>
#include <algorithm>

// Static members
PakExtractor::ProgressFunc PakExtractor::s_progressFunc;
PakExtractor::FileProgressFunc PakExtractor::s_fileProgressFunc;

void PakExtractor::Initialize()
{
    s_progressFunc = nullptr;
    s_fileProgressFunc = nullptr;
}

void PakExtractor::Shutdown()
{
    s_progressFunc = nullptr;
    s_fileProgressFunc = nullptr;
}

#ifdef QT_CORE_LIB
bool PakExtractor::Extract(const QString& inputFile, const QString& outputDir)
{
    return ExtractSingle(inputFile.toStdString(), outputDir.toStdString());
}
#endif

void PakExtractor::SetProgressFunc(const ProgressFunc& func)
{
    s_progressFunc = func;
}




void PakExtractor::ReportProgress(int percent)
{
    if (s_progressFunc)
        s_progressFunc(percent);
}

void PakExtractor::ReportFileProgress(size_t current, size_t total)
{
    if (s_fileProgressFunc)
        s_fileProgressFunc(current, total);
}

bool PakExtractor::ExtractSingle(const std::string& pakPath, const std::string& outputDir)
{
    namespace fs = std::filesystem;

    fs::path pakFsPath = FileIO::toPath(pakPath);
    if (!fs::exists(pakFsPath))
    {
        VortigauntLog::LogF("Error: Input path does not exist: %s", pakPath.c_str());
        return false;
    }

    auto ext = pakFsPath.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext != ".pak")
    {
        VortigauntLog::LogF("^2Error: Input is not a .pak file: %s", pakPath.c_str());
        return false;
    }

    std::string baseOut = outputDir;
    if (baseOut.empty())
    {
        baseOut = "VortigauntOutput";
    }
    std::error_code ec;
    fs::create_directories(FileIO::toPath(baseOut), ec);
    if (ec)
    {
        VortigauntLog::LogF("^2Error:^7 failed to create output directory: %s", ec.message().c_str());
        return false;
    }

    VortigauntLog::LogF("PAK extract: %s", pakPath.c_str());
    VortigauntLog::LogF("Output folder: %s", baseOut.c_str());

    // Read entire PAK file into memory
    std::ifstream is(pakFsPath, std::ios::binary | std::ios::ate);
    if (!is)
    {
        VortigauntLog::LogF("^2Error: cannot open PAK file.");
        return false;
    }

    std::streamsize size = is.tellg();
    if (size <= 0)
    {
        VortigauntLog::LogF("^2Error: invalid PAK file size.");
        return false;
    }

    is.seekg(0, std::ios::beg);
    std::vector<uint8_t> buffer(static_cast<size_t>(size));
    if (!is.read(reinterpret_cast<char*>(buffer.data()), size))
    {
        VortigauntLog::LogF("^2Error:^7 failed to read PAK file data.");
        return false;
    }

    std::u16string u16Name = pakFsPath.filename().generic_u16string();

    PakFile pak(std::move(buffer), std::move(u16Name));
    if (!pak.ParseHeader())
    {
        VortigauntLog::LogF("^2Error:^7 invalid PAK header.");
        return false;
    }

    if (!pak.ParseEntries())
    {
        VortigauntLog::LogF("^2Error:^7 failed to parse PAK entries.");
        return false;
    }

    const auto& entries = pak.GetEntries();
    if (entries.empty())
    {
        VortigauntLog::LogF("^3Warning:^7 PAK has no entries.");
        return true;  // Not an error, just empty
    }

    fs::path baseOutPath = FileIO::toPath(baseOut);
    size_t okCount = 0;
    size_t totalEntries = entries.size();

    for (size_t i = 0; i < entries.size(); ++i)
    {


        const auto& entry = entries[i];
        std::string utf8Path = String_UTF16toUTF8(entry.FilePath);
        fs::path relPath = fs::path(std::u16string(entry.FilePath));
        fs::path outPath = baseOutPath / relPath;

        VortigauntLog::LogF("  ^7[%zu/%zu] %s ^3(%s)^7\n", i + 1, entries.size(), utf8Path.c_str(), FormatSize(entry.RealSize).c_str());

        auto result = pak.UnpackEntry(entry);
        const bool ok = result.first;
        const auto& data = result.second;

        if (!ok)
        {
            VortigauntLog::LogF("    ^1-> FAILED ^7(unpack returned false)");
            continue;
        }

        std::error_code dirEc;
        fs::create_directories(outPath.parent_path(), dirEc);
        // ignore dir creation error, file creation will fail if it didn't work

        std::ofstream os(outPath, std::ios::binary);
        if (!os)
        {
            VortigauntLog::LogF("    ^1-> FAILED (cannot create output file)");
            continue;
        }

        os.write(reinterpret_cast<const char*>(data.data()),
                    static_cast<std::streamsize>(data.size()));
        ++okCount;

        // Report progress
        int percent = static_cast<int>((i + 1) * 100 / totalEntries);
        ReportProgress(percent);
    }

    VortigauntLog::LogF("^2PAK extraction finished. ^3%zu/%zu ^2files written to %s.", okCount, entries.size(), outputDir.c_str());

    return okCount > 0;
}

size_t PakExtractor::ExtractMultiple(const std::vector<std::string>& pakPaths, const std::string& outputDir, bool separateFolders)
{
    namespace fs = std::filesystem;

    if (pakPaths.empty())
    {
        VortigauntLog::LogF("No PAK files selected.");
        return 0;
    }

    VortigauntLog::LogF("Extracting %zu PAK files...", pakPaths.size());

    size_t successCount = 0;
    size_t totalFiles = pakPaths.size();

    for (size_t i = 0; i < pakPaths.size(); ++i)
    {


        const std::string& pakPath = pakPaths[i];

        ReportFileProgress(i + 1, totalFiles);

        fs::path pakFsPath = FileIO::toPath(pakPath);
        VortigauntLog::LogF("--- Extracting: %s ---", pakFsPath.filename().string().c_str());

        auto ext = pakFsPath.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext != ".pak")
        {
            VortigauntLog::LogF("  ^3Skipped (not a .pak file)");
            continue;
        }

        if (!fs::exists(pakFsPath))
        {
            VortigauntLog::LogF("  ^3Skipped (file not found)");
            continue;
        }

        // Determine output directory
        std::string targetDir;
        if (separateFolders)
        {
            targetDir = (FileIO::toPath(outputDir) / pakFsPath.stem()).string();
        }
        else
        {
            targetDir = outputDir;
        }

        if (ExtractSingle(pakPath, targetDir))
        {
            ++successCount;
        }
    }

    VortigauntLog::LogF("^2PAK extraction complete. ^5%zu of %zu ^2files processed.", successCount, pakPaths.size());
    return successCount;
}
