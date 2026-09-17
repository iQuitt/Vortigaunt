#pragma once

#include <assimp/types.h>  // aiVector3D together with its operators (vector3.inl)

#include <string>
#include <vector>

// Bone heat diffusion weighting for GoldSrc rigging.
//
// This is the algorithm Blender exposes as "Parent With Automatic Weights"
// (Baran & Popovic heat equilibrium), specialised for GoldSrc: the MDL format
// stores a single bone index per vertex, so the smooth weight field produced by
// the solver is collapsed to one bone per vertex at the end - exactly what
// "Limit Total = 1" + "Normalize All" does by hand in Blender.
//
// Unlike plain nearest-bone matching this is topology and occlusion aware, so a
// vertex on the inner thigh does not get stolen by the opposite leg just because
// it happens to be close in world space.

namespace heatrig {

// One deformer segment of the skeleton. A GoldSrc bone is a single point, so a
// bone's influence region is described by the segments running from it to each
// of its children (leaf bones get a virtual extension segment). Several segments
// may map to the same boneIndex.
struct BoneSegment {
    aiVector3D start;      // the bone's own world position
    aiVector3D end;        // child bone world position
    int        boneIndex;  // bone this segment grants influence to
};

struct Options {
    // Vertices closer together than this are welded into one solver vertex.
    // Also the tolerance used to keep duplicated SMD corners consistent.
    float weldEpsilon = 0.001f;

    // Conjugate gradient limits for the diffusion solve.
    int   maxIterations = 400;
    float tolerance = 1e-5f;

    // Cast rays to reject bones that are hidden behind geometry. This is what
    // stops weights from bleeding across limbs. Disable only for debugging.
    bool  useVisibility = true;

    // Heat source strength. Blender uses 1.0; larger values pin vertices harder
    // to their nearest bone and produce tighter, less diffuse regions.
    float heatStrength = 1.0f;

    // Pull the boundary between two bones towards the joint they share. 0 keeps
    // the raw argmax boundary, 1 snaps it as close to the pivot as the mesh
    // allows. Reduces the size of the crease when the joint bends.
    float pivotSnap = 1.0f;

    // Majority-vote cleanup passes over the mesh graph, applied only where the
    // solver itself was undecided, so clear regions are never disturbed.
    int   cleanupPasses = 4;

    // A vertex is considered undecided when the best and runner-up bone weights
    // differ by less than this.
    float undecidedMargin = 0.05f;

    // Connected regions smaller than this, that the solver was also unsure
    // about, are absorbed into the bone surrounding them. This is what removes
    // the isolated specks helper bones win in the middle of someone else's
    // territory - a twist bone catching a single vertex at the wrist, say.
    // A region the solver was confident about is kept whatever its size, so
    // small but genuine bones such as finger tips survive. 0 disables.
    int minRegionSize = 6;
};

struct Result {
    bool ok = false;
    std::string error;

    // One bone index per *input* vertex, in the order they were handed in.
    std::vector<int> boneIndices;

    // Per input vertex: best weight minus runner-up weight. Near zero means the
    // vertex sits on a seam. Useful for diagnostics and for the caller to decide
    // how much it trusts an assignment.
    std::vector<float> confidence;

    // Diagnostics.
    int weldedVertexCount = 0;
    int triangleCount = 0;
    int solvedBoneCount = 0;
    int unreachedVertexCount = 0;  // vertices no bone could see
    int absorbedRegionCount = 0;   // speckles folded into their surroundings
};

// positions: flat xyz per vertex, in triangle order (every 3 vertices = 1 triangle).
// segments:  skeleton description, see BoneSegment.
// Returns one bone index per input vertex. Vertices sharing a position always
// receive the same bone, so the compiled model cannot tear apart.
Result Solve(const std::vector<float>& positions,
             const std::vector<BoneSegment>& segments,
             const Options& options);

} // namespace heatrig
