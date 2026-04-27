#ifndef _FSUTILS_H_
#define _FSUTILS_H_

#include <cstdint>
#include <filesystem>
#include <span>
#include <string_view>
#include <vector>


// this isnt mine code i took from this project https://git.sr.ht/~leite/cso-pak

namespace fs = std::filesystem;

std::pair<bool, std::vector<uint8_t>> ReadFileToBuffer(
    std::string_view filename, uint64_t readLength = 0);

bool WriteBufferToFile(std::span<uint8_t> buff, std::string_view filename);

std::string FormatSize(uint64_t bytes);

#endif  // _FSUTILS_H_



