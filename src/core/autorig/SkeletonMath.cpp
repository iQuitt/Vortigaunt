#include "SkeletonMath.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_map>

namespace autorig
{

aiMatrix4x4 SmdEulerRotation(float rx, float ry, float rz)
{
    const float cx = std::cos(rx), sx = std::sin(rx);
    const float cy = std::cos(ry), sy = std::sin(ry);
    const float cz = std::cos(rz), sz = std::sin(rz);

    // aiMatrix4x4 is row major, translation lives in a4, b4, c4.
    aiMatrix4x4 mat;
    mat.a1 = cy * cz;
    mat.a2 = cz * sx * sy - cx * sz;
    mat.a3 = cx * cz * sy + sx * sz;
    mat.a4 = 0.0f;

    mat.b1 = cy * sz;
    mat.b2 = cx * cz + sx * sy * sz;
    mat.b3 = -cz * sx + cx * sy * sz;
    mat.b4 = 0.0f;

    mat.c1 = -sy;
    mat.c2 = cy * sx;
    mat.c3 = cx * cy;
    mat.c4 = 0.0f;

    mat.d1 = 0.0f;
    mat.d2 = 0.0f;
    mat.d3 = 0.0f;
    mat.d4 = 1.0f;

    return mat;
}

std::vector<aiMatrix4x4> ComputeWorldTransforms(const std::vector<SmdBone>& bones)
{
    std::vector<aiMatrix4x4> world(bones.size());
    std::vector<bool> done(bones.size(), false);

    auto resolve = [&](auto& self, int index) -> void {
        if (done[index])
        {
            return;
        }

        const SmdBone& bone = bones[index];
        aiMatrix4x4 translation;
        translation.a4 = bone.posX;
        translation.b4 = bone.posY;
        translation.c4 = bone.posZ;

        const aiMatrix4x4 local = translation * SmdEulerRotation(bone.rotX, bone.rotY, bone.rotZ);

        const int parent = bone.parentIndex;
        if (parent < 0 || parent >= static_cast<int>(bones.size()) || parent == index)
        {
            world[index] = local;
        }
        else
        {
            self(self, parent);
            world[index] = world[parent] * local;
        }
        done[index] = true;
    };

    for (size_t i = 0; i < bones.size(); i++)
    {
        resolve(resolve, static_cast<int>(i));
    }
    return world;
}

std::string NormalizeBoneName(const std::string& name)
{
    std::string out;
    out.reserve(name.size());

    for (char raw : name)
    {
        char c = static_cast<char>(std::tolower(static_cast<unsigned char>(raw)));
        if (c == '_' || c == '\t')
        {
            c = ' ';
        }
        if (c == ' ' && (out.empty() || out.back() == ' '))
        {
            continue;  // collapse runs, drop leading
        }
        out.push_back(c);
    }
    while (!out.empty() && out.back() == ' ')
    {
        out.pop_back();
    }

    // Source models prefix every bone with the rig's name.
    const std::string prefix = "valvebiped.";
    if (out.rfind(prefix, 0) == 0)
    {
        out = out.substr(prefix.size());
    }
    return out;
}

std::string CanonicalBoneName(const std::string& name)
{
    std::string normalized = NormalizeBoneName(name);

    static const std::unordered_map<std::string, std::string> kAliases = {
        { "bip01 l arm", "bip01 l clavicle" },
        { "bip01 r arm", "bip01 r clavicle" },
        { "bip01 l arm1", "bip01 l upperarm" },
        { "bip01 r arm1", "bip01 r upperarm" },
        { "bip01 l arm2", "bip01 l forearm" },
        { "bip01 r arm2", "bip01 r forearm" },
        { "bip01 l leg", "bip01 l thigh" },
        { "bip01 r leg", "bip01 r thigh" },
        { "bip01 l leg1", "bip01 l calf" },
        { "bip01 r leg1", "bip01 r calf" },
        { "bip01 l toe", "bip01 l toe0" },
        { "bip01 r toe", "bip01 r toe0" },
    };

    auto found = kAliases.find(normalized);
    return (found == kAliases.end()) ? normalized : found->second;
}

std::vector<int> MatchBonesByName(const std::vector<SmdBone>& from, const std::vector<SmdBone>& to)
{
    std::unordered_map<std::string, int> byName;
    for (size_t i = 0; i < to.size(); i++)
    {
        byName.emplace(CanonicalBoneName(to[i].name), static_cast<int>(i));
    }

    std::vector<int> mapping(from.size(), -1);
    for (size_t i = 0; i < from.size(); i++)
    {
        auto found = byName.find(CanonicalBoneName(from[i].name));
        if (found != byName.end())
        {
            mapping[i] = found->second;
        }
    }
    return mapping;
}

std::vector<int> MatchBonesByNameWithFallback(const std::vector<SmdBone>& from,
                                              const std::vector<SmdBone>& to,
                                              std::vector<std::string>* outUnmatched)
{
    std::vector<int> direct = MatchBonesByName(from, to);
    std::vector<int> mapping = direct;

    for (size_t i = 0; i < from.size(); i++)
    {
        if (direct[i] >= 0)
        {
            continue;
        }
        if (outUnmatched)
        {
            outUnmatched->push_back(from[i].name);
        }

        int cursor = from[i].parentIndex;
        int guard = 0;
        while (cursor >= 0 && cursor < static_cast<int>(from.size()) && guard < 256)
        {
            if (direct[cursor] >= 0)
            {
                mapping[i] = direct[cursor];
                break;
            }
            const int parent = from[cursor].parentIndex;
            if (parent == cursor)
            {
                break;
            }
            cursor = parent;
            guard++;
        }
    }
    return mapping;
}

BoneMatch MatchBonesForPosing(const std::vector<SmdBone>& from,
                              const std::vector<SmdBone>& to,
                              float scale)
{
    BoneMatch match;
    match.target = MatchBonesByNameWithFallback(from, to, &match.unmatched);
    match.frame.assign(from.size(), -1);
    match.axialScale.assign(from.size(), 1.0f);

    const std::vector<int> direct = MatchBonesByName(from, to);

    // The frame to measure in: the bone itself when it matched, otherwise the
    // ancestor it fell back to.
    for (size_t i = 0; i < from.size(); i++)
    {
        if (direct[i] >= 0)
        {
            match.frame[i] = static_cast<int>(i);
            continue;
        }
        int cursor = from[i].parentIndex;
        int guard = 0;
        while (cursor >= 0 && cursor < static_cast<int>(from.size()) && guard < 256)
        {
            if (direct[cursor] >= 0)
            {
                match.frame[i] = cursor;
                break;
            }
            const int parent = from[cursor].parentIndex;
            if (parent == cursor)
            {
                break;
            }
            cursor = parent;
            guard++;
        }
    }

    if (scale <= 0.0f)
    {
        scale = 1.0f;
    }

    const std::vector<aiMatrix4x4> fromWorld = ComputeWorldTransforms(from);
    const std::vector<aiMatrix4x4> toWorld = ComputeWorldTransforms(to);

    auto origin = [](const aiMatrix4x4& m) { return aiVector3D(m.a4, m.b4, m.c4); };

    std::vector<std::vector<int>> fromChildren(from.size());
    for (size_t i = 0; i < from.size(); i++)
    {
        const int parent = from[i].parentIndex;
        if (parent >= 0 && parent < static_cast<int>(from.size()) && parent != static_cast<int>(i))
        {
            fromChildren[parent].push_back(static_cast<int>(i));
        }
    }

    for (size_t i = 0; i < from.size(); i++)
    {
        if (direct[i] < 0)
        {
            continue;  // nothing of its own to line up
        }
        const int targetBone = direct[i];

        // Only a child that is the same joint on both sides says anything about
        // this bone's length. A helper hanging off the head does not, and taking
        // it would stretch the head to a helper's length.
        //
        // And only when there is exactly ONE such child: a bone that branches
        // cannot line all its children up at once, and picking one of them
        // arbitrarily is how the neck came out squashed to a third of its
        // length - its first matched child is a collarbone, not the head.
        int matchedChild = -1;
        int matchedCount = 0;
        for (int child : fromChildren[i])
        {
            const int targetChild = direct[child];
            if (targetChild < 0 || to[targetChild].parentIndex != targetBone)
            {
                continue;
            }
            matchedChild = child;
            matchedCount++;
        }

        if (matchedCount == 1)
        {
            const int targetChild = direct[matchedChild];
            const float sourceLength = (origin(fromWorld[matchedChild]) - origin(fromWorld[i])).Length() * scale;
            const float targetLength = (origin(toWorld[targetChild]) - origin(toWorld[targetBone])).Length();
            if (sourceLength > 1e-3f && targetLength > 1e-3f)
            {
                match.axialScale[i] = std::clamp(targetLength / sourceLength, 0.2f, 5.0f);
            }
        }
    }

    return match;
}

}  // namespace autorig
