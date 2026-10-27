// Copyright (C) 2026 iQuit (https://github.com/iQuitt) 
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once


#include <assimp/types.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

// from studio.h
constexpr int STUDIO_NF_FLATSHADE = 0x0001;
constexpr int STUDIO_NF_CHROME = 0x0002;
constexpr int STUDIO_NF_ADDITIVE = 0x0020;
constexpr int STUDIO_NF_MASKED = 0x0040;

struct StudioBone
{
    std::string name;
    int parent = -1;
    float value[6] = {};  
    float scale[6] = {};  
};

struct StudioTexture
{
    std::string name;
    int flags = 0;
    int width = 0;
    int height = 0;
    std::vector<uint8_t> pixels;
    std::vector<uint8_t> palette; 
};

struct StudioTriVertex
{
    int vertex = 0;  // StudioSubModel::vertices
    int normal = 0;  // StudioSubModel::normals
    float u = 0.0f;  // 0 to 1 across the texture
    float v = 0.0f;  // 0 to 1 down the texture
};

struct StudioMesh
{
    int texture = -1;                       
    std::vector<StudioTriVertex> triangles; 
};

struct StudioSubModel
{
    std::string name;
    std::vector<aiVector3D> vertices;  // each in the space of its bone
    std::vector<int> vertexBones;
    std::vector<aiVector3D> normals;
    std::vector<int> normalBones;
    std::vector<StudioMesh> meshes;
};

struct StudioBodyPart
{
    std::string name;
    std::vector<StudioSubModel> models;  // the first is the one shown by default
};

struct StudioAttachment
{
    std::string name;
    int bone = 0;
    aiVector3D origin;  // in the bone's space
};

// a sound, a muzzle flash, a shell: something the game does at a frame of a sequence
struct StudioEvent
{
    int frame = 0;
    int event = 0;
    std::string options;
};

struct StudioHitbox
{
    int bone = 0;
    int group = 0;
    aiVector3D min, max;
};

struct StudioBoneController
{
    int bone = 0;
    int type = 0;  // a motion flag, with 0x8000 for a wrap-around one
    float start = 0.0f;
    float end = 0.0f;
    int index = 0;  // 4 is the mouth
};

struct StudioSequence
{
    std::string name;
    float fps = 30.0f;
    int frames = 0;
    int blends = 1;  // an aim sequence has several, from looking up to looking down
    float blendStart = 0.0f;  // the angle of the first blend, -90 (up) in a CS aim
    float blendEnd = 1.0f;    // and of the last
    int blendType = 0;        // the motion flag the blend is over, XR for an aim
    int flags = 0;            // 1: loops
    int activity = 0;
    int activityWeight = 0;
    int motionType = 0;       // the motion flags the sequence moves the model by
    aiVector3D linearMovement;
    int entryNode = 0, exitNode = 0, nodeFlags = 0;
    std::vector<StudioEvent> events;
};

class StudioModel
{
public:
    bool Load(const std::string& path);
    const std::string& GetError() const { return m_error; }

    // blend runs over all the blends of the sequence, the first at 0 and the last at 1; two
    // neighbours are mixed. 0.5 is an aim straight ahead
    std::vector<aiMatrix4x4> BoneTransforms(int sequence, int frame, float blend = 0.0f) const;
    // each bone relative to its parent
    std::vector<aiMatrix4x4> LocalBoneTransforms(int sequence, int frame, float blend = 0.0f) const;
    // position and angles of every bone as stored, in one blend of a sequence; sequence -1 for the rest pose
    std::vector<std::array<float, 6>> AnimationValues(int sequence, int blendIndex, int frame) const;

    // writes the model with the vertices and normals of one bone moved by transform, given in that
    // bone's space; textures kept in a separate nameT.mdl go along when the name changes
    static bool TransformBoneVertices(const std::string& inputPath, const std::string& outputPath, int bone,
                                      const aiMatrix4x4& transform, std::string& error);
    // adds flags to every texture of the file
    static bool AddTextureFlags(const std::string& path, int flags, std::string& error);

    std::string name;
    std::vector<StudioBone> bones;
    std::vector<StudioTexture> textures;
    std::vector<StudioBodyPart> bodyParts;
    std::vector<StudioAttachment> attachments;
    std::vector<StudioSequence> sequences;
    std::vector<StudioHitbox> hitboxes;
    std::vector<StudioBoneController> controllers;
    // for each skin family, the texture of each skin reference
    std::vector<std::vector<int>> skinFamilies;
    int flags = 0;
    aiVector3D eyePosition, boundsMin, boundsMax, clipMin, clipMax;
    bool externalTextures = false;  // in nameT.mdl

private:
    struct SequenceData
    {
        int group = 0;       // which file the animation is in
        uint32_t offset = 0;  // of the per bone offset table within it
    };

    // position and angles of every bone in one blend of a sequence at a frame
    void BoneValues(int sequence, int blendIndex, int frame, std::vector<std::array<float, 6>>& values) const;

    bool Fail(const std::string& message);
    bool ReadTextures(const std::vector<uint8_t>& file);

    std::string m_error;
    std::vector<SequenceData> m_sequenceData;
    std::vector<std::vector<uint8_t>> m_groupFiles;  
};
