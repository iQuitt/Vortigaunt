#include "UnityAssetParser.h"
#include "core/VortigauntLog.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstring>
#include <unordered_map>



static const std::unordered_map<std::uint32_t, std::string> g_commonString = {
    {0, "AABB"}, {5, "AnimationClip"}, {19, "AnimationCurve"}, {34, "AnimationState"},
    {49, "Array"}, {55, "Base"}, {60, "BitField"}, {69, "bitset"}, {76, "bool"},
    {81, "char"}, {86, "ColorRGBA"}, {96, "Component"}, {106, "data"}, {111, "deque"},
    {117, "double"}, {124, "dynamic_array"}, {138, "FastPropertyName"}, {155, "first"},
    {161, "float"}, {167, "Font"}, {172, "GameObject"}, {183, "Generic Mono"},
    {196, "GradientNEW"}, {208, "GUID"}, {213, "GUIStyle"}, {222, "int"}, {226, "list"},
    {231, "long long"}, {241, "map"}, {245, "Matrix4x4f"}, {256, "MdFour"},
    {263, "MonoBehaviour"}, {277, "MonoScript"}, {288, "m_ByteSize"}, {299, "m_Curve"},
    {307, "m_EditorClassIdentifier"}, {331, "m_EditorHideFlags"}, {349, "m_Enabled"},
    {359, "m_ExtensionPtr"}, {374, "m_GameObject"}, {387, "m_Index"}, {395, "m_IsArray"},
    {405, "m_IsStatic"}, {416, "m_MetaFlag"}, {427, "m_Name"}, {434, "m_ObjectHideFlags"},
    {452, "m_PrefabInternal"}, {469, "m_PrefabParentObject"}, {490, "m_Script"},
    {499, "m_StaticEditorFlags"}, {519, "m_Type"}, {526, "m_Version"}, {536, "Object"},
    {543, "pair"}, {548, "PPtr<Component>"}, {564, "PPtr<GameObject>"}, {581, "PPtr<Material>"},
    {596, "PPtr<MonoBehaviour>"}, {616, "PPtr<MonoScript>"}, {633, "PPtr<Object>"},
    {646, "PPtr<Prefab>"}, {659, "PPtr<Sprite>"}, {672, "PPtr<TextAsset>"},
    {688, "PPtr<Texture>"}, {702, "PPtr<Texture2D>"}, {718, "PPtr<Transform>"},
    {734, "Prefab"}, {741, "Quaternionf"}, {753, "Rectf"}, {759, "RectInt"},
    {767, "RectOffset"}, {778, "second"}, {785, "set"}, {789, "short"}, {795, "size"},
    {800, "SInt16"}, {807, "SInt32"}, {814, "SInt64"}, {821, "SInt8"}, {827, "staticvector"},
    {840, "string"}, {847, "TextAsset"}, {857, "TextMesh"}, {866, "Texture"},
    {874, "Texture2D"}, {884, "Transform"}, {894, "TypelessData"}, {907, "UInt16"},
    {914, "UInt32"}, {921, "UInt64"}, {928, "UInt8"}, {934, "unsigned int"},
    {947, "unsigned long long"}, {966, "unsigned short"}, {981, "vector"},
    {988, "Vector2f"}, {997, "Vector3f"}, {1006, "Vector4f"}, {1015, "m_ScriptingClassIdentifier"},
    {1042, "Gradient"}, {1051, "Type*"}, {1057, "int2_storage"}, {1070, "int3_storage"},
    {1083, "BoundsInt"}, {1093, "m_CorrespondingSourceObject"}, {1121, "m_PrefabInstance"},
    {1138, "m_PrefabAsset"}, {1152, "FileSize"}
};

#include "utils/BinaryUtils.h"
#include "UnityClassIds.h"

using Utils::membuf;

// ---------------------------------------------------------------------------
// File-local helpers for the manual structural scanners
// ---------------------------------------------------------------------------

// Resolves a PPtr candidate to a classId; encapsulates local vs. cross-file lookup.
using ClassIdResolver = std::function<int32_t(int32_t fileID, int64_t pathID)>;

static void ReadRawPPtr(const char* dataPtr, size_t offset, bool bigEndian, int32_t& fileID, int64_t& pathID) {
    std::uint32_t rawFileId = 0;
    std::uint64_t rawPathId = 0;
    std::memcpy(&rawFileId, dataPtr + offset, 4);
    std::memcpy(&rawPathId, dataPtr + offset + 4, 8);
    if (bigEndian) {
        rawFileId = Utils::Swap32(rawFileId);
        rawPathId = Utils::Swap64(rawPathId);
    }
    fileID = static_cast<int32_t>(rawFileId);
    pathID = static_cast<int64_t>(rawPathId);
}

static UnityValue MakePPtrValue(int32_t fileID, int64_t pathID) {
    UnityValue v;
    v.type = UnityValue::ObjectVal;
    v.objectVal["m_FileID"].type = UnityValue::IntVal;
    v.objectVal["m_FileID"].intVal = fileID;
    v.objectVal["m_PathID"].type = UnityValue::IntVal;
    v.objectVal["m_PathID"].intVal = pathID;
    return v;
}

// Scans the payload for an int32 count followed by `count` contiguous Material
// PPtrs; returns the array on the first full match, empty otherwise.
static std::vector<UnityValue> ScanMaterialPPtrArray(const char* dataPtr, size_t dataSize, size_t startOffset,
                                                     bool bigEndian, int32_t maxFileId,
                                                     const ClassIdResolver& resolveClass) {
    for (size_t offset = startOffset; offset + 4 <= dataSize; offset += 4) {
        std::uint32_t rawCount = 0;
        std::memcpy(&rawCount, dataPtr + offset, 4);
        if (bigEndian) rawCount = Utils::Swap32(rawCount);
        std::int32_t count = static_cast<std::int32_t>(rawCount);
        if (count < 1 || count > 16) continue;
        if (offset + 4 + static_cast<size_t>(count) * 12 > dataSize) continue;

        bool allValid = true;
        std::vector<UnityValue> mats;
        for (std::int32_t i = 0; i < count; ++i) {
            std::int32_t fileID = 0;
            std::int64_t pathID = 0;
            ReadRawPPtr(dataPtr, offset + 4 + static_cast<size_t>(i) * 12, bigEndian, fileID, pathID);
            if (fileID < 0 || fileID > maxFileId || pathID == 0 ||
                resolveClass(fileID, pathID) != UnityClass::Material) {
                allValid = false;
                break;
            }
            mats.push_back(MakePPtrValue(fileID, pathID));
        }
        if (allValid) {
            return mats;
        }
    }
    return {};
}

// Scans the payload for the first PPtr resolving to `targetClassId`; NullVal if none.
static UnityValue ScanSinglePPtrOfClass(const char* dataPtr, size_t dataSize, size_t startOffset,
                                        bool bigEndian, int32_t maxFileId, int32_t targetClassId,
                                        const ClassIdResolver& resolveClass) {
    for (size_t offset = startOffset; offset + 12 <= dataSize; offset += 4) {
        std::int32_t fileID = 0;
        std::int64_t pathID = 0;
        ReadRawPPtr(dataPtr, offset, bigEndian, fileID, pathID);
        if (fileID >= 0 && fileID <= maxFileId && pathID != 0 &&
            resolveClass(fileID, pathID) == targetClassId) {
            return MakePPtrValue(fileID, pathID);
        }
    }
    return UnityValue{};
}

static UnityValue PackedFloatToValue(PackedFloatVector&& pfv) {
    UnityValue v;
    v.type = UnityValue::ObjectVal;
    v.objectVal["m_NumItems"].type = UnityValue::UIntVal;
    v.objectVal["m_NumItems"].uintVal = pfv.numItems;
    v.objectVal["m_Range"].type = UnityValue::FloatVal;
    v.objectVal["m_Range"].doubleVal = pfv.range;
    v.objectVal["m_Start"].type = UnityValue::FloatVal;
    v.objectVal["m_Start"].doubleVal = pfv.start;
    v.objectVal["m_Data"].type = UnityValue::ByteArrayVal;
    v.objectVal["m_Data"].byteData = std::move(pfv.data);
    v.objectVal["m_BitSize"].type = UnityValue::UIntVal;
    v.objectVal["m_BitSize"].uintVal = pfv.bitSize;
    return v;
}

static UnityValue PackedIntToValue(PackedIntVector&& piv) {
    UnityValue v;
    v.type = UnityValue::ObjectVal;
    v.objectVal["m_NumItems"].type = UnityValue::UIntVal;
    v.objectVal["m_NumItems"].uintVal = piv.numItems;
    v.objectVal["m_Data"].type = UnityValue::ByteArrayVal;
    v.objectVal["m_Data"].byteData = std::move(piv.data);
    v.objectVal["m_BitSize"].type = UnityValue::UIntVal;
    v.objectVal["m_BitSize"].uintVal = piv.bitSize;
    return v;
}

bool UnityAssetParser::Load(const std::vector<char>& data) {
    std::vector<char> copy = data;
    return Load(std::move(copy));
}

bool UnityAssetParser::Load(std::vector<char>&& data) {
    m_fileData = std::move(data);
    m_fileSize = m_fileData.size();
    membuf buf(m_fileData.data(), m_fileData.size());
    std::istream in(&buf);
    in.seekg(0);
    if (!ParseHeader(in)) {
        VortigauntLog::LogF("^1Error:^7 Failed to parse Unity asset header.\n");
        return false;
    }
    if (!ParseMetadata(in)) {
        VortigauntLog::LogF("^1Error:^7 Failed to parse Unity asset metadata.\n");
        return false;
    }

    // Resolve name field for all key objects (only exportable assets)
    for (auto& obj : m_objects) {
        if (obj.classId == UnityClass::Mesh || obj.classId == UnityClass::Texture2D || obj.classId == UnityClass::AnimationClip) {
            obj.name = GetObjectName(obj);
        }
    }

    return true;
}

bool UnityAssetParser::LoadFromFile(const std::string& filePath) {
    std::ifstream f(filePath, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    std::size_t sz = static_cast<std::size_t>(f.tellg());
    f.seekg(0, std::ios::beg);
    std::vector<char> buffer(sz);
    f.read(buffer.data(), sz);
    return Load(std::move(buffer));
}

bool UnityAssetParser::ParseHeader(std::istream& in) {
    std::uint32_t metadataSizeVal = 0;
    std::uint32_t fileSizeVal32 = 0;
    std::uint32_t versionVal = 0;
    std::uint32_t dataOffsetVal32 = 0;

    if (!in.read(reinterpret_cast<char*>(&metadataSizeVal), 4) ||
        !in.read(reinterpret_cast<char*>(&fileSizeVal32), 4) ||
        !in.read(reinterpret_cast<char*>(&versionVal), 4) ||
        !in.read(reinterpret_cast<char*>(&dataOffsetVal32), 4)) {
        return false;
    }

    metadataSizeVal = Utils::Swap32(metadataSizeVal);
    fileSizeVal32 = Utils::Swap32(fileSizeVal32);
    versionVal = Utils::Swap32(versionVal);
    dataOffsetVal32 = Utils::Swap32(dataOffsetVal32);

    if (versionVal >= 22) {
        m_formatVersion = versionVal;

        // Stream is currently at offset 16
        std::uint8_t endianFlag = 0;
        if (!in.read(reinterpret_cast<char*>(&endianFlag), 1)) return false;
        m_endianness = endianFlag;

        in.seekg(3, std::ios::cur); // skip reserved (3 bytes at offset 17-19)

        std::uint32_t actualMetadataSize = 0;
        if (!in.read(reinterpret_cast<char*>(&actualMetadataSize), 4)) return false;
        m_metadataSize = Utils::Swap32(actualMetadataSize);

        std::uint64_t actualFileSize = 0;
        if (!in.read(reinterpret_cast<char*>(&actualFileSize), 8)) return false;
        m_fileSize = Utils::Swap64(actualFileSize);

        std::uint64_t actualDataOffset = 0;
        if (!in.read(reinterpret_cast<char*>(&actualDataOffset), 8)) return false;
        m_dataOffset = Utils::Swap64(actualDataOffset);

        std::uint64_t unknown = 0;
        if (!in.read(reinterpret_cast<char*>(&unknown), 8)) return false; // skip unknown (8 bytes at offset 40-47)
    } else {
        m_metadataSize = metadataSizeVal;
        m_fileSize = fileSizeVal32;
        m_formatVersion = versionVal;
        m_dataOffset = dataOffsetVal32;

        if (m_formatVersion >= 9) {
            std::uint8_t endianFlag = 0;
            if (!in.read(reinterpret_cast<char*>(&endianFlag), 1)) return false;
            m_endianness = endianFlag;
            in.seekg(3, std::ios::cur); // reserved
        } else {
            m_endianness = 1; // version < 9 is big-endian
        }
    }

    if (m_formatVersion >= 7) {
        m_unityVersion = Utils::ReadNullTerminatedString(in);
    }
    if (m_formatVersion >= 8) {
        m_targetPlatform = ReadU32(in);
    }
    if (m_formatVersion >= 13) {
        std::uint8_t enableTypeTreeVal = 0;
        if (!in.read(reinterpret_cast<char*>(&enableTypeTreeVal), 1)) return false;
        m_enableTypeTree = (enableTypeTreeVal != 0);
    }

    VortigauntLog::LogF("[UnityAssetParser] Header parsed: version=%d, size=%llu, metadataSize=%llu, offset=%llu, endianness=%d, unityVersion=%s, platform=%u, typeTree=%d\n",
        m_formatVersion, m_fileSize, m_metadataSize, m_dataOffset, (int)m_endianness, m_unityVersion.c_str(), m_targetPlatform, (int)m_enableTypeTree);

    return true;
}

bool UnityAssetParser::ParseMetadata(std::istream& in) {
    VortigauntLog::LogF("[UnityAssetParser] ParseMetadata started. formatVersion=%d, offset=%lld\n", m_formatVersion, static_cast<long long>(in.tellg()));
    m_externals.clear();
    m_externalsParsed = false;
    if (m_formatVersion >= 17) {
        if (!ParseTypesV17(in)) {
            VortigauntLog::LogF("[UnityAssetParser] ParseMetadata: ParseTypesV17 failed!\n");
            return false;
        }

        if (!ParseObjectsV17(in)) {
            VortigauntLog::LogF("[UnityAssetParser] ParseMetadata: ParseObjectsV17 failed!\n");
            return false;
        }

        if (ParseScriptTypesAndExternals(in)) {
            m_externalsParsed = true;
        } else {
            VortigauntLog::LogF("[UnityAssetParser] Warning: failed to parse script types/externals table; cross-file PPtr resolution unavailable.\n");
            m_externals.clear();
            in.clear();
        }
    } else {
        // Fallback for version < 17: object table first, then types
        if (!ParseObjectsV17(in)) {
            VortigauntLog::LogF("[UnityAssetParser] ParseMetadata: ParseObjectsV17 (version < 17) failed!\n");
            return false;
        }
        if (m_enableTypeTree) {
            if (!ParseTypesV17(in)) {
                VortigauntLog::LogF("[UnityAssetParser] ParseMetadata: ParseTypesV17 (version < 17) failed!\n");
                return false;
            }
        }
    }
    VortigauntLog::LogF("[UnityAssetParser] ParseMetadata completed successfully. Offset=%lld\n", static_cast<long long>(in.tellg()));
    return true;
}

bool UnityAssetParser::ParseTypesV17(std::istream& in) {
    std::streampos typeCountPos = in.tellg();
    std::int32_t typeCount = ReadS32(in);
    VortigauntLog::LogF("[UnityAssetParser] ParseTypesV17: typeCount = %d (read at %lld)\n", typeCount, static_cast<long long>(typeCountPos));
    if (typeCount < 0) {
        VortigauntLog::LogF("[UnityAssetParser] ParseTypesV17: Invalid negative typeCount %d\n", typeCount);
        return false;
    }

    std::streampos curPos = in.tellg();
    in.seekg(0, std::ios::end);
    std::streampos endPos = in.tellg();
    in.seekg(curPos);
    std::int64_t remaining = static_cast<std::int64_t>(endPos - curPos);

    if (static_cast<std::int64_t>(typeCount) * 4 > remaining) {
        VortigauntLog::LogF("[UnityAssetParser] ParseTypesV17: typeCount %d exceeds stream bounds (remaining %lld)\n", typeCount, remaining);
        return false;
    }

    m_typesList.resize(typeCount);
    for (std::int32_t i = 0; i < typeCount; ++i) {
        if (in.fail() || in.eof()) {
            VortigauntLog::LogF("[UnityAssetParser] ParseTypesV17: Stream fail/eof before type %d of %d\n", i, typeCount);
            return false;
        }
        UnityType& ut = m_typesList[i];
        if (!ReadSerializedType(in, ut, false)) {
            VortigauntLog::LogF("[UnityAssetParser] ParseTypesV17: Failed to read type %d of %d\n", i, typeCount);
            return false;
        }
    }
    return true;
}

bool UnityAssetParser::ReadSerializedType(std::istream& in, UnityType& ut, bool isRefType) {
    if (in.fail() || in.eof()) return false;

    std::streampos startPos = in.tellg();
    ut.classId = ReadS32(in);

    if (m_formatVersion >= 16) {
        std::uint8_t stripped = 0;
        in.read(reinterpret_cast<char*>(&stripped), 1);
        ut.isStrippedType = (stripped != 0);
    } else {
        ut.isStrippedType = false;
    }

    if (m_formatVersion >= 17) {
        ut.scriptTypeIndex = static_cast<std::int16_t>(ReadU16(in));
    } else {
        ut.scriptTypeIndex = -1;
    }

    if (m_formatVersion >= 13) {
        bool isScript = false;
        if ((isRefType && ut.scriptTypeIndex >= 0) ||
            (!isRefType && ((m_formatVersion < 16 && ut.classId < 0) || (m_formatVersion >= 16 && ut.classId == 114)))) {
            isScript = true;
        }

        if (isScript) {
            in.read(reinterpret_cast<char*>(ut.scriptId), 16);
        } else {
            std::memset(ut.scriptId, 0, 16);
        }

        in.read(reinterpret_cast<char*>(ut.oldTypeHash), 16);
    } else {
        std::memset(ut.scriptId, 0, 16);
        std::memset(ut.oldTypeHash, 0, 16);
    }

    if (m_enableTypeTree) {
        if (!ReadTypeTree(in, ut)) {
            VortigauntLog::LogF("[UnityAssetParser] ReadSerializedType: ReadTypeTree failed for classId=%d, offset=%lld\n", ut.classId, static_cast<long long>(startPos));
            return false;
        }

        if (m_formatVersion >= 21) {
            if (isRefType) {
                std::string className = Utils::ReadNullTerminatedString(in);
                std::string namespaceStr = Utils::ReadNullTerminatedString(in);
                std::string assemblyName = Utils::ReadNullTerminatedString(in);
                VortigauntLog::LogF("[UnityAssetParser] ReadSerializedType: RefType parsed: classId=%d, class=%s, ns=%s, assembly=%s, offset=%lld\n",
                    ut.classId, className.c_str(), namespaceStr.c_str(), assemblyName.c_str(), static_cast<long long>(startPos));
            } else {
                std::int32_t depCount = ReadS32(in);
                if (depCount > 0) {
                    std::streampos dCurPos = in.tellg();
                    in.seekg(0, std::ios::end);
                    std::streampos dEndPos = in.tellg();
                    in.seekg(dCurPos);
                    std::int64_t dRemaining = static_cast<std::int64_t>(dEndPos - dCurPos);
                    if (static_cast<std::int64_t>(depCount) * 4 <= dRemaining) {
                        in.seekg(static_cast<std::streamoff>(depCount) * 4, std::ios::cur);
                    } else {
                        VortigauntLog::LogF("[UnityAssetParser] ReadSerializedType: typeDependencies count %d exceeds remaining stream size %lld\n", depCount, dRemaining);
                        return false;
                    }
                } else if (depCount < 0) {
                    VortigauntLog::LogF("[UnityAssetParser] ReadSerializedType: Invalid negative typeDependencies count %d\n", depCount);
                    return false;
                }
            }
        }
    }

    if (in.fail()) {
        VortigauntLog::LogF("[UnityAssetParser] ReadSerializedType: Stream failed for classId=%d, offset=%lld\n", ut.classId, static_cast<long long>(startPos));
        return false;
    }

    return true;
}

bool UnityAssetParser::ParseObjectsV17(std::istream& in) {
    std::streampos objCountPos = in.tellg();
    std::int32_t objectCount = ReadS32(in);
    VortigauntLog::LogF("[UnityAssetParser] ParseObjectsV17: objectCount = %d (read at %lld)\n", objectCount, static_cast<long long>(objCountPos));
    if (objectCount < 0) {
        VortigauntLog::LogF("[UnityAssetParser] ParseObjectsV17: Invalid negative objectCount %d\n", objectCount);
        return false;
    }

    std::streampos curPos = in.tellg();
    in.seekg(0, std::ios::end);
    std::streampos endPos = in.tellg();
    in.seekg(curPos);
    std::int64_t remaining = static_cast<std::int64_t>(endPos - curPos);

    if (static_cast<std::int64_t>(objectCount) * 20 > remaining) {
        VortigauntLog::LogF("[UnityAssetParser] ParseObjectsV17: objectCount %d exceeds stream bounds (remaining %lld)\n", objectCount, remaining);
        return false;
    }

    m_objects.resize(objectCount);
    for (std::int32_t i = 0; i < objectCount; ++i) {
        if (in.fail() || in.eof()) {
            VortigauntLog::LogF("[UnityAssetParser] ParseObjectsV17: Stream fail/eof before object %d of %d\n", i, objectCount);
            return false;
        }
        UnityObjectInfo& obj = m_objects[i];

        AlignStream(in, 4);

        if (m_formatVersion >= 22) {
            // For formatVersion >= 22, pathId is 64-bit aligned and read.
            obj.pathId = ReadS64(in);
            obj.offset = ReadU64(in);
        } else {
            obj.pathId = ReadS64(in);
            obj.offset = ReadU32(in);
        }

        obj.size = ReadU32(in);
        obj.typeIndex = ReadU32(in);

        if (obj.typeIndex < m_typesList.size()) {
            obj.classId = m_typesList[obj.typeIndex].classId;
        } else {
            obj.classId = 0;
        }
    }

    m_localPathIdToClassId.clear();
    m_localPathIdToClassId.reserve(objectCount);
    for (const auto& obj : m_objects) {
        m_localPathIdToClassId[obj.pathId] = obj.classId;
    }

    return true;
}

bool UnityAssetParser::ParseScriptTypesAndExternals(std::istream& in) {
    auto remainingBytes = [&in]() -> std::int64_t {
        std::streampos curPos = in.tellg();
        in.seekg(0, std::ios::end);
        std::streampos endPos = in.tellg();
        in.seekg(curPos);
        return static_cast<std::int64_t>(endPos - curPos);
    };

    if (m_formatVersion >= 11) {
        std::int32_t scriptCount = ReadS32(in);
        if (in.fail() || scriptCount < 0) {
            VortigauntLog::LogF("[UnityAssetParser] ParseScriptTypesAndExternals: Invalid scriptCount %d\n", scriptCount);
            return false;
        }
        if (static_cast<std::int64_t>(scriptCount) * 8 > remainingBytes()) {
            VortigauntLog::LogF("[UnityAssetParser] ParseScriptTypesAndExternals: scriptCount %d exceeds stream bounds\n", scriptCount);
            return false;
        }
        for (std::int32_t i = 0; i < scriptCount; ++i) {
            ReadS32(in); // localSerializedFileIndex
            if (m_formatVersion < 14) {
                ReadS32(in); // localIdentifierInFile
            } else {
                AlignStream(in, 4);
                ReadS64(in); // localIdentifierInFile
            }
            if (in.fail()) {
                VortigauntLog::LogF("[UnityAssetParser] ParseScriptTypesAndExternals: Stream failed at script type %d of %d\n", i, scriptCount);
                return false;
            }
        }
    }

    std::int32_t externalsCount = ReadS32(in);
    if (in.fail() || externalsCount < 0) {
        VortigauntLog::LogF("[UnityAssetParser] ParseScriptTypesAndExternals: Invalid externalsCount %d\n", externalsCount);
        return false;
    }
    std::int64_t minEntrySize = 1;                  // pathName terminator
    if (m_formatVersion >= 5) minEntrySize += 20;   // GUID + type
    if (m_formatVersion >= 6) minEntrySize += 1;    // tempEmpty terminator
    if (static_cast<std::int64_t>(externalsCount) * minEntrySize > remainingBytes()) {
        VortigauntLog::LogF("[UnityAssetParser] ParseScriptTypesAndExternals: externalsCount %d exceeds stream bounds\n", externalsCount);
        return false;
    }

    m_externals.clear();
    m_externals.reserve(externalsCount);
    for (std::int32_t i = 0; i < externalsCount; ++i) {
        if (m_formatVersion >= 6) {
            Utils::ReadNullTerminatedString(in); // tempEmpty, unused
        }
        if (m_formatVersion >= 5) {
            in.seekg(16, std::ios::cur); // GUID, unused
            ReadS32(in); // type, unused
        }
        std::string pathName = Utils::ReadNullTerminatedString(in);
        if (in.fail()) {
            VortigauntLog::LogF("[UnityAssetParser] ParseScriptTypesAndExternals: Stream failed at external %d of %d\n", i, externalsCount);
            return false;
        }
        UnityExternalFile ext;
        ext.pathName = pathName;
        size_t sep = pathName.find_last_of("/\\");
        ext.fileName = (sep == std::string::npos) ? pathName : pathName.substr(sep + 1);
        m_externals.push_back(std::move(ext));
    }

    VortigauntLog::LogF("[UnityAssetParser] Externals table parsed: %d entries\n", externalsCount);
    return true;
}

bool UnityAssetParser::ReadTypeTree(std::istream& in, UnityType& ut) {
    std::uint32_t nodeCount = ReadU32(in);
    std::uint32_t stringBufferSize = ReadU32(in);

    if (in.fail()) return false;

    std::streampos curPos = in.tellg();
    in.seekg(0, std::ios::end);
    std::streampos endPos = in.tellg();
    in.seekg(curPos);
    std::int64_t remaining = static_cast<std::int64_t>(endPos - curPos);

    std::int64_t minRequired = static_cast<std::int64_t>(nodeCount) * 24 + stringBufferSize;
    if (minRequired > remaining) {
        return false;
    }

    ut.nodes.resize(nodeCount);
    for (std::uint32_t i = 0; i < nodeCount; ++i) {
        if (in.fail() || in.eof()) return false;
        TypeTreeNode& node = ut.nodes[i];
        node.version = ReadU16(in);

        in.read(reinterpret_cast<char*>(&node.depth), 1);
        in.read(reinterpret_cast<char*>(&node.isArray), 1);

        node.typeOffset = ReadU32(in);
        node.nameOffset = ReadU32(in);
        node.size = ReadS32(in);
        node.index = ReadU32(in);
        node.metaFlags = ReadU32(in);

        if (m_formatVersion >= 19) {
            node.refTypeHash = ReadU64(in);
        } else {
            node.refTypeHash = 0;
        }
    }

    if (stringBufferSize > 0) {
        ut.localStringBuffer.resize(stringBufferSize);
        in.read(ut.localStringBuffer.data(), stringBufferSize);
    }

    return true;
}

std::string UnityAssetParser::ResolveString(std::uint32_t offset, const std::vector<char>& localBuffer) const {
    if (offset & 0x80000000u) {
        std::uint32_t idx = offset & 0x7FFFFFFFu;
        auto it = g_commonString.find(idx);
        if (it != g_commonString.end()) {
            return it->second;
        }
        return "";
    }
    if (offset >= localBuffer.size()) {
        return "";
    }
    return std::string(localBuffer.data() + offset);
}

void UnityAssetParser::GetUnityVersionParts(int& major, int& minor) const {
    major = 0;
    minor = 0;
    if (m_unityVersion.empty()) return;
    std::stringstream ss(m_unityVersion);
    char dot = '\0';
    if (ss >> major) {
        ss >> dot >> minor;
    }
}

UnityValue UnityAssetParser::DeserializeObject(const UnityObjectInfo& objInfo) const {
    if (objInfo.typeIndex >= m_typesList.size()) {
        return UnityValue{};
    }
    const UnityType& utype = m_typesList[objInfo.typeIndex];

    std::uint64_t absoluteOffset = m_dataOffset + objInfo.offset;
    if (absoluteOffset + objInfo.size > m_fileData.size()) {
        return UnityValue{};
    }

    const char* dataPtr = m_fileData.data() + absoluteOffset;
    membuf buf(dataPtr, objInfo.size);
    std::istream stream(&buf);
    UnityValue root;

    auto readAlignedString = [this, &objInfo](std::istream& in) { return ReadAlignedString(in, objInfo.size); };
    auto readByteArray = [this, &objInfo](std::istream& in) { return ReadByteArray(in, objInfo.size); };
    auto readFloatArray = [this, &objInfo](std::istream& in) { return ReadFloatArray(in, objInfo.size); };
    auto readUint32Array = [this, &objInfo](std::istream& in) { return ReadUint32Array(in, objInfo.size); };
    auto readPackedFloatVector = [this, &objInfo](std::istream& in) { return ReadPackedFloatVector(in, objInfo.size); };
    auto readPackedIntVector = [this, &objInfo](std::istream& in) { return ReadPackedIntVector(in, objInfo.size); };
    auto readPPtr = [this](std::istream& in) { return ReadPPtr(in); };

    const bool bigEndian = (m_endianness == 1);
    const int32_t maxFileId = m_externalsParsed ? static_cast<int32_t>(m_externals.size()) : 200;
    auto resolvePPtrClass = [this](int32_t fileID, int64_t pathID) -> int32_t {
        if (fileID == 0) return GetLocalClassId(pathID);
        if (m_pptrClassResolver) return m_pptrClassResolver(fileID, pathID);
        return -1;
    };

    if (utype.nodes.empty()) {
        if (objInfo.classId == UnityClass::Texture2D) {
            std::string name = readAlignedString(stream);
            
            int version_major = 0;
            int version_minor = 0;
            GetUnityVersionParts(version_major, version_minor);

            if (version_major > 2017 || (version_major == 2017 && version_minor >= 3)) {
                if (version_major < 2023 || (version_major == 2023 && version_minor < 2)) {
                    std::int32_t forcedFallbackFormat = ReadS32(stream);
                    std::uint8_t downscaleFallback = 0;
                    stream.read(reinterpret_cast<char*>(&downscaleFallback), 1);
                }
                if (version_major > 2020 || (version_major == 2020 && version_minor >= 2)) {
                    std::uint8_t isAlphaChannelOptional = 0;
                    stream.read(reinterpret_cast<char*>(&isAlphaChannelOptional), 1);
                }
                AlignStream(stream, 4);
            }
            
            std::int32_t width = ReadS32(stream);
            std::int32_t height = ReadS32(stream);
            std::int32_t completeImageSize = ReadS32(stream);
            
            std::int32_t mipsStripped = 0;
            if (version_major >= 2020) {
                mipsStripped = ReadS32(stream);
            }
            
            std::int32_t textureFormat = ReadS32(stream);
            
            std::uint8_t mipMap = 0;
            std::int32_t mipCount = 0;
            if (version_major < 5 || (version_major == 5 && version_minor < 2)) {
                stream.read(reinterpret_cast<char*>(&mipMap), 1);
            } else {
                mipCount = ReadS32(stream);
            }
            
            std::uint8_t isReadable = 0;
            if (version_major > 2 || (version_major == 2 && version_minor >= 6)) {
                stream.read(reinterpret_cast<char*>(&isReadable), 1);
            }
            
            std::uint8_t isPreProcessed = 0;
            if (version_major >= 2020) {
                stream.read(reinterpret_cast<char*>(&isPreProcessed), 1);
            }
            
            std::uint8_t ignoreMasterLimit = 0;
            if (version_major > 2019 || (version_major == 2019 && version_minor >= 3)) {
                stream.read(reinterpret_cast<char*>(&ignoreMasterLimit), 1);
            }
            
            if (version_major > 2022 || (version_major == 2022 && version_minor >= 2)) {
                AlignStream(stream, 4);
                std::string mipmapLimitGroupName = readAlignedString(stream);
            }
            
            if (version_major >= 3) {
                if (version_major < 5 || (version_major == 5 && version_minor <= 4)) {
                    std::uint8_t readAllowed = 0;
                    stream.read(reinterpret_cast<char*>(&readAllowed), 1);
                }
            }
            
            std::uint8_t streamingMipmaps = 0;
            if (version_major > 2018 || (version_major == 2018 && version_minor >= 2)) {
                stream.read(reinterpret_cast<char*>(&streamingMipmaps), 1);
            }
            
            AlignStream(stream, 4);
            
            std::int32_t streamingMipmapsPriority = 0;
            if (version_major > 2018 || (version_major == 2018 && version_minor >= 2)) {
                streamingMipmapsPriority = ReadS32(stream);
            }
            
            std::int32_t imageCount = ReadS32(stream);
            std::int32_t textureDimension = ReadS32(stream);
            
            std::int32_t filterMode = ReadS32(stream);
            std::int32_t aniso = ReadS32(stream);
            float mipBias = 0.0f;
            stream.read(reinterpret_cast<char*>(&mipBias), 4);
            if (m_endianness == 1) {
                std::uint32_t bits = *reinterpret_cast<std::uint32_t*>(&mipBias);
                bits = Utils::Swap32(bits);
                mipBias = *reinterpret_cast<float*>(&bits);
            }
            
            std::int32_t wrapU = 0;
            std::int32_t wrapV = 0;
            std::int32_t wrapW = 0;
            if (version_major >= 2017) {
                wrapU = ReadS32(stream);
                wrapV = ReadS32(stream);
                wrapW = ReadS32(stream);
            } else {
                wrapU = ReadS32(stream);
            }
            
            std::int32_t lightmapFormat = 0;
            if (version_major >= 3) {
                lightmapFormat = ReadS32(stream);
            }
            
            std::int32_t colorSpace = 0;
            if (version_major > 3 || (version_major == 3 && version_minor >= 5)) {
                colorSpace = ReadS32(stream);
            }
            
            if (version_major > 2020 || (version_major == 2020 && version_minor >= 2)) {
                std::int32_t blobLen = ReadS32(stream);
                std::streampos currPos = stream.tellg();
                std::int64_t remainingBytes = static_cast<std::int64_t>(objInfo.size) - currPos;
                if (blobLen > 0 && blobLen <= remainingBytes) {
                    stream.seekg(blobLen, std::ios::cur);
                } else if (blobLen > 0) {
                    stream.seekg(currPos - std::streampos(4));
                }
                AlignStream(stream, 4);
            }
            
            std::int32_t imageDataSize = ReadS32(stream);
            std::vector<std::uint8_t> imageData;
            
            root.type = UnityValue::ObjectVal;
            root.objectVal["m_Name"].type = UnityValue::StringVal;
            root.objectVal["m_Name"].stringVal = name;
            root.objectVal["m_Width"].type = UnityValue::IntVal;
            root.objectVal["m_Width"].intVal = width;
            root.objectVal["m_Height"].type = UnityValue::IntVal;
            root.objectVal["m_Height"].intVal = height;
            root.objectVal["m_TextureFormat"].type = UnityValue::IntVal;
            root.objectVal["m_TextureFormat"].intVal = textureFormat;
            
            if (imageDataSize > 0) {
                if (imageDataSize <= static_cast<std::int64_t>(objInfo.size)) {
                    imageData.resize(imageDataSize);
                    stream.read(reinterpret_cast<char*>(imageData.data()), imageDataSize);
                    AlignStream(stream, 4);
                }
            } else if ((version_major == 5 && version_minor >= 3) || version_major > 5) {
                std::int64_t streamOffset = 0;
                if (version_major >= 2020) {
                    streamOffset = ReadS64(stream);
                } else {
                    streamOffset = ReadU32(stream);
                }
                std::uint32_t streamSize = ReadU32(stream);
                std::string streamPath = readAlignedString(stream);
                
                UnityValue streamDataVal;
                streamDataVal.type = UnityValue::ObjectVal;
                streamDataVal.objectVal["offset"].type = UnityValue::UIntVal;
                streamDataVal.objectVal["offset"].uintVal = streamOffset;
                streamDataVal.objectVal["size"].type = UnityValue::UIntVal;
                streamDataVal.objectVal["size"].uintVal = streamSize;
                streamDataVal.objectVal["path"].type = UnityValue::StringVal;
                streamDataVal.objectVal["path"].stringVal = streamPath;
                root.objectVal["m_StreamData"] = std::move(streamDataVal);
            }
            root.objectVal["image data"].type = UnityValue::ByteArrayVal;
            root.objectVal["image data"].byteData = std::move(imageData);
            
        } else if (objInfo.classId == UnityClass::Mesh) {
            std::string name = readAlignedString(stream);
            
            int version_major = 0;
            int version_minor = 0;
            GetUnityVersionParts(version_major, version_minor);
            
            bool use16BitIndices = true;
            if (version_major < 3 || (version_major == 3 && version_minor < 5)) {
                use16BitIndices = ReadS32(stream) > 0;
            }
            
            std::vector<std::uint32_t> oldIndexBuffer;
            if (version_major == 2 && version_minor <= 5) {
                std::int32_t m_IndexBuffer_size = ReadS32(stream);
                if (m_IndexBuffer_size > 0 && m_IndexBuffer_size < 100000000) {
                    if (use16BitIndices) {
                        oldIndexBuffer.resize(m_IndexBuffer_size / 2);
                        for (std::int32_t i = 0; i < m_IndexBuffer_size / 2; ++i) {
                            oldIndexBuffer[i] = ReadU16(stream);
                        }
                        AlignStream(stream, 4);
                    } else {
                        oldIndexBuffer.resize(m_IndexBuffer_size / 4);
                        for (std::int32_t i = 0; i < m_IndexBuffer_size / 4; ++i) {
                            oldIndexBuffer[i] = ReadU32(stream);
                        }
                    }
                }
            }
            
            std::int32_t submeshCount = ReadS32(stream);
            if (submeshCount < 0 || submeshCount > 1000000) return UnityValue{};
            
            std::vector<UnityValue> submeshesVal;
            for (std::int32_t i = 0; i < submeshCount; ++i) {
                std::uint32_t firstByte = ReadU32(stream);
                std::uint32_t indexCount = ReadU32(stream);
                std::int32_t topology = ReadS32(stream);
                
                std::uint32_t triangleCount = 0;
                if (version_major < 4) {
                    triangleCount = ReadU32(stream);
                }
                
                std::uint32_t baseVertex = 0;
                if (version_major > 2017 || (version_major == 2017 && version_minor >= 3)) {
                    baseVertex = ReadU32(stream);
                }
                
                std::uint32_t firstVertex = 0;
                std::uint32_t vertexCount = 0;
                float center[3] = {0.0f}, extent[3] = {0.0f};
                if (version_major >= 3) {
                    firstVertex = ReadU32(stream);
                    vertexCount = ReadU32(stream);
                    stream.read(reinterpret_cast<char*>(center), 12);
                    stream.read(reinterpret_cast<char*>(extent), 12);
                }
                
                UnityValue smObj;
                smObj.type = UnityValue::ObjectVal;
                smObj.objectVal["firstByte"].type = UnityValue::UIntVal;
                smObj.objectVal["firstByte"].uintVal = firstByte;
                smObj.objectVal["indexCount"].type = UnityValue::UIntVal;
                smObj.objectVal["indexCount"].uintVal = indexCount;
                smObj.objectVal["topology"].type = UnityValue::UIntVal;
                smObj.objectVal["topology"].uintVal = topology;
                smObj.objectVal["baseVertex"].type = UnityValue::UIntVal;
                smObj.objectVal["baseVertex"].uintVal = baseVertex;
                smObj.objectVal["firstVertex"].type = UnityValue::UIntVal;
                smObj.objectVal["firstVertex"].uintVal = firstVertex;
                smObj.objectVal["vertexCount"].type = UnityValue::UIntVal;
                smObj.objectVal["vertexCount"].uintVal = vertexCount;
                
                submeshesVal.push_back(smObj);
            }
            
            if (version_major > 4 || (version_major == 4 && version_minor >= 1)) {
                if (version_major > 4 || (version_major == 4 && version_minor >= 3)) {
                    std::int32_t numVerts = ReadS32(stream);
                    if (numVerts < 0 || numVerts > 10000000) return UnityValue{};
                    for (std::int32_t i = 0; i < numVerts; ++i) {
                        stream.seekg(40, std::ios::cur);
                    }
                    
                    std::int32_t numShapes = ReadS32(stream);
                    if (numShapes < 0 || numShapes > 1000000) return UnityValue{};
                    for (std::int32_t i = 0; i < numShapes; ++i) {
                        stream.seekg(8, std::ios::cur);
                        std::uint8_t hasNorms = 0, hasTangents = 0;
                        stream.read(reinterpret_cast<char*>(&hasNorms), 1);
                        stream.read(reinterpret_cast<char*>(&hasTangents), 1);
                        AlignStream(stream, 4);
                    }
                    
                    std::int32_t numChannels = ReadS32(stream);
                    if (numChannels < 0 || numChannels > 1000000) return UnityValue{};
                    for (std::int32_t i = 0; i < numChannels; ++i) {
                        readAlignedString(stream);
                        stream.seekg(12, std::ios::cur);
                    }
                    
                    std::vector<float> fullWeights = readFloatArray(stream);
                } else {
                    std::int32_t m_ShapesSize = ReadS32(stream);
                    if (m_ShapesSize < 0 || m_ShapesSize > 1000000) return UnityValue{};
                    for (std::int32_t i = 0; i < m_ShapesSize; ++i) {
                        readAlignedString(stream);
                        stream.seekg(8, std::ios::cur);
                        stream.seekg(24, std::ios::cur);
                        std::uint8_t hasNorms = 0, hasTangents = 0;
                        stream.read(reinterpret_cast<char*>(&hasNorms), 1);
                        stream.read(reinterpret_cast<char*>(&hasTangents), 1);
                    }
                    AlignStream(stream, 4);
                    
                    std::int32_t m_ShapeVerticesSize = ReadS32(stream);
                    if (m_ShapeVerticesSize < 0 || m_ShapeVerticesSize > 10000000) return UnityValue{};
                    for (std::int32_t i = 0; i < m_ShapeVerticesSize; ++i) {
                        stream.seekg(40, std::ios::cur);
                    }
                }
            }
            
            if (version_major > 4 || (version_major == 4 && version_minor >= 3)) {
                std::int32_t numPoses = ReadS32(stream);
                if (numPoses < 0 || numPoses > 1000000) return UnityValue{};
                stream.seekg(static_cast<std::streamoff>(numPoses) * 64, std::ios::cur);
                
                std::vector<std::uint32_t> boneNameHashes = readUint32Array(stream);
                std::uint32_t rootBoneNameHash = ReadU32(stream);
            }
            
            if (version_major > 2 || (version_major == 2 && version_minor >= 6)) {
                if (version_major >= 2019) {
                    std::int32_t bonesAABBSize = ReadS32(stream);
                    if (bonesAABBSize >= 0 && bonesAABBSize < 1000000) {
                        stream.seekg(static_cast<std::streamoff>(bonesAABBSize) * 24, std::ios::cur);
                    }
                    std::vector<std::uint32_t> variableBoneCountWeights = readUint32Array(stream);
                }
                
                std::uint8_t meshCompression = 0;
                std::uint8_t streamCompression = 0;
                std::uint8_t isReadable = 1;
                std::uint8_t keepVertices = 1;
                std::uint8_t keepIndices = 1;
                
                stream.read(reinterpret_cast<char*>(&meshCompression), 1);
                if (version_major >= 4) {
                    if (version_major < 5) {
                        stream.read(reinterpret_cast<char*>(&streamCompression), 1);
                    }
                    stream.read(reinterpret_cast<char*>(&isReadable), 1);
                    stream.read(reinterpret_cast<char*>(&keepVertices), 1);
                    stream.read(reinterpret_cast<char*>(&keepIndices), 1);
                }
                AlignStream(stream, 4);
            }
            
            std::int32_t indexFormat = 0;
            std::vector<std::uint8_t> indexBuffer;
            if (version_major > 2 || (version_major == 2 && version_minor >= 6)) {
                if (version_major > 2017 || (version_major == 2017 && version_minor >= 3)) {
                    indexFormat = ReadS32(stream);
                    use16BitIndices = (indexFormat == 0);
                }
                indexBuffer = readByteArray(stream);
            } else {
                if (!oldIndexBuffer.empty()) {
                    if (use16BitIndices) {
                        indexBuffer.resize(oldIndexBuffer.size() * 2);
                        for (size_t i = 0; i < oldIndexBuffer.size(); ++i) {
                            indexBuffer[i * 2] = oldIndexBuffer[i] & 0xFF;
                            indexBuffer[i * 2 + 1] = (oldIndexBuffer[i] >> 8) & 0xFF;
                        }
                    } else {
                        indexBuffer.resize(oldIndexBuffer.size() * 4);
                        for (size_t i = 0; i < oldIndexBuffer.size(); ++i) {
                            indexBuffer[i * 4] = oldIndexBuffer[i] & 0xFF;
                            indexBuffer[i * 4 + 1] = (oldIndexBuffer[i] >> 8) & 0xFF;
                            indexBuffer[i * 4 + 2] = (oldIndexBuffer[i] >> 16) & 0xFF;
                            indexBuffer[i * 4 + 3] = (oldIndexBuffer[i] >> 24) & 0xFF;
                        }
                    }
                }
            }
            
            std::uint32_t vertexCount = 0;
            std::vector<UnityValue> channelsVal;
            std::vector<std::uint8_t> vertexDataBytes;
            
            if (version_major < 3 || (version_major == 3 && version_minor < 5)) {
                vertexCount = ReadS32(stream);
                stream.seekg(static_cast<std::streamoff>(vertexCount) * 12, std::ios::cur);
                
                std::int32_t skinSize = ReadS32(stream);
                if (skinSize > 0 && skinSize < 10000000) {
                    stream.seekg(static_cast<std::streamoff>(skinSize) * 32, std::ios::cur);
                }
                
                std::int32_t numPoses = ReadS32(stream);
                if (numPoses > 0 && numPoses < 1000000) {
                    stream.seekg(static_cast<std::streamoff>(numPoses) * 64, std::ios::cur);
                }
                
                std::int32_t uv0Size = ReadS32(stream);
                if (uv0Size > 0 && uv0Size < 10000000) {
                    stream.seekg(static_cast<std::streamoff>(uv0Size) * 8, std::ios::cur);
                }
                
                std::int32_t uv1Size = ReadS32(stream);
                if (uv1Size > 0 && uv1Size < 10000000) {
                    stream.seekg(static_cast<std::streamoff>(uv1Size) * 8, std::ios::cur);
                }
                
                if (version_major == 2 && version_minor <= 5) {
                    std::int32_t tangentSpaceSize = ReadS32(stream);
                    if (tangentSpaceSize > 0 && tangentSpaceSize < 10000000) {
                        stream.seekg(static_cast<std::streamoff>(tangentSpaceSize) * 28, std::ios::cur);
                    }
                } else {
                    std::int32_t tangentsSize = ReadS32(stream);
                    if (tangentsSize > 0 && tangentsSize < 10000000) {
                        stream.seekg(static_cast<std::streamoff>(tangentsSize) * 16, std::ios::cur);
                    }
                    std::int32_t normalsSize = ReadS32(stream);
                    if (normalsSize > 0 && normalsSize < 10000000) {
                        stream.seekg(static_cast<std::streamoff>(normalsSize) * 12, std::ios::cur);
                    }
                }
            } else {
                if (version_major < 2018 || (version_major == 2018 && version_minor < 2)) {
                    std::int32_t skinSize = ReadS32(stream);
                    if (skinSize > 0 && skinSize < 10000000) {
                        stream.seekg(static_cast<std::streamoff>(skinSize) * 32, std::ios::cur);
                    }
                }
                
                if (version_major == 3 || (version_major == 4 && version_minor <= 2)) {
                    std::int32_t numPoses = ReadS32(stream);
                    if (numPoses > 0 && numPoses < 1000000) {
                        stream.seekg(static_cast<std::streamoff>(numPoses) * 64, std::ios::cur);
                    }
                }
                
                if (version_major < 2018) {
                    std::uint32_t currentChannels = ReadU32(stream);
                }
                
                vertexCount = ReadU32(stream);
                
                if (version_major >= 4) {
                    std::int32_t channelSize = ReadS32(stream);
                    if (channelSize < 0 || channelSize > 100) return UnityValue{};
                    for (std::int32_t i = 0; i < channelSize; ++i) {
                        std::uint8_t chStream = 0, chOffset = 0, chFormat = 0, chDimension = 0;
                        stream.read(reinterpret_cast<char*>(&chStream), 1);
                        stream.read(reinterpret_cast<char*>(&chOffset), 1);
                        stream.read(reinterpret_cast<char*>(&chFormat), 1);
                        stream.read(reinterpret_cast<char*>(&chDimension), 1);
                        
                        chDimension = chDimension & 0xF;
                        
                        UnityValue chObj;
                        chObj.type = UnityValue::ObjectVal;
                        chObj.objectVal["stream"].type = UnityValue::UIntVal;
                        chObj.objectVal["stream"].uintVal = chStream;
                        chObj.objectVal["offset"].type = UnityValue::UIntVal;
                        chObj.objectVal["offset"].uintVal = chOffset;
                        chObj.objectVal["format"].type = UnityValue::UIntVal;
                        chObj.objectVal["format"].uintVal = chFormat;
                        chObj.objectVal["dimension"].type = UnityValue::UIntVal;
                        chObj.objectVal["dimension"].uintVal = chDimension;
                        channelsVal.push_back(chObj);
                    }
                }
                
                if (version_major < 5) {
                    std::int32_t streamCount = 0;
                    if (version_major < 4) {
                        streamCount = 4;
                    } else {
                        streamCount = ReadS32(stream);
                    }
                    for (std::int32_t i = 0; i < streamCount; ++i) {
                        if (version_major < 4) {
                            stream.seekg(16, std::ios::cur);
                        } else {
                            stream.seekg(12, std::ios::cur);
                        }
                    }
                }
                
                vertexDataBytes = readByteArray(stream);
            }
            
            if (version_major > 2 || (version_major == 2 && version_minor >= 6)) {
                UnityValue compressedMesh;
                compressedMesh.type = UnityValue::ObjectVal;
                compressedMesh.objectVal["m_Vertices"] = PackedFloatToValue(readPackedFloatVector(stream));
                compressedMesh.objectVal["m_UV"] = PackedFloatToValue(readPackedFloatVector(stream));
                if (version_major < 5) {
                    readPackedFloatVector(stream); // bind poses, not exported
                }
                compressedMesh.objectVal["m_Normals"] = PackedFloatToValue(readPackedFloatVector(stream));
                compressedMesh.objectVal["m_Tangents"] = PackedFloatToValue(readPackedFloatVector(stream));
                compressedMesh.objectVal["m_Weights"] = PackedIntToValue(readPackedIntVector(stream));
                compressedMesh.objectVal["m_NormalSigns"] = PackedIntToValue(readPackedIntVector(stream));
                compressedMesh.objectVal["m_TangentSigns"] = PackedIntToValue(readPackedIntVector(stream));
                if (version_major >= 5) {
                    compressedMesh.objectVal["m_FloatColors"] = PackedFloatToValue(readPackedFloatVector(stream));
                }
                compressedMesh.objectVal["m_BoneIndices"] = PackedIntToValue(readPackedIntVector(stream));
                compressedMesh.objectVal["m_Triangles"] = PackedIntToValue(readPackedIntVector(stream));

                if (version_major > 3 || (version_major == 3 && version_minor >= 5)) {
                    if (version_major < 5) {
                        readPackedIntVector(stream); // colors, not exported
                    } else {
                        compressedMesh.objectVal["m_UVInfo"].type = UnityValue::UIntVal;
                        compressedMesh.objectVal["m_UVInfo"].uintVal = ReadU32(stream);
                    }
                }
                root.objectVal["m_CompressedMesh"] = std::move(compressedMesh);
            }
            
            stream.seekg(24, std::ios::cur); // localAABB
            
            if (version_major < 3 || (version_major == 3 && version_minor <= 4)) {
                std::int32_t colorsSize = ReadS32(stream);
                if (colorsSize > 0 && colorsSize < 10000000) {
                    stream.seekg(colorsSize, std::ios::cur);
                }
                std::int32_t collisionTrianglesSize = ReadS32(stream);
                if (collisionTrianglesSize > 0 && collisionTrianglesSize < 10000000) {
                    stream.seekg(static_cast<std::streamoff>(collisionTrianglesSize) * 4, std::ios::cur);
                }
                std::int32_t collisionVertexCount = ReadS32(stream);
            }
            
            std::int32_t meshUsageFlags = ReadS32(stream);
            
            std::int32_t cookingOptions = 0;
            if (version_major > 2022 || (version_major == 2022 && version_minor >= 1)) {
                cookingOptions = ReadS32(stream);
            }
            
            if (version_major >= 5) {
                std::vector<std::uint8_t> bakedConvexCollisionMesh = readByteArray(stream);
                std::vector<std::uint8_t> bakedTriangleCollisionMesh = readByteArray(stream);
            }
            
            float meshMetrics[2] = {0.0f};
            if (version_major > 2018 || (version_major == 2018 && version_minor >= 2)) {
                stream.read(reinterpret_cast<char*>(meshMetrics), 8);
            }
            
            std::int64_t streamOffset = 0;
            std::uint32_t streamSize = 0;
            std::string streamPath = "";
            if (version_major > 2018 || (version_major == 2018 && version_minor >= 3)) {
                AlignStream(stream, 4);
                if (version_major >= 2020) {
                    streamOffset = ReadS64(stream);
                } else {
                    streamOffset = ReadU32(stream);
                }
                streamSize = ReadU32(stream);
                streamPath = readAlignedString(stream);
            }
            
            root.type = UnityValue::ObjectVal;
            root.objectVal["m_Name"].type = UnityValue::StringVal;
            root.objectVal["m_Name"].stringVal = name;
            root.objectVal["m_IndexFormat"].type = UnityValue::IntVal;
            root.objectVal["m_IndexFormat"].intVal = indexFormat;
            root.objectVal["m_IndexBuffer"].type = UnityValue::ByteArrayVal;
            root.objectVal["m_IndexBuffer"].byteData = std::move(indexBuffer);
            root.objectVal["m_SubMeshes"].type = UnityValue::ArrayVal;
            root.objectVal["m_SubMeshes"].arrayVal = std::move(submeshesVal);
            
            UnityValue vData;
            vData.type = UnityValue::ObjectVal;
            vData.objectVal["m_VertexCount"].type = UnityValue::UIntVal;
            vData.objectVal["m_VertexCount"].uintVal = vertexCount;
            vData.objectVal["m_Channels"].type = UnityValue::ArrayVal;
            vData.objectVal["m_Channels"].arrayVal = std::move(channelsVal);
            vData.objectVal["m_DataSize"].type = UnityValue::ByteArrayVal;
            vData.objectVal["m_DataSize"].byteData = std::move(vertexDataBytes);
            root.objectVal["m_VertexData"] = std::move(vData);
            
            UnityValue streamDataVal;
            streamDataVal.type = UnityValue::ObjectVal;
            streamDataVal.objectVal["offset"].type = UnityValue::UIntVal;
            streamDataVal.objectVal["offset"].uintVal = streamOffset;
            streamDataVal.objectVal["size"].type = UnityValue::UIntVal;
            streamDataVal.objectVal["size"].uintVal = streamSize;
            streamDataVal.objectVal["path"].type = UnityValue::StringVal;
            streamDataVal.objectVal["path"].stringVal = streamPath;
            root.objectVal["m_StreamData"] = std::move(streamDataVal);
            
        } else if (objInfo.classId == UnityClass::AnimationClip) {
            std::string name = readAlignedString(stream);
            root.type = UnityValue::ObjectVal;
            root.objectVal["m_Name"].type = UnityValue::StringVal;
            root.objectVal["m_Name"].stringVal = name;
        } else if (objInfo.classId == UnityClass::GameObject) {
            int version_major = 0;
            int version_minor = 0;
            GetUnityVersionParts(version_major, version_minor);
            
            std::int32_t compSize = ReadS32(stream);
            std::vector<UnityValue> components;
            if (compSize > 0 && compSize < 100000) {
                components.reserve(compSize);
                for (std::int32_t i = 0; i < compSize; ++i) {
                    if (version_major < 5 || (version_major == 5 && version_minor < 5)) {
                        stream.seekg(4, std::ios::cur); // first
                    }
                    components.push_back(readPPtr(stream));
                }
            }
            
            stream.seekg(4, std::ios::cur); // m_Layer
            std::string name = readAlignedString(stream);
            
            root.type = UnityValue::ObjectVal;
            root.objectVal["m_Name"].type = UnityValue::StringVal;
            root.objectVal["m_Name"].stringVal = name;
            root.objectVal["m_Components"].type = UnityValue::ArrayVal;
            root.objectVal["m_Components"].arrayVal = std::move(components);

        } else if (objInfo.classId == UnityClass::MeshFilter) {
            UnityValue m_GameObjectVal = readPPtr(stream);
            UnityValue m_MeshVal = readPPtr(stream);
            
            root.type = UnityValue::ObjectVal;
            root.objectVal["m_GameObject"] = std::move(m_GameObjectVal);
            root.objectVal["m_Mesh"] = std::move(m_MeshVal);

        } else if (objInfo.classId == UnityClass::MeshRenderer || objInfo.classId == UnityClass::Renderer) {
            // Structural scanner: GameObject PPtr, then the Material PPtr array
            std::int32_t goFileID = 0;
            std::int64_t goPathID = 0;
            if (objInfo.size >= 12) {
                ReadRawPPtr(dataPtr, 0, bigEndian, goFileID, goPathID);
            }

            std::vector<UnityValue> materials =
                ScanMaterialPPtrArray(dataPtr, objInfo.size, 12, bigEndian, maxFileId, resolvePPtrClass);

            root.type = UnityValue::ObjectVal;
            root.objectVal["m_GameObject"] = MakePPtrValue(goFileID, goPathID);
            root.objectVal["m_Materials"].type = UnityValue::ArrayVal;
            root.objectVal["m_Materials"].arrayVal = std::move(materials);

        } else if (objInfo.classId == UnityClass::SkinnedMeshRenderer) {
            // Structural scanner: GameObject PPtr, Material PPtr array, then the single Mesh PPtr
            std::int32_t goFileID = 0;
            std::int64_t goPathID = 0;
            if (objInfo.size >= 12) {
                ReadRawPPtr(dataPtr, 0, bigEndian, goFileID, goPathID);
            }

            std::vector<UnityValue> materials =
                ScanMaterialPPtrArray(dataPtr, objInfo.size, 12, bigEndian, maxFileId, resolvePPtrClass);
            UnityValue meshVal =
                ScanSinglePPtrOfClass(dataPtr, objInfo.size, 12, bigEndian, maxFileId, UnityClass::Mesh, resolvePPtrClass);

            root.type = UnityValue::ObjectVal;
            root.objectVal["m_GameObject"] = MakePPtrValue(goFileID, goPathID);
            root.objectVal["m_Materials"].type = UnityValue::ArrayVal;
            root.objectVal["m_Materials"].arrayVal = std::move(materials);
            root.objectVal["m_Mesh"] = std::move(meshVal);

        } else if (objInfo.classId == UnityClass::Material) {
            // Structural Scanner for Texture2D inside Material payload
            std::string name = readAlignedString(stream);
            UnityValue shaderPPtr = readPPtr(stream);
            
            std::streampos startOffset = stream.tellg();
            std::vector<UnityValue> texEnvs;

            if (stream.fail() || startOffset < 0) {
                startOffset = std::streampos(static_cast<std::streamoff>(objInfo.size));
            }

            for (size_t offset = static_cast<size_t>(startOffset); offset + 12 <= objInfo.size; offset += 4) {
                std::int32_t fileID = 0;
                std::int64_t pathID = 0;
                ReadRawPPtr(dataPtr, offset, bigEndian, fileID, pathID);

                if (fileID < 0 || fileID > maxFileId || pathID == 0) continue;
                if (resolvePPtrClass(fileID, pathID) != UnityClass::Texture2D) continue;

                bool duplicate = false;
                for (const auto& existingEnv : texEnvs) {
                    const auto& existingTex = existingEnv.GetField("second").GetField("m_Texture");
                    if (existingTex.GetField("m_PathID").AsInt() == pathID) {
                        duplicate = true;
                        break;
                    }
                }
                if (duplicate) continue;

                UnityValue envVal;
                envVal.type = UnityValue::ObjectVal;
                envVal.objectVal["m_Texture"] = MakePPtrValue(fileID, pathID);

                UnityValue kvPair;
                kvPair.type = UnityValue::ObjectVal;
                kvPair.objectVal["first"].type = UnityValue::StringVal;
                kvPair.objectVal["first"].stringVal = ""; // Dummy key
                kvPair.objectVal["second"] = std::move(envVal);
                texEnvs.push_back(std::move(kvPair));
            }

            UnityValue savedProps;
            savedProps.type = UnityValue::ObjectVal;
            savedProps.objectVal["m_TexEnvs"].type = UnityValue::ArrayVal;
            savedProps.objectVal["m_TexEnvs"].arrayVal = std::move(texEnvs);
            
            root.type = UnityValue::ObjectVal;
            root.objectVal["m_Name"].type = UnityValue::StringVal;
            root.objectVal["m_Name"].stringVal = name;
            root.objectVal["m_SavedProperties"] = std::move(savedProps);

        } else {
            return UnityValue{};
        }
    } else {
        size_t nodeIdx = 0;
        root = DeserializeNode(utype.nodes, utype.localStringBuffer, nodeIdx, stream, m_endianness == 1);
    }

    // Post-processing: resolve StreamData if companion data is loaded and target arrays are empty
    if (root.type == UnityValue::ObjectVal) {
        if (objInfo.classId == UnityClass::Texture2D) {
            auto& imgVal = root.objectVal["image data"];
            if (imgVal.IsNull() || (imgVal.type == UnityValue::ByteArrayVal && imgVal.byteData.empty())) {
                const auto& streamData = root.GetField("m_StreamData");
                if (!streamData.IsNull()) {
                    const auto& offsetVal = streamData.GetField("offset").IsNull() ? streamData.GetField("m_Offset") : streamData.GetField("offset");
                    const auto& sizeVal = streamData.GetField("size").IsNull() ? streamData.GetField("m_Size") : streamData.GetField("size");
                    const auto& pathVal = streamData.GetField("path").IsNull() ? streamData.GetField("m_Path") : streamData.GetField("path");
                    
                    std::uint64_t offset = offsetVal.AsUInt();
                    std::uint32_t size = static_cast<std::uint32_t>(sizeVal.AsUInt());
                    std::string path = pathVal.AsString();
                    
                    if (size > 0 && !path.empty()) {
                        std::filesystem::path p(path);
                        std::string filename = p.filename().string();
                        if (m_dataResolver) {
                            imgVal.type = UnityValue::ByteArrayVal;
                            if (!m_dataResolver->ReadData(filename, offset, size, imgVal.byteData)) {
                                VortigauntLog::LogF("WARNING: Failed to read Texture2D stream data from %s\n", filename.c_str());
                            }
                        }
                    }
                }
            }
        } else if (objInfo.classId == UnityClass::Mesh) {
            auto itVData = root.objectVal.find("m_VertexData");
            if (itVData != root.objectVal.end() && itVData->second.type == UnityValue::ObjectVal) {
                auto& vData = itVData->second;
                auto& dataSizeVal = vData.objectVal["m_DataSize"];
                if (dataSizeVal.IsNull() || (dataSizeVal.type == UnityValue::ByteArrayVal && dataSizeVal.byteData.empty())) {
                    const auto& streamData = root.GetField("m_StreamData");
                    if (!streamData.IsNull()) {
                        const auto& offsetVal = streamData.GetField("offset").IsNull() ? streamData.GetField("m_Offset") : streamData.GetField("offset");
                        const auto& sizeVal = streamData.GetField("size").IsNull() ? streamData.GetField("m_Size") : streamData.GetField("size");
                        const auto& pathVal = streamData.GetField("path").IsNull() ? streamData.GetField("m_Path") : streamData.GetField("path");
                        
                        std::uint64_t offset = offsetVal.AsUInt();
                        std::uint32_t size = static_cast<std::uint32_t>(sizeVal.AsUInt());
                        std::string path = pathVal.AsString();
                        
                        if (size > 0 && !path.empty()) {
                            std::filesystem::path p(path);
                            std::string filename = p.filename().string();
                            if (m_dataResolver) {
                                dataSizeVal.type = UnityValue::ByteArrayVal;
                                if (!m_dataResolver->ReadData(filename, offset, size, dataSizeVal.byteData)) {
                                    VortigauntLog::LogF("WARNING: Failed to read Mesh stream data from %s\n", filename.c_str());
                                }
                            }
                        }
                    }
                }
            }
        }
    }

    return root;
}

std::string UnityAssetParser::GetObjectName(const UnityObjectInfo& objInfo) const {
    if (objInfo.typeIndex >= m_typesList.size()) {
        return "";
    }
    const UnityType& utype = m_typesList[objInfo.typeIndex];

    std::uint64_t absoluteOffset = m_dataOffset + objInfo.offset;
    if (absoluteOffset + objInfo.size > m_fileData.size()) {
        return "";
    }

    const char* dataPtr = m_fileData.data() + absoluteOffset;
    membuf buf(dataPtr, objInfo.size);
    std::istream stream(&buf);

    if (utype.nodes.empty()) {
        if (objInfo.classId == UnityClass::Texture2D || objInfo.classId == UnityClass::Mesh || objInfo.classId == UnityClass::AnimationClip) {
            return ReadAlignedString(stream, objInfo.size);
        }
    } else {
        // If type tree is present, look at the nodes to locate "m_Name" and its offset/type.
        // In almost all named objects, "m_Name" is the first child field at depth 1.
        bool foundNameNode = false;
        for (size_t i = 1; i < utype.nodes.size(); ++i) {
            std::string fieldName = ResolveString(utype.nodes[i].nameOffset, utype.localStringBuffer);
            if (fieldName == "m_Name") {
                foundNameNode = true;
                break;
            }
        }

        if (foundNameNode) {
            size_t nodeIdx = 0;
            // The root node is a container (depth 0)
            const TypeTreeNode& rootNode = utype.nodes[nodeIdx];
            ++nodeIdx; // skip root
            
            while (nodeIdx < utype.nodes.size() && utype.nodes[nodeIdx].depth > rootNode.depth) {
                if (stream.eof() || stream.fail()) break;
                const TypeTreeNode& childNode = utype.nodes[nodeIdx];
                std::string fieldName = ResolveString(childNode.nameOffset, utype.localStringBuffer);
                UnityValue child = DeserializeNode(utype.nodes, utype.localStringBuffer, nodeIdx, stream, m_endianness == 1);
                if (fieldName == "m_Name") {
                    return child.AsString();
                }
            }
        }
    }
    return "";
}


UnityValue UnityAssetParser::DeserializeNode(const std::vector<TypeTreeNode>& nodes,
                                            const std::vector<char>& stringBuffer,
                                            size_t& nodeIdx,
                                            std::istream& in, bool bigEndian) const {
    if (nodeIdx >= nodes.size()) return UnityValue{};
    if (in.eof() || in.fail()) return UnityValue{};

    const TypeTreeNode& cur = nodes[nodeIdx];
    ++nodeIdx;

    UnityValue val;
    std::string typeName = ResolveString(cur.typeOffset, stringBuffer);
    bool align = (cur.metaFlags & 0x4000) != 0;

    if (typeName == "int" || typeName == "int32" || typeName == "SInt32") {
        std::int32_t tmp = 0;
        in.read(reinterpret_cast<char*>(&tmp), 4);
        if (bigEndian) tmp = ((tmp & 0xFF) << 24) | ((tmp & 0xFF00) << 8) | ((tmp & 0xFF0000) >> 8) | ((tmp & 0xFF000000) >> 24);
        val.type = UnityValue::IntVal;
        val.intVal = tmp;
    } else if (typeName == "unsigned int" || typeName == "uint" || typeName == "uint32" || typeName == "UInt32") {
        std::uint32_t tmp = 0;
        in.read(reinterpret_cast<char*>(&tmp), 4);
        if (bigEndian) tmp = ((tmp & 0xFF) << 24) | ((tmp & 0xFF00) << 8) | ((tmp & 0xFF0000) >> 8) | ((tmp & 0xFF000000) >> 24);
        val.type = UnityValue::UIntVal;
        val.uintVal = tmp;
    } else if (typeName == "float") {
        float f = 0.0f;
        in.read(reinterpret_cast<char*>(&f), 4);
        if (bigEndian) {
            std::uint32_t bits = *reinterpret_cast<std::uint32_t*>(&f);
            bits = Utils::Swap32(bits);
            f = *reinterpret_cast<float*>(&bits);
        }
        val.type = UnityValue::FloatVal;
        val.doubleVal = f;
    } else if (typeName == "double") {
        double d = 0.0;
        in.read(reinterpret_cast<char*>(&d), 8);
        if (bigEndian) {
            std::uint64_t bits = *reinterpret_cast<std::uint64_t*>(&d);
            bits = Utils::Swap64(bits);
            d = *reinterpret_cast<double*>(&bits);
        }
        val.type = UnityValue::DoubleVal;
        val.doubleVal = d;
    } else if (typeName == "bool") {
        std::uint8_t b = 0;
        in.read(reinterpret_cast<char*>(&b), 1);
        val.type = UnityValue::BoolVal;
        val.boolVal = (b != 0);
    } else if (typeName == "char" || typeName == "uint8" || typeName == "uint8_t" || typeName == "SInt8" || typeName == "int8" || typeName == "unsigned char") {
        std::uint8_t b = 0;
        in.read(reinterpret_cast<char*>(&b), 1);
        val.type = UnityValue::IntVal;
        val.intVal = static_cast<std::int8_t>(b);
    } else if (typeName == "short" || typeName == "int16" || typeName == "SInt16") {
        std::int16_t tmp = 0;
        in.read(reinterpret_cast<char*>(&tmp), 2);
        if (bigEndian) tmp = (tmp >> 8) | (tmp << 8);
        val.type = UnityValue::IntVal;
        val.intVal = tmp;
    } else if (typeName == "uint16" || typeName == "UInt16") {
        std::uint16_t tmp = 0;
        in.read(reinterpret_cast<char*>(&tmp), 2);
        if (bigEndian) tmp = Utils::Swap16(tmp);
        val.type = UnityValue::UIntVal;
        val.uintVal = tmp;
    } else if (typeName == "long long" || typeName == "int64" || typeName == "SInt64") {
        std::int64_t tmp = 0;
        in.read(reinterpret_cast<char*>(&tmp), 8);
        if (bigEndian) {
            std::uint64_t bits = *reinterpret_cast<std::uint64_t*>(&tmp);
            bits = Utils::Swap64(bits);
            tmp = static_cast<std::int64_t>(bits);
        }
        val.type = UnityValue::IntVal;
        val.intVal = tmp;
    } else if (typeName == "unsigned long long" || typeName == "uint64" || typeName == "UInt64") {
        std::uint64_t tmp = 0;
        in.read(reinterpret_cast<char*>(&tmp), 8);
        if (bigEndian) {
            tmp = Utils::Swap64(tmp);
        }
        val.type = UnityValue::UIntVal;
        val.uintVal = tmp;
    } else if (typeName == "string") {
        std::int32_t len = 0;
        in.read(reinterpret_cast<char*>(&len), 4);
        if (bigEndian) len = Utils::Swap32(len);
        val.type = UnityValue::StringVal;
        if (len > 0) {
            std::streampos sPos = in.tellg();
            in.seekg(0, std::ios::end);
            std::int64_t rem = static_cast<std::int64_t>(in.tellg() - sPos);
            in.seekg(sPos);
            if (len > rem || len > 100000000LL) {
                in.setstate(std::ios::failbit);
            } else {
                std::string s(len, '\0');
                in.read(&s[0], len);
                val.stringVal = std::move(s);
            }
        }
        AlignStream(in, 4);
    } else if (typeName == "Array" || cur.isArray) {
        size_t sizeNodeIdx = nodeIdx;
        UnityValue sizeVal = DeserializeNode(nodes, stringBuffer, nodeIdx, in, bigEndian);
        std::int64_t arrSize = sizeVal.AsInt();

        size_t elementNodeIdx = nodeIdx;
        size_t nextNodeIdx = elementNodeIdx + 1;
        while (nextNodeIdx < nodes.size() && nodes[nextNodeIdx].depth > nodes[elementNodeIdx].depth) {
            nextNodeIdx++;
        }

        // Optimize 1-byte raw byte array parsing
        bool isByteArray = false;
        if (elementNodeIdx < nodes.size()) {
            std::string elemType = ResolveString(nodes[elementNodeIdx].typeOffset, stringBuffer);
            if (elemType == "char" || elemType == "uint8" || elemType == "uint8_t" || elemType == "SInt8" || elemType == "unsigned char") {
                isByteArray = true;
            }
        }

        if (isByteArray) {
            val.type = UnityValue::ByteArrayVal;
            if (arrSize > 0) {
                std::streampos sPos = in.tellg();
                in.seekg(0, std::ios::end);
                std::int64_t rem = static_cast<std::int64_t>(in.tellg() - sPos);
                in.seekg(sPos);
                if (arrSize > rem || arrSize > 100000000LL) {
                    in.setstate(std::ios::failbit);
                } else {
                    val.byteData.resize(arrSize);
                    in.read(reinterpret_cast<char*>(val.byteData.data()), arrSize);
                }
            }
            nodeIdx = nextNodeIdx;
            AlignStream(in, 4);
        } else {
            val.type = UnityValue::ArrayVal;
            if (arrSize > 0) {
                std::streampos sPos = in.tellg();
                in.seekg(0, std::ios::end);
                std::int64_t rem = static_cast<std::int64_t>(in.tellg() - sPos);
                in.seekg(sPos);
                if (arrSize > rem || arrSize > 10000000LL) {
                    in.setstate(std::ios::failbit);
                } else {
                    val.arrayVal.reserve(arrSize);
                    for (std::int64_t i = 0; i < arrSize; ++i) {
                        if (in.eof() || in.fail()) break;
                        size_t tempIdx = elementNodeIdx;
                        val.arrayVal.push_back(DeserializeNode(nodes, stringBuffer, tempIdx, in, bigEndian));
                    }
                }
            }
            nodeIdx = nextNodeIdx;
        }
    } else {
        val.type = UnityValue::ObjectVal;
        while (nodeIdx < nodes.size() && nodes[nodeIdx].depth > cur.depth) {
            if (in.eof() || in.fail()) break;
            const TypeTreeNode& childNode = nodes[nodeIdx];
            std::string fieldName = ResolveString(childNode.nameOffset, stringBuffer);
            UnityValue child = DeserializeNode(nodes, stringBuffer, nodeIdx, in, bigEndian);
            val.objectVal.emplace(std::move(fieldName), std::move(child));
        }
    }

    if (align) {
        AlignStream(in, 4);
    }

    return val;
}

uint16_t UnityAssetParser::ReadU16(std::istream& in) const {
    return Utils::ReadU16(in, m_endianness == 1);
}

uint32_t UnityAssetParser::ReadU32(std::istream& in) const {
    return Utils::ReadU32(in, m_endianness == 1);
}

int32_t UnityAssetParser::ReadS32(std::istream& in) const {
    return Utils::ReadS32(in, m_endianness == 1);
}

uint64_t UnityAssetParser::ReadU64(std::istream& in) const {
    return Utils::ReadU64(in, m_endianness == 1);
}

int64_t UnityAssetParser::ReadS64(std::istream& in) const {
    return Utils::ReadS64(in, m_endianness == 1);
}

void UnityAssetParser::AlignStream(std::istream& in, int alignment) const {
    if (!in || alignment <= 0) return;
    std::streampos pos = in.tellg();
    if (pos < 0) return;
    std::streamoff offset = pos % alignment;
    if (offset != 0) {
        in.seekg(alignment - offset, std::ios::cur);
    }
}

std::string UnityAssetParser::ReadAlignedString(std::istream& in, std::uint32_t maxSize) const {
    std::int32_t len = ReadS32(in);
    std::streampos curr = in.tellg();
    std::int64_t remaining = static_cast<std::int64_t>(maxSize) - curr;
    if (len <= 0 || len > 10000000 || len > remaining) {
        return "";
    }
    std::string s(len, '\0');
    if (len > 0) {
        in.read(&s[0], len);
    }
    AlignStream(in, 4);
    return s;
}

std::vector<std::uint8_t> UnityAssetParser::ReadByteArray(std::istream& in, std::uint32_t maxSize) const {
    std::int32_t len = ReadS32(in);
    std::streampos curr = in.tellg();
    std::int64_t remaining = static_cast<std::int64_t>(maxSize) - curr;
    if (len <= 0 || len > 100000000 || len > remaining) {
        AlignStream(in, 4);
        return {};
    }
    std::vector<std::uint8_t> data(len);
    if (len > 0) {
        in.read(reinterpret_cast<char*>(data.data()), len);
    }
    AlignStream(in, 4);
    return data;
}

std::vector<float> UnityAssetParser::ReadFloatArray(std::istream& in, std::uint32_t maxSize) const {
    std::int32_t len = ReadS32(in);
    std::streampos curr = in.tellg();
    std::int64_t remaining = static_cast<std::int64_t>(maxSize) - curr;
    if (len <= 0 || len > 100000000 || (static_cast<std::int64_t>(len) * 4) > remaining) {
        return {};
    }
    std::vector<float> data(len);
    for (std::int32_t i = 0; i < len; ++i) {
        float f = 0.0f;
        in.read(reinterpret_cast<char*>(&f), 4);
        if (m_endianness == 1) {
            std::uint32_t bits = *reinterpret_cast<std::uint32_t*>(&f);
            bits = Utils::Swap32(bits);
            f = *reinterpret_cast<float*>(&bits);
        }
        data[i] = f;
    }
    return data;
}

std::vector<std::uint32_t> UnityAssetParser::ReadUint32Array(std::istream& in, std::uint32_t maxSize) const {
    std::int32_t len = ReadS32(in);
    std::streampos curr = in.tellg();
    std::int64_t remaining = static_cast<std::int64_t>(maxSize) - curr;
    if (len <= 0 || len > 100000000 || (static_cast<std::int64_t>(len) * 4) > remaining) {
        return {};
    }
    std::vector<std::uint32_t> data(len);
    for (std::int32_t i = 0; i < len; ++i) {
        data[i] = ReadU32(in);
    }
    return data;
}

int32_t UnityAssetParser::GetLocalClassId(int64_t pathID) const {
    auto it = m_localPathIdToClassId.find(pathID);
    if (it != m_localPathIdToClassId.end()) return it->second;
    return -1;
}

PackedFloatVector UnityAssetParser::ReadPackedFloatVector(std::istream& in, std::uint32_t maxSize) const {
    PackedFloatVector pfv;
    pfv.numItems = ReadU32(in);
    in.read(reinterpret_cast<char*>(&pfv.range), 4);
    in.read(reinterpret_cast<char*>(&pfv.start), 4);
    if (m_endianness == 1) {
        std::uint32_t bits = *reinterpret_cast<std::uint32_t*>(&pfv.range);
        bits = Utils::Swap32(bits);
        pfv.range = *reinterpret_cast<float*>(&bits);
        bits = *reinterpret_cast<std::uint32_t*>(&pfv.start);
        bits = Utils::Swap32(bits);
        pfv.start = *reinterpret_cast<float*>(&bits);
    }
    pfv.data = ReadByteArray(in, maxSize);
    in.read(reinterpret_cast<char*>(&pfv.bitSize), 1);
    AlignStream(in, 4);
    return pfv;
}

PackedIntVector UnityAssetParser::ReadPackedIntVector(std::istream& in, std::uint32_t maxSize) const {
    PackedIntVector piv;
    piv.numItems = ReadU32(in);
    piv.data = ReadByteArray(in, maxSize);
    in.read(reinterpret_cast<char*>(&piv.bitSize), 1);
    AlignStream(in, 4);
    return piv;
}

UnityValue UnityAssetParser::ReadPPtr(std::istream& in) const {
    std::int32_t fileID = ReadS32(in);
    std::int64_t pathID = (m_formatVersion < 14) ? ReadS32(in) : ReadS64(in);
    return MakePPtrValue(fileID, pathID);
}

float UnityValue::AsFloat() const {
    return static_cast<float>(doubleVal);
}

double UnityValue::AsDouble() const {
    return doubleVal;
}

const UnityValue& UnityValue::GetField(const std::string& name) const {
    auto it = objectVal.find(name);
    if (it != objectVal.end()) {
        return it->second;
    }
    static UnityValue nullVal;
    return nullVal;
}

std::vector<std::uint8_t> UnityValue::AsByteArray() const {
    if (type == ByteArrayVal) {
        return byteData;
    }
    if (type == ArrayVal) {
        std::vector<std::uint8_t> bytes;
        bytes.reserve(arrayVal.size());
        for (const auto& v : arrayVal) {
            bytes.push_back(static_cast<std::uint8_t>(v.AsInt()));
        }
        return bytes;
    }
    if (type == StringVal) {
        return std::vector<std::uint8_t>(stringVal.begin(), stringVal.end());
    }
    return {};
}
