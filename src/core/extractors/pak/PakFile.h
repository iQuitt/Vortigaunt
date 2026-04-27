#ifndef __PAKFILE_H_
#define __PAKFILE_H_

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#pragma pack(push, 1)
struct PakHeader
{
    uint32_t Checksum;
    uint8_t PakVersion;
    uint32_t NumEntries;
    uint8_t _pad[3];
};
#pragma pack(pop)

static_assert(sizeof(PakHeader) == 0xC);

enum PakEntryTypes
{
    PAK_TYPE_COMPRESSED = 0x1,
    PAK_TYPE_ENCRYPTED = 0x2,
    PAK_TYPE_ENCRYPTED_AGAIN = 0x4,
};

struct PakEntry_t
{
    std::u16string FilePath;
    uint32_t unk;
    PakEntryTypes Type;
    uint32_t FileOffset;
    uint32_t RealSize;
    uint32_t PackedSize;
    std::array<uint32_t, 4> BaseKey;
};

class PakFile
{
public:
    PakFile(std::vector<uint8_t>&& buffer, std::u16string&& filename);

    bool ParseHeader();
    bool ParseEntries();

    [[nodiscard]] inline std::span<const PakEntry_t> GetEntries() const
    {
        return this->m_Entries;
    }

    [[nodiscard]] std::pair<bool, std::vector<uint8_t>> UnpackEntry(
        const PakEntry_t& entry) const;

    [[nodiscard]] inline const auto& GetHeader() const
    {
        return this->m_Header;
    }

private:
    std::vector<uint8_t> m_Buffer;
    std::vector<uint8_t> m_BackupBuffer;
    std::u16string m_Filename;

    size_t m_HeaderStartOffset;
    size_t m_EntriesStartOffset;
    size_t m_DataStartOffset;

    PakHeader m_Header;
    std::vector<PakEntry_t> m_Entries;
};

#endif  // __PAKFILE_H_

