#include "core/mdl/StudioModel.h"

#include "utils/FileIO.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace {

// from studio.h.
constexpr size_t kBoneSize = 112;
constexpr size_t kSequenceSize = 176;
constexpr size_t kTextureSize = 80;
constexpr size_t kBodyPartSize = 76;
constexpr size_t kModelSize = 112;
constexpr size_t kMeshSize = 20;
constexpr size_t kAttachmentSize = 88;
constexpr size_t kAnimSize = 12;
constexpr size_t kEventSize = 76;
constexpr size_t kHitboxSize = 32;
constexpr size_t kControllerSize = 24;  

template <typename T>
bool readAt(const std::vector<uint8_t>& file, size_t offset, T& out)
{
    if (offset > file.size() || file.size() - offset < sizeof(T))
    {
        return false;
    }
    std::memcpy(&out, file.data() + offset, sizeof(T));
    return true;
}

bool inside(const std::vector<uint8_t>& file, size_t offset, size_t count, size_t elementSize)
{
    if (offset > file.size())
    {
        return false;
    }
    return count <= (file.size() - offset) / (elementSize ? elementSize : 1);
}

std::string readName(const std::vector<uint8_t>& file, size_t offset, size_t length)
{
    if (!inside(file, offset, length, 1))
    {
        return std::string();
    }
    const char* start = reinterpret_cast<const char*>(file.data() + offset);
    size_t n = 0;
    while (n < length && start[n] != '\0') n++;
    return std::string(start, n);
}

int32_t intAt(const std::vector<uint8_t>& file, size_t offset)
{
    int32_t value = 0;
    readAt(file, offset, value);
    return value;
}

bool readFile(const std::filesystem::path& path, std::vector<uint8_t>& out)
{
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f)
    {
        return false;
    }
    const std::streamsize size = f.tellg();
    if (size <= 0)
    {
        return false;
    }
    out.resize(static_cast<size_t>(size));
    f.seekg(0);
    f.read(reinterpret_cast<char*>(out.data()), size);
    return f.good();
}

// name.mdl -> nameT.mdl, name01.mdl
std::filesystem::path sibling(const std::filesystem::path& model, const std::string& suffix)
{
    std::filesystem::path out = model;
    out.replace_filename(model.stem().native() + std::filesystem::path(suffix).native() + model.extension().native());
    return out;
}

}  // namespace

bool StudioModel::Fail(const std::string& message)
{
    m_error = message;
    return false;
}

bool StudioModel::Load(const std::string& path)
{
    *this = StudioModel();

    const std::filesystem::path modelPath = FileIO::toPath(path);
    std::vector<uint8_t> file;
    if (!readFile(modelPath, file))
    {
        return Fail("Could not read " + path);
    }
    if (file.size() < 244 || std::memcmp(file.data(), "IDST", 4) != 0)
    {
        return Fail("Not a GoldSrc studio model (no IDST header): " + path);
    }
    if (intAt(file, 4) != 10)
    {
        return Fail("Unsupported studio model version " + std::to_string(intAt(file, 4)) + " (GoldSrc is 10)");
    }

    name = readName(file, 8, 64);
    auto vectorAt = [&file](size_t offset) {
        float v[3] = {};
        readAt(file, offset, v);
        return aiVector3D(v[0], v[1], v[2]);
    };
    eyePosition = vectorAt(76);
    clipMin = vectorAt(88);
    clipMax = vectorAt(100);
    boundsMin = vectorAt(112);
    boundsMax = vectorAt(124);
    flags = intAt(file, 136);

    // ---- bones ----
    const int numBones = intAt(file, 140);
    const size_t boneIndex = static_cast<size_t>(intAt(file, 144));
    if (numBones < 0 || !inside(file, boneIndex, static_cast<size_t>(numBones), kBoneSize))
    {
        return Fail("The bone table lies outside the file");
    }
    bones.resize(static_cast<size_t>(numBones));
    for (int i = 0; i < numBones; i++)
    {
        const size_t at = boneIndex + static_cast<size_t>(i) * kBoneSize;
        StudioBone& bone = bones[i];
        bone.name = readName(file, at, 32);
        bone.parent = intAt(file, at + 32);
        if (bone.parent >= numBones || bone.parent == i)
        {
            bone.parent = -1;
        }
        std::memcpy(bone.value, file.data() + at + 64, sizeof(bone.value));
        std::memcpy(bone.scale, file.data() + at + 88, sizeof(bone.scale));
    }

    // A bone that is its own ancestor would send every walk up the skeleton
    // round for ever; such a bone is cut loose.
    for (int i = 0; i < numBones; i++)
    {
        int steps = 0;
        for (int p = bones[i].parent; p >= 0; p = bones[p].parent)
        {
            if (++steps > numBones)
            {
                bones[i].parent = -1;
                break;
            }
        }
    }

    // sequences
    const int numSequences = intAt(file, 164);
    const size_t sequenceIndex = static_cast<size_t>(intAt(file, 168));
    if (numSequences > 0 && inside(file, sequenceIndex, static_cast<size_t>(numSequences), kSequenceSize))
    {
        for (int i = 0; i < numSequences; i++)
        {
            const size_t at = sequenceIndex + static_cast<size_t>(i) * kSequenceSize;
            StudioSequence sequence;
            sequence.name = readName(file, at, 32);
            readAt(file, at + 32, sequence.fps);
            sequence.frames = intAt(file, at + 56);
            sequence.blends = std::clamp(intAt(file, at + 120), 1, 16);
            readAt(file, at + 136, sequence.blendStart);
            readAt(file, at + 144, sequence.blendEnd);
            sequence.flags = intAt(file, at + 36);
            sequence.activity = intAt(file, at + 40);
            sequence.activityWeight = intAt(file, at + 44);
            sequence.motionType = intAt(file, at + 68);
            sequence.linearMovement = vectorAt(at + 76);
            sequence.blendType = intAt(file, at + 128);
            sequence.entryNode = intAt(file, at + 160);
            sequence.exitNode = intAt(file, at + 164);
            sequence.nodeFlags = intAt(file, at + 168);
            const int numEvents = intAt(file, at + 48);
            const size_t eventIndex = static_cast<size_t>(intAt(file, at + 52));
            if (numEvents > 0 && inside(file, eventIndex, static_cast<size_t>(numEvents), kEventSize))
            {
                for (int e = 0; e < numEvents; e++)
                {
                    const size_t eventAt = eventIndex + static_cast<size_t>(e) * kEventSize;
                    sequence.events.push_back({ intAt(file, eventAt), intAt(file, eventAt + 4), readName(file, eventAt + 12, 64) });
                }
            }
            sequences.push_back(sequence);

            SequenceData data;
            data.offset = static_cast<uint32_t>(intAt(file, at + 124));
            data.group = intAt(file, at + 156);
            m_sequenceData.push_back(data);
        }
    }

    // Animations in other models: name01.mdl, name02.mdl ...
    int maxGroup = 0;
    for (const SequenceData& data : m_sequenceData) maxGroup = std::max(maxGroup, data.group);
    maxGroup = std::min(maxGroup, 99);
    m_groupFiles.resize(static_cast<size_t>(maxGroup) + 1);
    for (int g = 1; g <= maxGroup; g++)
    {
        char suffix[8];
        std::snprintf(suffix, sizeof(suffix), "%02d", g);
        readFile(sibling(modelPath, suffix), m_groupFiles[g]);  
    }

    //attachments
    const int numAttachments = intAt(file, 212);
    const size_t attachmentIndex = static_cast<size_t>(intAt(file, 216));
    if (numAttachments > 0 && inside(file, attachmentIndex, static_cast<size_t>(numAttachments), kAttachmentSize))
    {
        for (int i = 0; i < numAttachments; i++)
        {
            const size_t at = attachmentIndex + static_cast<size_t>(i) * kAttachmentSize;
            StudioAttachment attachment;
            attachment.name = readName(file, at, 32);
            attachment.bone = intAt(file, at + 36);
            float origin[3] = {};
            readAt(file, at + 40, origin);
            attachment.origin = aiVector3D(origin[0], origin[1], origin[2]);
            if (attachment.bone >= 0 && attachment.bone < numBones)
            {
                attachments.push_back(attachment);
            }
        }
    }

    // hit boxes and bone controllers
    const int numHitboxes = intAt(file, 156);
    const size_t hitboxIndex = static_cast<size_t>(intAt(file, 160));
    if (numHitboxes > 0 && inside(file, hitboxIndex, static_cast<size_t>(numHitboxes), kHitboxSize))
    {
        for (int i = 0; i < numHitboxes; i++)
        {
            const size_t at = hitboxIndex + static_cast<size_t>(i) * kHitboxSize;
            const int bone = intAt(file, at);
            if (bone >= 0 && bone < numBones)
                hitboxes.push_back({ bone, intAt(file, at + 4), vectorAt(at + 8), vectorAt(at + 20) });
        }
    }
    const int numControllers = intAt(file, 148);
    const size_t controllerIndex = static_cast<size_t>(intAt(file, 152));
    if (numControllers > 0 && inside(file, controllerIndex, static_cast<size_t>(numControllers), kControllerSize))
    {
        for (int i = 0; i < numControllers; i++)
        {
            const size_t at = controllerIndex + static_cast<size_t>(i) * kControllerSize;
            StudioBoneController controller;
            controller.bone = intAt(file, at);
            controller.type = intAt(file, at + 4);
            readAt(file, at + 8, controller.start);
            readAt(file, at + 12, controller.end);
            controller.index = intAt(file, at + 20);
            if (controller.bone >= 0 && controller.bone < numBones)
                controllers.push_back(controller);
        }
    }

    // textures
    std::vector<uint8_t> textureFile;
    const std::vector<uint8_t>* textureSource = &file;
    if (intAt(file, 180) == 0)
    {
        if (readFile(sibling(modelPath, "T"), textureFile) || readFile(sibling(modelPath, "t"), textureFile))
        {
            if (textureFile.size() >= 244 && std::memcmp(textureFile.data(), "IDST", 4) == 0)
            {
                textureSource = &textureFile;
                externalTextures = true;
            }
        }
    }
    ReadTextures(*textureSource);


    std::vector<int16_t> skin;
    {
        const int numSkinRef = intAt(*textureSource, 192);
        const size_t skinIndex = static_cast<size_t>(intAt(*textureSource, 200));
        if (numSkinRef > 0 && inside(*textureSource, skinIndex, static_cast<size_t>(numSkinRef), 2))
        {
            skin.resize(static_cast<size_t>(numSkinRef));
            std::memcpy(skin.data(), textureSource->data() + skinIndex, skin.size() * 2);
        }
        const int numFamilies = intAt(*textureSource, 196);
        if (numSkinRef > 0 && numFamilies > 0 &&
            inside(*textureSource, skinIndex, static_cast<size_t>(numSkinRef) * static_cast<size_t>(numFamilies), 2))
        {
            for (int f = 0; f < numFamilies; f++)
            {
                std::vector<int> family(static_cast<size_t>(numSkinRef));
                for (int r = 0; r < numSkinRef; r++)
                {
                    int16_t texture = 0;
                    readAt(*textureSource, skinIndex + (static_cast<size_t>(f) * numSkinRef + r) * 2, texture);
                    family[r] = texture;
                }
                skinFamilies.push_back(std::move(family));
            }
        }
    }

    // body parts
    const int numBodyParts = intAt(file, 204);
    const size_t bodyPartIndex = static_cast<size_t>(intAt(file, 208));
    if (numBodyParts < 0 || !inside(file, bodyPartIndex, static_cast<size_t>(numBodyParts), kBodyPartSize))
    {
        return Fail("The body part table lies outside the file");
    }

    for (int b = 0; b < numBodyParts; b++)
    {
        const size_t partAt = bodyPartIndex + static_cast<size_t>(b) * kBodyPartSize;
        StudioBodyPart part;
        part.name = readName(file, partAt, 64);
        const int numModels = intAt(file, partAt + 64);
        const size_t modelIndex = static_cast<size_t>(intAt(file, partAt + 72));
        if (numModels < 0 || !inside(file, modelIndex, static_cast<size_t>(numModels), kModelSize))
        {
            return Fail("A model table lies outside the file");
        }

        for (int m = 0; m < numModels; m++)
        {
            const size_t modelAt = modelIndex + static_cast<size_t>(m) * kModelSize;
            StudioSubModel model;
            model.name = readName(file, modelAt, 64);

            const int numMeshes = intAt(file, modelAt + 72);
            const size_t meshIndex = static_cast<size_t>(intAt(file, modelAt + 76));
            const int numVertices = intAt(file, modelAt + 80);
            const size_t vertexInfoIndex = static_cast<size_t>(intAt(file, modelAt + 84));
            const size_t vertexIndex = static_cast<size_t>(intAt(file, modelAt + 88));
            const int numNormals = intAt(file, modelAt + 92);
            const size_t normalInfoIndex = static_cast<size_t>(intAt(file, modelAt + 96));
            const size_t normalIndex = static_cast<size_t>(intAt(file, modelAt + 100));

            if (numVertices < 0 || numNormals < 0 || numMeshes < 0 ||
                !inside(file, vertexIndex, static_cast<size_t>(numVertices), 12) ||
                !inside(file, vertexInfoIndex, static_cast<size_t>(numVertices), 1) ||
                !inside(file, normalIndex, static_cast<size_t>(numNormals), 12) ||
                !inside(file, normalInfoIndex, static_cast<size_t>(numNormals), 1) ||
                !inside(file, meshIndex, static_cast<size_t>(numMeshes), kMeshSize))
            {
                return Fail("Model \"" + model.name + "\" points outside the file");
            }

            model.vertices.resize(static_cast<size_t>(numVertices));
            model.vertexBones.resize(static_cast<size_t>(numVertices));
            for (int v = 0; v < numVertices; v++)
            {
                float p[3];
                std::memcpy(p, file.data() + vertexIndex + static_cast<size_t>(v) * 12, 12);
                model.vertices[v] = aiVector3D(p[0], p[1], p[2]);
                const int bone = file[vertexInfoIndex + static_cast<size_t>(v)];
                model.vertexBones[v] = bone < numBones ? bone : 0;
            }
            model.normals.resize(static_cast<size_t>(numNormals));
            model.normalBones.resize(static_cast<size_t>(numNormals));
            for (int n = 0; n < numNormals; n++)
            {
                float p[3];
                std::memcpy(p, file.data() + normalIndex + static_cast<size_t>(n) * 12, 12);
                model.normals[n] = aiVector3D(p[0], p[1], p[2]);
                const int bone = file[normalInfoIndex + static_cast<size_t>(n)];
                model.normalBones[n] = bone < numBones ? bone : 0;
            }

            for (int k = 0; k < numMeshes; k++)
            {
                const size_t meshAt = meshIndex + static_cast<size_t>(k) * kMeshSize;
                StudioMesh mesh;
                const int skinRef = intAt(file, meshAt + 8);
                int textureIndex = skinRef;
                if (skinRef >= 0 && skinRef < static_cast<int>(skin.size()))
                {
                    textureIndex = skin[skinRef];
                }
                mesh.texture = (textureIndex >= 0 && textureIndex < static_cast<int>(textures.size())) ? textureIndex : -1;

                const float width = mesh.texture >= 0 ? static_cast<float>(std::max(1, textures[mesh.texture].width)) : 1.0f;
                const float height = mesh.texture >= 0 ? static_cast<float>(std::max(1, textures[mesh.texture].height)) : 1.0f;


                size_t at = static_cast<size_t>(intAt(file, meshAt + 4));
                std::vector<StudioTriVertex> run;
                for (;;)
                {
                    int16_t count = 0;
                    if (!readAt(file, at, count) || count == 0)
                    {
                        break;
                    }
                    at += 2;
                    const bool fan = count < 0;
                    const size_t n = static_cast<size_t>(fan ? -count : count);
                    if (!inside(file, at, n, 8))
                    {
                        break;
                    }
                    run.resize(n);
                    for (size_t i = 0; i < n; i++)
                    {
                        int16_t raw[4];
                        std::memcpy(raw, file.data() + at + i * 8, 8);
                        run[i].vertex = raw[0];
                        run[i].normal = raw[1];
                        run[i].u = static_cast<float>(raw[2]) / width;
                        run[i].v = static_cast<float>(raw[3]) / height;
                    }
                    at += n * 8;

                    for (size_t i = 2; i < n; i++)
                    {
                        StudioTriVertex tri[3];
                        if (fan)
                        {
                            tri[0] = run[0]; tri[1] = run[i - 1]; tri[2] = run[i];
                        }
                        else if (i % 2 == 0)
                        {
                            tri[0] = run[i - 2]; tri[1] = run[i - 1]; tri[2] = run[i];
                        }
                        else
                        {
                            tri[0] = run[i - 1]; tri[1] = run[i - 2]; tri[2] = run[i];
                        }
                        bool valid = true;
                        for (const StudioTriVertex& tv : tri)
                        {
                            if (tv.vertex < 0 || tv.vertex >= numVertices || tv.normal < 0 || tv.normal >= numNormals)
                            {
                                valid = false;
                            }
                        }
                        if (valid)
                        {
                            mesh.triangles.insert(mesh.triangles.end(), tri, tri + 3);
                        }
                    }
                }
                model.meshes.push_back(std::move(mesh));
            }
            part.models.push_back(std::move(model));
        }
        bodyParts.push_back(std::move(part));
    }

    m_groupFiles[0] = std::move(file);
    return true;
}

bool StudioModel::TransformBoneVertices(const std::string& inputPath, const std::string& outputPath, int bone,
                                        const aiMatrix4x4& transform, std::string& error)
{
    std::vector<uint8_t> file;
    const std::filesystem::path input = FileIO::toPath(inputPath);
    if (!readFile(input, file) || file.size() < 244 || std::memcmp(file.data(), "IDST", 4) != 0 || intAt(file, 4) != 10)
    {
        error = "Not a GoldSrc studio model: " + inputPath;
        return false;
    }
    const int numBones = intAt(file, 140);
    if (bone < 0 || bone >= numBones)
    {
        error = "The model has no bone " + std::to_string(bone);
        return false;
    }

    // normals turn with the transform but keep their length
    aiMatrix3x3 turn(transform);
    const int numBodyParts = intAt(file, 204);
    const size_t bodyPartIndex = static_cast<size_t>(intAt(file, 208));
    if (numBodyParts < 0 || !inside(file, bodyPartIndex, static_cast<size_t>(numBodyParts), kBodyPartSize))
    {
        error = "The body part table lies outside the file";
        return false;
    }
    int moved = 0;
    for (int b = 0; b < numBodyParts; b++)
    {
        const size_t partAt = bodyPartIndex + static_cast<size_t>(b) * kBodyPartSize;
        const int numModels = intAt(file, partAt + 64);
        const size_t modelIndex = static_cast<size_t>(intAt(file, partAt + 72));
        if (numModels < 0 || !inside(file, modelIndex, static_cast<size_t>(numModels), kModelSize))
            continue;
        for (int m = 0; m < numModels; m++)
        {
            const size_t modelAt = modelIndex + static_cast<size_t>(m) * kModelSize;
            const struct { int count; size_t info; size_t data; bool normal; } arrays[] = {
                { intAt(file, modelAt + 80), static_cast<size_t>(intAt(file, modelAt + 84)), static_cast<size_t>(intAt(file, modelAt + 88)), false },
                { intAt(file, modelAt + 92), static_cast<size_t>(intAt(file, modelAt + 96)), static_cast<size_t>(intAt(file, modelAt + 100)), true },
            };
            for (const auto& array : arrays)
            {
                if (array.count <= 0 || !inside(file, array.data, static_cast<size_t>(array.count), 12) ||
                    !inside(file, array.info, static_cast<size_t>(array.count), 1))
                    continue;
                for (int i = 0; i < array.count; i++)
                {
                    if (file[array.info + static_cast<size_t>(i)] != bone)
                        continue;
                    float p[3];
                    uint8_t* at = file.data() + array.data + static_cast<size_t>(i) * 12;
                    std::memcpy(p, at, 12);
                    aiVector3D v(p[0], p[1], p[2]);
                    v = array.normal ? (turn * v).NormalizeSafe() : transform * v;
                    p[0] = v.x;
                    p[1] = v.y;
                    p[2] = v.z;
                    std::memcpy(at, p, 12);
                    moved += array.normal ? 0 : 1;
                }
            }
        }
    }
    if (moved == 0)
    {
        error = "No vertex of the model is on that bone";
        return false;
    }

    const std::filesystem::path output = FileIO::toPath(outputPath);
    std::ofstream out(output, std::ios::binary);
    out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
    if (!out.good())
    {
        error = "Could not write " + outputPath;
        return false;
    }
    out.close();

    std::error_code ec;
    if (intAt(file, 180) == 0 && !std::filesystem::equivalent(input, output, ec))
    {
        for (const char* suffix : { "T", "t" })
        {
            const std::filesystem::path textures = sibling(input, suffix);
            if (std::filesystem::exists(textures, ec))
            {
                std::filesystem::copy_file(textures, sibling(output, "T"), std::filesystem::copy_options::overwrite_existing, ec);
                break;
            }
        }
    }
    return true;
}

bool StudioModel::AddTextureFlags(const std::string& path, int flags, std::string& error)
{
    std::vector<uint8_t> file;
    if (!readFile(FileIO::toPath(path), file) || file.size() < 244 || std::memcmp(file.data(), "IDST", 4) != 0 || intAt(file, 4) != 10)
    {
        error = "Not a GoldSrc studio model: " + path;
        return false;
    }
    const int numTextures = intAt(file, 180);
    const size_t textureIndex = static_cast<size_t>(intAt(file, 184));
    if (numTextures < 0 || !inside(file, textureIndex, static_cast<size_t>(numTextures), kTextureSize))
    {
        error = "The texture table lies outside the file";
        return false;
    }
    for (int i = 0; i < numTextures; i++)
    {
        const size_t at = textureIndex + static_cast<size_t>(i) * kTextureSize + 64;
        const int32_t value = intAt(file, at) | flags;
        std::memcpy(file.data() + at, &value, 4);
    }
    std::ofstream out(FileIO::toPath(path), std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(file.data()), static_cast<std::streamsize>(file.size()));
    if (!out.good())
    {
        error = "Could not write " + path;
        return false;
    }
    return true;
}

bool StudioModel::ReadTextures(const std::vector<uint8_t>& file)
{
    const int numTextures = intAt(file, 180);
    const size_t textureIndex = static_cast<size_t>(intAt(file, 184));
    if (numTextures <= 0 || !inside(file, textureIndex, static_cast<size_t>(numTextures), kTextureSize))
    {
        return false;
    }
    for (int i = 0; i < numTextures; i++)
    {
        const size_t at = textureIndex + static_cast<size_t>(i) * kTextureSize;
        StudioTexture texture;
        texture.name = readName(file, at, 64);
        texture.flags = intAt(file, at + 64);
        texture.width = intAt(file, at + 68);
        texture.height = intAt(file, at + 72);
        const size_t dataIndex = static_cast<size_t>(intAt(file, at + 76));

        // Pixels first, the palette straight after them.
        if (texture.width > 0 && texture.height > 0 && texture.width <= 4096 && texture.height <= 4096)
        {
            const size_t pixelCount = static_cast<size_t>(texture.width) * static_cast<size_t>(texture.height);
            if (inside(file, dataIndex, pixelCount + 768, 1))
            {
                texture.pixels.assign(file.begin() + dataIndex, file.begin() + dataIndex + pixelCount);
                texture.palette.assign(file.begin() + dataIndex + pixelCount, file.begin() + dataIndex + pixelCount + 768);
            }
        }
        textures.push_back(std::move(texture));
    }
    return true;
}

void StudioModel::BoneValues(int sequence, int blendIndex, int frame, std::vector<std::array<float, 6>>& values) const
{
    values.assign(bones.size(), {});
    const std::vector<uint8_t>* file = nullptr;
    size_t animBase = 0;
    if (sequence >= 0 && sequence < static_cast<int>(m_sequenceData.size()))
    {
        const SequenceData& data = m_sequenceData[sequence];
        if (data.group >= 0 && data.group < static_cast<int>(m_groupFiles.size()) && !m_groupFiles[data.group].empty())
        {
            file = &m_groupFiles[data.group];
            animBase = data.offset + static_cast<size_t>(blendIndex) * bones.size() * kAnimSize;
            if (!inside(*file, animBase, bones.size(), kAnimSize))
            {
                file = nullptr;
            }
        }
        frame = std::clamp(frame, 0, std::max(0, sequences[sequence].frames - 1));
    }

    for (size_t i = 0; i < bones.size(); i++)
    {
        const StudioBone& bone = bones[i];
        float* value = values[i].data();
        std::memcpy(value, bone.value, sizeof(bone.value));

        if (file)
        {
            const size_t animAt = animBase + i * kAnimSize;
            uint16_t offsets[6] = {};
            readAt(*file, animAt, offsets);
            for (int j = 0; j < 6; j++)
            {
                if (offsets[j] == 0)
                {
                    continue;
                }

                size_t at = animAt + offsets[j];
                int k = frame;
                bool ok = true;
                for (;;)
                {
                    uint8_t header[2];
                    if (!readAt(*file, at, header))
                    {
                        ok = false;
                        break;
                    }
                    const int valid = header[0], total = header[1];
                    if (total == 0)
                    {
                        ok = false;
                        break;
                    }
                    if (total > k)
                    {
                        int16_t sample = 0;
                        const size_t index = static_cast<size_t>(valid > k ? k + 1 : valid);
                        if (readAt(*file, at + index * 2, sample))
                        {
                            value[j] = bone.value[j] + static_cast<float>(sample) * bone.scale[j];
                        }
                        break;
                    }
                    k -= total;
                    at += static_cast<size_t>(valid + 1) * 2;
                }
                (void)ok;
            }
        }
    }
}

std::vector<std::array<float, 6>> StudioModel::AnimationValues(int sequence, int blendIndex, int frame) const
{
    std::vector<std::array<float, 6>> values;
    BoneValues(sequence, blendIndex, frame, values);
    return values;
}

std::vector<aiMatrix4x4> StudioModel::LocalBoneTransforms(int sequence, int frame, float blend) const
{
    const int blends = sequence >= 0 && sequence < static_cast<int>(sequences.size()) ? sequences[sequence].blends : 1;
    const float place = std::clamp(blend, 0.0f, 1.0f) * static_cast<float>(blends - 1);
    const int lower = std::min(static_cast<int>(place), blends - 1);
    const int upper = std::min(lower + 1, blends - 1);
    const float mix = place - static_cast<float>(lower);
    std::vector<std::array<float, 6>> first;
    BoneValues(sequence, lower, frame, first);
    const bool blended = upper != lower && mix > 0.0f;
    std::vector<std::array<float, 6>> second;
    if (blended)
        BoneValues(sequence, upper, frame, second);

    std::vector<aiMatrix4x4> local(bones.size());
    for (size_t i = 0; i < bones.size(); i++)
    {
        const std::array<float, 6>& value = first[i];
        aiMatrix4x4 rotation;
        rotation.FromEulerAnglesXYZ(value[3], value[4], value[5]);
        aiVector3D position(value[0], value[1], value[2]);
        if (blended)
        {
            // positions mixed straight, angles through quaternions
            const std::array<float, 6>& other = second[i];
            aiMatrix4x4 otherRotation;
            otherRotation.FromEulerAnglesXYZ(other[3], other[4], other[5]);
            aiQuaternion mixed;
            aiQuaternion::Interpolate(mixed, aiQuaternion(aiMatrix3x3(rotation)), aiQuaternion(aiMatrix3x3(otherRotation)), mix);
            rotation = aiMatrix4x4(mixed.GetMatrix());
            position = position + (aiVector3D(other[0], other[1], other[2]) - position) * mix;
        }
        local[i] = rotation;
        local[i].a4 = position.x;
        local[i].b4 = position.y;
        local[i].c4 = position.z;
    }
    return local;
}

std::vector<aiMatrix4x4> StudioModel::BoneTransforms(int sequence, int frame, float blend) const
{
    const std::vector<aiMatrix4x4> local = LocalBoneTransforms(sequence, frame, blend);

    std::vector<aiMatrix4x4> world(bones.size());
    std::vector<bool> done(bones.size(), false);
    std::vector<int> chain;
    for (size_t i = 0; i < bones.size(); i++)
    {
        chain.clear();
        for (int b = static_cast<int>(i); b >= 0 && !done[b]; b = bones[b].parent)
        {
            chain.push_back(b);
        }
        for (size_t k = chain.size(); k-- > 0;)
        {
            const int b = chain[k];
            const int parent = bones[b].parent;
            world[b] = parent >= 0 ? world[parent] * local[b] : local[b];
            done[b] = true;
        }
    }
    return world;
}
