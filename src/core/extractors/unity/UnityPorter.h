#pragma once

#ifdef QT_CORE_LIB
#include "core/extractors/IExtractor.h"
#endif
#include "core/extractors/unity/UnityFsExtractor.h"
#include "core/extractors/unity/UnityAssetParser.h"
#include "core/VortigauntLog.h"
#include <filesystem>
#include <string>
#include <ankerl/unordered_dense.h>
#include <vector>

class UnityPorter {
public:
    UnityPorter() = default;
    ~UnityPorter() = default;

    // Route processing based on file type (bundle, direct assets file, or directory)
    void Process(const std::string& inputPath, const std::string& outputDir);

    // Process a UnityFS bundle: extract, parse assets, and optionally convert.
    void ProcessBundle(const std::string& bundlePath, const std::string& outputDir);

    // Process a serialized .assets file directly.
    void ProcessAssetFile(const std::string& assetPath, const std::string& outputDir);

private:
    void PreParseAssetMetadata(const std::string& assetPath);
    std::string FindMeshGlobalKey(int64_t meshPathId, const std::string& currentAssetPath);
    ankerl::unordered_dense::map<int64_t, std::vector<std::string>> BuildDependencyMap(
        const UnityAssetParser& parser,
        const std::string& assetPath,
        const std::vector<std::string>& companionFiles);
    void WriteMeshTextureMap(const UnityAssetParser& parser,
                             const ankerl::unordered_dense::map<int64_t, std::vector<std::string>>& meshToTextures,
                             const std::string& outputDir);
    void DumpDiagnostics(const UnityAssetParser& parser,
                         const ankerl::unordered_dense::map<int64_t, std::vector<std::string>>& meshToTextures,
                         const std::string& outputDir);

private:
    ankerl::unordered_dense::map<std::string, std::vector<std::string>> m_meshToTextures;
    ankerl::unordered_dense::map<std::string, std::string> m_textureNames;
    ankerl::unordered_dense::map<std::string, std::string> m_meshNames;
    ankerl::unordered_dense::map<std::string, int32_t> m_pathIdToClassId;
    ankerl::unordered_dense::map<int64_t, int32_t> m_pathIdToClassIdInt;
    ankerl::unordered_dense::map<std::string, std::vector<std::string>> m_matToTexs;
    ankerl::unordered_dense::map<std::string, std::vector<int64_t>> m_matToTexPathIds; // Ham PathID eşlemeleri
    ankerl::unordered_dense::set<std::string> m_preParsedFiles;
};

