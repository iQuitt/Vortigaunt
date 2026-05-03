#include "XfsExtractor.h"
#include "core/VortigauntLog.h"

#include <fstream>
#include "utils/FileIO.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <numeric>
#include <set>
#include <cstring>
#include <cstdarg>
#include <cstdio>
#include "utils/fsutils.hpp"
#include "utils/util.hpp"

#include "zlib.h"


XfsExtractor::ProgressFunc XfsExtractor::s_progressFunc = nullptr;

void XfsExtractor::Initialize()
{
    s_progressFunc = nullptr;
}

void XfsExtractor::Shutdown()
{
    s_progressFunc = nullptr;
}

#ifdef QT_CORE_LIB
bool XfsExtractor::Extract(const QString& inputFile, const QString& outputDir)
{
    if (!Load(inputFile.toStdString()))
    {
        return false;
    }
    return ExtractAll(outputDir.toStdString());
}
#endif

void XfsExtractor::SetProgressFunc(const ProgressFunc& func)
{
    s_progressFunc = func;
}


void XfsExtractor::ReportProgress(int percent)
{
    if (s_progressFunc)
    {
        s_progressFunc(percent);
    }
}



// ------------------------------------------------------------------------
//  Helper: sanitize a single path component for Windows filesystem
// ------------------------------------------------------------------------
static std::string SanitizePathComponent(const std::string& name)
{
    std::string out;
    out.reserve(name.size());

    for (unsigned char ch : name)
    {
        if (ch < 0x20)
        {
            out.push_back('_');
            continue;
        }
        switch (ch)
        {
        case '<': case '>': case ':': case '\"':
        case '/': case '\\': case '|': case '?': case '*':
            out.push_back('_');
            break;
        default:
            out.push_back(static_cast<char>(ch));
            break;
        }
    }

    while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
        out.pop_back();

    if (out.empty())
        out = "_";

    return out;
}


// FileInfoBlock structure (0x80 = 128 bytes per entry)
static const size_t FILE_INFO_BLOCK_SIZE = 0x80;


bool XfsExtractor::DecompressChunked(const uint8_t* source, size_t sourceLen,
                                      std::vector<uint8_t>& dest, size_t expectedSize) const
{
    
    dest.clear();
    dest.reserve(expectedSize);

    size_t srcPos = 0;
    
    std::vector<uint8_t> chunkOut;
    chunkOut.reserve(0x10000); // 64KB default chunk size
    
    while (srcPos < sourceLen && dest.size() < expectedSize)
    {
        if (srcPos + 3 > sourceLen)
            break;

        // Read 24-bit chunk size with FLAGS
        uint32_t chunkSizeRaw = static_cast<uint32_t>(source[srcPos])
                              | (static_cast<uint32_t>(source[srcPos + 1]) << 8)
                              | (static_cast<uint32_t>(source[srcPos + 2]) << 16);
        srcPos += 3;
        
        // Extract FLAGS (top 2 bits) and CHUNK_SIZE (bottom 22 bits)
        uint32_t flags = chunkSizeRaw >> 22;
        uint32_t chunkSize = chunkSizeRaw & 0x3FFFFF;
        
        uint32_t chunkZSize = 0;

        
        if (flags & 2)
        {
            if (!(flags & 1))
            {
                // Read compressed size
                if (srcPos + 3 > sourceLen) break;
                chunkZSize = static_cast<uint32_t>(source[srcPos])
                           | (static_cast<uint32_t>(source[srcPos + 1]) << 8)
                           | (static_cast<uint32_t>(source[srcPos + 2]) << 16);
                srcPos += 3;
            }
        }
        
        if (flags == 0)
        {
            // Special case: CHUNK_ZSIZE = CHUNK_SIZE, CHUNK_SIZE = 0x10000
            chunkZSize = chunkSize;
            chunkSize = 0x10000;
        }
        
        // Skip 2 bytes (DUMMY)
        if (srcPos + 2 > sourceLen) break;
        srcPos += 2;
        
        if (flags & 1)
        {
            // Raw data 
            if (srcPos + chunkSize > sourceLen) break;
            dest.insert(dest.end(), source + srcPos, source + srcPos + chunkSize);
            srcPos += chunkSize;
        }
        else
        {
            // Compressed data
            if (chunkZSize == 0) chunkZSize = chunkSize;
            if (srcPos + chunkZSize > sourceLen) break;
            
            chunkOut.resize(chunkSize > 0 ? chunkSize : 0x10000);
            uLongf destLen = static_cast<uLongf>(chunkOut.size());
            
            int err = uncompress(chunkOut.data(), &destLen, source + srcPos, static_cast<uLong>(chunkZSize));
            if (err != Z_OK)
            {
                // Try with inflateInit for streaming
                z_stream stream;
                stream.zalloc = Z_NULL;
                stream.zfree = Z_NULL;
                stream.opaque = Z_NULL;
                stream.next_in = const_cast<Bytef*>(source + srcPos);
                stream.avail_in = static_cast<uInt>(chunkZSize);
                stream.next_out = chunkOut.data();
                stream.avail_out = static_cast<uInt>(chunkOut.size());

                err = inflateInit(&stream);
                if (err == Z_OK)
                {
                    err = inflate(&stream, Z_FINISH);
                    destLen = stream.total_out;
                    inflateEnd(&stream);
                }
                
                if (err != Z_STREAM_END && err != Z_OK)
                {
                    // Try raw inflate (no zlib header)
                    stream.zalloc = Z_NULL;
                    stream.zfree = Z_NULL;
                    stream.opaque = Z_NULL;
                    stream.next_in = const_cast<Bytef*>(source + srcPos);
                    stream.avail_in = static_cast<uInt>(chunkZSize);
                    stream.next_out = chunkOut.data();
                    stream.avail_out = static_cast<uInt>(chunkOut.size());

                    err = inflateInit2(&stream, -MAX_WBITS);
                    if (err == Z_OK)
                    {
                        err = inflate(&stream, Z_FINISH);
                        destLen = stream.total_out;
                        inflateEnd(&stream);
                    }
                    
                    if (err != Z_STREAM_END && err != Z_OK && destLen == 0)
                    {
                        dest.insert(dest.end(), source + srcPos, source + srcPos + chunkZSize);
                        srcPos += chunkZSize;
                        continue;
                    }
                }
            }

            dest.insert(dest.end(), chunkOut.begin(), chunkOut.begin() + destLen);
            srcPos += chunkZSize;
        }
    }

    return !dest.empty() || expectedSize == 0;
}


bool XfsExtractor::Load(const std::string& xfsFilePath)
{
    m_entries.clear();
    m_xfsFilePath = xfsFilePath;

    std::ifstream in(FileIO::toPath(xfsFilePath), std::ios::binary | std::ios::ate);
    if (!in)
    {
        VortigauntLog::LogF("^1[XFS] Error:^7 cannot open file: ^4%s^7\n", xfsFilePath.c_str());
        return false;
    }

    m_fileSize = static_cast<std::uint64_t>(in.tellg());
    in.seekg(0, std::ios::beg);

    if (!ParseFileInfoTable(in))
    {
        VortigauntLog::LogF("^1[XFS] Error:^7 failed to parse file info table: ^4%s^7\n", xfsFilePath.c_str());
        return false;
    }

    VortigauntLog::LogF("DEBUG: Parsed ^3%zu^7 entries from ^4%s^7\n", m_entries.size(), xfsFilePath.c_str());
    return !m_entries.empty();
}

bool XfsExtractor::ParseFileInfoTable(std::ifstream& in)
{
    

    // Read tail offset (first 4 bytes)
    uint32_t tailOffset = 0;
    in.seekg(0, std::ios::beg);
    in.read(reinterpret_cast<char*>(&tailOffset), 4);
    if (!in)
    {
        VortigauntLog::LogF("^1[XFS]^7 Failed to read tail offset\n");
        return false;
    }


    if (tailOffset >= m_fileSize)
    {
        VortigauntLog::LogF("^1[XFS]^7 Invalid tail offset\n");
        return false;
    }

    in.seekg(tailOffset, std::ios::beg);
    if (!in)
    {
        VortigauntLog::LogF("^1[XFS]^7 Failed to seek to tail\n");
        return false;
    }

    uint8_t headerZSize = 0;
    in.read(reinterpret_cast<char*>(&headerZSize), 1);
    if (!in)
    {
        VortigauntLog::LogF("^1[XFS]^7 Failed to read header ZSIZE\n");
        return false;
    }


    std::streampos headerDataOffset = in.tellg();
    std::vector<uint8_t> compressedHeader(headerZSize);
    in.read(reinterpret_cast<char*>(compressedHeader.data()), headerZSize);
    if (!in)
    {
        VortigauntLog::LogF("^1[XFS]^7 Failed to read compressed header\n");
        return false;
    }

    std::vector<uint8_t> header(0x100); // More than enough
    uLongf headerDestLen = static_cast<uLongf>(header.size());
    
    int err = uncompress(header.data(), &headerDestLen, compressedHeader.data(), static_cast<uLong>(headerZSize));
    if (err != Z_OK)
    {
        VortigauntLog::LogF("^1[XFS]^7 Failed to decompress header: %d\n", err);
        return false;
    }
    header.resize(headerDestLen);


    // XFS header: "XFS" + version(1) + dummy(4) + ENTRIES(4) + zero(4) + XFS_OFFSET(4)
    // i just got it from quickbms xenesis.bms script
    if (header.size() < 20)
    {
        VortigauntLog::LogF("^1[XFS]^7 Header too small\n");
        return false;
    }

    // Check XFS signature
    if (header[0] != 'X' || header[1] != 'F' || header[2] != 'S')
    {
        VortigauntLog::LogF("^1[XFS]^7 Invalid signature: %c%c%c\n", header[0], header[1], header[2]);
        return false;
    }

    uint8_t xfsVersion = header[3];
    uint32_t dummy = *reinterpret_cast<uint32_t*>(&header[4]);
    uint32_t entries = *reinterpret_cast<uint32_t*>(&header[8]);
    uint32_t zero = *reinterpret_cast<uint32_t*>(&header[12]);
    uint32_t xfsOffset = *reinterpret_cast<uint32_t*>(&header[16]);


    if (entries == 0 || entries > 1000000)
    {
        VortigauntLog::LogF("^1[XFS]^7 Invalid entry count\n");
        return false;
    }

    uint64_t infoOffset = static_cast<uint64_t>(headerDataOffset) + headerZSize;
    in.seekg(static_cast<std::streamoff>(infoOffset), std::ios::beg);

    uint32_t infoSize = 0;
    uint32_t infoZSize = 0;

    if (xfsVersion == ' ')
    {
        uint8_t sizeBytes[3];
        in.read(reinterpret_cast<char*>(sizeBytes), 3);
        infoSize = static_cast<uint32_t>(sizeBytes[0])
                 | (static_cast<uint32_t>(sizeBytes[1]) << 8)
                 | (static_cast<uint32_t>(sizeBytes[2]) << 16);
        
        in.read(reinterpret_cast<char*>(sizeBytes), 3);
        infoZSize = static_cast<uint32_t>(sizeBytes[0])
                  | (static_cast<uint32_t>(sizeBytes[1]) << 8)
                  | (static_cast<uint32_t>(sizeBytes[2]) << 16);
    }
    else // Version '2' or others
    {
        uint8_t sizeBytes[3];
        in.read(reinterpret_cast<char*>(sizeBytes), 3);
        infoZSize = static_cast<uint32_t>(sizeBytes[0])
                  | (static_cast<uint32_t>(sizeBytes[1]) << 8)
                  | (static_cast<uint32_t>(sizeBytes[2]) << 16);
        infoSize = entries * FILE_INFO_BLOCK_SIZE;
    }


    if (infoZSize == 0 || infoZSize > 50 * 1024 * 1024)
    {
        VortigauntLog::LogF("^1[XFS]^7 Invalid info table compressed size\n");
        return false;
    }

    // Read compressed file table
    std::streampos tableOffset = in.tellg();
    std::vector<uint8_t> compressedTable(infoZSize);
    in.read(reinterpret_cast<char*>(compressedTable.data()), infoZSize);
    if (!in)
    {
        VortigauntLog::LogF("^1[XFS]^7 Failed to read compressed file table\n");
        return false;
    }

    // Decompress file table
    std::vector<uint8_t> fileTable(infoSize > 0 ? infoSize : entries * FILE_INFO_BLOCK_SIZE);
    uLongf tableDestLen = static_cast<uLongf>(fileTable.size());
    
    err = uncompress(fileTable.data(), &tableDestLen, compressedTable.data(), static_cast<uLong>(infoZSize));
    if (err != Z_OK)
    {
        // Try inflate
        z_stream stream;
        stream.zalloc = Z_NULL;
        stream.zfree = Z_NULL;
        stream.opaque = Z_NULL;
        stream.next_in = compressedTable.data();
        stream.avail_in = static_cast<uInt>(infoZSize);
        stream.next_out = fileTable.data();
        stream.avail_out = static_cast<uInt>(fileTable.size());

        err = inflateInit(&stream);
        if (err == Z_OK)
        {
            err = inflate(&stream, Z_FINISH);
            tableDestLen = stream.total_out;
            inflateEnd(&stream);
        }
        
        if (err != Z_STREAM_END && err != Z_OK && tableDestLen == 0)
        {
            VortigauntLog::LogF("^1[XFS]^7 Failed to decompress file table: %d\n", err);
            return false;
        }
    }
    fileTable.resize(tableDestLen);
    

    // Parse entries based on version
    m_entries.reserve(entries);

    if (xfsVersion == '2')
    {
        // Version 2: each entry is 0x80 bytes
        // Structure: NAME[0x70] + OFFSET(4) + DUMMY(4) + SIZE(4) + ZSIZE(4)
        size_t pos = 0;
        for (uint32_t i = 0; i < entries && pos + FILE_INFO_BLOCK_SIZE <= fileTable.size(); ++i)
        {


            const uint8_t* block = fileTable.data() + pos;
            
            XfsEntry entry;
            
            size_t nameLen = 0;
            while (nameLen < 0x70 && block[nameLen] != 0)
                nameLen++;
            entry.filename = std::string(reinterpret_cast<const char*>(block), nameLen);
            
            // Offsets at 0x70
            entry.offset = *reinterpret_cast<const uint32_t*>(&block[0x70]);
            // uint32_t dummy = *reinterpret_cast<const uint32_t*>(&block[0x74]); // skip
            entry.unpackedSize = *reinterpret_cast<const uint32_t*>(&block[0x78]);
            entry.packedSize = *reinterpret_cast<const uint32_t*>(&block[0x7C]);
            
            if (!entry.filename.empty() && entry.offset < tailOffset)
            {
                m_entries.push_back(entry);
            }
            
            pos += FILE_INFO_BLOCK_SIZE;
        }
    }
    else
    {
        // Version ' ' (space): recursive directory structure
        // Based on xenesis.bms script:
        // ENTRIES(4) + NAMESZ(1) + NAME(NAMESZ) + OFFSET(4) + OFFSET48(2) + SIZE(4) + ZSIZE(4) + IS_FILE(1)
        
        struct ParseContext {
            const uint8_t* data;
            size_t dataSize;
            size_t pos;
            uint32_t tailOffset;
            std::vector<XfsEntry>& entries;
            std::string currentPath;
            
            bool parseNode() {
                if (pos + 4 > dataSize) return false;
                
                // Read number of child entries
                uint32_t childEntries = *reinterpret_cast<const uint32_t*>(data + pos);
                pos += 4;
                
                if (pos + 1 > dataSize) return false;
                
                // Read name size
                uint8_t nameSz = data[pos];
                pos += 1;
                
                if (pos + nameSz > dataSize) return false;
                
                std::string name(reinterpret_cast<const char*>(data + pos), nameSz);
                pos += nameSz;
                
                if (pos + 15 > dataSize) return false; // Need at least 15 more bytes
                
                uint32_t offset32 = *reinterpret_cast<const uint32_t*>(data + pos);
                pos += 4;

                uint16_t offset48 = *reinterpret_cast<const uint16_t*>(data + pos);
                pos += 2;
                uint64_t offset = offset32 | (static_cast<uint64_t>(offset48) << 32);
                
                uint32_t size = *reinterpret_cast<const uint32_t*>(data + pos);
                pos += 4;
                
                uint32_t zsize = *reinterpret_cast<const uint32_t*>(data + pos);
                pos += 4;
                
                uint8_t isFile = data[pos];
                pos += 1;
                
                // Build path
                std::string fullPath = currentPath;
                if (!fullPath.empty() && !fullPath.empty() && fullPath.back() != '/')
                    fullPath += "/";
                fullPath += name;
                
                // If it's a file, add entry
                if (isFile != 0)
                {
                    XfsEntry entry;
                    entry.filename = fullPath;
                    entry.offset = static_cast<uint32_t>(offset); // Truncate for now
                    entry.unpackedSize = size;
                    entry.packedSize = zsize;
                    
                    if (!entry.filename.empty() && entry.offset < tailOffset)
                    {
                        entries.push_back(entry);
                    }
                }
                
                // Recurse into children
                std::string savedPath = currentPath;
                currentPath = fullPath;
                
                for (uint32_t i = 0; i < childEntries; ++i)
                {
                    if (!parseNode())
                        break;
                }
                
                currentPath = savedPath;
                return true;
            }
        };
        
        ParseContext ctx{fileTable.data(), fileTable.size(), 0, tailOffset, m_entries, ""};
        ctx.parseNode();
    }

    
    // Debug: show first few entries
    for (size_t i = 0; i < std::min<size_t>(5, m_entries.size()); ++i)
    {
        const auto& e = m_entries[i];
        VortigauntLog::LogF("DEBUG: Entry %zu: ^4%s^7 (offset=%u, size=%u, zsize=%u)\n", 
             i, e.filename.c_str(), e.offset, e.unpackedSize, e.packedSize);
    }
    
    return !m_entries.empty();
}

bool XfsExtractor::ExtractEntry(const XfsEntry& entry, const std::string& outputPath) const
{
    if (m_xfsFilePath.empty())
        return false;

    // Create parent directory
    std::filesystem::path outPath = FileIO::toPath(outputPath);
    std::error_code ec;
    std::filesystem::create_directories(outPath.parent_path(), ec);
    if (ec)
    {
        VortigauntLog::LogF("^1[XFS]^7 Failed to create parent directory: %s\n", ec.message().c_str());
        return false;
    }

    std::ifstream in(FileIO::toPath(m_xfsFilePath), std::ios::binary);
    if (!in)
    {
        VortigauntLog::LogF("^1[XFS]^7 Failed to open archive for extraction\n");
        return false;
    }

    // Use larger buffer for I/O
    const size_t BUFFER_SIZE = 1024 * 1024;
    std::vector<char> fileBuffer(BUFFER_SIZE);
    in.rdbuf()->pubsetbuf(fileBuffer.data(), static_cast<std::streamsize>(BUFFER_SIZE));

    in.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
    if (!in)
    {
        VortigauntLog::LogF("^1[XFS]^7 Failed to seek to entry offset\n");
        return false;
    }

    // Read compressed data
    std::vector<uint8_t> compressedData(entry.packedSize);
    in.read(reinterpret_cast<char*>(compressedData.data()), entry.packedSize);
    if (in.gcount() != static_cast<std::streamsize>(entry.packedSize))
    {
        VortigauntLog::LogF("^1[XFS]^7 Failed to read compressed data\n");
        return false;
    }

    // Check if data needs decompression or is raw
    std::vector<uint8_t> outputData;

    if (entry.unpackedSize == entry.packedSize)
    {
        // Data is not compressed
        outputData = std::move(compressedData);
    }
    else
    {
        // Check initial size marker in data
        if (compressedData.size() >= 3)
        {
            uint32_t initSize = static_cast<uint32_t>(compressedData[0])
                              | (static_cast<uint32_t>(compressedData[1]) << 8);
            
            // Original code: if (outputSize == initSize) pCurrFile += 3;
            // This suggests sometimes there's a 2-3 byte prefix to skip
            size_t skipBytes = 0;
            if (initSize == entry.unpackedSize || (initSize & 0xFFFF) == (entry.unpackedSize & 0xFFFF))
            {
                skipBytes = 3;
            }

            bool decompressSuccess = DecompressChunked(compressedData.data() + skipBytes, 
                                   compressedData.size() - skipBytes,
                                   outputData, entry.unpackedSize);
            
            if (!decompressSuccess && skipBytes > 0)
            {
                // Try without skip
                decompressSuccess = DecompressChunked(compressedData.data(), compressedData.size(),
                                       outputData, entry.unpackedSize);
            }
            
            if (!decompressSuccess)
            {
                // Try simple uncompress
                outputData.resize(entry.unpackedSize);
                uLongf destLen = entry.unpackedSize;
                int err = uncompress(outputData.data(), &destLen, 
                                   compressedData.data(), static_cast<uLong>(compressedData.size()));
                if (err == Z_OK)
                {
                    outputData.resize(destLen);
                    decompressSuccess = true;
                }
            }
            
            if (!decompressSuccess)
            {
                // Last resort: save raw data as-is
                outputData = std::move(compressedData);
            }
        }
        else
        {
            if (!DecompressChunked(compressedData.data(), compressedData.size(),
                                   outputData, entry.unpackedSize))
            {
                // Fallback: try simple uncompress
                outputData.resize(entry.unpackedSize);
                uLongf destLen = entry.unpackedSize;
                if (uncompress(outputData.data(), &destLen, 
                              compressedData.data(), static_cast<uLong>(compressedData.size())) == Z_OK)
                {
                    outputData.resize(destLen);
                }
                else
                {
                    // Last resort: save raw data
                    outputData = std::move(compressedData);
                }
            }
        }
    }

    // Write output file
    std::ofstream out(FileIO::toPath(outputPath), std::ios::binary);
    if (!out)
    {
        VortigauntLog::LogF("^1[XFS]^7 Failed to create output file: ^4%s^7\n", outputPath.c_str());
        return false;
    }

    out.write(reinterpret_cast<const char*>(outputData.data()), outputData.size());
    return out.good();
}

bool XfsExtractor::ExtractEntryToMemory(const XfsEntry& entry, std::vector<uint8_t>& outData) const
{
    outData.clear();

    if (m_xfsFilePath.empty())
        return false;

    std::ifstream in(FileIO::toPath(m_xfsFilePath), std::ios::binary);
    if (!in)
        return false;

    in.seekg(static_cast<std::streamoff>(entry.offset), std::ios::beg);
    if (!in)
        return false;

    std::vector<uint8_t> compressedData(entry.packedSize);
    in.read(reinterpret_cast<char*>(compressedData.data()), entry.packedSize);
    if (in.gcount() != static_cast<std::streamsize>(entry.packedSize))
        return false;

    if (entry.unpackedSize == entry.packedSize)
    {
        outData = std::move(compressedData);
        return true;
    }

    // Try decompression
    size_t skipBytes = 0;
    if (compressedData.size() >= 3)
    {
        uint32_t initSize = static_cast<uint32_t>(compressedData[0])
                          | (static_cast<uint32_t>(compressedData[1]) << 8);
        if ((initSize & 0xFFFF) == (entry.unpackedSize & 0xFFFF))
        {
            skipBytes = 3;
        }
    }

    if (!DecompressChunked(compressedData.data() + skipBytes,
                           compressedData.size() - skipBytes,
                           outData, entry.unpackedSize))
    {
        // Fallback
        if (skipBytes > 0)
        {
            DecompressChunked(compressedData.data(), compressedData.size(),
                              outData, entry.unpackedSize);
        }
    }

    return !outData.empty();
}

bool XfsExtractor::ExtractAll(const std::string& outputDir) const
{
    if (m_xfsFilePath.empty() || m_entries.empty())
    {
        VortigauntLog::LogF("^3[XFS]^7 Nothing to extract.\n");
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(FileIO::toPath(outputDir), ec);
    if (ec)
    {
         VortigauntLog::LogF("^1[XFS]^7 Failed to create output directory: %s\n", ec.message().c_str());
         return false;
    }

    VortigauntLog::LogF("\n^5======================================================================^7\n");
    VortigauntLog::LogF("^5XFS Archive Extractor^7\n");
    VortigauntLog::LogF("^5======================================================================^7\n");
    VortigauntLog::LogF("  ^5File:^7 ^4%s^7\n", m_xfsFilePath.c_str());
    VortigauntLog::LogF("  ^5Output:^7 ^4%s^7\n", outputDir.c_str());
    VortigauntLog::LogF("^5======================================================================^7\n\n");

    // Sort entries by offset for sequential I/O
    std::vector<size_t> indices(m_entries.size());
    std::iota(indices.begin(), indices.end(), 0);
    std::sort(indices.begin(), indices.end(), [this](size_t a, size_t b) {
        return m_entries[a].offset < m_entries[b].offset;
    });

    // Pre-create ALL directories at once (batch operation)
    std::set<std::filesystem::path> dirs;
    for (size_t idx : indices)
    {
        const XfsEntry& e = m_entries[idx];
        std::filesystem::path outPath = FileIO::toPath(outputDir) / FileIO::toPath(e.filename);
        if (outPath.has_parent_path())
        {
            dirs.insert(outPath.parent_path());
        }
    }
    for (const auto& dir : dirs)
    {
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);
    }

    // Open single file handle with large buffer for all extractions
    std::ifstream in(FileIO::toPath(m_xfsFilePath), std::ios::binary);
    if (!in)
    {
    VortigauntLog::LogF("^1[XFS]^7 Failed to open archive\n");
        return false;
    }
    
    // Use large I/O buffer (4MB)
    constexpr size_t IO_BUFFER_SIZE = 4 * 1024 * 1024;
    std::vector<char> ioBuffer(IO_BUFFER_SIZE);
    in.rdbuf()->pubsetbuf(ioBuffer.data(), static_cast<std::streamsize>(IO_BUFFER_SIZE));

    // Pre-allocate reusable buffers
    std::vector<uint8_t> compressedData;
    std::vector<uint8_t> outputData;
    compressedData.reserve(1 * 1024 * 1024); // 1MB initial
    outputData.reserve(2 * 1024 * 1024);     // 2MB initial

    size_t successCount = 0;
    size_t failCount = 0;
    const size_t total = indices.size();

    for (size_t i = 0; i < indices.size(); ++i)
    {
        const XfsEntry& e = m_entries[indices[i]];
        
        std::filesystem::path outPath = FileIO::toPath(outputDir) / FileIO::toPath(e.filename);
        
        // Inline extraction for speed (avoid function call overhead)
        bool success = false;
        
        in.seekg(static_cast<std::streamoff>(e.offset), std::ios::beg);
        if (in)
        {
            compressedData.resize(e.packedSize);
            in.read(reinterpret_cast<char*>(compressedData.data()), e.packedSize);
            
            if (in.gcount() == static_cast<std::streamsize>(e.packedSize))
            {
                if (e.unpackedSize == e.packedSize)
                {
                    // Uncompressed - direct write
                    outputData = std::move(compressedData);
                    compressedData.clear();
                    success = true;
                }
                else
                {
                    // Decompress
                    if (DecompressChunked(compressedData.data(), compressedData.size(),
                                         outputData, e.unpackedSize))
                    {
                        success = true;
                    }
                    else
                    {
                        // Fallback: try simple uncompress
                        outputData.resize(e.unpackedSize);
                        uLongf destLen = static_cast<uLongf>(e.unpackedSize);
                        if (uncompress(outputData.data(), &destLen, 
                                      compressedData.data(), static_cast<uLong>(compressedData.size())) == Z_OK)
                        {
                            outputData.resize(destLen);
                            success = true;
                        }
                        else
                        {
                            // Last resort: save as-is
                            outputData = std::move(compressedData);
                            compressedData.clear();
                            success = true;
                        }
                    }
                }
                
                if (success && !outputData.empty())
                {
                    std::ofstream out(outPath, std::ios::binary);
                    if (out)
                    {
                        out.write(reinterpret_cast<const char*>(outputData.data()), outputData.size());
                        success = out.good();
                    }
                    else
                    {
                        success = false;
                    }
                }
            }
        }
        
        if (success)
            ++successCount;
        else
            ++failCount;

        // Progress update every 200 files or on milestones
        if (i % 200 == 0 || i == total - 1 || (i > 0 && (i * 100 / total) != ((i - 1) * 100 / total)))
        {
            int percent = static_cast<int>((i + 1) * 100 / total);
            VortigauntLog::LogF("  ^7[%zu/%zu] %s ^3(%s)^7\n", 
                 i + 1, total, e.filename.c_str(), FormatSize(e.unpackedSize).c_str());
            ReportProgress(percent);
        }
    }

    VortigauntLog::LogF("\n^2XFS Extraction complete:^7 ^3%zu/%zu^7 files written to %s\n", successCount, m_entries.size(), outputDir.c_str());

	if (failCount > 0)
    {
        VortigauntLog::LogF("^1%zu files failed to extract. Please check the output above for details.^7\n",failCount);
    }
    return successCount > 0;
}

bool XfsExtractor::ExtractSelectedEntries(const std::vector<size_t>& indices, const std::string& outputDir) const
{
    if (m_xfsFilePath.empty() || m_entries.empty() || indices.empty())
    {
        VortigauntLog::LogF("^3[XFS]^7 Nothing to extract.\n");
        return false;
    }

    std::filesystem::create_directories(FileIO::toPath(outputDir));

    // Sort by offset
    std::vector<size_t> sortedIndices = indices;
    std::sort(sortedIndices.begin(), sortedIndices.end(), [this](size_t a, size_t b) {
        if (a >= m_entries.size() || b >= m_entries.size())
            return a < b;
        return m_entries[a].offset < m_entries[b].offset;
    });

    // Pre-create directories
    std::set<std::filesystem::path> dirs;
    for (size_t idx : sortedIndices)
    {
        if (idx >= m_entries.size()) continue;
        const XfsEntry& e = m_entries[idx];
        std::filesystem::path outPath = FileIO::toPath(outputDir) / FileIO::toPath(e.filename);
        if (!outPath.parent_path().empty())
        {
            dirs.insert(outPath.parent_path());
        }
    }
    for (const auto& dir : dirs)
    {
        std::filesystem::create_directories(dir);
    }

    size_t successCount = 0;
    const size_t total = sortedIndices.size();

    for (size_t i = 0; i < sortedIndices.size(); ++i)
    {
        size_t idx = sortedIndices[i];
        if (idx >= m_entries.size()) continue;

        const XfsEntry& e = m_entries[idx];
        std::filesystem::path outPath = FileIO::toPath(outputDir) / FileIO::toPath(e.filename);

        if (ExtractEntry(e, String_UTF16toUTF8(outPath.generic_u16string())))
        {
            ++successCount;
        }

        if (i % 50 == 0 || i == total - 1)
        {
            VortigauntLog::LogF("  [%zu/%zu] %s\n", i + 1, total, e.filename.c_str());
            int percent = static_cast<int>((i + 1) * 100 / total);
            ReportProgress(percent);
        }
    }

    VortigauntLog::LogF("\n[XFS] Extraction complete: %zu/%zu files\n", successCount, total);
    return successCount > 0;
}

std::uint64_t XfsExtractor::GetTotalUnpackedSize() const
{
    std::uint64_t total = 0;
    for (const auto& e : m_entries)
    {
        total += e.unpackedSize;
    }
    return total;
}

std::uint64_t XfsExtractor::GetTotalPackedSize() const
{
    std::uint64_t total = 0;
    for (const auto& e : m_entries)
    {
        total += e.packedSize;
    }
    return total;
}
