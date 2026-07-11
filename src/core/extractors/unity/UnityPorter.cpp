#include "UnityPorter.h"
#include "UnityDataResolver.h"
#include "VortigauntLog.h"
#include "UnityMeshConverter.h"
#include "UnityTextureDecoder.h"
#include "UnityClassIds.h"
#include "utils/BinaryUtils.h"
#include <algorithm>
#include <filesystem>
#include <ankerl/unordered_dense.h>
#include <vector>
#include <cstring>
#include <cstdlib>
#include <fstream>


static std::string NormalizePath(const std::string& pathStr) {
    try {
        return std::filesystem::path(pathStr).make_preferred().string();
    } catch (...) {
        return pathStr;
    }
}

static std::string CanonicalAbsPath(const std::string& pathStr) {
    try {
        return std::filesystem::absolute(pathStr).make_preferred().string();
    } catch (...) {
        return pathStr;
    }
}

static std::string ToLowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), ::tolower);
    return s;
}

static bool IsUiTexture(const std::string& name) {
    std::string lower = ToLowerCopy(name);

    static const std::vector<std::string> uiKeywords = {
        "icon", "sprite", "font", "checkmark", "cursor", "ui_", "hud_",
        "background", "button", "logo", "label", "slider", "loading",
        "connecticon", "ping micro", "cooldown", "arrow", "pointer",
        "scrollbar", "panel", "frame", "border", "checkmark", "checkmarksprite",
        "font texture", "please wait", "indicator", "checkmark", "loadingcircle"
    };

    for (const auto& keyword : uiKeywords) {
        if (lower.find(keyword) != std::string::npos) {
            return true;
        }
    }
    return false;
}

static std::string SelectAlbedoTexture(const std::vector<std::string>& texNames) {
    if (texNames.empty()) return "";
    std::vector<std::string> cleanNames;
    for (const auto& name : texNames) {
        if (!name.empty()) cleanNames.push_back(name);
    }
    if (cleanNames.empty()) return "";

    static const std::vector<std::string> albedoKeywords = {
        "albedotransparency", "albedo", "basecolor", "basemap", "diffuse", "_d", "_color", "_albedo"
    };
    for (const auto& keyword : albedoKeywords) {
        for (const auto& name : cleanNames) {
            std::string lower = ToLowerCopy(name);
            if (lower.find(keyword) != std::string::npos) {
                return name;
            }
        }
    }

    static const std::vector<std::string> utilityKeywords = {
        "normal", "n_opengl", "_n", "mask", "roughness", "metallic", "emission", "emissive", "_e", "height", "specular", "ao", "gloss"
    };
    for (const auto& name : cleanNames) {
        std::string lower = ToLowerCopy(name);
        bool hasUtility = false;
        for (const auto& keyword : utilityKeywords) {
            if (lower.find(keyword) != std::string::npos) {
                hasUtility = true;
                break;
            }
        }
        if (!hasUtility) {
            return name;
        }
    }
    return cleanNames[0];
}

static UnityValue GetArrayField(const UnityValue& parent, const std::string& fieldName) {
    const auto& field = parent.GetField(fieldName);
    if (field.type == UnityValue::ArrayVal) {
        return field;
    }
    if (field.type == UnityValue::ObjectVal) {
        const auto& arr = field.GetField("Array");
        if (arr.type == UnityValue::ArrayVal) {
            return arr;
        }
    }
    return UnityValue{};
}

static void DumpValue(std::ostream& os, const std::string& indent, const UnityValue& val) {
    if (val.type == UnityValue::NullVal) {
        os << "Null";
    } else if (val.type == UnityValue::IntVal) {
        os << val.intVal;
    } else if (val.type == UnityValue::UIntVal) {
        os << val.uintVal;
    } else if (val.type == UnityValue::FloatVal) {
        os << val.doubleVal << "f";
    } else if (val.type == UnityValue::DoubleVal) {
        os << val.doubleVal;
    } else if (val.type == UnityValue::BoolVal) {
        os << (val.boolVal ? "true" : "false");
    } else if (val.type == UnityValue::StringVal) {
        os << "\"" << val.stringVal << "\"";
    } else if (val.type == UnityValue::ByteArrayVal) {
        os << "ByteArray[" << val.byteData.size() << "]";
    } else if (val.type == UnityValue::ArrayVal) {
        os << "Array[" << val.arrayVal.size() << "] {\n";
        for (size_t i = 0; i < val.arrayVal.size(); ++i) {
            os << indent << "  [" << i << "]: ";
            DumpValue(os, indent + "  ", val.arrayVal[i]);
            os << "\n";
        }
        os << indent << "}";
    } else if (val.type == UnityValue::ObjectVal) {
        os << "Object {\n";
        for (const auto& pair : val.objectVal) {
            os << indent << "  " << pair.first << ": ";
            DumpValue(os, indent + "  ", pair.second);
            os << "\n";
        }
        os << indent << "}";
    }
}

static bool HasValuableTextures(const std::vector<std::string>& texs) {
    for (const auto& t : texs) {
        if (!t.empty()) return true;
    }
    return false;
}

static bool IsSerializedAssetFile(const std::string& filepath) {
    std::filesystem::path p(filepath);
    std::string ext = ToLowerCopy(p.extension().string());

    // Only allow real Unity serialized asset extensions or extensionless level/resources files
    if (ext != ".assets" && ext != ".sharedassets" && ext != ".globalgamemanagers" && !ext.empty()) {
        return false;
    }

    std::ifstream f(filepath, std::ios::binary);
    if (!f) return false;
    char header[16];
    f.read(header, 16);
    if (f.gcount() < 16) return false;

    std::uint32_t version = Utils::ReadBE32(reinterpret_cast<const std::uint8_t*>(header + 8));
    return (version >= 9);
}

static bool IsKeyClass(int32_t classId) {
    return classId == UnityClass::GameObject ||
           classId == UnityClass::Material ||
           classId == UnityClass::MeshRenderer ||
           classId == UnityClass::Renderer ||
           classId == UnityClass::Texture2D ||
           classId == UnityClass::MeshFilter ||
           classId == UnityClass::Mesh ||
           classId == UnityClass::AnimationClip ||
           classId == UnityClass::SkinnedMeshRenderer;
}

void UnityPorter::ClearRegistries() {
    m_summaries.clear();
    m_summaryKeyByPath.clear();
    m_meshTextures.clear();
    m_meshNamesByKey.clear();
    m_bundleExtractCounter = 0;
}

UnityPorter::ObjectRef UnityPorter::ResolveRef(int32_t currentKey, int32_t fileID, int64_t pathID) const {
    ObjectRef ref;
    ref.pathId = pathID;
    if (pathID == 0) {
        return ref; // null reference
    }
    if (currentKey < 0 || currentKey >= static_cast<int32_t>(m_summaries.size())) {
        return ref;
    }
    if (fileID == 0) {
        ref.fileKey = currentKey;
        return ref;
    }
    const auto& externalKeys = m_summaries[currentKey].externalKeys;
    int64_t index = static_cast<int64_t>(fileID) - 1;
    if (index < 0 || index >= static_cast<int64_t>(externalKeys.size())) {
        return ref;
    }
    ref.fileKey = externalKeys[static_cast<size_t>(index)];
    return ref;
}

int32_t UnityPorter::LookupClassId(int32_t currentKey, int32_t fileID, int64_t pathID) const {
    ObjectRef ref = ResolveRef(currentKey, fileID, pathID);
    if (ref.fileKey < 0 || ref.fileKey >= static_cast<int32_t>(m_summaries.size())) {
        return -1;
    }
    const auto& classIds = m_summaries[ref.fileKey].classIds;
    auto it = classIds.find(pathID);
    return (it != classIds.end()) ? it->second : -1;
}

std::string UnityPorter::MeshKey(const ObjectRef& ref) const {
    std::string key;
    if (ref.fileKey >= 0 && ref.fileKey < static_cast<int32_t>(m_summaries.size())) {
        key = ToLowerCopy(m_summaries[ref.fileKey].absPath);
    }
    key += "|";
    key += std::to_string(ref.pathId);
    return key;
}

int32_t UnityPorter::SummarizeFile(const std::string& absPathStr) {
    std::string absPath = CanonicalAbsPath(absPathStr);
    std::string pathLower = ToLowerCopy(absPath);

    auto itKey = m_summaryKeyByPath.find(pathLower);
    if (itKey != m_summaryKeyByPath.end()) {
        return itKey->second;
    }

    // Register the summary before any recursion so reference cycles terminate.
    // NOTE: m_summaries grows during recursion; never hold a reference into it
    // across a SummarizeFile call - always re-index via m_summaries[key].
    int32_t key = static_cast<int32_t>(m_summaries.size());
    m_summaries.emplace_back();
    m_summaries[key].absPath = absPath;
    m_summaryKeyByPath[pathLower] = key;

    // Keep globalgamemanagers entries empty to avoid excessive memory usage.
    if (pathLower.find("globalgamemanagers") != std::string::npos) {
        return key;
    }

    // Pass A: collect key-class object metadata and the externals list.
    std::vector<std::string> externalNames;
    try {
        UnityAssetParser parser;
        if (!parser.LoadFromFile(absPath)) {
            return key;
        }
        for (const auto& obj : parser.GetObjects()) {
            if (!IsKeyClass(obj.classId)) continue;
            m_summaries[key].classIds[obj.pathId] = obj.classId;
            if (obj.classId == UnityClass::Texture2D && !obj.name.empty()) {
                m_summaries[key].textureNames[obj.pathId] = obj.name;
            } else if (obj.classId == UnityClass::Mesh && !obj.name.empty()) {
                m_summaries[key].meshNames[obj.pathId] = obj.name;
            }
        }
        if (parser.HasExternalsTable()) {
            for (const auto& ext : parser.GetExternals()) {
                externalNames.push_back(ext.fileName);
            }
        }
    } catch (const std::exception& e) {
        VortigauntLog::LogF("EXCEPTION: Summarizing %s failed: %s\n", absPath.c_str(), e.what());
        return key;
    } catch (...) {
        return key;
    }

    // Resolve externals against regular files in the same directory (case-insensitive names).
    // Built-in references like "unity default resources" simply will not match and stay -1.
    ankerl::unordered_dense::map<std::string, std::string> siblingsByName;
    std::error_code ec;
    std::filesystem::path dir = std::filesystem::path(absPath).parent_path();
    if (std::filesystem::is_directory(dir, ec)) {
        for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
            if (!entry.is_regular_file()) continue;
            siblingsByName.emplace(ToLowerCopy(entry.path().filename().string()), entry.path().string());
        }
    }
    m_summaries[key].externalKeys.assign(externalNames.size(), -1);
    for (size_t i = 0; i < externalNames.size(); ++i) {
        if (externalNames[i].empty()) continue;
        auto itSibling = siblingsByName.find(ToLowerCopy(externalNames[i]));
        if (itSibling == siblingsByName.end()) continue;
        int32_t externalKey = SummarizeFile(itSibling->second); // may grow m_summaries
        m_summaries[key].externalKeys[i] = externalKey;
    }

    // Pass B: with externals resolved, record each Material's texture references.
    try {
        UnityAssetParser parser;
        if (!parser.LoadFromFile(absPath)) {
            return key;
        }
        parser.SetPPtrClassResolver([this, key](int32_t fileID, int64_t pathID) -> int32_t {
            return LookupClassId(key, fileID, pathID);
        });
        for (const auto& obj : parser.GetObjects()) {
            if (obj.classId != UnityClass::Material) continue;
            UnityValue val = parser.DeserializeObject(obj);
            if (val.type != UnityValue::ObjectVal) continue;
            UnityValue texEnvs = GetArrayField(val.GetField("m_SavedProperties"), "m_TexEnvs");
            if (texEnvs.type != UnityValue::ArrayVal) continue;
            for (const auto& kvPair : texEnvs.arrayVal) {
                const auto& texPtr = kvPair.GetField("second").GetField("m_Texture");
                int32_t fileID = static_cast<int32_t>(texPtr.GetField("m_FileID").AsInt());
                int64_t texPathId = texPtr.GetField("m_PathID").AsInt();
                ObjectRef texRef = ResolveRef(key, fileID, texPathId);
                if (texRef.pathId == 0 || texRef.fileKey < 0) continue;
                auto& refs = m_summaries[key].materialTextures[obj.pathId];
                bool alreadyKnown = false;
                for (const auto& r : refs) {
                    if (r.fileKey == texRef.fileKey && r.pathId == texRef.pathId) {
                        alreadyKnown = true;
                        break;
                    }
                }
                if (!alreadyKnown) {
                    refs.push_back(texRef);
                }
            }
        }
    } catch (const std::exception& e) {
        VortigauntLog::LogF("EXCEPTION: Material pass failed for %s: %s\n", absPath.c_str(), e.what());
    } catch (...) {}

    return key;
}

ankerl::unordered_dense::map<int64_t, std::vector<std::string>> UnityPorter::BuildDependencyMap(
    const UnityAssetParser& parser, int32_t mainKey) {

    ankerl::unordered_dense::map<int64_t, std::vector<std::string>> meshToTexturesMap;

    const auto& objects = parser.GetObjects();

    // Map from PathID to classId (local to current file)
    ankerl::unordered_dense::map<int64_t, int32_t> pathIdToClassId;
    ankerl::unordered_dense::set<int64_t> meshes;
    for (const auto& obj : objects) {
        pathIdToClassId[obj.pathId] = obj.classId;
        if (obj.classId == UnityClass::Mesh) {
            meshes.insert(obj.pathId);
        }
    }

    auto resolvePPtr = [this, mainKey](const UnityValue& pptr) -> ObjectRef {
        int32_t fileID = static_cast<int32_t>(pptr.GetField("m_FileID").AsInt());
        int64_t pathID = pptr.GetField("m_PathID").AsInt();
        return ResolveRef(mainKey, fileID, pathID);
    };

    auto appendUniqueRef = [](std::vector<ObjectRef>& refs, const ObjectRef& ref) {
        for (const auto& r : refs) {
            if (r.fileKey == ref.fileKey && r.pathId == ref.pathId) return;
        }
        refs.push_back(ref);
    };

    auto textureNameFor = [this](const ObjectRef& ref) -> std::string {
        if (ref.fileKey < 0 || ref.fileKey >= static_cast<int32_t>(m_summaries.size())) {
            return std::string();
        }
        const auto& names = m_summaries[ref.fileKey].textureNames;
        auto it = names.find(ref.pathId);
        return (it != names.end()) ? it->second : std::string();
    };

    // Reference structures
    struct MeshLink {
        ObjectRef meshRef;
        int64_t compPathId = 0;
    };
    ankerl::unordered_dense::map<std::string, MeshLink> meshLinks; // MeshKey(meshRef) -> owning component
    ankerl::unordered_dense::map<int64_t, int64_t> compToGo;
    ankerl::unordered_dense::map<int64_t, std::vector<int64_t>> goToGoComps;
    ankerl::unordered_dense::map<int64_t, std::vector<ObjectRef>> rendToMats;
    ankerl::unordered_dense::map<int64_t, std::vector<ObjectRef>> matToTexs; // local materials only

    auto linkComponentToGameObject = [&](const UnityValue& goPtr, int64_t compPathId) {
        // GameObject links stay local-only.
        if (goPtr.GetField("m_FileID").AsInt() != 0) return;
        int64_t goPathId = goPtr.GetField("m_PathID").AsInt();
        if (goPathId == 0) return;
        compToGo[compPathId] = goPathId;
        auto& comps = goToGoComps[goPathId];
        if (std::find(comps.begin(), comps.end(), compPathId) == comps.end()) {
            comps.push_back(compPathId);
        }
    };

    for (const auto& obj : objects) {
        if (obj.classId == UnityClass::GameObject) {
            UnityValue val = parser.DeserializeObject(obj);
            if (val.type != UnityValue::ObjectVal) continue;

            UnityValue m_Components = GetArrayField(val, "m_Component");
            if (m_Components.type != UnityValue::ArrayVal) {
                m_Components = GetArrayField(val, "m_Components");
            }
            if (m_Components.type != UnityValue::ArrayVal) continue;
            for (const auto& compPtrVal : m_Components.arrayVal) {
                const auto& nestedComp = compPtrVal.GetField("component");
                const UnityValue& compPtr = nestedComp.IsNull() ? compPtrVal : nestedComp;
                // GameObject-to-component links are always local; skip external ones.
                if (compPtr.GetField("m_FileID").AsInt() != 0) continue;
                int64_t compPathId = compPtr.GetField("m_PathID").AsInt();
                if (compPathId == 0) continue;

                auto itClass = pathIdToClassId.find(compPathId);
                if (itClass == pathIdToClassId.end()) continue;
                int32_t targetClass = itClass->second;
                if (targetClass == UnityClass::MeshFilter ||
                    targetClass == UnityClass::MeshRenderer ||
                    targetClass == UnityClass::Renderer ||
                    targetClass == UnityClass::SkinnedMeshRenderer) {
                    auto& comps = goToGoComps[obj.pathId];
                    if (std::find(comps.begin(), comps.end(), compPathId) == comps.end()) {
                        comps.push_back(compPathId);
                    }
                    compToGo[compPathId] = obj.pathId;
                }
            }
        }
        else if (obj.classId == UnityClass::MeshFilter) {
            UnityValue val = parser.DeserializeObject(obj);
            if (val.type != UnityValue::ObjectVal) continue;

            ObjectRef meshRef = resolvePPtr(val.GetField("m_Mesh"));
            if (meshRef.pathId != 0 && meshRef.fileKey >= 0) {
                meshLinks[MeshKey(meshRef)] = MeshLink{meshRef, obj.pathId};
            }
            linkComponentToGameObject(val.GetField("m_GameObject"), obj.pathId);
        }
        else if (obj.classId == UnityClass::MeshRenderer || obj.classId == UnityClass::Renderer) {
            UnityValue val = parser.DeserializeObject(obj);
            if (val.type != UnityValue::ObjectVal) continue;

            UnityValue materialsArray = GetArrayField(val, "m_Materials");
            if (materialsArray.type == UnityValue::ArrayVal) {
                for (const auto& matPtrVal : materialsArray.arrayVal) {
                    ObjectRef matRef = resolvePPtr(matPtrVal);
                    if (matRef.pathId != 0) {
                        appendUniqueRef(rendToMats[obj.pathId], matRef);
                    }
                }
            }
            linkComponentToGameObject(val.GetField("m_GameObject"), obj.pathId);
        }
        else if (obj.classId == UnityClass::SkinnedMeshRenderer) {
            UnityValue val = parser.DeserializeObject(obj);
            if (val.type != UnityValue::ObjectVal) continue;

            ObjectRef meshRef = resolvePPtr(val.GetField("m_Mesh"));
            if (meshRef.pathId != 0 && meshRef.fileKey >= 0) {
                meshLinks[MeshKey(meshRef)] = MeshLink{meshRef, obj.pathId};
            }
            UnityValue materialsArray = GetArrayField(val, "m_Materials");
            if (materialsArray.type == UnityValue::ArrayVal) {
                for (const auto& matPtrVal : materialsArray.arrayVal) {
                    ObjectRef matRef = resolvePPtr(matPtrVal);
                    if (matRef.pathId != 0) {
                        appendUniqueRef(rendToMats[obj.pathId], matRef);
                    }
                }
            }
            linkComponentToGameObject(val.GetField("m_GameObject"), obj.pathId);
        }
        else if (obj.classId == UnityClass::Material) {
            UnityValue val = parser.DeserializeObject(obj);
            if (val.type != UnityValue::ObjectVal) continue;

            UnityValue texEnvs = GetArrayField(val.GetField("m_SavedProperties"), "m_TexEnvs");
            if (texEnvs.type != UnityValue::ArrayVal) continue;
            for (const auto& kvPair : texEnvs.arrayVal) {
                ObjectRef texRef = resolvePPtr(kvPair.GetField("second").GetField("m_Texture"));
                if (texRef.pathId != 0 && texRef.fileKey >= 0) {
                    appendUniqueRef(matToTexs[obj.pathId], texRef);
                }
            }
        }
    }

    // Map each referenced Mesh (local or external) to texture names
    for (const auto& linkPair : meshLinks) {
        const ObjectRef& meshRef = linkPair.second.meshRef;
        int64_t compPathId = linkPair.second.compPathId;
        std::vector<std::string> texturesForThisMesh;

        auto itClass = pathIdToClassId.find(compPathId);
        if (itClass != pathIdToClassId.end()) {
            int32_t compClass = itClass->second;
            std::vector<ObjectRef> mats;

            if (compClass == UnityClass::SkinnedMeshRenderer) {
                auto itMats = rendToMats.find(compPathId);
                if (itMats != rendToMats.end()) {
                    mats = itMats->second;
                }
            }
            else if (compClass == UnityClass::MeshFilter) {
                auto itGo = compToGo.find(compPathId);
                if (itGo != compToGo.end()) {
                    auto itComps = goToGoComps.find(itGo->second);
                    if (itComps != goToGoComps.end()) {
                        for (int64_t siblingCompPathId : itComps->second) {
                            auto itSibClass = pathIdToClassId.find(siblingCompPathId);
                            if (itSibClass == pathIdToClassId.end()) continue;
                            if (itSibClass->second == UnityClass::MeshRenderer ||
                                itSibClass->second == UnityClass::Renderer) {
                                auto itMats = rendToMats.find(siblingCompPathId);
                                if (itMats != rendToMats.end()) {
                                    mats = itMats->second;
                                    break;
                                }
                            }
                        }
                    }
                }
            }

            // Map each material slot sequentially to its primary albedo texture
            for (const ObjectRef& matRef : mats) {
                std::vector<std::string> matTexNames;
                const std::vector<ObjectRef>* texRefs = nullptr;
                if (matRef.fileKey == mainKey) {
                    auto itTexs = matToTexs.find(matRef.pathId);
                    if (itTexs != matToTexs.end()) {
                        texRefs = &itTexs->second;
                    }
                } else if (matRef.fileKey >= 0 && matRef.fileKey < static_cast<int32_t>(m_summaries.size())) {
                    const auto& externalMats = m_summaries[matRef.fileKey].materialTextures;
                    auto itTexs = externalMats.find(matRef.pathId);
                    if (itTexs != externalMats.end()) {
                        texRefs = &itTexs->second;
                    }
                }
                if (texRefs) {
                    for (const ObjectRef& texRef : *texRefs) {
                        std::string texName = textureNameFor(texRef);
                        if (!texName.empty() && !IsUiTexture(texName)) {
                            matTexNames.push_back(texName);
                        }
                    }
                }
                texturesForThisMesh.push_back(SelectAlbedoTexture(matTexNames));
            }
        }

        const std::string& globalKey = linkPair.first;
        bool shouldStore = true;
        auto itExisting = m_meshTextures.find(globalKey);
        if (itExisting != m_meshTextures.end() &&
            HasValuableTextures(itExisting->second) && !HasValuableTextures(texturesForThisMesh)) {
            shouldStore = false;
        }
        if (shouldStore && !texturesForThisMesh.empty()) {
            m_meshTextures[globalKey] = texturesForThisMesh;
            if (meshRef.fileKey == mainKey) {
                meshToTexturesMap[meshRef.pathId] = texturesForThisMesh;
            }
            if (meshRef.fileKey >= 0 && meshRef.fileKey < static_cast<int32_t>(m_summaries.size())) {
                const auto& names = m_summaries[meshRef.fileKey].meshNames;
                auto itName = names.find(meshRef.pathId);
                if (itName != names.end() && !itName->second.empty()) {
                    m_meshNamesByKey[globalKey] = itName->second;
                }
            }
        }
    }

    // Enrich local mappings with accumulated ones if the local ones lack textures
    for (int64_t meshPathId : meshes) {
        auto itLocal = meshToTexturesMap.find(meshPathId);
        if (itLocal != meshToTexturesMap.end() && HasValuableTextures(itLocal->second)) {
            continue;
        }
        ObjectRef localRef;
        localRef.fileKey = mainKey;
        localRef.pathId = meshPathId;
        auto itGlobal = m_meshTextures.find(MeshKey(localRef));
        if (itGlobal != m_meshTextures.end() && HasValuableTextures(itGlobal->second)) {
            meshToTexturesMap[meshPathId] = itGlobal->second;
        }
    }

    return meshToTexturesMap;
}

void UnityPorter::WriteMeshTextureMap(const UnityAssetParser& parser,
                                      const ankerl::unordered_dense::map<int64_t, std::vector<std::string>>& meshToTextures,
                                      int32_t mainKey,
                                      const std::string& outputDir) {
    std::filesystem::path mapPath = std::filesystem::path(outputDir) / "vortigaunt_mesh_texture_map.txt";
    std::ofstream ofs(mapPath, std::ios::binary);
    if (!ofs) return;

    ofs << "=== VORTIGAUNT MESH TO TEXTURE MAPPING ===\n\n";

    ankerl::unordered_dense::map<int64_t, std::string> pathIdToName;
    for (const auto& obj : parser.GetObjects()) {
        if (obj.classId == UnityClass::Mesh) {
            pathIdToName[obj.pathId] = obj.name;
        }
    }

    for (const auto& pair : meshToTextures) {
        std::string meshName = pathIdToName[pair.first];
        if (meshName.empty()) {
            meshName = "Unknown_" + std::to_string(pair.first);
        }
        ofs << "Mesh: '" << meshName << "' -> Textures: ";
        for (const auto& texName : pair.second) {
            ofs << "'" << texName << "' ";
        }
        ofs << "\n";
    }

    std::string mainKeyPrefix;
    if (mainKey >= 0 && mainKey < static_cast<int32_t>(m_summaries.size())) {
        mainKeyPrefix = ToLowerCopy(m_summaries[mainKey].absPath) + "|";
    }
    for (const auto& pair : m_meshTextures) {
        if (!mainKeyPrefix.empty() && pair.first.starts_with(mainKeyPrefix)) continue;

        std::string meshName;
        auto itName = m_meshNamesByKey.find(pair.first);
        if (itName != m_meshNamesByKey.end()) {
            meshName = itName->second;
        }
        if (meshName.empty()) {
            meshName = "Unknown_" + pair.first;
        }
        ofs << "Mesh (Global): '" << meshName << "' -> Textures: ";
        for (const auto& texName : pair.second) {
            ofs << "'" << texName << "' ";
        }
        ofs << "\n";
    }

    ofs.close();
    VortigauntLog::LogF("Mesh-Texture mapping written to: %s\n", mapPath.string().c_str());
}

void UnityPorter::DumpDiagnostics(const UnityAssetParser& parser,
                                  const ankerl::unordered_dense::map<int64_t, std::vector<std::string>>& meshToTextures,
                                  int32_t mainKey,
                                  const std::string& outputDir) {
    const char* envDebug = std::getenv("VORTIGAUNT_DEBUG");
    if (!envDebug || std::strcmp(envDebug, "1") != 0) {
        return;
    }

    std::filesystem::path debugPath = std::filesystem::path(outputDir) / "vortigaunt_dependency_debug.txt";
    std::ofstream ofs(debugPath);
    if (!ofs) return;

    ofs << "=== VORTIGAUNT DEPENDENCY DIAGNOSTICS ===\n\n";

    const auto& objects = parser.GetObjects();
    ofs << "Total Objects parsed: " << objects.size() << "\n\n";

    ankerl::unordered_dense::map<int64_t, const UnityObjectInfo*> objByPathId;
    ankerl::unordered_dense::map<int64_t, std::string> textureNames;
    std::vector<int64_t> meshes;
    std::vector<int64_t> gameObjects;
    std::vector<int64_t> materials;
    std::vector<int64_t> meshFilters;
    std::vector<int64_t> meshRenderers;
    std::vector<int64_t> skinnedMeshRenderers;

    for (const auto& obj : objects) {
        objByPathId[obj.pathId] = &obj;
        if (obj.classId == UnityClass::Texture2D) {
            textureNames[obj.pathId] = obj.name;
        } else if (obj.classId == UnityClass::Mesh) {
            meshes.push_back(obj.pathId);
        } else if (obj.classId == UnityClass::GameObject) {
            gameObjects.push_back(obj.pathId);
        } else if (obj.classId == UnityClass::Material) {
            materials.push_back(obj.pathId);
        } else if (obj.classId == UnityClass::MeshFilter) {
            meshFilters.push_back(obj.pathId);
        } else if (obj.classId == UnityClass::MeshRenderer || obj.classId == UnityClass::Renderer) {
            meshRenderers.push_back(obj.pathId);
        } else if (obj.classId == UnityClass::SkinnedMeshRenderer) {
            skinnedMeshRenderers.push_back(obj.pathId);
        }
    }

    ofs << "--- Objects Count ---\n";
    ofs << "GameObjects (1): " << gameObjects.size() << "\n";
    ofs << "Materials (21): " << materials.size() << "\n";
    ofs << "Texture2Ds (28): " << textureNames.size() << "\n";
    ofs << "Meshes (43): " << meshes.size() << "\n";
    ofs << "MeshFilters (33): " << meshFilters.size() << "\n";
    ofs << "MeshRenderers (23/25): " << meshRenderers.size() << "\n";
    ofs << "SkinnedMeshRenderers (137): " << skinnedMeshRenderers.size() << "\n\n";

    // Trace GameObject parsing
    ofs << "--- GameObject Components Parsing ---\n";
    for (int64_t goPathId : gameObjects) {
        auto itObj = objByPathId.find(goPathId);
        if (itObj == objByPathId.end()) continue;
        UnityValue val = parser.DeserializeObject(*itObj->second);
        ofs << "GO [" << goPathId << "] Name: '" << val.GetField("m_Name").AsString() << "'\n";
        UnityValue m_Components = GetArrayField(val, "m_Component");
        if (m_Components.type != UnityValue::ArrayVal) {
            m_Components = GetArrayField(val, "m_Components");
        }
        if (m_Components.type == UnityValue::ArrayVal) {
            ofs << "  m_Component count: " << m_Components.arrayVal.size() << "\n";
            for (const auto& compPtrVal : m_Components.arrayVal) {
                int64_t compPathId = 0;
                const auto& nestedComp = compPtrVal.GetField("component");
                if (!nestedComp.IsNull()) {
                    compPathId = nestedComp.GetField("m_PathID").AsInt();
                    ofs << "    Component (Nested): PathID " << compPathId << "\n";
                } else {
                    compPathId = compPtrVal.GetField("m_PathID").AsInt();
                    ofs << "    Component (Direct): PathID " << compPathId << "\n";
                }
            }
        } else {
            ofs << "  m_Component field is NOT ArrayVal! (Type: " << m_Components.type << ")\n";
            if (val.type == UnityValue::ObjectVal) {
                ofs << "    Available fields in GameObject:\n";
                for (const auto& pair : val.objectVal) {
                    ofs << "      " << pair.first << " (Type: " << pair.second.type << ")\n";
                }
            }
        }
    }
    ofs << "\n";

    // Trace MeshFilters
    ofs << "--- MeshFilter Parsing ---\n";
    for (int64_t mfPathId : meshFilters) {
        auto itObj = objByPathId.find(mfPathId);
        if (itObj == objByPathId.end()) continue;
        UnityValue val = parser.DeserializeObject(*itObj->second);
        ofs << "MeshFilter [" << mfPathId << "]: ";
        DumpValue(ofs, "  ", val);
        ofs << "\n";
    }
    ofs << "\n";

    // Trace MeshRenderers
    ofs << "--- MeshRenderer Parsing ---\n";
    for (int64_t mrPathId : meshRenderers) {
        auto itObj = objByPathId.find(mrPathId);
        if (itObj == objByPathId.end()) continue;
        UnityValue val = parser.DeserializeObject(*itObj->second);
        ofs << "MeshRenderer [" << mrPathId << "]: ";
        DumpValue(ofs, "  ", val);
        ofs << "\n";
    }
    ofs << "\n";

    // Trace SkinnedMeshRenderers
    ofs << "--- SkinnedMeshRenderer Parsing ---\n";
    for (int64_t smrPathId : skinnedMeshRenderers) {
        auto itObj = objByPathId.find(smrPathId);
        if (itObj == objByPathId.end()) continue;
        UnityValue val = parser.DeserializeObject(*itObj->second);
        ofs << "SkinnedMeshRenderer [" << smrPathId << "]: ";
        DumpValue(ofs, "  ", val);
        ofs << "\n";
    }
    ofs << "\n";

    // Trace Material parsing
    ofs << "--- Material Texture Environments Parsing ---\n";
    for (int64_t matPathId : materials) {
        auto itObj = objByPathId.find(matPathId);
        if (itObj == objByPathId.end()) continue;
        UnityValue val = parser.DeserializeObject(*itObj->second);
        ofs << "Material [" << matPathId << "] Name: '" << val.GetField("m_Name").AsString() << "'\n";

        const auto& savedProp = val.GetField("m_SavedProperties");
        ofs << "  m_SavedProperties Type: " << savedProp.type << "\n";
        if (savedProp.type == UnityValue::ObjectVal) {
            UnityValue texEnvs = GetArrayField(savedProp, "m_TexEnvs");
            if (texEnvs.type == UnityValue::ArrayVal) {
                ofs << "  m_TexEnvs count: " << texEnvs.arrayVal.size() << "\n";
                for (const auto& kvPair : texEnvs.arrayVal) {
                    const auto& texPtr = kvPair.GetField("second").GetField("m_Texture");
                    int64_t texPathId = texPtr.GetField("m_PathID").AsInt();
                    auto itTexName = textureNames.find(texPathId);
                    ofs << "    Texture PathID: " << texPathId << " Name: '"
                        << (itTexName != textureNames.end() ? itTexName->second : std::string()) << "'\n";
                }
            } else {
                ofs << "  m_TexEnvs is NOT ArrayVal! (Type: " << texEnvs.type << ")\n";
                ofs << "    Available fields under m_SavedProperties:\n";
                for (const auto& pair : savedProp.objectVal) {
                    ofs << "      " << pair.first << " (Type: " << pair.second.type << ")\n";
                }
            }
        }
    }
    ofs << "\n";

    // Trace parsed Meshes
    ofs << "--- Parsed Meshes ---\n";
    for (int64_t meshPathId : meshes) {
        auto itObj = objByPathId.find(meshPathId);
        std::string meshName = (itObj != objByPathId.end() && !itObj->second->name.empty())
            ? itObj->second->name : std::string("Unknown");
        ofs << "Mesh [" << meshPathId << "] Name: '" << meshName << "'\n";
    }
    ofs << "\n";

    // Trace final resolved mapping (both local and accumulated)
    ofs << "--- Final Resolved Mesh to Textures Mapping (Local & Global) ---\n";
    for (const auto& pair : meshToTextures) {
        auto itObj = objByPathId.find(pair.first);
        std::string meshName = (itObj != objByPathId.end() && !itObj->second->name.empty())
            ? itObj->second->name : std::string("Unknown");
        ofs << "Mesh [" << pair.first << "] Name: '" << meshName << "' -> Textures: ";
        for (const auto& texName : pair.second) {
            ofs << "'" << texName << "' ";
        }
        ofs << "\n";
    }
    std::string mainKeyPrefix;
    if (mainKey >= 0 && mainKey < static_cast<int32_t>(m_summaries.size())) {
        mainKeyPrefix = ToLowerCopy(m_summaries[mainKey].absPath) + "|";
    }
    for (const auto& pair : m_meshTextures) {
        if (!mainKeyPrefix.empty() && pair.first.starts_with(mainKeyPrefix)) continue;
        std::string meshName = "Unknown";
        auto itName = m_meshNamesByKey.find(pair.first);
        if (itName != m_meshNamesByKey.end() && !itName->second.empty()) {
            meshName = itName->second;
        }
        ofs << "Mesh [" << pair.first << "] Name: '" << meshName << "' (Global) -> Textures: ";
        for (const auto& texName : pair.second) {
            ofs << "'" << texName << "' ";
        }
        ofs << "\n";
    }
    ofs << "\n";
    ofs.close();
}

void UnityPorter::ProcessBundle(const std::string& bundlePath, const std::string& outputDir)
{
    // Load the UnityFS bundle
    UnityFsExtractor extractor;
    if (!extractor.Load(bundlePath))
    {
        VortigauntLog::LogF("ERROR: Failed to load UnityFS bundle: %s\n", bundlePath.c_str());
        return;
    }

    // Create a per-bundle temporary extraction directory inside the output folder.
    // The name must be unique within one Process() run: reusing a single path
    // would let m_summaryKeyByPath return an earlier bundle's cached summary for
    // a same-named file extracted from a later bundle at the same absolute path.
    std::filesystem::path tempDir = std::filesystem::path(outputDir) /
        ("unity_extracted_" + std::to_string(m_bundleExtractCounter++));
    std::error_code ec;
    std::filesystem::create_directories(tempDir, ec);
    if (ec)
    {
        VortigauntLog::LogF("ERROR: Could not create temp directory %s: %s\n", tempDir.string().c_str(), ec.message().c_str());
        return;
    }

    // Extract all files from the bundle
    if (!extractor.ExtractAll(tempDir.string()))
    {
        VortigauntLog::LogF("ERROR: UnityFS extraction failed for %s\n", bundlePath.c_str());
        std::filesystem::remove_all(tempDir, ec);
        return;
    }

    // Walk the extracted directory and process any .assets files
    for (const auto& entry : std::filesystem::recursive_directory_iterator(tempDir))
    {
        if (!entry.is_regular_file())
            continue;
        std::string ext = ToLowerCopy(entry.path().extension().string());
        if (ext == ".assets" || IsSerializedAssetFile(entry.path().string()))
        {
            ProcessAssetFile(entry.path().string(), outputDir);
        }
    }

    // Clean up temporary extraction directory to free disk space
    std::filesystem::remove_all(tempDir, ec);

    VortigauntLog::LogF("UnityFS bundle processed successfully.\n");
}

void UnityPorter::Process(const std::string& inputPathStr, const std::string& outputDirStr)
{
    std::string inputPath = NormalizePath(inputPathStr);
    std::string outputDir = NormalizePath(outputDirStr);
    VortigauntLog::LogF("UnityPorter::Process starting: inputPath = %s, outputDir = %s\n", inputPath.c_str(), outputDir.c_str());

    ClearRegistries();

    if (std::filesystem::is_directory(inputPath))
    {
        VortigauntLog::LogF("Processing Unity files in directory: %s\n", inputPath.c_str());
        // Walk directory and find all .unity3d, .bundle, and .assets files
        for (const auto& entry : std::filesystem::recursive_directory_iterator(inputPath))
        {
            if (!entry.is_regular_file())
                continue;
            std::string compPath = NormalizePath(entry.path().string());
            std::string ext = ToLowerCopy(entry.path().extension().string());
            if (ext == ".unity3d" || ext == ".bundle")
            {
                VortigauntLog::LogF("Processing bundle: %s\n", compPath.c_str());
                ProcessBundle(compPath, outputDir);
            }
            else if (ext == ".assets" || IsSerializedAssetFile(compPath))
            {
                VortigauntLog::LogF("Processing asset file: %s\n", compPath.c_str());
                ProcessAssetFile(compPath, outputDir);
            }
        }
    }
    else
    {
        std::filesystem::path p(inputPath);
        std::string ext = ToLowerCopy(p.extension().string());
        VortigauntLog::LogF("UnityPorter::Process file: extension = %s\n", ext.c_str());

        if (ext == ".assets" || IsSerializedAssetFile(inputPath))
        {
            ProcessAssetFile(inputPath, outputDir);
        }
        else
        {
            ProcessBundle(inputPath, outputDir);
        }
    }

    // Free accumulated summaries and mappings
    ClearRegistries();
}

void UnityPorter::ProcessAssetFile(const std::string& assetPathStr, const std::string& outputDirStr)
{
    std::string assetPath = NormalizePath(assetPathStr);
    std::string outputDir = NormalizePath(outputDirStr);
    VortigauntLog::LogF("UnityPorter::ProcessAssetFile started: %s\n", assetPath.c_str());

    // Prevent attempting to parse huge globalgamemanagers assets which can cause memory blow-up
    if (ToLowerCopy(assetPath).find("globalgamemanagers") != std::string::npos) {
        VortigauntLog::LogF("WARNING: Skipping parsing of globalgamemanagers asset to avoid excessive memory usage.\n");
        return;
    }

    // Summarize this file and everything reachable through its externals table
    int32_t mainKey = SummarizeFile(assetPath);
    VortigauntLog::LogF("Summarized %zu asset file(s) for cross-file reference resolution.\n", m_summaries.size());

    std::filesystem::path p(assetPath);
    std::filesystem::path assetDir = p.parent_path();
    std::vector<std::string> dataExtensions = {".data", ".resource", ".resS", ".sharedassets", ".globalgamemanagers"};
    std::vector<std::string> allFiles = { assetPath };

    // Scan directory for companion files
    if (std::filesystem::exists(assetDir) && std::filesystem::is_directory(assetDir)) {
        for (const auto& compEntry : std::filesystem::directory_iterator(assetDir)) {
            if (!compEntry.is_regular_file()) continue;
            std::string compPath = NormalizePath(compEntry.path().string());
            std::string compExt = ToLowerCopy(compEntry.path().extension().string());
            if (std::find(dataExtensions.begin(), dataExtensions.end(), compExt) != dataExtensions.end()) {
                allFiles.push_back(compPath);
            }
        }
    }

    VortigauntLog::LogF("Files to load: %zu. Base directory: %s\n", allFiles.size(), assetDir.string().c_str());

    UnityDataResolver dataResolver;
    // Load companion data files into resolver
    dataResolver.LoadDirectory(assetDir.string(), {".data", ".resource", ".resS", ".sharedassets", ".globalgamemanagers", ""});

    UnityAssetParser parser;
    parser.SetDataResolver(&dataResolver);
    parser.SetPPtrClassResolver([this, mainKey](int32_t fileID, int64_t pathID) -> int32_t {
        return LookupClassId(mainKey, fileID, pathID);
    });

    // Ensure output directory exists
    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);
    if (ec) {
        VortigauntLog::LogF("ERROR: Could not create output directory %s: %s\n", outputDir.c_str(), ec.message().c_str());
        return;
    }
    // Load asset file with exception safety
    try {
        VortigauntLog::LogF("Calling parser.LoadFromFile...\n");
        if (!parser.LoadFromFile(assetPath)) {
            VortigauntLog::LogF("ERROR: Failed to load Unity asset file %s\n", assetPath.c_str());
            return;
        }
    } catch (const std::exception& e) {
        VortigauntLog::LogF("EXCEPTION: LoadFromFile threw for %s: %s\n", assetPath.c_str(), e.what());
        return;
    }
    // Extraction mode: copy original asset and companion files to output directory
    VortigauntLog::LogF("Successfully loaded asset file. Extracting raw files...\n");
    for (const auto& srcPath : allFiles) {
        try {
            std::filesystem::path src(srcPath);
            std::filesystem::path dst = std::filesystem::path(outputDir) / src.filename();
            std::filesystem::copy_file(src, dst,
                std::filesystem::copy_options::overwrite_existing);
            VortigauntLog::LogF("Extracted %s -> %s\n", src.string().c_str(), dst.string().c_str());
        } catch (const std::exception& e) {
            VortigauntLog::LogF("ERROR: Failed to copy %s: %s\n", srcPath.c_str(), e.what());
        }
    }

    // Build dependency mapping
    auto meshToTextures = BuildDependencyMap(parser, mainKey);
    DumpDiagnostics(parser, meshToTextures, mainKey, outputDir);
    WriteMeshTextureMap(parser, meshToTextures, mainKey, outputDir);

    // Convert assets (Mesh, Texture2D); AnimationClip conversion is not supported
    for (const auto& objInfo : parser.GetObjects())
    {
        if (objInfo.classId == UnityClass::Mesh)
        {
            VortigauntLog::LogF("Detected Mesh object (PathID %lld)\n", static_cast<long long>(objInfo.pathId));
            std::vector<std::string> textures;
            ObjectRef meshRef;
            meshRef.fileKey = mainKey;
            meshRef.pathId = objInfo.pathId;
            auto itGlobal = m_meshTextures.find(MeshKey(meshRef));
            if (itGlobal != m_meshTextures.end()) {
                textures = itGlobal->second;
            } else {
                auto it = meshToTextures.find(objInfo.pathId);
                if (it != meshToTextures.end()) {
                    textures = it->second;
                }
            }
            UnityMeshConverter::Convert(objInfo, parser, outputDir, textures, m_convertTexturesToBmp);
        }
        else if (objInfo.classId == UnityClass::Texture2D)
        {
            VortigauntLog::LogF("Detected Texture2D object (PathID %lld)\n", static_cast<long long>(objInfo.pathId));
            UnityTextureDecoder::Convert(objInfo, parser, outputDir, m_convertTexturesToBmp);
        }
    }
}
