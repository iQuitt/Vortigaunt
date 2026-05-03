#include "core/converters/Gr2Converter.h"
#include "core/smd/SmdWriter.h"
#include "core/VortigauntLog.h"

#include <filesystem>
#include <iostream>
#include <sstream>
#include <map>
#include <set>
#include <fstream>
#include <cstring> 
#include <cstdint> 
#include <cmath>   
#include "utils/util.hpp"

#include "granny.h"

#include "assimp/scene.h"
#include "assimp/Exporter.hpp"
#include "assimp/Importer.hpp"
#include "assimp/postprocess.h"
#include "assimp/material.h"


#ifdef QT_WIDGETS_LIB
#include <QImage>
#include <QString>
#endif

#include "utils/Dds.h"

#include "utils/Bmp.h"

using namespace VortigauntLog;

// Static variable to store GR2 file directory for texture search
static std::string s_currentGr2FileDir;


Gr2Converter::Gr2Converter()
    : m_grannyFile(nullptr)
    , m_grannyFileInfo(nullptr)
    , m_assimpScene(nullptr)
    , m_smdWriter(new SmdWriter())
{
}

Gr2Converter::~Gr2Converter()
{
    clearData();
    delete m_smdWriter;
}

void Gr2Converter::SetConverterSettings(const Gr2ConverterSettings& settings)
{
    m_settings = settings;
}

int Gr2Converter::ConvertSingleGr2File(const std::string& inputFilePath, const std::string& outputFilePath)
{
    // Store the GR2 file directory for texture search
    s_currentGr2FileDir = String_UTF16toUTF8(FileIO::toPath(inputFilePath).parent_path().generic_u16string());
    if (!s_currentGr2FileDir.empty() && s_currentGr2FileDir.back() != '/' && s_currentGr2FileDir.back() != '\\')
    {
        s_currentGr2FileDir += std::filesystem::path::preferred_separator;
    }
    
    if (!std::filesystem::exists(FileIO::toPath(inputFilePath)))
    {
        Vortigaunt_Printf("ERROR: GR2 file does not exist: " + inputFilePath);
        return GR2_CONVERT_RET_INVALID_INPUT_FILE;
    }

    if (!loadGr2File(inputFilePath))
    {
        Vortigaunt_Printf("ERROR: Failed to load GR2 file");
        return GR2_CONVERT_RET_LOAD_FAILED;
    }

    if (!m_grannyFileInfo)
    {
        Vortigaunt_Printf("ERROR: Failed to get GR2 file info");
        return GR2_CONVERT_RET_LOAD_FAILED;
    }

    // Check if we have at least one model/mesh
    if (m_grannyFileInfo->ModelCount == 0 && m_settings.ExportMeshes)
    {
        Vortigaunt_Printf("ERROR: GR2 file contains no models/meshes");
        clearData();
        return GR2_CONVERT_RET_NO_MESH;
    }

    if (!convertToAssimpScene())
    {
        Vortigaunt_Printf("ERROR: Failed to convert GR2 data to Assimp scene");
        clearData();
        return GR2_CONVERT_RET_EXPORT_FAILED;
    }

    bool success = convertToSmd(outputFilePath);

    if (!success)
    {
        Vortigaunt_Printf("ERROR: Export failed");
        clearData();
        return GR2_CONVERT_RET_EXPORT_FAILED;
    }

    // Export textures if materials are enabled
    if (m_settings.ExportMaterials && m_grannyFileInfo && m_grannyFileInfo->TextureCount > 0)
    {
        std::filesystem::path outputDir = FileIO::toPath(outputFilePath).parent_path();
        std::string smdFilePath = outputFilePath;
        exportTextures(String_UTF16toUTF8(outputDir.generic_u16string()), smdFilePath);
    }

    clearData();

    return GR2_CONVERT_RET_OK;
}

static bool ReadFileToBuffer(const std::string& inputFilePath, std::vector<char>& buffer)
{
    std::ifstream file(FileIO::toPath(inputFilePath), std::ios::binary | std::ios::ate);
    if (!file)
        return false;
        
    std::streamsize size = file.tellg();
    if (size <= 0)
        return false;
        
    file.seekg(0, std::ios::beg);
    buffer.resize(static_cast<size_t>(size));
    
    if (!file.read(buffer.data(), size))
    {
        buffer.clear();
        return false;
    }
        
    return true;
}

std::vector<std::string> Gr2Converter::GetAnimationNames(const std::string& inputFilePath)
{
    std::vector<std::string> names;
    
    std::vector<char> buffer;
    if (!ReadFileToBuffer(inputFilePath, buffer))
        return names;
        
    granny_file* file = GrannyReadEntireFileFromMemory(static_cast<granny_int32x>(buffer.size()), buffer.data());
    if (!file)
        return names;
    
    granny_file_info* fileInfo = GrannyGetFileInfo(file);
    if (fileInfo && fileInfo->AnimationCount > 0)
    {
        for (int i = 0; i < fileInfo->AnimationCount; ++i)
        {
            granny_animation* anim = fileInfo->Animations[i];
            if (anim && anim->Name)
            {
                names.push_back(anim->Name);
            }
            else
            {
                names.push_back("Animation_" + std::to_string(i));
            }
        }
    }
    
    GrannyFreeFile(file);
    return names;
}

int Gr2Converter::GetPolygonCount(const std::string& inputFilePath)
{
    std::vector<char> buffer;
    if (!ReadFileToBuffer(inputFilePath, buffer))
        return 0;

    granny_file* file = GrannyReadEntireFileFromMemory(static_cast<granny_int32x>(buffer.size()), buffer.data());
    if (!file)
        return 0;
    
    granny_file_info* fileInfo = GrannyGetFileInfo(file);
    if (!fileInfo || fileInfo->ModelCount == 0)
    {
        GrannyFreeFile(file);
        return 0;
    }
    
    int totalPolygons = 0;
    
    for (int modelIdx = 0; modelIdx < fileInfo->ModelCount; ++modelIdx)
    {
        granny_model* model = fileInfo->Models[modelIdx];
        if (!model) continue;
        
        for (int meshIdx = 0; meshIdx < model->MeshBindingCount; ++meshIdx)
        {
            granny_mesh* mesh = model->MeshBindings[meshIdx].Mesh;
            if (!mesh) continue;
            
            int indexCount = GrannyGetMeshIndexCount(mesh);
            totalPolygons += indexCount / 3;  // 3 indices per triangle
        }
    }
    
    GrannyFreeFile(file);
    return totalPolygons;
}

aiScene* Gr2Converter::LoadAndGetScene(const std::string& inputFilePath, const std::string& textureOutputDir)
{
    // Store the GR2 file directory for texture search
    s_currentGr2FileDir = String_UTF16toUTF8(FileIO::toPath(inputFilePath).parent_path().generic_u16string());
    if (!s_currentGr2FileDir.empty() && s_currentGr2FileDir.back() != '/' && s_currentGr2FileDir.back() != '\\')
    {
        s_currentGr2FileDir += std::filesystem::path::preferred_separator;
    }
    
    if (!std::filesystem::exists(FileIO::toPath(inputFilePath)))
    {
        Vortigaunt_Printf("ERROR: GR2 file does not exist: " + inputFilePath);
        return nullptr;
    }

    if (!loadGr2File(inputFilePath))
    {
        Vortigaunt_Printf("ERROR: Failed to load GR2 file");
        return nullptr;
    }

    if (!m_grannyFileInfo)
    {
        Vortigaunt_Printf("ERROR: Failed to get GR2 file info");
        return nullptr;
    }

    if (m_grannyFileInfo->ModelCount == 0)
    {
        Vortigaunt_Printf("ERROR: GR2 file contains no models/meshes");
        return nullptr;
    }


    if (!convertToAssimpScene())
    {
        Vortigaunt_Printf("ERROR: Failed to convert GR2 data to Assimp scene");
        return nullptr;
    }

    // Export textures if materials are enabled
    if (m_grannyFileInfo && m_grannyFileInfo->TextureCount > 0 && !textureOutputDir.empty())
    {
        exportTextures(textureOutputDir, "");
    }

    return m_assimpScene;
}

int Gr2Converter::ConvertGr2Animation(const std::string& inputFilePath, const std::string& outputFilePath, int animationIndex, const std::string& skeletonSourceFile)
{
    
    if (!std::filesystem::exists(FileIO::toPath(inputFilePath)))
    {
        Vortigaunt_Printf("ERROR: GR2 file does not exist: " + inputFilePath);
        return GR2_CONVERT_RET_INVALID_INPUT_FILE;
    }
    
    // Load skeleton from main model file if provided
    granny_file* skeletonFile = nullptr;
    granny_file_info* skeletonFileInfo = nullptr;
    std::vector<char> skeletonBuffer;
    
    if (!skeletonSourceFile.empty() && std::filesystem::exists(FileIO::toPath(skeletonSourceFile)))
    {
        if (ReadFileToBuffer(skeletonSourceFile, skeletonBuffer))
        {
            skeletonFile = GrannyReadEntireFileFromMemory(static_cast<granny_int32x>(skeletonBuffer.size()), skeletonBuffer.data());
            if (skeletonFile)
            {
                skeletonFileInfo = GrannyGetFileInfo(skeletonFile);
                if (!skeletonFileInfo || skeletonFileInfo->ModelCount == 0)
                {
                    GrannyFreeFile(skeletonFile);
                    skeletonFile = nullptr;
                    skeletonFileInfo = nullptr;
                }
            }
        }
    }
    
    if (!loadGr2File(inputFilePath))
    {
        Vortigaunt_Printf("ERROR: Failed to load GR2 file");
        if (skeletonFile) GrannyFreeFile(skeletonFile);
        return GR2_CONVERT_RET_LOAD_FAILED;
    }
    
    if (!m_grannyFileInfo)
    {
        Vortigaunt_Printf("ERROR: Failed to get GR2 file info");
        if (skeletonFile) GrannyFreeFile(skeletonFile);
        return GR2_CONVERT_RET_LOAD_FAILED;
    }
    
    if (animationIndex < 0 || animationIndex >= m_grannyFileInfo->AnimationCount)
    {
        Vortigaunt_Printf("ERROR: Invalid animation index: " + std::to_string(animationIndex));
        clearData();
        if (skeletonFile) GrannyFreeFile(skeletonFile);
        return GR2_CONVERT_RET_EXPORT_FAILED;
    }
    
    // If animation file has no models but we have skeleton from main file, use it
    if (m_grannyFileInfo->ModelCount == 0 && skeletonFileInfo && skeletonFileInfo->ModelCount > 0)
    {
        // Temporarily replace file info to use skeleton from main model
        granny_file_info* tempFileInfo = m_grannyFileInfo;
        m_grannyFileInfo = skeletonFileInfo;
        
        // Create settings for animation-only export
        Gr2ConverterSettings animSettings = m_settings;
        animSettings.ExportMeshes = false;
        animSettings.ExportAnimations = true;
        animSettings.ExportSkeleton = true;
        animSettings.ExportMaterials = false;
        
        SetConverterSettings(animSettings);
        
        if (!convertToAssimpScene())
        {
            Vortigaunt_Printf("ERROR: Failed to convert GR2 data to Assimp scene");
            m_grannyFileInfo = tempFileInfo; // Restore
            clearData();
            if (skeletonFile) GrannyFreeFile(skeletonFile);
            return GR2_CONVERT_RET_EXPORT_FAILED;
        }
        
        // Restore original file info for animation access
        m_grannyFileInfo = tempFileInfo;
        
        // Get model/skeleton from the skeleton file info
        granny_model* modelForAnim = nullptr;
        if (skeletonFileInfo && skeletonFileInfo->ModelCount > 0 && skeletonFileInfo->Models[0])
        {
            modelForAnim = skeletonFileInfo->Models[0];
        }
        
        if (!convertAnimationsFromGranny(modelForAnim))
        {
            Vortigaunt_Printf("WARNING: No Animations detected from Gr2.");
        }
        
        // Assign animations to scene (convertAnimationsFromGranny adds to m_animations but doesn't assign to scene)
        if (m_assimpScene && m_animations.size() > 0)
        {
            m_assimpScene->mNumAnimations = static_cast<unsigned int>(m_animations.size());
            m_assimpScene->mAnimations = m_animations.data();
        }
        
        // Filter to only the selected animation
        if (m_assimpScene && m_assimpScene->mNumAnimations > static_cast<unsigned int>(animationIndex))
        {
            aiAnimation* selectedAnim = m_assimpScene->mAnimations[animationIndex];
            aiAnimation** newAnims = new aiAnimation*[1];
            newAnims[0] = selectedAnim;
            m_assimpScene->mAnimations = newAnims;
            m_assimpScene->mNumAnimations = 1;
        }
    }
    else
    {
        // Normal path: animation file has its own model/skeleton
        Gr2ConverterSettings animSettings = m_settings;
        animSettings.ExportMeshes = false;
        animSettings.ExportAnimations = true;
        animSettings.ExportSkeleton = true;
        animSettings.ExportMaterials = false;
        
        SetConverterSettings(animSettings);
        
        if (!convertToAssimpScene())
        {
            Vortigaunt_Printf("ERROR: Failed to convert GR2 data to Assimp scene");
            clearData();
            if (skeletonFile) GrannyFreeFile(skeletonFile);
            return GR2_CONVERT_RET_EXPORT_FAILED;
        }
        
        // Filter to only the selected animation
        if (m_assimpScene && m_assimpScene->mNumAnimations > animationIndex)
        {
            aiAnimation* selectedAnim = m_assimpScene->mAnimations[animationIndex];
            aiAnimation** newAnims = new aiAnimation*[1];
            newAnims[0] = selectedAnim;
            m_assimpScene->mAnimations = newAnims;
            m_assimpScene->mNumAnimations = 1;
        }
    }
    
    bool success = convertToSmd(outputFilePath);
    
    if (!success)
    {
        Vortigaunt_Printf("ERROR: Export failed");
        clearData();
        if (skeletonFile) GrannyFreeFile(skeletonFile);
        return GR2_CONVERT_RET_EXPORT_FAILED;
    }
    
    clearData();
    if (skeletonFile) GrannyFreeFile(skeletonFile);
    
    return GR2_CONVERT_RET_OK;
}

bool Gr2Converter::loadGr2File(const std::string& inputFilePath)
{
    clearData();

    if (!ReadFileToBuffer(inputFilePath, m_grannyBuffer))
    {
        Vortigaunt_Printf("ERROR: Failed to read GR2 file to buffer");
        return false;
    }

    m_grannyFile = GrannyReadEntireFileFromMemory(static_cast<granny_int32x>(m_grannyBuffer.size()), m_grannyBuffer.data());
    if (!m_grannyFile)
    {
        Vortigaunt_Printf("ERROR: Failed to parse GR2 file");
        m_grannyBuffer.clear();
        return false;
    }

    m_grannyFileInfo = GrannyGetFileInfo(m_grannyFile);
    if (!m_grannyFileInfo)
    {
        Vortigaunt_Printf("ERROR: Failed to get GR2 file info");
        GrannyFreeFile(m_grannyFile);
        m_grannyFile = nullptr;
        return false;
    }

    return true;
}

bool Gr2Converter::convertToSmd(const std::string& outputFilePath)
{
    if (!m_assimpScene)
    {
        Vortigaunt_Printf("ERROR: Assimp Scene Failed");
        return false;
    }

    try
    {
        // Set mirror UV Y setting
        // Most Metin2 models need this. 
        m_smdWriter->SetMirrorUVY(m_settings.MirrorUVY);

        bool isAnimationOnly = (!m_settings.ExportMeshes && m_assimpScene->mNumAnimations > 0) ||
                                (m_assimpScene->mNumMeshes == 0 && m_assimpScene->mNumAnimations > 0);
        
        if (isAnimationOnly)
        {
            int animIndex = 0;
            return m_smdWriter->ExportAnimationSMD(m_assimpScene, animIndex, outputFilePath);
        }
        else
        {
            return m_smdWriter->ExportMeshSMD(m_assimpScene, outputFilePath);
        }
    }
    catch (const std::exception& e)
    {
        std::cerr << "Failed to export to SMD: " << e.what() << std::endl;
        return false;
    }

}

bool Gr2Converter::convertToAssimpScene()
{
    if (!m_grannyFileInfo)
    {
        Vortigaunt_Printf("ERROR: m_grannyFileInfo is null");
        return false;
    }

    m_assimpScene = new aiScene();
    m_assimpScene->mRootNode = new aiNode();
    m_assimpScene->mRootNode->mName = "Root";

    if (m_settings.ExportMeshes && m_grannyFileInfo->ModelCount > 0)
    {
        if (!convertMeshesFromGranny())
        {
            Vortigaunt_Printf("ERROR: Failed to convert meshes");
            return false;
        }
    }

    if (m_settings.ExportSkeleton && m_grannyFileInfo->ModelCount > 0)
    {
        if (!convertSkeletonFromGranny())
        {
            Vortigaunt_Printf("ERROR: Failed to convert skeleton");
            return false;
        }
    }

    if (m_settings.ExportMaterials && m_grannyFileInfo->TextureCount > 0)
    {
        if (!convertMaterialsFromGranny())
        {
            LogF("%s: Not found the Materials",__func__);
        }
    }

    // Convert animations
    if (m_settings.ExportAnimations && m_grannyFileInfo->AnimationCount > 0)
    {
        if (!convertAnimationsFromGranny(nullptr))
        {
            LogF("%s: Not found the Animations", __func__);

        }
    }

    // Setup scene
    m_assimpScene->mNumMeshes = static_cast<unsigned int>(m_meshes.size());
    if (m_meshes.size() > 0)
    {
        m_assimpScene->mMeshes = m_meshes.data();
    }

    m_assimpScene->mNumAnimations = static_cast<unsigned int>(m_animations.size());
    if (m_animations.size() > 0)
    {
        m_assimpScene->mAnimations = m_animations.data();
    }

    // Create mesh nodes
    if (m_meshes.size() > 0 && m_grannyFileInfo->ModelCount > 0)
    {
        granny_model* grannyModel = m_grannyFileInfo->Models[0];
        aiNode* meshNode = new aiNode();
        meshNode->mName = "Meshes";
        meshNode->mNumMeshes = static_cast<unsigned int>(m_meshes.size());
        meshNode->mMeshes = new unsigned int[m_meshes.size()];
        for (size_t i = 0; i < m_meshes.size(); ++i)
        {
            meshNode->mMeshes[i] = static_cast<unsigned int>(i);
        }

        // Apply InitialPlacement transform to mesh node
        if (grannyModel && grannyModel->InitialPlacement.Flags != 0)
        {
            float initialPlacementTransform[16];
            GrannyBuildCompositeTransform4x4(&grannyModel->InitialPlacement, initialPlacementTransform);
            meshNode->mTransformation = aiMatrix4x4(
                initialPlacementTransform[0], initialPlacementTransform[1], initialPlacementTransform[2], initialPlacementTransform[3],
                initialPlacementTransform[4], initialPlacementTransform[5], initialPlacementTransform[6], initialPlacementTransform[7],
                initialPlacementTransform[8], initialPlacementTransform[9], initialPlacementTransform[10], initialPlacementTransform[11],
                initialPlacementTransform[12], initialPlacementTransform[13], initialPlacementTransform[14], initialPlacementTransform[15]
            );
        }

        m_assimpScene->mRootNode->addChildren(1, &meshNode);
    }

    return true;
}

bool Gr2Converter::convertMeshesFromGranny()
{
    if (!m_grannyFileInfo || m_grannyFileInfo->ModelCount == 0)
    {
        LogF("%s ERROR: Failed to find Granny2 File", __func__);
        return false;
    }

    Vortigaunt_Printf("DEBUG: Total models in GR2 file: " + std::to_string(m_grannyFileInfo->ModelCount));
    
    // Get the main skeleton from the first model (for attachment models that don't have their own skeleton)
    granny_skeleton* mainSkeleton = nullptr;
    if (m_grannyFileInfo->ModelCount > 0 && m_grannyFileInfo->Models[0] && m_grannyFileInfo->Models[0]->Skeleton)
    {
        mainSkeleton = m_grannyFileInfo->Models[0]->Skeleton;
        Vortigaunt_Printf("DEBUG: Main skeleton found with " + std::to_string(mainSkeleton->BoneCount) + " bones");
    }
    else
    {
        Vortigaunt_Printf("DEBUG: No main skeleton found in first model");
    }

    // Process each model
    for (int modelIdx = 0; modelIdx < m_grannyFileInfo->ModelCount; ++modelIdx)
    {
        granny_model* grannyModel = m_grannyFileInfo->Models[modelIdx];
        
        if (!grannyModel)
        {
            Vortigaunt_Printf("DEBUG: Model " + std::to_string(modelIdx) + " is null");
            continue;
        }
        
        std::string modelName = grannyModel->Name ? grannyModel->Name : "Unnamed";
        Vortigaunt_Printf("DEBUG: Gr2 model " + std::to_string(modelIdx) + ": " + modelName);
        Vortigaunt_Printf("DEBUG:   - MeshBindingCount: " + std::to_string(grannyModel->MeshBindingCount));
        Vortigaunt_Printf("DEBUG:   - Has Skeleton: " + std::string(grannyModel->Skeleton ? "Yes" : "No"));
        if (grannyModel->Skeleton)
        {
            Vortigaunt_Printf("DEBUG:   - Total Bone: " + std::to_string(grannyModel->Skeleton->BoneCount));
        }
        Vortigaunt_Printf("DEBUG:   - InitialPlacement flags: " + std::to_string(grannyModel->InitialPlacement.Flags));
        
        if (!grannyModel->MeshBindingCount)
        {
            Vortigaunt_Printf("DEBUG:   - Skipping model (no mesh bindings)");
            continue;
        }

        // Process each mesh in the model
        for (int meshBindingIdx = 0; meshBindingIdx < grannyModel->MeshBindingCount; ++meshBindingIdx)
        {
            granny_mesh* grannyMesh = grannyModel->MeshBindings[meshBindingIdx].Mesh;
            if (!grannyMesh)
            {
                continue;
            }

            std::string meshName = grannyMesh->Name ? grannyMesh->Name : "Unnamed";
            Vortigaunt_Printf("DEBUG:   Mesh '" + meshName + "' MaterialBindingCount: " + std::to_string(grannyMesh->MaterialBindingCount));
            
            // Log ALL MaterialBindings to understand structure
            for (int matBindIdx = 0; matBindIdx < grannyMesh->MaterialBindingCount; ++matBindIdx)
            {
                if (grannyMesh->MaterialBindings[matBindIdx].Material)
                {
                    granny_material* mat = grannyMesh->MaterialBindings[matBindIdx].Material;
                    Vortigaunt_Printf("DEBUG: MaterialBinding[" + std::to_string(matBindIdx) + "]: '" + 
                        std::string(mat->Name ? mat->Name : "Unnamed") + "' MapCount: " + std::to_string(mat->MapCount));
                    
                    granny_texture* tex = getTextureFromMaterial(mat);
                    if (tex)
                    {
                        Vortigaunt_Printf("DEBUG: Texture: '" + std::string(tex->FromFileName ? tex->FromFileName : "Unnamed") + "'");
                    }
                    else
                    {
                        Vortigaunt_Printf("DEBUG: Cannot find the Texture");
                    }
                }
            }
            
            // Get all vertices from the mesh
            int vertexCount = GrannyGetMeshVertexCount(grannyMesh);
            if (vertexCount <= 0)
            {
                continue;
            }
            
            // Copy vertices using PWNT3432 format
            granny_pwnt3432_vertex* allGrannyVertices = new granny_pwnt3432_vertex[vertexCount];
            GrannyCopyMeshVertices(grannyMesh, GrannyPWNT3432VertexType, allGrannyVertices);
            
            // Get indices
            int indexCount = GrannyGetMeshIndexCount(grannyMesh);
            unsigned int* allIndices = new unsigned int[indexCount];
            GrannyCopyMeshIndices(grannyMesh, 4, allIndices);
            
            // Get triangle material groups
            int groupCount = GrannyGetMeshTriangleGroupCount(grannyMesh);
            granny_tri_material_group* groups = GrannyGetMeshTriangleGroups(grannyMesh);
            
            Vortigaunt_Printf("DEBUG: TriMaterialGroupCount: " + std::to_string(groupCount));
            
            // If only one material group or none, process as single mesh
            if (groupCount <= 1 || grannyMesh->MaterialBindingCount <= 1)
            {
                // Use existing single-mesh logic
                aiMesh* assimpMesh = new aiMesh();
                assimpMesh->mName = grannyMesh->Name ? grannyMesh->Name : "Mesh";
                assimpMesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;
                
                // Get material index from first binding
                int materialIndex = 0;
                if (grannyMesh->MaterialBindingCount > 0 && grannyMesh->MaterialBindings[0].Material)
                {
                    granny_texture* tex = getTextureFromMaterial(grannyMesh->MaterialBindings[0].Material);
                    if (tex)
                    {
                        materialIndex = findTextureIndex(tex);
                        if (materialIndex < 0) materialIndex = 0;
                        Vortigaunt_Printf("DEBUG: Using texture: '" + std::string(tex->FromFileName ? tex->FromFileName : "Unnamed") + "'");
                        Vortigaunt_Printf("DEBUG: Matched to texture index: " + std::to_string(materialIndex));
                    }
                }
                Vortigaunt_Printf("DEBUG: Final material index for mesh '" + meshName + "': " + std::to_string(materialIndex));
                assimpMesh->mMaterialIndex = materialIndex;

                // Allocate vertex data
                assimpMesh->mNumVertices = vertexCount;
                assimpMesh->mVertices = new aiVector3D[vertexCount];
                assimpMesh->mNormals = new aiVector3D[vertexCount];
                assimpMesh->mTextureCoords[0] = new aiVector3D[vertexCount];
                assimpMesh->mNumUVComponents[0] = 2;

                for (int vIdx = 0; vIdx < vertexCount; ++vIdx)
                {
                    const auto& v = allGrannyVertices[vIdx];
                    assimpMesh->mVertices[vIdx] = aiVector3D(v.Position[0], v.Position[1], v.Position[2]);
                    assimpMesh->mNormals[vIdx] = aiVector3D(v.Normal[0], v.Normal[1], v.Normal[2]);
                    assimpMesh->mTextureCoords[0][vIdx] = aiVector3D(v.UV[0], v.UV[1], 0.0f);
                }

                // Copy faces
                if (indexCount > 0 && indexCount % 3 == 0)
                {
                    int faceCount = indexCount / 3;
                    assimpMesh->mNumFaces = faceCount;
                    assimpMesh->mFaces = new aiFace[faceCount];

                    for (int fIdx = 0; fIdx < faceCount; ++fIdx)
                    {
                        aiFace& face = assimpMesh->mFaces[fIdx];
                        face.mNumIndices = 3;
                        face.mIndices = new unsigned int[3];
                        // Keep original winding order (Granny2 convention matches SMD)
                        face.mIndices[0] = allIndices[fIdx * 3 + 0];
                        face.mIndices[1] = allIndices[fIdx * 3 + 1];
                        face.mIndices[2] = allIndices[fIdx * 3 + 2];
                    }
                }
                
                // Fill original indices mapping for single mesh
                std::vector<unsigned int> originalIndices(vertexCount);
                for (int i = 0; i < vertexCount; ++i) originalIndices[i] = i;
                m_meshToOriginalIndices[assimpMesh] = originalIndices;

                // Will add bone weights later (after this block)
                m_meshes.push_back(assimpMesh);
            }
            else
            {
                Vortigaunt_Printf("DEBUG: Splitting multiple material mesh into " + std::to_string(groupCount) + " sub-meshes");
                
                for (int grpIdx = 0; grpIdx < groupCount; ++grpIdx)
                {
                    granny_tri_material_group& group = groups[grpIdx];
                    int matIdx = group.MaterialIndex;
                    int triFirst = group.TriFirst;
                    int triCount = group.TriCount;
                    
                    Vortigaunt_Printf("DEBUG: Group " + std::to_string(grpIdx) + ": MaterialIndex=" + std::to_string(matIdx) + 
                        ", TriFirst=" + std::to_string(triFirst) + ", TriCount=" + std::to_string(triCount));
                    
                    if (triCount <= 0) continue;
                    
                    // Get material and texture for this group
                    granny_material* groupMaterial = nullptr;
                    granny_texture* groupTexture = nullptr;
                    std::string subMeshName = meshName + "_" + std::to_string(grpIdx);
                    
                    if (matIdx >= 0 && matIdx < grannyMesh->MaterialBindingCount)
                    {
                        groupMaterial = grannyMesh->MaterialBindings[matIdx].Material;
                        if (groupMaterial)
                        {
                            groupTexture = getTextureFromMaterial(groupMaterial);
                            if (groupTexture)
                            {
                                // Use texture name as sub-mesh name
                                subMeshName = getTextureBaseName(groupTexture);
                                Vortigaunt_Printf("DEBUG: Sub-mesh will use texture: '" + subMeshName + "'");
                            }
                        }
                    }
                    
                    // Build vertex remap table - only include vertices used by this group's triangles
                    std::map<unsigned int, unsigned int> oldToNewVertex;
                    std::vector<unsigned int> newToOldVertex;
                    
                    int indexStart = triFirst * 3;
                    int indexEnd = (triFirst + triCount) * 3;
                    
                    for (int i = indexStart; i < indexEnd && i < indexCount; ++i)
                    {
                        unsigned int oldIdx = allIndices[i];
                        if (oldToNewVertex.find(oldIdx) == oldToNewVertex.end())
                        {
                            unsigned int newIdx = static_cast<unsigned int>(newToOldVertex.size());
                            oldToNewVertex[oldIdx] = newIdx;
                            newToOldVertex.push_back(oldIdx);
                        }
                    }
                    
                    int subMeshVertexCount = static_cast<int>(newToOldVertex.size());
                    if (subMeshVertexCount == 0) continue;
                    
                    // Create sub-mesh
                    aiMesh* subMesh = new aiMesh();
                    subMesh->mName = subMeshName;
                    subMesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;
                    
                    // Set material index
                    int textureIndex = 0;
                    if (groupTexture)
                    {
                        textureIndex = findTextureIndex(groupTexture);
                        if (textureIndex < 0) textureIndex = 0;
                    }
                    subMesh->mMaterialIndex = textureIndex;
                    Vortigaunt_Printf("DEBUG:S Sub-mesh '" + subMeshName + "' materialIndex=" + std::to_string(textureIndex));
                    
                    // Allocate vertex data
                    subMesh->mNumVertices = subMeshVertexCount;
                    subMesh->mVertices = new aiVector3D[subMeshVertexCount];
                    subMesh->mNormals = new aiVector3D[subMeshVertexCount];
                    subMesh->mTextureCoords[0] = new aiVector3D[subMeshVertexCount];
                    subMesh->mNumUVComponents[0] = 2;
                    
                    // Copy vertices with remapping
                    for (int newIdx = 0; newIdx < subMeshVertexCount; ++newIdx)
                    {
                        unsigned int oldIdx = newToOldVertex[newIdx];
                        const auto& v = allGrannyVertices[oldIdx];
                        subMesh->mVertices[newIdx] = aiVector3D(v.Position[0], v.Position[1], v.Position[2]);
                        subMesh->mNormals[newIdx] = aiVector3D(v.Normal[0], v.Normal[1], v.Normal[2]);
                        subMesh->mTextureCoords[0][newIdx] = aiVector3D(v.UV[0], v.UV[1], 0.0f);
                    }
                    
                    // Copy faces with remapped indices
                    subMesh->mNumFaces = triCount;
                    subMesh->mFaces = new aiFace[triCount];
                    
                    for (int tIdx = 0; tIdx < triCount; ++tIdx)
                    {
                        int srcFaceIdx = triFirst + tIdx;
                        aiFace& face = subMesh->mFaces[tIdx];
                        face.mNumIndices = 3;
                        face.mIndices = new unsigned int[3];
                        // Keep original winding order and remap indices
                        face.mIndices[0] = oldToNewVertex[allIndices[srcFaceIdx * 3 + 0]];
                        face.mIndices[1] = oldToNewVertex[allIndices[srcFaceIdx * 3 + 1]];
                        face.mIndices[2] = oldToNewVertex[allIndices[srcFaceIdx * 3 + 2]];
                    }
                    
                    // Store original indices for bone weight remapping
                    m_meshToOriginalIndices[subMesh] = newToOldVertex;
                    
                    m_meshes.push_back(subMesh);
                    Vortigaunt_Printf("DEBUG: Created sub-mesh '" + subMeshName + "' with " + std::to_string(subMeshVertexCount) + " vertices, " + std::to_string(triCount) + " faces");
                }
            }
            
            delete[] allGrannyVertices;
            delete[] allIndices;

            // Add bone weights to meshes
            // Get skeleton for bone weight processing
            granny_skeleton* skeleton = grannyModel->Skeleton;
            
            // Check if this is a rigid mesh (weapon, shield, etc.)
            bool isRigidAttachment = GrannyMeshIsRigid(grannyMesh);
            if (isRigidAttachment)
            {
                Vortigaunt_Printf("DEBUG: Mesh '" + meshName + "' is a rigid attachment (detected by GrannyMeshIsRigid)");
            }
            
            // Process bone weights for skinned meshes OR rigid attachments
            if (m_settings.ExportSkeleton && skeleton && (grannyMesh->BoneBindingCount > 0 || isRigidAttachment))
            {
                Vortigaunt_Printf("DEBUG: Processing bone weights for mesh '" + meshName + "'" + 
                    (isRigidAttachment ? " (rigid attachment - will bind to hand bone)" : ""));
                
                // For rigid attachments, find attachment bone name
                std::string attachmentBoneName;
                if (isRigidAttachment)
                {
                    // Try to get bone from first bone binding if available
                    if (grannyMesh->BoneBindingCount > 0 && grannyMesh->BoneBindings[0].BoneName)
                    {
                        attachmentBoneName = grannyMesh->BoneBindings[0].BoneName;
                        Vortigaunt_Printf("DEBUG: Rigid mesh will be bound to bone from binding: '" + attachmentBoneName + "'");
                    }
                    else
                    {
                        // Try to find a hand bone (prefer right hand, then left hand)
                        for (int boneIdx = 0; boneIdx < skeleton->BoneCount; ++boneIdx)
                        {
                            if (skeleton->Bones[boneIdx].Name)
                            {
                                std::string boneName = skeleton->Bones[boneIdx].Name;
                                if (boneName.find("R Hand") != std::string::npos || 
                                    boneName.find("R_Hand") != std::string::npos ||
                                    boneName.find("Rhand") != std::string::npos)
                                {
                                    attachmentBoneName = boneName;
                                    Vortigaunt_Printf("DEBUG:     Using right hand bone: '" + attachmentBoneName + "'");
                                    break;
                                }
                                else if (attachmentBoneName.empty() && 
                                         (boneName.find("L Hand") != std::string::npos ||
                                          boneName.find("L_Hand") != std::string::npos ||
                                          boneName.find("Lhand") != std::string::npos))
                                {
                                    attachmentBoneName = boneName;
                                }
                            }
                        }
                        
                        // Fallback to first bone if no hand bone found
                        if (attachmentBoneName.empty() && skeleton->BoneCount > 0 && skeleton->Bones[0].Name)
                        {
                            attachmentBoneName = skeleton->Bones[0].Name;
                            Vortigaunt_Printf("DEBUG: No hand bone found, using first bone: '" + attachmentBoneName + "'");
                        }
                    }
                }
                
                // Create a map of bone names to bone indices in the skeleton
                std::map<std::string, int> boneNameToIndex;
                for (int boneIdx = 0; boneIdx < skeleton->BoneCount; ++boneIdx)
                {
                    if (skeleton->Bones[boneIdx].Name)
                    {
                        boneNameToIndex[skeleton->Bones[boneIdx].Name] = boneIdx;
                    }
                }
                
                // For each mesh created from this granny mesh, add bone weights
                // We need to iterate over all recently added meshes
                for (aiMesh* assimpMesh : m_meshes)
                {
                    if (assimpMesh->mNumBones > 0) continue; // Already has bones
                    
                    // Create aiBone structures for each bone binding
                    std::vector<aiBone*> meshBones;
                    std::vector<aiBone*> meshBonesByBindingIndex;
                    std::map<std::string, aiBone*> boneNameToAiBone;
                    std::map<std::string, std::vector<aiVertexWeight>> boneWeightsMap;
                    
                    meshBonesByBindingIndex.resize(grannyMesh->BoneBindingCount, nullptr);
                    
                    for (int boneBindingIdx = 0; boneBindingIdx < grannyMesh->BoneBindingCount; ++boneBindingIdx)
                    {
                        granny_bone_binding* binding = &grannyMesh->BoneBindings[boneBindingIdx];
                        if (!binding->BoneName) continue;
                        
                        std::string boneName(binding->BoneName);
                        auto it = boneNameToIndex.find(boneName);
                        if (it == boneNameToIndex.end()) continue;
                        
                        int boneIdx = it->second;
                        granny_bone* grannyBone = &skeleton->Bones[boneIdx];
                        
                        aiBone* assimpBone = new aiBone();
                        assimpBone->mName = boneName;
                        
                        // Calculate inverse bind matrix (offset matrix)
                        // Granny uses row-major, transpose to column-major for correct bone transforms
                        aiMatrix4x4 inverseWorldMatrix(
                            grannyBone->InverseWorld4x4[0][0], grannyBone->InverseWorld4x4[1][0], grannyBone->InverseWorld4x4[2][0], grannyBone->InverseWorld4x4[3][0],
                            grannyBone->InverseWorld4x4[0][1], grannyBone->InverseWorld4x4[1][1], grannyBone->InverseWorld4x4[2][1], grannyBone->InverseWorld4x4[3][1],
                            grannyBone->InverseWorld4x4[0][2], grannyBone->InverseWorld4x4[1][2], grannyBone->InverseWorld4x4[2][2], grannyBone->InverseWorld4x4[3][2],
                            grannyBone->InverseWorld4x4[0][3], grannyBone->InverseWorld4x4[1][3], grannyBone->InverseWorld4x4[2][3], grannyBone->InverseWorld4x4[3][3]
                        );
                        
                        // Get mesh transform (InitialPlacement) if present
                        aiMatrix4x4 meshTransform;
                        if (grannyModel->InitialPlacement.Flags != 0)
                        {
                            float meshTransformArray[16];
                            GrannyBuildCompositeTransform4x4(&grannyModel->InitialPlacement, meshTransformArray);
                            // i hate this fuckin shit
                            meshTransform = aiMatrix4x4(
                                meshTransformArray[0], meshTransformArray[1], meshTransformArray[2], meshTransformArray[3],
                                meshTransformArray[4], meshTransformArray[5], meshTransformArray[6], meshTransformArray[7],
                                meshTransformArray[8], meshTransformArray[9], meshTransformArray[10], meshTransformArray[11],
                                meshTransformArray[12], meshTransformArray[13], meshTransformArray[14], meshTransformArray[15]
                            );
                        }
                        else
                        {
                            meshTransform = aiMatrix4x4(); // Identity
                        }
                        
                        assimpBone->mOffsetMatrix = inverseWorldMatrix * meshTransform;
                        
                        meshBones.push_back(assimpBone);
                        meshBonesByBindingIndex[boneBindingIdx] = assimpBone;
                        boneNameToAiBone[boneName] = assimpBone;
                        boneWeightsMap[boneName] = std::vector<aiVertexWeight>();
                    }
                    
                    // Re-read vertices to get bone indices and weights
                    granny_pwnt3432_vertex* boneVertices = new granny_pwnt3432_vertex[vertexCount];
                    GrannyCopyMeshVertices(grannyMesh, GrannyPWNT3432VertexType, boneVertices);
                    
                    // For rigid attachments, bind ALL vertices to the attachment bone
                    if (isRigidAttachment && !attachmentBoneName.empty())
                    {
                        // Find or create bone for attachment
                        aiBone* attachmentBone = nullptr;
                        auto attachIt = boneNameToAiBone.find(attachmentBoneName);
                        if (attachIt != boneNameToAiBone.end())
                        {
                            attachmentBone = attachIt->second;
                        }
                        else
                        {
                            // Create a new bone for the attachment
                            auto boneIdxIt = boneNameToIndex.find(attachmentBoneName);
                            if (boneIdxIt != boneNameToIndex.end())
                            {
                                int boneIdx = boneIdxIt->second;
                                granny_bone* grannyBone = &skeleton->Bones[boneIdx];
                                
                                attachmentBone = new aiBone();
                                attachmentBone->mName = attachmentBoneName;
                                
                                // Granny uses row-major, transpose to column-major for correct bone transforms
                                aiMatrix4x4 inverseWorldMatrix(
                                    grannyBone->InverseWorld4x4[0][0], grannyBone->InverseWorld4x4[1][0], grannyBone->InverseWorld4x4[2][0], grannyBone->InverseWorld4x4[3][0],
                                    grannyBone->InverseWorld4x4[0][1], grannyBone->InverseWorld4x4[1][1], grannyBone->InverseWorld4x4[2][1], grannyBone->InverseWorld4x4[3][1],
                                    grannyBone->InverseWorld4x4[0][2], grannyBone->InverseWorld4x4[1][2], grannyBone->InverseWorld4x4[2][2], grannyBone->InverseWorld4x4[3][2],
                                    grannyBone->InverseWorld4x4[0][3], grannyBone->InverseWorld4x4[1][3], grannyBone->InverseWorld4x4[2][3], grannyBone->InverseWorld4x4[3][3]
                                );
                                
                                aiMatrix4x4 meshTransform;
                                if (grannyModel->InitialPlacement.Flags != 0)
                                {
                                    float meshTransformArray[16];
                                    GrannyBuildCompositeTransform4x4(&grannyModel->InitialPlacement, meshTransformArray);
                                    meshTransform = aiMatrix4x4(
                                        // i hate fuckin this shit
                                        meshTransformArray[0], meshTransformArray[1], meshTransformArray[2], meshTransformArray[3],
                                        meshTransformArray[4], meshTransformArray[5], meshTransformArray[6], meshTransformArray[7],
                                        meshTransformArray[8], meshTransformArray[9], meshTransformArray[10], meshTransformArray[11],
                                        meshTransformArray[12], meshTransformArray[13], meshTransformArray[14], meshTransformArray[15]
                                    );
                                }
                                else
                                {
                                    meshTransform = aiMatrix4x4();
                                }
                                
                                attachmentBone->mOffsetMatrix = inverseWorldMatrix * meshTransform;
                                
                                meshBones.push_back(attachmentBone);
                                boneNameToAiBone[attachmentBoneName] = attachmentBone;
                                boneWeightsMap[attachmentBoneName] = std::vector<aiVertexWeight>();
                            }
                        }
                        
                        // Bind all vertices to attachment bone with weight 1.0
                        if (attachmentBone)
                        {
                            Vortigaunt_Printf("DEBUG: Binding all " + std::to_string(assimpMesh->mNumVertices) + " vertices to attachment bone '" + attachmentBoneName + "'");
                            for (unsigned int vIdx = 0; vIdx < assimpMesh->mNumVertices; ++vIdx)
                            {
                                aiVertexWeight weight;
                                weight.mVertexId = vIdx;
                                weight.mWeight = 1.0f;
                                boneWeightsMap[attachmentBoneName].push_back(weight);
                            }
                        }
                    }
                    else
                    {
                        // Normal skinned mesh bone weight processing
                        const std::vector<unsigned int>& originalIndices = m_meshToOriginalIndices[assimpMesh];
                        for (int vIdx = 0; vIdx < static_cast<int>(assimpMesh->mNumVertices); ++vIdx)
                        {
                            // Get the original vertex index from the map we built during mesh creation
                            if (vIdx >= static_cast<int>(originalIndices.size())) continue;
                            unsigned int originalVIdx = originalIndices[vIdx];
                            
                            if (originalVIdx >= static_cast<unsigned int>(vertexCount)) continue;
                            
                            const auto& v = boneVertices[originalVIdx];
                            
                            for (int wIdx = 0; wIdx < 4; ++wIdx)
                            {
                                if (v.BoneWeights[wIdx] > 0)
                                {
                                    int boneBindingIdx = v.BoneIndices[wIdx];
                                    if (boneBindingIdx >= 0 && boneBindingIdx < static_cast<int>(meshBonesByBindingIndex.size()) &&
                                        meshBonesByBindingIndex[boneBindingIdx] != nullptr)
                                    {
                                        aiBone* bone = meshBonesByBindingIndex[boneBindingIdx];
                                        std::string boneName = bone->mName.C_Str();
                                        auto weightsIt = boneWeightsMap.find(boneName);
                                        if (weightsIt != boneWeightsMap.end())
                                        {
                                            aiVertexWeight weight;
                                            weight.mVertexId = vIdx;
                                            weight.mWeight = static_cast<float>(v.BoneWeights[wIdx]) / 255.0f;
                                            weightsIt->second.push_back(weight);
                                        }
                                    }
                                }
                            }
                        }
                    }
                    
                    delete[] boneVertices;
                    
                    // Assign collected weights to bones
                    for (auto& bonePair : boneNameToAiBone)
                    {
                        std::string boneName = bonePair.first;
                        aiBone* bone = bonePair.second;
                        auto& weights = boneWeightsMap[boneName];
                        
                        if (!weights.empty())
                        {
                            bone->mNumWeights = static_cast<unsigned int>(weights.size());
                            bone->mWeights = new aiVertexWeight[weights.size()];
                            for (size_t i = 0; i < weights.size(); ++i)
                            {
                                bone->mWeights[i] = weights[i];
                            }
                        }
                    }
                    
                    // Assign bones to mesh
                    if (!meshBones.empty())
                    {
                        assimpMesh->mNumBones = static_cast<unsigned int>(meshBones.size());
                        assimpMesh->mBones = new aiBone*[meshBones.size()];
                        for (size_t i = 0; i < meshBones.size(); ++i)
                        {
                            assimpMesh->mBones[i] = meshBones[i];
                        }
                        Vortigaunt_Printf("DEBUG: Added " + std::to_string(meshBones.size()) + " bones to mesh '" + std::string(assimpMesh->mName.C_Str()) + "'");
                    }
                }
            }
        }
    }

    Vortigaunt_Printf("DEBUG: Total meshes exported: " + std::to_string(m_meshes.size()));
    return !m_meshes.empty();
}

bool Gr2Converter::convertSkeletonFromGranny()
{
    if (!m_grannyFileInfo || m_grannyFileInfo->ModelCount == 0)
    {
        return true; // No skeleton is OK
    }

    // Process first model's skeleton (most GR2 files have one model)
    granny_model* grannyModel = m_grannyFileInfo->Models[0];
    if (!grannyModel || !grannyModel->Skeleton)
    {
        return true; // No skeleton is OK
    }

    granny_skeleton* skeleton = grannyModel->Skeleton;
    int boneCount = skeleton->BoneCount;

    if (boneCount <= 0)
    {
        return true; // No bones is OK
    }

    // Get InitialPlacement transform for root bone
    float initialPlacementTransform[16] = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    if (grannyModel->InitialPlacement.Flags != 0)
    {
        GrannyBuildCompositeTransform4x4(&grannyModel->InitialPlacement, initialPlacementTransform);
    }
    aiMatrix4x4 initialPlacementMatrix(
        initialPlacementTransform[0], initialPlacementTransform[1], initialPlacementTransform[2], initialPlacementTransform[3],
        initialPlacementTransform[4], initialPlacementTransform[5], initialPlacementTransform[6], initialPlacementTransform[7],
        initialPlacementTransform[8], initialPlacementTransform[9], initialPlacementTransform[10], initialPlacementTransform[11],
        initialPlacementTransform[12], initialPlacementTransform[13], initialPlacementTransform[14], initialPlacementTransform[15]
    );

    // Create bone nodes - with duplicate name handling
    std::map<int, aiNode*> boneIndexToNode;
    std::map<std::string, int> boneNameCount;  // Track bone name occurrences for duplicate handling
    std::map<int, std::string> boneIndexToFinalName;  // Store final names for animation channel consistency
    
    for (int boneIdx = 0; boneIdx < boneCount; ++boneIdx)
    {
        granny_bone* grannyBone = &skeleton->Bones[boneIdx];
        aiNode* boneNode = new aiNode();
        
        // Handle duplicate bone names by adding suffix
        std::string baseName = grannyBone->Name ? grannyBone->Name : "Bone";
        std::string finalName = baseName;
        
        if (boneNameCount.find(baseName) != boneNameCount.end()) {
            // This name already exists, add suffix
            boneNameCount[baseName]++;
            finalName = baseName + "_" + std::to_string(boneNameCount[baseName]);
            Vortigaunt_Printf("WARNING: Duplicate bone name detected, renamed '" + baseName + "' to '" + finalName + "'");
        } else {
            boneNameCount[baseName] = 0;
        }
        
        boneNode->mName = finalName;
        boneIndexToFinalName[boneIdx] = finalName;
        
        // Extract transform components directly from LocalTransform (like reference implementation)
        // This ensures position is correctly extracted
        granny_transform& localTransform = grannyBone->LocalTransform;
        
        // Start with identity transform
        aiVector3D position(0.0f, 0.0f, 0.0f);
        aiQuaternion rotation(1.0f, 0.0f, 0.0f, 0.0f); // Identity quaternion
        aiVector3D scaling(1.0f, 1.0f, 1.0f);
        
        // Extract position if present
        if (localTransform.Flags & GrannyHasPosition)
        {
            position = aiVector3D(
                localTransform.Position[0],
                localTransform.Position[1],
                localTransform.Position[2]
            );
        }
        
        // Extract orientation if present
        // Granny stores quaternion as (x, y, z, w), but aiQuaternion expects (w, x, y, z)
        if (localTransform.Flags & GrannyHasOrientation)
        {
            rotation = aiQuaternion(
                localTransform.Orientation[3], // w
                localTransform.Orientation[0], // x
                localTransform.Orientation[1], // y
                localTransform.Orientation[2]  // z
            );
            // Normalize quaternion to prevent animation jitter from floating point errors
            rotation.Normalize();
        }
        
        // Extract scale if present
        if (localTransform.Flags & GrannyHasScaleShear)
        {
            scaling = aiVector3D(
                localTransform.ScaleShear[0][0],
                localTransform.ScaleShear[1][1],
                localTransform.ScaleShear[2][2]
            );
        }
        
        // Build transform matrix from components
        aiMatrix4x4 finalTransform(scaling, rotation, position);

        // Apply InitialPlacement to root bone only
        // Root bone has ParentIndex == GrannyNoParentBone (-1)
        if (grannyBone->ParentIndex == GrannyNoParentBone)
        {
            // Build InitialPlacement transform from components (like reference implementation)
            granny_transform& initialPlacement = grannyModel->InitialPlacement;
            
            aiVector3D initialPosition(0.0f, 0.0f, 0.0f);
            aiQuaternion initialRotation(1.0f, 0.0f, 0.0f, 0.0f);
            aiVector3D initialScaling(1.0f, 1.0f, 1.0f);
            
            if (initialPlacement.Flags & GrannyHasPosition)
            {
                initialPosition = aiVector3D(
                    initialPlacement.Position[0],
                    initialPlacement.Position[1],
                    initialPlacement.Position[2]
                );
            }
            
            if (initialPlacement.Flags & GrannyHasOrientation)
            {
                // Granny stores quaternion as (x, y, z, w), but aiQuaternion expects (w, x, y, z)
                initialRotation = aiQuaternion(
                    initialPlacement.Orientation[3], // w
                    initialPlacement.Orientation[0], // x
                    initialPlacement.Orientation[1], // y
                    initialPlacement.Orientation[2]  // z
                );
            }
            
            if (initialPlacement.Flags & GrannyHasScaleShear)
            {
                initialScaling = aiVector3D(
                    initialPlacement.ScaleShear[0][0],
                    initialPlacement.ScaleShear[1][1],
                    initialPlacement.ScaleShear[2][2]
                );
            }
            
            aiMatrix4x4 initialTransform(initialScaling, initialRotation, initialPosition);
            
            // Reference implementation: initialTransform * finalTransform
            // This means InitialPlacement is applied first, then local transform
            boneNode->mTransformation = initialTransform * finalTransform;
        }
        else
        {
            boneNode->mTransformation = finalTransform;
        }

        boneIndexToNode[boneIdx] = boneNode;
        m_skeletonNodes.push_back(boneNode);
    }

    // Build hierarchy
    for (int boneIdx = 0; boneIdx < boneCount; ++boneIdx)
    {
        granny_bone* grannyBone = &skeleton->Bones[boneIdx];
        aiNode* boneNode = boneIndexToNode[boneIdx];
        
        int parentIndex = grannyBone->ParentIndex;
        if (parentIndex >= 0 && parentIndex < boneCount)
        {
            aiNode* parentNode = boneIndexToNode[parentIndex];
            boneNode->mParent = parentNode;
            // Add to parent's children
            if (parentNode->mNumChildren == 0)
            {
                parentNode->mChildren = new aiNode*[1];
                parentNode->mChildren[0] = boneNode;
                parentNode->mNumChildren = 1;
            }
            else
            {
                // Reallocate children array
                aiNode** newChildren = new aiNode*[parentNode->mNumChildren + 1];
                for (unsigned int i = 0; i < parentNode->mNumChildren; ++i)
                {
                    newChildren[i] = parentNode->mChildren[i];
                }
                newChildren[parentNode->mNumChildren] = boneNode;
                delete[] parentNode->mChildren;
                parentNode->mChildren = newChildren;
                parentNode->mNumChildren++;
            }
        }
    }

    // Add root bone to scene root
    if (!m_skeletonNodes.empty())
    {
        // Find root bones (bones without parent)
        std::vector<aiNode*> rootBones;
        for (auto* node : m_skeletonNodes)
        {
            if (!node->mParent)
            {
                rootBones.push_back(node);
            }
        }

        if (!rootBones.empty())
        {
            m_assimpScene->mRootNode->addChildren(static_cast<unsigned int>(rootBones.size()), rootBones.data());
        }
    }

    return true;
}

bool Gr2Converter::convertMaterialsFromGranny()
{
    if (!m_grannyFileInfo || m_grannyFileInfo->TextureCount == 0)
    {
        return true; // No materials is OK
    }

    // Create a simple material for each texture
    m_assimpScene->mNumMaterials = m_grannyFileInfo->TextureCount;
    std::vector<aiMaterial*> materials;
    
    for (int texIdx = 0; texIdx < m_grannyFileInfo->TextureCount; ++texIdx)
    {
        granny_texture* grannyTex = m_grannyFileInfo->Textures[texIdx];
        if (!grannyTex)
        {
            continue;
        }

        aiMaterial* material = new aiMaterial();
        material->mNumAllocated = 1;
        
        // Set material name from texture filename
        if (grannyTex->FromFileName)
        {
            // Extract filename from path for material name
            std::string texPath = grannyTex->FromFileName;
            size_t lastSlash = texPath.find_last_of("/\\");
            std::string materialName = (lastSlash != std::string::npos) 
                ? texPath.substr(lastSlash + 1) 
                : texPath;
            
            // Remove extension
            size_t lastDot = materialName.find_last_of('.');
            if (lastDot != std::string::npos)
            {
                materialName = materialName.substr(0, lastDot);
            }
            
            aiString name(materialName.c_str());
            material->AddProperty(&name, AI_MATKEY_NAME);
            
            // Set texture path
            aiString texPathStr(grannyTex->FromFileName);
            material->AddProperty(&texPathStr, AI_MATKEY_TEXTURE_DIFFUSE(0));
        }
        else
        {
            // Default material name if no texture file
            aiString name("Material");
            material->AddProperty(&name, AI_MATKEY_NAME);
        }

        materials.push_back(material);
    }

    if (materials.empty())
    {
        m_assimpScene->mNumMaterials = 0;
        return true;
    }

    m_assimpScene->mMaterials = new aiMaterial*[materials.size()];
    for (size_t i = 0; i < materials.size(); ++i)
    {
        m_assimpScene->mMaterials[i] = materials[i];
    }
    m_assimpScene->mNumMaterials = static_cast<unsigned int>(materials.size());

    return true;
}

bool Gr2Converter::convertAnimationsFromGranny(granny_model* modelToSample)
{
    if (!m_grannyFileInfo || m_grannyFileInfo->AnimationCount == 0)
    {
        return true;
    }

    // Use provided model, or try to get from file info
    if (!modelToSample)
    {
        if (m_grannyFileInfo->ModelCount > 0)
        {
            modelToSample = m_grannyFileInfo->Models[0];
        }
        else
        {
            // If no model in file, we need a skeleton at least to create a dummy model
            // But usually this function is called with a model from skeleton file
            return true;
        }
    }

    if (!modelToSample || !modelToSample->Skeleton)
    {
        return true;
    }

    granny_skeleton* skeleton = modelToSample->Skeleton;
    granny_model_instance* modelInstance = GrannyInstantiateModel(modelToSample);
    granny_local_pose* localPose = GrannyNewLocalPose(skeleton->BoneCount);

    // Process each animation
    for (int animIdx = 0; animIdx < m_grannyFileInfo->AnimationCount; ++animIdx)
    {
        granny_animation* grannyAnim = m_grannyFileInfo->Animations[animIdx];
        if (!grannyAnim) continue;

        aiAnimation* assimpAnim = new aiAnimation();
        assimpAnim->mName = grannyAnim->Name ? grannyAnim->Name : ("Animation_" + std::to_string(animIdx));
        
        // Use 30 FPS for sampling consistent with SmdWriter
        const float samplingFPS = 30.0f;
        const float samplingTimeStep = 1.0f / samplingFPS;
        
        assimpAnim->mDuration = grannyAnim->Duration;
        assimpAnim->mTicksPerSecond = samplingFPS;

        // Play the animation on the model instance
        granny_control* control = GrannyPlayControlledAnimation(0.0f, grannyAnim, modelInstance);
        if (!control) {
            delete assimpAnim;
            continue;
        }
        GrannySetControlLoopCount(control, 1);

        // Sample at regular intervals
        int totalSteps = static_cast<int>(std::ceil(grannyAnim->Duration * samplingFPS)) + 1;
        if (totalSteps < 1) totalSteps = 1;
        
        // Prepare node animations (one for each bone)
        std::vector<aiNodeAnim*> nodeAnims(skeleton->BoneCount);
        for (int bIdx = 0; bIdx < skeleton->BoneCount; ++bIdx) {
            nodeAnims[bIdx] = new aiNodeAnim();
            nodeAnims[bIdx]->mNodeName = skeleton->Bones[bIdx].Name;
            
            nodeAnims[bIdx]->mNumPositionKeys = totalSteps;
            nodeAnims[bIdx]->mPositionKeys = new aiVectorKey[totalSteps];
            nodeAnims[bIdx]->mNumRotationKeys = totalSteps;
            nodeAnims[bIdx]->mRotationKeys = new aiQuatKey[totalSteps];
            nodeAnims[bIdx]->mNumScalingKeys = totalSteps;
            nodeAnims[bIdx]->mScalingKeys = new aiVectorKey[totalSteps];
        }

        // Sampling pass
        for (int s = 0; s < totalSteps; ++s) {
            float currentTime = static_cast<float>(s) * samplingTimeStep;
            if (currentTime > grannyAnim->Duration) currentTime = grannyAnim->Duration;
            
            GrannySetControlClock(control, currentTime);
            GrannySampleModelAnimations(modelInstance, 0, skeleton->BoneCount, localPose);
            
            for (int bIdx = 0; bIdx < skeleton->BoneCount; ++bIdx) {
                granny_transform* transform = GrannyGetLocalPoseTransform(localPose, bIdx);
                
                nodeAnims[bIdx]->mPositionKeys[s].mTime = currentTime;
                nodeAnims[bIdx]->mPositionKeys[s].mValue = aiVector3D(transform->Position[0], transform->Position[1], transform->Position[2]);
                
                nodeAnims[bIdx]->mRotationKeys[s].mTime = currentTime;
                // Normalize quaternion to prevent animation jitter from floating point errors
                aiQuaternion animQuat(transform->Orientation[3], transform->Orientation[0], transform->Orientation[1], transform->Orientation[2]);
                animQuat.Normalize();
                nodeAnims[bIdx]->mRotationKeys[s].mValue = animQuat;
                
                nodeAnims[bIdx]->mScalingKeys[s].mTime = currentTime;
                nodeAnims[bIdx]->mScalingKeys[s].mValue = aiVector3D(transform->ScaleShear[0][0], transform->ScaleShear[1][1], transform->ScaleShear[2][2]);
            }
        }

        assimpAnim->mNumChannels = skeleton->BoneCount;
        assimpAnim->mChannels = new aiNodeAnim*[skeleton->BoneCount];
        for (int bIdx = 0; bIdx < skeleton->BoneCount; ++bIdx) {
            assimpAnim->mChannels[bIdx] = nodeAnims[bIdx];
        }

        GrannyFreeControl(control);
        m_animations.push_back(assimpAnim);
    }

    GrannyFreeLocalPose(localPose);
    GrannyFreeModelInstance(modelInstance);

    return true;
}

bool Gr2Converter::exportTextures(const std::string& outputDir, const std::string& smdFilePath)
{
    if (!m_grannyFileInfo || m_grannyFileInfo->TextureCount == 0)
    {
        return true; // No textures is OK
    }

    std::filesystem::path outputPath = FileIO::toPath(outputDir);
    
    std::vector<std::string> exportedTexturePaths;
    int exportedCount = 0;
    for (int texIdx = 0; texIdx < m_grannyFileInfo->TextureCount; ++texIdx)
    {
        granny_texture* grannyTex = m_grannyFileInfo->Textures[texIdx];
        if (!grannyTex || !grannyTex->FromFileName)
        {
            continue;
        }

        std::string sourceTexPath = grannyTex->FromFileName;
        std::filesystem::path sourcePath = FileIO::toPath(sourceTexPath);
        
        // Try to find the texture file
        std::filesystem::path actualSourcePath;
        
        // First, try the path as-is
        if (std::filesystem::exists(sourcePath))
        {
            actualSourcePath = sourcePath;
        }
        else
        {
            // Try just the filename in the GR2 file's directory
            if (!s_currentGr2FileDir.empty())
            {
                std::filesystem::path tryPath = FileIO::toPath(s_currentGr2FileDir) / sourcePath.filename();
                if (std::filesystem::exists(tryPath))
                {
                    actualSourcePath = tryPath;
                }
            }
            
            // If still not found, try just the filename in current directory
            if (actualSourcePath.empty() && std::filesystem::exists(sourcePath.filename()))
            {
                actualSourcePath = sourcePath.filename();
            }
        }

        if (!actualSourcePath.empty() && std::filesystem::exists(actualSourcePath))
        {
            // Determine output texture filename
            std::string texName = String_UTF16toUTF8(sourcePath.stem().generic_u16string());
            // Sanitize filename (remove invalid characters)
            for (char& c : texName)
            {
                if (c == ' ' || c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
                {
                    c = '_';
                }
            }
            
            std::filesystem::path targetPath = outputPath / FileIO::toPath(texName + ".bmp");
            
            try
            {
                // Convert texture to 8-bit indexed color BMP
                if (!convertTextureto8IndexedBMP(String_UTF16toUTF8(actualSourcePath.generic_u16string()), String_UTF16toUTF8(targetPath.generic_u16string())))
                {
                    Vortigaunt_Printf("WARNING: Failed to convert texture " + String_UTF16toUTF8(sourcePath.filename().generic_u16string()) + " to 8-bit BMP");
                }
                else
                {
                    exportedTexturePaths.push_back(String_UTF16toUTF8(targetPath.generic_u16string()));
                    exportedCount++;
                }
            }
            catch (const std::exception& e)
            {
                Vortigaunt_Printf("WARNING: Failed to export texture " + String_UTF16toUTF8(sourcePath.filename().generic_u16string()) + " : " + e.what());
            }
        }
        else
        {
            Vortigaunt_Printf("WARNING: Texture file not found: " + sourceTexPath);
        }
    }

    if (exportedCount > 0)
    {
        // If SMD file exists, rename textures to match SMD texture names
        if (!smdFilePath.empty() && std::filesystem::exists(FileIO::toPath(smdFilePath)))
        {
            std::vector<std::string> smdTextureNames = parseSmdTextureNames(smdFilePath);
            if (!smdTextureNames.empty())
            {
                renameTexturesToMatchSmd(outputDir, smdTextureNames, exportedTexturePaths);
            }
        }
    }
    
    return true;
}

std::vector<std::string> Gr2Converter::parseSmdTextureNames(const std::string& smdFilePath)
{
    std::vector<std::string> textureNames;
    
    if (!std::filesystem::exists(FileIO::toPath(smdFilePath)))
    {
        Vortigaunt_Printf("WARNING: SMD file not found : " + smdFilePath);
        return textureNames;
    }
    
    std::ifstream file(FileIO::toPath(smdFilePath));
    if (!file.is_open())
    {
        Vortigaunt_Printf("WARNING: Failed to open SMD file: " + smdFilePath);
        return textureNames;
    }
    
    std::string line;
    bool inTrianglesSection = false;
    std::set<std::string> uniqueTextureNames;
    
    while (std::getline(file, line))
    {
        // Trim whitespace
        line.erase(0, line.find_first_not_of(" \t\r\n"));
        line.erase(line.find_last_not_of(" \t\r\n") + 1);
        
        // Check if we're entering triangles section
        if (line == "triangles")
        {
            inTrianglesSection = true;
            continue;
        }
        
        // Check if we're leaving triangles section
        if (inTrianglesSection && line == "end")
        {
            inTrianglesSection = false;
            continue;
        }
        
        // In triangles section, each triangle starts with a texture name
        if (inTrianglesSection && !line.empty())
        {
            std::string textureName = line;
            
            // Trim whitespace
            textureName.erase(0, textureName.find_first_not_of(" \t"));
            textureName.erase(textureName.find_last_not_of(" \t") + 1);
            
            if (!textureName.empty())
            {
                // Remove any path separators, keep just the filename
                size_t lastSlash = textureName.find_last_of("/\\");
                if (lastSlash != std::string::npos)
                {
                    textureName = textureName.substr(lastSlash + 1);
                }
                
                // Store unique texture names in order of first appearance
                if (uniqueTextureNames.find(textureName) == uniqueTextureNames.end())
                {
                    uniqueTextureNames.insert(textureName);
                    textureNames.push_back(textureName);
                }
            }
            
            // Skip the next 3 lines (vertex data for the triangle)
            // Each vertex is on one line with format: boneIndex x y z nx ny nz u v
            for (int i = 0; i < 3 && std::getline(file, line); ++i)
            {
                // Just skip these lines  they contain vertex data
            }
        }
    }
    
    file.close();
    
    return textureNames;
}

void Gr2Converter::renameTexturesToMatchSmd(const std::string& outputDir, const std::vector<std::string>& smdTextureNames, const std::vector<std::string>& exportedTexturePaths)
{
    if (smdTextureNames.empty() || exportedTexturePaths.empty())
    {
        Vortigaunt_Printf("DEBUG: renameTexturesToMatchSmd: SMD texture names or exported paths empty");
        return;
    }
    
    Vortigaunt_Printf("DEBUG: renameTexturesToMatchSmd: SMD texture names count: " + std::to_string(smdTextureNames.size()));
    Vortigaunt_Printf("DEBUG: renameTexturesToMatchSmd: Exported texture paths count: " + std::to_string(exportedTexturePaths.size()));
    
    std::filesystem::path outputPath = FileIO::toPath(outputDir);
    int renamedCount = 0;
    
    // Create a map of exported texture paths by their base name (without extension)
    // Key: texture base name (lowercase for case-insensitive matching)
    // Value: full path to exported texture
    std::map<std::string, std::filesystem::path> exportedTextureMap;
    for (const auto& exportedPath : exportedTexturePaths)
    {
        std::filesystem::path path = FileIO::toPath(exportedPath);
        if (std::filesystem::exists(path))
        {
            std::string baseName = String_UTF16toUTF8(path.stem().generic_u16string());
            // Convert to lowercase for case-insensitive matching
            std::transform(baseName.begin(), baseName.end(), baseName.begin(), [](unsigned char c){ return std::tolower(c); });
            exportedTextureMap[baseName] = path;
            Vortigaunt_Printf("DEBUG:   Exported texture: '" + baseName + "' -> " + String_UTF16toUTF8(path.generic_u16string()));
        }
    }
    
    // Create a map of mesh names to material indices from the scene
    std::map<std::string, int> meshNameToMaterialIndex;
    if (m_assimpScene)
    {
        Vortigaunt_Printf("DEBUG:   Scene has " + std::to_string(m_assimpScene->mNumMeshes) + " meshes");
        for (unsigned int meshIdx = 0; meshIdx < m_assimpScene->mNumMeshes; ++meshIdx)
        {
            aiMesh* mesh = m_assimpScene->mMeshes[meshIdx];
            if (mesh)
            {
                std::string meshName = mesh->mName.C_Str();
                std::transform(meshName.begin(), meshName.end(), meshName.begin(), [](unsigned char c){ return std::tolower(c); });
                meshNameToMaterialIndex[meshName] = mesh->mMaterialIndex;
                Vortigaunt_Printf("DEBUG:     Mesh '" + std::string(mesh->mName.C_Str()) + "' -> material index " + std::to_string(mesh->mMaterialIndex));
            }
        }
    }
    
    // Match SMD texture names to exported textures
    // SMD texture names are based on mesh names (e.g., "maticore_general.bmp" for mesh "maticore_general")
    // We need to match mesh names to texture file names
    std::set<std::filesystem::path> usedTextures; // Track which textures we've already renamed
    
    // Create a map of mesh names to their order (for fallback matching)
    std::map<std::string, int> meshNameToOrder;
    if (m_assimpScene)
    {
        int meshOrder = 0;
        for (unsigned int meshIdx = 0; meshIdx < m_assimpScene->mNumMeshes; ++meshIdx)
        {
            aiMesh* mesh = m_assimpScene->mMeshes[meshIdx];
            if (mesh)
            {
                std::string meshName = mesh->mName.C_Str();
                std::transform(meshName.begin(), meshName.end(), meshName.begin(), [](unsigned char c){ return std::tolower(c); });
                meshNameToOrder[meshName] = meshOrder++;
            }
        }
    }
    
    for (const auto& smdTexName : smdTextureNames)
    {
        // Get SMD texture name without extension
        std::string smdBaseName = smdTexName;
        size_t dotPos = smdBaseName.find_last_of('.');
        if (dotPos != std::string::npos)
        {
            smdBaseName = smdBaseName.substr(0, dotPos);
        }
        std::transform(smdBaseName.begin(), smdBaseName.end(), smdBaseName.begin(), [](unsigned char c){ return std::tolower(c); });
        
        Vortigaunt_Printf("DEBUG:   Processing SMD texture name: '" + smdTexName + "' (base: '" + smdBaseName + "')");
        
        // Try to find matching exported texture
        std::filesystem::path matchingTexturePath;
        
        // First Try: Match by mesh name -> material index -> texture index
        auto meshIt = meshNameToMaterialIndex.find(smdBaseName);
        if (meshIt != meshNameToMaterialIndex.end())
        {
            int materialIndex = meshIt->second;
            Vortigaunt_Printf("DEBUG: Found mesh '" + smdBaseName + "' with material index " + std::to_string(materialIndex));
            // Material index should correspond to texture index in GR2
            if (materialIndex >= 0 && materialIndex < static_cast<int>(exportedTexturePaths.size()))
            {
                std::filesystem::path candidatePath = FileIO::toPath(exportedTexturePaths[materialIndex]);
                if (std::filesystem::exists(candidatePath) && usedTextures.find(candidatePath) == usedTextures.end())
                {
                    matchingTexturePath = candidatePath;
                    Vortigaunt_Printf("DEBUG: Matched via material index: " + String_UTF16toUTF8(candidatePath.generic_u16string()));
                }
            }
            else
            {
                Vortigaunt_Printf("DEBUG: Material index " + std::to_string(materialIndex) + " out of range (max: " + std::to_string(exportedTexturePaths.size()) + ")");
            }
        }
        
        // Second Try: Direct name match (SMD texture name matches exported texture base name)
        if (matchingTexturePath.empty())
        {
            auto it = exportedTextureMap.find(smdBaseName);
            if (it != exportedTextureMap.end() && usedTextures.find(it->second) == usedTextures.end())
            {
                matchingTexturePath = it->second;
                Vortigaunt_Printf("DEBUG: Matched via direct name: " + String_UTF16toUTF8(matchingTexturePath.generic_u16string()));
            }
            else
            {
                Vortigaunt_Printf("DEBUG: No direct name match found for '" + smdBaseName + "'");
            }
        }
        
        // Third Try: Match by mesh order (if mesh name matches SMD texture name)
        // i hate granny2 sdk
        if (matchingTexturePath.empty())
        {
            auto meshOrderIt = meshNameToOrder.find(smdBaseName);
            if (meshOrderIt != meshNameToOrder.end())
            {
                int meshOrder = meshOrderIt->second;
                Vortigaunt_Printf("DEBUG: Found mesh order: " + std::to_string(meshOrder));
                // Try to match by order: first mesh -> first texture, etc.
                if (meshOrder >= 0 && meshOrder < static_cast<int>(exportedTexturePaths.size()))
                {
                    std::filesystem::path candidatePath = FileIO::toPath(exportedTexturePaths[meshOrder]);
                    if (std::filesystem::exists(candidatePath) && usedTextures.find(candidatePath) == usedTextures.end())
                    {
                        matchingTexturePath = candidatePath;
                        Vortigaunt_Printf("DEBUG:  Matched via mesh order: " + String_UTF16toUTF8(matchingTexturePath.generic_u16string()));
                    }
                }
            }
        }
        
        // If we found a matching texture, rename it
        if (!matchingTexturePath.empty() && std::filesystem::exists(matchingTexturePath))
        {
            // Ensure SMD texture name has .bmp extension
            std::string finalSmdName = smdTexName;
            if (finalSmdName.length() < 4 || finalSmdName.substr(finalSmdName.length() - 4) != ".bmp")
            {
                finalSmdName += ".bmp";
            }
            
            std::filesystem::path newPath = outputPath / FileIO::toPath(finalSmdName);
            
            // If the new name is different from the old name, rename it
            if (matchingTexturePath.filename() != newPath.filename())
            {
                try
                {
                    // If target file already exists, remove it first
                    if (std::filesystem::exists(newPath))
                    {
                        std::filesystem::remove(newPath);
                    }
                    
                    std::filesystem::rename(matchingTexturePath, newPath);
                    usedTextures.insert(matchingTexturePath);
                    renamedCount++;
                }
                catch (const std::exception& e)
                {
                    Vortigaunt_Printf("WARNING: Failed to rename texture " + String_UTF16toUTF8(matchingTexturePath.filename().generic_u16string()) + " to " + finalSmdName + ": " + e.what());
                }
            }
        }
    }
    
}



bool Gr2Converter::convertTextureto8IndexedBMP(const std::string& inputPath, const std::string& outputPath)
{
#ifdef QT_WIDGETS_LIB

    QImage sourceImage;
        
    // Check if it's a DDS file
    bool isDds = DDS::isDdsFile(inputPath);
    if (isDds)
    {
        // Use unified DDS decoder from dds
        sourceImage = DDS::loadDdsToQImage(QString::fromStdString(inputPath));
            
        if (sourceImage.isNull())
        {
            Vortigaunt_Printf("ERROR: Failed to decode DDS file: " + String_UTF16toUTF8(FileIO::toPath(inputPath).filename().generic_u16string()));
            return false;
        }
    }
    else
    {
        Vortigaunt_Printf("ERROR: Failed to load: The file isnt DDS");
        return false;
    }
        
    if (sourceImage.isNull())
    {
        Vortigaunt_Printf("ERROR: Failed to load image : " + inputPath);
        return false;
    }

    // Invert alpha channel if requested (for DDS textures with inverted alpha)
    if (m_settings.InvertAlpha && sourceImage.hasAlphaChannel())
    {
        // Convert to ARGB32 to ensure we can manipulate alpha
        QImage argbImage = sourceImage.convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < argbImage.height(); ++y)
        {
            QRgb* scanLine = reinterpret_cast<QRgb*>(argbImage.scanLine(y));
            for (int x = 0; x < argbImage.width(); ++x)
            {
                int alpha = qAlpha(scanLine[x]);
                int invertedAlpha = 255 - alpha;
                scanLine[x] = qRgba(qRed(scanLine[x]), qGreen(scanLine[x]), qBlue(scanLine[x]), invertedAlpha);
            }
        }
        sourceImage = argbImage;
        Vortigaunt_Printf("  Applied alpha inversion to texture");
    }

    // Use octree quantisation (same as CLI) for best quality
    QImage argb = sourceImage.convertToFormat(QImage::Format_ARGB32);
    if (!BMP::saveAsIndexed8(outputPath.c_str(),
                                  argb.width(), argb.height(),
                                  reinterpret_cast<const uint32_t*>(argb.constBits())))
    {
        Vortigaunt_Printf("  ERROR: Failed to save 8-bit BMP file: " + outputPath);
        return false;
    }

    return true;
    

#else
    // CLI build - Qt not available, just copy the file
    Vortigaunt_Printf("WARNING: Qt not available in CLI build, texture conversion to 8-bit BMP not supported");
    try
    {
        std::filesystem::copy_file(FileIO::toPath(inputPath), FileIO::toPath(outputPath), std::filesystem::copy_options::overwrite_existing);
        return true;
    }
    catch (const std::exception& e)
    {
        Vortigaunt_Printf("  ERROR: Failed to copy texture: " + std::string(e.what()));
        return false;
    }
#endif
}

// Get texture from a material using Granny2 SDK conventions
granny_texture* Gr2Converter::getTextureFromMaterial(granny_material* material)
{
    if (!material)
        return nullptr;
    
    if (material->MapCount > 0 && material->Maps[0].Material && material->Maps[0].Material->Texture)
    {
        return material->Maps[0].Material->Texture;
    }
    else if (material->Texture)
    {
        return material->Texture;
    }
    else if (material->MapCount > 0 && material->Maps[0].Material &&
             material->Maps[0].Material->MapCount > 0 &&
             material->Maps[0].Material->Maps[0].Material &&
             material->Maps[0].Material->Maps[0].Material->Texture)
    {
        return material->Maps[0].Material->Maps[0].Material->Texture;
    }
    
    return nullptr;
}

// Get base filename from texture path
std::string Gr2Converter::getTextureBaseName(granny_texture* texture)
{
    if (!texture || !texture->FromFileName)
        return "unnamed";
    
    std::filesystem::path texPath = FileIO::toPath(std::string(texture->FromFileName));
    return texPath.stem().generic_string();
}

// Find texture index in file info
int Gr2Converter::findTextureIndex(granny_texture* texture)
{
    if (!m_grannyFileInfo || !texture)
        return -1;
    
    for (int i = 0; i < m_grannyFileInfo->TextureCount; ++i)
    {
        if (m_grannyFileInfo->Textures[i] == texture)
            return i;
    }
    return -1;
}

void Gr2Converter::clearData()
{
    if (m_grannyFile)
    {
        GrannyFreeFile(m_grannyFile);
        m_grannyFile = nullptr;
        m_grannyFileInfo = nullptr;
        m_grannyBuffer.clear();
    }

    m_meshes.clear();
    m_skeletonNodes.clear();
    m_animations.clear();
    m_meshToOriginalIndices.clear();
    
    m_assimpScene = nullptr;
}

