// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "core/mdl/StudioDecompiler.h"

#include "core/mdl/StudioModel.h"
#include "utils/Bmp.h"
#include "utils/FileIO.h"
#include "utils/util.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <set>

namespace fs = std::filesystem;

namespace {

// from studio.h
constexpr int kLooping = 0x0001;
constexpr int kLinearMotion = 0x0040 | 0x0080 | 0x0100;  // LX LY LZ
constexpr int kRotatingController = 0x8000;
constexpr int kMouthController = 4;
constexpr int kFlatShade = 0x0001;
constexpr int kFullBright = 0x0004;

// the Half-Life activities, in their numbering; 0 is none
const char* const kActivities[] = {
    nullptr, "ACT_IDLE", "ACT_GUARD", "ACT_WALK", "ACT_RUN", "ACT_FLY", "ACT_SWIM", "ACT_HOP", "ACT_LEAP", "ACT_FALL",
    "ACT_LAND", "ACT_STRAFE_LEFT", "ACT_STRAFE_RIGHT", "ACT_ROLL_LEFT", "ACT_ROLL_RIGHT", "ACT_TURN_LEFT", "ACT_TURN_RIGHT",
    "ACT_CROUCH", "ACT_CROUCHIDLE", "ACT_STAND", "ACT_USE", "ACT_SIGNAL1", "ACT_SIGNAL2", "ACT_SIGNAL3", "ACT_TWITCH",
    "ACT_COWER", "ACT_SMALL_FLINCH", "ACT_BIG_FLINCH", "ACT_RANGE_ATTACK1", "ACT_RANGE_ATTACK2", "ACT_MELEE_ATTACK1",
    "ACT_MELEE_ATTACK2", "ACT_RELOAD", "ACT_ARM", "ACT_DISARM", "ACT_EAT", "ACT_DIESIMPLE", "ACT_DIEBACKWARD",
    "ACT_DIEFORWARD", "ACT_DIEVIOLENT", "ACT_BARNACLE_HIT", "ACT_BARNACLE_PULL", "ACT_BARNACLE_CHOMP", "ACT_BARNACLE_CHEW",
    "ACT_SLEEP", "ACT_INSPECT_FLOOR", "ACT_INSPECT_WALL", "ACT_IDLE_ANGRY", "ACT_WALK_HURT", "ACT_RUN_HURT", "ACT_HOVER",
    "ACT_GLIDE", "ACT_FLY_LEFT", "ACT_FLY_RIGHT", "ACT_DETECT_SCENT", "ACT_SNIFF", "ACT_BITE", "ACT_THREAT_DISPLAY",
    "ACT_FEAR_DISPLAY", "ACT_EXCITED", "ACT_SPECIAL_ATTACK1", "ACT_SPECIAL_ATTACK2", "ACT_COMBAT_IDLE", "ACT_WALK_SCARED",
    "ACT_RUN_SCARED", "ACT_VICTORY_DANCE", "ACT_DIE_HEADSHOT", "ACT_DIE_CHESTSHOT", "ACT_DIE_GUTSHOT", "ACT_DIE_BACKSHOT",
    "ACT_FLINCH_HEAD", "ACT_FLINCH_CHEST", "ACT_FLINCH_STOMACH", "ACT_FLINCH_LEFTARM", "ACT_FLINCH_RIGHTARM",
    "ACT_FLINCH_LEFTLEG", "ACT_FLINCH_RIGHTLEG",
};

// the motion flags by name, as QC writes them
const struct { int flag; const char* name; } kMotions[] = {
    { 0x0001, "X" }, { 0x0002, "Y" }, { 0x0004, "Z" }, { 0x0008, "XR" }, { 0x0010, "YR" }, { 0x0020, "ZR" },
    { 0x0040, "LX" }, { 0x0080, "LY" }, { 0x0100, "LZ" }, { 0x0200, "AX" }, { 0x0400, "AY" }, { 0x0800, "AZ" },
    { 0x1000, "AXR" }, { 0x2000, "AYR" }, { 0x4000, "AZR" },
};

std::string motionNames(int flags)
{
    std::string out;
    for (const auto& motion : kMotions)
    {
        if (flags & motion.flag)
            out += std::string(out.empty() ? "" : " ") + motion.name;
    }
    return out;
}

std::string format(const char* pattern, ...)
{
    char line[512];
    va_list args;
    va_start(args, pattern);
    std::vsnprintf(line, sizeof(line), pattern, args);
    va_end(args);
    return line;
}

std::string inQuotes(const std::string& text)
{
    std::string out = text;
    std::replace(out.begin(), out.end(), '"', '\'');
    return "\"" + out + "\"";
}

std::string bmpName(const std::string& texture)
{
    std::string name = FileIO::sanitizePathComponent(texture.empty() ? std::string("texture") : texture);
    const std::string lower = String_ToLower(name);
    if (lower.size() < 4 || lower.compare(lower.size() - 4, 4, ".bmp") != 0)
        name += ".bmp";
    return name;
}

// a file name not taken yet among names, without the extension
std::string uniqueName(std::string wanted, std::set<std::string>& taken)
{
    wanted = FileIO::sanitizePathComponent(wanted.empty() ? std::string("part") : wanted);
    std::string name = wanted;
    for (int n = 2; !taken.insert(String_ToLower(name)).second; ++n)
        name = wanted + "_" + std::to_string(n);
    return name;
}

std::string nodesBlock(const StudioModel& model)
{
    std::string text = "version 1\nnodes\n";
    for (size_t i = 0; i < model.bones.size(); ++i)
        text += format("%d %s %d\n", static_cast<int>(i), inQuotes(model.bones[i].name).c_str(), model.bones[i].parent);
    return text + "end\n";
}

std::string boneLines(const std::vector<std::array<float, 6>>& values)
{
    std::string text;
    for (size_t i = 0; i < values.size(); ++i)
    {
        const std::array<float, 6>& v = values[i];
        text += format("%d %.6f %.6f %.6f %.6f %.6f %.6f\n", static_cast<int>(i), v[0], v[1], v[2], v[3], v[4], v[5]);
    }
    return text;
}

bool writeText(const fs::path& path, const std::string& text)
{
    std::ofstream out(path, std::ios::binary);
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
    return out.good();
}

} // namespace

DecompileResult DecompileStudioModel(const std::string& mdlPath, const std::string& outputDir)
{
    DecompileResult result;
    StudioModel model;
    if (!model.Load(mdlPath))
    {
        result.error = model.GetError();
        return result;
    }

    const fs::path input = FileIO::toPath(mdlPath);
    const std::string stem = FileIO::fromPath(input.stem());
    const fs::path folder = FileIO::toPath(outputDir) / FileIO::toPath(FileIO::sanitizePathComponent(stem));
    std::error_code ec;
    fs::create_directories(folder / "anims", ec);
    if (ec)
    {
        result.error = "Could not create " + FileIO::fromPath(folder);
        return result;
    }
    result.folder = FileIO::fromPath(folder);

    std::set<std::string> boneNames;
    for (size_t i = 0; i < model.bones.size(); ++i)
    {
        std::string& name = model.bones[i].name;
        if (!name.empty() && boneNames.insert(String_ToLower(name)).second)
            continue;
        const std::string old = name;
        for (int n = static_cast<int>(i); !boneNames.insert(String_ToLower(name = "bone" + std::to_string(n))).second; n += 1000) {}
        result.warnings.push_back("Bone " + std::to_string(i) + (old.empty() ? " has no name" : " is named " + old + " as another") +
                                  ", it is called " + name);
    }

    std::vector<std::string>& textureFiles = result.textureFiles;
    textureFiles.resize(model.textures.size());
    std::set<std::string> takenTextures;
    for (size_t i = 0; i < model.textures.size(); ++i)
    {
        const StudioTexture& texture = model.textures[i];
        std::string name = bmpName(texture.name);
        name = uniqueName(name.substr(0, name.size() - 4), takenTextures) + ".bmp";
        textureFiles[i] = name;
        if (texture.pixels.empty() || texture.palette.size() < 768 ||
            !BMP::writeIndexed8Rgb(FileIO::fromPath(folder / FileIO::toPath(name)), texture.width, texture.height,
                                   texture.palette.data(), texture.pixels.data()))
        {
            result.warnings.push_back("Texture " + texture.name + " could not be written");
            continue;
        }
        result.textures++;
    }

    const std::vector<aiMatrix4x4> rest = model.BoneTransforms(-1, 0);
    const std::string nodes = nodesBlock(model);
    const std::string restSkeleton = "skeleton\ntime 0\n" + boneLines(model.AnimationValues(-1, 0, 0)) + "end\n";
    std::set<std::string> takenFiles;
    std::vector<std::vector<std::string>>& bodyFiles = result.bodyFiles;
    bodyFiles.resize(model.bodyParts.size());
    for (size_t b = 0; b < model.bodyParts.size(); ++b)
    {
        const StudioBodyPart& part = model.bodyParts[b];
        for (size_t m = 0; m < part.models.size(); ++m)
        {
            const StudioSubModel& sub = part.models[m];
            if (sub.vertices.empty())
            {
                bodyFiles[b].push_back(std::string()); 
                continue;
            }
            std::string base = fs::path(FileIO::toPath(sub.name)).stem().string();
            if (base.empty())
                base = part.name + "_" + std::to_string(m);
            const std::string name = uniqueName(base, takenFiles);
            std::string text = nodes + restSkeleton + "triangles\n";
            for (const StudioMesh& mesh : sub.meshes)
            {
                const std::string texture = mesh.texture >= 0 ? textureFiles[mesh.texture] : std::string("notexture.bmp");
                for (size_t t = 0; t + 2 < mesh.triangles.size(); t += 3)
                {
                    text += texture + "\n";
                    for (size_t k = 0; k < 3; ++k)
                    {
                        const StudioTriVertex& corner = mesh.triangles[t + 2 - k];
                        const int bone = sub.vertexBones[corner.vertex];
                        aiVector3D local = sub.vertices[corner.vertex];
                        for (ai_real* axis : { &local.x, &local.y, &local.z })
                            *axis += std::copysign(0.002f, *axis);
                        const aiVector3D p = rest[bone] * local;
                        const aiVector3D n = (aiMatrix3x3(rest[sub.normalBones[corner.normal]]) * sub.normals[corner.normal]).NormalizeSafe();
                        text += format("%d %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n", bone, p.x, p.y, p.z, n.x, n.y, n.z,
                                       corner.u, 1.0f - corner.v);
                    }
                }
            }
            text += "end\n";
            if (!writeText(folder / FileIO::toPath(name + ".smd"), text))
            {
                result.error = "Could not write " + name + ".smd";
                return result;
            }
            bodyFiles[b].push_back(name);
            result.smdFiles++;
        }
    }

    std::vector<std::vector<std::string>> animationFiles(model.sequences.size());
    std::set<std::string> takenAnimations;
    for (size_t s = 0; s < model.sequences.size(); ++s)
    {
        const StudioSequence& sequence = model.sequences[s];
        const std::string base = uniqueName(sequence.name, takenAnimations);
        for (int blend = 0; blend < sequence.blends; ++blend)
        {
            const std::string name = sequence.blends > 1 ? base + "_blend" + std::to_string(blend + 1) : base;
            std::string text = nodes + "skeleton\n";
            const int frames = std::max(1, sequence.frames);
            for (int frame = 0; frame < frames; ++frame)
            {
                std::vector<std::array<float, 6>> values = model.AnimationValues(static_cast<int>(s), blend, frame);
                if (sequence.motionType & kLinearMotion)
                {
                    const float along = frames > 1 ? static_cast<float>(frame) / static_cast<float>(frames - 1) : 0.0f;
                    for (size_t i = 0; i < values.size(); ++i)
                    {
                        if (model.bones[i].parent >= 0)
                            continue;
                        values[i][0] += sequence.linearMovement.x * along;
                        values[i][1] += sequence.linearMovement.y * along;
                        values[i][2] += sequence.linearMovement.z * along;
                    }
                }
                text += format("time %d\n", frame) + boneLines(values);
            }
            text += "end\n";
            if (!writeText(folder / "anims" / FileIO::toPath(name + ".smd"), text))
            {
                result.error = "Could not write anims/" + name + ".smd";
                return result;
            }
            animationFiles[s].push_back("anims/" + name);
            result.smdFiles++;
        }
    }

    std::string qc = format("// decompiled by Vortigaunt from %s\n\n", FileIO::fromPath(input.filename()).c_str());
    qc += "$modelname " + inQuotes(stem + ".mdl") + "\n$cd \".\"\n$cdtexture \".\"\n$scale 1.0\n";
    qc += "$origin 0 0 0 -90\n";
    if (model.flags != 0)
        qc += format("$flags %d\n", model.flags);
    if (model.externalTextures)
        qc += "$externaltextures\n";
    qc += format("$eyeposition %.6f %.6f %.6f\n", model.eyePosition.x, model.eyePosition.y, model.eyePosition.z);
    if (model.boundsMin != model.boundsMax)
        qc += format("$bbox %.6f %.6f %.6f %.6f %.6f %.6f\n", model.boundsMin.x, model.boundsMin.y, model.boundsMin.z,
                     model.boundsMax.x, model.boundsMax.y, model.boundsMax.z);
    if (model.clipMin != model.clipMax)
        qc += format("$cbox %.6f %.6f %.6f %.6f %.6f %.6f\n", model.clipMin.x, model.clipMin.y, model.clipMin.z,
                     model.clipMax.x, model.clipMax.y, model.clipMax.z);
    qc += "\n";

    for (size_t b = 0; b < model.bodyParts.size(); ++b)
    {
        const std::vector<std::string>& files = bodyFiles[b];
        const std::string part = model.bodyParts[b].name.empty() ? "body" + std::to_string(b) : model.bodyParts[b].name;
        if (files.size() == 1 && !files.front().empty())
        {
            qc += "$body " + inQuotes(part) + " " + inQuotes(files.front()) + "\n";
            continue;
        }
        qc += "$bodygroup " + inQuotes(part) + "\n{\n";
        for (const std::string& file : files)
            qc += file.empty() ? "    blank\n" : "    studio " + inQuotes(file) + "\n";
        qc += "}\n";
    }
    qc += "\n";

    for (size_t i = 0; i < model.textures.size(); ++i)
    {
        const int flags = model.textures[i].flags;
        const std::string name = inQuotes(textureFiles[i]);
        if (flags & STUDIO_NF_MASKED) qc += "$texrendermode " + name + " masked\n";
        if (flags & STUDIO_NF_ADDITIVE) qc += "$texrendermode " + name + " additive\n";
        if ((flags & kFlatShade) && !(flags & STUDIO_NF_CHROME)) qc += "$texrendermode " + name + " flatshade\n";
        if (flags & kFullBright) qc += "$texrendermode " + name + " fullbright\n";
    }

    if (model.skinFamilies.size() > 1)
    {
        std::vector<size_t> changing;
        for (size_t r = 0; r < model.skinFamilies.front().size(); ++r)
        {
            for (const std::vector<int>& family : model.skinFamilies)
            {
                if (family[r] != model.skinFamilies.front()[r])
                {
                    changing.push_back(r);
                    break;
                }
            }
        }
        if (!changing.empty())
        {
            qc += "$texturegroup \"skinfamilies\"\n{\n";
            for (const std::vector<int>& family : model.skinFamilies)
            {
                qc += "    {";
                for (size_t r : changing)
                {
                    const int texture = family[r];
                    qc += " " + inQuotes(texture >= 0 && texture < static_cast<int>(textureFiles.size()) ? textureFiles[texture] : std::string());
                }
                qc += " }\n";
            }
            qc += "}\n";
        }
    }
    qc += "\n";

    for (size_t i = 0; i < model.attachments.size(); ++i)
    {
        const StudioAttachment& attachment = model.attachments[i];
        qc += format("$attachment %d %s %.6f %.6f %.6f\n", static_cast<int>(i), inQuotes(model.bones[attachment.bone].name).c_str(),
                     attachment.origin.x, attachment.origin.y, attachment.origin.z);
    }
    for (const StudioBoneController& controller : model.controllers)
    {
        const std::string index = controller.index == kMouthController ? std::string("mouth") : std::to_string(controller.index);
        qc += "$controller " + index + " " + inQuotes(model.bones[controller.bone].name) + " " +
              motionNames(controller.type & ~kRotatingController) + format(" %.6f %.6f\n", controller.start, controller.end);
    }
    for (const StudioHitbox& hitbox : model.hitboxes)
    {
        qc += format("$hbox %d %s %.6f %.6f %.6f %.6f %.6f %.6f\n", hitbox.group, inQuotes(model.bones[hitbox.bone].name).c_str(),
                     hitbox.min.x, hitbox.min.y, hitbox.min.z, hitbox.max.x, hitbox.max.y, hitbox.max.z);
    }
    qc += "\n";

    for (size_t s = 0; s < model.sequences.size(); ++s)
    {
        const StudioSequence& sequence = model.sequences[s];
        qc += "$sequence " + inQuotes(sequence.name);
        for (const std::string& file : animationFiles[s])
            qc += " " + inQuotes(file);
        if (sequence.blends > 1)
            qc += " blend " + motionNames(sequence.blendType) + format(" %g %g", sequence.blendStart, sequence.blendEnd);
        qc += format(" fps %g", sequence.fps);
        if (sequence.flags & kLooping)
            qc += " loop";
        if (sequence.activity > 0 && sequence.activity < static_cast<int>(std::size(kActivities)))
            qc += std::string(" ") + kActivities[sequence.activity] + format(" %d", sequence.activityWeight);
        else if (sequence.activity != 0)
            qc += format(" ACT_%d %d", sequence.activity, sequence.activityWeight);
        if (sequence.motionType)
            qc += " " + motionNames(sequence.motionType);
        if (sequence.entryNode != 0 && sequence.entryNode == sequence.exitNode)
            qc += format(" node %d", sequence.entryNode);
        else if (sequence.entryNode != sequence.exitNode)
            qc += format(" %s %d %d", sequence.nodeFlags ? "rtransition" : "transition", sequence.entryNode, sequence.exitNode);
        if (!sequence.events.empty())
        {
            qc += " {\n";
            for (const StudioEvent& event : sequence.events)
                qc += format("    { event %d %d ", event.event, event.frame) + inQuotes(event.options) + " }\n";
            qc += "}";
        }
        qc += "\n";
    }

    const fs::path qcPath = folder / FileIO::toPath(FileIO::sanitizePathComponent(stem) + ".qc");
    if (!writeText(qcPath, qc))
    {
        result.error = "Could not write the QC";
        return result;
    }
    result.qcPath = FileIO::fromPath(qcPath);
    result.ok = true;
    return result;
}
