// Copyright (C) 2026 iQuit (https://github.com/iQuitt)
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include "core/smd/SmdParser.h"

#include <assimp/quaternion.h>
#include <assimp/vector3.h>

#include <cstdint>
#include <string>
#include <vector>

// GTA San Andreas animations (IFP, ANP3)
struct IfpKey
{
    float time = 0;  // seconds
    aiQuaternion rotation;
    aiVector3D translation;
    bool hasTranslation = false;
};

struct IfpBone
{
    std::string name;
    int boneId = 0;
    std::vector<IfpKey> keys;
};

struct IfpAnimation
{
    std::string name;
    float duration = 0;
    std::vector<IfpBone> bones;
};

std::vector<IfpAnimation> ReadIfp(const std::vector<uint8_t>& data, std::string* error = nullptr);

// one animation as an SMD on a ped skeleton DffConverter wrote (its reference SMD's bones),
// scale the GTA metre in units as the model was made with
bool WriteIfpSmd(const IfpAnimation& animation, const std::vector<SmdBone>& rest, const std::string& outPath, int fps = 30,
                 float scale = 39.37f);
