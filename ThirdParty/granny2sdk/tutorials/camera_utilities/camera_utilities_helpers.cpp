// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "camera_utilities.h"
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

DemoScene GlobalScene;

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
LRESULT CALLBACK
MainWindowCallback(HWND Window, UINT Message, WPARAM WParam, LPARAM LParam)
{
    LRESULT Result = 0;

    static int DownX, DownY;

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

        case WM_LBUTTONDOWN:
        {
            SetCapture(Window);
            DownX = LOWORD(LParam);
            DownY = HIWORD(LParam);
        } break;

        case WM_LBUTTONUP:
        {
            ReleaseCapture();
        } break;

//        case WM_MOUSEWHEEL:
//        {
//            int zDelta = GET_WHEEL_DELTA_WPARAM(WParam);
//          HandleMouseWheel(zDelta);
//        } break;

        case WM_MOUSEMOVE:
        {
            if (WParam & MK_LBUTTON)
            {
                int NewX = LOWORD(LParam);
                int NewY = HIWORD(LParam);

                int deltaX = NewX - DownX;
                int deltaY = NewY - DownY;
                DownX = NewX;
                DownY = NewY;

                HandleMouseMove(deltaX, deltaY);
            }
        } break;

        case WM_DESTROY:
        {
            GlobalRunning = false;
        } break;

        case WM_KEYDOWN:
        {
            switch (WParam)
            {
            case 'W': WKeyDown = true; break;
            case 'A': AKeyDown = true; break;
            case 'S': SKeyDown = true; break;
            case 'D': DKeyDown = true; break;
            };
        } break;

        case WM_KEYUP:
        {
            switch (WParam)
            {
            case 'W': WKeyDown = false; break;
            case 'A': AKeyDown = false; break;
            case 'S': SKeyDown = false; break;
            case 'D': DKeyDown = false; break;
            };
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
            GrannyNewWorldPoseNoComposite(GlobalScene.MaxBoneCount);

        return true;
    }
    else
    {
        // Very odd.  An error loading the file most likely.
        return false;
    }
}


bool InitLights()
{
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

    FinalizeCameraForFrame(Width, Height);

    /* Clear the backbuffer to a blue color, and start rendering our scene */
    g_pD3DDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                         D3DCOLOR_XRGB(0,0,255), 1.0f, 0L );

    if(SUCCEEDED(g_pD3DDevice->BeginScene()))
    {
        SimpleModelSetup(GlobalScene.DemoCamera,
                         GlobalScene.DirFromLight,
                         GlobalScene.LightColour,
                         GlobalScene.AmbientColour);

        // Loop over the models, and render their meshes
        for (size_t Idx = 0; Idx < GlobalScene.Models.size(); Idx++)
        {
            SimpleModel* Model = GlobalScene.Models[Idx];

            GrannySampleModelAnimationsAccelerated(
                Model->ModelInstance,
                GrannyGetSourceSkeleton(Model->ModelInstance)->BoneCount,
                Model->InitialMatrix,
                GlobalScene.SharedLocalPose,
                GlobalScene.SharedWorldPose);

            RenderSimpleModel(Model, GlobalScene.SharedWorldPose, g_pD3DDevice);
        }
        g_pD3DDevice->EndScene();
    }

    // Present the backbuffer contents to the display
    g_pD3DDevice->Present( NULL, NULL, NULL, NULL );
}


void Update(granny_real32 const CurrentTime,
            granny_real32 const DeltaTime)
{
    /* All that's necessary here is to set the current time for each of the models.  We
       don't make use of DeltaTime in this function, but if we were tracking extracted
       root motion, we'd be using it with $UpdateModelMatrix.  You can call
       $SampleModelAnimations (or one of that family of functions) at this point if you
       are sampling at update time, but we delay building the $local_pose and $world_pose
       until rendering in this app. */
    for (size_t Idx = 0; Idx < GlobalScene.Models.size(); Idx++)
    {
        // Set the model clock
        SimpleModel* Model = GlobalScene.Models[Idx];
        GrannySetModelClock(Model->ModelInstance, CurrentTime);
    }

    UpdateFrameMovement(DeltaTime);
}

