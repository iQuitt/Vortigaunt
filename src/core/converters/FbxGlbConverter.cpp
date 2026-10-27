// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "FbxGlbConverter.h"

#include "core/QCFile.h"
#include "core/VortigauntLog.h"
#include "utils/FileIO.h"
#include "utils/util.hpp"

#include <assimp/Importer.hpp>
#include <assimp/config.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <rapidjson/document.h>

#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#ifdef QT_WIDGETS_LIB
#include <QByteArray>
#include <QColor>
#include <QImage>
#include "utils/Bmp.h"
#include "utils/Dds.h"
#include "utils/ImageUtils.h"
#endif

namespace fs = std::filesystem;

namespace {

constexpr double kPi = 3.14159265358979323846;

// GoldSrc's own limits, from the Half-Life SDK's studio.h.
constexpr int kGoldSrcMaxBones = 128;
constexpr int kGoldSrcMaxVertsPerBodyPart = 2048;
constexpr size_t kGoldSrcBoneNameLength = 32;     // mstudiobone_t::name
constexpr size_t kGoldSrcTextureNameLength = 64;  // mstudiotexture_t::name

aiMatrix3x3 orthonormalize(const aiMatrix3x3& a)
{
    aiVector3D x(a.a1, a.b1, a.c1);
    aiVector3D y(a.a2, a.b2, a.c2);

    if (x.Length() < 1e-12f)
    {
        return aiMatrix3x3();
    }
    x.Normalize();

    y -= x * (y * x);
    if (y.Length() < 1e-12f)
    {
        return aiMatrix3x3();
    }
    y.Normalize();

    const aiVector3D z = x ^ y;
    return aiMatrix3x3(x.x, y.x, z.x,
                       x.y, y.y, z.y,
                       x.z, y.z, z.z);
}

// SMD stores a rotation as XYZ euler angles, R = Rz * Ry * Rx.
aiVector3D toSmdEuler(const aiMatrix3x3& r)
{
    const ai_real sy = std::clamp(-r.c1, ai_real(-1), ai_real(1));
    aiVector3D e;
    if (std::abs(sy) >= ai_real(0.99999))
    {
        e.y = std::copysign(static_cast<ai_real>(kPi / 2.0), sy);
        e.z = 0;
        e.x = std::atan2(-r.b3, r.b2);
    }
    else
    {
        e.y = std::asin(sy);
        e.x = std::atan2(r.c2, r.c3);
        e.z = std::atan2(r.b1, r.a1);
    }
    return e;
}

ai_real unwrap(ai_real angle, ai_real previous)
{
    const ai_real pi = static_cast<ai_real>(kPi);
    while (angle - previous > pi) angle -= 2 * pi;
    while (angle - previous < -pi) angle += 2 * pi;
    return angle;
}

std::string sanitize(const std::string& in, const char* fallback)
{
    std::string out;
    out.reserve(in.size());
    for (unsigned char c : in)
    {
        if (std::isalnum(c) || c == '_' || c == '-')
        {
            out.push_back(static_cast<char>(c));
        }
        else if (c == ' ' || c == '.' || c == ':' || c == '|' || c == '/' || c == '\\')
        {
            out.push_back('_');
        }
    }
    while (!out.empty() && out.back() == '_') out.pop_back();
    size_t start = 0;
    while (start < out.size() && out[start] == '_') start++;
    out = out.substr(start);
    return out.empty() ? std::string(fallback) : out;
}

class GltfSidecar
{
public:
    struct Primitive
    {
        int position = -1;
        int normal = -1;
        int uv = -1;
        int material = -1;
    };

    bool Load(const fs::path& path)
    {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (!f)
        {
            return false;
        }
        const std::streamsize size = f.tellg();
        if (size < 12)
        {
            return false;
        }
        std::vector<uint8_t> file(static_cast<size_t>(size));
        f.seekg(0);
        f.read(reinterpret_cast<char*>(file.data()), size);

        m_baseDir = path.parent_path();

        std::string json;
        if (std::memcmp(file.data(), "glTF", 4) == 0)
        {
            size_t pos = 12;
            while (pos + 8 <= file.size())
            {
                uint32_t chunkLength = 0, chunkType = 0;
                std::memcpy(&chunkLength, file.data() + pos, 4);
                std::memcpy(&chunkType, file.data() + pos + 4, 4);
                pos += 8;
                if (pos + chunkLength > file.size())
                {
                    break;
                }
                if (chunkType == 0x4E4F534A)  // "JSON"
                {
                    json.assign(reinterpret_cast<const char*>(file.data() + pos), chunkLength);
                }
                else if (chunkType == 0x004E4942 && m_bin.empty())  // "BIN"
                {
                    m_bin.assign(file.begin() + pos, file.begin() + pos + chunkLength);
                }
                pos += (chunkLength + 3) & ~size_t(3);
            }
        }
        else
        {
            json.assign(reinterpret_cast<const char*>(file.data()), file.size());
        }

        if (json.empty() || m_doc.Parse(json.data(), json.size()).HasParseError() || !m_doc.IsObject())
        {
            return false;
        }

        if (m_doc.HasMember("extensionsUsed") && m_doc["extensionsUsed"].IsArray())
        {
            for (const auto& e : m_doc["extensionsUsed"].GetArray())
            {
                if (e.IsString() && std::string(e.GetString()) == "KHR_mesh_quantization")
                {
                    m_quantized = true;
                }
            }
        }

        // assimp makes one mesh per primitive, walking meshes then primitives.
        if (m_doc.HasMember("meshes") && m_doc["meshes"].IsArray())
        {
            for (const auto& mesh : m_doc["meshes"].GetArray())
            {
                if (!mesh.IsObject() || !mesh.HasMember("primitives") || !mesh["primitives"].IsArray()) continue;
                for (const auto& p : mesh["primitives"].GetArray())
                {
                    Primitive prim;
                    if (p.IsObject())
                    {
                        if (p.HasMember("attributes") && p["attributes"].IsObject())
                        {
                            const auto& a = p["attributes"];
                            if (a.HasMember("POSITION") && a["POSITION"].IsInt()) prim.position = a["POSITION"].GetInt();
                            if (a.HasMember("NORMAL") && a["NORMAL"].IsInt()) prim.normal = a["NORMAL"].GetInt();
                            if (a.HasMember("TEXCOORD_0") && a["TEXCOORD_0"].IsInt()) prim.uv = a["TEXCOORD_0"].GetInt();
                        }
                        if (p.HasMember("material") && p["material"].IsInt()) prim.material = p["material"].GetInt();
                    }
                    m_primitives.push_back(prim);
                }
            }
        }

        m_loaded = true;
        return true;
    }

    bool IsLoaded() const { return m_loaded; }
    bool IsQuantized() const { return m_quantized; }
    const std::vector<Primitive>& Primitives() const { return m_primitives; }

    // Every element of an accessor as `components` floats, integers normalised
    // the way the glTF specification says.
    bool ReadAccessor(int index, int components, std::vector<float>& out, size_t& count)
    {
        const rapidjson::Value* acc = element("accessors", index);
        if (!acc || !acc->HasMember("bufferView") || !(*acc)["bufferView"].IsInt() ||
            !acc->HasMember("componentType") || !acc->HasMember("count"))
        {
            return false;
        }
        const int componentType = (*acc)["componentType"].GetInt();
        count = static_cast<size_t>((*acc)["count"].GetUint64());
        const bool normalized = acc->HasMember("normalized") && (*acc)["normalized"].IsBool() && (*acc)["normalized"].GetBool();
        const size_t accOffset = acc->HasMember("byteOffset") ? static_cast<size_t>((*acc)["byteOffset"].GetUint64()) : 0;

        int typeComponents = 1;
        if (acc->HasMember("type") && (*acc)["type"].IsString())
        {
            const std::string type = (*acc)["type"].GetString();
            if (type == "VEC2") typeComponents = 2;
            else if (type == "VEC3") typeComponents = 3;
            else if (type == "VEC4") typeComponents = 4;
        }
        if (typeComponents < components)
        {
            return false;
        }

        size_t componentSize = 0;
        switch (componentType)
        {
        case 5120: case 5121: componentSize = 1; break;
        case 5122: case 5123: componentSize = 2; break;
        case 5125: case 5126: componentSize = 4; break;
        default: return false;
        }

        const rapidjson::Value* view = element("bufferViews", (*acc)["bufferView"].GetInt());
        if (!view || !view->HasMember("buffer"))
        {
            return false;
        }
        size_t bufferSize = 0;
        const uint8_t* buffer = bufferData((*view)["buffer"].GetInt(), bufferSize);
        if (!buffer)
        {
            return false;
        }
        const size_t viewOffset = view->HasMember("byteOffset") ? static_cast<size_t>((*view)["byteOffset"].GetUint64()) : 0;
        const size_t elementSize = componentSize * typeComponents;
        size_t stride = view->HasMember("byteStride") ? static_cast<size_t>((*view)["byteStride"].GetUint64()) : 0;
        if (stride == 0) stride = elementSize;

        const size_t start = viewOffset + accOffset;
        if (count == 0 || start + (count - 1) * stride + elementSize > bufferSize)
        {
            return false;
        }

        out.resize(count * components);
        for (size_t i = 0; i < count; i++)
        {
            const uint8_t* p = buffer + start + i * stride;
            for (int c = 0; c < components; c++)
            {
                const uint8_t* q = p + c * componentSize;
                float value = 0.0f;
                switch (componentType)
                {
                case 5120: { int8_t v; std::memcpy(&v, q, 1); value = normalized ? std::max(v / 127.0f, -1.0f) : static_cast<float>(v); break; }
                case 5121: { uint8_t v; std::memcpy(&v, q, 1); value = normalized ? v / 255.0f : static_cast<float>(v); break; }
                case 5122: { int16_t v; std::memcpy(&v, q, 2); value = normalized ? std::max(v / 32767.0f, -1.0f) : static_cast<float>(v); break; }
                case 5123: { uint16_t v; std::memcpy(&v, q, 2); value = normalized ? v / 65535.0f : static_cast<float>(v); break; }
                case 5125: { uint32_t v; std::memcpy(&v, q, 4); value = static_cast<float>(v); break; }
                case 5126: { std::memcpy(&value, q, 4); break; }
                }
                out[i * components + c] = value;
            }
        }
        return true;
    }


    void UvTransform(int material, double offset[2], double scale[2], double& rotation)
    {
        offset[0] = offset[1] = 0.0;
        scale[0] = scale[1] = 1.0;
        rotation = 0.0;

        const rapidjson::Value* info = baseColorTexture(material);
        if (!info || !info->HasMember("extensions") || !(*info)["extensions"].IsObject()) return;
        const auto& ext = (*info)["extensions"];
        if (!ext.HasMember("KHR_texture_transform") || !ext["KHR_texture_transform"].IsObject()) return;
        const auto& t = ext["KHR_texture_transform"];
        if (t.HasMember("offset") && t["offset"].IsArray() && t["offset"].Size() == 2)
        {
            offset[0] = t["offset"][0].GetDouble();
            offset[1] = t["offset"][1].GetDouble();
        }
        if (t.HasMember("scale") && t["scale"].IsArray() && t["scale"].Size() == 2)
        {
            scale[0] = t["scale"][0].GetDouble();
            scale[1] = t["scale"][1].GetDouble();
        }
        if (t.HasMember("rotation") && t["rotation"].IsNumber())
        {
            rotation = t["rotation"].GetDouble();
        }
    }

    bool BaseColorImage(int material, std::vector<uint8_t>& bytes, std::string& unsupported)
    {
        const rapidjson::Value* info = baseColorTexture(material);
        if (!info || !info->HasMember("index") || !(*info)["index"].IsInt()) return false;
        const rapidjson::Value* tex = element("textures", (*info)["index"].GetInt());
        if (!tex) return false;

        int source = -1;
        if (tex->HasMember("source") && (*tex)["source"].IsInt())
        {
            source = (*tex)["source"].GetInt();
        }
        if (tex->HasMember("extensions") && (*tex)["extensions"].IsObject())
        {
            const auto& ext = (*tex)["extensions"];
            if (source < 0 && ext.HasMember("EXT_texture_webp") && ext["EXT_texture_webp"].HasMember("source"))
            {
                source = ext["EXT_texture_webp"]["source"].GetInt();
            }
            if (source < 0 && ext.HasMember("MSFT_texture_dds") && ext["MSFT_texture_dds"].HasMember("source"))
            {
                source = ext["MSFT_texture_dds"]["source"].GetInt();
            }
            if (source < 0 && ext.HasMember("KHR_texture_basisu"))
            {
                unsupported = "KTX2/Basis";
                return false;
            }
        }

        const rapidjson::Value* image = element("images", source);
        if (!image) return false;

        if (image->HasMember("bufferView") && (*image)["bufferView"].IsInt())
        {
            const rapidjson::Value* view = element("bufferViews", (*image)["bufferView"].GetInt());
            if (!view || !view->HasMember("buffer") || !view->HasMember("byteLength")) return false;
            size_t bufferSize = 0;
            const uint8_t* buffer = bufferData((*view)["buffer"].GetInt(), bufferSize);
            if (!buffer) return false;
            const size_t offset = view->HasMember("byteOffset") ? static_cast<size_t>((*view)["byteOffset"].GetUint64()) : 0;
            const size_t length = static_cast<size_t>((*view)["byteLength"].GetUint64());
            if (offset + length > bufferSize) return false;
            bytes.assign(buffer + offset, buffer + offset + length);
            return true;
        }
        if (image->HasMember("uri") && (*image)["uri"].IsString())
        {
            const std::string uri = (*image)["uri"].GetString();
            if (uri.rfind("data:", 0) == 0) return false;
            std::ifstream f(m_baseDir / FileIO::toPath(uri), std::ios::binary | std::ios::ate);
            if (!f) return false;
            bytes.resize(static_cast<size_t>(f.tellg()));
            f.seekg(0);
            f.read(reinterpret_cast<char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
            return !bytes.empty();
        }
        return false;
    }

private:
    const rapidjson::Value* element(const char* array, int index) const
    {
        if (index < 0 || !m_doc.HasMember(array) || !m_doc[array].IsArray()) return nullptr;
        const auto& a = m_doc[array];
        if (static_cast<rapidjson::SizeType>(index) >= a.Size() || !a[index].IsObject()) return nullptr;
        return &a[index];
    }

    const rapidjson::Value* baseColorTexture(int material) const
    {
        const rapidjson::Value* mat = element("materials", material);
        if (!mat || !mat->HasMember("pbrMetallicRoughness") || !(*mat)["pbrMetallicRoughness"].IsObject()) return nullptr;
        const auto& pbr = (*mat)["pbrMetallicRoughness"];
        if (!pbr.HasMember("baseColorTexture") || !pbr["baseColorTexture"].IsObject()) return nullptr;
        return &pbr["baseColorTexture"];
    }

    const uint8_t* bufferData(int index, size_t& size)
    {
        const rapidjson::Value* buffer = element("buffers", index);
        if (!buffer) return nullptr;

        if (!buffer->HasMember("uri"))
        {
            // The one buffer with no uri is the GLB's binary chunk.
            size = m_bin.size();
            return m_bin.empty() ? nullptr : m_bin.data();
        }

        auto cached = m_external.find(index);
        if (cached == m_external.end())
        {
            std::vector<uint8_t> data;
            const std::string uri = (*buffer)["uri"].IsString() ? (*buffer)["uri"].GetString() : "";
            if (!uri.empty() && uri.rfind("data:", 0) != 0)
            {
                std::ifstream f(m_baseDir / FileIO::toPath(uri), std::ios::binary | std::ios::ate);
                if (f)
                {
                    data.resize(static_cast<size_t>(f.tellg()));
                    f.seekg(0);
                    f.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
                }
            }
            cached = m_external.emplace(index, std::move(data)).first;
        }
        size = cached->second.size();
        return cached->second.empty() ? nullptr : cached->second.data();
    }

    bool m_loaded = false;
    bool m_quantized = false;
    rapidjson::Document m_doc;
    std::vector<uint8_t> m_bin;
    std::map<int, std::vector<uint8_t>> m_external;
    std::vector<Primitive> m_primitives;
    fs::path m_baseDir;
};

struct SceneNode
{
    const aiNode* source = nullptr;
    std::string name;
    int parent = -1;
    aiMatrix4x4 local;   // rest pose
    aiMatrix4x4 world;   // rest pose, already turned to GoldSrc's axes
    bool needed = false;
    int redirect = -1;      // set when merged away: the node that took it over
    long long influence = 0;  // vertices that follow this node
    int bone = -1;
};

struct OutVertex
{
    float p[3];
    float n[3];
    float u, v;
    int node;          // the node it follows
    int linkNode[4];
    float linkWeight[4];
    uint8_t linkCount;
};

struct OutMesh
{
    int material = 0;
    bool flip = false;
    std::vector<OutVertex> vertices;
    std::vector<uint32_t> indices;
};

struct OutMaterial
{
    int sourceIndex = -1;
    std::string smdName;  // what the SMD names it, a BMP file name
    std::vector<std::string> meshNames;
};

struct OutBone
{
    std::string name;
    int parent = -1;
    int node = -1;
    aiMatrix3x3 rotation;   // rest, world
    aiVector3D position;    // rest, world
    aiMatrix4x4 restInverse;
    bool restInvertible = false;
};

int collectNodes(const aiNode* node, int parent, const aiMatrix4x4& axis, std::vector<SceneNode>& out)
{
    const int index = static_cast<int>(out.size());
    out.push_back(SceneNode());
    out[index].source = node;
    out[index].name = node->mName.C_Str();
    out[index].parent = parent;
    out[index].local = node->mTransformation;
    out[index].world = (parent >= 0) ? out[parent].world * out[index].local : axis * out[index].local;

    for (unsigned int i = 0; i < node->mNumChildren; i++)
    {
        collectNodes(node->mChildren[i], index, axis, out);
    }
    return index;
}


aiMatrix4x4 axisConversion(const aiScene* scene, bool isGltf, bool reorient, std::string& description)
{
    aiMatrix4x4 axis;
    if (!reorient)
    {
        description = "left as the file has it";
        return axis;
    }

    int up = 1;
    if (!isGltf && scene->mMetaData)
    {
        int32_t value = 1;
        if (scene->mMetaData->Get("UpAxis", value))
        {
            up = value;
        }
    }

    const ai_real yUp[3][3] = { { 0, 0, 1 }, { 1, 0, 0 }, { 0, 1, 0 } };
    const ai_real zUp[3][3] = { { 0, -1, 0 }, { 1, 0, 0 }, { 0, 0, 1 } };
    const ai_real xUp[3][3] = { { 0, 1, 0 }, { 0, 0, 1 }, { 1, 0, 0 } };
    const ai_real (*pick)[3] = (up == 2) ? zUp : (up == 0 ? xUp : yUp);
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 3; c++) axis[r][c] = pick[r][c];

    description = (up == 2) ? "Z up in the file, turned to face +X" : (up == 0 ? "X up in the file, stood up on Z" : "Y up in the file, stood up on Z and turned to face +X");
    return axis;
}

aiVector3D sampleVector(const aiVectorKey* keys, unsigned int count, double time, const aiVector3D& fallback)
{
    if (count == 0) return fallback;
    if (count == 1 || time <= keys[0].mTime) return keys[0].mValue;
    if (time >= keys[count - 1].mTime) return keys[count - 1].mValue;

    unsigned int lo = 0, hi = count - 1;
    while (hi - lo > 1)
    {
        const unsigned int mid = (lo + hi) / 2;
        if (keys[mid].mTime <= time) lo = mid; else hi = mid;
    }
    const double span = keys[hi].mTime - keys[lo].mTime;
    const float f = span > 0.0 ? static_cast<float>((time - keys[lo].mTime) / span) : 0.0f;
    return keys[lo].mValue * (1.0f - f) + keys[hi].mValue * f;
}

aiQuaternion sampleRotation(const aiQuatKey* keys, unsigned int count, double time, const aiQuaternion& fallback)
{
    if (count == 0) return fallback;
    if (count == 1 || time <= keys[0].mTime) return keys[0].mValue;
    if (time >= keys[count - 1].mTime) return keys[count - 1].mValue;

    unsigned int lo = 0, hi = count - 1;
    while (hi - lo > 1)
    {
        const unsigned int mid = (lo + hi) / 2;
        if (keys[mid].mTime <= time) lo = mid; else hi = mid;
    }
    const double span = keys[hi].mTime - keys[lo].mTime;
    const float f = span > 0.0 ? static_cast<float>((time - keys[lo].mTime) / span) : 0.0f;
    aiQuaternion out;
    aiQuaternion::Interpolate(out, keys[lo].mValue, keys[hi].mValue, f);
    out.Normalize();
    return out;
}

// ---------------------------------------------------------------------------
// SMD text
// ---------------------------------------------------------------------------

class SmdFile
{
public:
    explicit SmdFile(const fs::path& path) : m_stream(path, std::ios::binary) { m_buffer.reserve(1 << 20); }

    bool IsOpen() const { return m_stream.is_open(); }

    void Line(const char* fmt, ...)
    {
        char text[512];
        va_list args;
        va_start(args, fmt);
        const int n = std::vsnprintf(text, sizeof(text), fmt, args);
        va_end(args);
        if (n > 0)
        {
            m_buffer.append(text, static_cast<size_t>(std::min<int>(n, sizeof(text) - 1)));
        }
        if (m_buffer.size() > (1 << 20) - 1024)
        {
            Flush();
        }
    }

    void Text(const std::string& s)
    {
        m_buffer.append(s);
        if (m_buffer.size() > (1 << 20) - 1024)
        {
            Flush();
        }
    }

    bool Close()
    {
        Flush();
        const bool good = m_stream.good();
        m_stream.close();
        return good;
    }

private:
    void Flush()
    {
        m_stream.write(m_buffer.data(), static_cast<std::streamsize>(m_buffer.size()));
        m_buffer.clear();
    }

    std::ofstream m_stream;
    std::string m_buffer;
};

void writeNodes(SmdFile& file, const std::vector<OutBone>& bones)
{
    file.Text("version 1\nnodes\n");
    for (size_t i = 0; i < bones.size(); i++)
    {
        file.Line("%d \"%s\" %d\n", static_cast<int>(i), bones[i].name.c_str(), bones[i].parent);
    }
    file.Text("end\n");
}

// One skeleton frame from each bone's world rotation and position.
void writeFrame(SmdFile& file, int frame, const std::vector<OutBone>& bones,
                const std::vector<aiMatrix3x3>& rotation, const std::vector<aiVector3D>& position,
                std::vector<aiVector3D>* previousEuler)
{
    file.Line("time %d\n", frame);
    for (size_t i = 0; i < bones.size(); i++)
    {
        aiMatrix3x3 localRotation = rotation[i];
        aiVector3D localPosition = position[i];
        const int parent = bones[i].parent;
        if (parent >= 0)
        {
            aiMatrix3x3 parentInverse = rotation[parent];
            parentInverse.Transpose();  // a rotation's inverse
            localRotation = parentInverse * rotation[i];
            localPosition = parentInverse * (position[i] - position[parent]);
        }

        aiVector3D euler = toSmdEuler(localRotation);
        if (previousEuler)
        {
            if (frame > 0)
            {
                euler.x = unwrap(euler.x, (*previousEuler)[i].x);
                euler.y = unwrap(euler.y, (*previousEuler)[i].y);
                euler.z = unwrap(euler.z, (*previousEuler)[i].z);
            }
            (*previousEuler)[i] = euler;
        }

        file.Line("%d %.6f %.6f %.6f %.6f %.6f %.6f\n", static_cast<int>(i),
                  localPosition.x, localPosition.y, localPosition.z, euler.x, euler.y, euler.z);
    }
}

#ifdef QT_WIDGETS_LIB

// support the images
const char* const kImageExtensions[] = { "png", "jpg", "jpeg", "tga", "bmp", "dds", "webp", "tif", "tiff" };

bool isImageFile(const fs::path& p)
{
    std::string ext = String_ToLower(FileIO::fromPath(p.extension()));
    if (!ext.empty() && ext[0] == '.') ext = ext.substr(1);
    for (const char* e : kImageExtensions)
    {
        if (ext == e) return true;
    }
    return false;
}

std::vector<fs::path> nearbyImages(const fs::path& modelDir)
{
    std::vector<fs::path> out;
    std::error_code ec;
    std::vector<fs::path> dirs = { modelDir };
    for (const auto& entry : fs::directory_iterator(modelDir, ec))
    {
        if (!entry.is_directory(ec)) continue;
        const std::string name = String_ToLower(FileIO::fromPath(entry.path().filename()));
        if (name.find("tex") != std::string::npos || name.find("material") != std::string::npos ||
            name.find("map") != std::string::npos || name.find("image") != std::string::npos ||
            name.find(".fbm") != std::string::npos)
        {
            dirs.push_back(entry.path());
        }
    }
    for (const fs::path& dir : dirs)
    {
        for (const auto& entry : fs::directory_iterator(dir, ec))
        {
            if (entry.is_regular_file(ec) && isImageFile(entry.path()))
            {
                out.push_back(entry.path());
            }
        }
    }
    return out;
}

// A texture path as the model wrote it, often absolute on someone else's disk.
bool findTextureFile(const std::string& reference, const fs::path& modelDir,
                     const std::vector<fs::path>& images, fs::path& found)
{
    std::string cleaned = reference;
    std::replace(cleaned.begin(), cleaned.end(), '\\', '/');
    const fs::path asWritten = FileIO::toPath(cleaned);
    std::error_code ec;

    if (asWritten.is_absolute() && fs::is_regular_file(asWritten, ec))
    {
        found = asWritten;
        return true;
    }
    if (fs::is_regular_file(modelDir / asWritten, ec))
    {
        found = modelDir / asWritten;
        return true;
    }

    const std::string wantedName = String_ToLower(FileIO::fromPath(asWritten.filename()));
    const std::string wantedStem = String_ToLower(FileIO::fromPath(asWritten.stem()));
    for (const fs::path& image : images)
    {
        if (String_ToLower(FileIO::fromPath(image.filename())) == wantedName)
        {
            found = image;
            return true;
        }
    }
    // The same picture saved in another format: a .psd reference, a .png on disk.
    for (const fs::path& image : images)
    {
        if (!wantedStem.empty() && String_ToLower(FileIO::fromPath(image.stem())) == wantedStem)
        {
            found = image;
            return true;
        }
    }
    return false;
}

std::vector<std::string> nameTokens(const std::string& name)
{
    static const std::set<std::string> generic = {
        "mat", "material", "materials", "tex", "texture", "textures", "base", "basecolor", "color", "colour",
        "diffuse", "diff", "albedo", "map", "img", "image", "default", "defaultmaterial", "mesh", "model", "d", "c"
    };
    std::vector<std::string> tokens;
    std::string current;
    auto flush = [&]() {
        if (!current.empty() && !generic.count(current))
        {
            tokens.push_back(current);
        }
        current.clear();
    };
    for (unsigned char c : name)
    {
        if (std::isalnum(c)) current.push_back(static_cast<char>(std::tolower(c)));
        else flush();
    }
    flush();
    return tokens;
}

// With no texture named by the model, the folder is the only clue left: a file
// called Hair_base.bmp next to a material called Hair_mat is very likely its
// texture. Scored by the distinctive words the names share.
bool guessTextureByName(const std::vector<std::string>& keys, const std::vector<fs::path>& images, fs::path& found)
{
    static const char* const notColour[] = { "normal", "nrm", "nor", "spec", "specular", "rough", "roughness", "metal",
                                             "metallic", "ao", "occlusion", "emis", "emissive", "mask", "height",
                                             "bump", "gloss", "orm", "n", "s" };
    int bestScore = 0;
    for (const fs::path& image : images)
    {
        const std::vector<std::string> imageTokens = nameTokens(FileIO::fromPath(image.stem()));
        if (imageTokens.empty()) continue;

        bool otherMap = false;
        for (const std::string& t : imageTokens)
        {
            for (const char* bad : notColour)
            {
                if (t == bad) otherMap = true;
            }
        }
        if (otherMap) continue;

        for (const std::string& key : keys)
        {
            const std::vector<std::string> keyTokens = nameTokens(key);
            int shared = 0;
            bool longWord = false;
            for (const std::string& t : imageTokens)
            {
                if (std::find(keyTokens.begin(), keyTokens.end(), t) != keyTokens.end())
                {
                    shared++;
                    if (t.size() >= 4) longWord = true;
                }
            }

            if (!longWord || shared * 2 < static_cast<int>(imageTokens.size())) continue;
            if (shared > bestScore)
            {
                bestScore = shared;
                found = image;
            }
        }
    }
    return bestScore > 0;
}

// more than 2% of the picture is see-through
bool hasTransparency(QImage image)
{
    image = image.convertToFormat(QImage::Format_ARGB32);
    long long seeThrough = 0;
    const long long total = static_cast<long long>(image.width()) * image.height();
    const int step = std::max(1, static_cast<int>(std::sqrt(static_cast<double>(total) / 65536.0)));
    long long sampled = 0;
    for (int y = 0; y < image.height(); y += step)
    {
        const QRgb* row = reinterpret_cast<const QRgb*>(image.constScanLine(y));
        for (int x = 0; x < image.width(); x += step)
        {
            sampled++;
            if (qAlpha(row[x]) < 128) seeThrough++;
        }
    }
    return sampled > 0 && seeThrough * 50 > sampled;
}

QImage decodeImageBytes(const uint8_t* data, size_t size)
{
    if (size >= 4 && std::memcmp(data, "DDS ", 4) == 0)
    {
        return DDS::loadDdsFromMemory(std::vector<uint8_t>(data, data + size));
    }
    return QImage::fromData(QByteArray::fromRawData(reinterpret_cast<const char*>(data), static_cast<qsizetype>(size)));
}
#endif  // QT_WIDGETS_LIB

}  // namespace

bool FbxGlbConverter::IsSupportedFile(const std::string& path)
{
    const std::string ext = FileIO::lowerExtension(path);
    return ext == ".fbx" || ext == ".glb" || ext == ".gltf" || ext == ".obj";
}

ConvertResult FbxGlbConverter::ConvertFile(const std::string& inputPath, const std::string& outputDir)
{
    const FbxGlbConvertReport report = Convert(inputPath, outputDir, m_options);
    ConvertResult result;
    result.ok = report.ok;
    result.error = report.error;
    result.outputFolder = report.outputFolder;
    if (!report.qcPath.empty()) result.qcFiles.push_back(report.qcPath);
    return result;
}

FbxGlbConvertReport FbxGlbConverter::Convert(const std::string& inputPath, const std::string& outputDir, const FbxGlbConvertOptions& options)
{
    FbxGlbConvertReport report;
    auto fail = [&](const std::string& message) {
        report.error = message;
        VortigauntLog::LogF("^1Error:^7 %s", message.c_str());
        return report;
    };
    auto warn = [&](const std::string& message) {
        report.warnings.push_back(message);
        VortigauntLog::LogF("^3Note:^7 %s", message.c_str());
    };

    const fs::path inputFile = FileIO::toPath(inputPath);
    const std::string extension = String_ToLower(FileIO::fromPath(inputFile.extension()));
    const bool isGltf = (extension == ".glb" || extension == ".gltf");
    const bool isObj = (extension == ".obj");
    if (!isGltf && !isObj && extension != ".fbx")
    {
        return fail("Not an FBX, GLB, glTF or OBJ file: " + inputPath);
    }

    std::string modelName = sanitize(FileIO::fromPath(inputFile.stem()), "model");
    VortigauntLog::LogF("^5Converting %s:^7 %s", isGltf ? "GLB" : (isObj ? "OBJ" : "FBX"), FileIO::fromPath(inputFile.filename()).c_str());

    Assimp::Importer importer;

    importer.SetPropertyBool(AI_CONFIG_IMPORT_FBX_PRESERVE_PIVOTS, false);
    const aiScene* scene = importer.ReadFile(inputPath,
                                             aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_LimitBoneWeights);
    if (!scene || !scene->mRootNode)
    {
        return fail(std::string("Could not read the model: ") + importer.GetErrorString());
    }

    GltfSidecar gltf;
    if (isGltf)
    {
        gltf.Load(inputFile);
    }

    //nodes
    std::string axisDescription;
    const aiMatrix4x4 axis = axisConversion(scene, isGltf, options.reorient, axisDescription);

    std::vector<SceneNode> nodes;
    collectNodes(scene->mRootNode, -1, axis, nodes);
    report.sourceNodes = static_cast<int>(nodes.size());

    std::unordered_map<std::string, int> nodeByName;
    for (size_t i = 0; i < nodes.size(); i++)
    {
        nodeByName.emplace(nodes[i].name, static_cast<int>(i));  // the first of a name wins
    }
    //materials
    std::vector<OutMaterial> materials;
    std::unordered_map<unsigned int, int> materialSlot;
    std::set<std::string> takenTextureNames;
    auto slotFor = [&](unsigned int sourceIndex, const std::string& meshName) {
        auto it = materialSlot.find(sourceIndex);
        if (it == materialSlot.end())
        {
            OutMaterial m;
            m.sourceIndex = static_cast<int>(sourceIndex);

            std::string base;
            if (sourceIndex < scene->mNumMaterials)
            {
                aiString name;
                if (scene->mMaterials[sourceIndex]->Get(AI_MATKEY_NAME, name) == AI_SUCCESS)
                {
                    base = name.C_Str();
                }


                aiString texturePath;
                for (aiTextureType type : { aiTextureType_DIFFUSE, aiTextureType_BASE_COLOR })
                {
                    if (scene->mMaterials[sourceIndex]->GetTexture(type, 0, &texturePath) != AI_SUCCESS) continue;
                    if (scene->GetEmbeddedTexture(texturePath.C_Str())) break;
                    std::string file = texturePath.C_Str();
                    std::replace(file.begin(), file.end(), '\\', '/');
                    const std::string stem = FileIO::fromPath(FileIO::toPath(file).stem());
                    if (!stem.empty()) base = stem;
                    break;
                }
            }
            if (base.empty() || base == "DefaultMaterial")
            {
                base = modelName + "_" + std::to_string(materials.size());
            }

            m.smdName = String_UniqueName(sanitize(base, "texture"), kGoldSrcTextureNameLength - 1 - 4, takenTextureNames) + ".bmp";

            it = materialSlot.emplace(sourceIndex, static_cast<int>(materials.size())).first;
            materials.push_back(m);
        }
        if (!meshName.empty())
        {
            materials[it->second].meshNames.push_back(meshName);
        }
        return it->second;
    };

    //geometry
    std::vector<OutMesh> meshes;
    bool quantizedMismatch = false;
    int quantizedMeshes = 0;
    int morphMeshes = 0;
    int unknownBones = 0;
    std::set<unsigned int> countedMeshes;

    for (size_t nodeIndex = 0; nodeIndex < nodes.size(); nodeIndex++)
    {
        const aiNode* node = nodes[nodeIndex].source;
        for (unsigned int k = 0; k < node->mNumMeshes; k++)
        {
            const unsigned int meshIndex = node->mMeshes[k];
            const aiMesh* mesh = scene->mMeshes[meshIndex];
            if (!mesh || mesh->mNumVertices == 0 || !mesh->mVertices) continue;

            if (countedMeshes.insert(meshIndex).second)
            {
                report.meshes++;
                if (mesh->mNumBones > 0) report.skinnedMeshes++;
                if (mesh->mNumAnimMeshes > 0) morphMeshes++;
            }

            // Vertex data, straight from the file where assimp gets it wrong.
            std::vector<float> ownPositions, ownNormals, ownUvs;
            bool haveOwnNormals = false, haveOwnUvs = false;
            bool useOwn = false;
            if (gltf.IsLoaded() && gltf.IsQuantized())
            {
                if (meshIndex < gltf.Primitives().size())
                {
                    const GltfSidecar::Primitive& prim = gltf.Primitives()[meshIndex];
                    size_t count = 0;
                    if (gltf.ReadAccessor(prim.position, 3, ownPositions, count) && count == mesh->mNumVertices)
                    {
                        useOwn = true;
                        size_t normalCount = 0, uvCount = 0;
                        haveOwnNormals = gltf.ReadAccessor(prim.normal, 3, ownNormals, normalCount) && normalCount == count;
                        haveOwnUvs = gltf.ReadAccessor(prim.uv, 2, ownUvs, uvCount) && uvCount == count;
                        if (haveOwnUvs)
                        {
                            double offset[2], scale[2], rotation = 0.0;
                            gltf.UvTransform(prim.material, offset, scale, rotation);
                            const double c = std::cos(rotation), s = std::sin(rotation);
                            for (size_t i = 0; i < count; i++)
                            {
                                const double u = ownUvs[i * 2 + 0] * scale[0];
                                const double v = ownUvs[i * 2 + 1] * scale[1];
                                const double tu = c * u - s * v + offset[0];
                                const double tv = s * u + c * v + offset[1];
                                ownUvs[i * 2 + 0] = static_cast<float>(tu);
                                ownUvs[i * 2 + 1] = static_cast<float>(1.0 - tv);  // glTF counts V from the top
                            }
                        }
                        quantizedMeshes++;
                    }
                }
                if (!useOwn)
                {
                    quantizedMismatch = true;
                }
            }

            // Skin: per vertex, the bones that pull on it.
            struct Influence { int node; float weight; };
            std::vector<std::vector<Influence>> influences;
            std::vector<aiMatrix4x4> skinMatrix;   // per mesh bone: bone world * offset
            std::vector<int> skinNode;
            if (mesh->mNumBones > 0)
            {
                influences.resize(mesh->mNumVertices);
                skinMatrix.resize(mesh->mNumBones);
                skinNode.assign(mesh->mNumBones, -1);
                for (unsigned int b = 0; b < mesh->mNumBones; b++)
                {
                    const aiBone* bone = mesh->mBones[b];
                    auto found = nodeByName.find(bone->mName.C_Str());
                    if (found == nodeByName.end())
                    {
                        unknownBones++;
                        continue;
                    }
                    skinNode[b] = found->second;
                    skinMatrix[b] = nodes[found->second].world * bone->mOffsetMatrix;
                    for (unsigned int w = 0; w < bone->mNumWeights; w++)
                    {
                        const aiVertexWeight& vw = bone->mWeights[w];
                        if (vw.mVertexId < mesh->mNumVertices && vw.mWeight > 0.0f)
                        {
                            influences[vw.mVertexId].push_back({ static_cast<int>(b), vw.mWeight });
                        }
                    }
                }
            }

            OutMesh out;
            out.material = slotFor(mesh->mMaterialIndex, mesh->mName.C_Str());
            out.vertices.resize(mesh->mNumVertices);

            const aiMatrix4x4& nodeWorld = nodes[nodeIndex].world;
            bool flipKnown = false;

            for (unsigned int v = 0; v < mesh->mNumVertices; v++)
            {
                aiVector3D position, normal;
                if (useOwn)
                {
                    position = aiVector3D(ownPositions[v * 3 + 0], ownPositions[v * 3 + 1], ownPositions[v * 3 + 2]);
                    if (haveOwnNormals) normal = aiVector3D(ownNormals[v * 3 + 0], ownNormals[v * 3 + 1], ownNormals[v * 3 + 2]);
                }
                else
                {
                    position = mesh->mVertices[v];
                    if (mesh->mNormals) normal = mesh->mNormals[v];
                }

                OutVertex& ov = out.vertices[v];
                ov.linkCount = 0;

                // Where the vertex stands in the rest pose: the weighted blend
                // of where each of its bones puts it, or the node's own
                // transform for a mesh with no skin.
                aiMatrix4x4 blended;
                bool skinned = false;
                if (!influences.empty() && !influences[v].empty())
                {
                    std::vector<Influence>& list = influences[v];
                    std::sort(list.begin(), list.end(), [](const Influence& a, const Influence& b) { return a.weight > b.weight; });
                    double total = 0.0;
                    for (const Influence& i : list) total += i.weight;
                    if (total > 1e-6)
                    {
                        blended = aiMatrix4x4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
                        for (const Influence& i : list)
                        {
                            const ai_real w = static_cast<ai_real>(i.weight / total);
                            blended = blended + skinMatrix[i.node] * w;
                            if (ov.linkCount < 4)
                            {
                                ov.linkNode[ov.linkCount] = skinNode[i.node];
                                ov.linkWeight[ov.linkCount] = w;
                                ov.linkCount++;
                            }
                        }
                        ov.node = skinNode[list.front().node];
                        skinned = true;
                    }
                }
                if (!skinned)
                {
                    blended = nodeWorld;
                    ov.node = static_cast<int>(nodeIndex);
                }

                if (!flipKnown)
                {
                    // A mirrored transform turns every triangle inside out.
                    out.flip = aiMatrix3x3(blended).Determinant() < 0;
                    flipKnown = true;
                }

                const aiVector3D wp = blended * position;
                aiVector3D wn = aiMatrix3x3(blended) * normal;
                wn.NormalizeSafe();  // stays zero when the file gave no normal

                ov.p[0] = wp.x; ov.p[1] = wp.y; ov.p[2] = wp.z;
                ov.n[0] = wn.x; ov.n[1] = wn.y; ov.n[2] = wn.z;

                if (useOwn && haveOwnUvs)
                {
                    ov.u = ownUvs[v * 2 + 0];
                    ov.v = ownUvs[v * 2 + 1];
                }
                else if (!useOwn && mesh->mTextureCoords[0])
                {
                    ov.u = mesh->mTextureCoords[0][v].x;
                    ov.v = mesh->mTextureCoords[0][v].y;
                }
                else
                {
                    ov.u = 0.0f;
                    ov.v = 0.0f;
                }
            }

            out.indices.reserve(static_cast<size_t>(mesh->mNumFaces) * 3);
            for (unsigned int f = 0; f < mesh->mNumFaces; f++)
            {
                const aiFace& face = mesh->mFaces[f];
                if (face.mNumIndices != 3) continue;  // points and lines have no place in a studio model
                if (face.mIndices[0] >= mesh->mNumVertices || face.mIndices[1] >= mesh->mNumVertices ||
                    face.mIndices[2] >= mesh->mNumVertices) continue;
                out.indices.push_back(face.mIndices[0]);
                out.indices.push_back(out.flip ? face.mIndices[2] : face.mIndices[1]);
                out.indices.push_back(out.flip ? face.mIndices[1] : face.mIndices[2]);
            }

            // A normal the file did not supply: the triangle's own.
            for (size_t i = 0; i + 2 < out.indices.size(); i += 3)
            {
                OutVertex* tri[3] = { &out.vertices[out.indices[i]], &out.vertices[out.indices[i + 1]], &out.vertices[out.indices[i + 2]] };
                aiVector3D faceNormal;
                bool computed = false;
                for (OutVertex* tv : tri)
                {
                    if (tv->n[0] != 0.0f || tv->n[1] != 0.0f || tv->n[2] != 0.0f) continue;
                    if (!computed)
                    {
                        const aiVector3D a(tri[0]->p[0], tri[0]->p[1], tri[0]->p[2]);
                        const aiVector3D b(tri[1]->p[0], tri[1]->p[1], tri[1]->p[2]);
                        const aiVector3D c(tri[2]->p[0], tri[2]->p[1], tri[2]->p[2]);
                        faceNormal = (b - a) ^ (c - a);
                        if (faceNormal.Length() > 1e-20f) faceNormal.Normalize();
                        else faceNormal = aiVector3D(0, 0, 1);
                        computed = true;
                    }
                    tv->n[0] = faceNormal.x;
                    tv->n[1] = faceNormal.y;
                    tv->n[2] = faceNormal.z;
                }
            }

            if (!out.indices.empty())
            {
                report.triangles += static_cast<int>(out.indices.size() / 3);
                meshes.push_back(std::move(out));
            }
        }
    }

    report.animationOnly = meshes.empty();

    // nodes become bones in this part
    for (const OutMesh& mesh : meshes)
    {
        // Only vertices a triangle uses count: FBX files carry strays.
        std::vector<uint8_t> used(mesh.vertices.size(), 0);
        for (uint32_t i : mesh.indices) used[i] = 1;
        for (size_t v = 0; v < mesh.vertices.size(); v++)
        {
            if (!used[v]) continue;
            const OutVertex& ov = mesh.vertices[v];
            if (ov.node >= 0)
            {
                nodes[ov.node].needed = true;
                nodes[ov.node].influence++;
            }
        }
    }
    if (report.animationOnly)
    {
        // Nothing to bind, so the skeleton is whatever the animations move.
        for (unsigned int a = 0; a < scene->mNumAnimations; a++)
        {
            const aiAnimation* anim = scene->mAnimations[a];
            for (unsigned int c = 0; c < anim->mNumChannels; c++)
            {
                auto found = nodeByName.find(anim->mChannels[c]->mNodeName.C_Str());
                if (found != nodeByName.end()) nodes[found->second].needed = true;
            }
        }
    }
    bool anyNeeded = false;
    for (const SceneNode& n : nodes) anyNeeded = anyNeeded || n.needed;
    if (!anyNeeded)
    {
        return fail("The file has neither a mesh nor an animation to convert.");
    }
    // A bone needs its whole chain of parents to stand where it does.
    for (size_t i = nodes.size(); i-- > 0;)
    {
        if (nodes[i].needed && nodes[i].parent >= 0) nodes[nodes[i].parent].needed = true;
    }

	// fit the bone limit so shut up to the compiler
    int neededCount = 0;
    for (const SceneNode& n : nodes) neededCount += n.needed ? 1 : 0;
    const int bonesBefore = neededCount;
    std::vector<std::string> mergedNames;

    if (options.maxBones > 0 && neededCount > options.maxBones)
    {
        std::vector<int> childCount(nodes.size(), 0);
        for (const SceneNode& n : nodes)
        {
            if (n.needed && n.parent >= 0) childCount[n.parent]++;
        }
        // The cheapest bone to lose is a leaf that few vertices follow: its
        // geometry simply moves with the parent from then on.
        while (neededCount > options.maxBones)
        {
            int victim = -1;
            for (size_t i = 0; i < nodes.size(); i++)
            {
                if (!nodes[i].needed || childCount[i] != 0 || nodes[i].parent < 0) continue;
                if (victim < 0 || nodes[i].influence < nodes[victim].influence) victim = static_cast<int>(i);
            }
            if (victim < 0) break;

            const int parent = nodes[victim].parent;
            nodes[victim].needed = false;
            nodes[victim].redirect = parent;
            nodes[parent].influence += nodes[victim].influence;
            childCount[parent]--;
            neededCount--;
            mergedNames.push_back(nodes[victim].name);
        }
        report.mergedBones = bonesBefore - neededCount;
    }

    auto resolveNode = [&](int index) {
        int guard = 0;
        while (index >= 0 && !nodes[index].needed && nodes[index].redirect >= 0 && guard++ < 4096)
        {
            index = nodes[index].redirect;
        }
        return index;
    };

    // bones
    std::vector<OutBone> bones;
    {
        std::set<std::string> takenBoneNames;
        for (size_t i = 0; i < nodes.size(); i++)
        {
            if (!nodes[i].needed) continue;
            OutBone bone;
            bone.node = static_cast<int>(i);
            bone.parent = (nodes[i].parent >= 0) ? nodes[nodes[i].parent].bone : -1;
            bone.name = String_UniqueName(sanitize(nodes[i].name, "bone"), kGoldSrcBoneNameLength - 1, takenBoneNames);
            const aiMatrix4x4& world = nodes[i].world;
            bone.rotation = orthonormalize(aiMatrix3x3(world));
            bone.position = aiVector3D(world.a4, world.b4, world.c4);
            bone.restInvertible = std::abs(world.Determinant()) > 1e-30f;
            if (bone.restInvertible)
            {
                bone.restInverse = world;
                bone.restInverse.Inverse();
            }
            nodes[i].bone = static_cast<int>(bones.size());
            bones.push_back(bone);
        }
    }
    report.bones = static_cast<int>(bones.size());

    // output
    const fs::path outFolder = FileIO::toPath(outputDir) / FileIO::toPath(modelName);
    std::error_code ec;
    fs::create_directories(outFolder, ec);
    if (ec)
    {
        return fail("Could not create the output folder: " + FileIO::fromPath(outFolder));
    }
    report.outputFolder = FileIO::fromPath(outFolder);

    // ref smd
    if (!report.animationOnly)
    {
        double mn[3] = { 1e300, 1e300, 1e300 }, mx[3] = { -1e300, -1e300, -1e300 };
        std::unordered_set<uint64_t> uniquePositions;

        const fs::path referencePath = outFolder / FileIO::toPath(modelName + ".smd");
        SmdFile file(referencePath);
        if (!file.IsOpen())
        {
            return fail("Could not write " + FileIO::fromPath(referencePath));
        }

        writeNodes(file, bones);
        file.Text("skeleton\n");
        {
            std::vector<aiMatrix3x3> rotation(bones.size());
            std::vector<aiVector3D> position(bones.size());
            for (size_t i = 0; i < bones.size(); i++)
            {
                rotation[i] = bones[i].rotation;
                position[i] = bones[i].position;
            }
            writeFrame(file, 0, bones, rotation, position, nullptr);
        }
        file.Text("end\ntriangles\n");

        for (const OutMesh& mesh : meshes)
        {
            const std::string materialLine = materials[mesh.material].smdName + "\n";
            for (size_t i = 0; i + 2 < mesh.indices.size(); i += 3)
            {
                file.Text(materialLine);
                for (int corner = 0; corner < 3; corner++)
                {
                    const OutVertex& ov = mesh.vertices[mesh.indices[i + corner]];
                    const int boneIndex = std::max(0, nodes[resolveNode(ov.node)].bone);

                    for (int k = 0; k < 3; k++)
                    {
                        mn[k] = std::min<double>(mn[k], ov.p[k]);
                        mx[k] = std::max<double>(mx[k], ov.p[k]);
                    }

                    uint64_t h = 1469598103934665603ull; //
                    for (int k = 0; k < 3; k++)
                    {
                        const int64_t q = static_cast<int64_t>(std::llround(static_cast<double>(ov.p[k]) * 1000.0));
                        h = (h ^ static_cast<uint64_t>(q)) * 1099511628211ull;
                    }
                    uniquePositions.insert(h);

                    int linkBone[4];
                    float linkWeight[4];
                    int linkCount = 0;
                    for (int l = 0; l < ov.linkCount; l++)
                    {
                        if (ov.linkNode[l] < 0) continue;
                        const int b = nodes[resolveNode(ov.linkNode[l])].bone;
                        if (b < 0) continue;
                        int slot = -1;
                        for (int s = 0; s < linkCount; s++)
                        {
                            if (linkBone[s] == b) slot = s;
                        }
                        if (slot < 0)
                        {
                            linkBone[linkCount] = b;
                            linkWeight[linkCount] = ov.linkWeight[l];
                            linkCount++;
                        }
                        else
                        {
                            linkWeight[slot] += ov.linkWeight[l];
                        }
                    }

                    if (linkCount > 1)
                    {
                        char links[160];
                        int at = std::snprintf(links, sizeof(links), " %d", linkCount);
                        for (int l = 0; l < linkCount && at < static_cast<int>(sizeof(links)) - 32; l++)
                        {
                            at += std::snprintf(links + at, sizeof(links) - at, " %d %.6f", linkBone[l], linkWeight[l]);
                        }
                        file.Line("%d %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f%s\n", boneIndex,
                                  ov.p[0], ov.p[1], ov.p[2], ov.n[0], ov.n[1], ov.n[2], ov.u, ov.v, links);
                    }
                    else
                    {
                        file.Line("%d %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n", boneIndex,
                                  ov.p[0], ov.p[1], ov.p[2], ov.n[0], ov.n[1], ov.n[2], ov.u, ov.v);
                    }
                }
            }
        }
        file.Text("end\n");
        if (!file.Close())
        {
            return fail("Writing " + FileIO::fromPath(referencePath) + " failed (disk full?).");
        }

        report.referenceSmd = FileIO::fromPath(referencePath);
        report.uniqueVertices = static_cast<int>(uniquePositions.size());
        for (int k = 0; k < 3; k++) report.size[k] = static_cast<float>(mx[k] - mn[k]);
    }

    // anim
    int animatedScaleChannels = 0;
    int morphChannels = 0;
    {
        std::set<std::string> takenAnimNames;
        std::vector<aiMatrix4x4> animWorld(nodes.size());
        std::vector<aiMatrix3x3> rotation(bones.size());
        std::vector<aiVector3D> position(bones.size());
        std::vector<aiVector3D> previousEuler(bones.size());

        // Rest pose split into its parts, for channels that key only some of them.
        std::vector<aiVector3D> restPosition(nodes.size()), restScale(nodes.size());
        std::vector<aiQuaternion> restRotation(nodes.size());
        for (size_t i = 0; i < nodes.size(); i++)
        {
            nodes[i].source->mTransformation.Decompose(restScale[i], restRotation[i], restPosition[i]);
        }

        fs::path animFolder;
        for (unsigned int a = 0; a < scene->mNumAnimations; a++)
        {
            const aiAnimation* anim = scene->mAnimations[a];
            if (!anim || anim->mNumChannels == 0)
            {
                morphChannels += anim ? static_cast<int>(anim->mNumMorphMeshChannels) : 0;
                continue;
            }
            morphChannels += static_cast<int>(anim->mNumMorphMeshChannels);

            std::vector<const aiNodeAnim*> channelOf(nodes.size(), nullptr);
            double firstKey = 1e300, lastKey = -1e300;
            for (unsigned int c = 0; c < anim->mNumChannels; c++)
            {
                const aiNodeAnim* ch = anim->mChannels[c];
                auto found = nodeByName.find(ch->mNodeName.C_Str());
                if (found != nodeByName.end()) channelOf[found->second] = ch;

                if (ch->mNumPositionKeys) { firstKey = std::min(firstKey, ch->mPositionKeys[0].mTime); lastKey = std::max(lastKey, ch->mPositionKeys[ch->mNumPositionKeys - 1].mTime); }
                if (ch->mNumRotationKeys) { firstKey = std::min(firstKey, ch->mRotationKeys[0].mTime); lastKey = std::max(lastKey, ch->mRotationKeys[ch->mNumRotationKeys - 1].mTime); }
                if (ch->mNumScalingKeys) { firstKey = std::min(firstKey, ch->mScalingKeys[0].mTime); lastKey = std::max(lastKey, ch->mScalingKeys[ch->mNumScalingKeys - 1].mTime); }

                for (unsigned int k = 1; k < ch->mNumScalingKeys; k++)
                {
                    if ((ch->mScalingKeys[k].mValue - ch->mScalingKeys[0].mValue).Length() > 1e-3f)
                    {
                        animatedScaleChannels++;
                        break;
                    }
                }
            }
            if (lastKey < firstKey)
            {
                firstKey = 0.0;
                lastKey = anim->mDuration;
            }

            const double ticksPerSecond = anim->mTicksPerSecond > 0.0 ? anim->mTicksPerSecond : 25.0;
            const double seconds = (lastKey - firstKey) / ticksPerSecond;
            const int frameCount = std::max(1, static_cast<int>(std::ceil(seconds * options.fps - 1e-6)) + 1);


            std::string rawName = anim->mName.C_Str();
            const size_t bar = rawName.find_last_of('|');
            if (bar != std::string::npos && bar + 1 < rawName.size()) rawName = rawName.substr(bar + 1);
            const std::string animName = String_UniqueName(sanitize(rawName, "anim"), 48, takenAnimNames);

            if (animFolder.empty())
            {
                animFolder = outFolder / "anims";
                fs::create_directories(animFolder, ec);
            }
            const fs::path animPath = animFolder / FileIO::toPath(animName + ".smd");
            SmdFile file(animPath);
            if (!file.IsOpen())
            {
                warn("Could not write " + FileIO::fromPath(animPath));
                continue;
            }

            writeNodes(file, bones);
            file.Text("skeleton\n");

            for (int frame = 0; frame < frameCount; frame++)
            {
                const double time = std::min(firstKey + (static_cast<double>(frame) / options.fps) * ticksPerSecond, lastKey);

                for (size_t i = 0; i < nodes.size(); i++)
                {
                    aiMatrix4x4 local = nodes[i].local;
                    if (const aiNodeAnim* ch = channelOf[i])
                    {
                        const aiVector3D p = sampleVector(ch->mPositionKeys, ch->mNumPositionKeys, time, restPosition[i]);
                        const aiVector3D s = sampleVector(ch->mScalingKeys, ch->mNumScalingKeys, time, restScale[i]);
                        const aiQuaternion r = sampleRotation(ch->mRotationKeys, ch->mNumRotationKeys, time, restRotation[i]);
                        local = aiMatrix4x4(s, r, p);
                    }
                    animWorld[i] = (nodes[i].parent >= 0) ? animWorld[nodes[i].parent] * local : axis * local;
                }

                for (size_t b = 0; b < bones.size(); b++)
                {
                    const OutBone& bone = bones[b];
                    const aiMatrix4x4& world = animWorld[bone.node];
                    position[b] = aiVector3D(world.a4, world.b4, world.c4);
                    if (bone.restInvertible)
                    {

                        const aiMatrix3x3 delta = aiMatrix3x3(world) * aiMatrix3x3(bone.restInverse);
                        rotation[b] = orthonormalize(delta) * bone.rotation;
                    }
                    else
                    {
                        rotation[b] = orthonormalize(aiMatrix3x3(world));
                    }
                }

                writeFrame(file, frame, bones, rotation, position, &previousEuler);
            }
            file.Text("end\n");
            if (!file.Close())
            {
                warn("Writing " + FileIO::fromPath(animPath) + " failed.");
                continue;
            }

            FbxGlbAnimationInfo info;
            info.name = animName;
            info.file = "anims/" + animName;
            info.frames = frameCount;
            report.animations.push_back(info);
        }
    }

    // textures
    std::vector<std::string> textureNotes;
    int alphaTextures = 0;
    if (!report.animationOnly && options.exportTextures)
    {
#ifdef QT_WIDGETS_LIB
        const fs::path modelDir = inputFile.parent_path();
        const std::vector<fs::path> images = nearbyImages(modelDir);

        for (const OutMaterial& material : materials)
        {
            const aiMaterial* source = (material.sourceIndex >= 0 && material.sourceIndex < static_cast<int>(scene->mNumMaterials))
                                           ? scene->mMaterials[material.sourceIndex] : nullptr;
            QImage image;
            std::string origin;
            enum { FromModel, Guessed, Placeholder } kind = Placeholder;

            if (source)
            {
                aiString texturePath;
                bool has = false;
                for (aiTextureType type : { aiTextureType_DIFFUSE, aiTextureType_BASE_COLOR })
                {
                    if (source->GetTexture(type, 0, &texturePath) == AI_SUCCESS)
                    {
                        has = true;
                        break;
                    }
                }
                if (has)
                {
                    if (const aiTexture* embedded = scene->GetEmbeddedTexture(texturePath.C_Str()))
                    {
                        if (embedded->mHeight == 0)
                        {
                            image = decodeImageBytes(reinterpret_cast<const uint8_t*>(embedded->pcData), embedded->mWidth);
                        }
                        else
                        {
                            image = QImage(reinterpret_cast<const uchar*>(embedded->pcData), static_cast<int>(embedded->mWidth),
                                           static_cast<int>(embedded->mHeight), QImage::Format_ARGB32).copy();
                        }
                        origin = "embedded in the file";
                    }
                    else
                    {
                        fs::path file;
                        if (findTextureFile(texturePath.C_Str(), modelDir, images, file))
                        {
                            image = ImageUtils::loadImage(FileIO::fromPath(file));
                            origin = FileIO::fromPath(file.filename());
                        }
                        else
                        {
                            textureNotes.push_back(material.smdName + ": the model uses \"" + std::string(texturePath.C_Str()) +
                                                   "\" but that file is not in the model's folder. Put it there and convert again, "
                                                   "or save your own texture over " + material.smdName);
                        }
                    }
                    if (!image.isNull()) kind = FromModel;
                }
            }

            if (image.isNull() && gltf.IsLoaded())
            {
                std::vector<uint8_t> bytes;
                std::string unsupported;
                if (gltf.BaseColorImage(material.sourceIndex, bytes, unsupported))
                {
                    image = decodeImageBytes(bytes.data(), bytes.size());
                    if (!image.isNull())
                    {
                        kind = FromModel;
                        origin = "embedded in the file";
                    }
                    else
                    {
                        textureNotes.push_back(material.smdName + ": embedded image is in a format that could not be decoded");
                    }
                }
                else if (!unsupported.empty())
                {
                    textureNotes.push_back(material.smdName + ": embedded image is " + unsupported + ", which is not supported");
                }
            }

            if (image.isNull())
            {
                std::vector<std::string> keys;
                if (source)
                {
                    aiString name;
                    if (source->Get(AI_MATKEY_NAME, name) == AI_SUCCESS && name.length > 0) keys.push_back(name.C_Str());
                }
                for (const std::string& meshName : material.meshNames) keys.push_back(meshName);
                if (materials.size() == 1) keys.push_back(FileIO::fromPath(inputFile.stem()));

                fs::path file;
                if (guessTextureByName(keys, images, file))
                {
                    image = ImageUtils::loadImage(FileIO::fromPath(file));
                    if (!image.isNull())
                    {
                        kind = Guessed;
                        origin = FileIO::fromPath(file.filename());
                    }
                }
            }

            if (image.isNull())
            {
                aiColor4D colour(0.6f, 0.6f, 0.6f, 1.0f);
                if (source)
                {
                    if (source->Get(AI_MATKEY_BASE_COLOR, colour) != AI_SUCCESS)
                    {
                        source->Get(AI_MATKEY_COLOR_DIFFUSE, colour);
                    }
                }
                image = QImage(16, 16, QImage::Format_ARGB32);
                image.fill(QColor::fromRgbF(std::clamp(colour.r, 0.0f, 1.0f), std::clamp(colour.g, 0.0f, 1.0f),
                                            std::clamp(colour.b, 0.0f, 1.0f)));
                kind = Placeholder;
            }

            const bool hadAlpha = hasTransparency(image);
            BMP::GoldSrcTextureOptions textureOptions;
            textureOptions.maxSize = options.maxTextureSize;
            textureOptions.powerOfTwo = true;
            BMP::GoldSrcTextureInfo written;
            if (!BMP::saveGoldSrcTexture(FileIO::fromPath(outFolder / FileIO::toPath(material.smdName)), image, textureOptions, &written))
            {
                warn("Could not write texture " + material.smdName);
                continue;
            }
            if (hadAlpha) alphaTextures++;

            if (kind == FromModel)
            {
                report.texturesWritten++;
                VortigauntLog::LogF("  ^2texture^7 %s  %dx%d  (%s)%s", material.smdName.c_str(), written.width, written.height, origin.c_str(),
                                    hadAlpha ? "  ^3has transparency^7" : "");
            }
            else if (kind == Guessed)
            {
                report.texturesGuessed++;
                VortigauntLog::LogF("  ^3texture^7 %s  %dx%d  (^3guessed by name:^7 %s)", material.smdName.c_str(), written.width, written.height, origin.c_str());
            }
            else
            {
                report.texturesPlaceholder++;
                VortigauntLog::LogF("  ^3texture^7 %s  ^3no image found, flat colour written in its place^7", material.smdName.c_str());
            }
        }
#else
        warn("Textures are converted by the GUI build only; the SMD names them but no BMP was written.");
#endif
    }

    // write the qc
    if (options.writeQC && !report.animationOnly)
    {
        QCFile qc;
        QCFileSettings settings;
        settings.ModelName = modelName + ".mdl";
        settings.MeshName = modelName;
        settings.Scale = 1.0f;
        settings.Fps = options.fps;
        qc.SetSettings(settings);
        for (const FbxGlbAnimationInfo& anim : report.animations)
        {
            qc.AddSequence(anim.name, anim.file, options.fps);
        }
        const fs::path qcPath = outFolder / FileIO::toPath(modelName + ".qc");
        if (qc.Write(FileIO::fromPath(qcPath)))
        {
            report.qcPath = FileIO::fromPath(qcPath);
        }
        else
        {
            warn("Could not write the QC: " + qc.GetError());
        }
    }

    // log the what happened
    VortigauntLog::LogF("orientation: %s", axisDescription.c_str());
    if (report.animationOnly)
    {
        VortigauntLog::LogF("^3This file has no mesh, only a skeleton and its animations.^7");
        VortigauntLog::LogF("%d bones, %zu animation(s) written to anims/. Use them with a model that has the same skeleton;",
                            report.bones, report.animations.size());
        VortigauntLog::LogF("no reference SMD and no QC were written, since there is nothing to compile on its own.");
    }
    else
    {
        VortigauntLog::LogF("mesh: %d triangles, %d vertices, %d mesh(es) of which %d skinned", report.triangles,
                            report.uniqueVertices, report.meshes, report.skinnedMeshes);
        VortigauntLog::LogF("skeleton: %d bones (the file has %d nodes)", report.bones, report.sourceNodes);
        VortigauntLog::LogF("size: %.1f long, %.1f wide, %.1f tall", report.size[0], report.size[1], report.size[2]);
        VortigauntLog::LogF("animations: %zu", report.animations.size());
    }
    for (const FbxGlbAnimationInfo& anim : report.animations)
    {
        VortigauntLog::LogF("%s  (%d frames)", anim.name.c_str(), anim.frames);
    }

    if (quantizedMeshes > 0)
    {
        VortigauntLog::LogF("%d mesh(es) used quantised vertex data (KHR_mesh_quantization) and were read directly from the file.", quantizedMeshes);
    }
    if (quantizedMismatch)
    {
        warn("Some quantised meshes could not be matched to the file's data; their geometry may be wrong.");
    }
    if (report.mergedBones > 0)
    {
        std::string sample;
        for (size_t i = 0; i < mergedNames.size() && i < 6; i++)
        {
            sample += (i ? ", " : "") + mergedNames[i];
        }
        if (mergedNames.size() > 6) sample += ", ...";
        warn("The model needs " + std::to_string(bonesBefore) + " bones and GoldSrc takes " + std::to_string(options.maxBones) +
             ". The " + std::to_string(report.mergedBones) + " least used end bones were merged into their parents (" + sample +
             "); what they moved now follows the parent.");
    }
    else if (report.bones > kGoldSrcMaxBones)
    {
        warn("The model has " + std::to_string(report.bones) + " bones; GoldSrc stops at " + std::to_string(kGoldSrcMaxBones) + ".");
    }
    if (!report.animationOnly && report.skinnedMeshes == 0)
    {
        warn(report.bones <= 1 ? "The model has no skeleton: it is a static mesh, bound whole to a single bone."
                               : "The model has no skin: every part is bound rigidly to the node it hangs from.");
    }
    if (!report.animationOnly && scene->mNumAnimations == 0)
    {
        VortigauntLog::LogF("  The file has no animation; the QC gets a one frame idle from the reference pose.");
    }
    if (unknownBones > 0)
    {
        warn(std::to_string(unknownBones) + " skin bone(s) have no node of that name in the file and were ignored.");
    }
    if (morphMeshes > 0 || morphChannels > 0)
    {
        warn("The model has morph targets (blend shapes). GoldSrc has no such thing, so facial and shape animation is lost.");
    }
    if (animatedScaleChannels > 0)
    {
        warn(std::to_string(animatedScaleChannels) + " animation channel(s) change a bone's scale. GoldSrc bones only move and turn, so that stretching is lost.");
    }
    if (!report.animationOnly && report.uniqueVertices > kGoldSrcMaxVertsPerBodyPart)
    {
        warn("The mesh has " + std::to_string(report.uniqueVertices) + " vertices and studiomdl takes about " +
             std::to_string(kGoldSrcMaxVertsPerBodyPart) + " per body part. Split the reference SMD via smdcut before compiling" +
             (report.triangles > 100000 ? "; at this size it should be reduced in a modelling tool first." : "."));
    }
    if (!report.animationOnly && report.size[2] > 0.0f && report.size[2] < 16.0f)
    {
        char text[256];
        std::snprintf(text, sizeof(text),
                      "The model is only %.2f units tall, which looks like metres. A GoldSrc player is 72 units; "
                      "$scale %.1f in the QC would make this one that tall.", report.size[2], 72.0f / report.size[2]);
        warn(text);
    }
    if (alphaTextures > 0)
    {
        warn(std::to_string(alphaTextures) + " texture(s) use transparency. A GoldSrc texture is a flat palette; "
             "see-through parts need $texrendermode masked or additive in the QC.");
    }
    for (const std::string& note : textureNotes)
    {
        warn(note);
    }
    if (report.texturesGuessed > 0)
    {
        warn(std::to_string(report.texturesGuessed) + " texture(s) were not named by the model and were picked from its folder by file name. Check they are the right ones.");
    }

    if (!report.referenceSmd.empty())
    {
        VortigauntLog::LogF("^2reference:^7 %s", report.referenceSmd.c_str());
    }
    if (!report.qcPath.empty())
    {
        VortigauntLog::LogF("^2QC:^7 %s", report.qcPath.c_str());
    }
    VortigauntLog::LogF("^2Done:^7 %s", report.outputFolder.c_str());

    report.ok = true;
    return report;
}
