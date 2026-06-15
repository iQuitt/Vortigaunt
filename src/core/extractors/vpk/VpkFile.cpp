#include "VpkFile.h"
#include "utils/util.hpp"
#include "core/VortigauntLog.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <sstream>

namespace fs = std::filesystem;

bool VpkFile::Load(const std::string& filePath)
{
    Clear();

    m_filePath = filePath;

    std::ifstream is(filePath, std::ios::binary | std::ios::ate);
    if (!is)
    {
        VortigauntLog::LogF("VpkFile: Failed to open '%s'", filePath.c_str());
        return false;
    }

    std::streamsize size = is.tellg();
    if (size <= 0)
    {
        VortigauntLog::LogF("VpkFile: Empty file '%s'", filePath.c_str());
        return false;
    }

    is.seekg(0, std::ios::beg);
    m_buffer.resize(static_cast<size_t>(size));
    if (!is.read(reinterpret_cast<char*>(m_buffer.data()), size))
    {
        VortigauntLog::LogF("VpkFile: Failed to read '%s'", filePath.c_str());
        return false;
    }

    return ParseHeader();
}

bool VpkFile::LoadFromBuffer(std::vector<uint8_t> buffer, const std::string& filePath)
{
    Clear();
    m_buffer = std::move(buffer);
    m_filePath = filePath;
    return ParseHeader();
}

void VpkFile::Clear()
{
    m_buffer.clear();
    m_entries.clear();
    m_archiveStreams.clear();
    m_filePath.clear();
    m_version = 0;
    m_headerSize = 0;
    m_treeSize = 0;
    m_fileDataSize = 0;
    m_dirPrefix.clear();
}

std::string VpkFile::GetBaseName() const
{
    fs::path p(m_filePath);
    std::string name = p.filename().string();

    // Strip _dir and _NNN suffixes
    auto pos = name.find("_dir");
    if (pos != std::string::npos)
    {
        name = name.substr(0, pos);
    }
    else
    {
        // Try to strip _NNN (e.g., pak01_000.vpk -> pak01)
        // For single-file vpks, just remove extension
        pos = name.rfind('.');
        if (pos != std::string::npos)
            name = name.substr(0, pos);
    }
    return name;
}

bool VpkFile::ParseHeader()
{
    if (m_buffer.size() < 4)
        return false;

    const uint8_t* data = m_buffer.data();
    size_t size = m_buffer.size();

    // Check signature
    uint32_t sig = *reinterpret_cast<const uint32_t*>(data);

    if (sig == VPK_SIGNATURE)
    {
        // Has header
        if (size < 12)
            return false;

        const auto* hdr = reinterpret_cast<const VpkHeaderV1*>(data);
        m_version = hdr->version;
        m_treeSize = hdr->treeSize;

        if (m_version == 1)
        {
            m_headerSize = 12;
        }
        else if (m_version == 2)
        {
            if (size < 28)
                return false;
            const auto* hdrV2 = reinterpret_cast<const VpkHeaderV2*>(data);
            m_fileDataSize = hdrV2->fileDataSize;
            m_headerSize = 28;
        }
        else
        {
            VortigauntLog::LogF("VpkFile: Unsupported VPK version %u", m_version);
            return false;
        }
    }
    else
    {
        // Pre-v1 (legacy): no header, tree starts at offset 0
        m_version = 0;
        m_headerSize = 0;
        m_treeSize = static_cast<uint32_t>(size);
    }

    // Determine directory prefix for loading archive chunks
    fs::path fullPath(m_filePath);
    std::string filename = fullPath.filename().string();
    std::string parentDir = fullPath.parent_path().string();

    // Check if this is a _dir.vpk
    auto dirPos = filename.find("_dir.vpk");
    if (dirPos != std::string::npos)
    {
        m_dirPrefix = (fs::path(parentDir) / filename.substr(0, dirPos)).string();
    }
    else
    {
        // Check if this is a _NNN.vpk (data chunk)
        // We can still parse it if the tree starts right away (no header)
        // But typically _NNN files don't have the tree
        auto nnnPos = filename.rfind('_');
        if (nnnPos != std::string::npos)
        {
            std::string after = filename.substr(nnnPos + 1);
            if (after.size() >= 3 && after.find('.') != std::string::npos)
            {
                std::string numPart = after.substr(0, after.find('.'));
                if (numPart.find_first_not_of("0123456789") == std::string::npos)
                {
                    // This is a numbered chunk file, not a directory VPK
                    // The tree should be in the _dir.vpk
                    m_dirPrefix = (fs::path(parentDir) / filename.substr(0, nnnPos)).string();
                    // We can't parse entries from a chunk file
                    VortigauntLog::LogF("VpkFile: '%s' appears to be a data chunk. Load the _dir.vpk instead.", filename.c_str());
                    return false;
                }
            }
        }
        m_dirPrefix = (fs::path(parentDir) / GetBaseName()).string();
    }

    // Parse directory tree
    return ParseDirectoryTree(m_buffer.data(), m_buffer.size(), m_headerSize);
}

bool VpkFile::ParseDirectoryTree(const uint8_t* data, size_t size, size_t offset)
{
    size_t pos = offset;

    // Read extensions in a loop
    while (pos < size)
    {
        std::string extension;
        while (pos < size && data[pos] != 0)
        {
            extension += static_cast<char>(data[pos]);
            pos++;
        }
        pos++; // skip null terminator

        if (extension.empty())
            break;

        // Read directories for this extension
        while (pos < size)
        {
            std::string directory;
            while (pos < size && data[pos] != 0)
            {
                directory += static_cast<char>(data[pos]);
                pos++;
            }
            pos++; // skip null terminator

            if (directory.empty())
                break;

            // Read file names for this directory+extension
            while (pos < size)
            {
                std::string fileName;
                while (pos < size && data[pos] != 0)
                {
                    fileName += static_cast<char>(data[pos]);
                    pos++;
                }
                pos++; // skip null terminator

                if (fileName.empty())
                    break;

                // Read entry struct
                if (!ReadEntry(data, size, pos, extension, directory, fileName))
                    return false;
            }
        }
    }

    VortigauntLog::LogF("VpkFile: Loaded %zu entries from '%s'", m_entries.size(), fs::path(m_filePath).filename().string().c_str());
    return true;
}

bool VpkFile::ReadEntry(const uint8_t* data, size_t size, size_t& offset,
                        const std::string& extension, const std::string& directory, const std::string& fileName)
{
    if (offset + sizeof(VpkDirEntry) > size)
        return false;

    const auto* entry = reinterpret_cast<const VpkDirEntry*>(data + offset);
    offset += sizeof(VpkDirEntry);

    if (entry->terminator != VPK_ENTRY_TERMINATOR)
    {
        VortigauntLog::LogF("VpkFile: Invalid entry terminator at offset %zu", offset - 2);
        return false;
    }

    VpkEntry vpkEntry;
    vpkEntry.crc32 = entry->crc32;
    vpkEntry.preloadBytes = entry->preloadBytes;
    vpkEntry.archiveIndex = entry->archiveIndex;
    vpkEntry.entryOffset = entry->entryOffset;
    vpkEntry.entryLength = entry->entryLength;

    // Construct full path
    std::string fullPath;
    if (!directory.empty() && directory != " ")
    {
        fullPath = directory;
        if (fullPath.back() != '/')
            fullPath += '/';
    }
    fullPath += fileName;
    if (!extension.empty())
    {
        fullPath += '.';
        fullPath += extension;
    }
    vpkEntry.fullPath = fullPath;

    // Read preload data
    if (entry->preloadBytes > 0 && offset + entry->preloadBytes <= size)
    {
        vpkEntry.preloadData.assign(data + offset, data + offset + entry->preloadBytes);
        offset += entry->preloadBytes;
    }

    m_entries.push_back(std::move(vpkEntry));
    return true;
}

std::vector<uint8_t> VpkFile::ExtractEntry(const VpkEntry& entry) const
{
    std::vector<uint8_t> result;

    // Start with preload data (if any)
    if (!entry.preloadData.empty())
    {
        result = entry.preloadData;
    }

    // Read archive data (if any)
    if (entry.entryLength > 0)
    {
        std::vector<uint8_t> archiveData = ReadFromArchive(entry.archiveIndex, entry.entryOffset, entry.entryLength);
        result.insert(result.end(), archiveData.begin(), archiveData.end());
    }

    return result;
}

std::vector<uint8_t> VpkFile::ReadFromArchive(uint16_t archiveIndex, uint32_t offset, uint32_t length) const
{
    if (length == 0)
        return {};

    // If data is in _dir.vpk itself
    // In V1: header + tree + inline data. entryOffset is relative to after tree.
    // In V2: header + tree + inline data (fileDataSize) + footer. Same addressing.
    if (archiveIndex == VPK_DIR_ARCHIVE_INDEX)
    {
        uint32_t absoluteOffset = m_headerSize + m_treeSize + offset;
        if (absoluteOffset + length <= m_buffer.size())
        {
            return std::vector<uint8_t>(m_buffer.begin() + absoluteOffset,
                                        m_buffer.begin() + absoluteOffset + length);
        }
        return {};
    }

    // Data is in numbered chunk file pak01_NNN.vpk
    std::string chunkFilename = m_dirPrefix + "_" +
        (archiveIndex < 10 ? "00" : (archiveIndex < 100 ? "0" : "")) +
        std::to_string(archiveIndex) + ".vpk";

    // Check cache first
    auto it = m_archiveStreams.find(archiveIndex);
    if (it == m_archiveStreams.end())
    {
        auto stream = std::make_unique<std::ifstream>(chunkFilename, std::ios::binary);
        if (!stream || !*stream)
        {
            VortigauntLog::LogF("VpkFile: Failed to open archive chunk '%s'", chunkFilename.c_str());
            return {};
        }
        it = m_archiveStreams.emplace(archiveIndex, std::move(stream)).first;
    }

    std::ifstream& chunkStream = *it->second;
    chunkStream.seekg(offset, std::ios::beg);
    if (!chunkStream)
    {
        // Might be at end, try seeking to beginning
        chunkStream.clear();
        chunkStream.seekg(offset, std::ios::beg);
        if (!chunkStream)
            return {};
    }

    std::vector<uint8_t> data(length);
    if (!chunkStream.read(reinterpret_cast<char*>(data.data()), length))
    {
        // Read as much as we can
        size_t bytesRead = static_cast<size_t>(chunkStream.gcount());
        data.resize(bytesRead);
    }

    return data;
}
