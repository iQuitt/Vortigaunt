#include "AutoRig.h"
#include "core/smd/SmdParser.h"
#include <limits>
#include <cmath>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <queue>


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
        if (parentIdx >= 0 && parentIdx < static_cast<int>(m_bones.size()) && parentIdx != static_cast<int>(i)) {
            m_boneChildren[parentIdx].push_back(static_cast<int>(i));
        }
    }
    
    CalculateBoneWorldPositions();
    
    // Calculate hierarchy depth for each bone
    m_boneDepths.resize(m_bones.size(), 0);
    for (size_t i = 0; i < m_bones.size(); i++) {
        int depth = 0;
        int cur = static_cast<int>(i);
        int limit = 0;
        while (cur >= 0 && cur < static_cast<int>(m_bones.size()) && limit < 256) {
            int parentIdx = m_bones[cur].parentIndex;
            if (parentIdx < 0 || parentIdx >= static_cast<int>(m_bones.size()) || parentIdx == cur) {
                break;
            }
            depth++;
            cur = parentIdx;
            limit++;
        }
        m_boneDepths[i] = depth;
    }
    
    // Calculate bone lengths (distance from parent to this bone in world space)
    m_boneLengths.resize(m_bones.size(), 0.0f);
    for (size_t i = 0; i < m_bones.size(); i++) {
        int parentIdx = m_bones[i].parentIndex;
        if (parentIdx >= 0 && parentIdx < static_cast<int>(m_boneWorldPositions.size()) && parentIdx != static_cast<int>(i)) {
            aiVector3D d = m_boneWorldPositions[i] - m_boneWorldPositions[parentIdx];
            m_boneLengths[i] = std::sqrt(d.x * d.x + d.y * d.y + d.z * d.z);
        }
    }
}

void AutoRig::SetIgnoredBones(const std::unordered_set<int>& ignoredBones) {
    m_ignoredBones = ignoredBones;
}

int AutoRig::ResolveNonIgnoredBone(int boneIndex) const {
    if (m_ignoredBones.empty()) {
        return boneIndex;
    }
    
    int current = boneIndex;
    int limit = 0;
    while (current >= 0 && current < static_cast<int>(m_bones.size()) && limit < 256) {
        if (m_ignoredBones.find(current) == m_ignoredBones.end()) {
            return current;
        }
        current = m_bones[current].parentIndex;
        limit++;
    }
    
    // Fallback: find the first non-ignored bone
    for (size_t i = 0; i < m_bones.size(); ++i) {
        if (m_ignoredBones.find(static_cast<int>(i)) == m_ignoredBones.end()) {
            return static_cast<int>(i);
        }
    }
    
    return boneIndex;
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
    std::vector<bool> computed(m_bones.size(), false);

    auto resolveTransform = [&](auto& self, int idx) -> void {
        if (computed[idx]) return;

        const SmdBone& bone = m_bones[idx];
        aiMatrix4x4 localRotation = CreateRotationMatrix(bone.rotX, bone.rotY, bone.rotZ);
        
        aiMatrix4x4 localTranslation;
        localTranslation.a4 = bone.posX;
        localTranslation.b4 = bone.posY;
        localTranslation.c4 = bone.posZ;
        
        aiMatrix4x4 localTransform = localTranslation * localRotation;

        int parentIdx = bone.parentIndex;
        if (parentIdx < 0 || parentIdx >= static_cast<int>(m_bones.size()) || parentIdx == idx) {
            worldTransforms[idx] = localTransform;
        } else {
            self(self, parentIdx);
            worldTransforms[idx] = worldTransforms[parentIdx] * localTransform;
        }
        computed[idx] = true;
    };

    for (size_t i = 0; i < m_bones.size(); i++) {
        resolveTransform(resolveTransform, static_cast<int>(i));
    }

    for (size_t i = 0; i < m_bones.size(); i++) {
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

int AutoRig::FindNearestBone(float x, float y, float z, float nx, float ny, float nz, bool useDepthPenalty) const {
    if (m_boneWorldPositions.empty()) {
        return 0;
    }

    int bestBone = 0;
    float minScore = std::numeric_limits<float>::max();
    
    aiVector3D vertex(x, y, z);
    constexpr float DEPTH_PENALTY = 0.005f; // it was 0.03
    
    // Normal filtering
    bool hasNormal = (nx * nx + ny * ny + nz * nz) > 0.001f;

    for (size_t p = 0; p < m_boneWorldPositions.size(); p++) {
        const aiVector3D& parentPos = m_boneWorldPositions[p];
        const std::vector<int>& children = m_boneChildren[p];

        if (m_bones[p].parentIndex < 0 && !children.empty()) {
            float rootDistSq = parentPos.x * parentPos.x + parentPos.y * parentPos.y + parentPos.z * parentPos.z;
            if (rootDistSq < 1.0f) {
                continue; // skip origin-root bones
            }
        }
        
        if (children.empty()) {
            // Leaf bone: project virtual child segment in the direction of its parent bone
            int parentIdx = m_bones[p].parentIndex;
            aiVector3D dir(0, 0, 1);
            float len = 25.0f;
            if (parentIdx >= 0 && parentIdx < static_cast<int>(m_boneWorldPositions.size())) {
                dir = parentPos - m_boneWorldPositions[parentIdx];
                if (dir.SquareLength() < 0.0001f) {
                    dir = aiVector3D(0, 0, 1);
                } else {
                    dir.Normalize();
                }
            }
            
            aiVector3D virtualChildPos = parentPos + dir * len;
            float t = 0.0f;
            float distSq = PointToSegmentDistSq(vertex, parentPos, virtualChildPos, t);
            
            float score = distSq;
            if (useDepthPenalty) {
                score *= (1.0f + m_boneDepths[p] * DEPTH_PENALTY);
            }
            
            

            
            if (score < minScore - 0.00001f) {
                minScore = score;
                bestBone = static_cast<int>(p);
            } else if (std::abs(score - minScore) <= 0.00001f) {
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
                
                // Map the segment p-c entirely to candidate parent bone p
                int candidateBone = static_cast<int>(p);
                
                float score = distSq;
                if (useDepthPenalty) {
                    score *= (1.0f + m_boneDepths[candidateBone] * DEPTH_PENALTY);
                }
                
                

                
                if (score < minScore - 0.00001f) {
                    minScore = score;
                    bestBone = candidateBone;
                } else if (std::abs(score - minScore) <= 0.00001f) {
                    if (!useDepthPenalty && m_boneDepths[candidateBone] > m_boneDepths[bestBone]) {
                        bestBone = candidateBone;
                    } else if (useDepthPenalty && m_boneDepths[candidateBone] < m_boneDepths[bestBone]) {
                        bestBone = candidateBone;
                    }
                }
            }
        }
    }

    return ResolveNonIgnoredBone(bestBone);
}

std::vector<int> AutoRig::RigVertices(const std::vector<float>& vertexPositions, const std::vector<float>& vertexNormals, bool useDepthPenalty) {
    std::vector<int> boneIndices;
    
    size_t vertexCount = vertexPositions.size() / 3;
    bool hasNormals = (vertexNormals.size() == vertexPositions.size());
    boneIndices.reserve(vertexCount);

    for (size_t i = 0; i < vertexCount; i++) {
        float x = vertexPositions[i * 3 + 0];
        float y = vertexPositions[i * 3 + 1];
        float z = vertexPositions[i * 3 + 2];
        
        float nx = 0.0f, ny = 0.0f, nz = 0.0f;
        if (hasNormals) {
            nx = vertexNormals[i * 3 + 0];
            ny = vertexNormals[i * 3 + 1];
            nz = vertexNormals[i * 3 + 2];
        }
        
        boneIndices.push_back(FindNearestBone(x, y, z, nx, ny, nz, useDepthPenalty));
    }

    return boneIndices;
}

std::vector<int> AutoRig::RigTriangles(const std::vector<float>& vertexPositions, const std::vector<float>& vertexNormals, int smoothingPasses, bool useDepthPenalty) {
    std::vector<int> boneIndices = RigVertices(vertexPositions, vertexNormals, useDepthPenalty);
    
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
    


    {
        std::vector<int> regionId(vertexCount, -1);
        std::vector<int> regionBone;
        std::vector<int> regionSize;
        // Store centroid per region
        std::vector<float> regionCentroidX, regionCentroidY, regionCentroidZ;
        int numRegions = 0;
        
        for (size_t i = 0; i < vertexCount; i++) {
            if (regionId[i] >= 0) continue;
            int bone = boneIndices[i];
            std::queue<int> q;
            q.push(static_cast<int>(i));
            regionId[i] = numRegions;
            int sz = 0;
            float cx = 0, cy = 0, cz = 0;
            while (!q.empty()) {
                int v = q.front(); q.pop();
                sz++;
                cx += vertexPositions[v * 3 + 0];
                cy += vertexPositions[v * 3 + 1];
                cz += vertexPositions[v * 3 + 2];
                for (int n : neighbors[v]) {
                    if (regionId[n] < 0 && boneIndices[n] == bone) {
                        regionId[n] = numRegions;
                        q.push(n);
                    }
                }
            }
            regionBone.push_back(bone);
            regionSize.push_back(sz);
            regionCentroidX.push_back(cx / sz);
            regionCentroidY.push_back(cy / sz);
            regionCentroidZ.push_back(cz / sz);
            numRegions++;
        }
        
        // For each bone, find its largest region
        std::unordered_map<int, int> largestRegionForBone;
        std::unordered_map<int, int> largestSizeForBone;
        for (int r = 0; r < numRegions; r++) {
            int bone = regionBone[r];
            if (regionSize[r] > largestSizeForBone[bone]) {
                largestSizeForBone[bone] = regionSize[r];
                largestRegionForBone[bone] = r;
            }
        }
        
        for (int r = 0; r < numRegions; r++) {
            int bone = regionBone[r];
            if (r == largestRegionForBone[bone]) continue;
            if (regionSize[r] >= largestSizeForBone[bone]) continue;
            
            int mainRegion = largestRegionForBone[bone];
            
            float dx = regionCentroidX[r] - regionCentroidX[mainRegion];
            float dy = regionCentroidY[r] - regionCentroidY[mainRegion];
            float dz = regionCentroidZ[r] - regionCentroidZ[mainRegion];
            float interIslandDist = std::sqrt(dx * dx + dy * dy + dz * dz);
            
            float boneLen = (bone < static_cast<int>(m_boneLengths.size())) ? m_boneLengths[bone] : 5.0f;
            float threshold = std::max(10.0f, boneLen * 2.0f);
            
            if (interIslandDist < threshold) continue; // too close
            
            std::unordered_map<int, int> neighborBoneCounts;
            for (size_t i = 0; i < vertexCount; i++) {
                if (regionId[i] != r) continue;
                for (int n : neighbors[i]) {
                    if (regionId[n] != r) {
                        neighborBoneCounts[boneIndices[n]]++;
                    }
                }
            }
            
            if (neighborBoneCounts.empty()) continue;
            
            int bestBone = bone;
            int bestCount = 0;
            for (auto& [b, c] : neighborBoneCounts) {
                if (c > bestCount) {
                    bestCount = c;
                    bestBone = b;
                }
            }
            
            for (size_t i = 0; i < vertexCount; i++) {
                if (regionId[i] == r) {
                    boneIndices[i] = bestBone;
                }
            }
        }
    }

    std::vector<int> newIndices(vertexCount);
    
    for (int pass = 0; pass < smoothingPasses; pass++) {
        bool changed = false;
        
        for (size_t i = 0; i < vertexCount; i++) {
            if (neighbors[i].empty()) {
                newIndices[i] = boneIndices[i];
                continue;
            }
            
            std::unordered_map<int, int> votes;
            votes[boneIndices[i]] = 2;
            
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
        float nx = 0.0f, ny = 0.0f, nz = 0.0f;
        if (mesh->mNormals) {
            nx = mesh->mNormals[i].x;
            ny = mesh->mNormals[i].y;
            nz = mesh->mNormals[i].z;
        }
        boneIndices.push_back(FindNearestBone(pos.x, pos.y, pos.z, nx, ny, nz));
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
    std::vector<bool> computed(bones.size(), false);
    
    auto resolveTransform = [&](auto& self, int idx) -> void {
        if (computed[idx]) return;
        
        const SmdBone& bone = bones[idx];
        aiMatrix4x4 localRotation = CreateRotationMatrix(bone.rotX, bone.rotY, bone.rotZ);
        
        aiMatrix4x4 localTranslation;
        localTranslation.a4 = bone.posX;
        localTranslation.b4 = bone.posY;
        localTranslation.c4 = bone.posZ;
        
        aiMatrix4x4 localTransform = localTranslation * localRotation;
        
        int parentIdx = bone.parentIndex;
        if (parentIdx < 0 || parentIdx >= static_cast<int>(bones.size()) || parentIdx == idx) {
            worldTransforms[idx] = localTransform;
        } else {
            self(self, parentIdx);
            worldTransforms[idx] = worldTransforms[parentIdx] * localTransform;
        }
        computed[idx] = true;
    };
    
    for (size_t i = 0; i < bones.size(); i++) {
        resolveTransform(resolveTransform, static_cast<int>(i));
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