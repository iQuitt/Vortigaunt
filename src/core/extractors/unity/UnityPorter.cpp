#include "UnityPorter.h"
#include "UnityDataResolver.h"
#include "VortigauntLog.h"
#include "UnityMeshConverter.h"
#include "UnityTextureDecoder.h"
#include "utils/BinaryUtils.h"
// #include "UnityAnimationExtractor.h"
#include <algorithm>
#include <filesystem>
#include <ankerl/unordered_dense.h>
#include <vector>
#include <cstring>
#include <cstdlib>

static std::string MakeGlobalKey(const std::string& assetPath, int64_t pathId) {
    try {
        return std::filesystem::absolute(assetPath).make_preferred().string() + "_" + std::to_string(pathId);
    } catch (...) {
        return assetPath + "_" + std::to_string(pathId);
    }
}

static std::string NormalizePath(const std::string& pathStr) {
    try {
        return std::filesystem::path(pathStr).make_preferred().string();
    } catch (...) {
        return pathStr;
    }
}



static bool IsUiTexture(const std::string& name) {
    std::string lower = name;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
    
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
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find(keyword) != std::string::npos) {
                return name;
            }
        }
    }

    static const std::vector<std::string> utilityKeywords = {
        "normal", "n_opengl", "_n", "mask", "roughness", "metallic", "emission", "emissive", "_e", "height", "specular", "ao", "gloss"
    };
    for (const auto& name : cleanNames) {
        std::string lower = name;
        std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
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

#include <fstream>
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



#include <fstream>
#include <unordered_set>

void UnityPorter::WriteMeshTextureMap(const UnityAssetParser& parser,
                                const ankerl::unordered_dense::map<int64_t, std::vector<std::string>>& meshToTextures,
                                const std::string& outputDir) {
    std::filesystem::path mapPath = std::filesystem::path(outputDir) / "vortigaunt_mesh_texture_map.txt";
    std::ofstream ofs(mapPath, std::ios::binary);
    if (!ofs) return;

    ofs << "=== VORTIGAUNT MESH TO TEXTURE MAPPING ===\n\n";

    ankerl::unordered_dense::map<int64_t, std::string> pathIdToName;
    for (const auto& obj : parser.GetObjects()) {
        if (obj.classId == 43) { // Mesh
            pathIdToName[obj.pathId] = obj.name;
        }
    }

    ankerl::unordered_dense::set<int64_t> outputtedMeshes;
    for (const auto& pair : meshToTextures) {
        std::string meshName = pathIdToName[pair.first];
        if (meshName.empty()) {
            meshName = "Unknown_" + std::to_string(pair.first);
        }
        ofs << "Mesh: '" << meshName << "' -> Textures [VORTIGAUNT_TEST_STAMP]: ";
        for (const auto& texName : pair.second) {
            ofs << "'" << texName << "' ";
        }
        ofs << "\n";
        outputtedMeshes.insert(pair.first);
    }

    for (const auto& pair : m_meshToTextures) {
        size_t underscore = pair.first.find_last_of('_');
        if (underscore == std::string::npos) continue;
        int64_t pathId = std::stoll(pair.first.substr(underscore + 1));
        if (outputtedMeshes.count(pathId)) continue;
        
        std::string meshName = "";
        auto itName = m_meshNames.find(pair.first);
        if (itName != m_meshNames.end()) {
            meshName = itName->second;
        } else {
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

    // Map path ID to class ID for debugging
    ankerl::unordered_dense::map<int64_t, int32_t> pathIdToClassId;
    ankerl::unordered_dense::map<int64_t, std::string> textureNames;
    std::vector<int64_t> meshes;
    std::vector<int64_t> gameObjects;
    std::vector<int64_t> materials;
    std::vector<int64_t> meshFilters;
    std::vector<int64_t> meshRenderers;
    std::vector<int64_t> skinnedMeshRenderers;

    for (const auto& obj : objects) {
        pathIdToClassId[obj.pathId] = obj.classId;
        if (obj.classId == 28) {
            textureNames[obj.pathId] = obj.name;
        } else if (obj.classId == 43) {
            meshes.push_back(obj.pathId);
        } else if (obj.classId == 1) {
            gameObjects.push_back(obj.pathId);
        } else if (obj.classId == 21) {
            materials.push_back(obj.pathId);
        } else if (obj.classId == 33) {
            meshFilters.push_back(obj.pathId);
        } else if (obj.classId == 23 || obj.classId == 25) {
            meshRenderers.push_back(obj.pathId);
        } else if (obj.classId == 137) {
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
        // Find GameObject object
        for (const auto& obj : objects) {
            if (obj.pathId == goPathId) {
                UnityValue val = parser.DeserializeObject(obj);
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
                break;
            }
        }
    }
    ofs << "\n";

    // Trace MeshFilters
    ofs << "--- MeshFilter Parsing ---\n";
    for (int64_t mfPathId : meshFilters) {
        for (const auto& obj : objects) {
            if (obj.pathId == mfPathId) {
                UnityValue val = parser.DeserializeObject(obj);
                ofs << "MeshFilter [" << mfPathId << "]: ";
                DumpValue(ofs, "  ", val);
                ofs << "\n";
                break;
            }
        }
    }
    ofs << "\n";

    // Trace MeshRenderers
    ofs << "--- MeshRenderer Parsing ---\n";
    for (int64_t mrPathId : meshRenderers) {
        for (const auto& obj : objects) {
            if (obj.pathId == mrPathId) {
                UnityValue val = parser.DeserializeObject(obj);
                ofs << "MeshRenderer [" << mrPathId << "]: ";
                DumpValue(ofs, "  ", val);
                ofs << "\n";
                break;
            }
        }
    }
    ofs << "\n";

    // Trace SkinnedMeshRenderers
    ofs << "--- SkinnedMeshRenderer Parsing ---\n";
    for (int64_t smrPathId : skinnedMeshRenderers) {
        for (const auto& obj : objects) {
            if (obj.pathId == smrPathId) {
                UnityValue val = parser.DeserializeObject(obj);
                ofs << "SkinnedMeshRenderer [" << smrPathId << "]: ";
                DumpValue(ofs, "  ", val);
                ofs << "\n";
                break;
            }
        }
    }
    ofs << "\n";

    // Trace Material parsing
    ofs << "--- Material Texture Environments Parsing ---\n";
    for (int64_t matPathId : materials) {
        for (const auto& obj : objects) {
            if (obj.pathId == matPathId) {
                UnityValue val = parser.DeserializeObject(obj);
                ofs << "Material [" << matPathId << "] Name: '" << val.GetField("m_Name").AsString() << "'\n";
                
                const auto& savedProp = val.GetField("m_SavedProperties");
                ofs << "  m_SavedProperties Type: " << savedProp.type << "\n";
                if (savedProp.type == UnityValue::ObjectVal) {
                    UnityValue texEnvs = GetArrayField(savedProp, "m_TexEnvs");
                    if (texEnvs.type == UnityValue::ArrayVal) {
                        ofs << "  m_TexEnvs count: " << texEnvs.arrayVal.size() << "\n";
                        for (const auto& kvPair : texEnvs.arrayVal) {
                            const auto& envVal = kvPair.GetField("second");
                            int64_t texPathId = envVal.GetField("m_Texture").GetField("m_PathID").AsInt();
                            ofs << "    Texture PathID: " << texPathId << " Name: '" << textureNames[texPathId] << "'\n";
                        }
                    } else {
                        ofs << "  m_TexEnvs is NOT ArrayVal! (Type: " << texEnvs.type << ")\n";
                        ofs << "    Available fields under m_SavedProperties:\n";
                        for (const auto& pair : savedProp.objectVal) {
                            ofs << "      " << pair.first << " (Type: " << pair.second.type << ")\n";
                        }
                    }
                }
                break;
            }
        }
    }
    ofs << "\n";

    // Trace parsed Meshes
    ofs << "--- Parsed Meshes ---\n";
    for (int64_t meshPathId : meshes) {
        std::string meshName = "Unknown";
        for (const auto& obj : objects) {
            if (obj.pathId == meshPathId) {
                meshName = obj.name;
                break;
            }
        }
        ofs << "Mesh [" << meshPathId << "] Name: '" << meshName << "'\n";
    }
    ofs << "\n";

    // Trace final resolved mapping (both local and global)
    ofs << "--- Final Resolved Mesh to Textures Mapping (Local & Global) ---\n";
    ankerl::unordered_dense::set<int64_t> outputtedMeshes;
    for (const auto& pair : meshToTextures) {
        std::string meshName = "Unknown";
        for (const auto& obj : objects) {
            if (obj.pathId == pair.first) {
                meshName = obj.name;
                break;
            }
        }
        ofs << "Mesh [" << pair.first << "] Name: '" << meshName << "' -> Textures: ";
        for (const auto& texName : pair.second) {
            ofs << "'" << texName << "' ";
        }
        ofs << "\n";
        outputtedMeshes.insert(pair.first);
    }
    for (const auto& pair : m_meshToTextures) {
        size_t underscore = pair.first.find_last_of('_');
        if (underscore == std::string::npos) continue;
        int64_t pathId = std::stoll(pair.first.substr(underscore + 1));
        if (outputtedMeshes.count(pathId)) continue;
        
        std::string meshName = "Unknown";
        for (const auto& obj : objects) {
            if (obj.pathId == pathId) {
                meshName = obj.name;
                break;
            }
        }
        ofs << "Mesh [" << pathId << "] Name: '" << meshName << "' (Global) -> Textures: ";
        for (const auto& texName : pair.second) {
            ofs << "'" << texName << "' ";
        }
        ofs << "\n";
    }
    ofs << "\n";
    ofs.close();
}

static bool HasValuableTextures(const std::vector<std::string>& texs) {
    for (const auto& t : texs) {
        if (!t.empty()) return true;
    }
    return false;
}

ankerl::unordered_dense::map<int64_t, std::vector<std::string>> UnityPorter::BuildDependencyMap(
    const UnityAssetParser& parser,
    const std::string& assetPath,
    const std::vector<std::string>& companionFiles) {

    ankerl::unordered_dense::map<int64_t, std::vector<std::string>> meshToTexturesMap;

    const auto& objects = parser.GetObjects();

    // Map from PathID to classId (local to current file)
    ankerl::unordered_dense::map<int64_t, int32_t> pathIdToClassId;
    // Map from PathID to Texture Name (local to current file)
    ankerl::unordered_dense::map<int64_t, std::string> textureNames;
    ankerl::unordered_dense::set<int64_t> meshes;

    for (const auto& obj : objects) {
        pathIdToClassId[obj.pathId] = obj.classId;
        if (obj.classId == 28) {
            textureNames[obj.pathId] = obj.name;
            if (!obj.name.empty()) {
                m_textureNames[MakeGlobalKey(assetPath, obj.pathId)] = obj.name;
            }
        } else if (obj.classId == 43) {
            meshes.insert(obj.pathId);
        }
    }

    // Reference structures
    ankerl::unordered_dense::map<int64_t, int64_t> meshToFilterOrSMR;
    ankerl::unordered_dense::map<int64_t, int64_t> compToGo;
    ankerl::unordered_dense::map<int64_t, std::vector<int64_t>> goToGoComps;
    ankerl::unordered_dense::map<int64_t, std::vector<int64_t>> rendToMats;
    ankerl::unordered_dense::map<int64_t, std::vector<int64_t>> matToTexs;

    for (const auto& obj : objects) {
        if (obj.classId == 1) { // GameObject
            UnityValue val = parser.DeserializeObject(obj);
            if (val.type != UnityValue::ObjectVal) continue;
            
            UnityValue m_Components = GetArrayField(val, "m_Component");
            if (m_Components.type != UnityValue::ArrayVal) {
                m_Components = GetArrayField(val, "m_Components");
            }
            if (m_Components.type == UnityValue::ArrayVal) {
                for (const auto& compPtrVal : m_Components.arrayVal) {
                    int64_t compPathId = 0;
                    const auto& nestedComp = compPtrVal.GetField("component");
                    if (!nestedComp.IsNull()) {
                        compPathId = nestedComp.GetField("m_PathID").AsInt();
                    } else {
                        compPathId = compPtrVal.GetField("m_PathID").AsInt();
                    }
                    
                    if (compPathId != 0) {
                        auto itClass = pathIdToClassId.find(compPathId);
                        if (itClass != pathIdToClassId.end()) {
                            int32_t targetClass = itClass->second;
                            if (targetClass == 33 || targetClass == 23 || targetClass == 25 || targetClass == 137) {
                                auto& comps = goToGoComps[obj.pathId];
                                if (std::find(comps.begin(), comps.end(), compPathId) == comps.end()) {
                                    comps.push_back(compPathId);
                                }
                                compToGo[compPathId] = obj.pathId;
                            }
                        }
                    }
                }
            }
        }
        else if (obj.classId == 33) { // MeshFilter
            UnityValue val = parser.DeserializeObject(obj);
            if (val.type != UnityValue::ObjectVal) continue;
            
            int64_t goPathId = val.GetField("m_GameObject").GetField("m_PathID").AsInt();
            int64_t meshPathId = val.GetField("m_Mesh").GetField("m_PathID").AsInt();
            
            if (meshPathId != 0) {
                meshToFilterOrSMR[meshPathId] = obj.pathId;
            }
            if (goPathId != 0) {
                compToGo[obj.pathId] = goPathId;
                auto& comps = goToGoComps[goPathId];
                if (std::find(comps.begin(), comps.end(), obj.pathId) == comps.end()) {
                    comps.push_back(obj.pathId);
                }
            }
        }
        else if (obj.classId == 23 || obj.classId == 25) { // MeshRenderer
            UnityValue val = parser.DeserializeObject(obj);
            if (val.type != UnityValue::ObjectVal) continue;
            
            int64_t goPathId = val.GetField("m_GameObject").GetField("m_PathID").AsInt();
            UnityValue materialsArray = GetArrayField(val, "m_Materials");
            if (materialsArray.type == UnityValue::ArrayVal) {
                for (const auto& matPtrVal : materialsArray.arrayVal) {
                    int64_t matPathId = matPtrVal.GetField("m_PathID").AsInt();
                    if (matPathId != 0) {
                        auto& mats = rendToMats[obj.pathId];
                        if (std::find(mats.begin(), mats.end(), matPathId) == mats.end()) {
                            mats.push_back(matPathId);
                        }
                    }
                }
            }
            if (goPathId != 0) {
                compToGo[obj.pathId] = goPathId;
                auto& comps = goToGoComps[goPathId];
                if (std::find(comps.begin(), comps.end(), obj.pathId) == comps.end()) {
                    comps.push_back(obj.pathId);
                }
            }
        }
        else if (obj.classId == 137) { // SkinnedMeshRenderer
            UnityValue val = parser.DeserializeObject(obj);
            if (val.type != UnityValue::ObjectVal) continue;
            
            int64_t goPathId = val.GetField("m_GameObject").GetField("m_PathID").AsInt();
            int64_t meshPathId = val.GetField("m_Mesh").GetField("m_PathID").AsInt();
            UnityValue materialsArray = GetArrayField(val, "m_Materials");
            
            if (meshPathId != 0) {
                meshToFilterOrSMR[meshPathId] = obj.pathId;
            }
            if (materialsArray.type == UnityValue::ArrayVal) {
                for (const auto& matPtrVal : materialsArray.arrayVal) {
                    int64_t matPathId = matPtrVal.GetField("m_PathID").AsInt();
                    if (matPathId != 0) {
                        auto& mats = rendToMats[obj.pathId];
                        if (std::find(mats.begin(), mats.end(), matPathId) == mats.end()) {
                            mats.push_back(matPathId);
                        }
                    }
                }
            }
            if (goPathId != 0) {
                compToGo[obj.pathId] = goPathId;
                auto& comps = goToGoComps[goPathId];
                if (std::find(comps.begin(), comps.end(), obj.pathId) == comps.end()) {
                    comps.push_back(obj.pathId);
                }
            }
        }
        else if (obj.classId == 21) { // Material
            UnityValue val = parser.DeserializeObject(obj);
            if (val.type != UnityValue::ObjectVal) continue;
            
            UnityValue texEnvs = GetArrayField(val.GetField("m_SavedProperties"), "m_TexEnvs");
            if (texEnvs.type == UnityValue::ArrayVal) {
                for (const auto& kvPair : texEnvs.arrayVal) {
                    const auto& envVal = kvPair.GetField("second");
                    int64_t texPathId = envVal.GetField("m_Texture").GetField("m_PathID").AsInt();
                    if (texPathId != 0) {
                        auto& texs = matToTexs[obj.pathId];
                        if (std::find(texs.begin(), texs.end(), texPathId) == texs.end()) {
                            texs.push_back(texPathId);
                        }
                        
                        std::string texName = "";
                        auto itName = textureNames.find(texPathId);
                        if (itName != textureNames.end()) {
                            texName = itName->second;
                        } else {
                            std::string suffix = "_" + std::to_string(texPathId);
                            for (const auto& pair : m_textureNames) {
                                if (pair.first.size() >= suffix.size() &&
                                    pair.first.compare(pair.first.size() - suffix.size(), suffix.size(), suffix) == 0) {
                                    texName = pair.second;
                                    break;
                                }
                            }
                        }
                        if (!texName.empty()) {
                            std::string matKey = MakeGlobalKey(assetPath, obj.pathId);
                            auto& globalTexs = m_matToTexs[matKey];
                            if (std::find(globalTexs.begin(), globalTexs.end(), texName) == globalTexs.end()) {
                                globalTexs.push_back(texName);
                            }
                        }
                    }
                }
            }
        }
    }

    // Map each Mesh (both local and external referenced) to Texture names
    ankerl::unordered_dense::set<int64_t> processedMeshes;
    for (const auto& pair : meshToFilterOrSMR) {
        int64_t meshPathId = pair.first;
        processedMeshes.insert(meshPathId);
        std::vector<std::string> texturesForThisMesh;

        int64_t compPathId = pair.second;
        auto itClass = pathIdToClassId.find(compPathId);
        if (itClass != pathIdToClassId.end()) {
            int32_t compClass = itClass->second;
            std::vector<int64_t> mats;

            if (compClass == 137) { // SkinnedMeshRenderer
                auto itMats = rendToMats.find(compPathId);
                if (itMats != rendToMats.end()) {
                    mats = itMats->second;
                }
            }
            else if (compClass == 33) { // MeshFilter
                auto itGo = compToGo.find(compPathId);
                if (itGo != compToGo.end()) {
                    int64_t goPathId = itGo->second;
                    auto itComps = goToGoComps.find(goPathId);
                    if (itComps != goToGoComps.end()) {
                        for (int64_t siblingCompPathId : itComps->second) {
                            auto itSibClass = pathIdToClassId.find(siblingCompPathId);
                            if (itSibClass != pathIdToClassId.end()) {
                                int32_t sibClass = itSibClass->second;
                                if (sibClass == 23 || sibClass == 25) { // MeshRenderer
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
            }

            // Map each material slot sequentially to its primary Albedo texture
            for (int64_t matPathId : mats) {
                std::vector<std::string> matTexNames;
                bool foundLocal = false;
                auto itTexs = matToTexs.find(matPathId);
                if (itTexs != matToTexs.end() && !itTexs->second.empty()) {
                    foundLocal = true;
                    for (int64_t texPathId : itTexs->second) {
                        std::string texName = "";
                        auto itName = textureNames.find(texPathId);
                        if (itName != textureNames.end() && !itName->second.empty()) {
                            texName = itName->second;
                        } else {
                            std::string suffix = "_" + std::to_string(texPathId);
                            for (const auto& pair : m_textureNames) {
                                if (pair.first.size() >= suffix.size() &&
                                    pair.first.compare(pair.first.size() - suffix.size(), suffix.size(), suffix) == 0) {
                                    texName = pair.second;
                                    break;
                                }
                            }
                        }
                        if (!texName.empty() && !IsUiTexture(texName)) {
                            matTexNames.push_back(texName);
                        }
                    }
                }
                if (!foundLocal) {
                    std::string suffix = "_" + std::to_string(matPathId);
                    for (const auto& gp : m_matToTexs) {
                        if (gp.first.size() >= suffix.size() &&
                            gp.first.compare(gp.first.size() - suffix.size(), suffix.size(), suffix) == 0) {
                            for (const auto& texName : gp.second) {
                                if (!texName.empty() && !IsUiTexture(texName)) {
                                    matTexNames.push_back(texName);
                                }
                            }
                            break;
                        }
                    }
                }

                std::string selectedAlbedo = SelectAlbedoTexture(matTexNames);
                texturesForThisMesh.push_back(selectedAlbedo);
            }
        }

        std::string globalKey = FindMeshGlobalKey(meshPathId, assetPath);
        bool shouldStore = true;
        auto itExisting = m_meshToTextures.find(globalKey);
        if (itExisting != m_meshToTextures.end()) {
            if (HasValuableTextures(itExisting->second) && !HasValuableTextures(texturesForThisMesh)) {
                shouldStore = false;
            }
        }
        
        if (shouldStore && !texturesForThisMesh.empty()) {
            meshToTexturesMap[meshPathId] = texturesForThisMesh;
            m_meshToTextures[globalKey] = texturesForThisMesh;
        }
    }

    // Enrich local mappings with global ones if the local ones are empty or non-existent
    for (int64_t meshPathId : meshes) {
        bool needsEnrichment = true;
        auto itLocal = meshToTexturesMap.find(meshPathId);
        if (itLocal != meshToTexturesMap.end() && HasValuableTextures(itLocal->second)) {
            needsEnrichment = false;
        }
        
        if (needsEnrichment) {
            std::string globalKey = FindMeshGlobalKey(meshPathId, assetPath);
            auto itGlobal = m_meshToTextures.find(globalKey);
            if (itGlobal != m_meshToTextures.end() && HasValuableTextures(itGlobal->second)) {
                meshToTexturesMap[meshPathId] = itGlobal->second;
            }
        }
    }

    return meshToTexturesMap;
}

static bool AreSiblingAssetsRelated(const std::string& inputPath, const std::string& siblingPath) {
    std::filesystem::path pIn(inputPath);
    std::filesystem::path pSib(siblingPath);
    std::string nameIn = pIn.stem().string();
    std::string nameSib = pSib.stem().string();
    
    std::transform(nameIn.begin(), nameIn.end(), nameIn.begin(), ::tolower);
    std::transform(nameSib.begin(), nameSib.end(), nameSib.begin(), ::tolower);
    
    // globalgamemanagers and boot.config never have mesh/texture relation to levels
    if (nameIn.find("globalgamemanagers") != std::string::npos || nameIn.find("boot.config") != std::string::npos) {
        return false;
    }
    
    // resources is always a shared asset file that can be related
    if (nameSib.find("resources") != std::string::npos || nameSib.find("globalgamemanagers") != std::string::npos) {
        return true;
    }
    
    // Extract number suffix from both names
    auto extractNum = [](const std::string& s) -> std::string {
        std::string num;
        for (char c : s) {
            if (std::isdigit(static_cast<unsigned char>(c))) {
                num.push_back(c);
            }
        }
        return num;
    };
    
    std::string numIn = extractNum(nameIn);
    std::string numSib = extractNum(nameSib);
    
    // If both have numbers and they don't match, they are completely unrelated!
    if (!numIn.empty() && !numSib.empty() && numIn != numSib) {
        return false;
    }
    
    return true;
}

static bool IsSerializedAssetFile(const std::string& filepath) {
    std::filesystem::path p(filepath);
    std::string ext = p.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    // Only allow real Unity serialized asset extensions or extensionless level/resources files
    if (ext != ".assets" && ext != ".sharedassets" && ext != ".globalgamemanagers" && !ext.empty()) {
        return false;
    }

    std::ifstream f(filepath, std::ios::binary);
    if (!f) return false;
    char header[16];
    f.read(header, 16);
    if (f.gcount() < 16) return false;
    
    // Read bytes 8-11 as big-endian uint32 (formatVersion)
    std::uint32_t version = Utils::ReadBE32(reinterpret_cast<const std::uint8_t*>(header + 8));
    return (version >= 9);
}

static bool IsKeyClass(int32_t classId) {
    return classId == 1 ||   // GameObject
           classId == 21 ||  // Material
           classId == 23 ||  // MeshRenderer
           classId == 25 ||  // Renderer
           classId == 28 ||  // Texture2D
           classId == 33 ||  // MeshFilter
           classId == 43 ||  // Mesh
           classId == 137 || // SkinnedMeshRenderer
           classId == 74;    // AnimationClip
}

void UnityPorter::PreParseAssetMetadata(const std::string& assetPathStr) {
    std::string assetPath = NormalizePath(assetPathStr);
    std::string lowerPath = assetPath;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
    if (lowerPath.find("globalgamemanagers") != std::string::npos) {
        return;
    }

    std::string doneKey = assetPath + "_preparsed";
    if (m_preParsedFiles.count(doneKey)) return;
    m_preParsedFiles.insert(doneKey);

    UnityAssetParser parser;
    try {
        if (parser.LoadFromFile(assetPath)) {
            const auto& objects = parser.GetObjects();
            for (const auto& obj : objects) {
                if (IsKeyClass(obj.classId)) {
                    std::string key = MakeGlobalKey(assetPath, obj.pathId);
                    m_pathIdToClassId[key] = obj.classId;
                    m_pathIdToClassIdInt[obj.pathId] = obj.classId;
                    if (obj.classId == 28 && !obj.name.empty()) {
                        m_textureNames[key] = obj.name;
                    }
                    if (obj.classId == 43 && !obj.name.empty()) {
                        m_meshNames[key] = obj.name;
                    }
                    if (obj.classId == 21) { // Material
                        // Gecikmeli çözümleme (Lazy Resolution): Ham PathID'leri topla
                        UnityValue val = parser.DeserializeObject(obj);
                        if (val.type == UnityValue::ObjectVal) {
                            UnityValue texEnvs = GetArrayField(val.GetField("m_SavedProperties"), "m_TexEnvs");
                            if (texEnvs.type == UnityValue::ArrayVal) {
                                std::string matKey = MakeGlobalKey(assetPath, obj.pathId);
                                auto& pathIds = m_matToTexPathIds[matKey];
                                for (const auto& kvPair : texEnvs.arrayVal) {
                                    const auto& envVal = kvPair.GetField("second");
                                    int64_t texPathId = envVal.GetField("m_Texture").GetField("m_PathID").AsInt();
                                    if (texPathId != 0) {
                                        if (std::find(pathIds.begin(), pathIds.end(), texPathId) == pathIds.end()) {
                                            pathIds.push_back(texPathId);
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    } catch (...) {}
}

std::string UnityPorter::FindMeshGlobalKey(int64_t meshPathId, const std::string& currentAssetPath) {
    std::string suffix = "_" + std::to_string(meshPathId);
    for (const auto& pair : m_meshNames) {
        if (pair.first.size() >= suffix.size() &&
            pair.first.compare(pair.first.size() - suffix.size(), suffix.size(), suffix) == 0) {
            return pair.first;
        }
    }
    return MakeGlobalKey(currentAssetPath, meshPathId);
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

    // Create a temporary extraction directory inside the output folder
    std::filesystem::path tempDir = std::filesystem::path(outputDir) / "unity_extracted";
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
        return;
    }

    // Walk the extracted directory and process any .assets files
    for (const auto& entry : std::filesystem::recursive_directory_iterator(tempDir))
    {
        if (!entry.is_regular_file())
            continue;
        std::string ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
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
    
    // Clear member registries
    m_meshToTextures.clear();
    m_textureNames.clear();
    m_meshNames.clear();
    m_pathIdToClassId.clear();
    m_pathIdToClassIdInt.clear();
    m_matToTexs.clear();
    m_matToTexPathIds.clear();
    m_preParsedFiles.clear();

    if (std::filesystem::is_directory(inputPath))
    {
        VortigauntLog::LogF("Processing Unity files in directory: %s\n", inputPath.c_str());
        // Walk directory and find all .unity3d, .bundle, and .assets files
        for (const auto& entry : std::filesystem::recursive_directory_iterator(inputPath))
        {
            if (!entry.is_regular_file())
                continue;
            std::string compPath = NormalizePath(entry.path().string());
            std::string ext = entry.path().extension().string();
            std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
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
        std::string ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
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

    // Clear member registries at the end to free memory
    m_meshToTextures.clear();
    m_textureNames.clear();
    m_meshNames.clear();
    m_pathIdToClassId.clear();
    m_pathIdToClassIdInt.clear();
    m_matToTexs.clear();
    m_matToTexPathIds.clear();
    m_preParsedFiles.clear();
}

void UnityPorter::ProcessAssetFile(const std::string& assetPathStr, const std::string& outputDirStr)
{
    std::string assetPath = NormalizePath(assetPathStr);
    std::string outputDir = NormalizePath(outputDirStr);
    VortigauntLog::LogF("UnityPorter::ProcessAssetFile started: %s\n", assetPath.c_str());
    
    // Direct top-level call memory safety & state isolation
    bool isTopLevelCall = m_preParsedFiles.empty();
    if (isTopLevelCall) {
        m_meshToTextures.clear();
        m_textureNames.clear();
        m_meshNames.clear();
        m_pathIdToClassId.clear();
        m_pathIdToClassIdInt.clear();
        m_matToTexs.clear();
        m_matToTexPathIds.clear();
        m_preParsedFiles.clear();
    }
    
    std::filesystem::path p(assetPath);
    std::filesystem::path assetDir = p.parent_path();
    std::vector<std::string> dataExtensions = {".data", ".resource", ".resS", ".sharedassets", ".globalgamemanagers"};
    std::vector<std::string> allFiles = { assetPath };
    
    // Scan directory for companion files
    if (std::filesystem::exists(assetDir) && std::filesystem::is_directory(assetDir)) {
        for (const auto& compEntry : std::filesystem::directory_iterator(assetDir)) {
            if (!compEntry.is_regular_file()) continue;
            std::string compPath = NormalizePath(compEntry.path().string());
            std::string compExt = compEntry.path().extension().string();
            std::transform(compExt.begin(), compExt.end(), compExt.begin(), ::tolower);
            if (std::find(dataExtensions.begin(), dataExtensions.end(), compExt) != dataExtensions.end()) {
                allFiles.push_back(compPath);
            }
        }
    }

    // Determine companion files for pre-parsing (only related files)
    std::vector<std::string> assetsToPreParse = { assetPath };
    if (std::filesystem::exists(assetDir) && std::filesystem::is_directory(assetDir)) {
        for (const auto& entry : std::filesystem::directory_iterator(assetDir)) {
            if (!entry.is_regular_file()) continue;
            std::string compPath = NormalizePath(entry.path().string());
            if (compPath != assetPath && IsSerializedAssetFile(compPath)) {
                // Pre-parse all companion files to guarantee absolute cross-file reference resolution
                assetsToPreParse.push_back(compPath);
            }
        }
    }

    VortigauntLog::LogF("Pre-parsing %zu companion files for %s...\n", assetsToPreParse.size(), p.filename().string().c_str());

    // Single-pass: Gather PathIDs, ClassIDs, and Material raw PathIDs globally
    for (const auto& assetFile : assetsToPreParse) {
        PreParseAssetMetadata(assetFile);
    }

    // Gecikmeli Çözümleme (Lazy Resolution): Tüm ham Material-to-Texture PathID'lerini isimlere çöz
    for (const auto& [matKey, pathIds] : m_matToTexPathIds) {
        auto& texNames = m_matToTexs[matKey];
        for (int64_t texPathId : pathIds) {
            std::string texName = "";
            std::string suffix = "_" + std::to_string(texPathId);
            for (const auto& pair : m_textureNames) {
                if (pair.first.size() >= suffix.size() &&
                    pair.first.compare(pair.first.size() - suffix.size(), suffix.size(), suffix) == 0) {
                    texName = pair.second;
                    break;
                }
            }
            if (!texName.empty()) {
                if (std::find(texNames.begin(), texNames.end(), texName) == texNames.end()) {
                    texNames.push_back(texName);
                }
            }
        }
    }

    VortigauntLog::LogF("Files to load: %zu. Base directory: %s\n", allFiles.size(), assetDir.string().c_str());

    // Prevent attempting to parse huge globalgamemanagers assets which can cause memory blow‑up
    std::string lowerPath = assetPath;
    std::transform(lowerPath.begin(), lowerPath.end(), lowerPath.begin(), ::tolower);
    if (lowerPath.find("globalgamemanagers") != std::string::npos) {
        VortigauntLog::LogF("WARNING: Skipping parsing of globalgamemanagers asset to avoid excessive memory usage.\n");
        return;
    }

    ankerl::unordered_dense::map<int64_t, std::string> dummyTextureMap;

    UnityDataResolver dataResolver;
    UnityAssetParser parser;
    parser.SetPathIdToClassMap(&m_pathIdToClassIdInt);
    parser.SetTextureNamesMap(&dummyTextureMap);
    // Load companion data files into resolver
    dataResolver.LoadDirectory(assetDir.string(), {".data", ".resource", ".resS", ".sharedassets", ".globalgamemanagers", ""});
    // Provide resolver to parser
    parser.SetDataResolver(&dataResolver);
    
    // Ensure output directory exists
    std::error_code ec;
    std::filesystem::create_directories(outputDir, ec);
    if (ec) {
        VortigauntLog::LogF("ERROR: Could not create output directory %s: %s\n", outputDir.c_str(), ec.message().c_str());
        return;
    }
    // Load asset files with exception safety
    try {
        VortigauntLog::LogF("Calling parser.LoadFromMultipleFiles...\n");
        if (!parser.LoadFromMultipleFiles(allFiles)) {
            VortigauntLog::LogF("ERROR: Failed to load Unity asset file %s\n", assetPath.c_str());
            return;
        }
    } catch (const std::exception& e) {
        VortigauntLog::LogF("EXCEPTION: LoadFromMultipleFiles threw for %s: %s\n", assetPath.c_str(), e.what());
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
    auto meshToTextures = BuildDependencyMap(parser, assetPath, allFiles);
    DumpDiagnostics(parser, meshToTextures, outputDir);
    WriteMeshTextureMap(parser, meshToTextures, outputDir);

    // Convert assets (Mesh, Texture2D, AnimationClip)
    const auto& objects = parser.GetObjects();
    for (const auto& objInfo : objects)
    {
        if (objInfo.classId == 43) // Mesh
        {
            VortigauntLog::LogF("Detected Mesh object (PathID %lld)\n", static_cast<long long>(objInfo.pathId));
            std::vector<std::string> textures;
            std::string meshKey = MakeGlobalKey(assetPath, objInfo.pathId);
            auto itGlobal = m_meshToTextures.find(meshKey);
            if (itGlobal != m_meshToTextures.end()) {
                textures = itGlobal->second;
            } else {
                auto it = meshToTextures.find(objInfo.pathId);
                if (it != meshToTextures.end()) {
                    textures = it->second;
                }
            }
            UnityMeshConverter::Convert(objInfo, parser, outputDir, textures);
        }
        else if (objInfo.classId == 28) // Texture2D
        {
            VortigauntLog::LogF("Detected Texture2D object (PathID %lld)\n", static_cast<long long>(objInfo.pathId));
            UnityTextureDecoder::Convert(objInfo, parser, outputDir);
        }
    }

    if (isTopLevelCall) {
        m_meshToTextures.clear();
        m_textureNames.clear();
        m_meshNames.clear();
        m_pathIdToClassId.clear();
        m_pathIdToClassIdInt.clear();
        m_matToTexs.clear();
        m_matToTexPathIds.clear();
        m_preParsedFiles.clear();
    }
}
