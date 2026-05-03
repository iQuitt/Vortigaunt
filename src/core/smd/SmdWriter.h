#pragma once

#include <string>
#include <vector>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <filesystem>
#include <cmath>
#include <algorithm>
#include <functional>
#include <unordered_map>
#include <set>
#include "assimp/scene.h"
#include "SmdParser.h"  // For SmdBone, SmdTriangle, SmdVertex structs

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifdef max
#undef max
#endif
#ifdef min
#undef min
#endif

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * SmdWriter - Writes Valve SMD (StudioMdl Data) files
 * 
 * Supports exporting:
 * - Reference meshes (geometry + skeleton) from Assimp scenes
 * - Animation sequences (skeleton animation only)
 * - Manual skeleton with parsed SMD triangles (for AutoRig)
 * 
 */
class SmdWriter
{
public:
    SmdWriter() : m_mirrorUVY(true), m_applyRootRotation(false), m_useTickBasedTiming(false), m_progressCallback(nullptr), m_materialName("default.bmp") {}
    ~SmdWriter() {}

    // Settings
    void SetMirrorUVY(bool enabled) { m_mirrorUVY = enabled; }
    void SetProgressCallback(std::function<void()> callback) { m_progressCallback = callback; }
    void SetApplyRootRotation(bool enabled) { m_applyRootRotation = enabled; }
    void SetUseTickBasedTiming(bool enabled) { m_useTickBasedTiming = enabled; }
    void SetHiddenMeshIndices(const std::set<unsigned int>& hiddenIndices) { m_hiddenMeshIndices = hiddenIndices; }
    void AddHiddenMeshIndex(unsigned int index) { m_hiddenMeshIndices.insert(index); }
    void ClearHiddenMeshIndices() { m_hiddenMeshIndices.clear(); }

    // for AutoRig
    void SetSkeleton(const std::vector<SmdBone>& bones) { m_manualBones = bones; }
    void SetMaterialName(const std::string& name) { m_materialName = name; }
    const std::string& GetError() const { return m_error; }

    bool ExportMeshSMD(const aiScene* scene, const std::string& outputPath);
    bool ExportAnimationSMD(const aiScene* scene, int animIndex, const std::string& outputPath);
    bool ExportAllAnimationsSMD(const aiScene* scene, const std::string& baseOutputPath);

    bool ExportSmdTriangles(const std::string& outputPath,
        const std::vector<SmdTriangle>& triangles,
        const std::vector<int>& boneIndices);
    bool ExportMeshWithManualSkeleton(const std::string& outputPath,
        const aiMesh* mesh,
        const std::vector<int>& boneIndices);
    bool ExportSceneWithManualSkeleton(const std::string& outputPath,
        const aiScene* scene,
        const std::vector<std::vector<int>>& boneIndicesPerMesh);

    aiVector3D InterpolatePosition(aiNodeAnim* nodeAnim, double time);
    aiQuaternion InterpolateRotation(aiNodeAnim* nodeAnim, double time);
    aiMatrix4x4 GetGlobalTransform(aiNode* node);

    void WriteHeader(std::ofstream& file);
    std::string SanitizeNodeName(const std::string& name);
    void WriteNodes(std::ofstream& file, const aiScene* scene);
    void WriteSkeletonReference(std::ofstream& file, const aiScene* scene);
    void WriteSkeletonAnimation(std::ofstream& file, const aiScene* scene, int animIndex);
    void WriteTriangles(std::ofstream& file, const aiScene* scene);

    void WriteManualNodes(std::ofstream& file);
    void WriteManualSkeleton(std::ofstream& file);
    void WriteManualTriangles(std::ofstream& file, const std::vector<SmdTriangle>& triangles, const std::vector<int>& boneIndices);
    void WriteManualTrianglesMesh(std::ofstream& file, const aiMesh* mesh, const std::vector<int>& boneIndices);
    void WriteManualTrianglesScene(std::ofstream& file, const aiScene* scene, const std::vector<std::vector<int>>& boneIndicesPerMesh);
    std::string GetMaterialNameFromMesh(const aiScene* scene, const aiMesh* mesh) const;

    void BuildNodeList(aiNode* node);
    int GetNodeIndex(aiNode* node);
    aiNode* FindNodeByName(aiNode* node, const std::string& name);
    void WriteVector3(std::ofstream& file, const aiVector3D& vec);
    void ConvertQuaternionToEuler(const aiQuaternion& quat, aiVector3D& euler);
    std::string GetTextureName(const aiScene* scene, unsigned int materialIndex);

private:
    std::vector<aiNode*> m_nodeList;
    bool m_mirrorUVY;
    bool m_applyRootRotation;
    bool m_useTickBasedTiming;
    std::set<unsigned int> m_hiddenMeshIndices;
    std::function<void()> m_progressCallback;
    
    // Manual skeleton support
    std::vector<SmdBone> m_manualBones;
    std::string m_materialName;
    std::string m_error;
};

