// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "WeaponModelConverter.h"

#include "core/QCFile.h"
#include "core/VortigauntLog.h"
#include "core/mdl/StudioModel.h"
#include "utils/Bmp.h"
#include "utils/FileIO.h"
#include "utils/util.hpp"

#include <assimp/matrix3x3.h>
#include <assimp/matrix4x4.h>
#include <assimp/types.h>
#include <assimp/vector3.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <vector>

namespace fs = std::filesystem;

namespace {

struct SkeletonBone
{
    const char* name;
    int parent;
    float position[3];
    float rotation[3];
};

struct Corner
{
    aiVector3D position;
    aiVector3D normal;
    float u = 0.0f, v = 0.0f;
    int bone = 0;
};

struct Triangle
{
    Corner corner[3];
    int texture = -1;
    int source = 0;  // source sub-model
    int hand = 0;  // 0 right, 1 left
};

struct HoldPose
{
    aiVector3D forward;
    aiVector3D side;
    aiVector3D palm;
};

struct Arm
{
    std::vector<aiVector3D> points;
    std::vector<int> pointBones;
    std::set<int> bones;
    aiVector3D contact;      // where it touches the weapon
    float gap = 1e30f;
    float handMeasure = 0;  // 0 if no hand bone was found
    std::string handBone;
    int handBoneIndex = -1;
};

struct GameDirProfile
{
    const char* name;
    const SkeletonBone* skeleton;
    const SkeletonBone* leftArm;
    HoldPose gun, knife, grenade, pack;
    float handMeasure;
};

//  for the left hand's weapon
struct OtherHand
{
    // in
    float scale = 1.0f;
    bool mirror = false;
    WeaponType kind = WeaponType::Gun;
    // out
    std::vector<Triangle> triangles;  // in the weapon's own frame
    aiMatrix4x4 inHand;
    aiVector3D lo, hi;
    aiVector3D muzzle;  // in hand space
    bool hasMuzzle = false;
};

// what the skeleton says about hands and fingers, per bone
struct HandBones
{
    std::vector<bool> handByShape;
    std::vector<bool> armBone;
    std::vector<bool> fingerByName;
    std::vector<int> fingerNumber;
};

// the skin of the hands in the idle pose, with the bone each point follows
struct HandSkin
{
    std::vector<aiVector3D> points;
    std::vector<int> bones;
};


const SkeletonBone kPlayerArm[] = {
    { "Bip01",            -1, { 0.233849f, -2.251618f, 38.192150f }, { 0.000000f, 0.000000f, -1.570795f } },
    { "Bip01 Pelvis",      0, { -2.276864f, 0.000003f, -1.238186f }, { -1.570795f, -1.570451f, 0.000000f } },
    { "Bip01 Spine",       1, { 1.797145f, 0.711796f, -0.000002f },  { -0.000004f, -0.000001f, 0.000739f } },
    { "Bip01 Spine1",      2, { 4.118605f, -0.003279f, 0.000000f },  { 0.000000f, 0.000000f, 0.000035f } },
    { "Bip01 Spine2",      3, { 4.118601f, -0.003280f, 0.000000f },  { 0.000000f, 0.000000f, 0.000049f } },
    { "Bip01 Spine3",      4, { 4.118600f, -0.003280f, 0.000000f },  { 0.000000f, 0.000000f, -0.000009f } },
    { "Bip01 Neck",        5, { 4.118531f, -0.003538f, 0.000000f },  { 0.000000f, 0.000000f, -0.019437f } },
    { "Bip01 R Clavicle",  6, { 0.000004f, 0.003543f, -1.732721f },  { -0.000040f, 1.501696f, -3.122992f } },
    { "Bip01 R UpperArm",  7, { 6.384777f, 0.000000f, -0.000001f },  { -0.025648f, 0.046980f, 0.004099f } },
    { "Bip01 R Forearm",   8, { 10.242681f, 0.000000f, 0.000002f },  { 0.000000f, 0.000000f, -0.008014f } },
    { "Bip01 R Hand",      9, { 11.375562f, 0.000000f, -0.000002f }, { 1.580468f, 0.132234f, 0.009455f } },
};
// sven and hl bones same but sven have different name
const SkeletonBone kHalfLifeArm[] = {
    { "Bip01",         -1, { 0.234000f, 0.368000f, 39.526000f }, { 0.000000f, 0.000000f, -1.571000f } },
    { "Bip01 Pelvis",   0, { 0.000000f, 0.000000f, 0.000000f },  { -1.571000f, -1.571000f, 0.000000f } },
    { "Bip01 Spine",    1, { 1.294000f, 0.684000f, 0.000000f },  { 0.000000f, 0.000000f, 0.114000f } },
    { "Bip01 Spine1",   2, { 5.310000f, -0.004000f, 0.000000f }, { 0.000000f, 0.000000f, -0.070000f } },
    { "Bip01 Spine2",   3, { 4.827000f, -0.005000f, 0.000000f }, { 0.000000f, 0.000000f, -0.096000f } },
    { "Bip01 Spine3",   4, { 5.793000f, -0.005000f, 0.000000f }, { 0.000000f, 0.000000f, 0.017000f } },
    { "Bip01 Neck",     5, { 6.662000f, -0.001000f, 0.000000f }, { 0.000000f, 0.000000f, 0.471000f } },
    { "Bip01 R Arm",    6, { -2.870000f, 1.400000f, -4.140000f }, { 0.000000f, 1.371000f, 2.670000f } },
    { "Bip01 R Arm1",   7, { 4.972000f, 0.000000f, 0.000000f },  { -0.115000f, -1.269000f, 0.117000f } },
    { "Bip01 R Arm2",   8, { 11.585000f, 0.000000f, 0.000000f }, { 0.000000f, 0.000000f, -0.200000f } },
    { "Bip01 R Hand",   9, { 10.127000f, 0.000000f, 0.000000f }, { 1.570000f, 0.000000f, -0.070000f } },
};
static_assert(sizeof(kHalfLifeArm) == sizeof(kPlayerArm), "both skeletons end at the hand, eleven bones in");

const SkeletonBone kPlayerLeftArm[] = {
    { "Bip01 L Clavicle", -1, { 0.000004f, 0.003534f, 1.732721f },  { -0.000040f, -1.501696f, -3.122911f } },
    { "Bip01 L UpperArm",  0, { 6.384776f, 0.000000f, 0.000001f },  { 0.025648f, -0.046980f, 0.004099f } },
    { "Bip01 L Forearm",   1, { 10.242682f, 0.000000f, -0.000002f }, { 0.000000f, 0.000000f, -0.008014f } },
    { "Bip01 L Hand",      2, { 11.375562f, 0.000000f, 0.000005f },  { -1.580468f, -0.132234f, 0.009455f } },
};
const SkeletonBone kHalfLifeLeftArm[] = {
    { "Bip01 L Arm",  -1, { -3.112000f, 1.517200f, 4.341900f }, { -0.000010f, -1.370790f, 2.669800f } },
    { "Bip01 L Arm1",  0, { 4.827200f, 0.000000f, 0.000000f },  { 0.112060f, 1.260120f, 0.113670f } },
    { "Bip01 L Arm2",  1, { 11.139700f, 0.000000f, 0.000000f }, { 0.000000f, 0.000000f, -0.199990f } },
    { "Bip01 L Hand",  2, { 10.314500f, 0.000000f, 0.000000f }, { -1.569990f, 0.000000f, -0.069770f } },
};
constexpr int kLeftArmBones = 4;
constexpr int kNeckBone = 6;
const char* const kLeftWeaponBoneName = "weapon_l";

constexpr int kPlayerArmBones = static_cast<int>(sizeof(kPlayerArm) / sizeof(kPlayerArm[0]));
constexpr int kHandBone = kPlayerArmBones - 1;
const char* const kWeaponBoneName = "weapon";


// forward, side and palm in "Bip01 R Hand" space, measured on stock p_ models
const HoldPose kGunHold = { aiVector3D(1.0f, -0.04f, 0.0f), aiVector3D(0.04f, 1.0f, 0.0f), aiVector3D(3.0f, 0.0f, 0.0f) };

const HoldPose kKnifeHold = { aiVector3D(0.42f, -0.26f, 0.87f), aiVector3D(0.12f, 0.97f, 0.22f), aiVector3D(2.7f, 0.4f, 0.6f) };

const HoldPose kPackHold = { aiVector3D(0.35f, 0.94f, 0.06f), aiVector3D(-0.87f, 0.29f, 0.41f), aiVector3D(4.2f, 4.2f, 1.1f) };
const HoldPose kHalfLifePackHold = { aiVector3D(-0.10f, 0.99f, 0.02f), aiVector3D(-0.87f, 0.0f, 0.49f), aiVector3D(4.2f, 4.2f, 1.2f) };

const HoldPose kGrenadeHold = { aiVector3D(0.33f, -0.21f, 0.92f), aiVector3D(-0.12f, 0.96f, 0.26f), aiVector3D(3.0f, 0.2f, 1.0f) };

// reach from hand bone to fingertips on the player model, calibrated on stock p_ models
constexpr float kPlayerHandMeasure = 5.9f;

const HoldPose kHalfLifeGunHold = { aiVector3D(0.99f, 0.09f, 0.04f), aiVector3D(-0.09f, 0.99f, 0.03f), aiVector3D(3.0f, 0.0f, 0.0f) };
const HoldPose kHalfLifeKnifeHold = { aiVector3D(0.26f, 0.0f, 0.97f), aiVector3D(-0.10f, 0.99f, 0.03f), aiVector3D(2.7f, 0.4f, 0.6f) };
const HoldPose kHalfLifeGrenadeHold = { aiVector3D(0.0f, 0.0f, 1.0f), aiVector3D(0.0f, 1.0f, 0.0f), aiVector3D(3.0f, 1.5f, 1.2f) };
constexpr float kHalfLifeHandMeasure = 6.7f;

// MAXSTUDIOVERTS is 2048
constexpr size_t kMaxVertsPerBody = 1900;

// the left hand bone is the right one mirrored Z flipped
HoldPose leftHandHold(const HoldPose& right)
{
    HoldPose left;
    left.forward = aiVector3D(right.forward.x, right.forward.y, -right.forward.z);
    left.side = aiVector3D(-right.side.x, -right.side.y, right.side.z);
    left.palm = aiVector3D(right.palm.x, right.palm.y, -right.palm.z);
    return left;
}

WeaponType kindFromName(std::string name)
{
    for (char& c : name) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    for (const char* word : { "knife", "sword", "katana", "crowbar", "wrench", "machete", "dagger", "blade", "hammer", "sabre", "saber",
                              "scythe", "sickle", "spear", "mace" })
    {
        if (name.find(word) != std::string::npos) return WeaponType::Knife;
    }
    for (const char* word : { "tripmine", "lasermine" })
    {
        if (name.find(word) != std::string::npos) return WeaponType::Pack;
    }
    for (const char* word : { "grenade", "flashbang", "smoke", "bomb", "satchel", "molotov", "squeak", "medkit" })
    {
        if (name.find(word) != std::string::npos) return WeaponType::Grenade;
    }
    // short words: whole-word match only
    std::string token;
    name.push_back(' ');
    for (unsigned char c : name)
    {
        if (std::isalnum(c))
        {
            token.push_back(static_cast<char>(c));
            continue;
        }
        if (token == "axe" || token == "bat") return WeaponType::Knife;
        if (token == "nade") return WeaponType::Grenade;
        if (token == "c4" || token == "mine" || token == "tripmine" || token == "lasermine") return WeaponType::Pack;
        token.clear();
    }
    return WeaponType::Auto;
}


aiMatrix4x4 holdMatrix(const HoldPose& hold)
{
    aiVector3D x = hold.forward;
    x.NormalizeSafe();
    aiVector3D y = hold.side - x * (hold.side * x);
    y.NormalizeSafe();
    const aiVector3D z = x ^ y;
    return aiMatrix4x4(x.x, y.x, z.x, hold.palm.x,
                       x.y, y.y, z.y, hold.palm.y,
                       x.z, y.z, z.z, hold.palm.z,
                       0, 0, 0, 1);
}


void eraseAll(std::string& s, const char* word)
{
    const size_t n = std::strlen(word);
    for (size_t at = s.find(word); at != std::string::npos; at = s.find(word, at))
    {
        s.erase(at, n);
    }
}

// get the player model's hand bones by name,
bool getHandNames(const std::string& name)
{
    std::string s = String_ToLower(name);
    for (const char* weaponWord : { "handguard", "handgun", "handle", "handcannon" }) // these are most common words
    {
        eraseAll(s, weaponWord);
    }
    for (const char* word : { "hand", "glove", "finger", "sleeve", "forearm" })  // these are most common words
    {
        if (s.find(word) != std::string::npos) return true;
    }
    // short words: whole-word match only
    std::string token;
    s.push_back(' ');
    for (unsigned char c : s)
    {
        if (std::isalpha(c))
        {
            token.push_back(static_cast<char>(c));
            continue;
        }
        if (token == "arm" || token == "arms" || token == "skin")
        {
            return true;
        }
        token.clear();
    }
    return false;
}

bool getBoneArm(const std::string& name)
{
    std::string s = String_ToLower(name);
    for (const char* weaponThings : { "handguard", "handgun", "handle", "armature", "armor" })
    {
        eraseAll(s, weaponThings);
    }
    for (const char* things : { "hand", "finger", "thumb", "pinky", "forearm", "upperarm", "wrist", "clavicle", "arm", "twist",
                              "bip01 l ", "bip01 r " })
    {
        if (s.find(things) != std::string::npos) return true;
    }
    return false;
}

void getHandsShape(const StudioModel& model, const std::vector<int>& verticesOnBone, std::vector<bool>& isHand,std::vector<bool>& isFinger, std::vector<int>& fingerNumber)
{
    const size_t count = model.bones.size();
    isHand.assign(count, false);
    isFinger.assign(count, false);
    fingerNumber.assign(count, -1);
    int nextFinger = 0;

    std::vector<std::vector<int>> children(count);
    for (size_t i = 0; i < count; i++)
    {
        const int parent = model.bones[i].parent;
        if (parent >= 0 && parent < static_cast<int>(count)) children[parent].push_back(static_cast<int>(i));
    }

    // joints of a chain, empty if it forks (a leaf nub is not a fork)
    auto plainChain = [&](int first) {
        std::vector<int> chain = { first };
        int at = first;
        while (!children[at].empty())
        {
            int next = children[at][0];
            int withChildren = 0;
            for (int c : children[at])
            {
                if (!children[c].empty())
                {
                    withChildren++;
                    next = c;
                }
            }
            if (withChildren > 1 || chain.size() > 4)
            {
                return std::vector<int>();
            }
            for (int c : children[at])
            {
                if (c != next) chain.push_back(c);
            }
            chain.push_back(next);
            at = next;
        }
        return chain;
    };
    auto joints = [&](int first) {
        int n = 1;
        for (int at = first; !children[at].empty(); n++)
        {
            int next = children[at][0];
            for (int c : children[at])
            {
                if (!children[c].empty()) next = c;
            }
            at = next;
            if (n > 8) break;
        }
        return n;
    };

    for (size_t bone = 0; bone < count; bone++)
    {
        std::vector<std::vector<int>> fingers;
        for (int child : children[bone])
        {
            std::vector<int> chain = plainChain(child);
            const int length = chain.empty() ? 0 : joints(child);
            if (length == 3 || length == 4)
            {
                fingers.push_back(std::move(chain));
            }
        }
        // a tool chained to the hand looks like a finger but carries far more vertices
        if (fingers.size() >= 4)
        {
            std::vector<int> load;
            for (const std::vector<int>& finger : fingers)
            {
                int total = 0;
                for (int joint : finger) total += verticesOnBone[joint];
                load.push_back(total);
            }
            std::vector<int> sorted = load;
            std::sort(sorted.begin(), sorted.end());
            const int typical = std::max(4, sorted[sorted.size() / 2]);
            std::vector<std::vector<int>> real;
            for (size_t i = 0; i < fingers.size(); i++)
            {
                if (load[i] <= 3 * typical) real.push_back(fingers[i]);
            }
            fingers.swap(real);
        }
        if (fingers.size() >= 4)
        {
            isHand[bone] = true;
            for (const std::vector<int>& finger : fingers)
            {
                for (int joint : finger)
                {
                    isFinger[joint] = true;
                    fingerNumber[joint] = nextFinger;
                }
                nextFinger++;
            }
        }
    }
}

std::string safeFileName(const std::string& name)
{
    std::string out;
    for (unsigned char c : name)
    {
        if (std::isalnum(c) || c == '_' || c == '-' || c == '.' || c == '$' || c == '#' || c == '@')
        {
            out.push_back(static_cast<char>(c));
        }
        else if (c == ' ')
        {
            out.push_back('_');
        }
    }
    return out;
}


// smallest sampled distance between two point sets
float gapBetween(const std::vector<aiVector3D>& from, const std::vector<aiVector3D>& to, size_t maxSamples)
{
    if (from.empty() || to.empty())
    {
        return 1e30f;
    }
    const size_t stepA = std::max<size_t>(1, from.size() / maxSamples);
    const size_t stepB = std::max<size_t>(1, to.size() / maxSamples);
    float best = 1e30f;
    for (size_t i = 0; i < from.size(); i += stepA)
    {
        for (size_t j = 0; j < to.size(); j += stepB)
        {
            best = std::min(best, (from[i] - to[j]).SquareLength());
        }
    }
    return std::sqrt(best);
}

aiVector3D longestAxis(const std::vector<aiVector3D>& points, const aiVector3D& centre)
{
    double c[3][3] = {};
    for (const aiVector3D& p : points)
    {
        const double d[3] = { p.x - centre.x, p.y - centre.y, p.z - centre.z };
        for (int r = 0; r < 3; r++)
            for (int k = 0; k < 3; k++) c[r][k] += d[r] * d[k];
    }
    // power iteration on the covariance matrix
    double v[3] = { 1.0, 0.3, 0.2 };
    for (int iteration = 0; iteration < 64; iteration++)
    {
        double n[3];
        for (int r = 0; r < 3; r++) n[r] = c[r][0] * v[0] + c[r][1] * v[1] + c[r][2] * v[2];
        const double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
        if (len < 1e-20)
        {
            break;
        }
        for (int r = 0; r < 3; r++) v[r] = n[r] / len;
    }
    aiVector3D axis(static_cast<float>(v[0]), static_cast<float>(v[1]), static_cast<float>(v[2]));
    return axis.NormalizeSafe();
}

aiVector3D thinnestAxis(const std::vector<aiVector3D>& points, const aiVector3D& centre)
{
    const aiVector3D first = longestAxis(points, centre);
    std::vector<aiVector3D> flattened;
    flattened.reserve(points.size());
    for (const aiVector3D& p : points)
    {
        const aiVector3D d = p - centre;
        flattened.push_back(d - first * (d * first));
    }
    const aiVector3D second = longestAxis(flattened, aiVector3D(0, 0, 0));
    aiVector3D third = first ^ second;
    return third.NormalizeSafe();
}

void extentAlong(const std::vector<aiVector3D>& points, const aiVector3D& axis, float& low, float& high)
{
    low = 1e30f;
    high = -1e30f;
    for (const aiVector3D& p : points)
    {
        const float d = p * axis;
        low = std::min(low, d);
        high = std::max(high, d);
    }
}


const GameDirProfile& profileFor(GameDir gameDir)
{
    static const GameDirProfile cstrike = { "Counter-Strike", kPlayerArm, kPlayerLeftArm, kGunHold, kKnifeHold, kGrenadeHold, kPackHold, kPlayerHandMeasure };
    static const GameDirProfile valve = { "Half-Life", kHalfLifeArm, kHalfLifeLeftArm, kHalfLifeGunHold, kHalfLifeKnifeHold, kHalfLifeGrenadeHold,
                                          kHalfLifePackHold,
                                          kHalfLifeHandMeasure };
    static const GameDirProfile svencoop = { "Sven Co-op", kHalfLifeArm, kHalfLifeLeftArm, kHalfLifeGunHold, kHalfLifeKnifeHold, kHalfLifeGrenadeHold,
                                          kHalfLifePackHold,
                                          kHalfLifeHandMeasure };
    switch (gameDir)
    {
    case GameDir::valve: return valve;
    case GameDir::svencoop: return svencoop;
    default: return cstrike;
    }
}

// with leftArm, triangles marked hand == 1 go to the left weapon bone
void writeSmd(const fs::path& path, const SkeletonBone* playerSkeleton, const SkeletonBone* leftArm, const aiMatrix4x4 toWorld[2],
              const std::vector<const Triangle*>& triangles, const std::vector<std::string>& textureNames,
              bool flipWinding)
{
    std::ofstream out(path, std::ios::binary);
    std::string text;
    char line[256];

    text += "version 1\nnodes\n";
    int weaponBone[2] = { 0, 0 };
    if (playerSkeleton)
    {
        for (int i = 0; i < kPlayerArmBones; i++)
        {
            std::snprintf(line, sizeof(line), "%d \"%s\" %d\n", i, playerSkeleton[i].name, playerSkeleton[i].parent);
            text += line;
        }
        weaponBone[0] = kPlayerArmBones;
        std::snprintf(line, sizeof(line), "%d \"%s\" %d\n", weaponBone[0], kWeaponBoneName, kHandBone);
        text += line;
        weaponBone[1] = weaponBone[0];
        if (leftArm)
        {
            const int first = weaponBone[0] + 1;
            for (int i = 0; i < kLeftArmBones; i++)
            {
                std::snprintf(line, sizeof(line), "%d \"%s\" %d\n", first + i, leftArm[i].name,
                              leftArm[i].parent < 0 ? kNeckBone : first + leftArm[i].parent);
                text += line;
            }
            weaponBone[1] = first + kLeftArmBones;
            std::snprintf(line, sizeof(line), "%d \"%s\" %d\n", weaponBone[1], kLeftWeaponBoneName, first + kLeftArmBones - 1);
            text += line;
        }
    }
    else
    {
        std::snprintf(line, sizeof(line), "0 \"%s\" -1\n", kWeaponBoneName);
        text += line;
    }

    text += "end\nskeleton\ntime 0\n";
    auto boneLine = [&](int index, const SkeletonBone& b) {
        std::snprintf(line, sizeof(line), "%d %.6f %.6f %.6f %.6f %.6f %.6f\n", index, b.position[0], b.position[1], b.position[2],
                      b.rotation[0], b.rotation[1], b.rotation[2]);
        text += line;
    };
    auto restLine = [&](int index) {
        std::snprintf(line, sizeof(line), "%d 0.000000 0.000000 0.000000 0.000000 0.000000 0.000000\n", index);
        text += line;
    };
    if (playerSkeleton)
    {
        for (int i = 0; i < kPlayerArmBones; i++) boneLine(i, playerSkeleton[i]);
    }
    restLine(weaponBone[0]);
    if (playerSkeleton && leftArm)
    {
        for (int i = 0; i < kLeftArmBones; i++) boneLine(weaponBone[0] + 1 + i, leftArm[i]);
        restLine(weaponBone[1]);
    }
    text += "end\ntriangles\n";

    const aiMatrix3x3 rotation[2] = { aiMatrix3x3(toWorld[0]), aiMatrix3x3(toWorld[1]) };
    for (const Triangle* triangle : triangles)
    {
        const int hand = triangle->hand ? 1 : 0;
        text += (triangle->texture >= 0 && triangle->texture < static_cast<int>(textureNames.size()))
                    ? textureNames[triangle->texture] : std::string("notexture.bmp");
        text += "\n";
        for (int k = 0; k < 3; k++)
        {
            const Corner& c = triangle->corner[flipWinding ? 2 - k : k];
            const aiVector3D p = toWorld[hand] * c.position;
            const aiVector3D n = rotation[hand] * c.normal;
            std::snprintf(line, sizeof(line), "%d %.6f %.6f %.6f %.6f %.6f %.6f %.6f %.6f\n", weaponBone[hand], p.x, p.y, p.z,
                          n.x, n.y, n.z, c.u, 1.0f - c.v);
            text += line;
        }
        if (text.size() > (1u << 20))
        {
            out.write(text.data(), static_cast<std::streamsize>(text.size()));
            text.clear();
        }
    }
    text += "end\n";
    out.write(text.data(), static_cast<std::streamsize>(text.size()));
}

float lengthAlong(const std::vector<aiVector3D>& points, const aiVector3D& axis)
{
    float low = 0, high = 0;
    extentAlong(points, axis, low, high);
    return high - low;
}

void boundsOf(const std::vector<aiVector3D>& points, aiVector3D& lo, aiVector3D& hi)
{
    lo = aiVector3D(1e30f, 1e30f, 1e30f);
    hi = aiVector3D(-1e30f, -1e30f, -1e30f);
    for (const aiVector3D& p : points)
    {
        lo = aiVector3D(std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z));
        hi = aiVector3D(std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z));
    }
}

// the points about their centre, turned into the weapon's frame
std::vector<aiVector3D> levelPoints(const std::vector<aiVector3D>& points, const aiVector3D& centre, const aiMatrix3x3& level)
{
    std::vector<aiVector3D> levelled;
    levelled.reserve(points.size());
    for (const aiVector3D& p : points) levelled.push_back(level * (p - centre));
    return levelled;
}

HandBones findHandBones(const StudioModel& model)
{
    HandBones bones;
    std::vector<int> verticesOnBone(model.bones.size(), 0);
    for (const StudioBodyPart& part : model.bodyParts)
    {
        if (part.models.empty()) continue;
        for (int bone : part.models.front().vertexBones) verticesOnBone[bone]++;
    }
    std::vector<bool> fingerByShape;
    getHandsShape(model, verticesOnBone, bones.handByShape, fingerByShape, bones.fingerNumber);

    bones.armBone.assign(model.bones.size(), false);
    bones.fingerByName.assign(model.bones.size(), false);
    for (size_t i = 0; i < model.bones.size(); i++)
    {
        bones.armBone[i] = bones.handByShape[i] || fingerByShape[i] || getBoneArm(model.bones[i].name);
        // "hand" and "arm" are not enough: a knife can hang from the hand bone
        const std::string name = String_ToLower(model.bones[i].name);
        bones.fingerByName[i] = name.find("finger") != std::string::npos || name.find("thumb") != std::string::npos ||
                                name.find("pinky") != std::string::npos;
    }
    return bones;
}

// arm skin: everything joined, triangle to triangle, to a finger vertex
std::vector<bool> findArmSkin(const StudioSubModel& sub, const HandBones& bones)
{
    std::vector<int> island(sub.vertices.size());
    for (size_t i = 0; i < island.size(); i++) island[i] = static_cast<int>(i);
    auto find = [&](int v) {
        while (island[v] != v)
        {
            island[v] = island[island[v]];
            v = island[v];
        }
        return v;
    };
    for (const StudioMesh& mesh : sub.meshes)
    {
        for (size_t i = 0; i + 2 < mesh.triangles.size(); i += 3)
        {
            const int a = find(mesh.triangles[i].vertex);
            island[find(mesh.triangles[i + 1].vertex)] = a;
            island[find(mesh.triangles[i + 2].vertex)] = a;
        }
    }
    // a named finger is enough; shape-only fingers need two in one island (a tool chain has one)
    std::vector<bool> islandHasFinger(sub.vertices.size(), false);
    std::vector<int> islandFinger(sub.vertices.size(), -1);
    for (size_t v = 0; v < sub.vertices.size(); v++)
    {
        const int bone = sub.vertexBones[v];
        const int root = find(static_cast<int>(v));
        if (bones.fingerByName[bone])
        {
            islandHasFinger[root] = true;
        }
        else if (bones.fingerNumber[bone] >= 0)
        {
            if (islandFinger[root] >= 0 && islandFinger[root] != bones.fingerNumber[bone]) islandHasFinger[root] = true;
            islandFinger[root] = bones.fingerNumber[bone];
        }
    }
    std::vector<bool> armSkin(sub.vertices.size(), false);
    for (size_t v = 0; v < sub.vertices.size(); v++)
    {
        armSkin[v] = islandHasFinger[find(static_cast<int>(v))];
    }
    return armSkin;
}

bool meshIsHands(const StudioBodyPart& part, const StudioSubModel& sub, const StudioMesh& mesh, const std::string& textureName,
                 const std::vector<bool>& armBone)
{
    if (getHandNames(part.name) || getHandNames(sub.name) || getHandNames(textureName))
    {
        return true;
    }
    std::set<int> armBones;
    size_t onArm = 0;
    for (const StudioTriVertex& tv : mesh.triangles)
    {
        const int bone = sub.vertexBones[tv.vertex];
        if (armBone[bone])
        {
            onArm++;
            armBones.insert(bone);
        }
    }
    return armBones.size() >= 4 && onArm * 10 >= mesh.triangles.size() * 6;
}

// splits every mesh into weapon triangles and hand skin; effects are dropped
void sortMeshes(const StudioModel& model, const std::vector<aiMatrix4x4>& pose, const HandBones& bones, bool keepEffects,
                std::vector<Triangle>& weapon, HandSkin& hands, WeaponModelResult& report)
{
    int sourceIndex = 0;
    for (const StudioBodyPart& part : model.bodyParts)
    {
        if (part.models.empty())
        {
            continue;
        }
        // default sub-model only; the others are alternatives
        const StudioSubModel& sub = part.models.front();
        sourceIndex++;
        const std::vector<bool> armSkin = findArmSkin(sub, bones);

        for (const StudioMesh& mesh : sub.meshes)
        {
            if (mesh.triangles.empty())
            {
                continue;
            }
            const StudioTexture* texture = mesh.texture >= 0 ? &model.textures[mesh.texture] : nullptr;
            const std::string textureName = texture ? texture->name : std::string("?");
            const std::string label = part.name + " / " + sub.name + " / " + textureName;

            if (meshIsHands(part, sub, mesh, textureName, bones.armBone))
            {
                report.removedHands.push_back(label);
                std::set<int> seen;
                for (const StudioTriVertex& tv : mesh.triangles)
                {
                    if (!seen.insert(tv.vertex).second) continue;
                    const int bone = sub.vertexBones[tv.vertex];
                    hands.points.push_back(pose[bone] * sub.vertices[tv.vertex]);
                    hands.bones.push_back(bone);
                }
                continue;
            }

            if (texture && (texture->flags & STUDIO_NF_ADDITIVE) && !keepEffects)
            {
                report.removedEffects.push_back(label);
                continue;
            }

            size_t strayFingers = 0;
            for (size_t i = 0; i + 2 < mesh.triangles.size(); i += 3)
            {
                if (armSkin[mesh.triangles[i].vertex])
                {
                    strayFingers++;
                    for (int k = 0; k < 3; k++)
                    {
                        const int vertex = mesh.triangles[i + k].vertex;
                        hands.points.push_back(pose[sub.vertexBones[vertex]] * sub.vertices[vertex]);
                        hands.bones.push_back(sub.vertexBones[vertex]);
                    }
                    continue;
                }
                Triangle triangle;
                triangle.texture = mesh.texture;
                triangle.source = sourceIndex;
                for (int k = 0; k < 3; k++)
                {
                    const StudioTriVertex& tv = mesh.triangles[i + k];
                    Corner& c = triangle.corner[k];
                    c.bone = sub.vertexBones[tv.vertex];
                    c.position = pose[c.bone] * sub.vertices[tv.vertex];
                    c.normal = aiMatrix3x3(pose[sub.normalBones[tv.normal]]) * sub.normals[tv.normal];
                    c.u = tv.u;
                    c.v = tv.v;
                }
                weapon.push_back(triangle);
            }
            if (strayFingers > 0)
            {
                report.removedHands.push_back(label + "  (only the " + std::to_string(strayFingers) + " triangles of it that are arm skin)");
            }
        }
    }
}

// size of the weapon from the middle 80% on each axis, so stray parts do not inflate it
float middleSpread(const std::vector<Triangle>& weapon)
{
    std::vector<float> axis[3];
    for (const Triangle& t : weapon)
    {
        for (const Corner& c : t.corner)
        {
            axis[0].push_back(c.position.x);
            axis[1].push_back(c.position.y);
            axis[2].push_back(c.position.z);
        }
    }
    float range[3];
    for (int k = 0; k < 3; k++)
    {
        std::sort(axis[k].begin(), axis[k].end());
        range[k] = axis[k][axis[k].size() * 9 / 10] - axis[k][axis[k].size() / 10];
    }
    return aiVector3D(range[0], range[1], range[2]).Length();
}

// keeps what hangs together with the biggest bone group; returns true when the model holds
// two weapons of a kind, and then keeps the one for the hand asked for
bool selectWeaponPieces(std::vector<Triangle>& weapon, const StudioModel& model, const std::vector<bool>& armBone,
                        const std::vector<aiVector3D>& handPoints, bool twinWanted, bool twinWithoutHands, bool dualMirror,
                        bool takeLeft, std::vector<std::string>& removedParts)
{
    std::map<int, std::vector<aiVector3D>> groups;
    for (const Triangle& t : weapon)
    {
        for (const Corner& c : t.corner) groups[c.bone].push_back(c.position);
    }
    int biggest = groups.begin()->first;
    for (const auto& [bone, points] : groups)
    {
        if (points.size() > groups[biggest].size()) biggest = bone;
    }
    const float threshold = 0.08f * middleSpread(weapon);

    auto grow = [&](int start, const std::set<int>& taken) {
        std::set<int> piece = { start };
        std::vector<aiVector3D> core = groups[start];
        bool grew = true;
        while (grew)
        {
            grew = false;
            for (const auto& [bone, points] : groups)
            {
                if (piece.count(bone) || taken.count(bone)) continue;
                if (gapBetween(points, core, 500) < threshold)
                {
                    piece.insert(bone);
                    core.insert(core.end(), points.begin(), points.end());
                    grew = true;
                }
            }
        }
        return piece;
    };
    auto measure = [&](const std::set<int>& piece, size_t& count, float& diagonal, float& side) {
        aiVector3D lo(1e30f, 1e30f, 1e30f), hi(-1e30f, -1e30f, -1e30f);
        count = 0;
        double y = 0;
        for (int bone : piece)
        {
            for (const aiVector3D& p : groups[bone])
            {
                lo = aiVector3D(std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z));
                hi = aiVector3D(std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z));
                y += p.y;
                count++;
            }
        }
        diagonal = (hi - lo).Length();
        side = count ? static_cast<float>(y / count) : 0.0f;
    };
    // a second weapon parked off screen is in nobody's hand
    auto inAHand = [&](const std::set<int>& piece) {
        // with no hands in the model there is nothing to judge by: only if asked for
        if (handPoints.empty()) return twinWithoutHands;
        std::vector<aiVector3D> points;
        for (int bone : piece) points.insert(points.end(), groups[bone].begin(), groups[bone].end());
        return gapBetween(points, handPoints, 400) < 1.5f * threshold;
    };

    bool dual = false;
    std::set<int> kept = grow(biggest, {});
    std::set<int> twin;

    // twin bone trees with equal vertex counts: two of a kind, even when they touch
    if (twinWanted)
    {
        std::map<int, size_t> load;
        std::map<int, std::set<int>> members;
        size_t total = 0;
        for (const auto& [bone, points] : groups)
        {
            int root = bone;
            for (int p = model.bones[bone].parent; p >= 0 && !armBone[p]; p = model.bones[p].parent) root = p;
            load[root] += points.size();
            members[root].insert(bone);
            total += points.size();
        }
        int first = -1, second = -1;
        for (const auto& [root, count] : load)
        {
            if (first < 0 || count > load[first]) { second = first; first = root; }
            else if (second < 0 || count > load[second]) second = root;
        }
        if (first >= 0 && second >= 0)
        {
            const size_t a = load[first], b = load[second];
            if (b * 10 >= a * 9 && b * 10 >= total * 3 && inAHand(members[first]) && inAHand(members[second]))
            {
                kept = members[first];
                twin = members[second];
                dual = true;
            }
        }
    }

    // or a second piece of similar size standing apart
    if (!dual && kept.size() < groups.size())
    {
        int next = -1;
        for (const auto& [bone, points] : groups)
        {
            if (kept.count(bone)) continue;
            if (next < 0 || points.size() > groups[next].size()) next = bone;
        }
        const std::set<int> second = grow(next, kept);
        size_t countA = 0, countB = 0;
        float sizeA = 0, sizeB = 0, sideA = 0, sideB = 0;
        measure(kept, countA, sizeA, sideA);
        measure(second, countB, sizeB, sideB);
        const bool alike = countB * 10 >= countA * 4 && countA * 10 >= countB * 4 && sizeB > 0.6f * sizeA && sizeB < 1.67f * sizeA;
        if (alike && twinWanted && inAHand(kept) && inAHand(second))
        {
            twin = second;
            dual = true;
        }
    }
    if (dual)
    {
        size_t countA = 0, countB = 0;
        float sizeA = 0, sizeB = 0, sideA = 0, sideB = 0;
        measure(kept, countA, sizeA, sideA);
        measure(twin, countB, sizeB, sideB);
        // +Y is the view's left
        const bool keptIsRight = dualMirror ? sideA > sideB : sideA < sideB;
        // main pass: right hand, other pass: left
        if (keptIsRight == takeLeft) kept.swap(twin);
    }

    if (kept.size() < groups.size())
    {
        std::vector<Triangle> connected;
        for (const Triangle& t : weapon)
        {
            if (kept.count(t.corner[0].bone) && kept.count(t.corner[1].bone) && kept.count(t.corner[2].bone))
            {
                connected.push_back(t);
            }
        }
        for (const auto& [bone, points] : groups)
        {
            if (!kept.count(bone) && !twin.count(bone)) removedParts.push_back(model.bones[bone].name);
        }
        weapon.swap(connected);
    }
    return dual;
}

void measureHand(Arm& arm, const StudioModel& model, const std::vector<aiMatrix4x4>& pose, const std::vector<bool>& handByShape)
{
    int handBone = -1, handChildren = 1, handDepth = -1;
    for (int bone : arm.bones)
    {
        int children = 0;
        for (int other : arm.bones)
        {
            if (model.bones[other].parent == bone) children++;
        }
        if (handByShape[bone]) children += 100;
        if (children < 2) continue;
        int depth = 0;
        for (int p = model.bones[bone].parent; p >= 0; p = model.bones[p].parent) depth++;
        if (children > handChildren || (children == handChildren && depth > handDepth))
        {
            handChildren = children;
            handDepth = depth;
            handBone = bone;
        }
    }
    if (handBone < 0)
    {
        return;
    }

    std::vector<bool> under(model.bones.size(), false);
    for (size_t i = 0; i < model.bones.size(); i++)
    {
        for (int b = static_cast<int>(i); b >= 0; b = model.bones[b].parent)
        {
            if (b == handBone)
            {
                under[i] = true;
                break;
            }
        }
    }
    const aiVector3D origin(pose[handBone].a4, pose[handBone].b4, pose[handBone].c4);
    std::vector<float> reach;
    for (size_t i = 0; i < arm.points.size(); i++)
    {
        if (under[arm.pointBones[i]]) reach.push_back((arm.points[i] - origin).Length());
    }
    if (reach.size() > 20)
    {
        std::sort(reach.begin(), reach.end());
        arm.handMeasure = reach[reach.size() * 95 / 100];
        arm.handBone = model.bones[handBone].name;

        // a guessed hand bone must not span most of the arm
        const std::string boneName = String_ToLower(model.bones[handBone].name);
        if (!handByShape[handBone] && boneName.find("hand") == std::string::npos)
        {
            aiVector3D lo, hi;
            boundsOf(arm.points, lo, hi);
            if (arm.handMeasure > 0.45f * (hi - lo).Length()) arm.handMeasure = 0.0f;
        }
    }
    arm.handBoneIndex = handBone;
}

std::vector<Arm> findArms(const StudioModel& model, const std::vector<aiMatrix4x4>& pose, const HandSkin& hands,
                          const std::vector<bool>& handByShape, const std::vector<aiVector3D>& weaponPoints)
{
    std::vector<Arm> arms;
    if (hands.points.empty())
    {
        return arms;
    }

    std::set<int> used(hands.bones.begin(), hands.bones.end());
    std::set<int> closed = used;
    for (int bone : used)
    {
        std::vector<int> chain;
        for (int p = model.bones[bone].parent; p >= 0; p = model.bones[p].parent)
        {
            chain.push_back(p);
            if (used.count(p))
            {
                closed.insert(chain.begin(), chain.end());
                break;
            }
        }
    }
    std::map<int, int> armOfRoot;
    std::vector<int> armOfBone(model.bones.size(), -1);
    for (int bone : closed)
    {
        int root = bone;
        while (model.bones[root].parent >= 0 && closed.count(model.bones[root].parent)) root = model.bones[root].parent;
        auto found = armOfRoot.find(root);
        if (found == armOfRoot.end())
        {
            found = armOfRoot.emplace(root, static_cast<int>(arms.size())).first;
            arms.emplace_back();
        }
        armOfBone[bone] = found->second;
        arms[found->second].bones.insert(bone);
    }
    for (size_t i = 0; i < hands.points.size(); i++)
    {
        Arm& arm = arms[armOfBone[hands.bones[i]]];
        arm.points.push_back(hands.points[i]);
        arm.pointBones.push_back(hands.bones[i]);
    }

    for (Arm& arm : arms)
    {
        if (arm.points.size() < 12)
        {
            continue;
        }
        // contact: the nearest seventh of the arm's skin
        std::vector<std::pair<float, size_t>> nearest(arm.points.size());
        const size_t step = std::max<size_t>(1, weaponPoints.size() / 1500);
        for (size_t i = 0; i < arm.points.size(); i++)
        {
            float best = 1e30f;
            for (size_t j = 0; j < weaponPoints.size(); j += step)
            {
                best = std::min(best, (arm.points[i] - weaponPoints[j]).SquareLength());
            }
            nearest[i] = { best, i };
        }
        std::sort(nearest.begin(), nearest.end());
        const size_t patch = std::max<size_t>(10, arm.points.size() / 7);
        double gap = 0.0;
        for (size_t i = 0; i < patch; i++)
        {
            arm.contact += arm.points[nearest[i].second];
            gap += std::sqrt(nearest[i].first);
        }
        arm.contact /= static_cast<float>(patch);
        arm.gap = static_cast<float>(gap / patch);

        measureHand(arm, model, pose, handByShape);
    }
    return arms;
}

// weapon rigged to a hand bone: that bone is the grip, even if the hand is not modelled
int findCarrierHand(const std::vector<Triangle>& weapon, const StudioModel& model, const std::vector<bool>& handByShape)
{
    std::map<int, int> load;
    for (const Triangle& t : weapon)
    {
        for (const Corner& c : t.corner) load[c.bone]++;
    }
    int mainBone = load.begin()->first;
    for (const auto& [bone, count] : load)
    {
        if (count > load[mainBone]) mainBone = bone;
    }
    for (int b = mainBone; b >= 0; b = model.bones[b].parent)
    {
        std::string name = String_ToLower(model.bones[b].name);
        for (const char* weaponWord : { "handguard", "handgun", "handle" }) eraseAll(name, weaponWord);
        if (handByShape[b] || name.find("hand") != std::string::npos)
        {
            return b;
        }
    }
    return -1;
}

bool pointsBackwards(const aiVector3D& forward, float rear, float front, const aiVector3D& grip, bool gripKnown, bool hasMuzzle,
                     const aiVector3D& muzzle, const aiVector3D& centre)
{
    const float at = grip * forward;
    const float ahead = front - at, behind = at - rear;
    if (gripKnown && std::max(ahead, behind) > 1.3f * std::max(0.0f, std::min(ahead, behind)))
    {
        return behind > ahead;
    }
    if (hasMuzzle)
    {
        return (muzzle - centre) * forward < 0;
    }
    return forward.x < 0;
}

WeaponType detectWeaponType(const std::vector<aiVector3D>& levelled, float scale, bool hasMuzzle, bool muzzleAtTip, int holdingArms)
{
    // size on the weapon's own axes
    aiVector3D thinAxis = thinnestAxis(levelled, aiVector3D(0, 0, 0));
    thinAxis.x = 0;
    if (thinAxis.Length() < 0.5f) thinAxis = aiVector3D(0, 1, 0);
    thinAxis.Normalize();
    const aiVector3D wideAxis = aiVector3D(1, 0, 0) ^ thinAxis;
    const aiVector3D size(lengthAlong(levelled, aiVector3D(1, 0, 0)), lengthAlong(levelled, wideAxis), lengthAlong(levelled, thinAxis));

    const float across = std::max(size.y, size.z), thin = std::max(0.01f, std::min(size.y, size.z));
    const float length = size.x * scale;  // in player units
    const bool elongated = size.x >= 1.6f * across;
    const bool flat = across > 1.8f * thin;

    //  gun, unless it is a lump
    if (muzzleAtTip && (elongated || flat)) return WeaponType::Gun;
    if (hasMuzzle && holdingArms >= 2 && length >= 16.0f) return WeaponType::Gun;
    if (size.x > 4.5f * across && flat) return WeaponType::Knife;
    // 2h weapon
    if (holdingArms >= 2 && length >= 16.0f) return WeaponType::Gun;
    if (size.x > 3.0f * across && flat) return WeaponType::Knife;
    // too big for the palm: gun
    if (length >= 20.0f) return WeaponType::Gun;
    return WeaponType::Grenade;
}

// reference: longest axis pointing away from the hand, and thinnest axis
bool referenceHold(const std::string& referenceModel, const GameDirProfile& profile, HoldPose& hold, std::string& error)
{
    StudioModel reference;
    int referenceHand = -1;
    if (reference.Load(referenceModel))
    {
        for (size_t i = 0; i < reference.bones.size(); i++)
        {
            if (String_ToLower(reference.bones[i].name) == "bip01 r hand") referenceHand = static_cast<int>(i);
        }
    }
    std::vector<aiVector3D> referencePoints;
    if (referenceHand >= 0)
    {
        const std::vector<aiMatrix4x4> referencePose = reference.BoneTransforms(0, 0);
        aiMatrix4x4 toReferenceHand = referencePose[referenceHand];
        toReferenceHand.Inverse();
        for (const StudioBodyPart& part : reference.bodyParts)
        {
            if (part.models.empty()) continue;
            const StudioSubModel& sub = part.models.front();
            for (const StudioMesh& mesh : sub.meshes)
            {
                for (const StudioTriVertex& tv : mesh.triangles)
                {
                    referencePoints.push_back(toReferenceHand * (referencePose[sub.vertexBones[tv.vertex]] * sub.vertices[tv.vertex]));
                }
            }
        }
    }
    if (referencePoints.size() < 9)
    {
        error = referenceModel.empty()
                    ? "The hold is set to follow a reference p_ model, but none was given."
                    : "The reference model could not be used: " +
                          (reference.GetError().empty() ? std::string("it has no \"Bip01 R Hand\" bone, so it is not a p_ model.")
                                                        : reference.GetError());
        return false;
    }
    aiVector3D referenceCentre;
    for (const aiVector3D& p : referencePoints) referenceCentre += p;
    referenceCentre /= static_cast<float>(referencePoints.size());

    aiVector3D along = longestAxis(referencePoints, referenceCentre);
    float low = 0, high = 0;
    extentAlong(referencePoints, along, low, high);
    if (std::abs(high) < std::abs(low)) along = -along;
    aiVector3D thin = thinnestAxis(referencePoints, referenceCentre);
    if (thin.y < 0) thin = -thin;

    const bool likeKnife = along * profile.knife.forward > along * profile.gun.forward;
    hold.forward = along;
    hold.side = thin;
    hold.palm = likeKnife ? profile.knife.palm : profile.gun.palm;
    return true;
}

// grenade: true when the narrow end (the fuse) is at the back and has to go up
bool grenadeIsUpsideDown(const std::vector<aiVector3D>& levelled)
{
    float low = 1e30f, high = -1e30f;
    for (const aiVector3D& q : levelled)
    {
        low = std::min(low, q.x);
        high = std::max(high, q.x);
    }
    double front = 0, back = 0;
    size_t frontCount = 0, backCount = 0;
    for (const aiVector3D& q : levelled)
    {
        const float across = std::sqrt(q.y * q.y + q.z * q.z);
        if (q.x > high - 0.25f * (high - low)) { front += across; frontCount++; }
        else if (q.x < low + 0.25f * (high - low)) { back += across; backCount++; }
    }
    if (!frontCount || !backCount)
    {
        return false;
    }
    front /= frontCount;
    back /= backCount;
    return back < 0.85 * front;
}

// stand a flat weapon upright: the view often shows it rolled
void standUpright(aiMatrix3x3& level, const std::vector<aiVector3D>& levelled, float maxRoll)
{
    aiVector3D thin = thinnestAxis(levelled, aiVector3D(0, 0, 0));
    thin.x = 0;
    if (thin.Length() <= 0.8f)
    {
        return;
    }
    thin.Normalize();
    if (thin.y < 0) thin = -thin;
    const aiVector3D tall = aiVector3D(1, 0, 0) ^ thin;
    const float roll = std::atan2(thin.z, thin.y);
    if (lengthAlong(levelled, tall) > 1.5f * lengthAlong(levelled, thin) && std::abs(roll) < maxRoll)
    {
        aiMatrix3x3 upright;
        aiMatrix3x3::Rotation(-roll, aiVector3D(1, 0, 0), upright);
        level = upright * level;
    }
}

// pack: true when the side that sticks out faces the palm and has to be turned away
bool packFacesPalm(const std::vector<aiVector3D>& levelled, bool mirror)
{
    float lowY = 1e30f, highY = -1e30f;
    for (const aiVector3D& q : levelled)
    {
        lowY = std::min(lowY, q.y);
        highY = std::max(highY, q.y);
    }
    float lean = (highY + lowY) * 0.5f;
    if (mirror) lean = -lean;  // the mirror comes later
    return lean < -0.03f * (highY - lowY);
}

// studiomdl rounds up to a power of two and rejects anything over 512; returns the resized pixels, empty if it fits
std::vector<uint8_t> fitTexture(const StudioTexture& texture, int& width, int& height)
{
    auto fit = [](int size) {
        int p = 1;
        while (p * 2 <= size && p * 2 <= 512) p *= 2;
        return p;
    };
    std::vector<uint8_t> resized;
    const int newWidth = fit(width), newHeight = fit(height);
    if (newWidth == width && newHeight == height)
    {
        return resized;
    }
    resized.resize(static_cast<size_t>(newWidth) * newHeight);
    for (int y = 0; y < newHeight; y++)
    {
        const int sy = std::min(height - 1, (y * 2 + 1) * height / (newHeight * 2));
        for (int x = 0; x < newWidth; x++)
        {
            const int sx = std::min(width - 1, (x * 2 + 1) * width / (newWidth * 2));
            resized[static_cast<size_t>(y) * newWidth + x] = texture.pixels[static_cast<size_t>(sy) * width + sx];
        }
    }
    width = newWidth;
    height = newHeight;
    return resized;
}

// writes the BMP of every texture the weapon uses
void writeTextures(const StudioModel& model, const std::vector<Triangle>& weapon, const fs::path& outFolder,
                   std::vector<std::string>& textureNames, std::vector<std::pair<std::string, const char*>>& renderModes,
                   std::vector<std::string>& written, const std::function<void(const std::string&)>& warn)
{
    textureNames.assign(model.textures.size(), std::string());
    std::set<int> usedTextures;
    for (const Triangle& t : weapon) usedTextures.insert(t.texture);
    std::set<std::string> taken;
    for (int index : usedTextures)
    {
        if (index < 0) continue;
        const StudioTexture& texture = model.textures[index];
        std::string name = safeFileName(texture.name);
        if (String_ToLower(name).size() < 5 || String_ToLower(name).substr(name.size() - 4) != ".bmp")
        {
            name += ".bmp";
        }
        if (name.size() <= 4)
        {
            name = "texture" + std::to_string(index) + ".bmp";
        }
        while (!taken.insert(String_ToLower(name)).second)
        {
            name = std::to_string(index) + "_" + name;
        }
        textureNames[index] = name;

        if (texture.pixels.empty() || texture.palette.size() < 768)
        {
            warn("Texture " + name + " has no pixel data in the model; the SMD names it but no BMP was written.");
            continue;
        }
        int width = texture.width, height = texture.height;
        const std::vector<uint8_t> resized = fitTexture(texture, width, height);
        const uint8_t* pixels = resized.empty() ? texture.pixels.data() : resized.data();

        uint8_t palette[1024];
        for (int i = 0; i < 256; i++)
        {
            palette[i * 4 + 0] = texture.palette[i * 3 + 2];
            palette[i * 4 + 1] = texture.palette[i * 3 + 1];
            palette[i * 4 + 2] = texture.palette[i * 3 + 0];
            palette[i * 4 + 3] = 0;
        }
        if (BMP::writeIndexed8(FileIO::fromPath(outFolder / FileIO::toPath(name)).c_str(), width, height, palette, pixels))
        {
            written.push_back(name);
        }
        else
        {
            warn("Could not write texture " + name);
        }
        if (texture.flags & STUDIO_NF_MASKED) renderModes.emplace_back(name, "masked");
        else if (texture.flags & STUDIO_NF_ADDITIVE) renderModes.emplace_back(name, "additive");
    }
}

std::vector<std::vector<const Triangle*>> splitIntoBodies(const std::vector<Triangle>& weapon)
{
    std::vector<std::vector<const Triangle*>> bodies;
    std::map<int, std::vector<const Triangle*>> bySource;
    for (const Triangle& t : weapon) bySource[t.source].push_back(&t);
    size_t room = 0;
    for (auto& [source, triangles] : bySource)
    {
        std::set<std::tuple<int, int, int>> distinct;
        for (const Triangle* t : triangles)
        {
            for (const Corner& c : t->corner)
            {
                distinct.emplace(static_cast<int>(std::lround(c.position.x * 1000.0f)),
                                 static_cast<int>(std::lround(c.position.y * 1000.0f)),
                                 static_cast<int>(std::lround(c.position.z * 1000.0f)));
            }
        }
        if (bodies.empty() || distinct.size() > room)
        {
            bodies.emplace_back();
            room = kMaxVertsPerBody;
        }
        bodies.back().insert(bodies.back().end(), triangles.begin(), triangles.end());
        room = distinct.size() > room ? 0 : room - distinct.size();
    }
    return bodies;
}

WeaponModelResult convertWeapon(const std::string& inputPath, const std::string& outputDir, const WeaponModelOptions& options,
                                OtherHand* otherHand)
{
    WeaponModelResult report;
    auto fail = [&](const std::string& message) {
        report.error = message;
        if (!otherHand) VortigauntLog::LogF("^1Error:^7 %s", message.c_str());
        return report;
    };
    auto warn = [&](const std::string& message) {
        report.warnings.push_back(message);
        if (!otherHand) VortigauntLog::LogF("^3Note:^7 %s", message.c_str());
    };

    const fs::path inputFile = FileIO::toPath(inputPath);
    if (!otherHand) VortigauntLog::LogF("^5Making p_ / w_ model from:^7 %s", FileIO::fromPath(inputFile.filename()).c_str());

    StudioModel model;
    if (!model.Load(inputPath))
    {
        return fail(model.GetError());
    }
    if (model.bones.empty() || model.bodyParts.empty())
    {
        return fail("The model has no geometry.");
    }

    // idle pose: frame 0 of the first sequence
    const std::vector<aiMatrix4x4> pose = model.BoneTransforms(0, 0);
    const HandBones handBones = findHandBones(model);

    // ---- hands, effects, weapon ----
    std::vector<Triangle> weapon;
    HandSkin hands;
    sortMeshes(model, pose, handBones, options.keepEffects, weapon, hands, report);
    if (weapon.empty())
    {
        return fail("Nothing was left after taking the hands out: every mesh of this model looks like hands or effects.");
    }

    // dual: mirror decided by game cstrike view models are left handed
    const bool dualMirror = otherHand ? otherHand->mirror
                                      : (options.mirror >= 0 ? options.mirror != 0 : options.gameDir == GameDir::cstrike);
    const bool twinWanted = otherHand || options.hold == WeaponType::Dual || options.hold == WeaponType::Auto;
    bool dual = selectWeaponPieces(weapon, model, handBones.armBone, hands.points, twinWanted,
                                   otherHand != nullptr || options.hold == WeaponType::Dual, dualMirror, otherHand != nullptr,
                                   report.removedParts);
    if (otherHand && !dual)
    {
        return fail("The second weapon was not found.");
    }
    if (options.hold == WeaponType::Dual && !dual && !otherHand)
    {
        warn("Dual was chosen, but the model does not have two weapons of a kind standing apart. It was made as a single gun.");
    }
    if (weapon.empty())
    {
        return fail("No connected weapon geometry was found.");
    }

    // weapon frame
    std::vector<aiVector3D> weaponPoints;
    weaponPoints.reserve(weapon.size() * 3);
    aiVector3D centre;
    for (const Triangle& t : weapon)
    {
        for (const Corner& c : t.corner)
        {
            weaponPoints.push_back(c.position);
            centre += c.position;
        }
    }
    centre /= static_cast<float>(weaponPoints.size());

    // axis only
    aiVector3D forward = longestAxis(weaponPoints, centre);
    aiVector3D muzzle;
    const bool hasMuzzle = !model.attachments.empty();
    if (hasMuzzle)
    {
        muzzle = pose[model.attachments[0].bone] * model.attachments[0].origin;
        if (dual)
        {
            for (const StudioAttachment& attachment : model.attachments)
            {
                const aiVector3D at = pose[attachment.bone] * attachment.origin;
                if ((at - centre).SquareLength() < (muzzle - centre).SquareLength()) muzzle = at;
            }
        }
    }
    float rear = 0, front = 0;
    extentAlong(weaponPoints, forward, rear, front);
    const float viewLength = front - rear;

    // holding hand
    const std::vector<Arm> arms = findArms(model, pose, hands, handBones.handByShape, weaponPoints);

    // trigger hand: the one nearer an end of the weapon
    const Arm* holder = nullptr;
    int holdingArms = 0;
    const float middle = centre * forward;
    for (const Arm& arm : arms)
    {
        if (arm.points.size() < 12 || arm.gap > 0.2f * viewLength) continue;
        holdingArms++;
        if (!holder || std::abs(arm.contact * forward - middle) > std::abs(holder->contact * forward - middle)) holder = &arm;
    }

    const int carrierHand = findCarrierHand(weapon, model, handBones.handByShape);

    aiVector3D grip;
    bool gripIsWrist = false;
    if (carrierHand >= 0)
    {
        grip = aiVector3D(pose[carrierHand].a4, pose[carrierHand].b4, pose[carrierHand].c4);
        gripIsWrist = true;
    }
    else if (holder)
    {
        grip = holder->contact;
    }
    else
    {
        // no hand: a quarter of the way along
        grip = centre + forward * (rear + 0.25f * viewLength - centre * forward);
        warn(hands.points.empty()
                 ? "No hands were found in this model, so nothing was removed and the grip position is a guess."
                 : "Neither hand touches the weapon in the idle pose, so the grip position is a guess.");
    }

    if (pointsBackwards(forward, rear, front, grip, carrierHand >= 0 || holder != nullptr, hasMuzzle, muzzle, centre))
    {
        forward = -forward;
        const float oldRear = rear;
        rear = -front;
        front = -oldRear;
    }

    // scale
    float scale = otherHand ? otherHand->scale : options.scale;
    if (scale <= 0.0f)
    {
        float measure = holder ? holder->handMeasure : 0.0f;
        if (measure <= 0.0f)
        {
            for (const Arm& arm : arms) measure = std::max(measure, arm.handMeasure);
        }
        if (measure > 0.0f)
        {
            scale = profileFor(options.gameDir).handMeasure / measure;
            report.scaleFromHands = true;
        }
        else
        {
            scale = 1.0f;
            warn("The size of the hands could not be measured, so the weapon keeps the view model's size. "
                 "Check it against a player and set the scale by hand if it is off.");
        }
    }
    report.scale = scale;


    const bool mirror = dual ? dualMirror : (options.mirror < 0 ? centre.y > 0.0f : options.mirror != 0);
    report.mirrored = mirror;

    aiMatrix3x3 level;
    aiMatrix3x3::FromToMatrix(forward, aiVector3D(1, 0, 0), level);

    WeaponType holdKind = otherHand ? otherHand->kind : options.hold;
    if (holdKind == WeaponType::Dual) holdKind = WeaponType::Gun;
    bool kindFromFileName = false;
    if (holdKind == WeaponType::Auto && dual) holdKind = WeaponType::Gun;
    if (holdKind == WeaponType::Auto && !dual)
    {
        holdKind = kindFromName(FileIO::fromPath(inputFile.stem()));
        kindFromFileName = holdKind != WeaponType::Auto;
    }
    if (holdKind == WeaponType::Auto)
    {
        // a muzzle attachment sits at the far end; knives have stray attachments
        const bool muzzleAtTip = hasMuzzle && muzzle * forward > front - 0.2f * viewLength;
        holdKind = detectWeaponType(levelPoints(weaponPoints, centre, level), scale, hasMuzzle, muzzleAtTip, holdingArms);
    }

    const GameDirProfile& profile = profileFor(options.gameDir);
    HoldPose hold = profile.gun;
    report.hold = "gun";
    if (holdKind == WeaponType::Knife)
    {
        hold = profile.knife;
        report.hold = "knife";
    }
    else if (holdKind == WeaponType::Grenade)
    {
        hold = profile.grenade;
        report.hold = "grenade";
        grip = centre;
    }
    else if (holdKind == WeaponType::Pack)
    {
        hold = profile.pack;
        report.hold = "pack";
        grip = centre;
    }
    else if (holdKind == WeaponType::Reference)
    {
        std::string error;
        if (!referenceHold(options.referenceModel, profile, hold, error))
        {
            return fail(error);
        }
        report.hold = "as in " + FileIO::fromPath(FileIO::toPath(options.referenceModel).filename());
    }

    // grenade. narrow end goes up
    const aiMatrix3x3 endForEnd(-1, 0, 0, 0, 1, 0, 0, 0, -1);
    if (holdKind == WeaponType::Grenade && grenadeIsUpsideDown(levelPoints(weaponPoints, centre, level)))
    {
        level = endForEnd * level;
        report.turnedByShape = true;
    }
    if (options.upsideDown)
    {
        level = endForEnd * level;
    }

    if (holdKind != WeaponType::Grenade)
    {
        standUpright(level, levelPoints(weaponPoints, centre, level), (dual || holdKind == WeaponType::Pack) ? 1.58f : 1.05f);

        // dual flip if the bulk ends up below the grip
        if (dual && (level * (centre - grip)).z < 0)
        {
            level = aiMatrix3x3(1, 0, 0, 0, -1, 0, 0, 0, -1) * level;
        }
    }

    if (holdKind == WeaponType::Pack && packFacesPalm(levelPoints(weaponPoints, centre, level), mirror))
    {
        level = aiMatrix3x3(-1, 0, 0, 0, -1, 0, 0, 0, 1) * level;
    }

    if (otherHand)
    {
        hold = leftHandHold(hold);
    }

    const aiVector3D gripInFrame = level * grip;
    aiMatrix4x4 toHand = aiMatrix4x4(level);
    toHand.a4 = -gripInFrame.x;
    toHand.b4 = -gripInFrame.y;
    toHand.c4 = -gripInFrame.z;
    aiMatrix4x4 shape;
    shape.a1 = scale;
    shape.b2 = mirror ? -scale : scale;
    shape.c3 = scale;
    toHand = shape * toHand;

    aiMatrix3x3 normalToHand = level;
    if (mirror)
    {
        aiMatrix3x3 flip;
        flip.b2 = -1;
        normalToHand = flip * level;
    }

    std::vector<aiVector3D> handSpace;
    handSpace.reserve(weaponPoints.size());
    for (Triangle& t : weapon)
    {
        for (Corner& c : t.corner)
        {
            c.position = toHand * c.position;
            c.normal = (normalToHand * c.normal).NormalizeSafe();
            handSpace.push_back(c.position);
        }
    }
    const aiVector3D muzzleInHand = toHand * muzzle;

    aiVector3D lo, hi;
    boundsOf(handSpace, lo, hi);
    report.length = hi.x - lo.x;
    report.triangles = static_cast<int>(weapon.size());
    report.dual = dual;

    // grip on the wrist bone. no palm offset
    if (gripIsWrist && holdKind != WeaponType::Grenade && holdKind != WeaponType::Pack)
    {
        hold.palm = aiVector3D(0, 0, 0);
    }
    const aiMatrix4x4 inHand = holdMatrix(hold);

    if (otherHand)
    {
        for (Triangle& t : weapon) t.hand = 1;
        otherHand->triangles = std::move(weapon);
        otherHand->inHand = inHand;
        otherHand->lo = lo;
        otherHand->hi = hi;
        otherHand->hasMuzzle = hasMuzzle;
        otherHand->muzzle = inHand * muzzleInHand;
        report.ok = true;
        return report;
    }

    OtherHand left;
    if (dual)
    {
        left.scale = scale;
        left.mirror = mirror;
        left.kind = holdKind;
        const WeaponModelResult second = convertWeapon(inputPath, outputDir, options, &left);
        if (second.ok && !left.triangles.empty())
        {
            weapon.insert(weapon.end(), left.triangles.begin(), left.triangles.end());
            report.triangles = static_cast<int>(weapon.size());
        }
        else
        {
            warn("The weapon for the left hand could not be placed (" + second.error + "); only the right hand's was made.");
            dual = false;
            report.dual = false;
        }
    }

    //output
    std::string baseName = FileIO::fromPath(inputFile.stem());
    if (String_ToLower(baseName).rfind("v_", 0) == 0)
    {
        baseName = baseName.substr(2);
    }
    baseName = safeFileName(baseName);
    if (baseName.empty())
    {
        baseName = "weapon";
    }

    const fs::path outFolder = FileIO::toPath(outputDir) / FileIO::toPath(baseName);
    std::error_code ec;
    fs::create_directories(outFolder, ec);
    if (ec)
    {
        return fail("Could not create the output folder: " + FileIO::fromPath(outFolder));
    }
    report.outputFolder = FileIO::fromPath(outFolder);

    std::vector<std::string> textureNames;
    std::vector<std::pair<std::string, const char*>> renderModes;
    writeTextures(model, weapon, outFolder, textureNames, renderModes, report.textures, warn);

    // bodies
    const std::vector<std::vector<const Triangle*>> bodies = splitIntoBodies(weapon);
    report.bodies = static_cast<int>(bodies.size());

   
    const bool flipWinding = !mirror;

    auto writeModel = [&](const std::string& prefix, const SkeletonBone* playerSkeleton, const aiMatrix4x4 toWorld[2],
                          const aiVector3D* attachment, const aiVector3D* leftAttachment, std::string& qcPathOut) {
        const std::string modelBase = prefix + baseName;
        QCFile qc;
        QCFileSettings settings;
        settings.ModelName = modelBase + ".mdl";
        settings.MeshName = modelBase;
        settings.Scale = 1.0f;
        settings.Fps = 30;
        qc.SetSettings(settings);

        const SkeletonBone* leftArm = (playerSkeleton && dual) ? profile.leftArm : nullptr;
        for (size_t b = 0; b < bodies.size(); b++)
        {
            const std::string smdName = b == 0 ? modelBase : modelBase + "_" + std::to_string(b + 1);
            writeSmd(outFolder / FileIO::toPath(smdName + ".smd"), playerSkeleton, leftArm, toWorld, bodies[b], textureNames, flipWinding);
            qc.AddBody(b == 0 ? "studio" : "studio" + std::to_string(b + 1), smdName);
        }
        for (const auto& mode : renderModes) qc.AddTextureRenderMode(mode.first, mode.second);
        if (attachment)
        {
            qc.AddAttachment(0, kWeaponBoneName, attachment->x, attachment->y, attachment->z);
        }
        if (leftAttachment)
        {
            qc.AddAttachment(1, kLeftWeaponBoneName, leftAttachment->x, leftAttachment->y, leftAttachment->z);
        }
        if (options.writeQC)
        {
            const fs::path qcPath = outFolder / FileIO::toPath(modelBase + ".qc");
            if (qc.Write(FileIO::fromPath(qcPath)))
            {
                qcPathOut = FileIO::fromPath(qcPath);
            }
            else
            {
                warn("Could not write the QC: " + qc.GetError());
            }
        }
    };

    auto boneMatrix = [](const SkeletonBone& b) {
        aiMatrix4x4 local;
        local.FromEulerAnglesXYZ(b.rotation[0], b.rotation[1], b.rotation[2]);
        local.a4 = b.position[0];
        local.b4 = b.position[1];
        local.c4 = b.position[2];
        return local;
    };

    // p_
    if (options.makeP)
    {
        std::vector<aiMatrix4x4> skeleton(kPlayerArmBones);
        for (int i = 0; i < kPlayerArmBones; i++)
        {
            const SkeletonBone& b = profile.skeleton[i];
            skeleton[i] = b.parent >= 0 ? skeleton[b.parent] * boneMatrix(b) : boneMatrix(b);
        }
        aiMatrix4x4 toWorld[2] = { skeleton[kHandBone] * inHand, skeleton[kHandBone] * inHand };
        if (dual)
        {
            aiMatrix4x4 leftHand = skeleton[kNeckBone];
            for (int i = 0; i < kLeftArmBones; i++) leftHand = leftHand * boneMatrix(profile.leftArm[i]);
            toWorld[1] = leftHand * left.inHand;
        }
        const aiVector3D attachment = inHand * muzzleInHand;
        writeModel("p_", profile.skeleton, toWorld, hasMuzzle ? &attachment : nullptr,
                   (dual && left.hasMuzzle) ? &left.muzzle : nullptr, report.pQcPath);
    }

    // w_
    if (options.makeW)
    {
        auto layDown = [](const aiVector3D& low, const aiVector3D& high) {
            aiMatrix4x4 lay;
            lay.b2 = 0; lay.b3 = -1;
            lay.c2 = 1; lay.c3 = 0;
            lay.a4 = -(low.x + high.x) * 0.5f;
            lay.b4 = (low.z + high.z) * 0.5f;
            lay.c4 = -low.y;
            return lay;
        };
        aiMatrix4x4 toWorld[2] = { layDown(lo, hi), layDown(lo, hi) };
        if (dual)
        {
            toWorld[1] = layDown(left.lo, left.hi);
            toWorld[1].b4 += (hi.z - lo.z) * 0.5f + (left.hi.z - left.lo.z) * 0.5f + 1.0f;
        }
        writeModel("w_", nullptr, toWorld, nullptr, nullptr, report.wQcPath);
    }

    VortigauntLog::LogF("  ^2hands removed:^7 %zu mesh(es)", report.removedHands.size());
    for (const std::string& s : report.removedHands) VortigauntLog::LogF("    %s", s.c_str());
    if (!report.removedEffects.empty())
    {
        VortigauntLog::LogF("  effects removed: %zu additive mesh(es)", report.removedEffects.size());
        for (const std::string& s : report.removedEffects) VortigauntLog::LogF("    %s", s.c_str());
    }
    if (!report.removedParts.empty())
    {
        std::string list;
        for (size_t i = 0; i < report.removedParts.size() && i < 8; i++) list += (i ? ", " : "") + report.removedParts[i];
        if (report.removedParts.size() > 8) list += ", ...";
        warn("Parts standing apart from the weapon in the idle pose were left out (bones: " + list +
             "). They are usually a spare magazine waiting off screen; if one belongs on the weapon, it has to be put back by hand.");
    }
    if (carrierHand >= 0)
    {
        VortigauntLog::LogF("  grip: the weapon is rigged to the hand bone %s", model.bones[carrierHand].name.c_str());
    }
    else if (holder)
    {
        VortigauntLog::LogF("  grip: where the %s hand holds the weapon%s", holder->contact.y > centre.y ? "left" : "right",
                            holder->handBone.empty() ? "" : (" (hand bone " + holder->handBone + ")").c_str());
    }
    VortigauntLog::LogF("  for: %s", profile.name);
    VortigauntLog::LogF("  held: %s%s", report.hold.c_str(),
                        options.hold != WeaponType::Auto ? "" : (kindFromFileName ? " (from the file name)" : " (chosen automatically)"));
    if (dual) VortigauntLog::LogF("  ^2dual:^7 one in each hand; the left one hangs from \"Bip01 L Hand\"");
    if (report.turnedByShape) VortigauntLog::LogF("  stood with its narrow end (the fuse) up");
    if (options.upsideDown) VortigauntLog::LogF("  turned upside down, as asked");
    VortigauntLog::LogF("  scale: %.3f%s", report.scale,
                        options.scale > 0 ? " (set by you)" : (report.scaleFromHands ? " (from the size of the hands)" : ""));
    VortigauntLog::LogF("  mirrored: %s", mirror ? "yes, the view model is built left handed" : "no");
    VortigauntLog::LogF("  weapon: %d triangles in %d body part(s), %.1f units long", report.triangles, report.bodies, report.length);
    if (report.length < 4.0f || report.length > 90.0f)
    {
        warn("That length is unusual for a weapon (the stock ones are 8 to 55 units). Set the scale by hand if it looks wrong.");
    }
    if (options.hold == WeaponType::Auto && !hasMuzzle && !kindFromFileName)
    {
        warn("The model has no muzzle attachment, so it was taken for a " + report.hold +
             " from its shape. If that is wrong, choose the kind of weapon yourself.");
    }
    if (report.triangles > 2500)
    {
        warn("View models are far more detailed than p_ and w_ models usually are (those are 500 to 1200 triangles). "
             "This one will work but costs more to draw for every player holding it.");
    }
    if (!report.pQcPath.empty()) VortigauntLog::LogF("  ^2p_ QC:^7 %s", report.pQcPath.c_str());
    if (!report.wQcPath.empty()) VortigauntLog::LogF("  ^2w_ QC:^7 %s", report.wQcPath.c_str());
    VortigauntLog::LogF("^2Done:^7 %s", report.outputFolder.c_str());

    report.ok = true;
    return report;
}

}  // namespace

bool WeaponModelConverter::IsViewModelFile(const std::string& path)
{
    if (FileIO::lowerExtension(path) != ".mdl")
    {
        return false;
    }
    return String_ToLower(FileIO::fromPath(FileIO::toPath(path).stem())).rfind("v_", 0) == 0;
}

std::vector<std::vector<std::vector<bool>>> WeaponModelConverter::FindHandMeshes(const StudioModel& model)
{
    const HandBones bones = findHandBones(model);
    auto textureName = [&model](const StudioMesh& mesh) {
        return mesh.texture >= 0 && mesh.texture < static_cast<int>(model.textures.size()) ? model.textures[mesh.texture].name : std::string();
    };
    std::vector<std::vector<std::vector<bool>>> hands(model.bodyParts.size());
    bool namedSkin = false;
    for (size_t p = 0; p < model.bodyParts.size(); ++p)
    {
        const StudioBodyPart& part = model.bodyParts[p];
        for (const StudioSubModel& sub : part.models)
        {
            std::vector<bool> meshes;
            for (const StudioMesh& mesh : sub.meshes)
            {
                const bool hand = !mesh.triangles.empty() && meshIsHands(part, sub, mesh, textureName(mesh), bones.armBone);
                namedSkin = namedSkin || (hand && getHandNames(textureName(mesh)));
                meshes.push_back(hand);
            }
            hands[p].push_back(std::move(meshes));
        }
    }
    // with textures named for the hands, a part of the gun held in the hand is not taken for them by its bones
    if (namedSkin)
    {
        for (size_t p = 0; p < model.bodyParts.size(); ++p)
        {
            const StudioBodyPart& part = model.bodyParts[p];
            for (size_t m = 0; m < part.models.size(); ++m)
            {
                const StudioSubModel& sub = part.models[m];
                for (size_t i = 0; i < sub.meshes.size(); ++i)
                {
                    if (hands[p][m][i] && !getHandNames(textureName(sub.meshes[i])) && !getHandNames(part.name) && !getHandNames(sub.name))
                        hands[p][m][i] = false;
                }
            }
        }
    }
    return hands;
}

ConvertResult WeaponModelConverter::ConvertFile(const std::string& inputPath, const std::string& outputDir)
{
    const WeaponModelResult report = Convert(inputPath, outputDir, m_options);
    ConvertResult result;
    result.ok = report.ok;
    result.error = report.error;
    result.outputFolder = report.outputFolder;
    if (!report.pQcPath.empty()) result.qcFiles.push_back(report.pQcPath);
    if (!report.wQcPath.empty()) result.qcFiles.push_back(report.wQcPath);
    return result;
}

WeaponModelResult WeaponModelConverter::Convert(const std::string& inputPath, const std::string& outputDir,
                                                const WeaponModelOptions& options)
{
    return convertWeapon(inputPath, outputDir, options, nullptr);
}
