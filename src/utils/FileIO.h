// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace FileIO {

inline std::filesystem::path toPath(const std::string& utf8str)
{
#ifdef _WIN32
    if (utf8str.empty())
        return {};

    int wlen = MultiByteToWideChar(CP_UTF8, 0, utf8str.data(),
                                   static_cast<int>(utf8str.size()), nullptr, 0);
    if (wlen <= 0)
        return std::filesystem::path(utf8str);

    std::wstring wstr(static_cast<size_t>(wlen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8str.data(),
                        static_cast<int>(utf8str.size()), wstr.data(), wlen);
    return std::filesystem::path(wstr);
#else
    // in Linux std::string is already UTF-8
    return std::filesystem::path(utf8str);
#endif
}

inline std::filesystem::path toPath(std::string_view utf8sv)
{
    return toPath(std::string(utf8sv));
}

inline std::string fromPath(const std::filesystem::path& path)
{
    const std::u8string utf8 = path.u8string();
    return std::string(utf8.begin(), utf8.end());
}

inline std::ifstream openRead(const std::string& utf8Path, std::ios::openmode mode = std::ios::in)
{
    return std::ifstream(toPath(utf8Path), mode);
}

inline std::ofstream openWrite(const std::string& utf8Path, std::ios::openmode mode = std::ios::out)
{
    return std::ofstream(toPath(utf8Path), mode);
}

inline FILE* openFile(const std::string& utf8Path, const char* mode)
{
#ifdef _WIN32
    if (utf8Path.empty() || !mode)
        return nullptr;

    int wPathLen = MultiByteToWideChar(CP_UTF8, 0, utf8Path.data(),
                                       static_cast<int>(utf8Path.size()), nullptr, 0);
    if (wPathLen <= 0)
        return fopen(utf8Path.c_str(), mode); 

    std::wstring wPath(static_cast<size_t>(wPathLen), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8Path.data(),
                        static_cast<int>(utf8Path.size()), wPath.data(), wPathLen);

    // Convert mode
    std::wstring wMode;
    for (const char* p = mode; *p; ++p)
        wMode += static_cast<wchar_t>(*p);

    return _wfopen(wPath.c_str(), wMode.c_str());
#else
    return fopen(utf8Path.c_str(), mode);
#endif
}

inline bool fileExists(const std::string& utf8Path)
{
    std::error_code ec;
    return std::filesystem::exists(toPath(utf8Path), ec);
}

inline bool isRegularFile(const std::string& utf8Path)
{
    std::error_code ec;
    return std::filesystem::is_regular_file(toPath(utf8Path), ec);
}

// a whole file's bytes, into a vector of any one-byte type; false (and out empty) when it is missing or empty
template <typename Byte>
inline bool readBytes(const std::filesystem::path& path, std::vector<Byte>& out)
{
    static_assert(sizeof(Byte) == 1, "bytes only");
    out.clear();
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    const std::streamsize size = file ? static_cast<std::streamsize>(file.tellg()) : 0;
    if (size <= 0)
        return false;
    out.resize(static_cast<size_t>(size));
    file.seekg(0);
    if (!file.read(reinterpret_cast<char*>(out.data()), size))
    {
        out.clear();
        return false;
    }
    return true;
}

template <typename Byte>
inline bool readBytes(const std::string& utf8Path, std::vector<Byte>& out)
{
    return readBytes(toPath(utf8Path), out);
}

// a whole text file as it is; false when it cannot be opened
inline bool readText(const std::string& utf8Path, std::string& out)
{
    out.clear();
    std::ifstream file(toPath(utf8Path), std::ios::binary);
    if (!file.is_open())
        return false;
    out.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    return true;
}

// ".fbx" for "Model.FBX"
inline std::string lowerExtension(const std::string& utf8Path)
{
    std::string ext = fromPath(toPath(utf8Path).extension());
    for (char& c : ext)
    {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return ext;
}

// Replace characters that are illegal in a Windows path component with '_'.
// Used by the archive extractors so a hostile archive cannot escape the output folder.
inline std::string sanitizePathComponent(const std::string& name)
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


inline std::filesystem::path safeRelativePath(const std::string& utf8Path)
{
    std::filesystem::path out;
    std::string part;
    auto flush = [&]() {
        if (!part.empty() && part != "." && part != "..")
            out /= toPath(sanitizePathComponent(part));
        part.clear();
    };
    for (char c : utf8Path)
    {
        if (c == '/' || c == '\\')
            flush();
        else
            part += c;
    }
    flush();
    return out.empty() ? std::filesystem::path("_") : out;
}

} // namespace FileIO
