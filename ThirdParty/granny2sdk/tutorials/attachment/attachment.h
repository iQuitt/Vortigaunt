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
    granny_mesh*         Mesh;
    granny_mesh_binding* MeshBinding;
    std::vector<DemoTexture*> MaterialBindings;

    IDirect3DIndexBuffer9*  IndexBuffer;
    IDirect3DVertexBuffer9* VertexBuffer;

    DemoMesh();
    ~DemoMesh();
};

struct DemoModel
{
    granny_model_instance* ModelInstance;

    // Attachment members
    DemoModel* AttachedTo;
    int        BaseModelBone;
    int        AttachModelBone;

    std::vector<DemoMesh*> BoundMeshes;
    granny_real32 InitialMatrix[16];

    DemoModel(granny_model*);
    ~DemoModel();
};

struct DemoScene
{
    granny_camera DemoCamera;

    granny_file      *CharacterFile;
    granny_file_info *CharacterFileInfo;

    granny_file      *BatonFile;
    granny_file_info *BatonFileInfo;

    granny_file      *CapeFile;
    granny_file_info *CapeFileInfo;

    std::vector<DemoTexture*> Textures;

    DemoModel* Character;
    DemoModel* Baton;
	DemoModel* Cape;

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

    DemoScene();
    ~DemoScene();
};

// ---------------------------------------------------------
// Global D3D Objects: Note that we only have a few shader
// objects, so we'll just make them global.
extern IDirect3D9* g_pD3D;
extern IDirect3DDevice9* g_pD3DDevice;
extern IDirect3DVertexDeclaration9* g_pVertexDecl_Rigid;
extern IDirect3DVertexDeclaration9* g_pVertexDecl_Skinned;
extern IDirect3DVertexShader9* g_pVertexShader_Rigid;
extern IDirect3DVertexShader9* g_pVertexShader_Skinned;
extern IDirect3DPixelShader9* g_pPixelShader;

// ---------------------------------------------------------
// Global Win32 objects
extern LPCTSTR MainWindowTitle;
extern HWND g_hwnd;
extern bool GlobalRunning;

bool CreateDemoTextures();
bool CreateDemoModels();
bool CreateSharedPoses();
bool InitCameraAndLights();
void RenderModel(DemoModel*);
void RenderAttachedModel(DemoModel*);
void RenderMeshBindings(DemoModel*         Model,
                        granny_world_pose* Pose);


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


#define DX9_RENDERING_H
#endif
