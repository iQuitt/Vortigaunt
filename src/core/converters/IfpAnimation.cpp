// Copyright (C) 2026 iQuit (https://github.com/iQuitt)
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "IfpAnimation.h"

#include "core/autorig/SkeletonMath.h"
#include "utils/FileIO.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iomanip>

namespace
{

std::string trimmedLower(const char* text, size_t size)
{
    std::string name(text, strnlen(text, size));
    name.erase(0, name.find_first_not_of(' '));
    name.erase(name.find_last_not_of(' ') + 1);
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return name;
}

// the key at a time: rotations slerped, translations blended
IfpKey sample(const IfpBone& bone, float time)
{
    const std::vector<IfpKey>& keys = bone.keys;
    if (time <= keys.front().time)
        return keys.front();
    if (time >= keys.back().time)
        return keys.back();
    size_t next = 1;
    while (keys[next].time < time)
        ++next;
    const IfpKey& a = keys[next - 1];
    const IfpKey& b = keys[next];
    const float t = b.time > a.time ? (time - a.time) / (b.time - a.time) : 0.0f;
    IfpKey key = a;
    aiQuaternion::Interpolate(key.rotation, a.rotation, b.rotation, t);
    key.rotation.Normalize();
    key.translation = a.translation + (b.translation - a.translation) * t;
    return key;
}

} // namespace

std::vector<IfpAnimation> ReadIfp(const std::vector<uint8_t>& data, std::string* error)
{
    std::vector<IfpAnimation> animations;
    auto fail = [&](const char* why) {
        if (error)
            *error = why;
        return std::vector<IfpAnimation>();
    };
    if (data.size() < 36 || std::memcmp(data.data(), "ANP3", 4) != 0)
        return fail("not a San Andreas animation file (ANP3)");
    size_t p = 32;
    auto u32 = [&](size_t at) { uint32_t v; std::memcpy(&v, &data[at], 4); return v; };
    auto i16 = [&](size_t at) { int16_t v; std::memcpy(&v, &data[at], 2); return v; };
    const uint32_t count = u32(p);
    p += 4;
    for (uint32_t a = 0; a < count; ++a)
    {
        if (p + 36 > data.size())
            return fail("cut short");
        IfpAnimation animation;
        animation.name = std::string(reinterpret_cast<const char*>(&data[p]), strnlen(reinterpret_cast<const char*>(&data[p]), 24));
        const uint32_t objects = u32(p + 24);
        p += 36;
        for (uint32_t o = 0; o < objects; ++o)
        {
            if (p + 36 > data.size())
                return fail("cut short");
            IfpBone bone;
            bone.name = trimmedLower(reinterpret_cast<const char*>(&data[p]), 24);
            const uint32_t type = u32(p + 24), frames = u32(p + 28);
            bone.boneId = int(u32(p + 32));
            p += 36;
            const bool translated = type == 4 || type == 2;
            const size_t step = translated ? 16 : 10;
            if (p + size_t(frames) * step > data.size())
                return fail("cut short");
            for (uint32_t f = 0; f < frames; ++f, p += step)
            {
                IfpKey key;
                key.rotation = aiQuaternion(i16(p + 6) / 4096.0f, i16(p) / 4096.0f, i16(p + 2) / 4096.0f, i16(p + 4) / 4096.0f);
                key.rotation.Normalize();
                key.time = i16(p + 8) / 60.0f;
                if (translated)
                {
                    key.translation = aiVector3D(i16(p + 10), i16(p + 12), i16(p + 14)) / 1024.0f;
                    key.hasTranslation = true;
                }
                bone.keys.push_back(key);
            }
            if (!bone.keys.empty())
            {
                animation.duration = std::max(animation.duration, bone.keys.back().time);
                animation.bones.push_back(std::move(bone));
            }
        }
        animations.push_back(std::move(animation));
    }
    return animations;
}

bool WriteIfpSmd(const IfpAnimation& animation, const std::vector<SmdBone>& rest, const std::string& outPath, int fps, float scale)
{
    std::ofstream file(FileIO::toPath(outPath));
    if (!file)
        return false;
    // which animated bone drives each bone of the skeleton
    std::vector<const IfpBone*> driver(rest.size(), nullptr);
    for (size_t i = 0; i < rest.size(); ++i)
    {
        const std::string name = trimmedLower(rest[i].name.c_str(), rest[i].name.size());
        for (const IfpBone& bone : animation.bones)
            if (bone.name == name)
                driver[i] = &bone;
    }
    // the motions call the ped's root "Normal" some DFFs name it otherwise ("Root")
    for (size_t i = 0; i < rest.size(); ++i)
        if (!driver[i] && rest[i].parentIndex == 0)
            for (const IfpBone& bone : animation.bones)
                if (bone.name == "normal")
                    driver[i] = &bone;

    // GTA's peds are 3ds Max bipeds with the "Bip01 " dropped from most names; it goes back so the bones match others
    file << "version 1\nnodes\n";
    for (const SmdBone& bone : rest)
    {
        std::string name = bone.name;
        if (bone.parentIndex == 0)
            name = "Bip01";
        else if (bone.parentIndex > 0 && name.rfind("Bip01", 0) != 0)
            name = "Bip01 " + name;
        file << "  " << bone.index << " \"" << name << "\" " << bone.parentIndex << "\n";
    }
    file << "end\nskeleton\n" << std::fixed << std::setprecision(6);
    const int frames = std::max(1, int(std::ceil(animation.duration * fps)) + 1);
    for (int f = 0; f < frames; ++f)
    {
        file << "  time " << f << "\n";
        for (size_t i = 0; i < rest.size(); ++i)
        {
            const SmdBone& bone = rest[i];
            float px = bone.posX, py = bone.posY, pz = bone.posZ, rx = bone.rotX, ry = bone.rotY, rz = bone.rotZ;
            // the ped's root lies down in the DFF; a motion without it stands it up, facing the way GTA's peds face
            if (!driver[i] && bone.parentIndex == 0)
            {
                rx = ry = 0;
                rz = float(AI_MATH_HALF_PI);
            }
            if (driver[i] && bone.parentIndex >= 0)
            {
                const IfpKey key = sample(*driver[i], float(f) / fps);
                autorig::SmdEulerAngles(key.rotation.GetMatrix(), rx, ry, rz);
                if (key.hasTranslation)
                {
                    px = key.translation.x * scale;
                    py = key.translation.y * scale;
                    pz = key.translation.z * scale;
                }
            }
            file << "    " << i << " " << px << " " << py << " " << pz << " " << rx << " " << ry << " " << rz << "\n";
        }
    }
    file << "end\n";
    return bool(file);
}
