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
#include "simple_model_helper.h"
#include <d3d9.h>
#include <vector>

struct DemoScene
{
    granny_camera DemoCamera;

    granny_file      *SceneFile;
    granny_file_info *SceneFileInfo;

    std::vector<SimpleTexture*> Textures;
    std::vector<SimpleModel*>   Models;

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
extern const char* MainWindowTitle;
extern HWND g_hwnd;
extern bool GlobalRunning;

bool CreateSharedPoses();
bool InitCamera();
bool InitLights();
void Update(granny_real32 const CurrentTime,
            granny_real32 const DeltaTime);

extern bool WKeyDown;
extern bool AKeyDown;
extern bool SKeyDown;
extern bool DKeyDown;

// ---------------------------------------------------------
// Handy functions.
bool InitializeD3D();
void CleanupD3D();

void Render(int Width, int Height);

void FinalizeCameraForFrame(int Width, int Height);
void HandleMouseMove(int deltaX, int deltaY);
void UpdateFrameMovement(const float DeltaTime);

LRESULT CALLBACK MainWindowCallback(HWND Window, UINT Message,
                                    WPARAM WParam, LPARAM LParam);


#define DX9_RENDERING_H
#endif
