#pragma once

#ifdef QT_CORE_LIB
#include "core/extractors/IExtractor.h"
#endif
#include "core/extractors/unity/UnityFsExtractor.h"
#include "core/extractors/unity/UnityAssetParser.h"
#include "core/VortigauntLog.h"
#include <cstdint>
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

    // When false, Texture2D objects are exported as 32-bit RGBA PNG instead of
    // 8-bit indexed BMP (mesh material names follow the chosen extension).
    void SetConvertTexturesToBmp(bool enabled) { m_convertTexturesToBmp = enabled; }

private:
    struct ObjectRef { int32_t fileKey = -1; int64_t pathId = 0; };   // fileKey indexes m_summaries
    struct FileSummary {
        std::string absPath;                                          // canonical: absolute().make_preferred()
        std::vector<int32_t> externalKeys;                            // parallel to that file's externals; -1 = unresolved
        ankerl::unordered_dense::map<int64_t, int32_t> classIds;      // key classes only (IsKeyClass)
        ankerl::unordered_dense::map<int64_t, std::string> textureNames;  // Texture2D
        ankerl::unordered_dense::map<int64_t, std::string> meshNames;     // Mesh
        ankerl::unordered_dense::map<int64_t, std::vector<ObjectRef>> materialTextures; // Material -> texture refs
    };

    // Summarize a serialized file (key-class metadata, externals, material textures).
    // Recurses into resolvable externals; cycle-safe via the pre-registered key.
    int32_t SummarizeFile(const std::string& absPath);
    // Map a PPtr (fileID, pathID) seen in file currentKey to a concrete object reference.
    ObjectRef ResolveRef(int32_t currentKey, int32_t fileID, int64_t pathID) const;
    // Class id of the object a PPtr in file currentKey points to; -1 if unknown.
    int32_t LookupClassId(int32_t currentKey, int32_t fileID, int64_t pathID) const;
    std::string MeshKey(const ObjectRef& ref) const;
    void ClearRegistries();

    ankerl::unordered_dense::map<int64_t, std::vector<std::string>> BuildDependencyMap(
        const UnityAssetParser& parser, int32_t mainKey);
    void WriteMeshTextureMap(const UnityAssetParser& parser,
                             const ankerl::unordered_dense::map<int64_t, std::vector<std::string>>& meshToTextures,
                             int32_t mainKey,
                             const std::string& outputDir);
    void DumpDiagnostics(const UnityAssetParser& parser,
                         const ankerl::unordered_dense::map<int64_t, std::vector<std::string>>& meshToTextures,
                         int32_t mainKey,
                         const std::string& outputDir);

private:
    std::vector<FileSummary> m_summaries;
    ankerl::unordered_dense::map<std::string, int32_t> m_summaryKeyByPath;  // lowercased absPath -> key
    // Accumulated across one Process() run; key = MeshKey(fileKey, pathId) built as absPathLower + "|" + pathId
    ankerl::unordered_dense::map<std::string, std::vector<std::string>> m_meshTextures;
    ankerl::unordered_dense::map<std::string, std::string> m_meshNamesByKey;
    // Makes each ProcessBundle extraction directory unique within one Process()
    // run so cached summary paths never alias a later bundle's extracted files.
    uint32_t m_bundleExtractCounter = 0;
    bool m_convertTexturesToBmp = true;
};
