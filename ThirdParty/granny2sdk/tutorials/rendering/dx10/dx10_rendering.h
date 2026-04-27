#if !defined(DX10_RENDERING_H)
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
#include <d3d10.h>
#include <d3dx10.h>
#include <vector>

struct DemoTexture
{
    char* Name;
    ID3D10Texture2D* TextureBuffer;
    ID3D10ShaderResourceView* TextureView;

    DemoTexture();
    ~DemoTexture();
};

struct DemoMesh
{
    granny_mesh*              Mesh;
    granny_mesh_binding*      MeshBinding;
    std::vector<DemoTexture*> MaterialBindings;

    ID3D10Buffer* IndexBuffer;
    DXGI_FORMAT   IndexBufferFormat;

    ID3D10Buffer* VertexBuffer;

    DemoMesh();
    ~DemoMesh();
};

struct DemoModel
{
    granny_model_instance* ModelInstance;
    std::vector<DemoMesh*> BoundMeshes;

    granny_real32 InitialMatrix[16];

    ID3D10Buffer* MatrixBuffers;

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

    DemoScene();
    ~DemoScene();
};

// ---------------------------------------------------------
// Global D3D Objects: Note that we only have a few shader
// objects, so we'll just make them global.
extern IDXGISwapChain* g_pSwapChain;
extern ID3D10Device* g_pD3DDevice;

extern ID3D10RenderTargetView* g_pRenderTargetView;
extern ID3D10DepthStencilView* g_pDepthView;

extern ID3D10RasterizerState* g_pRenderState;

extern ID3D10Effect* g_pEffect10;
extern ID3D10EffectTechnique* g_pSkinnedTechnique;
extern ID3D10EffectTechnique* g_pRigidTechnique;

extern ID3D10EffectVectorVariable* g_pLightDir;
extern ID3D10EffectVectorVariable* g_pLightColor;
extern ID3D10EffectVectorVariable* g_pAmbientColor;

extern ID3D10EffectMatrixVariable* g_pMatObj2World;
extern ID3D10EffectMatrixVariable* g_pMatWorld2View;
extern ID3D10EffectMatrixVariable* g_pMatView2Clip;

extern ID3D10EffectShaderResourceVariable* g_ptxDiffuse;

extern ID3D10EffectMatrixVariable* g_pmBoneArray;

extern ID3D10InputLayout* g_pSkinnedVertexLayout;
extern ID3D10InputLayout* g_pRigidVertexLayout;


#if 0
extern D3DXHANDLE g_hSkinningMatrices;
extern IDirect3DVertexDeclaration9* g_pVertexDecl_Rigid;
extern IDirect3DVertexDeclaration9* g_pVertexDecl_Skinned;
#endif

// ---------------------------------------------------------
// Global Win32 objects
extern const char* MainWindowTitle;
extern HWND g_hwnd;
extern bool GlobalRunning;

// ---------------------------------------------------------
// Handy functions.
bool InitializeD3D();
void CleanupD3D();

void Render(int Width, int Height);

LRESULT CALLBACK MainWindowCallback(HWND Window, UINT Message,
                                    WPARAM WParam, LPARAM LParam);


#define DX10_RENDERING_H
#endif
