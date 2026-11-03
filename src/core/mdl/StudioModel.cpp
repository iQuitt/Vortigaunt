// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "core/mdl/StudioModel.h"

#include "utils/FileIO.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace {

constexpr float kDegreesToRadians = 3.14159265358979f / 180.0f;

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
            sequence.boundsMin = vectorAt(at + 96);
            sequence.boundsMax = vectorAt(at + 108);
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
                    sequence.events.push_back({ intAt(file, eventAt), intAt(file, eventAt + 4), readName(file, eventAt + 12, 64), intAt(file, eventAt + 8) });
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
        auto group = std::make_shared<std::vector<uint8_t>>();
        if (readFile(sibling(modelPath, suffix), *group))
            m_groupFiles[g] = std::move(group);
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
            controller.rest = intAt(file, at + 16);
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
    if (externalTextures)
        m_textureFile = std::make_shared<const std::vector<uint8_t>>(textureFile);


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
                mesh.skinRef = skinRef;
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

    m_groupFiles[0] = std::make_shared<const std::vector<uint8_t>>(std::move(file));
    m_path = path;
    return true;
}

namespace {

void putBytes(std::vector<uint8_t>& file, size_t offset, const void* data, size_t size)
{
    if (offset <= file.size() && file.size() - offset >= size)
        std::memcpy(file.data() + offset, data, size);
}

void putInt(std::vector<uint8_t>& file, size_t offset, int32_t value)
{
    putBytes(file, offset, &value, 4);
}

void putFloat(std::vector<uint8_t>& file, size_t offset, float value)
{
    putBytes(file, offset, &value, 4);
}

void putVector(std::vector<uint8_t>& file, size_t offset, const aiVector3D& value)
{
    const float v[3] = { value.x, value.y, value.z };
    putBytes(file, offset, v, sizeof(v));
}

// fixed-size name, cut to keep the closing zero
void putName(std::vector<uint8_t>& file, size_t offset, size_t length, const std::string& text)
{
    if (offset > file.size() || file.size() - offset < length)
        return;
    std::memset(file.data() + offset, 0, length);
    std::memcpy(file.data() + offset, text.data(), std::min(text.size(), length - 1));
}

// 4-byte aligned space at the end of the file
size_t appendRoom(std::vector<uint8_t>& file, size_t size)
{
    const size_t at = (file.size() + 3) & ~static_cast<size_t>(3);
    file.resize(at + size, 0);
    return at;
}

bool writeFile(const std::filesystem::path& path, const std::vector<uint8_t>& data)
{
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(data.data()), static_cast<std::streamsize>(data.size()));
    return out.good();
}

}  // namespace

bool StudioModel::Save(const std::string& path, std::string& error) const
{
    if (m_groupFiles.empty() || !m_groupFiles[0])
    {
        error = "The model was not read from a file";
        return false;
    }
    std::vector<uint8_t> file = *m_groupFiles[0];

    putName(file, 8, 64, name);
    putVector(file, 76, eyePosition);
    putVector(file, 88, clipMin);
    putVector(file, 100, clipMax);
    putVector(file, 112, boundsMin);
    putVector(file, 124, boundsMax);
    putInt(file, 136, flags);

    const size_t boneIndex = static_cast<size_t>(intAt(file, 144));
    const size_t boneCount = std::min(bones.size(), static_cast<size_t>(std::max(0, intAt(file, 140))));
    for (size_t i = 0; i < boneCount; i++)
    {
        const size_t at = boneIndex + i * kBoneSize;
        putName(file, at, 32, bones[i].name);
        putBytes(file, at + 64, bones[i].value, sizeof(bones[i].value));
        putBytes(file, at + 88, bones[i].scale, sizeof(bones[i].scale));
    }

    // tables stay in place; entries dropped on load (no bone) are left out
    const size_t controllerIndex = static_cast<size_t>(intAt(file, 152));
    const size_t controllerCount = std::min(controllers.size(), static_cast<size_t>(std::max(0, intAt(file, 148))));
    for (size_t i = 0; i < controllerCount; i++)
    {
        const size_t at = controllerIndex + i * kControllerSize;
        const StudioBoneController& c = controllers[i];
        putInt(file, at, c.bone);
        putInt(file, at + 4, c.type);
        putFloat(file, at + 8, c.start);
        putFloat(file, at + 12, c.end);
        putInt(file, at + 16, c.rest);
        putInt(file, at + 20, c.index);
    }
    putInt(file, 148, static_cast<int32_t>(controllerCount));

    const size_t hitboxIndex = static_cast<size_t>(intAt(file, 160));
    const size_t hitboxCount = std::min(hitboxes.size(), static_cast<size_t>(std::max(0, intAt(file, 156))));
    for (size_t i = 0; i < hitboxCount; i++)
    {
        const size_t at = hitboxIndex + i * kHitboxSize;
        putInt(file, at, hitboxes[i].bone);
        putInt(file, at + 4, hitboxes[i].group);
        putVector(file, at + 8, hitboxes[i].min);
        putVector(file, at + 20, hitboxes[i].max);
    }
    putInt(file, 156, static_cast<int32_t>(hitboxCount));

    // more attachments than before move to the end of the file
    size_t attachmentIndex = static_cast<size_t>(intAt(file, 216));
    if (attachments.size() > static_cast<size_t>(std::max(0, intAt(file, 212))))
        attachmentIndex = appendRoom(file, attachments.size() * kAttachmentSize);
    for (size_t i = 0; i < attachments.size(); i++)
    {
        const size_t at = attachmentIndex + i * kAttachmentSize;
        putName(file, at, 32, attachments[i].name);
        putInt(file, at + 32, 0);
        putInt(file, at + 36, attachments[i].bone);
        putVector(file, at + 40, attachments[i].origin);
    }
    putInt(file, 212, static_cast<int32_t>(attachments.size()));
    putInt(file, 216, static_cast<int32_t>(attachments.empty() ? 0 : attachmentIndex));

    const size_t sequenceIndex = static_cast<size_t>(intAt(file, 168));
    const size_t sequenceCount = std::min(sequences.size(), static_cast<size_t>(std::max(0, intAt(file, 164))));
    for (size_t i = 0; i < sequenceCount; i++)
    {
        const size_t at = sequenceIndex + i * kSequenceSize;
        const StudioSequence& s = sequences[i];
        putFloat(file, at + 32, s.fps);
        putInt(file, at + 36, s.flags);
        putInt(file, at + 40, s.activity);
        putInt(file, at + 44, s.activityWeight);
        putVector(file, at + 76, s.linearMovement);
        putVector(file, at + 96, s.boundsMin);
        putVector(file, at + 108, s.boundsMax);
        // more events than before move to the end of the file
        const size_t had = static_cast<size_t>(std::max(0, intAt(file, at + 48)));
        size_t eventIndex = static_cast<size_t>(intAt(file, at + 52));
        if (s.events.size() > had)
            eventIndex = appendRoom(file, s.events.size() * kEventSize);
        for (size_t e = 0; e < s.events.size(); e++)
        {
            const size_t eventAt = eventIndex + e * kEventSize;
            putInt(file, eventAt, s.events[e].frame);
            putInt(file, eventAt + 4, s.events[e].event);
            putInt(file, eventAt + 8, s.events[e].type);
            putName(file, eventAt + 12, 64, s.events[e].options);
        }
        putInt(file, at + 48, static_cast<int32_t>(s.events.size()));
        putInt(file, at + 52, static_cast<int32_t>(s.events.empty() ? 0 : eventIndex));
    }

    const size_t bodyPartIndex = static_cast<size_t>(intAt(file, 208));
    const size_t bodyPartCount = std::min(bodyParts.size(), static_cast<size_t>(std::max(0, intAt(file, 204))));
    for (size_t b = 0; b < bodyPartCount; b++)
    {
        const size_t partAt = bodyPartIndex + b * kBodyPartSize;
        const size_t modelIndex = static_cast<size_t>(intAt(file, partAt + 72));
        const size_t modelCount = std::min(bodyParts[b].models.size(), static_cast<size_t>(std::max(0, intAt(file, partAt + 64))));
        for (size_t m = 0; m < modelCount; m++)
        {
            const size_t modelAt = modelIndex + m * kModelSize;
            const StudioSubModel& sub = bodyParts[b].models[m];
            putName(file, modelAt, 64, sub.name);
            // vertices in place (scaling changes them)
            const size_t vertexIndex = static_cast<size_t>(intAt(file, modelAt + 88));
            const size_t vertexCount = std::min(sub.vertices.size(), static_cast<size_t>(std::max(0, intAt(file, modelAt + 80))));
            for (size_t v = 0; v < vertexCount; v++) putVector(file, vertexIndex + v * 12, sub.vertices[v]);
        }
    }

    // textures live here or in nameT.mdl
    std::vector<uint8_t> textureFile;
    if (externalTextures && m_textureFile)
        textureFile = *m_textureFile;
    std::vector<uint8_t>& textureSource = externalTextures && m_textureFile ? textureFile : file;
    const size_t textureIndex = static_cast<size_t>(intAt(textureSource, 184));
    const size_t textureCount = std::min(textures.size(), static_cast<size_t>(std::max(0, intAt(textureSource, 180))));
    // resize factor per texture; mesh UVs are in pixels
    std::vector<std::pair<float, float>> grew(textureCount, { 1.0f, 1.0f });
    bool anyGrew = false;
    for (size_t i = 0; i < textureCount; i++)
    {
        const size_t at = textureIndex + i * kTextureSize;
        const StudioTexture& t = textures[i];
        putName(textureSource, at, 64, t.name);
        putInt(textureSource, at + 64, t.flags);
        const size_t pixelCount = static_cast<size_t>(std::max(0, t.width)) * static_cast<size_t>(std::max(0, t.height));
        if (t.pixels.size() != pixelCount || t.palette.size() != 768 || pixelCount == 0)
            continue;
        const int oldWidth = intAt(textureSource, at + 68), oldHeight = intAt(textureSource, at + 72);
        size_t dataIndex = static_cast<size_t>(intAt(textureSource, at + 76));
        if (oldWidth != t.width || oldHeight != t.height)
        {
            // a resized picture moves to the end of the file
            dataIndex = appendRoom(textureSource, pixelCount + 768);
            putInt(textureSource, at + 68, t.width);
            putInt(textureSource, at + 72, t.height);
            putInt(textureSource, at + 76, static_cast<int32_t>(dataIndex));
            grew[i] = { oldWidth > 0 ? static_cast<float>(t.width) / oldWidth : 1.0f, oldHeight > 0 ? static_cast<float>(t.height) / oldHeight : 1.0f };
            anyGrew = true;
        }
        putBytes(textureSource, dataIndex, t.pixels.data(), pixelCount);
        putBytes(textureSource, dataIndex + pixelCount, t.palette.data(), 768);
    }
    if (anyGrew)
    {
        // meshes reach their texture through skin family 0
        const int numSkinRef = intAt(textureSource, 192);
        const size_t skinIndex = static_cast<size_t>(intAt(textureSource, 200));
        for (size_t b = 0; b < bodyPartCount; b++)
        {
            const size_t partAt = bodyPartIndex + b * kBodyPartSize;
            const size_t modelIndex = static_cast<size_t>(intAt(file, partAt + 72));
            const int modelCount = intAt(file, partAt + 64);
            for (int m = 0; m < modelCount; m++)
            {
                const size_t modelAt = modelIndex + static_cast<size_t>(m) * kModelSize;
                const int meshCount = intAt(file, modelAt + 72);
                const size_t meshIndex = static_cast<size_t>(intAt(file, modelAt + 76));
                for (int k = 0; k < meshCount; k++)
                {
                    const size_t meshAt = meshIndex + static_cast<size_t>(k) * kMeshSize;
                    const int skinRef = intAt(file, meshAt + 8);
                    int16_t texture = static_cast<int16_t>(skinRef);
                    if (skinRef >= 0 && skinRef < numSkinRef)
                        readAt(textureSource, skinIndex + static_cast<size_t>(skinRef) * 2, texture);
                    if (texture < 0 || static_cast<size_t>(texture) >= grew.size() || (grew[texture].first == 1.0f && grew[texture].second == 1.0f))
                        continue;
                    // tricmds: count (<0 fan), then vertex, normal, s, t per corner
                    size_t at = static_cast<size_t>(intAt(file, meshAt + 4));
                    for (;;)
                    {
                        int16_t count = 0;
                        if (!readAt(file, at, count) || count == 0)
                            break;
                        at += 2;
                        const size_t corners = static_cast<size_t>(count < 0 ? -count : count);
                        if (!inside(file, at, corners, 8))
                            break;
                        for (size_t c = 0; c < corners; c++)
                        {
                            int16_t st[2];
                            std::memcpy(st, file.data() + at + c * 8 + 4, 4);
                            st[0] = static_cast<int16_t>(std::lround(st[0] * grew[texture].first));
                            st[1] = static_cast<int16_t>(std::lround(st[1] * grew[texture].second));
                            std::memcpy(file.data() + at + c * 8 + 4, st, 4);
                        }
                        at += corners * 8;
                    }
                }
            }
        }
    }


    // changed submodels are rewritten at the end; the engine lights normals mesh by mesh, so group them per mesh
    for (size_t b = 0; b < bodyPartCount; b++)
    {
        const size_t partAt = bodyPartIndex + b * kBodyPartSize;
        const size_t modelIndex = static_cast<size_t>(intAt(file, partAt + 72));
        const size_t modelCount = std::min(bodyParts[b].models.size(), static_cast<size_t>(std::max(0, intAt(file, partAt + 64))));
        for (size_t m = 0; m < modelCount; m++)
        {
            const size_t modelAt = modelIndex + m * kModelSize;
            const StudioSubModel& sub = bodyParts[b].models[m];
            const size_t meshIndex = static_cast<size_t>(intAt(file, modelAt + 76));
            if (static_cast<size_t>(std::max(0, intAt(file, modelAt + 72))) != sub.meshes.size() || !inside(file, meshIndex, sub.meshes.size(), kMeshSize))
                continue;
            bool changed = static_cast<size_t>(intAt(file, modelAt + 80)) != sub.vertices.size() ||
                           static_cast<size_t>(intAt(file, modelAt + 92)) != sub.normals.size();
            for (size_t k = 0; k < sub.meshes.size() && !changed; k++)
                changed = static_cast<size_t>(intAt(file, meshIndex + k * kMeshSize)) != sub.meshes[k].triangles.size() / 3;
            if (!changed)
                continue;

            std::vector<aiVector3D> normals;
            std::vector<uint8_t> normalBones;
            std::vector<std::vector<int>> placed(sub.meshes.size());
            std::vector<size_t> meshNormals(sub.meshes.size());
            for (size_t k = 0; k < sub.meshes.size(); k++)
            {
                placed[k].assign(sub.normals.size(), -1);
                const size_t first = normals.size();
                for (const StudioTriVertex& corner : sub.meshes[k].triangles)
                    if (placed[k][corner.normal] < 0)
                    {
                        placed[k][corner.normal] = static_cast<int>(normals.size());
                        normals.push_back(sub.normals[corner.normal]);
                        normalBones.push_back(static_cast<uint8_t>(sub.normalBones[corner.normal]));
                    }
                meshNormals[k] = normals.size() - first;
            }
            if (sub.vertices.size() > static_cast<size_t>(kStudioMaxVertices) || normals.size() > static_cast<size_t>(kStudioMaxVertices))
            {
                error = "Submodel \"" + sub.name + "\" has more vertices or normals than GoldSrc draws (" + std::to_string(kStudioMaxVertices) + ")";
                return false;
            }

            const size_t vertexInfo = appendRoom(file, sub.vertices.size());
            for (size_t v = 0; v < sub.vertices.size(); v++) file[vertexInfo + v] = static_cast<uint8_t>(sub.vertexBones[v]);
            const size_t vertexData = appendRoom(file, sub.vertices.size() * 12);
            for (size_t v = 0; v < sub.vertices.size(); v++) putVector(file, vertexData + v * 12, sub.vertices[v]);
            const size_t normalInfo = appendRoom(file, normals.size());
            if (!normals.empty())
                std::memcpy(file.data() + normalInfo, normalBones.data(), normals.size());
            const size_t normalData = appendRoom(file, normals.size() * 12);
            for (size_t n = 0; n < normals.size(); n++) putVector(file, normalData + n * 12, normals[n]);
            putInt(file, modelAt + 80, static_cast<int32_t>(sub.vertices.size()));
            putInt(file, modelAt + 84, static_cast<int32_t>(vertexInfo));
            putInt(file, modelAt + 88, static_cast<int32_t>(vertexData));
            putInt(file, modelAt + 92, static_cast<int32_t>(normals.size()));
            putInt(file, modelAt + 96, static_cast<int32_t>(normalInfo));
            putInt(file, modelAt + 100, static_cast<int32_t>(normalData));

            for (size_t k = 0; k < sub.meshes.size(); k++)
            {
                const StudioMesh& mesh = sub.meshes[k];
                const size_t meshAt = meshIndex + k * kMeshSize;
                const float width = mesh.texture >= 0 && mesh.texture < static_cast<int>(textures.size()) ? static_cast<float>(textures[mesh.texture].width) : 1.0f;
                const float height = mesh.texture >= 0 && mesh.texture < static_cast<int>(textures.size()) ? static_cast<float>(textures[mesh.texture].height) : 1.0f;
                // one 3-corner strip per face, ended by 0
                const size_t faces = mesh.triangles.size() / 3;
                const size_t runs = appendRoom(file, faces * (2 + 3 * 8) + 2);
                size_t at = runs;
                for (size_t i = 0; i < faces * 3; i++)
                {
                    if (i % 3 == 0)
                    {
                        const int16_t three = 3;
                        putBytes(file, at, &three, 2);
                        at += 2;
                    }
                    const StudioTriVertex& corner = mesh.triangles[i];
                    const int16_t values[4] = { static_cast<int16_t>(corner.vertex), static_cast<int16_t>(placed[k][corner.normal]),
                                                static_cast<int16_t>(std::lround(corner.u * width)), static_cast<int16_t>(std::lround(corner.v * height)) };
                    putBytes(file, at, values, 8);
                    at += 8;
                }
                putInt(file, meshAt, static_cast<int32_t>(faces));
                putInt(file, meshAt + 4, static_cast<int32_t>(runs));
                putInt(file, meshAt + 12, static_cast<int32_t>(meshNormals[k]));
            }
        }
    }

    putInt(file, 72, static_cast<int32_t>(file.size()));
    const std::filesystem::path target = FileIO::toPath(path);
    if (!writeFile(target, file))
    {
        error = "Could not write " + path;
        return false;
    }
    if (externalTextures && m_textureFile)
    {
        putInt(textureFile, 72, static_cast<int32_t>(textureFile.size()));
        if (!writeFile(sibling(target, "T"), textureFile))
        {
            error = "Could not write the textures beside " + path;
            return false;
        }
    }
    // copy name01.mdl... under the new name
    if (FileIO::toPath(m_path) != target)
        for (size_t g = 1; g < m_groupFiles.size(); g++)
        {
            if (!m_groupFiles[g])
                continue;
            char suffix[8];
            std::snprintf(suffix, sizeof(suffix), "%02d", static_cast<int>(g));
            if (!writeFile(sibling(target, suffix), *m_groupFiles[g]))
            {
                error = "Could not write the animations beside " + path;
                return false;
            }
        }
    return true;
}

void StudioModel::Rotate(const aiVector3D& degrees)
{
    aiMatrix4x4 turn;
    turn.FromEulerAnglesXYZ(degrees.x * kDegreesToRadians, degrees.y * kDegreesToRadians, degrees.z * kDegreesToRadians);
    for (StudioBone& bone : bones)
    {
        if (bone.parent >= 0)
            continue;
        aiMatrix4x4 local;
        local.FromEulerAnglesXYZ(bone.value[3], bone.value[4], bone.value[5]);
        local.a4 = bone.value[0];
        local.b4 = bone.value[1];
        local.c4 = bone.value[2];
        aiVector3D scaling, angles, position;
        (turn * local).Decompose(scaling, angles, position);
        const float values[6] = { position.x, position.y, position.z, angles.x, angles.y, angles.z };
        std::copy(values, values + 6, bone.value);
    }
}

void StudioModel::Move(const aiVector3D& offset)
{
    for (StudioBone& bone : bones)
    {
        if (bone.parent >= 0)
            continue;
        bone.value[0] += offset.x;
        bone.value[1] += offset.y;
        bone.value[2] += offset.z;
    }
}

void StudioModel::Scale(float factor, int parts)
{
    if (parts & ScaleMeshes)
        for (StudioBodyPart& part : bodyParts)
            for (StudioSubModel& sub : part.models)
                for (aiVector3D& vertex : sub.vertices) vertex *= factor;
    if (parts & ScaleHitboxes)
        for (StudioHitbox& hitbox : hitboxes)
        {
            hitbox.min *= factor;
            hitbox.max *= factor;
        }
    if (parts & ScaleSequenceBoxes)
        for (StudioSequence& sequence : sequences)
        {
            sequence.boundsMin *= factor;
            sequence.boundsMax *= factor;
            sequence.linearMovement *= factor;
        }
    // position = value + scale * animation value
    if (parts & ScaleBones)
        for (StudioBone& bone : bones)
            for (int i = 0; i < 3; i++)
            {
                bone.value[i] *= factor;
                bone.scale[i] *= factor;
            }
    if (parts & ScaleEyePosition)
        eyePosition *= factor;
    if (parts & ScaleAttachments)
        for (StudioAttachment& attachment : attachments) attachment.origin *= factor;
}

bool StudioModel::MakeDoubleSided(float thickness, bool rims, int texture, int& added, std::string& error)
{
    added = 0;
    std::vector<StudioBodyPart> made = bodyParts;
    for (StudioBodyPart& part : made)
        for (StudioSubModel& sub : part.models)
        {
            auto chosen = [texture](const StudioMesh& mesh) { return texture < 0 || mesh.texture == texture; };
            // rotated to start at the lowest index, so a face and its twin differ only in winding
            auto faceKey = [](int a, int b, int c) {
                if (b < a && b < c) return std::array<int, 3>{ b, c, a };
                if (c < a && c < b) return std::array<int, 3>{ c, a, b };
                return std::array<int, 3>{ a, b, c };
            };
            std::vector<std::array<int, 3>> faces;
            for (const StudioMesh& mesh : sub.meshes)
                for (size_t i = 0; i + 2 < mesh.triangles.size(); i += 3)
                    faces.push_back(faceKey(mesh.triangles[i].vertex, mesh.triangles[i + 1].vertex, mesh.triangles[i + 2].vertex));
            std::sort(faces.begin(), faces.end());

            // inner shell: vertices pushed in along their mean normal
            const size_t vertexCount = sub.vertices.size();
            std::vector<int> inner(vertexCount, -1);
            if (thickness != 0.0f)
            {
                std::vector<aiVector3D> along(vertexCount);
                for (const StudioMesh& mesh : sub.meshes)
                    if (chosen(mesh))
                        for (const StudioTriVertex& corner : mesh.triangles) along[corner.vertex] += sub.normals[corner.normal];
                for (size_t v = 0; v < vertexCount; v++)
                {
                    if (along[v].SquareLength() == 0.0f)
                        continue;
                    along[v].Normalize();
                    inner[v] = static_cast<int>(sub.vertices.size());
                    sub.vertices.push_back(sub.vertices[v] - along[v] * thickness);
                    sub.vertexBones.push_back(sub.vertexBones[v]);
                }
            }
            std::vector<int> flipped(sub.normals.size(), -1);
            auto back = [&](StudioTriVertex corner) {
                if (inner[corner.vertex] >= 0)
                    corner.vertex = inner[corner.vertex];
                if (flipped[corner.normal] < 0)
                {
                    flipped[corner.normal] = static_cast<int>(sub.normals.size());
                    sub.normals.push_back(-sub.normals[corner.normal]);
                    sub.normalBones.push_back(sub.normalBones[corner.normal]);
                }
                corner.normal = flipped[corner.normal];
                return corner;
            };

            // original face counts and directed edges, for the rims
            std::vector<size_t> had(sub.meshes.size());
            std::vector<std::pair<int, int>> edges;
            for (size_t k = 0; k < sub.meshes.size(); k++)
            {
                StudioMesh& mesh = sub.meshes[k];
                had[k] = mesh.triangles.size();
                if (!chosen(mesh))
                    continue;
                for (size_t i = 0; i + 2 < had[k]; i += 3)
                {
                    const StudioTriVertex a = mesh.triangles[i], b = mesh.triangles[i + 1], c = mesh.triangles[i + 2];
                    edges.insert(edges.end(), { { a.vertex, b.vertex }, { b.vertex, c.vertex }, { c.vertex, a.vertex } });
                    // already double-sided
                    if (thickness == 0.0f && std::binary_search(faces.begin(), faces.end(), faceKey(a.vertex, c.vertex, b.vertex)))
                        continue;
                    mesh.triangles.insert(mesh.triangles.end(), { back(a), back(c), back(b) });
                    added++;
                }
            }
            if (rims && thickness != 0.0f)
            {
                std::sort(edges.begin(), edges.end());
                for (size_t k = 0; k < sub.meshes.size(); k++)
                {
                    StudioMesh& mesh = sub.meshes[k];
                    if (!chosen(mesh))
                        continue;
                    for (size_t i = 0; i + 2 < had[k]; i += 3)
                    {
                        const StudioTriVertex corners[3] = { mesh.triangles[i], mesh.triangles[i + 1], mesh.triangles[i + 2] };
                        const aiVector3D middle = (sub.vertices[corners[0].vertex] + sub.vertices[corners[1].vertex] + sub.vertices[corners[2].vertex]) / 3.0f;
                        for (int e = 0; e < 3; e++)
                        {
                            StudioTriVertex outerA = corners[e], outerB = corners[(e + 1) % 3];
                            // open edge: no face uses it in reverse
                            if (std::binary_search(edges.begin(), edges.end(), std::pair{ outerB.vertex, outerA.vertex }))
                                continue;
                            // rim normal points away from the face
                            const aiVector3D& from = sub.vertices[outerA.vertex];
                            const aiVector3D side = sub.vertices[outerB.vertex] - from;
                            aiVector3D out = from + side * 0.5f - middle;
                            if (side.SquareLength() > 0.0f)
                                out -= side * ((out * side) / side.SquareLength());
                            outerA.normal = outerB.normal = static_cast<int>(sub.normals.size());
                            sub.normals.push_back(out.NormalizeSafe());
                            sub.normalBones.push_back(sub.vertexBones[outerA.vertex]);
                            StudioTriVertex innerA = outerA, innerB = outerB;
                            innerA.vertex = inner[outerA.vertex];
                            innerB.vertex = inner[outerB.vertex];
                            mesh.triangles.insert(mesh.triangles.end(), { outerB, outerA, innerA, outerB, innerA, innerB });
                            added += 2;
                        }
                    }
                }
            }
            if (sub.vertices.size() > static_cast<size_t>(kStudioMaxVertices) || sub.normals.size() > static_cast<size_t>(kStudioMaxVertices))
            {
                error = "Submodel \"" + sub.name + "\" would have " + std::to_string(sub.vertices.size()) + " vertices and " +
                        std::to_string(sub.normals.size()) + " normals; GoldSrc draws at most " + std::to_string(kStudioMaxVertices) + " of each";
                added = 0;
                return false;
            }
        }
    bodyParts = std::move(made);
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
        if (data.group >= 0 && data.group < static_cast<int>(m_groupFiles.size()) && m_groupFiles[data.group] && !m_groupFiles[data.group]->empty())
        {
            file = m_groupFiles[data.group].get();
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

std::vector<aiMatrix4x4> StudioModel::LocalBoneTransforms(int sequence, int frame, float blend,
                                                         const std::vector<std::array<float, 6>>* adjust, float between) const
{
    const bool known = sequence >= 0 && sequence < static_cast<int>(sequences.size());
    const int blends = known ? sequences[sequence].blends : 1;
    const float place = std::clamp(blend, 0.0f, 1.0f) * static_cast<float>(blends - 1);
    const int lower = std::min(static_cast<int>(place), blends - 1);
    const int upper = std::min(lower + 1, blends - 1);
    const float mix = place - static_cast<float>(lower);
    const int next = known ? std::min(frame + 1, std::max(0, sequences[sequence].frames - 1)) : frame;
    const float toward = next != frame ? std::clamp(between, 0.0f, 1.0f) : 0.0f;

    // lerp positions, slerp angles
    struct Pose
    {
        std::vector<aiVector3D> position;
        std::vector<aiQuaternion> rotation;
    };
    auto pose = [&](int blendIndex, int at) {
        std::vector<std::array<float, 6>> values;
        BoneValues(sequence, blendIndex, at, values);
        Pose made;
        made.position.resize(bones.size());
        made.rotation.resize(bones.size());
        for (size_t i = 0; i < bones.size(); i++)
        {
            std::array<float, 6> value = values[i];
            if (adjust && i < adjust->size())
                for (int j = 0; j < 6; j++) value[j] += (*adjust)[i][j];
            aiMatrix4x4 rotation;
            rotation.FromEulerAnglesXYZ(value[3], value[4], value[5]);
            made.position[i] = aiVector3D(value[0], value[1], value[2]);
            made.rotation[i] = aiQuaternion(aiMatrix3x3(rotation));
        }
        return made;
    };
    auto mixInto = [this](Pose& into, const Pose& other, float amount) {
        for (size_t i = 0; i < bones.size(); i++)
        {
            aiQuaternion mixed;
            aiQuaternion::Interpolate(mixed, into.rotation[i], other.rotation[i], amount);
            into.rotation[i] = mixed;
            into.position[i] = into.position[i] + (other.position[i] - into.position[i]) * amount;
        }
    };
    // this frame and the next, in both blends
    auto framePose = [&](int blendIndex) {
        Pose made = pose(blendIndex, frame);
        if (toward > 0.0f)
            mixInto(made, pose(blendIndex, next), toward);
        return made;
    };
    Pose result = framePose(lower);
    if (upper != lower && mix > 0.0f)
        mixInto(result, framePose(upper), mix);

    std::vector<aiMatrix4x4> local(bones.size());
    for (size_t i = 0; i < bones.size(); i++)
    {
        local[i] = aiMatrix4x4(result.rotation[i].GetMatrix());
        local[i].a4 = result.position[i].x;
        local[i].b4 = result.position[i].y;
        local[i].c4 = result.position[i].z;
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
