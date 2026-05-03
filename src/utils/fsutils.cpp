#include "fsutils.hpp"

#include <fstream>
#include "utils/FileIO.h"
// this isnt mine code i took from this project https://git.sr.ht/~leite/cso-pak

std::pair<bool, std::vector<uint8_t>> ReadFileToBuffer(
    std::string_view filename, uint64_t readLength /*= 0*/)
{
    if (FileIO::isRegularFile(std::string(filename)) == false)
    {
        return { false, {} };
    }

    std::ifstream is(FileIO::toPath(std::string(filename)), std::ios::binary | std::ios::ate);

    if (is.is_open() == false)
    {
        return { false, {} };
    }

    if (readLength == 0)
    {
        uint64_t iFileSize = is.tellg();
        readLength = iFileSize;
    }

    std::vector<uint8_t> res(readLength);

    is.seekg(std::ios::beg);
    is.read(reinterpret_cast<char*>(res.data()), readLength);

    return { true, res };
}

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
    const char* units[] = { "B", "KB", "MB", "GB" };
    int i = 0;
    double size = static_cast<double>(bytes);
    while (size >= 1024 && i < 4)
    {
        size /= 1024;
        i++;
    }

    char buf[64];
    if (i == 0)
    {
        std::snprintf(buf, sizeof(buf), "%llu B", (unsigned long long)bytes);
    }
    else
    {
        std::snprintf(buf, sizeof(buf), "%.2f %s", size, units[i]);
    }
    return std::string(buf);
}

