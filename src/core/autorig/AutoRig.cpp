#include "AutoRig.h"
#include "core/smd/SmdParser.h"
#include <limits>
#include <cmath>
#include <algorithm>
#include <unordered_map>


bool AutoRig::LoadSkeleton(const std::string& smdPath) {
    SmdParser parser;
    if (!parser.Parse(smdPath)) {
        m_error = parser.GetError();
        return false;
    }
    
    SetSkeleton(parser.GetBones());
    return true;
}

void AutoRig::SetSkeleton(const std::vector<SmdBone>& bones) {
    m_bones = bones;
    
    // Build child connection tree for topological geometric distance calculation
    m_boneChildren.clear();
    m_boneChildren.resize(m_bones.size());
    for (size_t i = 0; i < m_bones.size(); i++) {
        int parentIdx = m_bones[i].parentIndex;
        if (parentIdx >= 0 && parentIdx < static_cast<int>(m_bones.size())) {
            m_boneChildren[parentIdx].push_back(static_cast<int>(i));
        }
    }
    
    CalculateBoneWorldPositions();
    
    // Calculate hierarchy depth for each bone
    m_boneDepths.resize(m_bones.size(), 0);
    for (size_t i = 0; i < m_bones.size(); i++) {
        int depth = 0;
        int cur = static_cast<int>(i);
        while (m_bones[cur].parentIndex >= 0) {
            depth++;
            cur = m_bones[cur].parentIndex;
        }
        m_boneDepths[i] = depth;
    }
    
    // Calculate bone lengths (distance from parent to this bone in world space)
    m_boneLengths.resize(m_bones.size(), 0.0f);
    for (size_t i = 0; i < m_bones.size(); i++) {
        int parentIdx = m_bones[i].parentIndex;
        if (parentIdx >= 0 && parentIdx < static_cast<int>(m_boneWorldPositions.size())) {
            aiVector3D d = m_boneWorldPositions[i] - m_boneWorldPositions[parentIdx];
            m_boneLengths[i] = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        }
    }
}

aiMatrix4x4 AutoRig::CreateRotationMatrix(float rx, float ry, float rz) const 
{

    // Create rotation matrix from euler angles (XYZ order, as used by GoldSrc/SMD )  
    // SMD uses XYZ Euler angles (rotate around X, then Y, then Z)
    // https://developer.valvesoftware.com/wiki/SMD
    float cx = std::cos(rx), sx = std::sin(rx);
    float cy = std::cos(ry), sy = std::sin(ry);
    float cz = std::cos(rz), sz = std::sin(rz);

    // Assimp aiMatrix4x4 is row-major:
    // a1 a2 a3 a4   (row 1)
    // b1 b2 b3 b4   (row 2)
    // c1 c2 c3 c4   (row 3)
    // d1 d2 d3 d4   (row 4) - translation goes in d1,d2,d3
    
    aiMatrix4x4 mat;
    mat.a1 = cy * cz;
    mat.a2 = cz * sx * sy - cx * sz;
    mat.a3 = cx * cz * sy + sx * sz;
    mat.a4 = 0;

    mat.b1 = cy * sz;
    mat.b2 = cx * cz + sx * sy * sz;
    mat.b3 = -cz * sx + cx * sy * sz;
    mat.b4 = 0;

    mat.c1 = -sy;
    mat.c2 = cy * sx;
    mat.c3 = cx * cy;
    mat.c4 = 0;

    mat.d1 = 0;
    mat.d2 = 0;
    mat.d3 = 0;
    mat.d4 = 1;

    return mat;
}

void AutoRig::CalculateBoneWorldPositions() {
    m_boneWorldPositions.clear();
    m_boneWorldPositions.resize(m_bones.size());

    // Calculate world transform for each bone
    std::vector<aiMatrix4x4> worldTransforms(m_bones.size());

    for (size_t i = 0; i < m_bones.size(); i++) {
        const SmdBone& bone = m_bones[i];

        // rotation + translation
        aiMatrix4x4 localRotation = CreateRotationMatrix(bone.rotX, bone.rotY, bone.rotZ);
        
        // Translation matrix, its  goes in a4,b4,c4
        aiMatrix4x4 localTranslation;
        localTranslation.a4 = bone.posX;
        localTranslation.b4 = bone.posY;
        localTranslation.c4 = bone.posZ;
        
        // Local transform = Translation * Rotation (apply rotation first, then translation)
        aiMatrix4x4 localTransform = localTranslation * localRotation;

        if (bone.parentIndex < 0) {
            // Root bone - local transform is world transform
            worldTransforms[i] = localTransform;
        } else if (bone.parentIndex < static_cast<int>(i)) {
            // Child bone - world = parent_world * local
            worldTransforms[i] = worldTransforms[bone.parentIndex] * localTransform;
        } else {
            // Parent not processed yet (shouldn't happen with proper ordering)
            worldTransforms[i] = localTransform;
        }

        // Extract world position from transform matrix
        // aiMatrix4x4 is row-major, translation is in a4,b4,c4 (column 4)
        m_boneWorldPositions[i].x = worldTransforms[i].a4;
        m_boneWorldPositions[i].y = worldTransforms[i].b4;
        m_boneWorldPositions[i].z = worldTransforms[i].c4;
    }
}

aiVector3D AutoRig::GetBoneWorldPosition(int boneIndex) const {
    if (boneIndex >= 0 && boneIndex < static_cast<int>(m_boneWorldPositions.size())) {
        return m_boneWorldPositions[boneIndex];
    }
    return aiVector3D(0, 0, 0);
}



// Calculate point to line segment distance squared
static float PointToSegmentDistSq(const aiVector3D& point, const aiVector3D& segStart, 
                                   const aiVector3D& segEnd, float& outT) {
    aiVector3D seg = segEnd - segStart;
    aiVector3D toPoint = point - segStart;
    
    float segLenSq = seg.x * seg.x + seg.y * seg.y + seg.z * seg.z;
    
    if (segLenSq < 0.0001f) {
        // Degenerate segment (start == end)
        outT = 0.0f;
        return toPoint.x * toPoint.x + toPoint.y * toPoint.y + toPoint.z * toPoint.z;
    }
    
    // Project point onto line, clamped to segment
    float t = (toPoint.x * seg.x + toPoint.y * seg.y + toPoint.z * seg.z) / segLenSq;
    outT = std::max(0.0f, std::min(1.0f, t));
    
    // Closest point on segment
    aiVector3D closest;
    closest.x = segStart.x + outT * seg.x;
    closest.y = segStart.y + outT * seg.y;
    closest.z = segStart.z + outT * seg.z;
    
    // Distance from point to closest
    aiVector3D diff = point - closest;
    return diff.x * diff.x + diff.y * diff.y + diff.z * diff.z;
}

int AutoRig::FindNearestBone(float x, float y, float z, bool useDepthPenalty) const {
    if (m_boneWorldPositions.empty()) {
        return 0;
    }

    int bestBone = 0;
    float minScore = std::numeric_limits<float>::max();
    
    aiVector3D vertex(x, y, z);
    constexpr float DEPTH_PENALTY = 0.03f;

    for (size_t p = 0; p < m_boneWorldPositions.size(); p++) {
        const aiVector3D& parentPos = m_boneWorldPositions[p];
        const std::vector<int>& children = m_boneChildren[p];
        
        if (children.empty()) {
            // Leaf or lone bone: evaluate point distance directly
            float dx = x - parentPos.x;
            float dy = y - parentPos.y;
            float dz = z - parentPos.z;
            float distSq = dx * dx + dy * dy + dz * dz;
            
            float score = distSq;
            if (useDepthPenalty) {
                score *= (1.0f + m_boneDepths[p] * DEPTH_PENALTY);
            }
            
            if (score < minScore - 0.00001f) {
                minScore = score;
                bestBone = static_cast<int>(p);
            } else if (std::abs(score - minScore) <= 0.00001f) {
                // Tie-breaker for overlapping bones
                if (!useDepthPenalty && m_boneDepths[p] > m_boneDepths[bestBone]) {
                    bestBone = static_cast<int>(p);
                } else if (useDepthPenalty && m_boneDepths[p] < m_boneDepths[bestBone]) {
                    bestBone = static_cast<int>(p);
                }
            }
        } else {
            // Internal bone: evaluate segment distance to ALL children
            for (int c : children) {
                const aiVector3D& childPos = m_boneWorldPositions[c];
                float t = 0.0f;
                float distSq = PointToSegmentDistSq(vertex, parentPos, childPos, t);
                
                // Which joint on the segment is the vertex closer to?
                // Split the segment at 50% to give half the volume to the child.
                // This perfectly handles inverted hierarchies (like FPS hands where Elbow -> Shoulder).
                int candidateBone = (t < 0.5f) ? static_cast<int>(p) : c;
                
                float score = distSq;
                if (useDepthPenalty) {
                    score *= (1.0f + m_boneDepths[candidateBone] * DEPTH_PENALTY);
                }
                
                if (score < minScore - 0.00001f) {
                    minScore = score;
                    bestBone = candidateBone;
                } else if (std::abs(score - minScore) <= 0.00001f) {
                    // Tie-breaker for overlapping bones
                    if (!useDepthPenalty && m_boneDepths[candidateBone] > m_boneDepths[bestBone]) {
                        // For weapons: deeper child wins ties (e.g. v_weapon.knife wins over v_weapon)
                        bestBone = candidateBone;
                    } else if (useDepthPenalty && m_boneDepths[candidateBone] < m_boneDepths[bestBone]) {
                        // For players: shallower parent wins ties
                        bestBone = candidateBone;
                    }
                }
            }
        }
    }

    return bestBone;
}

std::vector<int> AutoRig::RigVertices(const std::vector<float>& vertexPositions, bool useDepthPenalty) {
    std::vector<int> boneIndices;
    
    size_t vertexCount = vertexPositions.size() / 3;
    boneIndices.reserve(vertexCount);

    for (size_t i = 0; i < vertexCount; i++) {
        float x = vertexPositions[i * 3 + 0];
        float y = vertexPositions[i * 3 + 1];
        float z = vertexPositions[i * 3 + 2];
        
        boneIndices.push_back(FindNearestBone(x, y, z, useDepthPenalty));
    }

    return boneIndices;
}

std::vector<int> AutoRig::RigTriangles(const std::vector<float>& vertexPositions, int smoothingPasses, bool useDepthPenalty) {
    std::vector<int> boneIndices = RigVertices(vertexPositions, useDepthPenalty);
    
    size_t vertexCount = boneIndices.size();
    if (vertexCount < 3 || smoothingPasses <= 0) {
        return boneIndices;
    }


    // In SMD, every 3 consecutive vertices form a triangle
    // Build neighbor list: for each vertex, which other vertices share a triangle
    size_t triCount = vertexCount / 3;
    std::vector<std::vector<int>> neighbors(vertexCount);
    
    for (size_t t = 0; t < triCount; t++) {
        int v0 = static_cast<int>(t * 3 + 0);
        int v1 = static_cast<int>(t * 3 + 1);
        int v2 = static_cast<int>(t * 3 + 2);
        
        neighbors[v0].push_back(v1);
        neighbors[v0].push_back(v2);
        neighbors[v1].push_back(v0);
        neighbors[v1].push_back(v2);
        neighbors[v2].push_back(v0);
        neighbors[v2].push_back(v1);
    }
    
    // Also connect vertices that share the same position (welded vertices across triangles)
    // This is critical for SMD meshes where the same spatial vertex appears in multiple triangles
    std::unordered_map<uint64_t, std::vector<int>> positionMap;
    
    auto hashPosition = [](float x, float y, float z) -> uint64_t {
        int32_t ix = static_cast<int32_t>(x * 100.0f);
        int32_t iy = static_cast<int32_t>(y * 100.0f);
        int32_t iz = static_cast<int32_t>(z * 100.0f);
        uint64_t h = static_cast<uint64_t>(static_cast<uint32_t>(ix));
        h = (h << 20) ^ static_cast<uint64_t>(static_cast<uint32_t>(iy));
        h = (h << 20) ^ static_cast<uint64_t>(static_cast<uint32_t>(iz));
        return h;
    };
    
    for (size_t i = 0; i < vertexCount; i++) {
        float vx = vertexPositions[i * 3 + 0];
        float vy = vertexPositions[i * 3 + 1];
        float vz = vertexPositions[i * 3 + 2];
        uint64_t key = hashPosition(vx, vy, vz);
        positionMap[key].push_back(static_cast<int>(i));
    }
    
    // Connect all vertices at the same position as neighbors
    for (auto& [key, indices] : positionMap) {
        for (size_t a = 0; a < indices.size(); a++) {
            for (size_t b = a + 1; b < indices.size(); b++) {
                neighbors[indices[a]].push_back(indices[b]);
                neighbors[indices[b]].push_back(indices[a]);
            }
        }
    }
    



    // Each pass: if a vertex's bone assignment is the minority among its neighbors,
    // switch it to the majority bone. This propagates correct assignments across
    // joint boundaries, similar to heat diffusion on the mesh surface.
    std::vector<int> newIndices(vertexCount);
    
    for (int pass = 0; pass < smoothingPasses; pass++) {
        bool changed = false;
        
        for (size_t i = 0; i < vertexCount; i++) {
            if (neighbors[i].empty()) {
                newIndices[i] = boneIndices[i];
                continue;
            }
            
            // Count bone votes from neighbors (include self)
            std::unordered_map<int, int> votes;
            votes[boneIndices[i]] = 2;  // Self gets 2 votes (weighted anchor)
            
            for (int neighborIdx : neighbors[i]) {
                votes[boneIndices[neighborIdx]]++;
            }
            
            // Find the bone with most votes
            int bestBone = boneIndices[i];
            int bestCount = 0;
            for (auto& [bone, count] : votes) {
                if (count > bestCount) {
                    bestCount = count;
                    bestBone = bone;
                }
            }
            
            if (bestBone != boneIndices[i]) {
                changed = true;
            }
            newIndices[i] = bestBone;
        }
        
        boneIndices = newIndices;
        
        // Early exit if nothing changed
        if (!changed) break;
    }
    
    return boneIndices;
}

std::vector<int> AutoRig::RigMesh(const aiMesh* mesh) {
    std::vector<int> boneIndices;
    
    if (!mesh) {
        return boneIndices;
    }

    boneIndices.reserve(mesh->mNumVertices);

    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        const aiVector3D& pos = mesh->mVertices[i];
        boneIndices.push_back(FindNearestBone(pos.x, pos.y, pos.z));
    }

    return boneIndices;
}

std::vector<std::vector<int>> AutoRig::RigScene(const aiScene* scene) {
    std::vector<std::vector<int>> result;
    
    if (!scene) {
        return result;
    }

    result.reserve(scene->mNumMeshes);

    for (unsigned int i = 0; i < scene->mNumMeshes; i++) {
        result.push_back(RigMesh(scene->mMeshes[i]));
    }

    return result;
}

std::vector<aiMatrix4x4> AutoRig::CalculateWorldTransforms(const std::vector<SmdBone>& bones) const {
    std::vector<aiMatrix4x4> worldTransforms(bones.size());
    
    for (size_t i = 0; i < bones.size(); i++) {
        const SmdBone& bone = bones[i];
        
        // Create local transform matrix
        aiMatrix4x4 localRotation = CreateRotationMatrix(bone.rotX, bone.rotY, bone.rotZ);
        
        aiMatrix4x4 localTranslation;
        localTranslation.a4 = bone.posX;
        localTranslation.b4 = bone.posY;
        localTranslation.c4 = bone.posZ;
        
        aiMatrix4x4 localTransform = localTranslation * localRotation;
        
        if (bone.parentIndex < 0) {
            worldTransforms[i] = localTransform;
        } else if (bone.parentIndex < static_cast<int>(i)) {
            worldTransforms[i] = worldTransforms[bone.parentIndex] * localTransform;
        } else {
            worldTransforms[i] = localTransform;
        }
    }
    
    return worldTransforms;
}

aiVector3D AutoRig::RotateAroundPivot(const aiVector3D& point, const aiVector3D& pivot, 
                                          const aiMatrix4x4& rotation) const {
    aiVector3D local = point - pivot;
    
    // matrix * vector
    aiVector3D rotated;
    rotated.x = rotation.a1 * local.x + rotation.a2 * local.y + rotation.a3 * local.z;
    rotated.y = rotation.b1 * local.x + rotation.b2 * local.y + rotation.b3 * local.z;
    rotated.z = rotation.c1 * local.x + rotation.c2 * local.y + rotation.c3 * local.z;
    
    return rotated + pivot;
}