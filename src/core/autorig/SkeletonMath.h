// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

// Skeleton arithmetic shared by the rigging code, and the one piece of naming
// knowledge it needs: how to tell that two bones from two games are the same
// bone.

#include "core/smd/SmdParser.h"

#include <assimp/types.h>

#include <string>
#include <vector>

namespace autorig
{

// SMD stores rotations as XYZ euler angles in radians.
// https://developer.valvesoftware.com/wiki/SMD
aiMatrix4x4 SmdEulerRotation(float rx, float ry, float rz);

// The angles SmdEulerRotation takes back from a rotation.
void SmdEulerAngles(const aiMatrix3x3& rotation, float& rx, float& ry, float& rz);

// World transform of every bone, parents resolved first.
std::vector<aiMatrix4x4> ComputeWorldTransforms(const std::vector<SmdBone>& bones);


std::string NormalizeBoneName(const std::string& name);

std::string CanonicalBoneName(const std::string& name);

// For each bone in `from`, the index of the bone in `to` that means the same
// joint, or -1. Matching is by canonical name.
std::vector<int> MatchBonesByName(const std::vector<SmdBone>& from, const std::vector<SmdBone>& to);


std::vector<int> MatchBonesByNameWithFallback(const std::vector<SmdBone>& from,
                                              const std::vector<SmdBone>& to,
                                              std::vector<std::string>* outUnmatched = nullptr);

// What moving a mesh between two skeletons needs to know per bone.
struct BoneMatch
{
    // Bone of `to` to place the geometry in, or -1.
    std::vector<int> target;

    std::vector<int> frame;

    std::vector<float> axialScale;

    std::vector<std::string> unmatched;
};

// `scale` is the overall size ratio applied to the mesh before placing it.
BoneMatch MatchBonesForPosing(const std::vector<SmdBone>& from,
                              const std::vector<SmdBone>& to,
                              float scale);

}  // namespace autorig
