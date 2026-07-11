#include "UnityMeshConverter.h"
#include "UnityAssetParser.h"
#include "VortigauntLog.h"
#include "core/smd/SmdWriter.h"
#include "utils/BinaryUtils.h"
#include <algorithm>
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstring>
#include <cmath>


struct PackedBits {
    std::uint32_t numItems = 0;
    float range = 0.0f, start = 0.0f;
    std::vector<std::uint8_t> data;
    std::uint32_t bitSize = 0;
};

static VertexFormat ToVertexFormat(int format, int version_major) {
    if (version_major < 2017) {
        switch (format) {
            case 0: return VertexFormat::Float;
            case 1: return VertexFormat::Float16;
            case 2: return VertexFormat::UNorm8;
            case 3: return VertexFormat::UInt8;
            case 4: return VertexFormat::UInt32;
            default: return VertexFormat::Float;
        }
    } else if (version_major < 2019) {
        switch (format) {
            case 0: return VertexFormat::Float;
            case 1: return VertexFormat::Float16;
            case 2:
            case 3: return VertexFormat::UNorm8;
            case 4: return VertexFormat::SNorm8;
            case 5: return VertexFormat::UNorm16;
            case 6: return VertexFormat::SNorm16;
            case 7: return VertexFormat::UInt8;
            case 8: return VertexFormat::SInt8;
            case 9: return VertexFormat::UInt16;
            case 10: return VertexFormat::SInt16;
            case 11: return VertexFormat::UInt32;
            case 12: return VertexFormat::SInt32;
            default: return VertexFormat::Float;
        }
    } else {
        return static_cast<VertexFormat>(format);
    }
}

static std::size_t GetFormatSize(VertexFormat format) {
    switch (format) {
        case VertexFormat::Float:
        case VertexFormat::UInt32:
        case VertexFormat::SInt32:
            return 4;
        case VertexFormat::Float16:
        case VertexFormat::UNorm16:
        case VertexFormat::SNorm16:
        case VertexFormat::UInt16:
        case VertexFormat::SInt16:
            return 2;
        case VertexFormat::UNorm8:
        case VertexFormat::SNorm8:
        case VertexFormat::UInt8:
        case VertexFormat::SInt8:
            return 1;
        default:
            return 4;
    }
}

static float ReadChannelComponent(const std::uint8_t* streamData, std::size_t offset, VertexFormat format, bool bigEndian) {
    switch (format) {
        case VertexFormat::Float: {
            std::uint32_t u = Utils::Read32(streamData + offset, bigEndian);
            float f = 0.0f;
            std::memcpy(&f, &u, 4);
            return f;
        }
        case VertexFormat::Float16: {
            std::uint16_t h = Utils::Read16(streamData + offset, bigEndian);
            return Utils::Float16ToFloat32(h);
        }
        case VertexFormat::UNorm8:
            return static_cast<float>(streamData[offset]) / 255.0f;
        case VertexFormat::SNorm8: {
            float f = static_cast<float>(static_cast<std::int8_t>(streamData[offset])) / 127.0f;
            return (f < -1.0f) ? -1.0f : f;
        }
        case VertexFormat::UNorm16: {
            std::uint16_t u = Utils::Read16(streamData + offset, bigEndian);
            return static_cast<float>(u) / 65535.0f;
        }
        case VertexFormat::SNorm16: {
            std::int16_t s = static_cast<std::int16_t>(Utils::Read16(streamData + offset, bigEndian));
            float f = static_cast<float>(s) / 32767.0f;
            return (f < -1.0f) ? -1.0f : f;
        }
        case VertexFormat::UInt8:
            return static_cast<float>(streamData[offset]);
        case VertexFormat::SInt8:
            return static_cast<float>(static_cast<std::int8_t>(streamData[offset]));
        case VertexFormat::UInt16: {
            std::uint16_t u = Utils::Read16(streamData + offset, bigEndian);
            return static_cast<float>(u);
        }
        case VertexFormat::SInt16: {
            std::int16_t s = static_cast<std::int16_t>(Utils::Read16(streamData + offset, bigEndian));
            return static_cast<float>(s);
        }
        case VertexFormat::UInt32: {
            std::uint32_t u = Utils::Read32(streamData + offset, bigEndian);
            return static_cast<float>(u);
        }
        case VertexFormat::SInt32: {
            std::int32_t s = static_cast<std::int32_t>(Utils::Read32(streamData + offset, bigEndian));
            return static_cast<float>(s);
        }
        default:
            return 0.0f;
    }
}

static void ReadChannelVec(const std::vector<std::uint8_t>& rawBuf, const MeshChannel& ch, std::size_t stride, std::size_t streamStart, std::uint32_t vertexIndex, int versionMajor, bool bigEndian, int componentCount, float* out)
{
    VertexFormat vf = ToVertexFormat(ch.format, versionMajor);
    std::size_t fmtSize = GetFormatSize(vf);
    std::size_t vertOffset = streamStart + static_cast<std::size_t>(vertexIndex) * stride + ch.offset;

    if (vertOffset + ch.dimension * fmtSize > rawBuf.size()) {
        return;
    }
    for (int c = 0; c < componentCount; ++c) {
        out[c] = ReadChannelComponent(rawBuf.data(), vertOffset + c * fmtSize, vf, bigEndian);
    }
}

static float ToTolerantFloat(const UnityValue& v) {
    switch (v.type) {
        case UnityValue::FloatVal:
        case UnityValue::DoubleVal:
            return v.AsFloat();
        case UnityValue::IntVal:
            return static_cast<float>(v.intVal);
        case UnityValue::UIntVal:
            return static_cast<float>(v.uintVal);
        default:
            return 0.0f;
    }
}

static PackedBits ReadPackedValue(const UnityValue& v) {
    PackedBits p;
    if (v.IsNull()) {
        return p;
    }
    std::uint64_t numItems = v.GetField("m_NumItems").AsUInt();
    if (numItems > 100000000ull) {
        numItems = 100000000ull;
    }
    p.range = ToTolerantFloat(v.GetField("m_Range"));
    p.start = ToTolerantFloat(v.GetField("m_Start"));
    p.data = v.GetField("m_Data").AsByteArray();
    std::uint64_t bitSize = v.GetField("m_BitSize").AsUInt();
    // Values are unpacked into 32-bit integers; wider bit sizes never occur in valid files.
    p.bitSize = static_cast<std::uint32_t>(bitSize > 32 ? 32 : bitSize);
    // Untrusted file data: never let the item count exceed what m_Data can actually
    // hold, so allocations sized from it stay bounded by the real payload.
    if (p.bitSize > 0) {
        std::uint64_t maxItems = static_cast<std::uint64_t>(p.data.size()) * 8ull / p.bitSize;
        if (numItems > maxItems) {
            numItems = maxItems;
        }
    } else if (numItems > 4000000ull) {
        // bitSize == 0 vectors carry no backing data (every value decodes to the
        // start constant), so keep a tighter sanity cap on their count.
        numItems = 4000000ull;
    }
    p.numItems = static_cast<std::uint32_t>(numItems);
    return p;
}

// Unpacks the raw bit-packed values. Returns fewer than numItems entries when the
// backing data is truncated. Callers must handle bitSize == 0 themselves.
static std::vector<std::uint32_t> UnpackRawValues(const PackedBits& p) {
    std::vector<std::uint32_t> values;
    if (p.numItems == 0 || p.bitSize == 0) {
        return values;
    }
    values.reserve(p.numItems);
    const std::uint32_t mask = (p.bitSize >= 32) ? 0xFFFFFFFFu : ((1u << p.bitSize) - 1u);
    std::size_t indexPos = 0;
    std::uint32_t bitPos = 0;
    for (std::uint32_t i = 0; i < p.numItems; ++i) {
        std::uint32_t value = 0;
        std::uint32_t bits = 0;
        while (bits < p.bitSize) {
            if (indexPos >= p.data.size()) {
                return values;
            }
            value |= (static_cast<std::uint32_t>(p.data[indexPos]) >> bitPos) << bits;
            std::uint32_t num = std::min(p.bitSize - bits, 8u - bitPos);
            bitPos += num;
            bits += num;
            if (bitPos == 8) {
                ++indexPos;
                bitPos = 0;
            }
        }
        values.push_back(value & mask);
    }
    return values;
}

static std::vector<std::int32_t> UnpackInts(const PackedBits& p) {
    std::vector<std::int32_t> result;
    if (p.numItems == 0) {
        return result;
    }
    if (p.bitSize == 0) {
        result.assign(p.numItems, 0);
        return result;
    }
    std::vector<std::uint32_t> raw = UnpackRawValues(p);
    result.reserve(raw.size());
    for (std::uint32_t v : raw) {
        result.push_back(static_cast<std::int32_t>(v));
    }
    return result;
}

static std::vector<float> UnpackFloats(const PackedBits& p) {
    std::vector<float> result;
    if (p.numItems == 0) {
        return result;
    }
    if (p.bitSize == 0) {
        result.assign(p.numItems, p.start);
        return result;
    }
    const std::uint32_t maxValue = (p.bitSize >= 32) ? 0xFFFFFFFFu : ((1u << p.bitSize) - 1u);
    std::vector<std::uint32_t> raw = UnpackRawValues(p);
    result.reserve(raw.size());
    for (std::uint32_t v : raw) {
        result.push_back(static_cast<float>(v) / static_cast<float>(maxValue) * p.range + p.start);
    }
    return result;
}

void UnityMeshConverter::Convert(const UnityObjectInfo& objInfo,
                                 const UnityAssetParser& parser,
                                 const std::string& outputDir,
                                 const std::vector<std::string>& textures,
                                 bool texturesAreBmp)
{
    UnityValue root = parser.DeserializeObject(objInfo);
    bool bigEndian = parser.IsBigEndian();

    // Get Index Buffer
    std::int64_t indexFormat = root.GetField("m_IndexFormat").AsInt();
    const auto& indexBuf = root.GetField("m_IndexBuffer").AsByteArray();

    std::vector<std::uint32_t> indices;
    if (indexFormat == 0) { // 16-bit
        std::size_t count = indexBuf.size() / 2;
        indices.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            indices[i] = Utils::Read16(indexBuf.data() + i * 2, bigEndian);
        }
    } else { // 32-bit
        std::size_t count = indexBuf.size() / 4;
        indices.resize(count);
        for (std::size_t i = 0; i < count; ++i) {
            indices[i] = Utils::Read32(indexBuf.data() + i * 4, bigEndian);
        }
    }

    // Get SubMeshes
    std::vector<MeshSubMesh> subMeshes;
    const auto& subMeshesVal = root.GetField("m_SubMeshes").AsArray();
    for (const auto& smVal : subMeshesVal) {
        MeshSubMesh sm;
        sm.firstByte = smVal.GetField("firstByte").AsUInt();
        sm.indexCount = smVal.GetField("indexCount").AsUInt();
        sm.topology = smVal.GetField("topology").AsUInt();
        sm.baseVertex = smVal.GetField("baseVertex").AsUInt();
        sm.firstVertex = smVal.GetField("firstVertex").IsNull() ? 0 : smVal.GetField("firstVertex").AsUInt();
        sm.vertexCount = smVal.GetField("vertexCount").IsNull() ? 0 : smVal.GetField("vertexCount").AsUInt();
        subMeshes.push_back(sm);
    }

    // Parse VertexData
    const auto& vData = root.GetField("m_VertexData");
    std::uint32_t vertexCount = vData.GetField("m_VertexCount").AsUInt();
    const auto& rawBuf = vData.GetField("m_DataSize").AsByteArray();

    int version_major = 0;
    int version_minor = 0;
    parser.GetUnityVersionParts(version_major, version_minor);

    std::vector<MeshChannel> channels;
    const auto& channelsVal = vData.GetField("m_Channels").AsArray();
    for (const auto& chVal : channelsVal) {
        MeshChannel c;
        c.stream = chVal.GetField("stream").AsUInt();
        c.offset = chVal.GetField("offset").AsUInt();
        c.format = chVal.GetField("format").AsUInt();
        c.dimension = chVal.GetField("dimension").AsUInt() & 0xF;
        channels.push_back(c);
    }

    // Compressed meshes carry their data in packed bit vectors instead of m_VertexData.
    const UnityValue& cm = root.GetField("m_CompressedMesh");
    PackedBits cmVerts = ReadPackedValue(cm.GetField("m_Vertices"));
    bool useCompressed = cmVerts.numItems > 0;

    if (!useCompressed && (vertexCount == 0 || indices.empty() || channels.empty())) {
        VortigauntLog::LogF("WARNING: Mesh PathID %lld has no vertex data or indices. Skipping.\n", static_cast<long long>(objInfo.pathId));
        return;
    }

    std::vector<MeshVertex> parsedVertices;
    if (useCompressed) {
        vertexCount = cmVerts.numItems / 3;
        std::string displayName = objInfo.name.empty() ? std::to_string(objInfo.pathId) : objInfo.name;
        VortigauntLog::LogF("Mesh '%s': decoding compressed mesh data (%u vertices)\n", displayName.c_str(), vertexCount);
        parsedVertices.assign(vertexCount, MeshVertex{});

        std::vector<float> positions = UnpackFloats(cmVerts);
        for (std::uint32_t i = 0; i < vertexCount; ++i) {
            std::size_t base = static_cast<std::size_t>(i) * 3;
            if (base + 2 >= positions.size()) break;
            parsedVertices[i].x = positions[base];
            parsedVertices[i].y = positions[base + 1];
            parsedVertices[i].z = positions[base + 2];
        }

        // Normals are stored as (x, y) pairs plus a sign bit for the reconstructed z.
        PackedBits cmNormals = ReadPackedValue(cm.GetField("m_Normals"));
        if (cmNormals.numItems > 0) {
            std::vector<float> normalData = UnpackFloats(cmNormals);
            std::vector<std::int32_t> signs = UnpackInts(ReadPackedValue(cm.GetField("m_NormalSigns")));
            if (normalData.size() >= static_cast<std::size_t>(vertexCount) * 2 && signs.size() >= vertexCount) {
                for (std::uint32_t i = 0; i < vertexCount; ++i) {
                    std::size_t base = static_cast<std::size_t>(i) * 2;
                    float x = normalData[base];
                    float y = normalData[base + 1];
                    float z = std::sqrt(std::max(0.0f, 1.0f - x * x - y * y));
                    if (signs[i] == 0) z = -z;
                    parsedVertices[i].nx = x;
                    parsedVertices[i].ny = y;
                    parsedVertices[i].nz = z;
                }
            }
        }

        PackedBits cmUV = ReadPackedValue(cm.GetField("m_UV"));
        if (cmUV.numItems > 0) {
            std::vector<float> uvData = UnpackFloats(cmUV);
            std::uint64_t uvInfo = cm.GetField("m_UVInfo").AsUInt();
            if (uvInfo != 0) {
                // Lowest 4 uvInfo bits describe UV channel 0: 2 bits dimension-1, 1 bit exists.
                std::uint32_t dim = static_cast<std::uint32_t>(uvInfo & 3) + 1;
                bool exists = (uvInfo & 4) != 0;
                if (exists) {
                    for (std::uint32_t i = 0; i < vertexCount; ++i) {
                        std::size_t base = static_cast<std::size_t>(i) * dim;
                        if (base >= uvData.size()) break;
                        parsedVertices[i].u = uvData[base];
                        parsedVertices[i].v = (dim >= 2 && base + 1 < uvData.size()) ? uvData[base + 1] : 0.0f;
                    }
                }
            } else if (cmUV.numItems >= static_cast<std::uint64_t>(vertexCount) * 2) {
                for (std::uint32_t i = 0; i < vertexCount; ++i) {
                    std::size_t base = static_cast<std::size_t>(i) * 2;
                    if (base + 1 >= uvData.size()) break;
                    parsedVertices[i].u = uvData[base];
                    parsedVertices[i].v = uvData[base + 1];
                }
            }
        }

        PackedBits cmTriangles = ReadPackedValue(cm.GetField("m_Triangles"));
        if (cmTriangles.numItems > 0) {
            std::vector<std::int32_t> triData = UnpackInts(cmTriangles);
            indices.resize(triData.size());
            for (std::size_t i = 0; i < triData.size(); ++i) {
                indices[i] = static_cast<std::uint32_t>(triData[i]);
            }
        }
    } else {
        // Untrusted file data: every vertex occupies at least one byte of the vertex
        // buffer, so a larger count can only come from a corrupt file. Bound it here
        // before it sizes the parsedVertices allocation below.
        if (static_cast<std::size_t>(vertexCount) > rawBuf.size()) {
            VortigauntLog::LogF("WARNING: Mesh PathID %lld vertex count %u exceeds vertex data size %zu. Skipping.\n",
                static_cast<long long>(objInfo.pathId), vertexCount, rawBuf.size());
            return;
        }
        std::unordered_map<int, std::size_t> streamStrides;
        for (const auto& chan : channels) {
            if (chan.dimension == 0) continue;
            int stream = chan.stream;
            VertexFormat vf = ToVertexFormat(chan.format, version_major);
            std::size_t chanSize = chan.dimension * GetFormatSize(vf);
            std::size_t maxOffset = chan.offset + chanSize;
            if (maxOffset > streamStrides[stream]) {
                streamStrides[stream] = maxOffset;
            }
        }

        std::unordered_map<int, std::size_t> streamOffsets;
        std::size_t currentOffset = 0;
        int maxStream = 0;
        for (const auto& chan : channels) {
            if (chan.dimension > 0 && chan.stream > maxStream) {
                maxStream = chan.stream;
            }
        }
        for (int s = 0; s <= maxStream; ++s) {
            streamOffsets[s] = currentOffset;
            if (streamStrides.count(s)) {
                std::size_t streamDataSize = vertexCount * streamStrides[s];
                currentOffset += streamDataSize;
                currentOffset = (currentOffset + 15) & ~15; // 16-byte aligned stream starts
            }
        }

        // Parse vertices
        parsedVertices.assign(vertexCount, MeshVertex{});
        // most unity assets gave wrong UV, so all have to do is mirror Y axis to fix it
        int uvChannelIndex = (version_major >= 2018) ? 4 : 3;
        for (std::uint32_t v = 0; v < vertexCount; ++v) {
            MeshVertex& vert = parsedVertices[v];

            // Position (Channel 0)
            if (channels.size() > 0 && channels[0].dimension >= 3) {
                const auto& ch = channels[0];
                float pos[3] = { 0.0f, 0.0f, 0.0f };
                ReadChannelVec(rawBuf, ch, streamStrides[ch.stream], streamOffsets[ch.stream], v, version_major, bigEndian, 3, pos);
                vert.x = pos[0];
                vert.y = pos[1];
                vert.z = pos[2];
            }

            // Normal
            if (channels.size() > 1 && channels[1].dimension >= 3) {
                const auto& ch = channels[1];
                float nrm[3] = { 0.0f, 0.0f, 0.0f };
                ReadChannelVec(rawBuf, ch, streamStrides[ch.stream], streamOffsets[ch.stream], v, version_major, bigEndian, 3, nrm);
                vert.nx = nrm[0];
                vert.ny = nrm[1];
                vert.nz = nrm[2];
            }

            // UV
            if (channels.size() > static_cast<std::size_t>(uvChannelIndex) && channels[uvChannelIndex].dimension >= 2) {
                const auto& ch = channels[uvChannelIndex];
                float uv[2] = { 0.0f, 0.0f };
                ReadChannelVec(rawBuf, ch, streamStrides[ch.stream], streamOffsets[ch.stream], v, version_major, bigEndian, 2, uv);
                vert.u = uv[0];
                vert.v = uv[1];
            }
        }
    }

    std::string baseName = objInfo.name.empty() ? std::to_string(objInfo.pathId) : Utils::SanitizeFilename(objInfo.name);

    const char* texExtension = texturesAreBmp ? ".bmp" : ".png";
    auto materialNameFor = [&textures, texExtension](std::size_t s) {
        std::string matName = "material_" + std::to_string(s);
        if (!textures.empty()) {
            const std::string& tex = (s < textures.size()) ? textures[s] : textures[0];
            if (!tex.empty()) {
                matName = Utils::SanitizeFilename(tex) + texExtension;
            }
        }
        return matName;
    };

    // 4. Export OBJ file
    std::filesystem::path objPath = std::filesystem::path(outputDir) / (baseName + ".obj");
    std::ofstream objOfs(objPath, std::ios::binary);
    if (objOfs) {
        objOfs << "# Exported by Vortigaunt\n";
        objOfs << "o " << baseName << "\n\n";

        for (const auto& v : parsedVertices) {
            objOfs << "v " << -v.x << " " << v.y << " " << v.z << "\n";
        }
        objOfs << "\n";

        for (const auto& v : parsedVertices) {
            objOfs << "vt " << v.u << " " << 1.0f - v.v << "\n";
        }
        objOfs << "\n";

        for (const auto& v : parsedVertices) {
            objOfs << "vn " << -v.nx << " " << v.ny << " " << v.nz << "\n";
        }
        objOfs << "\n";

        for (std::size_t s = 0; s < subMeshes.size(); ++s) {
            const auto& sm = subMeshes[s];
            if (sm.topology != 0) continue; // Triangles only

            objOfs << "g submesh_" << s << "\n";
            objOfs << "usemtl " << materialNameFor(s) << "\n";

            std::size_t indexSize = (indexFormat == 0) ? 2 : 4;
            std::size_t startIndex = sm.firstByte / indexSize;
            std::size_t numIndices = sm.indexCount;

            for (std::size_t i = 0; i + 2 < numIndices; i += 3) {
                if (startIndex + i + 2 >= indices.size()) break;
                std::uint32_t idx0 = indices[startIndex + i] + 1;
                std::uint32_t idx1 = indices[startIndex + i + 1] + 1;
                std::uint32_t idx2 = indices[startIndex + i + 2] + 1;

                // Flip winding order from CW to CCW
                objOfs << "f " << idx0 << "/" << idx0 << "/" << idx0 << " "
                               << idx2 << "/" << idx2 << "/" << idx2 << " "
                               << idx1 << "/" << idx1 << "/" << idx1 << "\n";
            }
            objOfs << "\n";
        }
        objOfs.close();
        VortigauntLog::LogF("OBJ Mesh converted: %s\n", objPath.string().c_str());
    }

    // 5. Export SMD file via SmdWriter
    std::filesystem::path smdPath = std::filesystem::path(outputDir) / (baseName + ".smd");
    std::vector<SmdTriangle> smdTriangles;
    for (std::size_t s = 0; s < subMeshes.size(); ++s) {
        const auto& sm = subMeshes[s];
        if (sm.topology != 0) continue; // Triangles only

        std::size_t indexSize = (indexFormat == 0) ? 2 : 4;
        std::size_t startIndex = sm.firstByte / indexSize;
        std::size_t numIndices = sm.indexCount;

        std::string matName = materialNameFor(s);

        for (std::size_t i = 0; i + 2 < numIndices; i += 3) {
            if (startIndex + i + 2 >= indices.size()) break;
            std::uint32_t idx0 = indices[startIndex + i];
            std::uint32_t idx1 = indices[startIndex + i + 1];
            std::uint32_t idx2 = indices[startIndex + i + 2];

            if (idx0 >= parsedVertices.size() || idx1 >= parsedVertices.size() || idx2 >= parsedVertices.size()) {
                continue;
            }

            SmdTriangle tri;
            tri.material = matName;

            // Flip winding order from CW to CCW
            const std::uint32_t vertIndices[3] = { idx0, idx2, idx1 };
            for (int v = 0; v < 3; ++v) {
                const auto& parsedV = parsedVertices[vertIndices[v]];
                SmdVertex& sv = tri.vertices[v];
                sv.boneIndex = 0;
                sv.x = -parsedV.x;
                sv.y = -parsedV.z;
                sv.z = parsedV.y;
                sv.nx = -parsedV.nx;
                sv.ny = -parsedV.nz;
                sv.nz = parsedV.ny;
                sv.u = parsedV.u;
                sv.v = 1.0f - parsedV.v;
            }
            smdTriangles.push_back(tri);
        }
    }

    if (!smdTriangles.empty()) {
        SmdWriter writer;

        // Define a root skeleton bone
        SmdBone rootBone;
        rootBone.index = 0;
        rootBone.name = "root";
        rootBone.parentIndex = -1;
        rootBone.posX = 0.0f;
        rootBone.posY = 0.0f;
        rootBone.posZ = 0.0f;
        rootBone.rotX = 0.0f;
        rootBone.rotY = 0.0f;
        rootBone.rotZ = 0.0f;
        writer.SetSkeleton({ rootBone });

        std::vector<int> emptyBoneIndices;
        if (!writer.ExportSmdTriangles(smdPath.string(), smdTriangles, emptyBoneIndices)) {
            VortigauntLog::LogF("ERROR: Failed to export SMD via SmdWriter: %s\n", writer.GetError().c_str());
        } else {
            VortigauntLog::LogF("SMD Mesh converted via SmdWriter: %s\n", smdPath.string().c_str());
        }
    }
}
