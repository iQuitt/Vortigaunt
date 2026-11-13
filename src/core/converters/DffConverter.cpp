// Copyright (C) 2026 iQuit (https://github.com/iQuitt)
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "DffConverter.h"

#include "core/QCFile.h"
#include "core/autorig/SkeletonMath.h"
#include "core/smd/SmdWriter.h"
#include "utils/Bmp.h"
#include "utils/FileIO.h"

#include <rw.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <tuple>
#include <unordered_map>

namespace fs = std::filesystem;

namespace
{

// librw is one global engine
std::mutex g_rwMutex;
bool g_rwStarted = false;

constexpr uint32_t kNodeNameId = 0x253F2FE;  // frame names, written by GTA's exporters
constexpr int32_t kNodeNameSize = 32;
int32_t g_nodeNameOffset = 0;

void* nameConstruct(void* object, int32_t offset, int32_t)
{
    *PLUGINOFFSET(char, object, offset) = '\0';
    return object;
}

void* nameDestruct(void* object, int32_t, int32_t)
{
    return object;
}

void* nameCopy(void* dst, void* src, int32_t offset, int32_t)
{
    std::memcpy(PLUGINOFFSET(char, dst, offset), PLUGINOFFSET(char, src, offset), kNodeNameSize);
    return dst;
}

rw::Stream* nameRead(rw::Stream* stream, int32_t length, void* object, int32_t offset, int32_t)
{
    char* name = PLUGINOFFSET(char, object, offset);
    const int32_t kept = std::min(length, kNodeNameSize - 1);
    stream->read8(name, kept);
    name[kept] = '\0';
    if (length > kept)
        stream->seek(length - kept);
    return stream;
}

rw::Stream* nameWrite(rw::Stream* stream, int32_t, void*, int32_t, int32_t)
{
    return stream;
}

int32_t nameSize(void*, int32_t, int32_t)
{
    return 0;
}

rw::Texture* nameOnlyTexture(const char* name, const char*)
{
    rw::Texture* texture = rw::Texture::create(nullptr);
    if (texture)
    {
        std::strncpy(texture->name, name, sizeof(texture->name) - 1);
        texture->name[sizeof(texture->name) - 1] = 0;
    }
    return texture;
}

bool startRw()
{
    if (g_rwStarted)
        return true;
    if (!rw::Engine::init())
        return false;
    rw::registerMeshPlugin();
    rw::registerNativeDataPlugin();
    rw::registerAtomicRightsPlugin();
    rw::registerMaterialRightsPlugin();
    rw::registerHAnimPlugin();
    rw::registerSkinPlugin();
    rw::registerMatFXPlugin();
    rw::registerUVAnimPlugin();
    g_nodeNameOffset = rw::Frame::registerPlugin(kNodeNameSize, kNodeNameId, nameConstruct, nameDestruct, nameCopy);
    rw::Frame::registerPluginStream(kNodeNameId, nameRead, nameWrite, nameSize);
    if (!rw::Engine::open(nullptr) || !rw::Engine::start())
        return false;
    rw::Texture::setLoadTextures(true);
    rw::Texture::setCreateDummies(false);
    rw::Texture::readCB = nameOnlyTexture;
    g_rwStarted = true;
    return true;
}

std::string lower(std::string text)
{
    std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return text;
}

bool readFile(const std::string& path, std::vector<uint8_t>& data)
{
    std::ifstream file(FileIO::toPath(path), std::ios::binary);
    if (!file)
        return false;
    data.assign(std::istreambuf_iterator<char>(file), {});
    return true;
}

std::string frameName(rw::Frame* frame)
{
    return PLUGINOFFSET(char, frame, g_nodeNameOffset);
}

// librw keeps basis vectors as rows: right, up, at, pos
aiMatrix4x4 toMatrix(const rw::Matrix& m)
{
    return aiMatrix4x4(m.right.x, m.up.x, m.at.x, m.pos.x,
                       m.right.y, m.up.y, m.at.y, m.pos.y,
                       m.right.z, m.up.z, m.at.z, m.pos.z,
                       0, 0, 0, 1);
}

std::vector<TxdImage> readTxd(std::vector<uint8_t>& data, std::string* error)
{
    std::vector<TxdImage> images;
    rw::StreamMemory stream;
    stream.open(data.data(), uint32_t(data.size()));
    if (!rw::findChunk(&stream, rw::ID_TEXDICTIONARY, nullptr, nullptr))
    {
        if (error)
            *error = "not a texture dictionary";
        return images;
    }
    rw::TexDictionary* txd = rw::TexDictionary::streamRead(&stream);
    stream.close();
    if (!txd)
    {
        if (error)
            *error = "the texture dictionary could not be read";
        return images;
    }
    FORLIST(lnk, txd->textures)
    {
        rw::Texture* texture = rw::Texture::fromDict(lnk);
        rw::Image* image = texture->raster ? texture->raster->toImage() : nullptr;
        if (!image)
            continue;
        image->unpalettize(true);
        image->convertTo32();
        TxdImage out;
        out.name = texture->name;
        out.width = image->width;
        out.height = image->height;
        out.rgba.resize(size_t(out.width) * out.height * 4);
        for (int y = 0; y < out.height; ++y)
            std::memcpy(out.rgba.data() + size_t(y) * out.width * 4, image->pixels + size_t(y) * image->stride, size_t(out.width) * 4);
        for (size_t i = 3; i < out.rgba.size() && !out.alpha; i += 4)
            out.alpha = out.rgba[i] < 255;
        image->destroy();
        images.push_back(std::move(out));
    }
    if (rw::TexDictionary::getCurrent() == txd)
        rw::TexDictionary::setCurrent(nullptr);
    txd->destroy();
    return images;
}

// what GTA's data files say about one model
struct GameInfo
{
    std::string txd;          // its texture dictionary, when not named after the model
    bool vehicle = false;
    rw::RGBA colors[4] = { { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 } };
    bool hasColors = false;
};

std::vector<std::string> splitFields(const std::string& line)
{
    std::vector<std::string> fields;
    std::string field;
    for (char c : line)
    {
        if (c == ',' || c == ' ' || c == '\t' || c == '\r')
        {
            if (!field.empty())
                fields.push_back(field);
            field.clear();
        }
        else
            field += c;
    }
    if (!field.empty())
        fields.push_back(field);
    return fields;
}

std::string stripComment(std::string line)
{
    const size_t hash = line.find('#');
    if (hash != std::string::npos)
        line.erase(hash);
    return line;
}

GameInfo readGameInfo(const std::string& gameFolder, const std::string& model)
{
    GameInfo info;
    if (gameFolder.empty())
        return info;
    const fs::path root = FileIO::toPath(gameFolder);

    // vehicles.ide: id, model, txd, type, ...
    std::ifstream ide(root / "data" / "vehicles.ide");
    for (std::string line; std::getline(ide, line);)
    {
        const std::vector<std::string> fields = splitFields(stripComment(line));
        if (fields.size() >= 4 && lower(fields[1]) == model)
        {
            info.txd = lower(fields[2]);
            info.vehicle = true;
            break;
        }
    }

    // carcols.dat: the palette, then each model's colour pairs (car) or fours (car4); the first is used
    std::ifstream carcols(root / "data" / "carcols.dat");
    std::vector<rw::RGBA> palette;
    std::string section;
    for (std::string line; std::getline(carcols, line);)
    {
        std::vector<std::string> fields = splitFields(stripComment(line));
        if (fields.empty())
            continue;
        const std::string first = lower(fields[0]);
        if (fields.size() == 1 && (first == "col" || first == "car" || first == "car4" || first == "end"))
        {
            section = first == "end" ? "" : first;
            continue;
        }
        if (section == "col" && fields.size() >= 3)
        {
            // "r,g,b" or "r g b", sometimes with a "." between
            int rgb[3];
            int n = 0;
            for (const std::string& field : fields)
                if (n < 3 && std::isdigit(static_cast<unsigned char>(field[0])))
                    rgb[n++] = std::atoi(field.c_str());
            if (n == 3)
                palette.push_back({ uint8_t(rgb[0]), uint8_t(rgb[1]), uint8_t(rgb[2]), 255 });
        }
        else if ((section == "car" || section == "car4") && first == model && !info.hasColors)
        {
            const size_t count = section == "car" ? 2 : 4;
            for (size_t i = 0; i < count && i + 1 < fields.size(); ++i)
            {
                const size_t index = size_t(std::atoi(fields[i + 1].c_str()));
                if (index < palette.size())
                    info.colors[i] = palette[index];
            }
            info.hasColors = fields.size() > 2;
        }
    }
    return info;
}

// GTA faces +Y; an SMD faces -Y, which studiomdl turns 90 degrees to GoldSrc's +X
const aiMatrix3x3 kAxes(-1, 0, 0,
                        0, -1, 0,
                        0, 0, 1);

aiMatrix4x4 orthonormal(aiMatrix4x4 m)
{
    aiVector3D x(m.a1, m.b1, m.c1), y(m.a2, m.b2, m.c2);
    x.Normalize();
    aiVector3D z = (x ^ y).Normalize();
    y = z ^ x;
    m.a1 = x.x; m.b1 = x.y; m.c1 = x.z;
    m.a2 = y.x; m.b2 = y.y; m.c2 = y.z;
    m.a3 = z.x; m.b3 = z.y; m.c3 = z.z;
    return m;
}

struct Piece
{
    rw::Atomic* atomic = nullptr;
    int bone = 0;
    aiMatrix4x4 world;  // GTA space, the frame's, or the copy's for wheels
};

struct MaterialKey
{
    std::string texture;
    uint32_t color = 0xffffff;
    bool translucent = false;
    bool operator<(const MaterialKey& o) const
    {
        return std::tie(texture, color, translucent) < std::tie(o.texture, o.color, o.translucent);
    }
};

// the placeholder colours GTA paints over, and the lights it switches
uint32_t gameColor(const rw::RGBA& c, const GameInfo& info)
{
    const uint32_t rgb = uint32_t(c.red) << 16 | uint32_t(c.green) << 8 | c.blue;
    auto pack = [](const rw::RGBA& p) { return uint32_t(p.red) << 16 | uint32_t(p.green) << 8 | p.blue; };
    switch (rgb)
    {
    case 0x3CFF00: return pack(info.colors[0]);
    case 0xFF00AF: return pack(info.colors[1]);
    case 0x00FFFF: return pack(info.colors[2]);
    case 0xFF00FF: return pack(info.colors[3]);
    case 0xFFAF00: case 0x00FFC8: case 0xB9FF00: case 0xFF3C00: return 0xFFFFFF;
    default: return rgb;
    }
}

} // namespace

bool DffConverter::IsSupportedFile(const std::string& path)
{
    return lower(FileIO::toPath(path).extension().string()) == ".dff";
}

std::vector<TxdImage> DffConverter::ReadTxd(const std::vector<uint8_t>& data, std::string* error)
{
    std::lock_guard lock(g_rwMutex);
    if (!startRw())
    {
        if (error)
            *error = "RenderWare could not start";
        return {};
    }
    std::vector<uint8_t> copy = data;
    return readTxd(copy, error);
}

std::vector<TxdImage> DffConverter::ReadTxd(const std::string& path, std::string* error)
{
    std::vector<uint8_t> data;
    if (!readFile(path, data))
    {
        if (error)
            *error = "cannot open " + path;
        return {};
    }
    return ReadTxd(data, error);
}

std::string DffConverter::FindGameFolder(const std::string& path)
{
    std::error_code ec;
    for (fs::path dir = FileIO::toPath(path).parent_path(); !dir.empty(); dir = dir.parent_path())
    {
        if (fs::exists(dir / "data" / "vehicles.ide", ec) && fs::exists(dir / "models", ec))
            return FileIO::fromPath(dir);
        if (dir == dir.parent_path())
            break;
    }
    return {};
}

DffConvertReport DffConverter::Convert(const std::string& inputPath, const std::string& outputDir)
{
    DffConvertReport report;
    std::vector<uint8_t> data;
    if (!readFile(inputPath, data))
    {
        report.error = "cannot open " + inputPath;
        return report;
    }
    const fs::path input = FileIO::toPath(inputPath);
    const std::string model = lower(input.stem().string());
    std::string gameFolder = m_options.gameFolder;
    if (gameFolder.empty())
        gameFolder = FindGameFolder(inputPath);
    const GameInfo game = readGameInfo(gameFolder, model);

    std::lock_guard lock(g_rwMutex);
    if (!startRw())
    {
        report.error = "RenderWare could not start";
        return report;
    }

    // textures: the model's own dictionary first, then the shared vehicle one
    std::unordered_map<std::string, TxdImage> textures;
    auto addTxd = [&](const fs::path& path) {
        std::vector<uint8_t> bytes;
        std::error_code ec;
        if (!fs::exists(path, ec) || !readFile(FileIO::fromPath(path), bytes))
            return;
        for (TxdImage& image : readTxd(bytes, nullptr))
            textures.try_emplace(lower(image.name), std::move(image));
    };
    const fs::path folder = input.parent_path();
    addTxd(folder / (input.stem().string() + ".txd"));
    if (!game.txd.empty())
        addTxd(folder / (game.txd + ".txd"));
    addTxd(folder / "vehicle.txd");
    if (!gameFolder.empty())
        addTxd(FileIO::toPath(gameFolder) / "models" / "generic" / "vehicle.txd");

    rw::StreamMemory stream;
    stream.open(data.data(), uint32_t(data.size()));
    rw::Clump* clump = nullptr;
    if (rw::findChunk(&stream, rw::ID_CLUMP, nullptr, nullptr))
        clump = rw::Clump::streamRead(&stream);
    stream.close();
    if (!clump)
    {
        report.error = "not a RenderWare model, or of a version librw cannot read";
        return report;
    }

    // every frame a bone, parents first
    std::vector<rw::Frame*> frames;
    std::map<rw::Frame*, int> boneOf;
    std::vector<int> parents;
    std::vector<std::string> names;
    std::vector<aiMatrix4x4> worlds;  // GTA space
    std::function<void(rw::Frame*, int)> walk = [&](rw::Frame* frame, int parent) {
        const int index = int(frames.size());
        frames.push_back(frame);
        boneOf[frame] = index;
        parents.push_back(parent);
        std::string name = frameName(frame);
        // Max exports some with a space in front
        name.erase(0, name.find_first_not_of(' '));
        name.erase(name.find_last_not_of(' ') + 1);
        names.push_back(name.empty() ? "frame" + std::to_string(index) : name);
        worlds.push_back(toMatrix(*frame->getLTM()));
        for (rw::Frame* child = frame->child; child; child = child->next)
            walk(child, index);
    };
    walk(clump->getFrame(), -1);
    if (names[0].empty() || names[0].rfind("frame", 0) == 0)
        names[0] = model;

    // pieces by role: body, _ok/_dam pairs, extras; the LODs (_vlo) stay out
    std::vector<Piece> body;
    std::map<std::string, std::pair<std::vector<Piece>, std::vector<Piece>>> damage;
    std::map<std::string, std::vector<Piece>> extras;
    Piece wheel;
    FORLIST(lnk, clump->atomics)
    {
        rw::Atomic* atomic = rw::Atomic::fromClump(lnk);
        if (!atomic->geometry || !atomic->getFrame())
            continue;
        const int bone = boneOf[atomic->getFrame()];
        const std::string name = lower(names[bone]);
        const Piece piece{ atomic, bone, worlds[bone] };
        auto ends = [&](const char* suffix) {
            const size_t n = std::strlen(suffix);
            return name.size() > n && name.compare(name.size() - n, n, suffix) == 0;
        };
        if (ends("_vlo") || ends("_lo"))
            continue;
        if (ends("_ok"))
            damage[name.substr(0, name.size() - 3)].first.push_back(piece);
        else if (ends("_dam"))
            damage[name.substr(0, name.size() - 4)].second.push_back(piece);
        else if (name.rfind("extra", 0) == 0 && game.vehicle)
            extras[name].push_back(piece);
        else
        {
            body.push_back(piece);
            if (name == "wheel")
                wheel = piece;
        }
    }

    // cars keep one wheel; GTA draws it on every wheel dummy, turned round on the left
    if (wheel.atomic)
    {
        const int wheelParent = parents[wheel.bone];
        const aiMatrix4x4 offset = aiMatrix4x4(worlds[wheelParent]).Inverse() * wheel.world;
        for (size_t i = 0; i < frames.size(); ++i)
        {
            const std::string name = lower(names[i]);
            if (name.rfind("wheel_", 0) != 0 || name.size() < 9 || name.compare(name.size() - 6, 6, "_dummy") != 0 || int(i) == wheelParent)
                continue;
            aiMatrix4x4 turn;
            if (name[6] == 'l')
                aiMatrix4x4::RotationZ(float(AI_MATH_PI), turn);
            const int bone = int(names.size());
            names.push_back(name.substr(0, name.size() - 6));
            parents.push_back(int(i));
            frames.push_back(nullptr);
            worlds.push_back(worlds[i] * turn * offset);
            body.push_back({ wheel.atomic, bone, worlds.back() });
        }
    }

    // bones in GoldSrc space and size
    const float scale = m_options.scale;
    auto toGold = [&](const aiVector3D& p) { return kAxes * p * scale; };
    std::vector<aiMatrix4x4> goldWorlds(worlds.size());
    std::vector<SmdBone> bones(worlds.size());
    for (size_t i = 0; i < worlds.size(); ++i)
    {
        aiMatrix4x4 w = orthonormal(worlds[i]);
        const aiMatrix3x3 rotation = kAxes * aiMatrix3x3(w);
        const aiVector3D position = toGold(aiVector3D(w.a4, w.b4, w.c4));
        goldWorlds[i] = aiMatrix4x4(rotation);
        goldWorlds[i].a4 = position.x;
        goldWorlds[i].b4 = position.y;
        goldWorlds[i].c4 = position.z;
        aiMatrix4x4 local = goldWorlds[i];
        if (parents[i] >= 0)
            local = aiMatrix4x4(goldWorlds[parents[i]]).Inverse() * goldWorlds[i];
        SmdBone& bone = bones[i];
        bone.index = int(i);
        bone.name = names[i];
        bone.parentIndex = parents[i];
        bone.posX = local.a4;
        bone.posY = local.b4;
        bone.posZ = local.c4;
        autorig::SmdEulerAngles(aiMatrix3x3(local), bone.rotX, bone.rotY, bone.rotZ);
    }
    report.bones = int(bones.size());

    // the skin's bones, for peds
    auto skinBones = [&](rw::Atomic* atomic) {
        std::vector<int> map;
        rw::HAnimHierarchy* hierarchy = rw::Skin::getHierarchy(atomic);
        if (!hierarchy)
            hierarchy = rw::HAnimHierarchy::find(clump->getFrame());
        if (!hierarchy)
            return map;
        hierarchy->attach();
        for (int32_t i = 0; i < hierarchy->numNodes; ++i)
        {
            auto it = boneOf.find(hierarchy->nodeInfo[i].frame);
            map.push_back(it == boneOf.end() ? 0 : it->second);
        }
        return map;
    };

    // textures go out once per texture and colour
    std::map<MaterialKey, std::string> written;
    std::set<std::string> missing;
    std::map<std::string, std::string> renderModes;
    const fs::path outFolder = FileIO::toPath(outputDir);
    std::error_code ec;
    fs::create_directories(outFolder, ec);
    auto materialName = [&](rw::Material* material) -> std::string {
        MaterialKey key;
        key.texture = material->texture ? lower(material->texture->name) : std::string();
        key.color = gameColor(material->color, game);
        key.translucent = material->color.alpha < 255;
        auto found = written.find(key);
        if (found != written.end())
            return found->second;

        const TxdImage* image = nullptr;
        if (!key.texture.empty())
        {
            auto it = textures.find(key.texture);
            if (it != textures.end())
                image = &it->second;
            else
                missing.insert(key.texture);
        }
        char hex[8];
        std::snprintf(hex, sizeof(hex), "%06X", key.color);
        std::string name = key.texture.empty() ? std::string("color_") + hex : key.texture;
        if (!key.texture.empty() && key.color != 0xFFFFFF)
            name += std::string("_") + hex;
        if (key.translucent)
            name += "_glass";
        name += ".bmp";

        int width = 16, height = 16;
        std::vector<uint32_t> argb;
        bool alpha = false;
        if (image)
        {
            width = image->width;
            height = image->height;
            alpha = image->alpha;
            argb.resize(size_t(width) * height);
            for (size_t i = 0; i < argb.size(); ++i)
            {
                const uint8_t* p = &image->rgba[i * 4];
                argb[i] = uint32_t(p[3]) << 24 | uint32_t(p[0]) << 16 | uint32_t(p[1]) << 8 | p[2];
            }
        }
        else
            argb.assign(size_t(width) * height, 0xFFFFFFFF);
        // GTA multiplies the texture by the material colour
        const uint32_t tint[3] = { key.color >> 16 & 0xFF, key.color >> 8 & 0xFF, key.color & 0xFF };
        for (uint32_t& p : argb)
        {
            const uint32_t r = (p >> 16 & 0xFF) * tint[0] / 255, g = (p >> 8 & 0xFF) * tint[1] / 255, b = (p & 0xFF) * tint[2] / 255;
            p = (p & 0xFF000000) | r << 16 | g << 8 | b;
        }
        BMP::GoldSrcTextureOptions options;
        options.masked = alpha && !key.translucent;
        BMP::GoldSrcTextureInfo info;
        if (BMP::saveGoldSrcTexture(FileIO::fromPath(outFolder / name), width, height, argb.data(), options, &info))
            ++report.textures;
        if (key.translucent)
            renderModes[name] = "additive";
        else if (info.masked)
            renderModes[name] = "masked";
        written[key] = name;
        return name;
    };

    auto trianglesOf = [&](const std::vector<Piece>& pieces) {
        std::vector<SmdTriangle> triangles;
        for (const Piece& piece : pieces)
        {
            rw::Geometry* geometry = piece.atomic->geometry;
            if (geometry->numMorphTargets < 1 || !geometry->triangles)
                continue;
            const rw::MorphTarget& morph = geometry->morphTargets[0];
            const rw::TexCoords* uvs = geometry->numTexCoordSets > 0 ? geometry->texCoords[0] : nullptr;
            rw::Skin* skin = rw::Skin::get(geometry);
            const std::vector<int> skinMap = skin ? skinBones(piece.atomic) : std::vector<int>();
            const aiMatrix3x3 normalWorld = aiMatrix3x3(orthonormal(piece.world));

            // a skinned vertex sits where its bones put it: the bone's frame times its inverse bind matrix
            std::vector<aiMatrix4x4> skinning;
            if (skin && !skinMap.empty())
            {
                for (int32_t b = 0; b < skin->numBones && b < int32_t(skinMap.size()); ++b)
                    skinning.push_back(worlds[skinMap[b]] * toMatrix(*reinterpret_cast<const rw::Matrix*>(&skin->inverseMatrices[b * 16])));
            }
            std::vector<aiVector3D> positions(geometry->numVertices);
            for (int32_t v = 0; v < geometry->numVertices; ++v)
            {
                const rw::V3d& p = morph.vertices[v];
                const aiVector3D local(p.x, p.y, p.z);
                if (skinning.empty())
                {
                    positions[v] = piece.world * local;
                    continue;
                }
                float total = 0;
                for (int w = 0; w < 4; ++w)
                {
                    const float weight = skin->weights[v * 4 + w];
                    const int index = skin->indices[v * 4 + w];
                    if (weight <= 0 || index >= int(skinning.size()))
                        continue;
                    positions[v] += (skinning[index] * local) * weight;
                    total += weight;
                }
                positions[v] = total > 0 ? positions[v] / total : piece.world * local;
            }

            // GTA's vehicles carry no normals; faces sharing a corner's position within 60 degrees smooth it
            std::vector<aiVector3D> faceNormals(geometry->numTriangles);
            std::map<std::tuple<float, float, float>, std::vector<int>> facesAt;
            for (int32_t t = 0; t < geometry->numTriangles; ++t)
            {
                const rw::Triangle& tri = geometry->triangles[t];
                const aiVector3D& a = positions[tri.v[0]];
                const aiVector3D& b = positions[tri.v[1]];
                const aiVector3D& c = positions[tri.v[2]];
                faceNormals[t] = (b - a) ^ (c - a);
                for (int k = 0; k < 3; ++k)
                {
                    const aiVector3D& p = positions[tri.v[k]];
                    facesAt[{ p.x, p.y, p.z }].push_back(t);
                }
            }

            for (int32_t t = 0; t < geometry->numTriangles; ++t)
            {
                const rw::Triangle& tri = geometry->triangles[t];
                if (tri.matId >= geometry->matList.numMaterials || faceNormals[t].SquareLength() <= 0)
                    continue;
                SmdTriangle out;
                out.material = materialName(geometry->matList.materials[tri.matId]);
                const aiVector3D own = aiVector3D(faceNormals[t]).Normalize();
                for (int k = 0; k < 3; ++k)
                {
                    const int v = tri.v[k];
                    aiVector3D normal;
                    if ((geometry->flags & rw::Geometry::NORMALS) && skinning.empty())
                    {
                        const rw::V3d& n = morph.normals[v];
                        normal = normalWorld * aiVector3D(n.x, n.y, n.z);
                    }
                    else
                    {
                        const aiVector3D& p = positions[v];
                        for (int f : facesAt[{ p.x, p.y, p.z }])
                            if (aiVector3D(faceNormals[f]).Normalize() * own > 0.5f)
                                normal += faceNormals[f];
                    }
                    if (normal.SquareLength() <= 0)
                        normal = own;
                    normal = kAxes * normal.Normalize();
                    const aiVector3D position = toGold(positions[v]);

                    int bone = piece.bone;
                    if (skin && !skinMap.empty())
                    {
                        int best = 0;
                        for (int w = 1; w < 4; ++w)
                            if (skin->weights[v * 4 + w] > skin->weights[v * 4 + best])
                                best = w;
                        const int index = skin->indices[v * 4 + best];
                        if (index < int(skinMap.size()))
                            bone = skinMap[index];
                    }
                    SmdVertex& corner = out.vertices[k];
                    corner.boneIndex = bone;
                    corner.x = position.x;
                    corner.y = position.y;
                    corner.z = position.z;
                    corner.nx = normal.x;
                    corner.ny = normal.y;
                    corner.nz = normal.z;
                    corner.u = uvs ? uvs[v].u : 0.0f;
                    corner.v = uvs ? 1.0f - uvs[v].v : 0.0f;
                }
                triangles.push_back(out);
            }
        }
        return triangles;
    };

    SmdWriter writer;
    writer.SetSkeleton(bones);
    auto writeSmd = [&](const std::string& name, const std::vector<Piece>& pieces) -> std::string {
        const std::vector<SmdTriangle> triangles = trianglesOf(pieces);
        const std::string file = name + ".smd";
        if (!writer.ExportSmdTriangles(FileIO::fromPath(outFolder / file), triangles, {}))
            return {};
        report.triangles += int(triangles.size());
        return file;
    };

    const std::string bodySmd = writeSmd(model, body);
    if (bodySmd.empty())
    {
        report.error = "cannot write " + FileIO::fromPath(outFolder / (model + ".smd"));
        clump->destroy();
        return report;
    }
    report.smdPath = FileIO::fromPath(outFolder / bodySmd);

    QCFile qc;
    QCFileSettings settings;
    settings.ModelName = model + ".mdl";
    settings.MeshName = model;
    qc.SetSettings(settings);
    qc.AddBody("body", model);
    for (const auto& [name, pair] : damage)
    {
        if (pair.first.empty())
            continue;
        std::vector<std::string> choices{ writeSmd(model + "_" + name + "_ok", pair.first) };
        if (m_options.damagedParts && !pair.second.empty())
            choices.push_back(writeSmd(model + "_" + name + "_dam", pair.second));
        for (std::string& choice : choices)
            choice = fs::path(choice).stem().string();
        qc.AddBodyGroup(name, choices);
        report.bodyGroups.push_back(name);
    }
    if (m_options.extras)
    {
        for (const auto& [name, pieces] : extras)
        {
            const std::string smd = writeSmd(model + "_" + name, pieces);
            qc.AddBodyGroup(name, { "blank", fs::path(smd).stem().string() });
            report.bodyGroups.push_back(name);
        }
    }
    for (const auto& [texture, mode] : renderModes)
        qc.AddTextureRenderMode(texture, mode);
    // GTA marks where smoke, lights and the driver go with empty frames; studiomdl drops boneless frames, so they
    // become attachments on the root, which always stays (GoldSrc keeps four)
    {
        const char* marks[] = { "exhaust", "headlights", "taillights", "ped_frontseat" };
        const aiMatrix4x4 toRoot = aiMatrix4x4(goldWorlds[0]).Inverse();
        int index = 0;
        for (const char* mark : marks)
        {
            for (size_t i = 0; i < names.size(); ++i)
            {
                if (lower(names[i]) != mark)
                    continue;
                const aiVector3D at = toRoot * aiVector3D(goldWorlds[i].a4, goldWorlds[i].b4, goldWorlds[i].c4);
                qc.AddAttachment(index++, names[0], at.x, at.y, at.z);
                report.attachments.push_back(mark);
                break;
            }
        }
    }
    // bikes: steering and both wheels, on the axes GTA turns them (local Z, local X)
    auto hasBone = [&](const char* name) { return std::find(names.begin(), names.end(), name) != names.end(); };
    if (hasBone("forks_front") && hasBone("wheel_front") && hasBone("wheel_rear"))
    {
        qc.AddController(0, "forks_front", "ZR", -45, 45);
        if (hasBone("handlebars"))
            qc.AddController(1, "handlebars", "ZR", -45, 45);
        qc.AddController(2, "wheel_front", "XR", 0, 360);
        qc.AddController(3, "wheel_rear", "XR", 0, 360);
        report.controllers = 4;
    }
    qc.AddSequence("idle", model, 30);
    report.missingTextures.assign(missing.begin(), missing.end());
    clump->destroy();

    if (m_options.writeQC)
    {
        const fs::path qcPath = outFolder / (model + ".qc");
        if (!qc.Write(FileIO::fromPath(qcPath)))
        {
            report.error = qc.GetError();
            return report;
        }
        report.qcPath = FileIO::fromPath(qcPath);
    }
    report.ok = true;
    return report;
}

ConvertResult DffConverter::ConvertFile(const std::string& inputPath, const std::string& outputDir)
{
    ConvertResult result;
    const fs::path folder = FileIO::toPath(outputDir) / FileIO::toPath(inputPath).stem();
    const DffConvertReport report = Convert(inputPath, FileIO::fromPath(folder));
    result.ok = report.ok;
    result.error = report.error;
    result.outputFolder = FileIO::fromPath(folder);
    if (!report.qcPath.empty())
        result.qcFiles.push_back(report.qcPath);
    return result;
}
