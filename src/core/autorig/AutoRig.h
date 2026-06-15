#pragma once

#include "core/smd/SmdParser.h"
#include <assimp/scene.h>
#include <vector>
#include <cmath>
#include <unordered_set>

// Generic topology-based automatic rigging for GoldSrc models

// Still have little issues, gonna fix later


class AutoRig {
public:
    AutoRig() = default;
    ~AutoRig() = default;

    // Load skeleton from a reference SMD file
    bool LoadSkeleton(const std::string& smdPath);
    
    // Load skeleton from already parsed SMD
    void SetSkeleton(const std::vector<SmdBone>& bones);
    

    // assigns bone indices to each vertex
    // Returns vector of bone indices, one per vertex
    // vertexPositions: flat array of vertex positions (x,y,z for each vertex)
    // vertexNormals: flat array of vertex normals (nx,ny,nz for each vertex) - optional, pass empty to disable normal filtering
    // useDepthPenalty: If true, uses hierarchy depth to prevent helper bones from stealing parent areas (good for players, bad for hands)
    std::vector<int> RigVertices(const std::vector<float>& vertexPositions, const std::vector<float>& vertexNormals, bool useDepthPenalty = true);
    

    // Rig vertices with triangle topology smoothing (recommended for SMD)
    // Uses mesh adjacency to smooth bone assignments at joint boundaries
    // vertexPositions: flat array (x,y,z per vertex), vertices are in triangle order (every 3 = 1 triangle)
    // vertexNormals: flat array of vertex normals (nx,ny,nz per vertex) - optional, pass empty to disable
    // smoothingPasses: number of mesh-topology smoothing iterations (default 3)
    // useDepthPenalty: see RigVertices
    std::vector<int> RigTriangles(const std::vector<float>& vertexPositions, const std::vector<float>& vertexNormals, int smoothingPasses = 3, bool useDepthPenalty = true);
    
    // Rig an Assimp mesh
    std::vector<int> RigMesh(const aiMesh* mesh);
    
    // Rig all meshes in an Assimp scene
    // Returns bone indices per mesh, per vertex
    std::vector<std::vector<int>> RigScene(const aiScene* scene);
    
    // Get bone world position (after computing transforms)
    aiVector3D GetBoneWorldPosition(int boneIndex) const;
    
    // Get skeleton bones
    const std::vector<SmdBone>& GetBones() const { return m_bones; }
    
    // Check if skeleton is loaded
    bool HasSkeleton() const { return !m_bones.empty(); }
    
    // Get error message
    const std::string& GetError() const { return m_error; }

    // Set bones to ignore during rigging
    void SetIgnoredBones(const std::unordered_set<int>& ignoredBones);

    // Resolve an ignored bone to its nearest non-ignored ancestor
    int ResolveNonIgnoredBone(int boneIndex) const;

private:
    std::vector<SmdBone> m_bones;
    std::vector<std::vector<int>> m_boneChildren;
    std::vector<aiVector3D> m_boneWorldPositions;
    std::vector<int> m_boneDepths;      
    std::vector<float> m_boneLengths;    // Distance from parent to this bone
    std::string m_error;
    std::unordered_set<int> m_ignoredBones;
    
    // Calculate world-space positions for all bones
    void CalculateBoneWorldPositions();
    
    // Calculate world transforms for a given skeleton
    std::vector<aiMatrix4x4> CalculateWorldTransforms(const std::vector<SmdBone>& bones) const;
    
    // Create rotation matrix from euler angles
    aiMatrix4x4 CreateRotationMatrix(float rx, float ry, float rz) const;
    
    // Find the nearest bone for a given vertex position
    // nx,ny,nz: vertex normal direction for directional filtering (set to 0,0,0 to disable)
    int FindNearestBone(float x, float y, float z, float nx, float ny, float nz, bool useDepthPenalty = true) const;
    
    // Rotate a point around a pivot
    aiVector3D RotateAroundPivot(const aiVector3D& point, const aiVector3D& pivot, 
                                  const aiMatrix4x4& rotation) const;
};
