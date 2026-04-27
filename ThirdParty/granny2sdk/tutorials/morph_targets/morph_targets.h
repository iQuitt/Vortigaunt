#if !defined(DX9_RENDERING_H)
// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// See .cpp file for details...
//
// $Notice: $
// ========================================================================
#include "granny.h"
#include <d3d9.h>
#include <vector>

struct DemoTexture
{
    char* Name;
    IDirect3DTexture9* TextureBuffer;

    DemoTexture();
    ~DemoTexture();
};

struct DemoMesh
{
    granny_mesh*        Mesh;

    granny_track_group* MorphAnim;
    granny_real32       AnimDuration;

    granny_mesh_binding* MeshBinding;
    std::vector<DemoTexture*> MaterialBindings;

    IDirect3DIndexBuffer9*  IndexBuffer;
    IDirect3DVertexBuffer9* VertexBuffer;

    std::vector<IDirect3DVertexBuffer9*> MorphBuffers;
    std::vector<granny_vector_track*>    MorphTracks;

    DemoMesh();
    ~DemoMesh();
};

struct DemoModel
{
    granny_model_instance* ModelInstance;
    std::vector<DemoMesh*> BoundMeshes;

    granny_real32 InitialMatrix[16];

    DemoModel();
    ~DemoModel();
};

struct DemoScene
{
    granny_camera DemoCamera;

    granny_file      *SceneFile;
    granny_file_info *SceneFileInfo;

    std::vector<DemoTexture*> Textures;
    std::vector<DemoModel*>   Models;

    // For the purposes of this sample, we'll be sampling the model
    // animation immediately before rendering, which allows us to share
    // the local pose across all models.
    granny_int32x      MaxBoneCount;
    granny_local_pose* SharedLocalPose;
    granny_world_pose* SharedWorldPose;

    // Simple directional light
    float DirFromLight[4];
    float LightColour[4];
    float AmbientColour[4];

    float CurrentTime;

    DemoScene();
    ~DemoScene();
};

// ---------------------------------------------------------
// Global D3D Objects: Note that we only have a few shader
// objects, so we'll just make them global.
extern IDirect3D9* g_pD3D;
extern IDirect3DDevice9* g_pD3DDevice;
extern IDirect3DVertexDeclaration9* g_pVertexDecl_Morph;
extern IDirect3DVertexShader9* g_pVertexShader_Morph;
extern IDirect3DPixelShader9* g_pPixelShader;

// ---------------------------------------------------------
// Global Win32 objects
extern const char* MainWindowTitle;
extern HWND g_hwnd;
extern bool GlobalRunning;

bool CreateDemoTextures();
bool CreateDemoModels();
bool CreateSharedPoses();
bool InitCameraAndLights();
void RenderModel(DemoModel*);
void Update(granny_real32 const CurrentTime,
            granny_real32 const DeltaTime);


// ---------------------------------------------------------
// Handy functions.
bool InitializeD3D();
void CleanupD3D();
void SetTransposedMatrix4x4(LPDIRECT3DDEVICE9 D3DDevice,
                            int ShaderConstant,
                            float const *Matrix );
void SetTransposedMatrix4x3(LPDIRECT3DDEVICE9 D3DDevice,
                            int ShaderConstant,
                            float const *Matrix );

void Render(int Width, int Height);

LRESULT CALLBACK MainWindowCallback(HWND Window, UINT Message,
                                    WPARAM WParam, LPARAM LParam);


// These must match the constant declarations in the vertex shaders...
#define MATRIX_CONSTANT_OBJ2WORLD  0
#define MATRIX_CONSTANT_WORLD2VIEW 4
#define MATRIX_CONSTANT_VIEW2CLIP  8
#define VEC3_DIRFROMLIGHT          12
#define VEC4_LIGHTCOLOUR           13
#define VEC4_AMBIENTCOLOUR         14
#define MORPH_CONSTANTS            15


#define DX9_RENDERING_H
#endif
