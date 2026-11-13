// Copyright (C) 2026 iQuit (https://github.com/iQuitt)
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

// One file in a GTA III / Vice City / San Andreas IMG archive
struct ImgEntry
{
    std::string name;
    uint64_t offset = 0;  // bytes
    uint64_t size = 0;    // bytes, whole 2048 byte sectors
};

// GTA IMG archive: VER2 (San Andreas) or a version 1 .img with its .dir (GTA III, Vice City)
class ImgFile
{
public:
    static constexpr uint64_t kSector = 2048;

    bool Load(const std::string& path);
    const std::vector<ImgEntry>& GetEntries() const { return m_entries; }
    bool Read(const ImgEntry& entry, std::vector<uint8_t>& data) const;
    const std::string& GetError() const { return m_error; }

private:
    mutable std::ifstream m_file;
    uint64_t m_fileSize = 0;
    std::vector<ImgEntry> m_entries;
    std::string m_error;
};
