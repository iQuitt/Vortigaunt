// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "ik_2bone.h"
#include <assert.h>

// Note that we're using extremely simple objects, and a global scene
// database that you'll want to completely ignore when you build your
// own app on top of Granny.  This simply keeps everything very
// straightfoward.

// ---------------------------------------------------------
// Global D3D Objects: Note that we only have a few shader
// objects, so we'll just make them global.
IDirect3D9*       g_pD3D = NULL;
IDirect3DDevice9* g_pD3DDevice = NULL;


// ---------------------------------------------------------
// Win32 objects
const char* MainWindowTitle = "DirectX9 GrannySample";
HWND g_hwnd = NULL;
bool GlobalRunning = false;

bool CreateSharedPoses();
bool InitCameraAndLights();

extern DemoScene GlobalScene;

// ---------------------------------------------------------------------------------------
// These functions may be of interest, but aren't directly relevant to the tutorial, so we
// stuff them out of the documentation autogenerate section.
// ---------------------------------------------------------------------------------------
char const* const GlobalSceneFilename = "media\\pixo_run_walk.gr2";
char const* const ShaderFile          = "media\\Shaders\\PixoShader.hlsl";

#define HARDCODED_DISPLAY_WIDTH  1280
#define HARDCODED_DISPLAY_HEIGHT 720

// #define DEBUG_VS

#define CHECKED_RELEASE(x) { if (x) { (x)->Release(); } (x) = NULL; } 0

bool InitializeD3D()
{
    // Create a window to draw in...
    {
        WNDCLASSEX WindowClass = {sizeof(WindowClass)};
        WindowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
        WindowClass.lpfnWndProc = MainWindowCallback;
        WindowClass.hInstance = GetModuleHandle(0);
        //WindowClass.hIcon = LoadIcon(WindowClass.hInstance, MAKEINTRESOURCE(101));
        WindowClass.hCursor = LoadCursor(0, IDC_ARROW);
        WindowClass.lpszClassName = "DX9Basic_SimpleClass";

        if(!RegisterClassEx(&WindowClass))
            return false;

        g_hwnd = CreateWindowEx(0, WindowClass.lpszClassName, MainWindowTitle,
                                WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                CW_USEDEFAULT, CW_USEDEFAULT,
                                HARDCODED_DISPLAY_WIDTH,
                                HARDCODED_DISPLAY_HEIGHT,
                                0, 0, GetModuleHandle(0), 0);
        if (!g_hwnd)
            return false;
    }

    // Create the D3D object.
    g_pD3D = Direct3DCreate9( D3D_SDK_VERSION );
    if( !g_pD3D )
    {
        return false;
    }

    // Set up the structure used to create the D3DDevice.
    D3DPRESENT_PARAMETERS d3dpp;
    ZeroMemory( &d3dpp, sizeof(d3dpp) );
    d3dpp.BackBufferWidth        = HARDCODED_DISPLAY_WIDTH;
    d3dpp.BackBufferHeight       = HARDCODED_DISPLAY_HEIGHT;
    d3dpp.BackBufferFormat       = D3DFMT_X8R8G8B8;
    d3dpp.BackBufferCount        = 1;
    d3dpp.SwapEffect             = D3DSWAPEFFECT_DISCARD;
    d3dpp.hDeviceWindow          = g_hwnd;
    d3dpp.Windowed               = TRUE;
    d3dpp.EnableAutoDepthStencil = TRUE;
    d3dpp.AutoDepthStencilFormat = D3DFMT_D24S8;
    d3dpp.PresentationInterval   = D3DPRESENT_INTERVAL_ONE;

    // Create the Direct3D device.
    HRESULT hr = g_pD3D->CreateDevice(D3DADAPTER_DEFAULT,
                                      D3DDEVTYPE_HAL,
                                      g_hwnd,
#ifdef DEBUG_VS
                                      D3DCREATE_SOFTWARE_VERTEXPROCESSING,
#else
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING,
#endif
                                      &d3dpp,
                                      &g_pD3DDevice);
    if (FAILED(hr))
    {
        return false;
    }

    if (!InitSimpleModelHelper(g_pD3DDevice))
        return false;

    return true;
}


void CleanupD3D()
{
    CleanupSimpleModelHelper();
    CHECKED_RELEASE(g_pD3DDevice);
    CHECKED_RELEASE(g_pD3D);
}

//-------------------------------------------------------------------------------------
// Name: MainWindowCallback
// Desc: Our message handler...
//-------------------------------------------------------------------------------------
LRESULT CALLBACK
MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;

    switch(Message)
    {
        case WM_ACTIVATEAPP:
        {
            if (WParam != 0)
                InvalidateRect(Window, 0, false);

            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;

        case WM_PAINT:
        {
            // Let Win32 know we're repainting
            HDC RenderingDC;
            PAINTSTRUCT PaintInfo;
            RenderingDC = BeginPaint(Window, &PaintInfo);

            // Render with D3D
            RECT ClientRectangle;
            GetClientRect(Window, &ClientRectangle);

            Render(ClientRectangle.right, ClientRectangle.bottom);

            // Let Win32 know we're finished
            EndPaint(Window, &PaintInfo);
        } break;

        case WM_DESTROY:
        {
            GlobalRunning = false;
        } break;

        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }

    return(Result);
}



DemoScene::DemoScene()
  : SceneFile(NULL),
    SceneFileInfo(NULL),
    SharedLocalPose(NULL),
    SharedWorldPose(NULL),
    MaxBoneCount(-1)
{
    // Nada
}

DemoScene::~DemoScene()
{
    {for(granny_uint32x i = 0; i < Textures.size(); ++i)
    {
        delete Textures[i];
        Textures[i] = NULL;
    }}

    {for(granny_uint32x i = 0; i < Models.size(); ++i)
    {
        delete Models[i];
        Models[i] = NULL;
    }}

    GrannyFreeLocalPose(SharedLocalPose);
    GrannyFreeWorldPose(SharedWorldPose);
    SharedLocalPose = NULL;
    SharedWorldPose = NULL;

    GrannyFreeFile(SceneFile);
    SceneFile = NULL;
    SceneFileInfo = NULL;
}



bool CreateSharedPoses()
{
    GlobalScene.MaxBoneCount = -1;
    {for(granny_uint32x i = 0; i < GlobalScene.Models.size(); ++i)
    {
        granny_skeleton* Skeleton =
            GrannyGetSourceSkeleton(GlobalScene.Models[i]->ModelInstance);

        if (Skeleton->BoneCount > GlobalScene.MaxBoneCount)
            GlobalScene.MaxBoneCount = Skeleton->BoneCount;
    }}

    if (GlobalScene.MaxBoneCount != -1)
    {
        GlobalScene.SharedLocalPose = GrannyNewLocalPose(GlobalScene.MaxBoneCount);

        // More on this later (in the Render() description).  GrannyNewWorldPose would work
        // equally well here.
        GlobalScene.SharedWorldPose =
            GrannyNewWorldPose(GlobalScene.MaxBoneCount);

        return true;
    }
    else
    {
        // Very odd.  An error loading the file most likely.
        return false;
    }
}

bool InitCameraAndLights()
{
    assert(GlobalScene.SceneFileInfo);

    GrannyInitializeDefaultCamera(&GlobalScene.DemoCamera);
    GlobalScene.DemoCamera.NearClipPlane = 0.1f;
    GlobalScene.DemoCamera.FarClipPlane  = 1000.f;
    GlobalScene.DemoCamera.FOVY = 0.2f * 3.14f;
    GlobalScene.DemoCamera.Offset[1] = 25.0f;
    GlobalScene.DemoCamera.Offset[2] = 200.0f;
    GlobalScene.DemoCamera.ElevAzimRoll[1] = -0.8f;

    /* Setup our hardcoded light.  This points it in the same general direction as the camera, but offset slightly. */
    GlobalScene.DirFromLight[0]  = -0.8660f;
    GlobalScene.DirFromLight[1]  = 0.5f;
    GlobalScene.DirFromLight[2]  = 0;
    GlobalScene.LightColour[0]   = 0.8f;
    GlobalScene.LightColour[1]   = 0.8f;
    GlobalScene.LightColour[2]   = 0.8f;
    GlobalScene.LightColour[3]   = 0.8f;
    GlobalScene.AmbientColour[0] = 0.2f;
    GlobalScene.AmbientColour[1] = 0.2f;
    GlobalScene.AmbientColour[2] = 0.2f;
    GlobalScene.AmbientColour[3] = 0.2f;

    return true;
}

void Render(int Width, int Height)
{
    if (!GlobalRunning)
        return;
    assert(g_pD3DDevice);

    /* Note that I set the camera aspect ratios every frame, because the width and height
       of the window may have changed which affects the aspect ratio correction.  However,
       if you're a full screen game and you know when you're changing screen modes and
       such, then you'd only have to call GrannySetCameraAspectRatios() when you actually
       change modes. */
    GrannySetCameraAspectRatios(&GlobalScene.DemoCamera,
                                GrannyGetMostLikelyPhysicalAspectRatio(Width, Height),
                                (float)Width, (float)Height,
                                (float)Width, (float)Height);
    GrannyBuildCameraMatrices(&GlobalScene.DemoCamera);

    /* Clear the backbuffer to a blue color, and start rendering our scene */
    g_pD3DDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                         D3DCOLOR_XRGB(0,0,255), 1.0f, 0L );

    if(SUCCEEDED(g_pD3DDevice->BeginScene()))
    {
        SimpleModelSetup(GlobalScene.DemoCamera,
                         GlobalScene.DirFromLight,
                         GlobalScene.LightColour,
                         GlobalScene.AmbientColour);

        g_pD3DDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

        // Loop over the models, and render their meshes
        for (size_t Idx = 0; Idx < GlobalScene.Models.size(); Idx++)
        {
            ComputeLegPosition(GlobalScene.Models[Idx]);
            RenderSimpleModel(GlobalScene.Models[Idx],
                              GlobalScene.SharedWorldPose,
                              g_pD3DDevice);
        }
        g_pD3DDevice->EndScene();
    }

    // Present the backbuffer contents to the display
    g_pD3DDevice->Present( NULL, NULL, NULL, NULL );
}


INT WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR, INT )
{
    // Check the versions
    if(!GrannyVersionsMatch)
    {
        printf("Warning: the Granny DLL currently loaded "
               "doesn't match the .h file used during compilation\n");
        return EXIT_FAILURE;
    }

    /* Create the D3D objects */
    if (InitializeD3D() == false)
    {
        printf("Error: unable to initialize D3D device.  "
               "DX9 compliant renderer required.\n");
        return EXIT_FAILURE;
    }

    /* Load the scene file, and bind the meshes, textures, etc. */
    {
        GlobalScene.SceneFile = GrannyReadEntireFile(GlobalSceneFilename);
        if (GlobalScene.SceneFile == NULL)
            return EXIT_FAILURE;

        GlobalScene.SceneFileInfo = GrannyGetFileInfo(GlobalScene.SceneFile);
        if (GlobalScene.SceneFileInfo == NULL)
            return EXIT_FAILURE;
    }

    /* Create our demo objects.  Note that the order is important here, CreateDemoModels
       will be looking up textures as it iterates, and CreateSharedPoses will scan the
       created models. */
    if (CreateSimpleTextures(g_pD3DDevice, GlobalScene.SceneFileInfo, GlobalScene.Textures) == false ||
        CreateSimpleModels(g_pD3DDevice, GlobalScene.SceneFileInfo, GlobalScene.Textures, GlobalScene.Models)   == false ||
        CreateSharedPoses()  == false ||
        InitCameraAndLights() == false)
    {
        return EXIT_FAILURE;
    }


    /* Mark the start time so we can compute update intervals */
    granny_system_clock StartClock;
    GrannyGetSystemSeconds(&StartClock);
    granny_system_clock LastClock = StartClock;
    GlobalScene.CurrentTime = 0.0f;

    /* Enter our "game" loop.  Each frame, we compute the current time, and the time
       elapsed since the last frame, and call Update to advance the model animations.  The
       rest is simply Win32 boilerplate to keep the message pump filled. */
    GlobalRunning = true;
    while (GlobalRunning)
    {
        // Extract the current time and the frame delta
        granny_real32 CurrentTime, DeltaTime;
        {
            granny_system_clock CurrClock;
            GrannyGetSystemSeconds(&CurrClock);

            // Ignore clock recentering issues for this example
            CurrentTime = GrannyGetSecondsElapsed(&StartClock, &CurrClock);
            DeltaTime   = GrannyGetSecondsElapsed(&LastClock, &CurrClock);
            LastClock = CurrClock;
        }

        // Update the world
        GlobalScene.CurrentTime = CurrentTime;

        InvalidateRect(g_hwnd, 0, false);
        MSG Message;
        while(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&Message);
            DispatchMessage(&Message);
        }
    }

    CleanupD3D();
    return EXIT_SUCCESS;
}
