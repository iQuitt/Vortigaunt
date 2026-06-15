#include "UnityMeshConverter.h"
#include "UnityAssetParser.h"
#include "VortigauntLog.h"
#include "core/smd/SmdWriter.h"
#include "utils/BinaryUtils.h"
#include <fstream>
#include <filesystem>
#include <cstdint>
#include <vector>
#include <string>
#include <unordered_map>
#include <cstring>
#include <cmath>



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

void UnityMeshConverter::Convert(const UnityObjectInfo& objInfo,
                                 const UnityAssetParser& parser,
                                 const std::string& outputDir,
                                 const std::vector<std::string>& textures)
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

    if (vertexCount == 0 || indices.empty() || channels.empty()) {
        VortigauntLog::LogF("WARNING: Mesh PathID %lld has no vertex data or indices. Skipping.\n", static_cast<long long>(objInfo.pathId));
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
    std::vector<MeshVertex> parsedVertices(vertexCount);
    for (std::uint32_t v = 0; v < vertexCount; ++v) {
        MeshVertex& vert = parsedVertices[v];
        
        // Position (Channel 0)
        if (channels.size() > 0 && channels[0].dimension >= 3) {
            const auto& ch = channels[0];
            std::size_t stride = streamStrides[ch.stream];
            std::size_t streamStart = streamOffsets[ch.stream];
            std::size_t vertOffset = streamStart + v * stride + ch.offset;
            VertexFormat vf = ToVertexFormat(ch.format, version_major);
            std::size_t fmtSize = GetFormatSize(vf);
            
            if (vertOffset + ch.dimension * fmtSize <= rawBuf.size()) {
                vert.x = ReadChannelComponent(rawBuf.data(), vertOffset, vf, bigEndian);
                vert.y = ReadChannelComponent(rawBuf.data(), vertOffset + fmtSize, vf, bigEndian);
                vert.z = ReadChannelComponent(rawBuf.data(), vertOffset + 2 * fmtSize, vf, bigEndian);
            }
        }
        
        // Normal 
        if (channels.size() > 1 && channels[1].dimension >= 3) {
            const auto& ch = channels[1];
            std::size_t stride = streamStrides[ch.stream];
            std::size_t streamStart = streamOffsets[ch.stream];
            std::size_t vertOffset = streamStart + v * stride + ch.offset;
            VertexFormat vf = ToVertexFormat(ch.format, version_major);
            std::size_t fmtSize = GetFormatSize(vf);
            
            if (vertOffset + ch.dimension * fmtSize <= rawBuf.size()) {
                vert.nx = ReadChannelComponent(rawBuf.data(), vertOffset, vf, bigEndian);
                vert.ny = ReadChannelComponent(rawBuf.data(), vertOffset + fmtSize, vf, bigEndian);
                vert.nz = ReadChannelComponent(rawBuf.data(), vertOffset + 2 * fmtSize, vf, bigEndian);
            }
        }
        
        // UV
        // most unity assets gave wrong UV, so all have to do is mirror Y axis to fix it
        int uvChannelIndex = (version_major >= 2018) ? 4 : 3;
        if (channels.size() > static_cast<std::size_t>(uvChannelIndex) && channels[uvChannelIndex].dimension >= 2) {
            const auto& ch = channels[uvChannelIndex];
            std::size_t stride = streamStrides[ch.stream];
            std::size_t streamStart = streamOffsets[ch.stream];
            std::size_t vertOffset = streamStart + v * stride + ch.offset;
            VertexFormat vf = ToVertexFormat(ch.format, version_major);
            std::size_t fmtSize = GetFormatSize(vf);
            
            if (vertOffset + ch.dimension * fmtSize <= rawBuf.size()) {
                vert.u = ReadChannelComponent(rawBuf.data(), vertOffset, vf, bigEndian);
                vert.v = ReadChannelComponent(rawBuf.data(), vertOffset + fmtSize, vf, bigEndian);
            }
        }
    }

    std::string baseName = objInfo.name.empty() ? std::to_string(objInfo.pathId) : Utils::SanitizeFilename(objInfo.name);

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
            
            std::string matName = "material_" + std::to_string(s);
            if (!textures.empty()) {
                if (s < textures.size()) {
                    if (!textures[s].empty()) {
                        matName = Utils::SanitizeFilename(textures[s]) + ".bmp";
                    }
                } else {
                    if (!textures[0].empty()) {
                        matName = Utils::SanitizeFilename(textures[0]) + ".bmp";
                    }
                }
            }
            objOfs << "usemtl " << matName << "\n";
            
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
        
        std::string matName = "material_" + std::to_string(s);
        if (!textures.empty()) {
            if (s < textures.size()) {
                if (!textures[s].empty()) {
                    matName = Utils::SanitizeFilename(textures[s]) + ".bmp";
                }
            } else {
                if (!textures[0].empty()) {
                    matName = Utils::SanitizeFilename(textures[0]) + ".bmp";
                }
            }
        }
        
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
