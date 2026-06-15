#pragma once

#include <string>
#include <vector>
#include <cstdint>

// Forward declarations
struct UnityObjectInfo;
class UnityAssetParser;

enum class VertexFormat {
    Float,
    Float16,
    UNorm8,
    SNorm8,
    UNorm16,
    SNorm16,
    UInt8,
    SInt8,
    UInt16,
    SInt16,
    UInt32,
    SInt32
};

struct MeshVertex {
    float x = 0.0f, y = 0.0f, z = 0.0f;
    float nx = 0.0f, ny = 0.0f, nz = 0.0f;
    float u = 0.0f, v = 0.0f;
};

struct MeshSubMesh {
    std::uint32_t firstByte;
    std::uint32_t indexCount;
    std::uint32_t topology;
    std::uint32_t baseVertex;
    std::uint32_t firstVertex;
    std::uint32_t vertexCount;
};

struct MeshChannel {
    std::uint8_t stream;
    std::uint8_t offset;
    std::uint8_t format;
    std::uint8_t dimension;
};

class UnityMeshConverter {
public:
    // Convert a Unity mesh object to GoldSrc .smd file.
    static void Convert(const UnityObjectInfo& objInfo, const UnityAssetParser& parser, const std::string& outputDir, const std::vector<std::string>& textures = {});
};
