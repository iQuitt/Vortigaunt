#include "UnityFsExtractor.h"
#include "core/VortigauntLog.h"
#include "utils/FileIO.h"
#include "utils/BinaryUtils.h"
#include "lz4.h"
#include "LzmaDec.h"
#include "Alloc.h"

#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cstring>



// Static members
UnityFsExtractor::ProgressFunc UnityFsExtractor::s_progressFunc = nullptr;
UnityFsExtractor::FileProgressFunc UnityFsExtractor::s_fileProgressFunc = nullptr;

void UnityFsExtractor::Initialize()
{
    s_progressFunc = nullptr;
    s_fileProgressFunc = nullptr;
}

void UnityFsExtractor::Shutdown()
{
    s_progressFunc = nullptr;
    s_fileProgressFunc = nullptr;
}

#ifdef QT_CORE_LIB
bool UnityFsExtractor::Extract(const QString& inputFile, const QString& outputDir)
{
    if (!Load(inputFile.toStdString()))
    {
        return false;
    }
    return ExtractAll(outputDir.toStdString());
}
#endif

void UnityFsExtractor::SetProgressFunc(const ProgressFunc& func)
{
    s_progressFunc = func;
}

void UnityFsExtractor::SetFileProgressFunc(const FileProgressFunc& func)
{
    s_fileProgressFunc = func;
}

void UnityFsExtractor::ReportProgress(int percent)
{
    if (s_progressFunc)
    {
        s_progressFunc(percent);
    }
}

void UnityFsExtractor::ReportFileProgress(size_t current, size_t total)
{
    if (s_fileProgressFunc)
    {
        s_fileProgressFunc(current, total);
    }
}

static bool DecompressLZ4Block(const char* src, size_t srcSize, char* dst, size_t dstSize)
{
    int ret = LZ4_decompress_safe(src, dst, static_cast<int>(srcSize), static_cast<int>(dstSize));
    return ret >= 0;
}

static bool DecompressLZMABlock(const char* src, size_t srcSize, char* dst, size_t dstSize)
{
    if (srcSize < 5)
        return false;

    const Byte* props = reinterpret_cast<const Byte*>(src);
    const Byte* srcData = props + 5;
    SizeT srcLen = srcSize - 5;
    SizeT destLen = dstSize;

    ELzmaStatus status = LZMA_STATUS_NOT_SPECIFIED;
    SRes res = LzmaDecode(
        reinterpret_cast<Byte*>(dst), &destLen,
        srcData, &srcLen,
        props, 5,
        LZMA_FINISH_ANY, &status, &g_Alloc
    );
    return res == SZ_OK;
}

static bool DecompressBlock(const char* src, size_t srcSize, char* dst, size_t dstSize, std::uint32_t compressionType)
{
    switch (compressionType)
    {
        case 0: // Uncompressed
            std::memcpy(dst, src, std::min(srcSize, dstSize));
            return true;
        case 1: // LZMA
            return DecompressLZMABlock(src, srcSize, dst, dstSize);
        case 2: // LZ4
        case 3: // LZ4HC
            return DecompressLZ4Block(src, srcSize, dst, dstSize);
        default:
            return false;
    }
}

bool UnityFsExtractor::Load(const std::string& bundleFilePath)
{
    m_entries.clear();
    m_blocks.clear();
    m_bundleFilePath = bundleFilePath;

    std::ifstream in(FileIO::toPath(bundleFilePath), std::ios::binary);
    if (!in)
    {
        VortigauntLog::LogF("Error: cannot open UnityFS file: ^4%s^7\n", bundleFilePath.c_str());
        return false;
    }

    std::string signature = Utils::ReadNullTerminatedString(in, 1000);
    if (signature != "UnityFS")
    {
        VortigauntLog::LogF("Error: not a valid UnityFS archive: ^4%s^7 (signature: %s)\n", bundleFilePath.c_str(), signature.c_str());
        return false;
    }

    std::uint8_t temp[4];
    in.read(reinterpret_cast<char*>(temp), 4);
    std::uint32_t formatVersion = Utils::ReadBE32(temp);

    std::string unityVer = Utils::ReadNullTerminatedString(in, 1000);
    std::string unityRev = Utils::ReadNullTerminatedString(in, 1000);

    if (formatVersion < 6)
    {
        VortigauntLog::LogF("Error: Unsupported UnityFS format version %u in ^4%s^7\n", formatVersion, bundleFilePath.c_str());
        return false;
    }

    std::uint8_t sizeHeader[20];
    in.read(reinterpret_cast<char*>(sizeHeader), 20);
    std::uint64_t bundleSize = Utils::ReadBE64(sizeHeader);
    std::uint32_t compressedMetadataSize = Utils::ReadBE32(sizeHeader + 8);
    std::uint32_t uncompressedMetadataSize = Utils::ReadBE32(sizeHeader + 12);
    std::uint32_t flags = Utils::ReadBE32(sizeHeader + 16);

    // Metadata is stored immediately after the header if Flags & 0x80 is set (typical for format >= 6)
    // Find absolute position of data blocks
    m_blocksOffset = in.tellg();
    if ((flags & 0x80) == 0)
    {
        // Metadata is at the end of the file
        m_blocksOffset = 0; // Header is first, blocks follow immediately, metadata is at the end
    }
    else
    {
        m_blocksOffset += compressedMetadataSize;
    }

    // Read metadata block
    std::vector<char> compressedMeta(compressedMetadataSize);
    in.read(compressedMeta.data(), compressedMetadataSize);

    std::vector<char> decompressedMeta(uncompressedMetadataSize);
    std::uint32_t metaCompression = flags & 0x3F;

    if (!DecompressBlock(compressedMeta.data(), compressedMetadataSize, decompressedMeta.data(), uncompressedMetadataSize, metaCompression))
    {
        VortigauntLog::LogF("Error: failed to decompress metadata block in ^4%s^7\n", bundleFilePath.c_str());
        return false;
    }

    // Parse decompressed metadata
    const std::uint8_t* p = reinterpret_cast<const std::uint8_t*>(decompressedMeta.data());
    const std::uint8_t* end = p + uncompressedMetadataSize;

    if (p + 16 > end) return false;
    p += 16; // Skip GUID

    if (p + 4 > end) return false;
    std::uint32_t blockCount = Utils::ReadBE32(p);
    p += 4;

    // DoS prevention: each block takes at least 10 bytes in metadata
    if (static_cast<size_t>(blockCount) * 10 > static_cast<size_t>(end - p)) return false;

    m_blocks.reserve(blockCount);
    for (std::uint32_t i = 0; i < blockCount; ++i)
    {
        if (p + 10 > end) return false;
        BlockInfo block;
        block.decompressedSize = Utils::ReadBE32(p);
        block.compressedSize = Utils::ReadBE32(p + 4);
        block.flags = Utils::ReadBE16(p + 8);
        p += 10;
        m_blocks.push_back(block);
    }

    if (p + 4 > end) return false;
    std::uint32_t fileCount = Utils::ReadBE32(p);
    p += 4;

    // DoS prevention: each file entry takes at least 21 bytes (20 bytes fields + at least 1 byte null-terminated filename)
    if (static_cast<size_t>(fileCount) * 21 > static_cast<size_t>(end - p)) return false;

    m_entries.reserve(fileCount);
    for (std::uint32_t i = 0; i < fileCount; ++i)
    {
        if (p + 20 > end) return false;
        UnityEntry entry;
        entry.offset = Utils::ReadBE64(p);
        entry.size = Utils::ReadBE64(p + 8);
        entry.flags = Utils::ReadBE32(p + 16);
        p += 20;

        // Bounded search for null-terminator to prevent out-of-bounds reading
        const std::uint8_t* nullChar = std::find(p, end, '\0');
        if (nullChar == end)
        {
            return false;
        }
        entry.filename = std::string(reinterpret_cast<const char*>(p), nullChar - p);
        p = nullChar + 1;

        m_entries.push_back(entry);
    }

    VortigauntLog::LogF("^5[UnityFS]^7 Loaded ^3%zu^7 files from ^4%s^7 (Unity: %s)\n", m_entries.size(), bundleFilePath.c_str(), unityVer.c_str());
    return true;
}

bool UnityFsExtractor::ExtractAll(const std::string& outputDir) const
{
    std::vector<size_t> indices(m_entries.size());
    for (size_t i = 0; i < m_entries.size(); ++i)
    {
        indices[i] = i;
    }
    return ExtractSelectedEntries(indices, outputDir);
}

bool UnityFsExtractor::ExtractSelectedEntries(const std::vector<size_t>& indices, const std::string& outputDir) const
{
    if (m_bundleFilePath.empty() || m_entries.empty() || indices.empty())
    {
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(FileIO::toPath(outputDir), ec);

    std::ifstream in(FileIO::toPath(m_bundleFilePath), std::ios::binary);
    if (!in)
    {
        return false;
    }

    // 1. Decompress all blocks into a contiguous virtual stream in memory
    std::uint64_t totalDecompressedSize = 0;
    for (const auto& block : m_blocks)
    {
        totalDecompressedSize += block.decompressedSize;
    }

    // If size is unreasonably large, prevent memory crash
    static const std::uint64_t MAX_UNPACK_BUFFER = 1024ull * 1024ull * 1024ull; // 1 GB limit
    if (totalDecompressedSize > MAX_UNPACK_BUFFER)
    {
        VortigauntLog::LogF("^1Error:^7 Uncompressed bundle size too large (%llu MB) for native memory unpack.\n", totalDecompressedSize / 1024 / 1024);
        return false;
    }

    std::vector<char> virtualStream(totalDecompressedSize);
    size_t streamOffset = 0;

    in.seekg(m_blocksOffset, std::ios::beg);

    for (size_t i = 0; i < m_blocks.size(); ++i)
    {
        const auto& block = m_blocks[i];
        std::vector<char> compressedBlock(block.compressedSize);
        in.read(compressedBlock.data(), block.compressedSize);

        std::uint32_t compression = block.flags & 0x3F;
        if (!DecompressBlock(compressedBlock.data(), block.compressedSize, virtualStream.data() + streamOffset, block.decompressedSize, compression))
        {
            VortigauntLog::LogF("^1Error:^7 failed to decompress block %zu in UnityFS.\n", i);
            return false;
        }
        streamOffset += block.decompressedSize;

        ReportProgress(static_cast<int>((i + 1) * 50 / m_blocks.size())); // Up to 50% for block decompression
    }

    // 2. Slice out files and write to output path
    size_t writtenCount = 0;
    for (size_t idx : indices)
    {
        if (idx >= m_entries.size())
            continue;

        const auto& entry = m_entries[idx];
        if (entry.size == 0)
            continue;

        if (entry.offset + entry.size > totalDecompressedSize)
        {
            VortigauntLog::LogF("^1Warning:^7 Entry %s is out of stream range.\n", entry.filename.c_str());
            continue;
        }

        // Build clean target path
        std::filesystem::path cleanName = FileIO::toPath(entry.filename).filename();
        std::filesystem::path outPath = FileIO::toPath(outputDir) / cleanName;

        std::filesystem::create_directories(outPath.parent_path(), ec);

        std::ofstream out(outPath, std::ios::binary);
        if (!out)
        {
            continue;
        }

        out.write(virtualStream.data() + entry.offset, entry.size);
        writtenCount++;

        ReportProgress(50 + static_cast<int>((writtenCount * 50) / indices.size())); // 50% to 100% for file writing
        ReportFileProgress(writtenCount, indices.size());
    }

    VortigauntLog::LogF("^2[UnityFS]^7 Successfully extracted ^3%zu/%zu^7 assets to ^4%s^7\n", writtenCount, indices.size(), outputDir.c_str());
    return writtenCount > 0;
}

bool UnityFsExtractor::ExtractEntryToMemory(const UnityEntry& entry, std::vector<char>& outData) const
{
    outData.clear();
    if (m_bundleFilePath.empty() || entry.size == 0)
        return false;

    std::ifstream in(FileIO::toPath(m_bundleFilePath), std::ios::binary);
    if (!in)
        return false;

    // To extract a single entry, we find which blocks intersect the range [entry.offset, entry.offset + entry.size)
    std::uint64_t fileStart = entry.offset;
    std::uint64_t fileEnd = entry.offset + entry.size;

    std::vector<char> fileBuffer;
    fileBuffer.reserve(entry.size);

    std::uint64_t currentBlockVirtualStart = 0;
    in.seekg(m_blocksOffset, std::ios::beg);

    for (const auto& block : m_blocks)
    {
        std::uint64_t currentBlockVirtualEnd = currentBlockVirtualStart + block.decompressedSize;

        // Check if block intersects file range
        if (fileStart < currentBlockVirtualEnd && fileEnd > currentBlockVirtualStart)
        {
            // Decompress this block
            std::vector<char> compressedBlock(block.compressedSize);
            in.read(compressedBlock.data(), block.compressedSize);

            std::vector<char> decompressedBlock(block.decompressedSize);
            std::uint32_t compression = block.flags & 0x3F;

            if (!DecompressBlock(compressedBlock.data(), block.compressedSize, decompressedBlock.data(), block.decompressedSize, compression))
            {
                return false;
            }

            // Copy intersection part
            std::uint64_t intersectStart = std::max(fileStart, currentBlockVirtualStart);
            std::uint64_t intersectEnd = std::min(fileEnd, currentBlockVirtualEnd);

            size_t srcOffset = intersectStart - currentBlockVirtualStart;
            size_t copySize = intersectEnd - intersectStart;

            size_t origSize = fileBuffer.size();
            fileBuffer.resize(origSize + copySize);
            std::memcpy(fileBuffer.data() + origSize, decompressedBlock.data() + srcOffset, copySize);
        }
        else
        {
            // Just skip this block's compressed data in the stream
            in.seekg(block.compressedSize, std::ios::cur);
        }

        currentBlockVirtualStart = currentBlockVirtualEnd;
    }

    if (fileBuffer.size() == entry.size)
    {
        outData = std::move(fileBuffer);
        return true;
    }
    return false;
}
