// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once



// assimp old and suck thats why this file exist

#include <string>
#include <vector>

#include "core/converters/IConverter.h"

struct FbxGlbConvertOptions
{
    bool writeQC = true;
    bool exportTextures = true;


    bool reorient = true;


    // MAXSTUDIOBONES
    int maxBones = 128;

    int fps = 30;

    int maxTextureSize = 512;
};

struct FbxGlbAnimationInfo
{
    std::string name;
    std::string file;
    int frames = 0;
};

struct FbxGlbConvertReport
{
    bool ok = false;
    std::string error;

    std::string outputFolder;
    std::string referenceSmd;  // empty when the file had no mesh
    std::string skeletonSmd;   
    std::string qcPath;

    int sourceNodes = 0;
    int bones = 0;
    int mergedBones = 0;
    int meshes = 0;
    int skinnedMeshes = 0;
    int triangles = 0;
    int uniqueVertices = 0;
    float size[3] = { 0.0f, 0.0f, 0.0f };  //xyz

    bool animationOnly = false;
    std::vector<FbxGlbAnimationInfo> animations;

    int texturesWritten = 0;      // converted from a texture the model named
    int texturesGuessed = 0;      // picked from the folder by name
    int texturesPlaceholder = 0;  // none found, a flat colour stands in

    std::vector<std::string> warnings;
};

class FbxGlbConverter : public IConverter
{
public:

    static bool IsSupportedFile(const std::string& path);


    FbxGlbConvertReport Convert(const std::string& inputPath, const std::string& outputDir, const FbxGlbConvertOptions& options = FbxGlbConvertOptions());

    void SetOptions(const FbxGlbConvertOptions& options) { m_options = options; }

    const char* Name() const override { return "FBX/GLB/OBJ"; }
    bool CanConvert(const std::string& path) const override { return IsSupportedFile(path); }
    ConvertResult ConvertFile(const std::string& inputPath, const std::string& outputDir) override;

private:
    FbxGlbConvertOptions m_options;
};
