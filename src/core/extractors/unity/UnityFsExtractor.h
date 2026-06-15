#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>
#include <memory>

struct UnityEntry
{
    std::string filename;
    std::uint64_t offset;
    std::uint64_t size;
    std::uint32_t flags;
};

#ifdef QT_CORE_LIB
#include "core/extractors/IExtractor.h"
class UnityFsExtractor : public IExtractor
#else
class UnityFsExtractor
#endif
{
public:
    void Initialize();
    void Shutdown();

#ifdef QT_CORE_LIB
    // IExtractor Implementation
    QString GetName() const override { return "UnityFsExtractor"; }
    QStringList GetSupportedExtensions() const override { return { "unity3d", "bundle" }; }
    bool CanHandle(const QString& filePath) const override
    {
        QString ext = filePath.section('.', -1).toLower();
        return ext == "unity3d" || ext == "bundle";
    }
    bool Extract(const QString& inputFile, const QString& outputDir) override;
#endif

    using ProgressFunc = std::function<void(int percent)>;
    using FileProgressFunc = std::function<void(size_t current, size_t total)>;

    // Set custom progress callbacks
    static void SetProgressFunc(const ProgressFunc& func);
    static void SetFileProgressFunc(const FileProgressFunc& func);

    // Parse bundle directory and fill m_entries
    bool Load(const std::string& bundleFilePath);

    // Extract all files in the UnityFS archive into outputDir
    bool ExtractAll(const std::string& outputDir) const;

    // Extract selected entries by indices
    bool ExtractSelectedEntries(const std::vector<size_t>& indices, const std::string& outputDir) const;

    // Extract a single entry to memory
    bool ExtractEntryToMemory(const UnityEntry& entry, std::vector<char>& outData) const;

    const std::vector<UnityEntry>& GetEntries() const { return m_entries; }
    const std::string& GetFilePath() const { return m_bundleFilePath; }

private:
    static ProgressFunc s_progressFunc;
    static FileProgressFunc s_fileProgressFunc;

    static void ReportProgress(int percent);
    static void ReportFileProgress(size_t current, size_t total);

    struct BlockInfo
    {
        std::uint32_t decompressedSize;
        std::uint32_t compressedSize;
        std::uint16_t flags; // 0 = uncompressed, 1 = lzma, 2/3 = lz4
    };



private:
    std::string m_bundleFilePath;
    std::vector<UnityEntry> m_entries;
    std::vector<BlockInfo> m_blocks;
    std::uint64_t m_blocksOffset = 0; // File offset where block data begins
};
