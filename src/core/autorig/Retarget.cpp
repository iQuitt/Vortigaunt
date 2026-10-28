// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "core/autorig/Retarget.h"

#include "core/autorig/SkeletonMath.h"
#include "core/converters/FbxGlbConverter.h"
#include "core/converters/Gr2Converter.h"
#include "core/mdl/StudioDecompiler.h"
#include "utils/FileIO.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <map>
#include <regex>
#include <sstream>

namespace fs = std::filesystem;

namespace
{

constexpr size_t kMostSequenceBytes = 60000;  // of studiomdl's 64K sequence bytes

// an SMD's skeleton and frames; a missing bone keeps the previous frame's value
struct Skeleton
{
    std::vector<std::string> names;
    std::vector<int> parents;
    std::vector<std::vector<std::array<float, 6>>> frames;
};

bool readSkeleton(const std::string& path, Skeleton& skeleton, std::string& error)
{
    std::ifstream in(FileIO::toPath(path));
    if (!in)
    {
        error = "Could not open " + path;
        return false;
    }
    std::string line;
    enum { None, Nodes, Frames } section = None;
    while (std::getline(in, line))
    {
        std::istringstream words(line);
        std::string first;
        if (!(words >> first)) continue;
        if (section == None)
        {
            if (first == "nodes") section = Nodes;
            else if (first == "skeleton") section = Frames;
            continue;
        }
        if (first == "end")
        {
            if (section == Frames) break;
            section = None;
            continue;
        }
        if (section == Nodes)
        {
            const size_t open = line.find('"'), close = line.rfind('"');
            if (open == std::string::npos || close <= open) continue;
            skeleton.names.push_back(line.substr(open + 1, close - open - 1));
            skeleton.parents.push_back(std::atoi(line.substr(close + 1).c_str()));
            continue;
        }
        if (first == "time")
        {
            skeleton.frames.push_back(skeleton.frames.empty() ? std::vector<std::array<float, 6>>(skeleton.names.size(), std::array<float, 6>{})
                                                               : skeleton.frames.back());
            continue;
        }
        if (skeleton.frames.empty()) continue;
        const int bone = std::atoi(first.c_str());
        std::array<float, 6> value{};
        for (float& v : value) words >> v;
        if (bone >= 0 && bone < static_cast<int>(skeleton.names.size())) skeleton.frames.back()[bone] = value;
    }
    if (skeleton.names.empty() || skeleton.frames.empty())
    {
        error = path + " has no skeleton";
        return false;
    }
    return true;
}

std::vector<SmdBone> bonesAt(const Skeleton& skeleton, size_t frame)
{
    std::vector<SmdBone> bones(skeleton.names.size());
    for (size_t i = 0; i < bones.size(); ++i)
    {
        const std::array<float, 6>& v = skeleton.frames[frame][i];
        bones[i] = { static_cast<int>(i), skeleton.names[i], skeleton.parents[i], v[0], v[1], v[2], v[3], v[4], v[5] };
    }
    return bones;
}

// the Biped root bone
int bipedRoot(const Skeleton& skeleton)
{
    for (size_t i = 0; i < skeleton.names.size(); ++i)
        if (autorig::CanonicalBoneName(skeleton.names[i]) == "bip01") return static_cast<int>(i);
    for (size_t i = 0; i < skeleton.names.size(); ++i)
        if (skeleton.parents[i] < 0) return static_cast<int>(i);
    return -1;
}

int boneNamed(const Skeleton& skeleton, const char* name)
{
    for (size_t i = 0; i < skeleton.names.size(); ++i)
        if (autorig::CanonicalBoneName(skeleton.names[i]) == name) return static_cast<int>(i);
    return -1;
}

int hipsOf(const Skeleton& skeleton)
{
    const int pelvis = boneNamed(skeleton, "bip01 pelvis");
    return pelvis >= 0 ? pelvis : bipedRoot(skeleton);
}

std::vector<int> spineOf(const Skeleton& skeleton)
{
    std::vector<int> chain;
    const int hips = hipsOf(skeleton);
    int at = boneNamed(skeleton, "bip01 neck");
    if (at < 0) at = boneNamed(skeleton, "bip01 head");
    if (hips < 0 || at < 0) return chain;
    std::vector<bool> belowHips(skeleton.names.size(), false);
    for (int up = hips; up >= 0 && !belowHips[up]; up = skeleton.parents[up]) belowHips[up] = true;
    for (at = skeleton.parents[at]; at >= 0 && !belowHips[at] && chain.size() < skeleton.names.size(); at = skeleton.parents[at])
        chain.insert(chain.begin(), at);
    if (at < 0) chain.clear();
    return chain;
}

bool isBiped(const Skeleton& skeleton)
{
    for (const std::string& name : skeleton.names)
        if (autorig::NormalizeBoneName(name) == "bip01") return true;
    return false;
}

aiVector3D positionOf(const aiMatrix4x4& m)
{
    return aiVector3D(m.a4, m.b4, m.c4);
}

float legLength(const Skeleton& skeleton, int root, const aiVector3D& up)
{
    const std::vector<aiMatrix4x4> world = autorig::ComputeWorldTransforms(bonesAt(skeleton, 0));
    aiVector3D ankles;
    int found = 0;
    for (const char* foot : { "bip01 l foot", "bip01 r foot" })
    {
        const int i = boneNamed(skeleton, foot);
        if (i < 0) continue;
        ankles += positionOf(world[i]);
        found++;
    }
    if (root < 0 || !found) return 0.0f;
    return (positionOf(world[root]) - ankles / static_cast<float>(found)) * up;
}

bool isMirrored(const Skeleton& skeleton, const std::vector<aiMatrix4x4>& world)
{
    const int left = boneNamed(skeleton, "bip01 l thigh"), right = boneNamed(skeleton, "bip01 r thigh");
    int foot = boneNamed(skeleton, "bip01 l foot"), toe = boneNamed(skeleton, "bip01 l toe0");
    if (foot < 0 || toe < 0)
    {
        foot = boneNamed(skeleton, "bip01 r foot");
        toe = boneNamed(skeleton, "bip01 r toe0");
    }
    if (left < 0 || right < 0 || foot < 0 || toe < 0) return false;
    const aiVector3D forward = positionOf(world[toe]) - positionOf(world[foot]), side = positionOf(world[left]) - positionOf(world[right]);
    return -forward.y * side.x + forward.x * side.y < 0.0f;
}

aiMatrix4x4 reflected(aiMatrix4x4 m)
{
    m.a2 = -m.a2;
    m.a3 = -m.a3;
    m.a4 = -m.a4;
    m.b1 = -m.b1;
    m.c1 = -m.c1;
    return m;
}

aiMatrix3x3 bodyFrame(const Skeleton& skeleton, const std::vector<aiMatrix4x4>& world)
{
    const int left = boneNamed(skeleton, "bip01 l thigh"), right = boneNamed(skeleton, "bip01 r thigh");
    if (left < 0 || right < 0) return aiMatrix3x3();
    aiVector3D x = positionOf(world[left]) - positionOf(world[right]);
    x.z = 0.0f;
    if (x.SquareLength() < 1e-8f) return aiMatrix3x3();
    x.Normalize();
    return aiMatrix3x3(x.x, -x.y, 0.0f, x.y, x.x, 0.0f, 0.0f, 0.0f, 1.0f);
}

std::string lowerExtension(const std::string& path)
{
    std::string ext = FileIO::fromPath(FileIO::toPath(path).extension());
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return ext;
}

bool isFbxGlb(const std::string& ext)
{
    return ext == ".fbx" || ext == ".glb" || ext == ".gltf";
}

FbxGlbConvertReport convertFbxGlb(const std::string& path, const fs::path& work)
{
    FbxGlbConvertOptions options;
    options.writeQC = false;
    options.exportTextures = false;
    options.maxBones = 0;
    FbxGlbConverter converter;
    return converter.Convert(path, FileIO::fromPath(work / "fbx"), options);
}

DecompileResult decompile(const std::string& path, const fs::path& work)
{
    return DecompileStudioModel(path, FileIO::fromPath(work / "mdl" / FileIO::toPath(path).stem()));
}

// SMD's euler angles of a rotation, R = Rz Ry Rx
void smdEuler(const aiMatrix3x3& m, float& rx, float& ry, float& rz)
{
    ry = std::asin(std::clamp(-m.c1, -1.0f, 1.0f));
    if (std::abs(m.c1) < 0.9999f)
    {
        rx = std::atan2(m.c2, m.c3);
        rz = std::atan2(m.b1, m.a1);
    }
    else
    {
        rx = std::atan2(-m.b3, m.b2);
        rz = 0.0f;
    }
}

std::string readQc(const std::string& qcPath)
{
    std::ifstream in(FileIO::toPath(qcPath), std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

void readOptions(const std::string& options, retarget::QcSequence& sequence)
{
    static const std::regex fps(R"re(\bfps\s+([0-9.]+))re"), activity(R"re(\b(ACT_\w+))re"), loop(R"re(\bloop\b)re");
    std::smatch m;
    if (std::regex_search(options, m, fps)) sequence.fps = std::max(1, static_cast<int>(std::stod(m[1].str()) + 0.5));
    if (std::regex_search(options, m, activity)) sequence.activity = m[1].str();
    sequence.loop = std::regex_search(options, loop);
}

}  // namespace

namespace retarget
{

RetargetResult Retarget(const std::string& sourceRest, const std::string& sourceMotion, const std::string& targetRest, const std::string& outPath, int fps)
{
    RetargetResult result;
    Skeleton rest, motion, target;
    if (!readSkeleton(sourceRest, rest, result.error) || !readSkeleton(sourceMotion, motion, result.error) ||
        !readSkeleton(targetRest, target, result.error))
        return result;

    // target bone -> motion bone
    std::map<std::string, int> motionBone;
    for (size_t i = 0; i < motion.names.size(); ++i) motionBone.emplace(autorig::CanonicalBoneName(motion.names[i]), static_cast<int>(i));
    std::vector<int> from(target.names.size(), -1);
    for (size_t i = 0; i < target.names.size(); ++i)
    {
        const auto found = motionBone.find(autorig::CanonicalBoneName(target.names[i]));
        if (found != motionBone.end()) from[i] = found->second;
    }

    for (size_t i = 0; i < target.names.size(); ++i)
    {
        const int parent = target.parents[i];
        if (from[i] < 0 || parent < 0 || autorig::CanonicalBoneName(target.names[i]).rfind("bip01", 0) == 0) continue;
        bool under = false;
        if (parent < static_cast<int>(i) && from[parent] >= 0)
            for (int up = motion.parents[from[i]], guard = 0; up >= 0 && !under && guard < static_cast<int>(motion.names.size()); up = motion.parents[up], ++guard)
                under = up == from[parent];
        if (!under) from[i] = -1;
    }

    const bool biped = isBiped(motion) && isBiped(target);

    if (!biped)
    {
        const std::vector<int> want = spineOf(target), have = spineOf(motion);
        for (size_t k = 0; k < want.size() && !have.empty(); ++k)
            from[want[k]] = have[want.size() > 1 ? (2 * k * (have.size() - 1) + want.size() - 1) / (2 * (want.size() - 1)) : have.size() - 1];
    }
    for (size_t i = 0; i < target.names.size(); ++i)
        if (from[i] < 0) result.unmatched.push_back(target.names[i]);

    const size_t bones = target.names.size();
    const std::vector<aiMatrix4x4> targetWorld = autorig::ComputeWorldTransforms(bonesAt(target, 0));

    std::vector<aiMatrix4x4> sourceWorld = autorig::ComputeWorldTransforms(bonesAt(motion, 0));
    {
        const std::vector<aiMatrix4x4> restWorld = autorig::ComputeWorldTransforms(bonesAt(rest, 0));
        std::map<std::string, int> restBone;
        for (size_t i = 0; i < rest.names.size(); ++i) restBone.emplace(autorig::CanonicalBoneName(rest.names[i]), static_cast<int>(i));
        for (size_t i = 0; i < motion.names.size(); ++i)
        {
            const auto found = restBone.find(autorig::CanonicalBoneName(motion.names[i]));
            if (found != restBone.end()) sourceWorld[i] = restWorld[found->second];
        }
    }

    const bool mirror = !biped && isMirrored(motion, sourceWorld) && !isMirrored(target, targetWorld);
    if (mirror)
        for (aiMatrix4x4& m : sourceWorld) m = reflected(m);
    const aiMatrix3x3 facing = biped ? aiMatrix3x3() : bodyFrame(target, targetWorld) * bodyFrame(motion, sourceWorld).Transpose();
    const int sourceHips = biped ? bipedRoot(motion) : hipsOf(motion);
    const float sourceLegs = legLength(rest, biped ? bipedRoot(rest) : hipsOf(rest), aiMatrix3x3(facing).Transpose() * aiVector3D(0.0f, 0.0f, 1.0f));
    const float targetLegs = legLength(target, biped ? bipedRoot(target) : hipsOf(target), aiVector3D(0.0f, 0.0f, 1.0f));
    if (sourceLegs <= 0.0f || targetLegs <= 0.0f)
    {
        result.error = "The feet of the skeletons were not found, to size the motion by";
        return result;
    }
    result.scale = targetLegs / sourceLegs;

    std::vector<aiMatrix3x3> offset(bones);
    if (!biped)
    {
        std::vector<aiMatrix3x3> align(bones);
        for (size_t i = 0; i < bones; ++i)
        {
            const int parent = target.parents[i];
            if (parent >= 0 && parent < static_cast<int>(i)) align[i] = align[parent];
            // the one matched child, or the one up the trunk
            int child = -1, children = 0, trunk = -1;
            for (size_t c = i + 1; c < bones; ++c)
                if (target.parents[c] == static_cast<int>(i) && from[c] >= 0)
                {
                    child = static_cast<int>(c);
                    children++;
                    const std::string name = autorig::CanonicalBoneName(target.names[c]);
                    if (name.rfind("bip01 spine", 0) == 0 || name == "bip01 neck" || name == "bip01 head") trunk = child;
                }
            if (trunk >= 0)
            {
                child = trunk;
                children = 1;
            }
            if (from[i] >= 0 && children == 1)
            {
                aiVector3D want = positionOf(targetWorld[child]) - positionOf(targetWorld[i]);
                aiVector3D have = facing * (positionOf(sourceWorld[from[child]]) - positionOf(sourceWorld[from[i]]));
                if (want.SquareLength() > 1e-8f && have.SquareLength() > 1e-8f)
                    aiMatrix3x3::FromToMatrix(want.Normalize(), have.Normalize(), align[i]);
            }
            if (from[i] >= 0) offset[i] = (facing * aiMatrix3x3(sourceWorld[from[i]])).Transpose() * align[i] * aiMatrix3x3(targetWorld[i]);
        }
    }

    // world rotations, so an extra bone in a chain does not add up
    std::vector<std::vector<std::array<float, 6>>> frames(motion.frames.size(), target.frames[0]);
    std::vector<aiMatrix3x3> turned(bones);
    for (size_t frame = 0; frame < frames.size(); ++frame)
    {
        std::vector<aiMatrix4x4> world = autorig::ComputeWorldTransforms(bonesAt(motion, frame));
        if (mirror)
            for (aiMatrix4x4& m : world) m = reflected(m);
        for (size_t i = 0; i < bones; ++i)
        {
            std::array<float, 6>& value = frames[frame][i];
            const int parent = target.parents[i];
            const aiMatrix3x3 local(autorig::SmdEulerRotation(value[3], value[4], value[5]));
            turned[i] = from[i] >= 0 ? facing * aiMatrix3x3(world[from[i]]) * offset[i] : parent >= 0 ? turned[parent] * local : local;
            smdEuler(parent >= 0 ? aiMatrix3x3(turned[parent]).Transpose() * turned[i] : turned[i], value[3], value[4], value[5]);
            if (parent >= 0 || sourceHips < 0) continue;
            // root movement at the target's size
            aiVector3D at = positionOf(world[sourceHips]) * result.scale;
            if (!biped) at = positionOf(targetWorld[i]) + facing * (positionOf(world[sourceHips]) - positionOf(sourceWorld[sourceHips])) * result.scale;
            value[0] = at.x;
            value[1] = at.y;
            value[2] = at.z;
        }
    }

    // studiomdl's 64K sequence limit: long motions skip frames
    size_t moving = 0;
    for (size_t i = 0; i < bones; ++i)
        for (int k = 0; k < 6; ++k)
            for (size_t frame = 1; frame < frames.size(); ++frame)
                if (std::abs(frames[frame][i][k] - frames[0][i][k]) > 1e-4f)
                {
                    moving++;
                    break;
                }
    size_t step = 1;
    fps = std::max(fps, 1);
    while (step < static_cast<size_t>(fps) && (fps % step || moving * ((frames.size() + step - 1) / step) * 2 + bones * 12 > kMostSequenceBytes)) step++;
    result.fps = static_cast<int>(fps / step);

    std::string smd = "version 1\nnodes\n";
    char text[256];
    for (size_t i = 0; i < bones; ++i)
    {
        std::snprintf(text, sizeof(text), "  %zu \"%s\" %d\n", i, target.names[i].c_str(), target.parents[i]);
        smd += text;
    }
    smd += "end\nskeleton\n";
    for (size_t frame = 0; frame < frames.size(); frame += step)
    {
        std::snprintf(text, sizeof(text), "  time %zu\n", frame / step);
        smd += text;
        for (size_t i = 0; i < bones; ++i)
        {
            const std::array<float, 6>& v = frames[frame][i];
            std::snprintf(text, sizeof(text), "%zu %f %f %f %f %f %f\n", i, v[0], v[1], v[2], v[3], v[4], v[5]);
            smd += text;
        }
    }
    smd += "end\n";
    std::ofstream out(FileIO::toPath(outPath), std::ios::binary | std::ios::trunc);
    out << smd;
    if (!out.good())
    {
        result.error = "Could not write " + outPath;
        return result;
    }
    result.frames = static_cast<int>((frames.size() + step - 1) / step);
    result.ok = true;
    return result;
}

std::string SourceSkeleton(const std::string& path, const std::string& workFolder, std::string& error)
{
    const std::string ext = lowerExtension(path);
    const fs::path work = FileIO::toPath(workFolder), file = FileIO::toPath(path);
    std::error_code ec;
    fs::create_directories(work, ec);
    if (ext == ".smd") return path;
    if (ext == ".gr2")
    {
        const fs::path rest = work / FileIO::toPath(FileIO::fromPath(file.stem()) + "_rest.smd");
        Gr2Converter converter;
        Gr2ConverterSettings settings;
        settings.ExportMaterials = false;
        converter.SetConverterSettings(settings);
        if (converter.ConvertSingleGr2File(path, FileIO::fromPath(rest)) == GR2_CONVERT_RET_OK && fs::exists(rest, ec)) return FileIO::fromPath(rest);
    }
    else if (isFbxGlb(ext))
    {
        const FbxGlbConvertReport report = convertFbxGlb(path, work);
        if (report.ok) return report.referenceSmd.empty() ? report.skeletonSmd : report.referenceSmd;
    }
    else if (ext == ".mdl")
    {
        const DecompileResult decompiled = decompile(path, work);
        const std::string body = decompiled.ok ? QcBody(decompiled.qcPath) : std::string();
        if (!body.empty() && fs::exists(FileIO::toPath(body), ec)) return body;
    }
    else
    {
        error = FileIO::fromPath(file.filename()) + " is not a .smd, .gr2, .fbx, .glb or .mdl";
        return {};
    }
    error = "Could not read the skeleton of " + FileIO::fromPath(file.filename());
    return {};
}

std::vector<QcSequence> SourceMotions(const std::string& path, const std::string& character, const std::string& workFolder, std::string& error)
{
    std::vector<QcSequence> motions;
    const std::string ext = lowerExtension(path);
    const fs::path work = FileIO::toPath(workFolder), file = FileIO::toPath(path);
    const std::string stem = FileIO::fromPath(file.stem());
    std::error_code ec;
    fs::create_directories(work, ec);
    QcSequence motion;
    motion.name = stem;
    if (ext == ".smd")
    {
        motion.path = path;
        motions.push_back(motion);
    }
    else if (ext == ".gr2")
    {
        if (lowerExtension(character) != ".gr2")
        {
            error = "A .gr2 motion needs a .gr2 character";
            return {};
        }
        const fs::path moved = work / FileIO::toPath(stem + "_motion.smd");
        Gr2Converter converter;
        if (converter.ConvertGr2Animation(path, FileIO::fromPath(moved), 0, character) == GR2_CONVERT_RET_OK && fs::exists(moved, ec))
        {
            motion.path = FileIO::fromPath(moved);
            motions.push_back(motion);
        }
    }
    else if (isFbxGlb(ext))
    {
        const FbxGlbConvertReport report = convertFbxGlb(path, work);
        for (const FbxGlbAnimationInfo& animation : report.animations)
        {
            // a single animation is named after its file
            motion.name = report.animations.size() == 1 ? stem : animation.name;
            motion.path = FileIO::fromPath(FileIO::toPath(report.outputFolder) / FileIO::toPath(animation.file + ".smd"));
            motions.push_back(motion);
        }
    }
    else if (ext == ".mdl")
    {
        const DecompileResult decompiled = decompile(path, work);
        if (decompiled.ok)
            for (QcSequence sequence : QcSequences(decompiled.qcPath))
            {
                if (lowerExtension(sequence.path) != ".smd") sequence.path += ".smd";
                sequence.path = FileIO::fromPath(FileIO::toPath(decompiled.qcPath).parent_path() / FileIO::toPath(sequence.path));
                motions.push_back(sequence);
            }
    }
    else
    {
        error = FileIO::fromPath(file.filename()) + " is not a .smd, .gr2, .fbx, .glb or .mdl";
        return {};
    }
    if (motions.empty()) error = "No motion was found in " + FileIO::fromPath(file.filename());
    return motions;
}

bool AddSequence(const std::string& qcPath, const std::string& name, const std::string& animPath, int fps, bool loop, std::string& error, const std::string& activity, int weight)
{
    std::ifstream in(FileIO::toPath(qcPath), std::ios::binary);
    if (!in)
    {
        error = "Could not open " + qcPath;
        return false;
    }
    std::string qc((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    in.close();
    std::string block = "$sequence \"" + name + "\" {\n\t\"" + animPath + "\"\n\tfps " + std::to_string(fps) + "\n";
    if (loop) block += "\tloop\n";
    if (!activity.empty()) block += "\t" + activity + " " + std::to_string(std::max(weight, 1)) + "\n";
    block += "}\n";

    // replace an existing sequence of that name
    const std::string head = "$sequence \"" + name + "\"";
    const size_t at = qc.find(head);
    if (at != std::string::npos)
    {
        const size_t open = qc.find('{', at), next = qc.find('\n', at);
        size_t end = next == std::string::npos ? qc.size() : next + 1;
        if (open != std::string::npos && (next == std::string::npos || open < next))
        {
            const size_t close = qc.find('}', open);
            end = close == std::string::npos ? qc.size() : std::min(qc.size(), qc.find('\n', close) == std::string::npos ? qc.size() : qc.find('\n', close) + 1);
        }
        qc.replace(at, end - at, block);
    }
    else
    {
        if (!qc.empty() && qc.back() != '\n') qc += "\n";
        qc += block;
    }
    std::ofstream out(FileIO::toPath(qcPath), std::ios::binary | std::ios::trunc);
    out << qc;
    if (!out.good())
    {
        error = "Could not write " + qcPath;
        return false;
    }
    return true;
}

std::string QcBody(const std::string& qcPath)
{
    const std::string qc = readQc(qcPath);
    static const std::regex studio(R"re(studio\s+"([^"]+)")re"), body(R"re(\$body\s+"[^"]*"\s+"([^"]+)")re");
    // first $body or $bodygroup studio
    std::smatch a, b;
    const bool inGroup = std::regex_search(qc, a, studio), alone = std::regex_search(qc, b, body);
    std::string file;
    if (alone && (!inGroup || b.position(0) < a.position(0))) file = b[1].str();
    else if (inGroup) file = a[1].str();
    if (file.empty()) return {};
    if (file.size() < 4 || file.compare(file.size() - 4, 4, ".smd") != 0) file += ".smd";
    return FileIO::fromPath(FileIO::toPath(qcPath).parent_path() / FileIO::toPath(file));
}

std::vector<QcSequence> QcSequences(const std::string& qcPath)
{
    std::vector<QcSequence> sequences;
    const std::string qc = readQc(qcPath);
    static const std::regex block(R"re(\$sequence\s+"([^"]+)"\s*\{([^}]*)\})re"), line(R"re(\$sequence\s+"([^"]+)"\s+"([^"]+)"([^\r\n]*))re"),
        quoted(R"re("([^"]+)")re");
    for (auto it = std::sregex_iterator(qc.begin(), qc.end(), block); it != std::sregex_iterator(); ++it)
    {
        QcSequence sequence;
        sequence.name = (*it)[1].str();
        const std::string inside = (*it)[2].str();
        std::smatch file;
        if (!std::regex_search(inside, file, quoted)) continue;
        sequence.path = file[1].str();
        readOptions(inside, sequence);
        sequences.push_back(sequence);
    }
    for (auto it = std::sregex_iterator(qc.begin(), qc.end(), line); it != std::sregex_iterator(); ++it)
    {
        QcSequence sequence;
        sequence.name = (*it)[1].str();
        sequence.path = (*it)[2].str();
        readOptions((*it)[3].str(), sequence);
        sequences.push_back(sequence);
    }
    for (QcSequence& sequence : sequences) std::replace(sequence.path.begin(), sequence.path.end(), '\\', '/');
    return sequences;
}

}  // namespace retarget
