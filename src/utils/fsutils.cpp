// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "fsutils.hpp"

#include <cstdio>
#include <fstream>
#include <iterator>
#include "utils/FileIO.h"
// this isnt mine code i took from this project https://git.sr.ht/~leite/cso-pak

bool WriteBufferToFile(std::span<uint8_t> buff, std::string_view filename)
{
    std::ofstream os(FileIO::toPath(std::string(filename)), std::ios::binary);

    if (os.is_open() == false)
    {
        return false;
    }

    os.write(reinterpret_cast<char*>(buff.data()), buff.size_bytes());

    return true;
}

std::string FormatSize(uint64_t bytes)
{
    const char* units[] = { "B", "KB", "MB", "GB", "TB" };
    const int last = static_cast<int>(std::size(units)) - 1;
    int i = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024 && i < last)
    {
        size /= 1024;
        i++;
    }

    char buf[64];
    if (i == 0)
        std::snprintf(buf, sizeof(buf), "%llu B", static_cast<unsigned long long>(bytes));
    else
        std::snprintf(buf, sizeof(buf), i == 1 ? "%.1f %s" : "%.2f %s", size, units[i]);
    return std::string(buf);
}

