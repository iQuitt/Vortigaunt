#pragma once

#include <filesystem>
#include <fstream>
#include <string>
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

} // namespace FileIO
