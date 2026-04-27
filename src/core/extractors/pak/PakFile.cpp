#include "PakFile.h"

#include <algorithm>
#include <iostream>
#include <span>
#include <stdexcept>
#include <string_view>

#include "PakView.hpp"
#include "SnowCipher.h"
#include "utils/util.hpp"

using namespace std::string_literals;
using namespace std::string_view_literals;

// 
constexpr const std::u16string_view PAK_KEY = u"CqeLFV@*0IfewH("sv;
constexpr const uint8_t PAK_VERSION = 2;
constexpr const uint32_t PAK_ENTRY_MAX_PATH_LEN = 16384;
constexpr const size_t PAK_TYPE_TOP_BYTES = 0x400;

bool IsPakHeaderValid(const PakHeader& header)
{
    return header.PakVersion == PAK_VERSION &&
           (header.PakVersion + header.NumEntries == header.Checksum);
}

inline PakHeader GetPakHeader(const std::span<uint8_t> pkgData,
                              const uint8_t* key)
{
    PakHeader header;

    SnowCipher snow;
    snow.SetKey(key);

    snow.DecryptBuffer(&header, pkgData.data(), sizeof(header));

    return header;
}

inline auto GenerateHeaderKey(std::u16string_view filename)
{
    std::array<uint8_t, 128> res;

    std::u16string fullKey(filename.data(), filename.length());
    fullKey += PAK_KEY;

    const auto keyPtr = fullKey.c_str();
    const auto keySize = fullKey.size();

    for (size_t i = 0; i < res.max_size(); i++)
    {
        res[i] = i + keyPtr[i % keySize];
    }

    return res;
}

inline auto GenerateEntriesKey(std::u16string_view filename)
{
    std::array<uint8_t, 128> res;

    std::u16string fullKey(filename.data(), filename.length());
    fullKey += PAK_KEY;

    const auto keyLen = fullKey.length();

    for (size_t i = 0; i < res.max_size(); i++)
    {
        res[i] = i + (i -
                      3 * (((0x55555556LL * i) >> 32) +
                           (((0x55555556LL * i) >> 32) >> 31)) +
                      2) *
                         fullKey[keyLen - i % keyLen - 1];
    }

    return res;
}

inline auto GenerateDataKey(std::u16string_view filename,
                            std::span<const uint32_t> baseKey)
{
    std::array<uint8_t, 128> res;

    const auto filenamePtr = filename.data();
    const auto filenameLen = filename.size();

    const auto baseKeyPtr = reinterpret_cast<const uint8_t*>(baseKey.data());

    for (size_t i = 0; i < res.max_size(); i++)
    {
        res[i] = i + filenamePtr[(i % filenameLen)] *
                         (i + baseKeyPtr[i % 16] -
                          5 * ((((0x66666667LL * i) >> 32) >> 1) +
                               (((0x66666667LL * i) >> 32) >> 31)) +
                          2);
    }

    return res;
}

template <typename CHAR_TYPE>
inline size_t GetSpecialSumOfChars(std::basic_string_view<CHAR_TYPE> str)
{
    size_t res = 0;

    for (const auto character : str)
    {
        res += character + character * 2;
    }

    return res;
}

PakFile::PakFile(std::vector<uint8_t>&& buffer, std::u16string&& filename)
    : m_Buffer(buffer), m_Filename(filename), m_HeaderStartOffset(0),
      m_EntriesStartOffset(0)
{
    this->m_BackupBuffer = this->m_Buffer;
}

bool PakFile::ParseHeader()
{
    auto generatedKey = GenerateHeaderKey(this->m_Filename);

    size_t sumOfChars = GetSumOfChars<char16_t>(this->m_Filename);
    this->m_HeaderStartOffset = sumOfChars % 312 + 30;

    this->m_Header =
        GetPakHeader({ this->m_Buffer.data() + this->m_HeaderStartOffset,
                       this->m_Buffer.size() - this->m_HeaderStartOffset },
                     generatedKey.data());

    std::cout << "csopak header: offset=" << m_HeaderStartOffset
              << " checksum=" << m_Header.Checksum
              << " version=" << (int)m_Header.PakVersion
              << " numEntries=" << m_Header.NumEntries << std::endl;

    return IsPakHeaderValid(this->m_Header);
}

bool PakFile::ParseEntries()
{
    auto entriesKey = GenerateEntriesKey(this->m_Filename);
    this->m_EntriesStartOffset =
        this->m_HeaderStartOffset + 42 +
        (GetSpecialSumOfChars<char16_t>(this->m_Filename) % 212);

    std::cout << "[CSO] ParseEntries: headerOffset=" << m_HeaderStartOffset
              << " entriesOffset=" << m_EntriesStartOffset
              << " numEntries=" << m_Header.NumEntries << std::endl;

    PakView<SnowCipher, 4> view(
        { this->m_Buffer.data() + this->m_EntriesStartOffset,
          this->m_Buffer.size() - this->m_EntriesStartOffset },
        entriesKey);

    this->m_Entries.reserve(this->m_Header.NumEntries);

    for (size_t i = 0; i < this->m_Header.NumEntries; i++)
    {
        size_t offsetBefore = view.GetCurOffset();
        std::cout << "[CSO] entry " << i
                  << " curOffset(before)=" << offsetBefore << std::endl;

        auto strPathLen = view.Read<uint32_t>();
        size_t offsetAfterPathLen = view.GetCurOffset();
        std::cout << "[CSO] entry " << i
                  << " pathLen=" << strPathLen 
                  << " (read " << (offsetAfterPathLen - offsetBefore) << " bytes)" << std::endl;

        if (strPathLen > PAK_ENTRY_MAX_PATH_LEN)
        {
            std::cout << "[CSO] entry " << i
                      << " pathLen too big, aborting ParseEntries() but keeping already parsed entries."
                      << std::endl;
            // Let's not completely fail, just abort parsing further entries for this PAK
            break;
        }

        size_t offsetBeforePath = view.GetCurOffset();
        auto filePath = view.ReadString<std::u16string>(strPathLen);
        size_t offsetAfterPath = view.GetCurOffset();
        std::cout << "[CSO] entry " << i
                  << " path=" << String_UTF16toUTF8(filePath) 
                  << " (strLen=" << strPathLen 
                  << " bytes=" << (strPathLen * 2)
                  << " aligned=" << GetAlignedLength<4>(strPathLen * 2)
                  << " read=" << (offsetAfterPath - offsetBeforePath) << ")" << std::endl;

        size_t offsetBeforeRest = view.GetCurOffset();
        auto unk = view.Read<uint32_t>();
        auto type = view.Read<uint32_t>();
        auto offset = view.Read<uint32_t>();
        auto realSize = view.Read<uint32_t>();
        auto paddedSize = view.Read<uint32_t>();
        auto baseKey = view.ReadArray<uint32_t, 4>();
        size_t offsetAfterRest = view.GetCurOffset();
        std::cout << "[CSO] entry " << i 
                  << " rest fields read=" << (offsetAfterRest - offsetBeforeRest)
                  << " total=" << (offsetAfterRest - offsetBefore) << std::endl;

        PakEntry_t newEntry = { std::move(filePath),
                                unk,
                                static_cast<PakEntryTypes>(type),
                                offset,
                                realSize,
                                paddedSize,
                                std::move(baseKey) };

        this->m_Entries.push_back(std::move(newEntry));
    }

    this->m_DataStartOffset = this->m_EntriesStartOffset + view.GetCurOffset();

    if (this->m_DataStartOffset & 0x3FF)
    {
        this->m_DataStartOffset =
            this->m_DataStartOffset - (this->m_DataStartOffset & 0x3FF) + 0x400;
    }

    return true;
}

[[nodiscard]] std::pair<bool, std::vector<uint8_t>> PakFile::UnpackEntry(
    const PakEntry_t& entry) const
{

    std::vector<uint8_t> res(entry.RealSize);

    auto dataKey = GenerateDataKey(entry.FilePath, entry.BaseKey);
    auto startOffset = this->m_DataStartOffset + (entry.FileOffset << 10);

    // Bounds check: ensure we can read from the backup buffer
    if (startOffset >= this->m_BackupBuffer.size())
    {
        return { false, {} };
    }

    // Always copy raw data first — without this, unencrypted entries
    // would return an all-zero buffer since no decryption branch fills it.
    {
        const auto availableBytes = this->m_BackupBuffer.size() - startOffset;
        const auto copyLen = std::min<size_t>(entry.RealSize, availableBytes);
        std::copy(this->m_BackupBuffer.data() + startOffset,
                  this->m_BackupBuffer.data() + startOffset + copyLen,
                  res.begin());
    }

    if (entry.Type & PAK_TYPE_ENCRYPTED_AGAIN)
    {
        const auto availableBytes = this->m_BackupBuffer.size() - startOffset;
        
        PakView<SnowCipher, 4> view(
            { this->m_BackupBuffer.data() + startOffset, availableBytes },
            dataKey);

        auto buff = view.ReadBytes(entry.RealSize, true);
        auto buffPtr = buff.get();

        // Copy only up to res.size() to avoid iterator past end
        const auto copyLen = std::min<size_t>(entry.RealSize, res.size());
        std::copy(buffPtr, buffPtr + copyLen, res.begin());
    }

    if (entry.Type & PAK_TYPE_ENCRYPTED)
    {
        const auto maxTopBytes = std::min<size_t>(res.size(), PAK_TYPE_TOP_BYTES);

        PakView<SnowCipher, 4> view({ res.data(), maxTopBytes }, dataKey);

        auto buff = view.ReadBytes(maxTopBytes);
        auto buffPtr = buff.get();

        // Copy only up to res.size() to avoid iterator past end
        const auto copyLen = std::min<size_t>(maxTopBytes, res.size());
        std::copy(buffPtr, buffPtr + copyLen, res.begin());
    }

    if (entry.Type & PAK_TYPE_COMPRESSED)
    {
        throw std::runtime_error("Implement me");
    }

    return { true, std::move(res) };
}
