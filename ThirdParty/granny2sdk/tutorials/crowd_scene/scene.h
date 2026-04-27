//-----------------------------------------------------------------------------
// File: scene.h
//
// Desc: Scene structures to manage the walkers
//-----------------------------------------------------------------------------
#pragma once

#include <vector>
#include <d3d9.h>
#include <d3dx9.h>
#include "granny.h"

class Scene;
class Mesh;
class AnimatedModel;
struct Texture;


class Mesh
{
public:
    Mesh(AnimatedModel*      TheOwningModel,
         granny_mesh*     TheGrannyMesh,
         granny_skeleton* SkeletonToBind);
    ~Mesh();

    void CreateD3DObjects(LPDIRECT3DDEVICE9 d3ddev);
    void DestroyD3DObjects(LPDIRECT3DDEVICE9 d3ddev);
    void Render(LPDIRECT3DDEVICE9 d3ddev, granny_matrix_4x4* CompositeBuffer);

private:    
    struct TriGroup
    {
        // Created by the mesh load routine
        Texture* ColorTexture;   // textures may be null
        Texture* NormalTexture;
        int      FirstTri;
        int      NumTris;

        // Created during CreateD3DObjects
        int                      NumVertices;
        LPDIRECT3DINDEXBUFFER9   D3DIndexBuffer;
        LPDIRECT3DVERTEXBUFFER9  D3DVertexBuffer;
        std::vector<int>*             BoneRemapBuffer;

        void CreateD3DObjects(LPDIRECT3DDEVICE9, Mesh*);

        TriGroup() {
            NumVertices = -1;
            D3DIndexBuffer = NULL;
            D3DVertexBuffer = NULL;
            BoneRemapBuffer = NULL;
        }
    };

    // Granny's information
    AnimatedModel*        OwningModel;
    granny_mesh*          GrannyMesh;
    granny_mesh_binding*  GrannyMeshBinding;
    std::vector<TriGroup> TriGroupings;
};

class AnimatedModel
{
public:
    AnimatedModel(Scene*        TheScene,
                  granny_model* TheGrannyModel);
    ~AnimatedModel();

    void CreateD3DObjects ( LPDIRECT3DDEVICE9 d3ddev );
    void DestroyD3DObjects( LPDIRECT3DDEVICE9 d3ddev );

    const char* GetModelName() const { return GrannyModel->Name; }
    Scene*      GetOwningScene() { return OwningScene; }

    void Render(LPDIRECT3DDEVICE9 d3ddev, granny_world_pose* ThePose);

    // For convenience
public:
    Scene*             OwningScene;

    granny_model*      GrannyModel;
    granny_world_pose* GrannyWorldPose;  // Correctly sized pose for this model, scratch space

    std::vector<Mesh*> m_meshes;
};


class Animation
{
public:
    Animation(Scene* TheScene,
              const char*         SourceFilename,
              granny_animation*   TheAnimation,
              granny_track_group* TheTrackGroup);
    ~Animation();

    const char* GetFilename() const { return Filename; }
    const char* GetAnimationName() const { return GrannyAnimation->Name; }
    const char* GetTrackGroupName() const { return GrannyTrackGroup->Name; }

    granny_animation* GetGrannyAnimation() { return GrannyAnimation; }

private:
    Scene*              OwningScene;
    char*               Filename;
    granny_animation*   GrannyAnimation;
    granny_track_group* GrannyTrackGroup;
};

class Scene
{
public:
    Scene();
    ~Scene();

    bool FindOrLoadGrannyFile(const char*        Filename,
                              granny_file**      TheLoadedFile,
                              granny_file_info** TheFileInfo);
    AnimatedModel* FindOrLoadModel(const char* Filename,
                                   const char* Modelname);
    Animation* FindOrLoadAnimation(const char* Filename,
                                   const char* AnimationNameKey,
                                   const char* TrackGroupName);
    Texture* FindTexture(granny_texture* TheTexture);

    void CreateD3DObjects ( LPDIRECT3DDEVICE9 d3ddev );
    void DestroyD3DObjects ( LPDIRECT3DDEVICE9 d3ddev );


    // Animated model decl/shader
    LPDIRECT3DVERTEXSHADER9      GetAnimatedShader() { return D3DVertexShaderAnimated; }
    LPDIRECT3DVERTEXDECLARATION9 GetAnimatedVertexDeclaration()  { return D3DVertexDeclarationAnimated; }

    // Camera matrices...
    D3DXMATRIX MatProjection;
    D3DXMATRIX MatView;

private:
    struct WrappedFile;
    std::vector<WrappedFile*>   m_grannyFiles;
    std::vector<AnimatedModel*> m_models;
    std::vector<Animation*>     m_animations;
    std::vector<Texture*>       m_textures;

    // D3D Info
    LPDIRECT3DVERTEXSHADER9      D3DVertexShaderAnimated;
    LPDIRECT3DVERTEXDECLARATION9 D3DVertexDeclarationAnimated;
};
