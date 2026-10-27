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

// World transform of every bone, parents resolved first.
std::vector<aiMatrix4x4> ComputeWorldTransforms(const std::vector<SmdBone>& bones);

// Reduce a bone name to the form used for matching: lower case, the
// "ValveBiped." prefix dropped, underscores and runs of spaces collapsed to a
// single space. "ValveBiped.Bip01_L_UpperArm" and "Bip01 L UpperArm" meet here.
std::string NormalizeBoneName(const std::string& name);

// ...and then to one canonical spelling per joint. Half-Life and Sven Co-op
// skeletons name the limbs by position along the chain, Counter-Strike and CSO
// name them anatomically, and they mean exactly the same joints:
//
//     Bip01 L Arm   Bip01 L Arm1     Bip01 L Arm2    ->  clavicle, upper arm, forearm
//     Bip01 L Leg   Bip01 L Leg1                     ->  thigh, calf
//
// Without this a Half-Life model matched against a Counter-Strike skeleton
// loses every limb bone and the arms collapse onto the neck.
std::string CanonicalBoneName(const std::string& name);

// For each bone in `from`, the index of the bone in `to` that means the same
// joint, or -1. Matching is by canonical name.
std::vector<int> MatchBonesByName(const std::vector<SmdBone>& from, const std::vector<SmdBone>& to);

// As above, but a bone with no namesake follows the nearest ancestor that has
// one, so a skeleton carrying extra helper bones still maps completely.
// Bones with no matched ancestor at all stay -1 and are named in outUnmatched.
std::vector<int> MatchBonesByNameWithFallback(const std::vector<SmdBone>& from,
                                              const std::vector<SmdBone>& to,
                                              std::vector<std::string>* outUnmatched = nullptr);

// What moving a mesh between two skeletons needs to know per bone.
struct BoneMatch
{
    // Bone of `to` to place the geometry in, or -1.
    std::vector<int> target;

    // Bone of `from` whose local frame the geometry should be measured in. For
    // a matched bone that is the bone itself; for one that fell back to an
    // ancestor it is THAT ancestor, not the bone. Measuring in the bone's own
    // frame and then placing the result in an ancestor's frame would shift the
    // geometry by the offset between them - a jaw skinned to a "mouth" bone
    // with no namesake would jump off the head.
    std::vector<int> frame;

    // Stretch along the bone (local X, which is where Biped rigs put the child)
    // that makes this bone's child land exactly on the matching child of the
    // target skeleton. 1 where there is no matched child to line up with.
    //
    // Without it every limb is carried rigidly and the pieces stop meeting:
    // a forearm a fifth shorter than the reference's leaves the hand floating
    // a couple of units off the wrist.
    std::vector<float> axialScale;

    std::vector<std::string> unmatched;
};

// `scale` is the overall size ratio applied to the mesh before placing it.
BoneMatch MatchBonesForPosing(const std::vector<SmdBone>& from,
                              const std::vector<SmdBone>& to,
                              float scale);

}  // namespace autorig
