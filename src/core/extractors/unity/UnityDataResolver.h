#pragma once

#include <string>
#include <ankerl/unordered_dense.h>
#include <vector>
#include <cstdint>


class UnityDataResolver {
public:
    UnityDataResolver() = default;
    ~UnityDataResolver() = default;

    // Load all files matching the provided extensions under the given directory.
    // Returns true if at least one file path was recorded.
    bool LoadDirectory(const std::string& directory,
                       const std::vector<std::string>& extensions = {".resource", ".resS", ".dat"});

    // Check if a companion file exists by filename
    bool HasFile(const std::string& filename) const;

    // Get the size of a companion file by filename
    std::uint64_t GetFileSize(const std::string& filename) const;

    // Read a specific chunk of data from a companion file into a std::vector<std::uint8_t>
    bool ReadData(const std::string& filename, std::uint64_t offset, std::uint32_t size, std::vector<std::uint8_t>& outBuffer) const;

    // Read a specific chunk of data from a companion file into a std::vector<char>
    bool ReadData(const std::string& filename, std::uint64_t offset, std::uint32_t size, std::vector<char>& outBuffer) const;

    // Clear all loaded data paths.
    void Clear();

private:
    template<typename T>
    bool ReadDataGeneric(const std::string& filename, std::uint64_t offset, std::uint32_t size, std::vector<T>& outBuffer) const;

    // Maps normalized filename or normalized absolute path to the original absolute path on disk
    ankerl::unordered_dense::map<std::string, std::string> m_filePaths;
};

