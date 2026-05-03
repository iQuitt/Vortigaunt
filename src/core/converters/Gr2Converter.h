#pragma once

#include <string>
#include <memory>
#include <vector>

#include <map>

#ifdef ENABLE_GRANNY2

// Forward declarations
struct granny_file;
struct granny_file_info;
struct granny_model;
struct granny_mesh;
struct granny_animation;
struct granny_skeleton;
struct granny_texture;
struct granny_material;

class SmdWriter;

enum Gr2ConvertResult
{
    GR2_CONVERT_RET_OK = 0,
    GR2_CONVERT_RET_INVALID_INPUT_FILE = 1,
    GR2_CONVERT_RET_LOAD_FAILED = 2,
    GR2_CONVERT_RET_NO_MESH = 3,
    GR2_CONVERT_RET_EXPORT_FAILED = 4,
};

/**
 * @brief Settings for GR2 model conversion
 */
struct Gr2ConverterSettings
{
    bool ExportMeshes = true;
    bool ExportAnimations = true;
    bool ExportSkeleton = true;
    bool ExportMaterials = true;
    bool DeveloperMode = false; // Enable detailed debug messages
    bool MirrorUVY = true; // Mirror UV Y-axis (default true for backward compatibility)
    bool InvertAlpha = false; // Invert alpha channel for DDS textures
    

};

/**
 * @brief Converts Granny2 GR2 models to SMD/MAP format for GoldSrc
 */
class Gr2Converter
{
public:

    
    Gr2Converter();
    ~Gr2Converter();

    int ConvertSingleGr2File(const std::string& inputFilePath, const std::string& outputFilePath);
    int ConvertGr2Animation(const std::string& inputFilePath, const std::string& outputFilePath, int animationIndex, const std::string& skeletonSourceFile = "");
    std::vector<std::string> GetAnimationNames(const std::string& inputFilePath);
    int GetPolygonCount(const std::string& inputFilePath);
    void SetConverterSettings(const Gr2ConverterSettings& settings);
    
    // Direct GR2 to Assimp scene conversion (for MAP export without SMD intermediate)
    // Returns the scene pointer - caller must NOT delete it, Gr2Converter owns it
    struct aiScene* LoadAndGetScene(const std::string& inputFilePath, const std::string& textureOutputDir);
    
public:


private:

    bool loadGr2File(const std::string& inputFilePath);
    bool convertToAssimpScene();
    bool convertMeshesFromGranny();
    bool convertSkeletonFromGranny();
    bool convertMaterialsFromGranny();
    bool convertAnimationsFromGranny(struct granny_model* modelToSample = nullptr);
    bool convertToSmd(const std::string& outputFilePath);
    bool exportTextures(const std::string& outputDir, const std::string& smdFilePath = "");
    bool convertTextureto8IndexedBMP(const std::string& inputPath, const std::string& outputPath);
    std::vector<std::string> parseSmdTextureNames(const std::string& smdFilePath);
    void renameTexturesToMatchSmd(const std::string& outputDir, const std::vector<std::string>& smdTextureNames, const std::vector<std::string>& exportedTexturePaths);
    void clearData();
    
    // for multi-material mesh processing
    granny_texture* getTextureFromMaterial(granny_material* material);
    std::string getTextureBaseName(granny_texture* texture);
    int findTextureIndex(granny_texture* texture);

    // Granny2 SDK data
    granny_file* m_grannyFile;
    granny_file_info* m_grannyFileInfo;
    std::vector<char> m_grannyBuffer;
    
    // Assimp scene for export
    struct aiScene* m_assimpScene;
    
    // Map of Assimp mesh to original Granny vertex indices (for bone weight remapping)
    std::map<struct aiMesh*, std::vector<unsigned int>> m_meshToOriginalIndices;

    SmdWriter* m_smdWriter;
    Gr2ConverterSettings m_settings;
    
    // for Assimp conversion
    std::vector<struct aiMesh*> m_meshes;
    std::vector<struct aiNode*> m_skeletonNodes;
    std::vector<struct aiAnimation*> m_animations;
};

#else // ENABLE_GRANNY2 


// NOTE: Granny2 SDK Does Not Support Linux so here is the stubs for fix the compile error


// Stub enum for compatibility
enum Gr2ConvertResult
{
    GR2_CONVERT_RET_OK = 0,
    GR2_CONVERT_RET_INVALID_INPUT_FILE = 1,
    GR2_CONVERT_RET_LOAD_FAILED = 2,
    GR2_CONVERT_RET_NO_MESH = 3,
    GR2_CONVERT_RET_EXPORT_FAILED = 4,
};

/**
 * @brief Stub settings for GR2 conversion (Linux compatibility)
 */
struct Gr2ConverterSettings
{
    bool ExportMeshes = true;
    bool ExportAnimations = true;
    bool ExportSkeleton = true;
    bool ExportMaterials = true;
    bool DeveloperMode = false;
    bool MirrorUVY = true;
    bool InvertAlpha = false;
    bool ExportToMap = false;

};

/**
 * @brief Stub: GR2 converter. 
 */
class Gr2Converter
{
public:

    
    Gr2Converter() {}
    ~Gr2Converter() {}

    int ConvertSingleGr2File(const std::string&, const std::string&) { return GR2_CONVERT_RET_LOAD_FAILED; }
    int ConvertGr2Animation(const std::string&, const std::string&, int, const std::string& = "") { return GR2_CONVERT_RET_LOAD_FAILED; }
    std::vector<std::string> GetAnimationNames(const std::string&) { return {}; }
    int GetPolygonCount(const std::string&) { return 0; }
    void SetConverterSettings(const Gr2ConverterSettings&) {}
    struct aiScene* LoadAndGetScene(const std::string&, const std::string&) { return nullptr; }
    

};

#endif // ENABLE_GRANNY2
