#include "SmdWriter.h"
#include "core/VortigauntLog.h"
#include "utils/FileIO.h"
#include "utils/util.hpp"
#include <functional>

// ExportMeshSMD - Export mesh as reference SMD (contains geometry and skeleton)
bool SmdWriter::ExportMeshSMD(const aiScene* scene, const std::string& outputPath)
{
    if (!scene) return false;

    std::ofstream file(FileIO::toPath(outputPath));
    if (!file.is_open()) {
        std::cerr << "Failed to create SMD file: " << outputPath << std::endl;
        return false;
    }

    // Build node list for consistent indexing
    m_nodeList.clear();
    if (scene->mRootNode) {
        BuildNodeList(scene->mRootNode);
    }

    WriteHeader(file);
    WriteNodes(file, scene);
    WriteSkeletonReference(file, scene);
    WriteTriangles(file, scene);

    file.close();
    std::cout << "Exported reference SMD: " << outputPath << std::endl;
    return true;
}

// ExportAnimationSMD - Export animation as animation SMD (contains only skeleton animation)
bool SmdWriter::ExportAnimationSMD(const aiScene* scene, int animIndex, const std::string& outputPath)
{
    if (!scene || animIndex >= (int)scene->mNumAnimations) return false;

    // Call callback at start to keep UI responsive
    if (m_progressCallback) {
        m_progressCallback();
    }

    std::ofstream file(FileIO::toPath(outputPath));
    if (!file.is_open()) {
        std::cerr << "Failed to create animation SMD file: " << outputPath << std::endl;
        return false;
    }

    // Build node list for consistent indexing
    m_nodeList.clear();
    if (scene->mRootNode) {
        BuildNodeList(scene->mRootNode);
    }

    // Call callback after node building
    if (m_progressCallback) {
        m_progressCallback();
    }

    WriteHeader(file);
    WriteNodes(file, scene);
    WriteSkeletonAnimation(file, scene, animIndex);

    file.close();
    return true;
}


// ExportAllAnimationsSMD - Export multiple animations as separate SMD files (legacy)
bool SmdWriter::ExportAllAnimationsSMD(const aiScene* scene, const std::string& baseOutputPath)
{
    if (!scene || scene->mNumAnimations == 0) return false;

    std::filesystem::path basePath = FileIO::toPath(baseOutputPath);
    std::string baseDir = String_UTF16toUTF8(basePath.parent_path().generic_u16string());
    std::string baseName = String_UTF16toUTF8(basePath.stem().generic_u16string());

    for (unsigned int i = 0; i < scene->mNumAnimations; i++) {
        std::string animName = scene->mAnimations[i]->mName.C_Str();
        if (animName.empty()) {
            animName = "anim_" + std::to_string(i);
        }

        std::string animPath;
        if (baseDir.empty()) {
            animPath = baseName + "_" + animName + ".smd";
        }
        else {
            animPath = baseDir + "/" + baseName + "_" + animName + ".smd";
        }

        VortigauntLog::LogF("  Exporting Animation: %s", animName.c_str());

        if (!ExportAnimationSMD(scene, i, animPath)) {
            return false;
        }
    }

    return true;
}

// InterpolatePosition - Interpolate position keyframes
aiVector3D SmdWriter::InterpolatePosition(aiNodeAnim* nodeAnim, double time)
{
    if (!nodeAnim || nodeAnim->mNumPositionKeys == 0)
        return aiVector3D(0.0f, 0.0f, 0.0f);

    if (nodeAnim->mNumPositionKeys == 1)
        return nodeAnim->mPositionKeys[0].mValue;

    // Clamp before/after range
    if (time <= nodeAnim->mPositionKeys[0].mTime)
        return nodeAnim->mPositionKeys[0].mValue;
    if (time >= nodeAnim->mPositionKeys[nodeAnim->mNumPositionKeys - 1].mTime)
        return nodeAnim->mPositionKeys[nodeAnim->mNumPositionKeys - 1].mValue;

    unsigned int left = 0;
    unsigned int right = nodeAnim->mNumPositionKeys - 1;
    while (right - left > 1) {
        unsigned int mid = (left + right) / 2;
        if (nodeAnim->mPositionKeys[mid].mTime <= time) {
            left = mid;
        }
        else {
            right = mid;
        }
    }
    unsigned int index = left;

    const aiVectorKey& key0 = nodeAnim->mPositionKeys[index];
    const aiVectorKey& key1 = nodeAnim->mPositionKeys[index + 1];

    double delta = key1.mTime - key0.mTime;
    double factor = (delta > 0.0) ? (time - key0.mTime) / delta : 0.0;
    factor = std::clamp(factor, 0.0, 1.0);

    return key0.mValue * static_cast<float>(1.0 - factor) +
        key1.mValue * static_cast<float>(factor);
}

// InterpolateRotation - Interpolate rotation keyframes
aiQuaternion SmdWriter::InterpolateRotation(aiNodeAnim* nodeAnim, double time)
{
    if (!nodeAnim || nodeAnim->mNumRotationKeys == 0)
        return aiQuaternion(1.0f, 0.0f, 0.0f, 0.0f); // identity (w,x,y,z)

    if (nodeAnim->mNumRotationKeys == 1)
        return nodeAnim->mRotationKeys[0].mValue;

    if (time <= nodeAnim->mRotationKeys[0].mTime)
        return nodeAnim->mRotationKeys[0].mValue;
    if (time >= nodeAnim->mRotationKeys[nodeAnim->mNumRotationKeys - 1].mTime)
        return nodeAnim->mRotationKeys[nodeAnim->mNumRotationKeys - 1].mValue;

    unsigned int left = 0;
    unsigned int right = nodeAnim->mNumRotationKeys - 1;
    while (right - left > 1) {
        unsigned int mid = (left + right) / 2;
        if (nodeAnim->mRotationKeys[mid].mTime <= time) {
            left = mid;
        }
        else {
            right = mid;
        }
    }
    unsigned int index = left;

    const aiQuatKey& key0 = nodeAnim->mRotationKeys[index];
    const aiQuatKey& key1 = nodeAnim->mRotationKeys[index + 1];

    double delta = key1.mTime - key0.mTime;
    double factor = (delta > 0.0) ? (time - key0.mTime) / delta : 0.0;
    factor = std::clamp(factor, 0.0, 1.0);

    aiQuaternion out;
    aiQuaternion::Interpolate(out, key0.mValue, key1.mValue, static_cast<float>(factor));
    out.Normalize();
    return out;
}

// GetGlobalTransform - Compute global transform for a node by walking up the parent chain
aiMatrix4x4 SmdWriter::GetGlobalTransform(aiNode* node)
{
    aiMatrix4x4 m;
    aiMatrix4x4::Translation(aiVector3D(0, 0, 0), m); // identity
    aiNode* cur = node;
    while (cur)
    {
        m = cur->mTransformation * m;
        cur = cur->mParent;
    }
    return m;
}

// WriteHeader - Write SMD header
void SmdWriter::WriteHeader(std::ofstream& file)
{
    file << "version 1\n";
}

// SanitizeNodeName - Sanitize node name for SMD format
std::string SmdWriter::SanitizeNodeName(const std::string& name)
{
    std::string result = name;

    // Replace problematic characters
    for (size_t i = 0; i < result.length(); i++) {
        char c = result[i];
        // Replace quotes, newlines, tabs, and other problematic chars
        // Also parentheses, brackets, braces, and dots which studiomdl dislikes
        if (c == '"' || c == '\n' || c == '\r' || c == '\t' || 
            c == '(' || c == ')' || c == '[' || c == ']' || 
            c == '{' || c == '}' || c == ':' || c == ',' || c == '.') {
            result[i] = '_';
        }
        // Replace backslashes (escape character issues)
        else if (c == '\\') {
            result[i] = '/';
        }
    }

    // If name is empty after sanitization, give it a default
    if (result.empty()) {
        result = "bone";
    }

    return result;
}

void SmdWriter::WriteNodes(std::ofstream& file, const aiScene* scene)
{
    file << "nodes\n";

    for (size_t i = 0; i < m_nodeList.size(); i++) {
        aiNode* node = m_nodeList[i];
        std::string nodeName = SanitizeNodeName(node->mName.C_Str());

        // Find parent index
        int parentIndex = -1;
        if (node->mParent) {
            parentIndex = GetNodeIndex(node->mParent);
        }

        file << i << " \"" << nodeName << "\" " << parentIndex << "\n";
    }

    file << "end\n";
}

// WriteSkeletonReference - Write skeleton section for reference mesh (time 0)
void SmdWriter::WriteSkeletonReference(std::ofstream& file, const aiScene* scene)
{
    file << "skeleton\n";
    file << "time 0\n";

    for (size_t i = 0; i < m_nodeList.size(); i++) {
        aiNode* node = m_nodeList[i];

        // Extract position and rotation from transformation matrix
        aiVector3D position, scaling;
        aiQuaternion rotation;
        node->mTransformation.Decompose(scaling, rotation, position);

        // Use unified euler conversion to ensure consistency with animation frames
        aiVector3D eulerAngles;
        ConvertQuaternionToEuler(rotation, eulerAngles);

        // Fix coordinate system: GLB is Y-up, SMD/Source is Z-up
        if (m_applyRootRotation && !node->mParent) {
            eulerAngles.x -= static_cast<float>(M_PI / 2.0);
        }

        file << i << " ";
        WriteVector3(file, position);
        file << " ";
        WriteVector3(file, eulerAngles);
        file << "\n";
    }

    file << "end\n";
}

// WriteSkeletonAnimation - Write skeleton section for animation
void SmdWriter::WriteSkeletonAnimation(std::ofstream& file, const aiScene* scene, int animIndex)
{
    file << "skeleton\n";

    aiAnimation* anim = scene->mAnimations[animIndex];

    // Get ticks per second (default to 25 if not specified)
    double ticksPerSecond = anim->mTicksPerSecond > 0 ? anim->mTicksPerSecond : 25.0;

    // Determine animation duration from channels
    double maxTime = 0.0;
    bool hasAnyKeys = false;
    for (unsigned int c = 0; c < anim->mNumChannels; ++c) {
        aiNodeAnim* ch = anim->mChannels[c];
        if (ch->mNumPositionKeys > 0) {
            maxTime = std::max(maxTime, ch->mPositionKeys[ch->mNumPositionKeys - 1].mTime);
            hasAnyKeys = true;
        }
        if (ch->mNumRotationKeys > 0) {
            maxTime = std::max(maxTime, ch->mRotationKeys[ch->mNumRotationKeys - 1].mTime);
            hasAnyKeys = true;
        }
    }

    // Convert to seconds if using tick-based timing
    double maxTimeSeconds = m_useTickBasedTiming ? (maxTime / ticksPerSecond) : maxTime;

    if (!hasAnyKeys) {
        // Fallback: single bind-pose frame
        file << "time 0\n";
        for (size_t nodeIdx = 0; nodeIdx < m_nodeList.size(); ++nodeIdx) {
            aiNode* node = m_nodeList[nodeIdx];
            aiVector3D pos, scale;
            aiQuaternion rot;
            node->mTransformation.Decompose(scale, rot, pos);

            aiVector3D euler;
            ConvertQuaternionToEuler(rot, euler);
            if (!node->mParent) {
                euler.x -= static_cast<float>(M_PI / 2.0);
            }

            file << nodeIdx << " ";
            WriteVector3(file, pos);
            file << " ";
            WriteVector3(file, euler);
            file << "\n";
        }
        file << "end\n";
        return;
    }

    std::unordered_map<std::string, aiNodeAnim*> channelNameMap;
    channelNameMap.reserve(anim->mNumChannels);
    for (unsigned int c = 0; c < anim->mNumChannels; ++c) {
        channelNameMap[anim->mChannels[c]->mNodeName.C_Str()] = anim->mChannels[c];
    }

    std::vector<aiNodeAnim*> nodeChannelMap(m_nodeList.size(), nullptr);
    for (size_t nodeIdx = 0; nodeIdx < m_nodeList.size(); ++nodeIdx) {
        std::string nodeName = m_nodeList[nodeIdx]->mName.C_Str();
        auto it = channelNameMap.find(nodeName);
        if (it != channelNameMap.end()) {
            nodeChannelMap[nodeIdx] = it->second;
        }
    }

    std::vector<aiVector3D> bindPositions(m_nodeList.size());
    std::vector<aiQuaternion> bindRotations(m_nodeList.size());
    for (size_t nodeIdx = 0; nodeIdx < m_nodeList.size(); ++nodeIdx) {
        aiVector3D bindScale;
        m_nodeList[nodeIdx]->mTransformation.Decompose(bindScale, bindRotations[nodeIdx], bindPositions[nodeIdx]);
        bindRotations[nodeIdx].Normalize();
    }

    const double fps = 30.0;
    int totalFrames = static_cast<int>(std::ceil(maxTimeSeconds * fps)) + 1;
    if (totalFrames <= 0) totalFrames = 1;

    for (int frame = 0; frame < totalFrames; ++frame) {
        double timeInSeconds = static_cast<double>(frame) / fps;
        if (timeInSeconds > maxTimeSeconds) {
            timeInSeconds = maxTimeSeconds;
        }
        double interpolationTime = m_useTickBasedTiming ? (timeInSeconds * ticksPerSecond) : timeInSeconds;

        file << "time " << frame << "\n";

        for (size_t nodeIdx = 0; nodeIdx < m_nodeList.size(); ++nodeIdx) {
            aiNodeAnim* nodeAnim = nodeChannelMap[nodeIdx];

            aiVector3D position = bindPositions[nodeIdx];
            aiQuaternion finalRot = bindRotations[nodeIdx];

            if (nodeAnim) {
                if (nodeAnim->mNumPositionKeys > 0) {
                    position = InterpolatePosition(nodeAnim, interpolationTime);
                }
                if (nodeAnim->mNumRotationKeys > 0) {
                    finalRot = InterpolateRotation(nodeAnim, interpolationTime);
                }
            }

            finalRot.Normalize();

            // Filter out micro-fluctuations in position
            const float POS_THRESHOLD = 0.0001f;
            position.x = std::abs(position.x) < POS_THRESHOLD ? 0.0f : position.x;
            position.y = std::abs(position.y) < POS_THRESHOLD ? 0.0f : position.y;
            position.z = std::abs(position.z) < POS_THRESHOLD ? 0.0f : position.z;

            aiVector3D eulerAngles;
            ConvertQuaternionToEuler(finalRot, eulerAngles);

            if (m_applyRootRotation && !m_nodeList[nodeIdx]->mParent) {
                eulerAngles.x -= static_cast<float>(M_PI / 2.0);
            }

            file << nodeIdx << " ";
            WriteVector3(file, position);
            file << " ";
            WriteVector3(file, eulerAngles);
            file << "\n";
        }

        // Call progress callback every 20 frames to keep UI responsive
        if (m_progressCallback && (frame % 20 == 0)) {
            m_progressCallback();
        }
    }

    file << "end\n";
}

// WriteTriangles - Write triangles section (mesh geometry)
void SmdWriter::WriteTriangles(std::ofstream& file, const aiScene* scene)
{
    file << "triangles\n";

    for (unsigned int meshIdx = 0; meshIdx < scene->mNumMeshes; meshIdx++) {
        // Skip hidden meshes
        if (m_hiddenMeshIndices.find(meshIdx) != m_hiddenMeshIndices.end()) {
            continue;
        }

        aiMesh* mesh = scene->mMeshes[meshIdx];

        std::string textureName;
        
        if (mesh->mMaterialIndex < scene->mNumMaterials) {
            aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
            aiString materialName;
            if (material->Get(AI_MATKEY_NAME, materialName) == AI_SUCCESS) {
                std::string matName = materialName.C_Str();
                if (!matName.empty() && matName != "DefaultMaterial" && matName.find("Material") != 0 && matName.find("material") != 0) {
                    textureName = matName;
                }
            }
        }
        
        if (textureName.empty()) {
            textureName = mesh->mName.C_Str();
        }
        
        if (textureName.empty() || textureName.find("Mesh") == 0 || textureName.find("meshes") == 0) {
            std::function<const aiNode*(const aiNode*, unsigned int)> findNode = [&](const aiNode* node, unsigned int idx) -> const aiNode* {
                for (unsigned int i = 0; i < node->mNumMeshes; ++i) {
                    if (node->mMeshes[i] == idx) return node;
                }
                for (unsigned int i = 0; i < node->mNumChildren; ++i) {
                    const aiNode* found = findNode(node->mChildren[i], idx);
                    if (found) return found;
                }
                return nullptr;
            };
            const aiNode* node = findNode(scene->mRootNode, meshIdx);
            if (node) {
                std::string nodeName = node->mName.C_Str();
                if (!nodeName.empty()) {
                    textureName = nodeName;
                }
            }
        }
        
        if (textureName.empty()) {
            textureName = "default";
        }
        
        std::replace(textureName.begin(), textureName.end(), ' ', '_');
        std::replace(textureName.begin(), textureName.end(), '/', '_');
        std::replace(textureName.begin(), textureName.end(), '\\', '_');
        textureName += ".bmp";

        for (unsigned int faceIdx = 0; faceIdx < mesh->mNumFaces; faceIdx++) {
            aiFace& face = mesh->mFaces[faceIdx];
            if (face.mNumIndices != 3) continue; // Only triangles

            file << textureName << "\n";

            for (unsigned int vertIdx = 0; vertIdx < 3; vertIdx++) {
                unsigned int index = face.mIndices[vertIdx];

                // Find parent bone (highest weight)
                int parentBone = 0;
                if (mesh->mNumBones > 0) {
                    float maxWeight = 0.0f;
                    for (unsigned int boneIdx = 0; boneIdx < mesh->mNumBones; boneIdx++) {
                        aiBone* bone = mesh->mBones[boneIdx];
                        for (unsigned int weightIdx = 0; weightIdx < bone->mNumWeights; weightIdx++) {
                            if (bone->mWeights[weightIdx].mVertexId == index) {
                                if (bone->mWeights[weightIdx].mWeight > maxWeight) {
                                    maxWeight = bone->mWeights[weightIdx].mWeight;
                                    aiNode* boneNode = FindNodeByName(scene->mRootNode, bone->mName.C_Str());
                                    if (boneNode) {
                                        parentBone = GetNodeIndex(boneNode);
                                    }
                                }
                            }
                        }
                    }
                }

                file << parentBone << " ";

                // Position
                if (mesh->mVertices) {
                    aiVector3D pos = mesh->mVertices[index];
                    if (m_applyRootRotation) {
                        float newY = -pos.z;
                        float newZ = pos.y;
                        pos.y = newY;
                        pos.z = newZ;
                    }
                    WriteVector3(file, pos);
                }
                else {
                    file << "0.0 0.0 0.0";
                }
                file << " ";

                // Normal
                if (mesh->mNormals) {
                    aiVector3D norm = mesh->mNormals[index];
                    if (m_applyRootRotation) {
                        float newY = -norm.z;
                        float newZ = norm.y;
                        norm.y = newY;
                        norm.z = newZ;
                    }
                    WriteVector3(file, norm);
                }
                else {
                    file << "0.0 0.0 1.0";
                }
                file << " ";

                // UV coordinates
                if (mesh->mTextureCoords[0]) {
                    file << std::fixed << std::setprecision(6)
                        << mesh->mTextureCoords[0][index].x << " ";
                    if (m_mirrorUVY) {
                        file << (1.0f - mesh->mTextureCoords[0][index].y);
                    }
                    else {
                        file << mesh->mTextureCoords[0][index].y;
                    }
                }
                else {
                    file << "0.0 0.0";
                }

                // Bone weights
                if (mesh->mNumBones > 0) {
                    std::vector<std::pair<int, float>> weights;
                    for (unsigned int boneIdx = 0; boneIdx < mesh->mNumBones; boneIdx++) {
                        aiBone* bone = mesh->mBones[boneIdx];
                        for (unsigned int weightIdx = 0; weightIdx < bone->mNumWeights; weightIdx++) {
                            if (bone->mWeights[weightIdx].mVertexId == index) {
                                aiNode* boneNode = FindNodeByName(scene->mRootNode, bone->mName.C_Str());
                                if (boneNode) {
                                    int boneIndex = GetNodeIndex(boneNode);
                                    weights.push_back({ boneIndex, bone->mWeights[weightIdx].mWeight });
                                }
                            }
                        }
                    }

                    if (!weights.empty()) {
                        file << " " << weights.size();
                        for (const auto& weight : weights) {
                            file << " " << weight.first << " " << std::fixed << std::setprecision(6) << weight.second;
                        }
                    }
                }

                file << "\n";
            }
        }
    }

    file << "end\n";
}

void SmdWriter::BuildNodeList(aiNode* node)
{
    if (!node) return;

    m_nodeList.push_back(node);

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        BuildNodeList(node->mChildren[i]);
    }
}

int SmdWriter::GetNodeIndex(aiNode* node)
{
    for (size_t i = 0; i < m_nodeList.size(); i++) {
        if (m_nodeList[i] == node) {
            return (int)i;
        }
    }
    return -1;
}

aiNode* SmdWriter::FindNodeByName(aiNode* node, const std::string& name)
{
    if (!node) return nullptr;

    if (node->mName.data == name) {
        return node;
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        aiNode* found = FindNodeByName(node->mChildren[i], name);
        if (found) return found;
    }

    return nullptr;
}

// WriteVector3 - Write a 3D vector to file
void SmdWriter::WriteVector3(std::ofstream& file, const aiVector3D& vec)
{
    file << std::fixed << std::setprecision(6) << vec.x << " " << vec.y << " " << vec.z;
}

// ConvertQuaternionToEuler - Convert quaternion to Euler angles (XYZ order)
void SmdWriter::ConvertQuaternionToEuler(const aiQuaternion& quat, aiVector3D& euler)
{
    // Create a copy and normalize to ensure unit quaternion
    aiQuaternion q = quat;
    q.Normalize();

    // Clamp quaternion components to valid range
    const double x = std::clamp(static_cast<double>(q.x), -1.0, 1.0);
    const double y = std::clamp(static_cast<double>(q.y), -1.0, 1.0);
    const double z = std::clamp(static_cast<double>(q.z), -1.0, 1.0);
    const double w = std::clamp(static_cast<double>(q.w), -1.0, 1.0);

    // Build rotation matrix elements from quaternion
    const double m00 = 1.0 - 2.0 * y * y - 2.0 * z * z;
    const double m10 = 2.0 * x * y + 2.0 * w * z;
    const double m20 = 2.0 * x * z - 2.0 * w * y;

    const double m01 = 2.0 * x * y - 2.0 * w * z;
    const double m11 = 1.0 - 2.0 * x * x - 2.0 * z * z;
    const double m21 = 2.0 * y * z + 2.0 * w * x;

    const double m02 = 2.0 * x * z + 2.0 * w * y;
    const double m12 = 2.0 * y * z - 2.0 * w * x;
    const double m22 = 1.0 - 2.0 * x * x - 2.0 * y * y;

    const double GIMBAL_THRESHOLD = 0.99999;
    const double sinPitch = -m20;
    const double clampedSinPitch = std::clamp(sinPitch, -1.0, 1.0);

    double ex, ey, ez;

    if (std::abs(clampedSinPitch) >= GIMBAL_THRESHOLD) {
        // Gimbal lock case
        ey = std::copysign(M_PI / 2.0, clampedSinPitch);
        ez = 0.0;
        ex = std::atan2(-m12, m11);
    }
    else {
        // Normal case
        ey = std::asin(clampedSinPitch);
        ex = std::atan2(m21, m22);
        ez = std::atan2(m10, m00);
    }

    // Filter out micro-fluctuations
    const double ANGLE_THRESHOLD = 0.00001;
    ex = std::abs(ex) < ANGLE_THRESHOLD ? 0.0 : ex;
    ey = std::abs(ey) < ANGLE_THRESHOLD ? 0.0 : ey;
    ez = std::abs(ez) < ANGLE_THRESHOLD ? 0.0 : ez;

    euler.x = static_cast<float>(ex);
    euler.y = static_cast<float>(ey);
    euler.z = static_cast<float>(ez);
}

// GetTextureName - Get texture name from material
std::string SmdWriter::GetTextureName(const aiScene* scene, unsigned int materialIndex)
{
    if (materialIndex >= scene->mNumMaterials) {
        return "default";
    }

    aiMaterial* material = scene->mMaterials[materialIndex];
    aiString texturePath;

    if (material->GetTexture(aiTextureType_DIFFUSE, 0, &texturePath) == AI_SUCCESS) {
        std::filesystem::path path = FileIO::toPath(std::string(texturePath.C_Str()));
        return path.stem().generic_string();
    }

    return "default";
}


// WriteManualNodes - Write nodes section from manual skeleton
void SmdWriter::WriteManualNodes(std::ofstream& file)
{
    file << "nodes\n";
    
    for (const auto& bone : m_manualBones) {
        file << "  " << bone.index << " \"" << bone.name << "\" " << bone.parentIndex << "\n";
    }
    
    file << "end\n";
}

// WriteManualSkeleton - Write skeleton section from manual bones (time 0)
void SmdWriter::WriteManualSkeleton(std::ofstream& file)
{
    file << "skeleton\n";
    file << "  time 0\n";
    
    for (const auto& bone : m_manualBones) {
        file << "    " << bone.index << " "
             << std::fixed << std::setprecision(6)
             << bone.posX << " " << bone.posY << " " << bone.posZ << " "
             << bone.rotX << " " << bone.rotY << " " << bone.rotZ << "\n";
    }
    
    file << "end\n";
}

// GetMaterialNameFromMesh - Get material name from Assimp mesh
std::string SmdWriter::GetMaterialNameFromMesh(const aiScene* scene, const aiMesh* mesh) const
{
    if (!scene || !mesh) {
        return m_materialName;
    }

    // Try to get material name from mesh
    if (mesh->mMaterialIndex < scene->mNumMaterials) {
        aiMaterial* mat = scene->mMaterials[mesh->mMaterialIndex];
        aiString name;
        if (mat->Get(AI_MATKEY_NAME, name) == AI_SUCCESS && name.length > 0) {
            std::string matName = name.C_Str();
            // Add .bmp extension if not present
            if (matName.find('.') == std::string::npos) {
                matName += ".bmp";
            }
            return matName;
        }
        
        // Try diffuse texture
        aiString texPath;
        if (mat->GetTexture(aiTextureType_DIFFUSE, 0, &texPath) == AI_SUCCESS && texPath.length > 0) {
            std::string texName = texPath.C_Str();
            // Extract filename from path
            size_t lastSlash = texName.find_last_of("/\\");
            if (lastSlash != std::string::npos) {
                texName = texName.substr(lastSlash + 1);
            }
            return texName;
        }
    }

    return m_materialName;
}

// WriteManualTriangles - Write triangles from parsed SMD data with new bone indices
void SmdWriter::WriteManualTriangles(std::ofstream& file, const std::vector<SmdTriangle>& triangles, const std::vector<int>& boneIndices)
{
    file << "triangles\n";
    
    size_t vertexIndex = 0;
    
    for (const auto& tri : triangles) {
        file << tri.material << "\n";
        
        for (int v = 0; v < 3; v++) {
            const SmdVertex& vert = tri.vertices[v];
            
            // Use new bone assignment if available, otherwise keep original
            int boneIdx = vert.boneIndex;
            if (vertexIndex < boneIndices.size()) {
                boneIdx = boneIndices[vertexIndex];
            }
            
            file << "  " << boneIdx << " "
                 << std::fixed << std::setprecision(6)
                 << vert.x << " " << vert.y << " " << vert.z << " "
                 << vert.nx << " " << vert.ny << " " << vert.nz << " "
                 << vert.u << " " << vert.v << "\n";
            
            vertexIndex++;
        }
    }
    
    file << "end\n";
}

// WriteManualTrianglesMesh - Write triangles from Assimp mesh with manual bone indices
void SmdWriter::WriteManualTrianglesMesh(std::ofstream& file, const aiMesh* mesh, const std::vector<int>& boneIndices)
{
    file << "triangles\n";
    
    for (unsigned int f = 0; f < mesh->mNumFaces; f++) {
        const aiFace& face = mesh->mFaces[f];
        
        if (face.mNumIndices != 3) {
            continue; // Skip non-triangles
        }
        
        file << m_materialName << "\n";
        
        for (unsigned int v = 0; v < 3; v++) {
            unsigned int idx = face.mIndices[v];
            
            const aiVector3D& pos = mesh->mVertices[idx];
            
            // Get normal (default to up if not present)
            aiVector3D normal(0, 0, 1);
            if (mesh->HasNormals()) {
                normal = mesh->mNormals[idx];
            }
            
            // Get UV (default to 0,0 if not present)
            float u = 0, v_coord = 0;
            if (mesh->HasTextureCoords(0)) {
                u = mesh->mTextureCoords[0][idx].x;
                v_coord = mesh->mTextureCoords[0][idx].y;
            }
            
            // Get bone index from manual assignment
            int boneIdx = 0;
            if (idx < boneIndices.size()) {
                boneIdx = boneIndices[idx];
            }
            
            // Write vertex: boneIndex x y z nx ny nz u v
            file << "  " << boneIdx << " "
                 << std::fixed << std::setprecision(6)
                 << pos.x << " " << pos.y << " " << pos.z << " "
                 << normal.x << " " << normal.y << " " << normal.z << " "
                 << u << " " << v_coord << "\n";
        }
    }
    
    file << "end\n";
}

// WriteManualTrianglesScene - Write triangles from Assimp scene with manual bone indices per mesh
void SmdWriter::WriteManualTrianglesScene(std::ofstream& file, const aiScene* scene, const std::vector<std::vector<int>>& boneIndicesPerMesh)
{
    file << "triangles\n";
    
    for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
        const aiMesh* mesh = scene->mMeshes[m];
        const std::vector<int>& boneIndices = boneIndicesPerMesh[m];
        
        std::string material = GetMaterialNameFromMesh(scene, mesh);
        
        for (unsigned int f = 0; f < mesh->mNumFaces; f++) {
            const aiFace& face = mesh->mFaces[f];
            
            if (face.mNumIndices != 3) {
                continue;
            }
            
            file << material << "\n";
            
            for (unsigned int v = 0; v < 3; v++) {
                unsigned int idx = face.mIndices[v];
                
                const aiVector3D& pos = mesh->mVertices[idx];
                
                aiVector3D normal(0, 0, 1);
                if (mesh->HasNormals()) {
                    normal = mesh->mNormals[idx];
                }
                
                float u = 0, v_coord = 0;
                if (mesh->HasTextureCoords(0)) {
                    u = mesh->mTextureCoords[0][idx].x;
                    v_coord = mesh->mTextureCoords[0][idx].y;
                }
                
                int boneIdx = 0;
                if (idx < boneIndices.size()) {
                    boneIdx = boneIndices[idx];
                }
                
                file << "  " << boneIdx << " "
                     << std::fixed << std::setprecision(6)
                     << pos.x << " " << pos.y << " " << pos.z << " "
                     << normal.x << " " << normal.y << " " << normal.z << " "
                     << u << " " << v_coord << "\n";
            }
        }
    }
    
    file << "end\n";
}

// ExportSmdTriangles - Export parsed SMD triangles with manual skeleton and bone indices
bool SmdWriter::ExportSmdTriangles(const std::string& outputPath,
                                    const std::vector<SmdTriangle>& triangles,
                                    const std::vector<int>& boneIndices)
{
    if (m_manualBones.empty()) {
        m_error = "No skeleton set";
        return false;
    }
    
    if (triangles.empty()) {
        m_error = "No triangles provided";
        return false;
    }
    
    std::ofstream file(FileIO::toPath(outputPath));
    if (!file.is_open()) {
        m_error = "Failed to open output file: " + outputPath;
        return false;
    }
    
    WriteHeader(file);
    WriteManualNodes(file);
    WriteManualSkeleton(file);
    WriteManualTriangles(file, triangles, boneIndices);
    
    file.close();
    return true;
}

// ExportMeshWithManualSkeleton - Export Assimp mesh with manual skeleton
bool SmdWriter::ExportMeshWithManualSkeleton(const std::string& outputPath,
                                              const aiMesh* mesh,
                                              const std::vector<int>& boneIndices)
{
    if (m_manualBones.empty()) {
        m_error = "No skeleton set";
        return false;
    }
    
    if (!mesh) {
        m_error = "No mesh provided";
        return false;
    }
    
    std::ofstream file(FileIO::toPath(outputPath));
    if (!file.is_open()) {
        m_error = "Failed to open output file: " + outputPath;
        return false;
    }
    
    WriteHeader(file);
    WriteManualNodes(file);
    WriteManualSkeleton(file);
    WriteManualTrianglesMesh(file, mesh, boneIndices);
    
    file.close();
    return true;
}

// ExportSceneWithManualSkeleton - Export Assimp scene with manual skeleton
bool SmdWriter::ExportSceneWithManualSkeleton(const std::string& outputPath,
                                               const aiScene* scene,
                                               const std::vector<std::vector<int>>& boneIndicesPerMesh)
{
    if (m_manualBones.empty()) {
        m_error = "No skeleton set";
        return false;
    }
    
    if (!scene || scene->mNumMeshes == 0) {
        m_error = "No meshes in scene";
        return false;
    }
    
    if (boneIndicesPerMesh.size() != scene->mNumMeshes) {
        m_error = "Bone indices count doesn't match mesh count";
        return false;
    }
    
    std::ofstream file(FileIO::toPath(outputPath));
    if (!file.is_open()) {
        m_error = "Failed to open output file: " + outputPath;
        return false;
    }
    
    WriteHeader(file);
    WriteManualNodes(file);
    WriteManualSkeleton(file);
    WriteManualTrianglesScene(file, scene, boneIndicesPerMesh);
    
    file.close();
    return true;
}

