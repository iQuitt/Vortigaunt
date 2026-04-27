// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_test.h"
#include <assert.h>

// ---------------------------------------------------------
// Win32 objects
const char* MainWindowTitle = "DirectX9 GrannySample";
HWND g_hwnd = NULL;
bool GlobalRunning = false;

// ---------------------------------------------------------
// Global D3D Objects: Note that we only have a few shader
// objects, so we'll just make them global.
IDirect3D9*       g_pD3D = NULL;
IDirect3DDevice9* g_pD3DDevice = NULL;

DemoScene* GlobalScene = 0;

// ---------------------------------------------------------------------------------------
// These functions may be of interest, but aren't directly relevant to the tutorial, so we
// stuff them out of the documentation autogenerate section.
// ---------------------------------------------------------------------------------------

#define HARDCODED_DISPLAY_WIDTH  1280
#define HARDCODED_DISPLAY_HEIGHT 720

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
                                      D3DCREATE_HARDWARE_VERTEXPROCESSING,
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
void KeyEvent(char Character);

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

        case WM_CHAR:
        {
            KeyEvent(char(WParam));
        } break;

        default:
        {
            Result = DefWindowProc(Window, Message, WParam, LParam);
        } break;
    }

    return(Result);
}




bool CreateSharedPoses()
{
    assert(GlobalScene->Models.size() == 1);

    // More on this later (in the Render() description).  GrannyNewWorldPose would work
    // equally well here.
    GlobalScene->SharedWorldPose =
        GrannyNewWorldPoseNoComposite(GrannyGetSourceSkeleton(GlobalScene->Models[0]->ModelInstance)->BoneCount);

    return true;
}



bool InitCameraAndLights()
{
    assert(GlobalScene->SceneFileInfo);
    granny_file_info* Info = GlobalScene->SceneFileInfo;

    // Ok, now we can setup the camera...
    GrannyInitializeDefaultCamera(&GlobalScene->DemoCamera);
    GlobalScene->DemoCamera.NearClipPlane = 1;
    GlobalScene->DemoCamera.FarClipPlane  = 400;
    GlobalScene->DemoCamera.FOVY = 0.2f * 3.14f;
    GlobalScene->DemoCamera.ElevAzimRoll[0] = 1.3f;
    GlobalScene->DemoCamera.Offset[1] = 20;
    GlobalScene->DemoCamera.Offset[2] = 150;

    /* Setup our hardcoded light.  This points it in the same general direction as the camera, but offset slightly. */
    GlobalScene->DirFromLight[0]  = -0.8660f;
    GlobalScene->DirFromLight[1]  = -0.5f;
    GlobalScene->DirFromLight[2]  = 0;
    GlobalScene->LightColour[0]   = 0.8f;
    GlobalScene->LightColour[1]   = 0.8f;
    GlobalScene->LightColour[2]   = 0.8f;
    GlobalScene->LightColour[3]   = 0.8f;
    GlobalScene->AmbientColour[0] = 0.2f;
    GlobalScene->AmbientColour[1] = 0.2f;
    GlobalScene->AmbientColour[2] = 0.2f;
    GlobalScene->AmbientColour[3] = 0.2f;

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
    GrannySetCameraAspectRatios(&GlobalScene->DemoCamera,
                                GrannyGetMostLikelyPhysicalAspectRatio(Width, Height),
                                (float)Width, (float)Height,
                                (float)Width, (float)Height);
    GrannyBuildCameraMatrices(&GlobalScene->DemoCamera);

    /* Clear the backbuffer to a blue color, and start rendering our scene */
    g_pD3DDevice->Clear(0L, NULL,
                        D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                        D3DCOLOR_XRGB(0,0,255), 1.0f, 0L );

    if(SUCCEEDED(g_pD3DDevice->BeginScene()))
    {
        SimpleModelSetup(GlobalScene->DemoCamera,
                         GlobalScene->DirFromLight,
                         GlobalScene->LightColour,
                         GlobalScene->AmbientColour);

        // render the model
        assert(GlobalScene->Models.size() == 1);

        SamplePosesForGraph();
        RenderSimpleModel(GlobalScene->Models[0], GlobalScene->SharedWorldPose, g_pD3DDevice);

        // Flush the pose cache to ensure we don't leak memory...
        GrannyClearPoseCache(GlobalScene->PoseCache);

        g_pD3DDevice->EndScene();
    }

    // Present the backbuffer contents to the display
    g_pD3DDevice->Present( NULL, NULL, NULL, NULL );
}

DemoScene::DemoScene()
  : SceneFile(NULL),
    SceneFileInfo(NULL),
    SharedWorldPose(NULL),
    PoseCache(GrannyNewPoseCache())
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
    Textures.clear();

    {for(granny_uint32x i = 0; i < Models.size(); ++i)
    {
        delete Models[i];
        Models[i] = NULL;
    }}
    Models.clear();

    GrannyFreeWorldPose(SharedWorldPose);
    SharedWorldPose = NULL;

    GrannyFreeFile(SceneFile);
    SceneFile = NULL;
    SceneFileInfo = NULL;

    GrannyFreePoseCache(PoseCache);
    PoseCache = 0;
}
