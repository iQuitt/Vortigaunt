#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <functional>

// XFS file entry structure
struct XfsEntry
{
    std::string filename;       // File name/path within archive
    std::uint64_t offset;       // Offset to compressed data in archive
    std::uint32_t packedSize;   // Compressed size
    std::uint32_t unpackedSize; // Decompressed size
};

// XFS Archive Extractor (Xenesis File System)
// This Archive file made by Softnyx company for their games like Rakion, Wolfteam, etc.
// Tested only Wolfteam. it may work other Softnyx games. Not guaranteed.

#ifdef QT_CORE_LIB
#include "core/extractors/IExtractor.h"
class XfsExtractor : public IExtractor
#else
class XfsExtractor
#endif
{
public:
    void Initialize();
    void Shutdown();

#ifdef QT_CORE_LIB
    // IExtractor Implementation
    QString GetName() const override { return "XfsExtractor"; }
    QStringList GetSupportedExtensions() const override { return { "xfs" }; }
    bool Extract(const QString& inputFile, const QString& outputDir) override;
#endif

    using ProgressFunc = std::function<void(int percent)>;

    // Set custom progress callback. If not set, no progress updates are sent.
    static void SetProgressFunc(const ProgressFunc& func);
    static ProgressFunc s_progressFunc;

    // Parse file index table from XFS archive.
    bool Load(const std::string& xfsFilePath);

    // Extract all files to outputDir
    bool ExtractAll(const std::string& outputDir) const;

    // Extract selected entries by indices
    bool ExtractSelectedEntries(const std::vector<size_t>& indices, const std::string& outputDir) const;

    // Extract a single entry to the specified output path
    bool ExtractEntry(const XfsEntry& entry, const std::string& outputPath) const;

    // Extract a single entry to memory
    bool ExtractEntryToMemory(const XfsEntry& entry, std::vector<uint8_t>& outData) const;

    // Getters
    const std::vector<XfsEntry>& GetEntries() const { return m_entries; }
    const std::string& GetFilePath() const { return m_xfsFilePath; }
    std::uint64_t GetTotalUnpackedSize() const;
    std::uint64_t GetTotalPackedSize() const;

private:
    static void ReportProgress(int percent);

    // Parse the compressed file info table from archive tail
    bool ParseFileInfoTable(std::ifstream& in);

    // Decompress chunked zlib data
    bool DecompressChunked(const uint8_t* source, size_t sourceLen,
                           std::vector<uint8_t>& dest, size_t expectedSize) const;

private:
    std::string m_xfsFilePath;
    std::vector<XfsEntry> m_entries;
    std::uint64_t m_fileSize = 0;
};
