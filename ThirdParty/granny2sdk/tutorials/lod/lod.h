#if !defined(LOD_H)
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
#include <d3dx9.h>
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

    int LODNotch;

    DemoScene();
    ~DemoScene();
};

// ---------------------------------------------------------
// Global D3D Objects: Note that we only have a few shader
// objects, so we'll just make them global.
extern IDirect3D9* g_pD3D;
extern IDirect3DDevice9* g_pD3DDevice;

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


#define LOD_H
#endif
