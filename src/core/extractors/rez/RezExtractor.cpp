#include "RezExtractor.h"
#include "core/VortigauntLog.h"

#include <fstream>
#include "utils/FileIO.h"
#include <iostream>
#include <filesystem>
#include <algorithm>
#include <numeric>
#include <set>
#include <cstring>
#include <functional>
#include <cctype>
#include <cstdarg>
#include <cstdio>
#include "utils/fsutils.hpp"

#include "LzmaDec.h"
#include "Alloc.h"

// so many thanks to kungfulon for cf REZ https://gist.github.com/kungfulon/dfa49323eb7a55db964f10174e57c19f  

using std::uint32_t;
using std::uint64_t;

// Static member definitions
RezExtractor::ProgressFunc RezExtractor::s_progressFunc = nullptr;
RezExtractor::FileProgressFunc RezExtractor::s_fileProgressFunc = nullptr;

void RezExtractor::Initialize()
{
    s_progressFunc = nullptr;
    s_fileProgressFunc = nullptr;
}

void RezExtractor::Shutdown()
{
    s_progressFunc = nullptr;
    s_fileProgressFunc = nullptr;
}

#ifdef QT_CORE_LIB
bool RezExtractor::Extract(const QString& inputFile, const QString& outputDir)
{
    // For RezExtractor, we need to Load first, then ExtractAll
    if (!Load(inputFile.toStdString()))
    {
        return false;
    }
    return ExtractAll(outputDir.toStdString());
}
#endif

void RezExtractor::SetProgressFunc(const ProgressFunc& func)
{
    s_progressFunc = func;
}


void RezExtractor::ReportProgress(int percent)
{
    if (s_progressFunc)
    {
        s_progressFunc(percent);
    }
}

void RezExtractor::ReportFileProgress(size_t current, size_t total)
{
    if (s_fileProgressFunc)
    {
        s_fileProgressFunc(current, total);
    }
}

static const unsigned char CF_REZ_KEY[] = {
    0xF0, 0xF0, 0x9D, 0x09, 0x0A, 0x66, 0xAD, 0x6A, 0x85, 0x1D,
    0xFD, 0x3F, 0x51, 0x23, 0xE7, 0xF3, 0xB1, 0x0E, 0x78, 0xEC,
    0xD1, 0x50, 0x7B, 0x6B, 0x17, 0x3F, 0x61, 0xC5, 0x79, 0x0C,
    0x57, 0x32, 0x1A, 0xF3, 0xB8, 0x6B, 0x68, 0xDE, 0x2A, 0x5F,
    0x01, 0xBA, 0x98, 0x3A, 0x99, 0xC0, 0x54, 0x02, 0x24, 0xF7,
    0x9B, 0x09, 0x87, 0x23, 0xC4, 0x6F, 0x0E, 0x6C, 0x44, 0xFA,
    0xDB, 0xFB, 0xE8, 0x85, 0xAB, 0xC2, 0x65, 0x3C, 0x0E, 0xC4,
    0x93, 0xF6, 0x6D, 0x0B, 0x8A, 0xD6, 0x11, 0x8D, 0xE3, 0x8F,
    0x71, 0x52, 0x5D, 0x6E, 0xFC, 0xFD, 0x29, 0x82, 0xB0, 0x1D,
    0x13, 0x11, 0xAE, 0x5C, 0xD5, 0xA9, 0x1B, 0xF8, 0xCE, 0xFC,
    0x79, 0x9C, 0x5A, 0xD6, 0xCE, 0xFD, 0x0C, 0x64, 0xCA, 0x60,
    0x16, 0x12, 0x31, 0x5B, 0x08, 0x3A, 0xCF, 0x04, 0x3E, 0xEA,
    0x23, 0xDC, 0x28, 0xFA, 0x20, 0xA5, 0xC0, 0xB8, 0x21, 0x73,
    0x5E, 0x6C, 0x6A, 0x2B, 0x31, 0xE9, 0x6D, 0xBD, 0x9A, 0x73,
    0x11, 0x4C, 0xB1, 0x43, 0x3A, 0x8E, 0x28, 0xCE, 0xDC, 0x9B,
    0xD4, 0x31, 0xCF, 0x77, 0x1D, 0xE4, 0x9F, 0x8A, 0x8B, 0x0A,
    0xB2, 0x4E, 0xC0, 0x8D, 0xDD, 0x74, 0x0B, 0x56, 0xCF, 0xB7,
    0xEE, 0xD5, 0x74, 0xA7, 0xB5, 0x1B, 0xA1, 0xA9, 0x85, 0xCB,
    0x45, 0x68, 0xFF, 0x1F, 0x59, 0xFB, 0xCD, 0x42, 0xDA, 0xFF,
    0x59, 0x37, 0x05, 0xE7, 0xDC, 0x9E, 0x12, 0xBD, 0x1B, 0x87,
    0xBB, 0x97, 0x02, 0x9A, 0xC2, 0x04, 0x66, 0xD3, 0xBE, 0xA7,
    0x2C, 0x11, 0x66, 0x4E, 0x10, 0xBD, 0xA8, 0xB3, 0x54, 0xC2,
    0xC0, 0x39, 0x8D, 0x17, 0x91, 0xDA, 0xE0, 0x21, 0x86, 0x8A,
    0xD3, 0x24, 0x37, 0x4A, 0x10, 0x13, 0x0A, 0x38, 0x45, 0xE2,
    0x26, 0xC6, 0x66, 0xC0, 0xDE, 0x73, 0x9B, 0x53, 0xE2, 0x2D,
    0x0A, 0x57, 0x7E, 0xAC, 0xC9, 0xC4, 0x0C, 0x04, 0x33, 0xD5,
    0xFA, 0x9F, 0xE5, 0x15, 0x8A, 0xFD, 0x95, 0xCF, 0x9A, 0x57,
    0x16, 0x02, 0xB2, 0x81, 0xBE, 0x39, 0x8C, 0x3A, 0x72, 0x6A,
    0x6F, 0x34, 0x8A, 0x2F, 0x84, 0x0E, 0xEE, 0x96, 0x6D, 0x80,
    0x83, 0xBC, 0x6A, 0x02, 0x45, 0x84, 0x3A, 0x1C, 0x49, 0xA0,
    0x01, 0xB7, 0xDA, 0x2C, 0x76, 0x96, 0xFF, 0x1D, 0x8E, 0x49,
    0xA7, 0xCA, 0xF5, 0xD6, 0xB0, 0xBD, 0x7F, 0x51, 0x21, 0x25,
    0xEA, 0xAC, 0xB7, 0x15, 0x16, 0xF6, 0x24, 0xD7, 0x0E, 0x54,
    0x27, 0x96, 0x0D, 0xEC, 0xD4, 0x96, 0xC9, 0x00, 0x33, 0x4D,
    0x43, 0x83, 0x8C, 0x7B, 0x59, 0x5E, 0x96, 0xAF, 0x5F, 0xAC,
    0xC3, 0x4A, 0xF9, 0x23, 0xFC, 0x62, 0x7B, 0xFF, 0xF5, 0xB9,
    0x0C, 0x91, 0x6A, 0x01, 0xCD, 0xC9, 0x87, 0xBB, 0x43, 0xFC,
    0xA4, 0xE7, 0x49, 0x0D, 0xB5, 0xC7, 0xC3, 0x5A, 0x95, 0xF7,
    0x52, 0x91, 0x78, 0x1D, 0x52, 0xC4, 0xBC, 0x63, 0x5A, 0xE4,
    0x6A, 0x11, 0x7B, 0xFF, 0x8D, 0x72, 0x8E, 0x64, 0xB5, 0x53,
    0xB8, 0x07, 0xDD, 0x4E, 0x7F, 0x4D, 0xF4, 0x35, 0x99, 0x96,
    0x4A, 0xC6, 0xC6, 0xB7, 0x20, 0xF6, 0xEB, 0xA9, 0xA1, 0x18,
    0xAF, 0xA7, 0x77, 0x07, 0xE2, 0x0B, 0x49, 0xBA, 0xE1, 0x12,
    0x60, 0x55, 0x41, 0xDD, 0xA8, 0x21, 0x03, 0xE5, 0x5B, 0x8F,
    0x81, 0x1E, 0x8D, 0x8B, 0x6A, 0x11, 0xE0, 0x6F, 0xF9, 0x2F,
    0x96, 0xC1, 0xBA, 0x8E, 0x4D, 0x06, 0x06, 0x62, 0x9A, 0xE8,
    0x92, 0x66, 0xCC, 0xFB, 0x34, 0x7B, 0x11, 0x42, 0x34, 0xBC,
    0x3D, 0xDC, 0x63, 0x3E, 0x7A, 0xF7, 0x2C, 0xD4, 0x19, 0x60,
    0xF5, 0xF3, 0xC5, 0xE1, 0xF9, 0x1D, 0x5F, 0xB4, 0xEF, 0xEF,
    0xBA, 0x4E, 0xB1, 0x35, 0x7B, 0xBD, 0x26, 0x1D, 0x61, 0xD0,
    0xB0, 0xF4, 0x2C, 0x65, 0x64, 0x84, 0x6B, 0xFB, 0x3C, 0x74,
    0x6D, 0xE1, 0x93, 0xD2, 0x98, 0x36, 0x2A, 0x18, 0x5F, 0xFA,
    0xE2, 0xE1, 0x23, 0x7C, 0x8C, 0x93, 0x2E, 0x53, 0xEE, 0x40,
    0x23, 0x2C, 0x56, 0xF3, 0xFB, 0xB3, 0xEC, 0xBC, 0xFA, 0xC7,
    0x06, 0xA6, 0xC0, 0x4B, 0xCC, 0xE8, 0xBB, 0xC1, 0x4C, 0x84,
    0x41, 0x01, 0x67, 0xA2, 0x8F, 0x43, 0xB2, 0xD6, 0xEA, 0xB6,
    0xA4, 0xA0, 0x21, 0xF7, 0x45, 0x5E, 0xBC, 0x8E, 0x9F, 0xF2,
    0x03, 0xCC, 0x3B, 0x5F, 0x35, 0x36, 0xD4, 0x91, 0x18, 0xC3,
    0x9E, 0xA6, 0x36, 0x32, 0x44, 0xE0, 0xFA, 0xB2, 0xF1, 0x91,
    0xEF, 0x1F, 0x9D, 0x39, 0x66, 0x10, 0xDA, 0x18, 0xC2, 0xFE,
    0x66, 0x73, 0x9F, 0xBA, 0xC8, 0xD2, 0x2C, 0x7B, 0x23, 0x6A,
    0xD9, 0xBD, 0x9E, 0x02, 0xB2, 0x35, 0x7E, 0x87, 0x9E, 0x1B,
    0x58, 0x9A, 0xC1, 0x06, 0x70, 0x49, 0x3D, 0x9A, 0xB4, 0x46,
    0x9F, 0x4D, 0x67, 0xCB, 0x2A, 0x82, 0xDC, 0x75, 0x4A, 0x32,
    0x70, 0x50, 0x68, 0x6E, 0x0A, 0x5C, 0x65, 0xF2, 0x5E, 0xC4,
    0xF6, 0x0E, 0x34, 0x04, 0x23, 0x24, 0xF3, 0x4B, 0x30, 0xF3,
    0xB2, 0x4E, 0x26, 0x02, 0x07, 0xC8, 0x3D, 0x54, 0xE5, 0xFB,
    0x6F, 0xB4, 0xB0, 0x5E, 0x71, 0xD8, 0xE1, 0xB9, 0x44, 0x92,
    0x69, 0x02, 0xBB, 0x5C, 0x16, 0x24, 0x16, 0x70, 0x3E, 0xFD,
    0x09, 0xBD, 0xF2, 0xD2, 0x69, 0xE7, 0xEE, 0x74, 0xB3, 0xA1,
    0x92, 0x5A, 0xC0, 0x99, 0x1A, 0xF2, 0xDD, 0x3A, 0x62, 0x5E,
    0x81, 0x7D, 0x66, 0xF0, 0xE9, 0x14, 0xCA, 0x8F, 0xDD, 0x24,
    0xA6, 0x5A, 0xD4, 0xD8, 0xD3, 0xB8, 0xBB, 0x03, 0x03, 0x1D,
    0xA6, 0x19, 0xD1, 0xC6, 0x9E, 0xBA, 0x25, 0xA8, 0xD8, 0x16,
    0x0B, 0xCF, 0x8D, 0x5C, 0x5B, 0x78, 0xB9, 0x88, 0x60, 0x19,
    0xFB, 0xB8, 0xC1, 0xA0, 0xD9, 0x65, 0xF3, 0x24, 0xAF, 0x9F,
    0x6A, 0x4F, 0x72, 0xAC, 0xD2, 0xB3, 0xAC, 0x2F, 0x87, 0x5C,
    0xCB, 0x2B, 0x9A, 0xD0, 0x1C, 0x18, 0x8F, 0xC7, 0xA7, 0x47,
    0x26, 0xD6, 0x32, 0xE5, 0x68, 0x4A, 0xA5, 0xC4, 0x31, 0x7C,
    0x16, 0x44, 0x8C, 0xD8, 0xB0, 0x8C, 0x01, 0xD6, 0xCD, 0x51,
    0x37, 0x2B, 0x62, 0x7B, 0x0F, 0x66, 0x20, 0xD8, 0x88, 0x4B,
    0x6C, 0x23, 0xAB, 0x1C, 0x84, 0xA2, 0xAF, 0x15, 0x01, 0x95,
    0xAC, 0x62, 0x03, 0xBB, 0x0F, 0xC2, 0x3C, 0x29, 0x0F, 0x24,
    0x22, 0xB9, 0x6B, 0x72, 0x86, 0x46, 0xA6, 0xD6, 0xCB, 0x06,
    0x0E, 0xB0, 0x04, 0x2C, 0xBD, 0x7E, 0x35, 0x29, 0xED, 0xFE,
    0xF9, 0xB9, 0xC1, 0xBC, 0xC9, 0x0A, 0xD8, 0x5B, 0x2F, 0x33,
    0xE9, 0xD0, 0x0F, 0x3E, 0x9A, 0xCC, 0x63, 0x0C, 0xE0, 0xA3,
    0x91, 0x4A, 0x25, 0xE1, 0xA9, 0xB3, 0x6B, 0xD2, 0xC6, 0xF2,
    0xBA, 0x41, 0xD5, 0x51, 0x0F, 0xAE, 0xFB, 0x7C, 0x0F, 0x30,
    0xE4, 0x9A, 0xBE, 0x50, 0x36, 0xF9, 0x7A, 0x17, 0x62, 0x8E,
    0x7B, 0x94, 0x23, 0x8C, 0x15, 0x0C, 0xD5, 0x48, 0x02, 0x2B,
    0xFB, 0xB6, 0xEB, 0x5B, 0x22, 0xBE, 0x75, 0x9E, 0x6A, 0x99,
    0x1A, 0x0D, 0xF6, 0x90, 0xFC, 0x57, 0x79, 0x43, 0x01, 0x6F,
    0x2F, 0xCD, 0x74, 0xAB, 0x74, 0xF5, 0x65, 0x9D, 0x43, 0xBB,
    0x13, 0xDE, 0xD5, 0x6D, 0x97, 0x08, 0xA9, 0x9E, 0x11, 0x2E,
    0x2A, 0x29, 0xA0, 0xFD, 0x3F, 0x84, 0x52, 0xDB, 0xFB, 0xB4,
    0x67, 0x30, 0xB3, 0x08, 0x0B, 0x2D, 0xB7, 0xEE, 0xDA, 0x41,
    0xED, 0x1C, 0x6A, 0x7F, 0x98, 0x4F, 0x14, 0x45, 0x75, 0xD4,
    0x42, 0x44, 0x8C, 0x34, 0x86, 0x4F, 0xD9, 0x28, 0xAF, 0x10,
    0x1E, 0x25, 0x22, 0xF7, 0x1A, 0xC0, 0xBE, 0xA0, 0x5D, 0x1E,
    0x7C, 0xE3, 0x0F, 0xBE, 0x17, 0xE4, 0xC5, 0xD5, 0xF9, 0x4D,
    0xD0, 0x7F, 0xA7
};



static std::string SanitizePathComponent(const std::string& name)
{

    std::string out;
    out.reserve(name.size());

    for (unsigned char ch : name)
    {
        if (ch < 0x20) // control chars
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

    // Remove trailing spaces and dots which are not allowed in Windows filenames.
    while (!out.empty() && (out.back() == ' ' || out.back() == '.'))
        out.pop_back();

    if (out.empty())
        out = "_";

    return out;
}


static void REZ_DecryptBuffer(std::vector<unsigned char>& data, uint32_t keyPos)
{
    const size_t keyLen = sizeof(CF_REZ_KEY) / sizeof(CF_REZ_KEY[0]);
    const size_t n = data.size();
    for (size_t i = 0; i < n; ++i)
    {
        const unsigned char k = CF_REZ_KEY[(static_cast<uint64_t>(keyPos) + i) % keyLen];
        const unsigned char b = data[i];
        data[i] = static_cast<unsigned char>((0x49 + (k ^ static_cast<unsigned char>(~b))) & 0xFF);
    }
}


// To avoid crashes on corrupted/huge data, we put a hard cap on the
// uncompressed size.
static bool DecompressLZMA(const std::vector<char>& in, std::vector<char>& out)
{
    if (in.size() < LZMA_PROPS_SIZE + 8)
        return false;

    const Byte* src = reinterpret_cast<const Byte*>(in.data());
    unsigned char props[LZMA_PROPS_SIZE];
    std::memcpy(props, src, LZMA_PROPS_SIZE);

    UInt64 unpackSize = 0;
    for (int i = 0; i < 8; ++i)
        unpackSize |= (UInt64)src[LZMA_PROPS_SIZE + i] << (8 * i);

    // Hard cap for safety – prevents allocating insane buffers on corrupted headers (e.g. 900MB REZ with bad entries).
    // i dont know how much makes sense is this
    static const UInt64 MAX_LZMA_OUTPUT = 512ull * 1024ull * 1024ull; // 512 MB

    if (unpackSize == static_cast<UInt64>(-1) || unpackSize == 0 || unpackSize > MAX_LZMA_OUTPUT)
    {
        // Unknown size. fall back to raw which is so fuckin stupid way.
        return false;
    }

    SizeT destLen = static_cast<SizeT>(unpackSize);
    out.assign(destLen, 0);

    SizeT srcLen = static_cast<SizeT>(in.size() - (LZMA_PROPS_SIZE + 8));
    const Byte* srcData = src + LZMA_PROPS_SIZE + 8;

    ELzmaStatus status = LZMA_STATUS_NOT_SPECIFIED;
    SRes res = LzmaDecode(reinterpret_cast<Byte*>(out.data()), &destLen,
        srcData, &srcLen,
        props, LZMA_PROPS_SIZE,
        LZMA_FINISH_ANY, &status, &g_Alloc);
    if (res != SZ_OK)
    {
        out.clear();
        return false;
    }

    out.resize(destLen);
    return true;
}



bool RezExtractor::Load(const std::string& rezFilePath)
{
    m_entries.clear();
    m_rezFilePath = rezFilePath;

    std::ifstream in(FileIO::toPath(rezFilePath), std::ios::binary);
    if (!in)
    {
        VortigauntLog::LogF("Error: cannot open REZ file: ^4%s^7\n", rezFilePath.c_str());
        return false;
    }

    if (!ParseDirectory(in))
    {
        VortigauntLog::LogF("Error: failed to parse REZ directory table: ^4%s^7\n", rezFilePath.c_str());
        return false;
    }

    VortigauntLog::LogF("^5[REZ]^7 Parsed ^3%zu^7 entries from ^4%s^7\n", m_entries.size(), rezFilePath.c_str());
    return !m_entries.empty();
}

bool RezExtractor::ExtractAll(const std::string& outputDir) const
{
    if (m_rezFilePath.empty() || m_entries.empty())
    {
        VortigauntLog::LogF("^3ERROR: ^7 Nothing to extract (no entries or file not loaded).\n");
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(FileIO::toPath(outputDir), ec);
    if (ec)
    {
        VortigauntLog::LogF("^1Error:^7 failed to create output directory: ^4%s^7 (%s)\n", outputDir.c_str(), ec.message().c_str());
        return false;
    }

    std::ifstream in(FileIO::toPath(m_rezFilePath), std::ios::binary);
    if (!in)
    {
        VortigauntLog::LogF("^1Error:^7 cannot reopen REZ file for extraction: ^4%s^7\n", m_rezFilePath.c_str());
        return false;
    }

    size_t successCount = 0;

    VortigauntLog::LogF("\n======================================================================\n");
    VortigauntLog::LogF("^2 REZ File Extractor ^7\n");
    VortigauntLog::LogF("======================================================================\n");
    VortigauntLog::LogF("  ^3File:^7 ^4%s^7\n", m_rezFilePath.c_str());
    VortigauntLog::LogF("  ^3Output:^7 ^4%s^7\n", outputDir.c_str());
    VortigauntLog::LogF("======================================================================\n\n");

    std::vector<size_t> entryIndices(m_entries.size());
    std::iota(entryIndices.begin(), entryIndices.end(), 0);
    std::sort(entryIndices.begin(), entryIndices.end(), [this](size_t a, size_t b) {
        return m_entries[a].offset < m_entries[b].offset;
    });

    std::set<std::filesystem::path> createdDirs;
    
    struct ProcessedEntry {
        const RezEntry* entry;
        std::filesystem::path safeRelPath;
        std::filesystem::path outPath;
        size_t originalIndex;
    };
    std::vector<ProcessedEntry> processedEntries;
    processedEntries.reserve(m_entries.size());
    
    for (size_t idx : entryIndices)
    {
        const RezEntry& e = m_entries[idx];
        if (e.size == 0)
            continue;

        // Guard against insane sizes

        // TODO: find a better way. i dont feel comfortable with this being 1 GB, but it is what it is for now.
        static const std::uint64_t MAX_ENTRY_SIZE = 1024ull * 1024ull * 1024ull; // 1 GB
        if (e.size > MAX_ENTRY_SIZE)
            continue;

        // Build sanitized path
        std::filesystem::path relPath = FileIO::toPath(e.filename);
        std::filesystem::path safeRelPath;
        for (const auto& part : relPath)
        {
            std::string s = part.string();
            if (s == "." || s == ".." || s.empty())
                continue;
            safeRelPath /= SanitizePathComponent(s);
        }
        if (safeRelPath.empty())
        {
            safeRelPath = SanitizePathComponent(e.filename);
        }

        std::filesystem::path outPath = FileIO::toPath(outputDir) / safeRelPath;
        std::filesystem::path parentDir = outPath.parent_path();
        
        // Track directories to create
        if (!parentDir.empty() && createdDirs.find(parentDir) == createdDirs.end())
        {
            createdDirs.insert(parentDir);
        }

        processedEntries.push_back({&e, safeRelPath, outPath, idx});
    }

    for (const auto& dir : createdDirs)
    {
        std::error_code dirEc;
        std::filesystem::create_directories(dir, dirEc);
    }

    // Progress tracking (report every 5% or every 100 files, whichever comes first)
    const size_t totalEntries = processedEntries.size();
    size_t progressReportInterval = std::max<size_t>(1, totalEntries / 20); // 20 reports total
    if (progressReportInterval < 1) progressReportInterval = 1;
    size_t lastReportedCount = 0;

    // Extract entries in sorted order (sequential reads are faster)
    for (size_t i = 0; i < processedEntries.size(); ++i)
    {
        const ProcessedEntry& pe = processedEntries[i];
        const RezEntry& e = *pe.entry;

        // Sequential read optimization: if we're already past this offset, seek back
        // Otherwise, if we're before it, seek forward
        std::streampos currentPos = in.tellg();
        if (currentPos < 0 || static_cast<uint32_t>(currentPos) != e.offset)
        {
            in.seekg(e.offset, std::ios::beg);
            if (!in)
            {
                continue; // Skip failed seeks silently for speed
            }
        }

        // Guard against insane sizes (already checked in pre-processing, but double-check)
        static const std::uint64_t MAX_ENTRY_SIZE = 1024ull * 1024ull * 1024ull; // 1 GB
        if (e.size > MAX_ENTRY_SIZE)
        {
            continue; // Skip very large files
        }
        
        size_t finalSize = 0;
        bool lzmaOk = false;
        
        // Always use standard extraction with LZMA decompression
        std::vector<char> compressed(static_cast<size_t>(e.size));
        in.read(compressed.data(), static_cast<std::streamsize>(e.size));
        std::streamsize got = in.gcount();
        if (got <= 0)
        {
            continue; // Skip silently for speed
        }
        compressed.resize(static_cast<size_t>(got));

        // Try LZMA decompress first; if it fails, fall back to raw data
        std::vector<char> decompressed;
        lzmaOk = DecompressLZMA(compressed, decompressed);
        const std::vector<char>& toWrite = lzmaOk ? decompressed : compressed;
        finalSize = toWrite.size();

        // Write output file
        std::ofstream out(pe.outPath, std::ios::binary);
        if (!out)
        {
            continue; // Skip silently for speed
        }
        
        out.write(toWrite.data(), static_cast<std::streamsize>(toWrite.size()));
        
        ++successCount;

        //  Log every 100 files or last file for speed
        if (i % 100 == 0 || i == processedEntries.size() - 1)
        {
            VortigauntLog::LogF("^7[%zu/%zu] %s ^3(%s)^7 %s\n",
                i + 1, totalEntries,
                pe.safeRelPath.string().c_str(),
                FormatSize(finalSize).c_str(),
                (lzmaOk ? "^7[LZMA]" : "^7[raw]"));
        }

        if (i - lastReportedCount >= progressReportInterval || i == processedEntries.size() - 1)
        {
            int percent = static_cast<int>((i + 1) * 100 / totalEntries);
            ReportProgress(percent);
            ReportFileProgress(i + 1, totalEntries);
            lastReportedCount = i;
        }
    }

    VortigauntLog::LogF("\n^2[REZ] Extraction finished!^7 ^3%zu/%zu^7 files written to ^4%s^7\n",
        successCount, m_entries.size(), outputDir.c_str());

    return successCount > 0;
}

bool RezExtractor::ExtractSelectedEntries(const std::vector<size_t>& indices, const std::string& outputDir) const
{
    if (m_rezFilePath.empty() || m_entries.empty() || indices.empty())
    {
        VortigauntLog::LogF("^3[REZ]^7 Nothing to extract (no entries or file not loaded).\n");
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(FileIO::toPath(outputDir), ec);
    if (ec)
    {
        VortigauntLog::LogF("ERROR:^7 failed to create output directory: ^4%s^7 (%s)\n", outputDir.c_str(), ec.message().c_str());
        return false;
    }

    std::ifstream in(FileIO::toPath(m_rezFilePath), std::ios::binary);
    if (!in)
    {
        VortigauntLog::LogF("ERROR:^7 cannot reopen REZ file for extraction: ^4%s^7\n", m_rezFilePath.c_str());
        return false;
    }

    std::vector<size_t> sortedIndices = indices;
    std::sort(sortedIndices.begin(), sortedIndices.end(), [this](size_t a, size_t b) {
        if (a >= m_entries.size() || b >= m_entries.size())
            return a < b;
        return m_entries[a].offset < m_entries[b].offset;
    });

    std::set<std::filesystem::path> createdDirs;
    
    struct ProcessedEntry {
        const RezEntry* entry;
        std::filesystem::path safeRelPath;
        std::filesystem::path outPath;
        size_t originalIndex;
    };
    std::vector<ProcessedEntry> processedEntries;
    processedEntries.reserve(sortedIndices.size());
    
    for (size_t idx : sortedIndices)
    {
        if (idx >= m_entries.size())
            continue;
            
        const RezEntry& e = m_entries[idx];
        if (e.size == 0)
            continue;


        // TODO: i dont feel comfortable with this being 1 GB, but it is what it is for now.
        static const std::uint64_t MAX_ENTRY_SIZE = 1024ull * 1024ull * 1024ull; // 1 GB
        if (e.size > MAX_ENTRY_SIZE)
            continue;

        std::filesystem::path relPath = FileIO::toPath(e.filename);
        std::filesystem::path safeRelPath;
        for (const auto& part : relPath)
        {
            std::string s = part.string();
            if (s == "." || s == ".." || s.empty())
                continue;
            safeRelPath /= SanitizePathComponent(s);
        }
        if (safeRelPath.empty())
        {
            safeRelPath = SanitizePathComponent(e.filename);
        }

        std::filesystem::path outPath = FileIO::toPath(outputDir) / safeRelPath;
        std::filesystem::path parentDir = outPath.parent_path();
        
        if (!parentDir.empty() && createdDirs.find(parentDir) == createdDirs.end())
        {
            createdDirs.insert(parentDir);
        }

        processedEntries.push_back({&e, safeRelPath, outPath, idx});
    }

    for (const auto& dir : createdDirs)
    {
        std::error_code dirEc;
        std::filesystem::create_directories(dir, dirEc);
    }

    // Progress tracking
    const size_t totalEntries = processedEntries.size();
    size_t progressReportInterval = std::max<size_t>(1, totalEntries / 20);
    if (progressReportInterval < 1) progressReportInterval = 1;
    size_t lastReportedCount = 0;

    size_t successCount = 0;

    // Extract entries in sorted order (sequential reads are faster)
    for (size_t i = 0; i < processedEntries.size(); ++i)
    {
        const ProcessedEntry& pe = processedEntries[i];
        const RezEntry& e = *pe.entry;

        std::streampos currentPos = in.tellg();
        if (currentPos < 0 || static_cast<uint32_t>(currentPos) != e.offset)
        {
            in.seekg(e.offset, std::ios::beg);
            if (!in)
            {
                continue; // Skip failed seeks silently for speed
            }
        }

        // Guard against insane sizes (already checked in pre-processing)
		// TODO: i dont feel comfortable with this being 1 GB, but it is what it is for now.
        static const std::uint64_t MAX_ENTRY_SIZE = 1024ull * 1024ull * 1024ull; // 1 GB
        if (e.size > MAX_ENTRY_SIZE)
        {
            continue; // Skip very large files
        }
        
        size_t finalSize = 0;
        bool lzmaOk = false;
        
        // Always use standard extraction with LZMA decompression
        std::vector<char> compressed(static_cast<size_t>(e.size));
        in.read(compressed.data(), static_cast<std::streamsize>(e.size));
        std::streamsize got = in.gcount();
        if (got <= 0) continue;
        compressed.resize(static_cast<size_t>(got));

        std::vector<char> decompressed;
        lzmaOk = DecompressLZMA(compressed, decompressed);
        const std::vector<char>& toWrite = lzmaOk ? decompressed : compressed;
        finalSize = toWrite.size();

        std::ofstream out(pe.outPath, std::ios::binary);
        if (!out) continue;
        
        out.write(toWrite.data(), static_cast<std::streamsize>(toWrite.size()));

        ++successCount;
        
        // Log every 100 files or last file for speed
        if (i % 100 == 0 || i == processedEntries.size() - 1)
        {
            VortigauntLog::LogF("^7[%zu/%zu] %s ^3(%s)^7 %s\n",
                i + 1, totalEntries,
                pe.safeRelPath.string().c_str(),
                FormatSize(finalSize).c_str(),
                (lzmaOk ? "^7[LZMA]" : "^7[Raw]"));
        }

        // Progress reporting (less frequent)
        if (i - lastReportedCount >= progressReportInterval || i == processedEntries.size() - 1)
        {
            int percent = static_cast<int>((i + 1) * 100 / totalEntries);
            ReportProgress(percent);
            ReportFileProgress(i + 1, totalEntries);
            lastReportedCount = i;
        }
    }

    return successCount > 0;
}

bool RezExtractor::ExtractEntryToMemory(const RezEntry& entry, std::vector<char>& outData) const
{
    outData.clear();

    if (m_rezFilePath.empty() || entry.size == 0)
        return false;

    std::ifstream in(FileIO::toPath(m_rezFilePath), std::ios::binary);
    if (!in)
        return false;

    in.seekg(entry.offset, std::ios::beg);
    if (!in)
        return false;

    static const std::uint64_t MAX_ENTRY_SIZE = 1024ull * 1024ull * 1024ull;
    if (entry.size > MAX_ENTRY_SIZE)
        return false;

    std::vector<char> compressed(static_cast<size_t>(entry.size));
    in.read(compressed.data(), static_cast<std::streamsize>(entry.size));
    std::streamsize got = in.gcount();
    if (got <= 0)
        return false;
    compressed.resize(static_cast<size_t>(got));

    // Try LZMA decompress first
    std::vector<char> decompressed;
    if (DecompressLZMA(compressed, decompressed))
    {
        outData = std::move(decompressed);
    }
    else
    {
        outData = std::move(compressed);
    }

    return true;
}

bool RezExtractor::ExtractEntry(const RezEntry& entry, const std::string& outputPath) const
{
    std::vector<char> data;
    if (!ExtractEntryToMemory(entry, data))
        return false;

    std::filesystem::path outPath = FileIO::toPath(outputPath);
    
    std::error_code ec;
    std::filesystem::create_directories(outPath.parent_path(), ec);
    if (ec)
    {
        // Fail silently or log if needed, but return false
        return false;
    }

    std::ofstream out(outPath, std::ios::binary);
    if (!out)
        return false;

    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    return true;
}


bool RezExtractor::ParseDirectory(std::ifstream& in)
{

    in.clear();
    in.seekg(0, std::ios::beg);

    unsigned char header[148] = {};
    in.read(reinterpret_cast<char*>(header), sizeof(header));
    if (in.gcount() != static_cast<std::streamsize>(sizeof(header)))
    {
        VortigauntLog::LogF("^1Error:^7 Failed to read REZ header (expected 148 bytes).\n");
        return false;
    }

    std::string fileType(reinterpret_cast<char*>(header + 2), 60);
    while (!fileType.empty() &&
        (fileType.back() == '\0' || fileType.back() == '\r' ||
            fileType.back() == '\n' || fileType.back() == ' '))
    {
        fileType.pop_back();
    }

    std::string firstLine;
    {
        size_t pos = fileType.find('\n');
        if (pos == std::string::npos)
            pos = fileType.find('\r');
        firstLine = fileType.substr(0, pos);
    }
    if (!firstLine.empty())
        VortigauntLog::LogF("%s\n", firstLine.c_str());

    auto readLE32 = [](const unsigned char* p) -> uint32_t
        {
            return static_cast<uint32_t>(p[0])
                | (static_cast<uint32_t>(p[1]) << 8)
                | (static_cast<uint32_t>(p[2]) << 16)
                | (static_cast<uint32_t>(p[3]) << 24);
        };

    //uint32_t version = readLE32(header + 0x7F);
    uint32_t rootDirPos = readLE32(header + 0x83);
    uint32_t rootDirSize = readLE32(header + 0x87);
    uint32_t rootDirTime = readLE32(header + 0x8B);
    (void)rootDirTime; // currently unused

    m_entries.clear();


    uint32_t totalDirSize = rootDirSize;
    size_t processedBytes = 0;
    size_t entryCount = 0;
    int lastReportedProgress = 10;

    std::function<void(const std::string&, uint32_t, uint32_t)> processDirectory;
    processDirectory = [&](const std::string& dirPrefix, uint32_t pos, uint32_t size)
        {

            in.clear();
            in.seekg(pos, std::ios::beg);
            if (!in)
                return;

            std::vector<unsigned char> encrypted;
            encrypted.resize(size);
            in.read(reinterpret_cast<char*>(encrypted.data()), static_cast<std::streamsize>(size));
            std::streamsize got = in.gcount();
            if (got <= 0)
                return;
            encrypted.resize(static_cast<size_t>(got));

            REZ_DecryptBuffer(encrypted, pos);

            if (totalDirSize > 0)
            {
                processedBytes += size;
                int percent = 10 + static_cast<int>((processedBytes * 80) / totalDirSize);
                percent = std::min(percent, 90); // Cap at 90% during parsing
                
                if (percent > lastReportedProgress)
                {
                    lastReportedProgress = percent;
                    ReportProgress(percent);
                }
            }

            size_t offset = 0;
            const size_t total = encrypted.size();

            while (offset + 4 <= total)
            {
                uint32_t entryType;
                std::memcpy(&entryType, &encrypted[offset], sizeof(uint32_t));
                offset += 4;

                if (entryType == 1) // Directory
                {
                    if (offset + 16 > total)
                        break;

                    uint32_t dirPos, dirSize, dirTime, nameLen;
                    std::memcpy(&dirPos, &encrypted[offset + 0], 4);
                    std::memcpy(&dirSize, &encrypted[offset + 4], 4);
                    std::memcpy(&dirTime, &encrypted[offset + 8], 4);
                    std::memcpy(&nameLen, &encrypted[offset + 12], 4);
                    offset += 16;

                    if (nameLen == 0 || offset + nameLen + 1 > total)
                        break;

                    std::string dirName(reinterpret_cast<char*>(&encrypted[offset]), nameLen);
                    offset += nameLen + 1; 

                    static size_t dirLogCounter = 0;
                    if (++dirLogCounter % 100 == 0)
                    {
                        VortigauntLog::LogF("[DIR] %s%s/\n", dirPrefix.c_str(), dirName.c_str());
                    }

                    processDirectory(dirPrefix + dirName + "/", dirPos, dirSize);
                }
                else if (entryType == 0) // File
                {
                    if (offset + 28 > total)
                        break;

                    uint32_t filePos, fileSize, fileTime, fileId, fileType, numKeys, nameLen;
                    std::memcpy(&filePos, &encrypted[offset + 0], 4);
                    std::memcpy(&fileSize, &encrypted[offset + 4], 4);
                    std::memcpy(&fileTime, &encrypted[offset + 8], 4);
                    std::memcpy(&fileId, &encrypted[offset + 12], 4);
                    std::memcpy(&fileType, &encrypted[offset + 16], 4);
                    std::memcpy(&numKeys, &encrypted[offset + 20], 4);
                    std::memcpy(&nameLen, &encrypted[offset + 24], 4);
                    offset += 28;

                    if (nameLen == 0 || offset + nameLen + 1 > total)
                        break;

                    std::string fileName(reinterpret_cast<char*>(&encrypted[offset]), nameLen);
                    offset += nameLen + 1;

                    // Skip extra data: 33 bytes + numKeys * 4 
                    uint64_t extra = 33ull + static_cast<uint64_t>(numKeys) * 4ull;
                    if (offset + extra > total)
                        break;
                    offset += static_cast<size_t>(extra);

                    std::string ext(reinterpret_cast<char*>(&fileType));
                    std::reverse(ext.begin(), ext.end());
                    while (!ext.empty() && (ext.back() == '\0' || ext.back() == ' '))
                        ext.pop_back();

                    std::string fullName = dirPrefix + fileName;
                    if (!ext.empty())
                    {
                        fullName += ".";
                        fullName += ext;
                    }

                    RezEntry entry;
                    entry.filename = fullName;
                    entry.offset = filePos;
                    entry.size = fileSize;
                    entry.flags = 0;
                    m_entries.push_back(entry);
                    ++entryCount;

                }
                else
                {
                    // Unknown entry type we need stop.
                    break;
                }
            }
        };

    ReportProgress(10); // ¯\_(ツ)_/¯ 
    processDirectory("", rootDirPos, rootDirSize);
    ReportProgress(100);// ¯\_(ツ)_/¯ 

    if (m_entries.empty())
    {
        VortigauntLog::LogF("[REZ] No file entries found in directory.\n");
        return false;
    }

    return true;
}

