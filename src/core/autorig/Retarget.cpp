#include "core/autorig/Retarget.h"

#include "core/autorig/SkeletonMath.h"
#include "core/converters/Gr2Converter.h"
#include "utils/FileIO.h"

#include <algorithm>
#include <array>
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

// leg length in the rest pose
float legLength(const Skeleton& skeleton)
{
    const std::vector<aiMatrix4x4> world = autorig::ComputeWorldTransforms(bonesAt(skeleton, 0));
    const int root = bipedRoot(skeleton);
    float ankles = 0.0f;
    int found = 0;
    for (size_t i = 0; i < skeleton.names.size(); ++i)
    {
        const std::string name = autorig::CanonicalBoneName(skeleton.names[i]);
        if (name == "bip01 l foot" || name == "bip01 r foot")
        {
            ankles += world[i].c4;
            found++;
        }
    }
    if (root < 0 || !found) return 0.0f;
    return world[root].c4 - ankles / found;
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

RetargetResult Retarget(const std::string& sourceRest, const std::string& sourceMotion, const std::string& targetRest, const std::string& outPath)
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
        else result.unmatched.push_back(target.names[i]);
    }

    const float sourceLegs = legLength(rest), targetLegs = legLength(target);
    if (sourceLegs <= 0.0f || targetLegs <= 0.0f)
    {
        result.error = "The feet of the skeletons were not found, to size the motion by";
        return result;
    }
    result.scale = targetLegs / sourceLegs;

    // world rotations, so an extra bone in a chain does not add up
    const int sourceRoot = bipedRoot(motion);
    const size_t bones = target.names.size();
    std::vector<std::vector<std::array<float, 6>>> frames(motion.frames.size(), target.frames[0]);
    std::vector<aiMatrix3x3> turned(bones);
    for (size_t frame = 0; frame < frames.size(); ++frame)
    {
        const std::vector<aiMatrix4x4> world = autorig::ComputeWorldTransforms(bonesAt(motion, frame));
        for (size_t i = 0; i < bones; ++i)
        {
            std::array<float, 6>& value = frames[frame][i];
            const int parent = target.parents[i];
            const aiMatrix3x3 local(autorig::SmdEulerRotation(value[3], value[4], value[5]));
            turned[i] = from[i] >= 0 ? aiMatrix3x3(world[from[i]]) : parent >= 0 ? turned[parent] * local : local;
            smdEuler(parent >= 0 ? aiMatrix3x3(turned[parent]).Transpose() * turned[i] : turned[i], value[3], value[4], value[5]);
            // root movement at the target's size
            if (parent < 0 && sourceRoot >= 0)
            {
                const aiMatrix4x4& root = world[sourceRoot];
                value[0] = root.a4 * result.scale;
                value[1] = root.b4 * result.scale;
                value[2] = root.c4 * result.scale;
            }
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
    while (step < 30 && (30 % step || moving * ((frames.size() + step - 1) / step) * 2 + bones * 12 > kMostSequenceBytes)) step++;
    result.fps = static_cast<int>(30 / step);

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

RetargetResult RetargetGr2Motion(const std::string& characterGr2, const std::string& motionGr2, const std::string& targetRest, const std::string& outPath, const std::string& workFolder)
{
    RetargetResult result;
    std::error_code ec;
    const fs::path work = FileIO::toPath(workFolder);
    fs::create_directories(work, ec);
    const fs::path character = FileIO::toPath(characterGr2), motion = FileIO::toPath(motionGr2);
    const fs::path rest = work / FileIO::toPath(FileIO::fromPath(character.stem()) + "_rest.smd");
    const fs::path moved = work / FileIO::toPath(FileIO::fromPath(motion.stem()) + "_motion.smd");
    if (!fs::exists(rest, ec))
    {
        Gr2Converter converter;
        Gr2ConverterSettings settings;
        settings.ExportMaterials = false;
        converter.SetConverterSettings(settings);
        if (converter.ConvertSingleGr2File(characterGr2, FileIO::fromPath(rest)) != GR2_CONVERT_RET_OK || !fs::exists(rest, ec))
        {
            result.error = "Could not read the skeleton of " + FileIO::fromPath(character.filename());
            return result;
        }
    }
    Gr2Converter converter;
    if (converter.ConvertGr2Animation(motionGr2, FileIO::fromPath(moved), 0, characterGr2) != GR2_CONVERT_RET_OK || !fs::exists(moved, ec))
    {
        result.error = "Could not read the motion " + FileIO::fromPath(motion.filename());
        return result;
    }
    return Retarget(FileIO::fromPath(rest), FileIO::fromPath(moved), targetRest, outPath);
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
