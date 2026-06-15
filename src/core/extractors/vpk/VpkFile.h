#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <fstream>

#pragma pack(push, 1)
struct VpkHeaderV1
{
    uint32_t signature;
    uint32_t version;
    uint32_t treeSize;
};

struct VpkHeaderV2
{
    uint32_t signature;
    uint32_t version;
    uint32_t treeSize;
    uint32_t fileDataSize;
    uint32_t archiveMD5Size;
    uint32_t otherMD5Size;
    uint32_t signatureSize;
};

struct VpkDirEntry
{
    uint32_t crc32;
    uint16_t preloadBytes;
    uint16_t archiveIndex;
    uint32_t entryOffset;
    uint32_t entryLength;
    uint16_t terminator;
};
#pragma pack(pop)

constexpr uint32_t VPK_SIGNATURE = 0x55AA1234;
constexpr uint16_t VPK_DIR_ARCHIVE_INDEX = 0x7FFF;
constexpr uint16_t VPK_ENTRY_TERMINATOR = 0xFFFF;

struct VpkEntry
{
    std::string fullPath;
    uint32_t crc32;
    uint16_t preloadBytes;
    uint16_t archiveIndex;
    uint32_t entryOffset;
    uint32_t entryLength;
    std::vector<uint8_t> preloadData;

    bool isInDirVpk() const { return archiveIndex == VPK_DIR_ARCHIVE_INDEX; }
};

class VpkFile
{
public:
    VpkFile() = default;
    ~VpkFile() = default;

    bool Load(const std::string& filePath);
    bool LoadFromBuffer(std::vector<uint8_t> buffer, const std::string& filePath);

    const std::vector<VpkEntry>& GetEntries() const { return m_entries; }
    size_t GetEntryCount() const { return m_entries.size(); }

    std::vector<uint8_t> ExtractEntry(const VpkEntry& entry) const;

    uint32_t GetVersion() const { return m_version; }
    const std::string& GetPath() const { return m_filePath; }
    std::string GetBaseName() const;

    void Clear();

private:
    bool ParseHeader();
    bool ParseDirectoryTree(const uint8_t* data, size_t size, size_t offset);
    bool ReadEntry(const uint8_t* data, size_t size, size_t& offset,
                   const std::string& extension, const std::string& directory, const std::string& fileName);

    std::vector<uint8_t> m_buffer;
    std::string m_filePath;

    uint32_t m_version = 0;
    uint32_t m_headerSize = 0;
    uint32_t m_treeSize = 0;
    uint32_t m_fileDataSize = 0;

    std::vector<VpkEntry> m_entries;

    mutable std::map<uint16_t, std::unique_ptr<std::ifstream>> m_archiveStreams;
    std::string m_dirPrefix;

    std::vector<uint8_t> ReadFromArchive(uint16_t archiveIndex, uint32_t offset, uint32_t length) const;
};
