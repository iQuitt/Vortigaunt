#if !defined(GSTATE_TEST_H)
// ========================================================================
// $Notice: $
// ========================================================================
#include "granny.h"
#include "gstate_base.h"
#include "simple_model_helper.h"
#include <d3d9.h>
#include <vector>

struct DemoScene
{
    granny_camera DemoCamera;

    granny_file      *SceneFile;
    granny_file_info *SceneFileInfo;

    granny_file*           CharacterFile;
    gstate_character_info* CharacterInfo;

    std::vector<SimpleModel*>   Models;
    std::vector<SimpleTexture*> Textures;

    granny_pose_cache* PoseCache;

    gstate_character_instance* Instance;

    float DeltaTime;

    // For the purposes of this sample, we'll be sampling the model
    // animation immediately before rendering, which allows us to share
    // the local pose across all models.
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

// ---------------------------------------------------------
// Global Win32 objects
extern const char* MainWindowTitle;
extern HWND g_hwnd;
extern bool GlobalRunning;

bool CreateSharedPoses();
bool InitCameraAndLights();

void SamplePosesForGraph();

// ---------------------------------------------------------
// Handy functions.
bool InitializeD3D();
void CleanupD3D();

void Render(int Width, int Height);

LRESULT CALLBACK MainWindowCallback(HWND Window, UINT Message,
                                    WPARAM WParam, LPARAM LParam);


#define GSTATE_TEST_H
#endif
