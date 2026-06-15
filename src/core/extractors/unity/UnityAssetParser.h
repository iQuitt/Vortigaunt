#pragma once

#include <cstring>
#include <vector>
#include <ankerl/unordered_dense.h>
#include <memory>
#include <cstdint>
#include <istream>
#include <filesystem>
#include <string>

#include "UnityDataResolver.h"

// Forward declarations
struct UnityObjectInfo;
struct TypeTreeNode;
struct UnityType;
struct UnityValue;

// TypeTreeNode definition
struct TypeTreeNode {
    uint16_t version;
    uint8_t depth;
    uint8_t isArray;
    uint32_t typeOffset;
    uint32_t nameOffset;
    int32_t size;
    uint32_t index;
    uint32_t metaFlags;
    uint64_t refTypeHash;
    std::string typeName;
    std::string fieldName;
};

// UnityType definition
struct UnityType {
    int32_t classId;
    bool isStrippedType;
    int16_t scriptTypeIndex;
    uint8_t scriptId[16]; // script hash
    uint8_t oldTypeHash[16];
    std::vector<TypeTreeNode> nodes;
    std::vector<char> localStringBuffer; // Per-type string buffer
};

// UnityValue definition
struct UnityValue {
    enum Type {
        NullVal,
        BoolVal,
        IntVal,
        UIntVal,
        FloatVal,
        DoubleVal,
        StringVal,
        ArrayVal,
        ObjectVal,
        ByteArrayVal
    };
    Type type = NullVal;
    bool boolVal = false;
    int64_t intVal = 0;
    uint64_t uintVal = 0;
    double doubleVal = 0.0;
    std::string stringVal;
    std::vector<UnityValue> arrayVal;
    ankerl::unordered_dense::map<std::string, UnityValue> objectVal;
    std::vector<uint8_t> byteData; // Raw byte data for ByteArrayVal

    bool IsNull() const { return type == NullVal; }
    bool AsBool() const { return boolVal; }
    int64_t AsInt() const {
        if (type == UIntVal) return static_cast<int64_t>(uintVal);
        return intVal;
    }
    uint64_t AsUInt() const {
        if (type == IntVal) return static_cast<uint64_t>(intVal);
        return uintVal;
    }
    float AsFloat() const;
    double AsDouble() const;
    const std::string& AsString() const { return stringVal; }
    const std::vector<UnityValue>& AsArray() const { return arrayVal; }
    const ankerl::unordered_dense::map<std::string, UnityValue>& AsObject() const { return objectVal; }
    const UnityValue& GetField(const std::string& name) const;
    std::vector<uint8_t> AsByteArray() const;
};

struct UnityObjectInfo {
    int64_t pathId;
    uint64_t offset;       // Byte offset relative to data section start
    uint32_t size;
    int32_t classId;       // Resolved from typeIndex for version >= 17
    uint32_t typeIndex;
    std::string name;      // Object name (resolved after deserialization if available)
};

struct PackedFloatVector {
    std::uint32_t numItems = 0;
    float range = 0.0f;
    float start = 0.0f;
    std::vector<std::uint8_t> data;
    std::uint8_t bitSize = 0;
};

struct PackedIntVector {
    std::uint32_t numItems = 0;
    std::vector<std::uint8_t> data;
    std::uint8_t bitSize = 0;
};

// UnityAssetParser class
class UnityAssetParser {
public:
    UnityAssetParser() = default;
    void SetDataResolver(UnityDataResolver* resolver) { m_dataResolver = resolver; }
    const UnityDataResolver* GetDataResolver() const { return m_dataResolver; }
    const std::vector<char>& GetFileData() const { return m_fileData; }

    void SetPathIdToClassMap(const ankerl::unordered_dense::map<int64_t, int32_t>* map) { m_pathIdToClassId = map; }
    void SetTextureNamesMap(const ankerl::unordered_dense::map<int64_t, std::string>* map) { m_textureNames = map; }

    bool Load(const std::vector<char>& data);
    bool Load(std::vector<char>&& data);
    bool LoadFromFile(const std::string& filePath);
    bool LoadFromMultipleFiles(const std::vector<std::string>& files);

    const std::vector<UnityObjectInfo>& GetObjects() const { return m_objects; }
    const std::vector<UnityType>& GetTypesList() const { return m_typesList; }

    UnityValue DeserializeObject(const UnityObjectInfo& objInfo) const;
    std::string ResolveString(uint32_t offset, const std::vector<char>& localBuffer) const;
    std::string GetObjectName(const UnityObjectInfo& objInfo) const;

    uint32_t GetFormatVersion() const { return m_formatVersion; }
    const std::string& GetUnityVersion() const { return m_unityVersion; }
    void GetUnityVersionParts(int& major, int& minor) const;
    uint64_t GetDataOffset() const { return m_dataOffset; }
    bool IsBigEndian() const { return m_endianness == 1; }

private:
    bool ParseHeader(std::istream& in);
    bool ParseMetadata(std::istream& in);
    bool ParseTypesV17(std::istream& in);
    bool ParseObjectsV17(std::istream& in);
    bool ReadTypeTree(std::istream& in, UnityType& ut);
    bool ReadSerializedType(std::istream& in, UnityType& ut, bool isRefType);
    UnityValue DeserializeNode(const std::vector<TypeTreeNode>& nodes,
                               const std::vector<char>& stringBuffer,
                               size_t& nodeIdx,
                               std::istream& in, bool bigEndian) const;

    // Endian-aware read helpers
    uint16_t ReadU16(std::istream& in) const;
    uint32_t ReadU32(std::istream& in) const;
    int32_t ReadS32(std::istream& in) const;
    uint64_t ReadU64(std::istream& in) const;
    int64_t ReadS64(std::istream& in) const;
    void AlignStream(std::istream& in, int alignment = 4) const;

    // Helper methods for manual deserialization
    std::string ReadAlignedString(std::istream& in, std::uint32_t maxSize) const;
    std::vector<std::uint8_t> ReadByteArray(std::istream& in, std::uint32_t maxSize) const;
    std::vector<float> ReadFloatArray(std::istream& in, std::uint32_t maxSize) const;
    std::vector<std::uint32_t> ReadUint32Array(std::istream& in, std::uint32_t maxSize) const;
    int32_t GetClassId(int64_t pathID) const;
    int32_t GetLocalClassId(int64_t pathID) const;
    PackedFloatVector ReadPackedFloatVector(std::istream& in, std::uint32_t maxSize) const;
    PackedIntVector ReadPackedIntVector(std::istream& in, std::uint32_t maxSize) const;
    UnityValue ReadPPtr(std::istream& in) const;
    void SkipStringArray(std::istream& in, std::uint32_t maxSize) const;
    UnityValue DeserializeRenderer(std::istream& in) const;

    std::vector<char> m_fileData;
    uint64_t m_fileSize = 0;
    uint32_t m_formatVersion = 0;
    uint64_t m_metadataSize = 0;
    uint64_t m_dataOffset = 0;
    uint8_t m_endianness = 0; // 0 = little, 1 = big

    std::string m_unityVersion;
    uint32_t m_targetPlatform = 0;
    bool m_enableTypeTree = false;

    std::vector<UnityType> m_typesList;        // Indexed by type array position
    std::vector<UnityObjectInfo> m_objects;
    ankerl::unordered_dense::map<int64_t, int32_t> m_localPathIdToClassId;
    UnityDataResolver* m_dataResolver = nullptr;
    const ankerl::unordered_dense::map<int64_t, int32_t>* m_pathIdToClassId = nullptr;
    const ankerl::unordered_dense::map<int64_t, std::string>* m_textureNames = nullptr;
};
