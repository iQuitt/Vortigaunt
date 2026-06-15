#include "UnityDataResolver.h"
#include "utils/BinaryUtils.h"
#include <fstream>
#include <algorithm>
#include <filesystem>



bool UnityDataResolver::LoadDirectory(const std::string& directory, const std::vector<std::string>& extensions) {
    bool anyLoaded = false;
    std::filesystem::path dirPath(directory);
    if (!std::filesystem::exists(dirPath) || !std::filesystem::is_directory(dirPath))
        return false;

    // Normalize extensions list to lowercase to resolve case-sensitivity issues
    std::vector<std::string> lowerExts;
    for (const auto& ext : extensions) {
        std::string le = ext;
        std::transform(le.begin(), le.end(), le.begin(), ::tolower);
        lowerExts.push_back(le);
    }

    for (const auto& entry : std::filesystem::recursive_directory_iterator(dirPath)) {
        if (!entry.is_regular_file()) continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (std::find(lowerExts.begin(), lowerExts.end(), ext) == lowerExts.end())
            continue;

        // Record absolute path, keyed by normalized filename and absolute path
        std::string absPath = entry.path().string();
        m_filePaths[Utils::Normalise(absPath)] = absPath;
        m_filePaths[Utils::Normalise(entry.path().filename().string())] = absPath;
        anyLoaded = true;
    }
    return anyLoaded;
}

bool UnityDataResolver::HasFile(const std::string& filename) const {
    return m_filePaths.find(Utils::Normalise(filename)) != m_filePaths.end();
}

std::uint64_t UnityDataResolver::GetFileSize(const std::string& filename) const {
    auto it = m_filePaths.find(Utils::Normalise(filename));
    if (it == m_filePaths.end()) return 0;
    std::error_code ec;
    return std::filesystem::file_size(it->second, ec);
}

template<typename T>
bool UnityDataResolver::ReadDataGeneric(const std::string& filename, std::uint64_t offset, std::uint32_t size, std::vector<T>& outBuffer) const {
    std::string target = Utils::Normalise(filename);
    auto it = m_filePaths.find(target);
    if (it == m_filePaths.end()) {
        std::filesystem::path p(filename);
        it = m_filePaths.find(Utils::Normalise(p.filename().string()));
    }
    if (it == m_filePaths.end()) {
        return false;
    }

    std::ifstream f(it->second, std::ios::binary);
    if (!f) {
        return false;
    }

    f.seekg(0, std::ios::end);
    std::uint64_t fileSize = f.tellg();
    if (offset > fileSize) {
        return false;
    }
    if (offset + size > fileSize) {
        size = static_cast<std::uint32_t>(fileSize - offset);
    }

    f.seekg(offset, std::ios::beg);
    outBuffer.resize(size);
    if (size > 0) {
        f.read(reinterpret_cast<char*>(outBuffer.data()), size);
    }
    return !f.bad();
}

bool UnityDataResolver::ReadData(const std::string& filename, std::uint64_t offset, std::uint32_t size, std::vector<std::uint8_t>& outBuffer) const {
    return ReadDataGeneric(filename, offset, size, outBuffer);
}

bool UnityDataResolver::ReadData(const std::string& filename, std::uint64_t offset, std::uint32_t size, std::vector<char>& outBuffer) const {
    return ReadDataGeneric(filename, offset, size, outBuffer);
}

void UnityDataResolver::Clear() {
    m_filePaths.clear();
}



