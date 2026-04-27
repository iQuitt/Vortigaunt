#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <fstream>

struct RezEntry
{
    std::string filename;
    std::uint32_t offset;
    std::uint32_t size;
    std::uint32_t flags;
};

#ifdef QT_CORE_LIB
#include "core/extractors/IExtractor.h"
class RezExtractor : public IExtractor
#else
class RezExtractor
#endif
{
public:
    void Initialize();
    void Shutdown();

#ifdef QT_CORE_LIB
    // IExtractor Implementation
    QString GetName() const override { return "RezExtractor"; }
    QStringList GetSupportedExtensions() const override { return { "rez" }; }
    bool Extract(const QString& inputFile, const QString& outputDir) override;
#endif

    using ProgressFunc = std::function<void(int percent)>;
    using FileProgressFunc = std::function<void(size_t current, size_t total)>;

    // Set custom progress callback. If not set, no progress updates are sent.
    static void SetProgressFunc(const ProgressFunc& func);

    // Parse directory table and fill m_entries.
    bool Load(const std::string& rezFilePath);

    // Extract all files in the REZ archive into outputDir.
    bool ExtractAll(const std::string& outputDir) const;

    // Extract selected entries by indices
    bool ExtractSelectedEntries(const std::vector<size_t>& indices, const std::string& outputDir) const;

    // Extract a single entry to the specified output path
    bool ExtractEntry(const RezEntry& entry, const std::string& outputPath) const;

    // Extract a single entry and return its data in memory
    bool ExtractEntryToMemory(const RezEntry& entry, std::vector<char>& outData) const;


    const std::vector<RezEntry>& GetEntries() const { return m_entries; }
    const std::string& GetFilePath() const { return m_rezFilePath; }

private:
    static ProgressFunc s_progressFunc;
    static FileProgressFunc s_fileProgressFunc;
    static std::function<bool()> s_cancelFunc;
    static void ReportProgress(int percent);
    static void ReportFileProgress(size_t current, size_t total);

    bool ParseDirectory(std::ifstream& in);

private:
    std::string m_rezFilePath;
    std::vector<RezEntry> m_entries;
};

