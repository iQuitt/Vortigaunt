#include "GmaFile.h"
#include "core/VortigauntLog.h"
#include "utils/FileIO.h"

#include <fstream>
#include <filesystem>
#include <streambuf>
#include <cstring>
#include <limits>

#include "Precomp.h"
#include "LzmaDec.h"
#include "Alloc.h"

namespace
{
    constexpr char GMA_IDENT[4] = { 'G', 'M', 'A', 'D' };
    constexpr uint8_t GMA_MAX_VERSION = 3;

    constexpr uint64_t GMA_MAX_LZMA_UNPACK = 0x100000000ull;

    class MemoryStreamBuf : public std::streambuf
    {
    public:
        MemoryStreamBuf(const uint8_t* data, size_t size)
        {
            char* p = const_cast<char*>(reinterpret_cast<const char*>(data));
            setg(p, p, p + size);
        }

    protected:
        pos_type seekoff(off_type off, std::ios_base::seekdir dir, std::ios_base::openmode) override
        {
            char* base = eback();
            char* end = egptr();
            char* target = nullptr;

            if (dir == std::ios_base::beg)      target = base + off;
            else if (dir == std::ios_base::cur) target = gptr() + off;
            else                                target = end + off;

            if (target < base || target > end)
                return pos_type(off_type(-1));

            setg(base, target, end);
            return pos_type(target - base);
        }

        pos_type seekpos(pos_type pos, std::ios_base::openmode mode) override
        {
            return seekoff(off_type(pos), std::ios_base::beg, mode);
        }
    };

    class MemoryIStream : public std::istream
    {
    public:
        MemoryIStream(const uint8_t* data, size_t size)
            : std::istream(nullptr), m_buf(data, size)
        {
            rdbuf(&m_buf);
        }

    private:
        MemoryStreamBuf m_buf;
    };

    template <typename T>
    bool readValue(std::istream& in, T& out)
    {
        in.read(reinterpret_cast<char*>(&out), sizeof(T));
        return in.good();
    }

    bool readString(std::istream& in, std::string& out)
    {
        out.clear();
        char c;
        while (in.get(c))
        {
            if (c == '\0')
                return true;
            out += c;
            if (out.size() > 1024 * 1024) // corrupted file guard
                return false;
        }
        return false;
    }

    bool decompressLzma(const std::vector<uint8_t>& input, std::vector<uint8_t>& output)
    {
        // .lzma layout: 5 props bytes + 8 bytes uncompressed size + data
        constexpr size_t headerSize = LZMA_PROPS_SIZE + 8;
        if (input.size() <= headerSize)
            return false;

        uint64_t unpackSize = 0;
        for (int i = 0; i < 8; ++i)
            unpackSize |= static_cast<uint64_t>(input[LZMA_PROPS_SIZE + i]) << (i * 8);

        // Unknown size (all 0xFF) or implausible size -> not a supported LZMA GMA
        if (unpackSize == 0 || unpackSize == std::numeric_limits<uint64_t>::max() ||
            unpackSize > GMA_MAX_LZMA_UNPACK)
            return false;

        output.resize(static_cast<size_t>(unpackSize));

        SizeT destLen = static_cast<SizeT>(unpackSize);
        SizeT srcLen = static_cast<SizeT>(input.size() - headerSize);
        ELzmaStatus status;

        SRes res = LzmaDecode(output.data(), &destLen,
                              input.data() + headerSize, &srcLen,
                              input.data(), LZMA_PROPS_SIZE,
                              LZMA_FINISH_ANY, &status, &g_Alloc);

        if (res != SZ_OK)
            return false;

        output.resize(static_cast<size_t>(destLen));
        return true;
    }
}

bool GmaFile::Load(const std::string& filePath)
{
    Clear();
    m_filePath = filePath;

    auto stream = std::make_unique<std::ifstream>(FileIO::toPath(filePath), std::ios::binary);
    if (!stream->is_open())
    {
        VortigauntLog::LogF("GMA: Cannot open file: %s", filePath.c_str());
        return false;
    }

    char ident[4] = {};
    stream->read(ident, 4);
    if (!stream->good())
    {
        VortigauntLog::LogF("GMA: File too small: %s", filePath.c_str());
        return false;
    }

    if (std::memcmp(ident, GMA_IDENT, 4) == 0)
    {
        stream->seekg(0, std::ios::beg);
        m_stream = std::move(stream);
        if (!Parse(*m_stream))
        {
            Clear();
            return false;
        }
        return true;
    }

    // Not plain GMAD — workshop downloads are often LZMA-compressed whole-file.
    stream->seekg(0, std::ios::end);
    const auto fileSize = static_cast<size_t>(stream->tellg());
    stream->seekg(0, std::ios::beg);

    std::vector<uint8_t> compressed(fileSize);
    stream->read(reinterpret_cast<char*>(compressed.data()), static_cast<std::streamsize>(fileSize));
    if (!stream->good())
    {
        VortigauntLog::LogF("GMA: Failed to read file: %s", filePath.c_str());
        return false;
    }
    stream.reset();

    if (!decompressLzma(compressed, m_memory) ||
        m_memory.size() < 4 || std::memcmp(m_memory.data(), GMA_IDENT, 4) != 0)
    {
        VortigauntLog::LogF("GMA: Not a valid GMA file (bad ident, not LZMA-compressed either): %s", filePath.c_str());
        Clear();
        return false;
    }

    VortigauntLog::LogF("GMA: LZMA-compressed addon detected, decompressed %zu bytes.", m_memory.size());

    m_stream = std::make_unique<MemoryIStream>(m_memory.data(), m_memory.size());
    if (!Parse(*m_stream))
    {
        Clear();
        return false;
    }
    return true;
}

bool GmaFile::Parse(std::istream& in)
{
    char ident[4] = {};
    in.read(ident, 4);
    if (!in.good() || std::memcmp(ident, GMA_IDENT, 4) != 0)
    {
        VortigauntLog::LogF("GMA: Invalid ident.");
        return false;
    }

    char version = 0;
    if (!readValue(in, version))
        return false;
    m_formatVersion = static_cast<uint8_t>(version);

    if (m_formatVersion > GMA_MAX_VERSION)
    {
        VortigauntLog::LogF("GMA: Unsupported format version %d.", m_formatVersion);
        return false;
    }

    if (!readValue(in, m_steamId) || !readValue(in, m_timestamp))
        return false;

    if (m_formatVersion > 1)
    {
        // Required content strings — read until empty string
        std::string content;
        do
        {
            if (!readString(in, content))
                return false;
        } while (!content.empty());
    }

    if (!readString(in, m_name) ||
        !readString(in, m_description) ||
        !readString(in, m_author))
        return false;

    if (!readValue(in, m_addonVersion))
        return false;

    // File index
    uint64_t offset = 0;
    for (;;)
    {
        uint32_t fileNumber = 0;
        if (!readValue(in, fileNumber))
        {
            VortigauntLog::LogF("GMA: Unexpected end of file while reading index.");
            return false;
        }

        if (fileNumber == 0)
            break;

        GmaEntry entry;
        int64_t size = 0;
        if (!readString(in, entry.fullPath) ||
            !readValue(in, size) ||
            !readValue(in, entry.crc32))
        {
            VortigauntLog::LogF("GMA: Corrupted file index.");
            return false;
        }

        if (size < 0)
        {
            VortigauntLog::LogF("GMA: Invalid entry size for %s.", entry.fullPath.c_str());
            return false;
        }

        entry.size = static_cast<uint64_t>(size);
        entry.offset = offset;
        offset += entry.size;

        m_entries.push_back(std::move(entry));
    }

    m_fileBlockOffset = static_cast<uint64_t>(in.tellg());
    return true;
}

std::vector<uint8_t> GmaFile::ExtractEntry(const GmaEntry& entry) const
{
    std::vector<uint8_t> data;

    if (!m_stream || entry.size == 0)
        return data;

    m_stream->clear();
    m_stream->seekg(static_cast<std::streamoff>(m_fileBlockOffset + entry.offset), std::ios::beg);
    if (!m_stream->good())
        return data;

    data.resize(static_cast<size_t>(entry.size));
    m_stream->read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(entry.size));
    if (!m_stream->good())
    {
        data.clear();
        return data;
    }

    return data;
}

bool GmaFile::IsSafeEntryPath(const std::string& path)
{
    if (path.empty())
        return false;
    if (path[0] == '/' || path[0] == '\\')
        return false;
    if (path.find(':') != std::string::npos)
        return false;

    // Reject any ".." path component
    std::string component;
    for (size_t i = 0; i <= path.size(); ++i)
    {
        if (i == path.size() || path[i] == '/' || path[i] == '\\')
        {
            if (component == "..")
                return false;
            component.clear();
        }
        else
        {
            component += path[i];
        }
    }

    return true;
}

std::string GmaFile::GetBaseName() const
{
    return std::filesystem::path(FileIO::toPath(m_filePath)).stem().string();
}

void GmaFile::Clear()
{
    m_stream.reset();
    m_memory.clear();
    m_memory.shrink_to_fit();
    m_formatVersion = 0;
    m_steamId = 0;
    m_timestamp = 0;
    m_addonVersion = 0;
    m_name.clear();
    m_description.clear();
    m_author.clear();
    m_fileBlockOffset = 0;
    m_entries.clear();
}
