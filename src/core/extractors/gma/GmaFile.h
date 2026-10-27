// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include <istream>

// GMA (Garry's Mod Addon) archive.
// Format reference: https://github.com/Facepunch/gmad
//
// Layout:
//   ident        char[4]  "GMAD"
//   version      char     (currently 3)
//   steamid      uint64
//   timestamp    uint64
//   [version > 1] required content strings, null-terminated, until empty string
//   name         string (null-terminated)
//   description  string (null-terminated, usually JSON)
//   author       string (null-terminated)
//   addonversion int32
//   file index:  { fileNumber uint32 (0 = end), name string, size int64, crc uint32 }...
//   file block:  raw file contents concatenated in index order

struct GmaEntry
{
    std::string fullPath;   // lowercase relative path as stored by gmad
    uint64_t size = 0;
    uint32_t crc32 = 0;
    uint64_t offset = 0;    // relative to the file block
};

class GmaFile
{
public:
    GmaFile() = default;
    ~GmaFile() { Clear(); }
    // a decompressed addon lives in a temp file this object deletes
    GmaFile(const GmaFile&) = delete;
    GmaFile& operator=(const GmaFile&) = delete;

    bool Load(const std::string& filePath);

    const std::vector<GmaEntry>& GetEntries() const { return m_entries; }
    size_t GetEntryCount() const { return m_entries.size(); }

    std::vector<uint8_t> ExtractEntry(const GmaEntry& entry) const;

    uint8_t GetFormatVersion() const { return m_formatVersion; }
    uint64_t GetSteamId() const { return m_steamId; }
    uint64_t GetTimestamp() const { return m_timestamp; }
    int32_t GetAddonVersion() const { return m_addonVersion; }
    const std::string& GetName() const { return m_name; }
    const std::string& GetDescription() const { return m_description; }
    const std::string& GetAuthor() const { return m_author; }

    const std::string& GetPath() const { return m_filePath; }
    std::string GetBaseName() const;

    // gmad stores relative lowercase paths; reject anything that could
    // escape the output directory (absolute paths, "..", drive letters).
    static bool IsSafeEntryPath(const std::string& path);

    void Clear();

private:
    bool Parse(std::istream& in);

    std::string m_filePath;

    // entries are read from here, opened for each read: the .gma itself, or for an
    // LZMA-compressed addon its decompressed copy in the temp folder
    std::string m_dataPath;
    bool m_ownsDataFile = false;

    uint8_t m_formatVersion = 0;
    uint64_t m_steamId = 0;
    uint64_t m_timestamp = 0;
    int32_t m_addonVersion = 0;
    std::string m_name;
    std::string m_description;
    std::string m_author;

    uint64_t m_fileBlockOffset = 0;
    std::vector<GmaEntry> m_entries;
};
