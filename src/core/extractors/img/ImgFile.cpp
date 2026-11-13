// Copyright (C) 2026 iQuit (https://github.com/iQuitt)
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "ImgFile.h"

#include "utils/FileIO.h"

#include <algorithm>
#include <cstring>
#include <filesystem>

namespace
{

// one directory record, the same in .dir files and after the VER2 header
#pragma pack(push, 1)
struct DirRecord
{
    uint32_t offset;  // sectors
    uint16_t streamingSize;  // sectors; version 1 keeps a 32 bit size across both fields
    uint16_t archiveSize;
    char name[24];
};
#pragma pack(pop)
static_assert(sizeof(DirRecord) == 32);

std::string recordName(const DirRecord& record)
{
    return std::string(record.name, strnlen(record.name, sizeof(record.name)));
}

} // namespace

bool ImgFile::Load(const std::string& path)
{
    m_entries.clear();
    m_file.open(FileIO::toPath(path), std::ios::binary);
    if (!m_file)
    {
        m_error = "cannot open " + path;
        return false;
    }
    m_file.seekg(0, std::ios::end);
    m_fileSize = static_cast<uint64_t>(m_file.tellg());
    m_file.seekg(0);

    char magic[4] = {};
    m_file.read(magic, 4);
    std::vector<DirRecord> records;
    bool version2 = m_file && std::memcmp(magic, "VER2", 4) == 0;
    if (version2)
    {
        uint32_t count = 0;
        m_file.read(reinterpret_cast<char*>(&count), 4);
        if (!m_file || uint64_t(count) * sizeof(DirRecord) + 8 > m_fileSize)
        {
            m_error = "broken VER2 header";
            return false;
        }
        records.resize(count);
        m_file.read(reinterpret_cast<char*>(records.data()), std::streamsize(count * sizeof(DirRecord)));
    }
    else if (m_file && std::memcmp(magic, "VER3", 4) == 0)
    {
        m_error = "GTA IV IMG archives are not supported";
        return false;
    }
    else
    {
        std::filesystem::path dir = FileIO::toPath(path);
        dir.replace_extension(".dir");
        std::ifstream dirFile(dir, std::ios::binary);
        if (!dirFile)
        {
            m_error = "not a VER2 archive and no .dir file next to it";
            return false;
        }
        DirRecord record;
        while (dirFile.read(reinterpret_cast<char*>(&record), sizeof(record)))
            records.push_back(record);
    }
    if (!version2)
        m_file.clear();
    else if (!m_file)
    {
        m_error = "the directory is cut short";
        return false;
    }

    for (const DirRecord& record : records)
    {
        uint64_t sectors = version2 ? (record.archiveSize ? record.archiveSize : record.streamingSize)
                                    : uint64_t(record.streamingSize) | uint64_t(record.archiveSize) << 16;
        ImgEntry entry{ recordName(record), uint64_t(record.offset) * kSector, sectors * kSector };
        if (entry.name.empty() || entry.offset >= m_fileSize)
            continue;
        entry.size = std::min(entry.size, m_fileSize - entry.offset);
        m_entries.push_back(std::move(entry));
    }
    return true;
}

bool ImgFile::Read(const ImgEntry& entry, std::vector<uint8_t>& data) const
{
    data.resize(entry.size);
    m_file.clear();
    m_file.seekg(std::streamoff(entry.offset));
    m_file.read(reinterpret_cast<char*>(data.data()), std::streamsize(entry.size));
    return bool(m_file);
}
