#if !defined(IK_2BONE_H)
// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "granny.h"
#include "simple_model_helper.h"

struct DemoScene
{
    float CurrentTime;
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

// ---------------------------------------------------------
// Global Win32 objects
extern const char* MainWindowTitle;
extern HWND g_hwnd;
extern bool GlobalRunning;

// ---------------------------------------------------------
// Handy functions.
void ComputeLegPosition(SimpleModel* Model);

bool InitializeD3D();
void CleanupD3D();
void SetTransposedMatrix4x4(D3DXHANDLE ShaderConstant,
                            float const *Matrix );

void Render(int Width, int Height);

LRESULT CALLBACK MainWindowCallback(HWND Window, UINT Message,
                                    WPARAM WParam, LPARAM LParam);


#define IK_2BONE_H
#endif
