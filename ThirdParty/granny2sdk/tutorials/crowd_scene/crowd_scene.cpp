//-----------------------------------------------------------------------------
// File: crowd_scene.cpp
//
// Desc: DirectX window application created by the DirectX AppWizard
//-----------------------------------------------------------------------------
#define DIRECTINPUT_VERSION 0x0800
#include "crowd_scene.h"
#include "scene.h"
#include "walking_model.h"

#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <basetsd.h>
#include <math.h>
#include <stdio.h>
#include <d3dx9.h>
#include <tchar.h>
#include <dinput.h>
#include <vector>

#include "resource/resource.h"

#include "d3dapp/D3DApp.h"
#include "d3dapp/d3dfont.h"
#include "d3dapp/diutil.h"

using namespace std;

// Loads the pixo model and animations
WalkingModel* GetPixoWalkingModel(Scene* TheScene);

// Reset the camera the original position
void CreateDefaultCamera(granny_camera* TheCamera);

//-----------------------------------------------------------------------------
// Defines, and constants
//-----------------------------------------------------------------------------
#define DXAPP_KEY        TEXT("Software\\DirectX AppWizard Apps\\CrowdScene")

// This GUID must be unique for every game, and the same for
// every instance of this app.
// The GUID allows DirectInput to remember input settings
// {8B00DC90-59E0-4fd5-A27B-5F3A57B4AF35}
static const GUID g_guidApp =
{ 0x8b00dc90, 0x59e0, 0x4ed5, { 0xa2, 0x7b, 0x5f, 0x3a, 0x57, 0xb4, 0xaf, 0x35 } };


// Controls the application parameters
static int AppNumInstances = 50;
static int AppMaxAt30 = 700;
static int AppNumForSpeedTest = 3000;
static float ArenaRadius = 5.0f;

static const int AppMaxThreads = 16;
static int AppNumThreads = 1;

//-----------------------------------------------------------------------------
// Structures and classes
//-----------------------------------------------------------------------------
// DirectInput action mapper reports events only when buttons/axis change
// so we need to remember the present state of relevant axis/buttons for
// each DirectInput device.  The CInputDeviceManager will store a
// pointer for each device that points to this struct
struct InputDeviceState
{
    // TODO: change as needed
    FLOAT fAxisRotateLR;
    BOOL  bButtonRotateLeft;
    BOOL  bButtonRotateRight;

    FLOAT fAxisRotateUD;
    BOOL  bButtonRotateUp;
    BOOL  bButtonRotateDown;

    BOOL  bButtonPlaySoundButtonDown;
};

// Struct to store the current input state
struct UserInput
{
    // TODO: change as needed
    FLOAT fAxisRotateUD;
    FLOAT fAxisRotateLR;
    BOOL bDoConfigureInput;
    BOOL bDoConfigureDisplay;

    FLOAT fAxisMouseX;
    FLOAT fAxisMouseY;
    FLOAT fAxisMouseZ;
    BOOL bMouseButtonLeft;
    BOOL bMouseButtonMiddle;
    BOOL bMouseButtonRight;

    BOOL bToggleHelp;
    BOOL bReset;
    BOOL bSpeedTest;
    BOOL bMax30Fps;

    BOOL iChangeInModelCount;
    int  iChangeInErr;
    float fChangeInBias;
    BOOL bToggleLODLock;
    BOOL bToggleInstanceLock;

    // Status of the modifier keys...
    BOOL bShiftKey;
    BOOL bCtrlKey;
    BOOL bRShiftKey;
    BOOL bRCtrlKey;
};

//-----------------------------------------------------------------------------
// Name: class CMyD3DApplication
// Desc: Application class. The base class (CD3DApplication) provides the
//       generic functionality needed in all Direct3D samples. CMyD3DApplication
//       adds functionality specific to this sample program.
//-----------------------------------------------------------------------------
class CMyD3DApplication : public CD3DApplication
{
public:
    BOOL                    m_bLoadingApp;          // TRUE, if the app is loading
    CD3DFont*               m_pFont;                // Font for drawing text

    CInputDeviceManager*    m_pInputDeviceManager;  // DirectInput device manager
    DIACTIONFORMAT          m_diafGame;             // Action format for game play
    LPDIRECT3DSURFACE9      m_pDIConfigSurface;     // Surface for config'ing DInput devices
    UserInput               m_UserInput;            // Struct for storing user input

    FLOAT                   m_fWorldRotX;           // World rotation state X-axis
    FLOAT                   m_fWorldRotY;           // World rotation state Y-axis
    granny_camera           m_grannycam;

    Scene                   m_scene;

    float                   m_gameClock;
    float                   m_speedScale;

    float                   m_animTimeLastFrame;

    bool                    m_enableSkeletonLOD;
    bool                    m_enableAnimationLOD;
    float                   m_allowedErr;
    float                   m_animLODBias;

    bool                    m_lodPositionLocked;
    granny_triple           m_lodPosition;

    int m_lockInstance;

    WalkingModel*                 m_walkingModel;
    std::vector<WalkingInstance*> m_walkingInstances;

    bool m_renderingOn;
    bool m_renderingSkelOn;

protected:
    virtual HRESULT OneTimeSceneInit();
    virtual HRESULT InitDeviceObjects();
    virtual HRESULT RestoreDeviceObjects();
    virtual HRESULT InvalidateDeviceObjects();
    virtual HRESULT DeleteDeviceObjects();
    virtual HRESULT Render();
    virtual HRESULT FrameMove();
    virtual HRESULT FinalCleanup();
    virtual HRESULT ConfirmDevice( D3DCAPS9*, DWORD, D3DFORMAT );
    VOID    Pause( bool bPause );

    HRESULT RenderText();

    HRESULT InitInput( HWND hWnd );
    void    UpdateInput( UserInput* pUserInput );
    void    CleanupDirectInput();

    // Helper function
    void RenderScreenQuad(float l, float r, float t, float b,
                          LPDIRECT3DTEXTURE9, bool alphaBlend);

    // Render a floor for the characters to walk on.
    void RenderStage();

public:
    LRESULT MsgProc( HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam );
    CMyD3DApplication();
    virtual ~CMyD3DApplication();

    HRESULT InputAddDeviceCB( CInputDeviceManager::DeviceInfo* pDeviceInfo, const DIDEVICEINSTANCE* pdidi );
    static HRESULT CALLBACK StaticInputAddDeviceCB( CInputDeviceManager::DeviceInfo* pDeviceInfo, const DIDEVICEINSTANCE* pdidi, LPVOID pParam );
    BOOL    ConfigureInputDevicesCB( IUnknown* pUnknown );
    static BOOL CALLBACK StaticConfigureInputDevicesCB( IUnknown* pUnknown, VOID* pUserData );
};


// Simple vertex types
struct my_simple_vertex
{
    float x, y, z;
    DWORD diffuse;
    float u,v;
};
#define MY_SIMPLE_VERTEX_FVF ( D3DFVF_XYZ | D3DFVF_DIFFUSE | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE2(0) )

struct my_rhw_vertex
{
    float x,y,z,w;
    DWORD diffuse;
    float u,v;
};
#define MY_RHW_VERTEX_FVF ( D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1 | D3DFVF_TEXCOORDSIZE2(0) )


// Input semantics used by this app
enum INPUT_SEMANTICS
{
    // Gameplay semantics
    // TODO: change as needed
    INPUT_ROTATE_AXIS_LR=1, INPUT_ROTATE_AXIS_UD,
    INPUT_CONFIG_INPUT,     INPUT_CONFIG_DISPLAY,

    // Mouse input.
    INPUT_MOUSE_LR,         INPUT_MOUSE_UD,
    INPUT_MOUSE_SCROLL,
    INPUT_MOUSE_LEFT,       INPUT_MOUSE_MIDDLE,     INPUT_MOUSE_RIGHT,
    INPUT_SHIFT_KEY,
    INPUT_CTRL_KEY,
    INPUT_RSHIFT_KEY,
    INPUT_RCTRL_KEY,

    INPUT_TOGGLE_HELP,
    INPUT_TOGGLE_RESET,
    INPUT_TOGGLE_RENDERING,
    INPUT_TOGGLE_RENDERSKELETON,

    INPUT_TOGGLE_SKELLOD,
    INPUT_TOGGLE_ANIMLOD,
    INPUT_TOGGLE_LODFADE,

    INPUT_TOGGLE_LOCKLODPOSITION,
    INPUT_TOGGLE_INSTANCELOCK,

    INPUT_INCREASE_INSTANCES,
    INPUT_DECREASE_INSTANCES,

    INPUT_INCREASE_TIMESCALE,
    INPUT_DECREASE_TIMESCALE,

    INPUT_INCREASE_ALLOWEDERR,
    INPUT_DECREASE_ALLOWEDERR,

    INPUT_INCREASE_ANIMLODBIAS,
    INPUT_DECREASE_ANIMLODBIAS,

    INPUT_SPEEDTEST,
    INPUT_30FPS,

    INPUT_INCREASE_THREADS,
    INPUT_DECREASE_THREADS,
};


// Actions used by this app
DIACTION g_rgGameAction[] =
{
    // TODO: change as needed.  Be sure to delete user map files
    // (C:\Program Files\Common Files\DirectX\DirectInput\User Maps\*.ini)
    // after changing this, otherwise settings won't reset and will be read
    // from the out of date ini files

    // Device input (joystick, etc.) that is pre-defined by DInput, according
    // to genre type. The genre for this app is space simulators.
    { INPUT_ROTATE_AXIS_LR,   DIAXIS_3DCONTROL_LATERAL,     0, TEXT("Rotate left/right"), },
    { INPUT_ROTATE_AXIS_UD,   DIAXIS_3DCONTROL_MOVE,        0, TEXT("Rotate up/down"), },

    { INPUT_SHIFT_KEY,      DIKEYBOARD_LSHIFT,              0, TEXT("Shift key"), },
    { INPUT_CTRL_KEY,       DIKEYBOARD_LCONTROL,            0, TEXT("Ctrl key"), },
    { INPUT_RSHIFT_KEY,     DIKEYBOARD_RSHIFT,              0, TEXT("Other shift key"), },
    { INPUT_RCTRL_KEY,      DIKEYBOARD_RCONTROL,            0, TEXT("Other ctrl key"), },

    { INPUT_CONFIG_DISPLAY,   DIKEYBOARD_F2,                DIA_APPFIXED, TEXT("Configure Display"), },
    { INPUT_CONFIG_INPUT,     DIKEYBOARD_F3,                DIA_APPFIXED, TEXT("Configure Input"), },
    { INPUT_TOGGLE_HELP,      DIKEYBOARD_F1,                0, TEXT("Toggle Help"), },
    { INPUT_TOGGLE_RENDERING, DIKEYBOARD_R,                 0, TEXT("Toggle rendering"), },
    { INPUT_TOGGLE_RENDERSKELETON, DIKEYBOARD_T,            0, TEXT("Toggle rendering of the skeleton"), },
    { INPUT_TOGGLE_RESET,     DIKEYBOARD_BACK,              0, TEXT("Reset application state"), },
    { INPUT_SPEEDTEST,        DIKEYBOARD_GRAVE,             0, TEXT("Setup the speedtest"), },
    { INPUT_30FPS,            DIKEYBOARD_1,                 0, TEXT("Setup the max for 30fps"), },

    { INPUT_TOGGLE_ANIMLOD, DIKEYBOARD_A,                   0, TEXT("Toggle Animation LOD"), },
    { INPUT_TOGGLE_SKELLOD, DIKEYBOARD_S,                   0, TEXT("Toggle Skeleton LOD"), },
    { INPUT_TOGGLE_LODFADE, DIKEYBOARD_F,                   0, TEXT("Toggle lod blending"), },

    { INPUT_TOGGLE_LOCKLODPOSITION, DIKEYBOARD_L,           0, TEXT("Lock Camera LOD Position"), },
    { INPUT_TOGGLE_INSTANCELOCK,    DIKEYBOARD_I,           0, TEXT("Lock Camera to an instance"), },

    { INPUT_DECREASE_INSTANCES, DIKEYBOARD_LEFT,            0, TEXT("Decrease the number of models (shift/ctrl: by a lot)"), },
    { INPUT_INCREASE_INSTANCES, DIKEYBOARD_RIGHT,           0, TEXT("Increase the number of models (shift/ctrl: by a lot)"), },

    { INPUT_DECREASE_TIMESCALE, DIKEYBOARD_UP,              0, TEXT("Decrease the timescale"), },
    { INPUT_INCREASE_TIMESCALE, DIKEYBOARD_DOWN,            0, TEXT("Increase the timescale"), },

    { INPUT_DECREASE_ALLOWEDERR, DIKEYBOARD_LBRACKET,       0, TEXT("Decrease allowed animation error"), },
    { INPUT_INCREASE_ALLOWEDERR, DIKEYBOARD_RBRACKET,       0, TEXT("Increase allowed animation error"), },

    { INPUT_DECREASE_ANIMLODBIAS, DIKEYBOARD_9,             0, TEXT("Decrease allowed animation LOD bias"), },
    { INPUT_INCREASE_ANIMLODBIAS, DIKEYBOARD_0,             0, TEXT("Increase allowed animation LOD bias"), },

    { INPUT_DECREASE_THREADS, DIKEYBOARD_PRIOR,             0, TEXT("Decrease allowed worker threads"), },
    { INPUT_INCREASE_THREADS, DIKEYBOARD_NEXT,              0, TEXT("Increase allowed worker threads"), },

    // Middle-mouse camera controls.
    { INPUT_MOUSE_LR,         DIMOUSE_XAXIS,                DIA_APPFIXED, TEXT("Mouse X"), },
    { INPUT_MOUSE_UD,         DIMOUSE_YAXIS,                DIA_APPFIXED, TEXT("Mouse Y"), },
    { INPUT_MOUSE_SCROLL,     DIMOUSE_WHEEL,                DIA_APPFIXED, TEXT("Mouse wheel"), },
    { INPUT_MOUSE_LEFT,       DIMOUSE_BUTTON0,              0, TEXT("Mouse left button"), },
    { INPUT_MOUSE_MIDDLE,     DIMOUSE_BUTTON2,              0, TEXT("Mouse middle button"), },
    { INPUT_MOUSE_RIGHT,      DIMOUSE_BUTTON1,              0, TEXT("Mouse right button"), },
};

#define NUMBER_OF_GAMEACTIONS    (sizeof(g_rgGameAction)/sizeof(DIACTION))

//-----------------------------------------------------------------------------
// Global access to the app (needed for the global WndProc())
//-----------------------------------------------------------------------------
CMyD3DApplication* g_pApp  = NULL;
HINSTANCE          g_hInst = NULL;

LPDIRECT3DTEXTURE9 d3dtexRadLogo    = NULL;
LPDIRECT3DTEXTURE9 d3dtexGrannyLogo = NULL;
LPDIRECT3DTEXTURE9 d3dtexBackdrop   = NULL;
LPDIRECT3DTEXTURE9 d3dtexSpeedTest  = NULL;

struct
{
    LPDIRECT3DTEXTURE9 *TexPtr;
    int Width;
    int Height;
    char *Filename;
} TheTextures[] =
{
    // The backdrop is by Adam Hart-Davis (yes, the TV presenter), and it's from the DHD Multimedia Gallery, at http://gallery.hd.org/
    // Say thankyou to the nice people for putting pretty pics on the web for all to use.

    { &d3dtexRadLogo,    256,  256, "media/crowd_scene/radlogo.png" },
    { &d3dtexGrannyLogo, 256,  256, "media/crowd_scene/granny piano logo.png" },
    { &d3dtexBackdrop,   512,  512, "media/crowd_scene/England-Northumberland.jpg" },
    { &d3dtexSpeedTest,  256,  256, "media/crowd_scene/speed_test.png" }
};

bool ShowText = false;
bool IsMultiProc = false;


//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
// Name: WinMain()
// Desc: Entry point to the program. Initializes everything, and goes into a
//       message-processing loop. Idle time is used to render the scene.
//-----------------------------------------------------------------------------
INT WINAPI WinMain( HINSTANCE hInst, HINSTANCE, LPSTR, INT )
{
    CMyD3DApplication d3dApp;

    g_pApp  = &d3dApp;
    g_hInst = hInst;

    InitCommonControls();
    if( FAILED( d3dApp.Create( hInst ) ) )
        return 0;

    return d3dApp.Run();
}




struct ThreadArgs
{
    CMyD3DApplication* Application;

    HANDLE GoEvent;
    HANDLE DoneEvent;
    
    int StartIdx;
    int EndIdx;
    granny_real32 Elapsed;
    bool TerminateLoop;
};


vector<HANDLE>      s_WorkerThreads;
vector<ThreadArgs*> s_ThreadArgs;

DWORD WINAPI DoWorkThreadProc(LPVOID lpParameter)
{
    ThreadArgs* Args = (ThreadArgs*)lpParameter;

    do
    {
        WaitForSingleObject(Args->GoEvent, INFINITE);
        
        for (int i = Args->StartIdx; i < Args->EndIdx; ++i)
            Args->Application->m_walkingInstances[i]->DoThreadWork(Args->Elapsed);

        SetEvent(Args->DoneEvent);
    } while(Args->TerminateLoop == false);
    
    return 0;
}

static void AlignThreadCount(CMyD3DApplication* Application)
{
	if (AppNumThreads == (int)s_WorkerThreads.size())
		return;

	while ((int)s_WorkerThreads.size() < AppNumThreads)
	{

        ThreadArgs* newArgs = new ThreadArgs;
        newArgs->Application = Application;
        newArgs->GoEvent     = CreateEvent(0, FALSE, FALSE, 0);
        newArgs->DoneEvent   = CreateEvent(0, FALSE, FALSE, 0);
        newArgs->StartIdx    = 0;
        newArgs->EndIdx      = 0;
        newArgs->Elapsed     = 0;
        newArgs->TerminateLoop = false;
        s_ThreadArgs.push_back(newArgs);

        HANDLE NewThread = CreateThread(0, 0, DoWorkThreadProc, (int*)newArgs, 0, 0);
        s_WorkerThreads.push_back(NewThread);
    }

    while ((int)s_WorkerThreads.size() > AppNumThreads)
    {
        ThreadArgs* Args   = s_ThreadArgs.back();    s_ThreadArgs.pop_back();
        HANDLE      Thread = s_WorkerThreads.back(); s_WorkerThreads.pop_back();

		Args->TerminateLoop = true;
		Args->StartIdx = 0;
		Args->EndIdx = 0;
		SetEvent(Args->GoEvent);
		WaitForSingleObject(Thread, INFINITE);
		CloseHandle(Args->GoEvent);
		CloseHandle(Args->DoneEvent);
		CloseHandle(Thread);
		delete Args;
    }
}


//-----------------------------------------------------------------------------
// Name: CMyD3DApplication()
// Desc: Application constructor.   Paired with ~CMyD3DApplication()
//       Member variables should be initialized to a known state here.
//       The application window has not yet been created and no Direct3D device
//       has been created, so any initialization that depends on a window or
//       Direct3D should be deferred to a later stage.
//-----------------------------------------------------------------------------
CMyD3DApplication::CMyD3DApplication()
  : m_walkingModel(NULL)
{
    m_dwCreationWidth           = 800;
    m_dwCreationHeight          = 600;
    m_strWindowTitle            = TEXT( "Granny3D Animation - \"Crowd Scene\" Example" );
    m_d3dEnumeration.AppUsesDepthBuffer   = TRUE;
    m_bStartFullscreen          = false;
    m_bShowCursorWhenFullscreen = false;

    // Create a D3D font using d3dfont.cpp
    m_pFont                     = new CD3DFont( _T("Arial"), 12, D3DFONT_BOLD );
    m_bLoadingApp               = TRUE;
    m_pInputDeviceManager       = NULL;
    m_pDIConfigSurface          = NULL;

    ZeroMemory( &m_UserInput, sizeof(m_UserInput) );
    m_fWorldRotX                = 0.0f;
    m_fWorldRotY                = 0.0f;

    m_renderingOn        = true;
    m_renderingSkelOn    = false;
    m_enableAnimationLOD = true;
    m_enableSkeletonLOD  = true;
    m_allowedErr = 3.0f;
    m_animLODBias = 0.2f;
    m_lodPositionLocked = false;

    m_lockInstance = -1;

    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);
    if (sysInfo.dwNumberOfProcessors > 1)
    {
        IsMultiProc = true;
    }
}




//-----------------------------------------------------------------------------
// Name: ~CMyD3DApplication()
// Desc: Application destructor.  Paired with CMyD3DApplication()
//-----------------------------------------------------------------------------
CMyD3DApplication::~CMyD3DApplication()
{
    SAFE_DELETE(m_walkingModel);
}


// LAMELAMELAME!
void ReadAppVariables()
{
    FILE* pFile = fopen("crowd_scene.ini", "rb");
    if (!pFile)
        return;

    fseek(pFile, 0, SEEK_END);
    int len = ftell(pFile);
    fseek(pFile, 0, SEEK_SET);

    char* buffer = new char[len];
    if (fread(buffer, len, 1, pFile))
    {
        char* pLine = strtok(buffer, "\r\n");
        while (pLine)
        {
            if (strncmp(pLine, "AppNumInstances=", strlen("AppNumInstances=")) == 0)
            {
                AppNumInstances = atoi(pLine + strlen("AppNumInstances="));
            }
            else if (strncmp(pLine, "AppMaxAt30=", strlen("AppMaxAt30=")) == 0)
            {
                AppMaxAt30 = atoi(pLine + strlen("AppMaxAt30="));
            }
            else if (strncmp(pLine, "AppNumForSpeedTest=", strlen("AppNumForSpeedTest=")) == 0)
            {
                AppNumForSpeedTest = atoi(pLine + strlen("AppNumForSpeedTest="));
            }

            pLine = strtok(NULL, "\r\n");
        }
    }

    fclose(pFile);
    delete [] buffer;
}

//-----------------------------------------------------------------------------
// Name: OneTimeSceneInit()
// Desc: Paired with FinalCleanup().
//       The window has been created and the IDirect3D9 interface has been
//       created, but the device has not been created yet.  Here you can
//       perform application-related initialization and cleanup that does
//       not depend on a device.
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::OneTimeSceneInit()
{
    // TODO: perform one time initialization
    ReadAppVariables();

    // Drawing loading status message until app finishes loading
    SendMessage( m_hWnd, WM_PAINT, 0, 0 );

    // Initialize DirectInput
    InitInput( m_hWnd );

    // Init Granny's Camera
    CreateDefaultCamera(&m_grannycam);

    // Setup our model and animations...
    m_walkingModel = GetPixoWalkingModel(&m_scene);
    ASSERT(m_walkingModel);

    // Setup N instances...
    for (int i = 0; i < AppNumInstances; i++)
    {
        m_walkingInstances.push_back(new WalkingInstance(m_walkingModel, ArenaRadius));
    }

    // Reset the gameclock
    m_gameClock  = 0.0f;
    m_speedScale = 1.0f;

    m_bLoadingApp = FALSE;
    return S_OK;
}

//-----------------------------------------------------------------------------
// Name: StaticInputAddDeviceCB()
// Desc: Static callback helper to call into CMyD3DApplication class
//-----------------------------------------------------------------------------
HRESULT CALLBACK CMyD3DApplication::StaticInputAddDeviceCB(
    CInputDeviceManager::DeviceInfo* pDeviceInfo,
    const DIDEVICEINSTANCE* pdidi,
    LPVOID pParam )
{
    CMyD3DApplication* pApp = (CMyD3DApplication*) pParam;
    return pApp->InputAddDeviceCB( pDeviceInfo, pdidi );
}




//-----------------------------------------------------------------------------
// Name: InputAddDeviceCB()
// Desc: Called from CInputDeviceManager whenever a device is added.
//       Set the dead zone, and creates a new InputDeviceState for each device
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::InputAddDeviceCB( CInputDeviceManager::DeviceInfo* pDeviceInfo,
                                             const DIDEVICEINSTANCE* pdidi )
{
    UNREFERENCED_PARAMETER( pdidi );

    // Setup the deadzone
    DIPROPDWORD dipdw;
    dipdw.diph.dwSize       = sizeof(DIPROPDWORD);
    dipdw.diph.dwHeaderSize = sizeof(DIPROPHEADER);
    dipdw.diph.dwObj        = 0;
    dipdw.diph.dwHow        = DIPH_DEVICE;
    dipdw.dwData            = 500;
    pDeviceInfo->pdidDevice->SetProperty( DIPROP_DEADZONE, &dipdw.diph );

    // Create a new InputDeviceState for each device so the
    // app can record its state
    InputDeviceState* pNewInputDeviceState = new InputDeviceState;
    ZeroMemory( pNewInputDeviceState, sizeof(InputDeviceState) );
    pDeviceInfo->pParam = (LPVOID) pNewInputDeviceState;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: InitInput()
// Desc: Initialize DirectInput objects
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::InitInput( HWND hWnd )
{
    HRESULT hr;

    // Setup action format for the actual gameplay
    ZeroMemory( &m_diafGame, sizeof(DIACTIONFORMAT) );
    m_diafGame.dwSize          = sizeof(DIACTIONFORMAT);
    m_diafGame.dwActionSize    = sizeof(DIACTION);
    m_diafGame.dwDataSize      = NUMBER_OF_GAMEACTIONS * sizeof(DWORD);
    m_diafGame.guidActionMap   = g_guidApp;

    // TODO: change the genre as needed
    m_diafGame.dwGenre         = DIVIRTUAL_CAD_3DCONTROL;

    m_diafGame.dwNumActions    = NUMBER_OF_GAMEACTIONS;
    m_diafGame.rgoAction       = g_rgGameAction;
    m_diafGame.lAxisMin        = -100;
    m_diafGame.lAxisMax        = 100;
    m_diafGame.dwBufferSize    = 16;
    _tcscpy( m_diafGame.tszActionMap, _T("CrowdScene Game") );

    // Create a new input device manager
    m_pInputDeviceManager = new CInputDeviceManager();

    if( FAILED( hr = m_pInputDeviceManager->Create( hWnd, NULL, m_diafGame,
                                                    StaticInputAddDeviceCB, this ) ) )
	{
        return hr;
	}

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: ConfirmDevice()
// Desc: Called during device initialization, this code checks the display device
//       for some minimum set of capabilities
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::ConfirmDevice( D3DCAPS9* pCaps, DWORD dwBehavior,
                                          D3DFORMAT Format )
{
    UNREFERENCED_PARAMETER( Format );
    UNREFERENCED_PARAMETER( dwBehavior );
    UNREFERENCED_PARAMETER( pCaps );

    BOOL bCapsAcceptable;

    // TODO: Perform checks to see if these display caps are acceptable.
    bCapsAcceptable = TRUE;

    if( bCapsAcceptable )
        return S_OK;
    else
        return E_FAIL;
}




//-----------------------------------------------------------------------------
// Name: InitDeviceObjects()
// Desc: Paired with DeleteDeviceObjects()
//       The device has been created.  Resources that are not lost on
//       Reset() can be created here -- resources in D3DPOOL_MANAGED,
//       D3DPOOL_SCRATCH, or D3DPOOL_SYSTEMMEM.  Image surfaces created via
//       CreateOffScreenPlainSurface are never lost and can be created here.  Vertex
//       shaders and pixel shaders can also be created here as they are not
//       lost on Reset().
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::InitDeviceObjects()
{
    // TODO: create device objects
    HRESULT hr;

    // Init the font
    hr = m_pFont->InitDeviceObjects( m_pd3dDevice );
    if( FAILED( hr ) )
        return hr;

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RestoreDeviceObjects()
// Desc: Paired with InvalidateDeviceObjects()
//       The device exists, but may have just been Reset().  Resources in
//       D3DPOOL_DEFAULT and any other device state that persists during
//       rendering should be set here.  Render states, matrices, textures,
//       etc., that don't change during rendering can be set once here to
//       avoid redundant state setting during Render() or FrameMove().
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::RestoreDeviceObjects()
{
    // TODO: setup render states
    HRESULT hr;

    // Set up the pipeline
    for ( int i = 0; i < 8; i++ )
    {
        m_pd3dDevice->SetTextureStageState( i, D3DTSS_COLOROP,   D3DTOP_DISABLE );
        m_pd3dDevice->SetTextureStageState( i, D3DTSS_COLORARG1, D3DTA_TEXTURE );
        m_pd3dDevice->SetTextureStageState( i, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
        m_pd3dDevice->SetTextureStageState( i, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
        m_pd3dDevice->SetTextureStageState( i, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
        m_pd3dDevice->SetTextureStageState( i, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
        m_pd3dDevice->SetTextureStageState( i, D3DTSS_TEXCOORDINDEX, i );
        m_pd3dDevice->SetTextureStageState( i, D3DTSS_TEXTURETRANSFORMFLAGS, D3DTTFF_DISABLE );
        m_pd3dDevice->SetSamplerState( i, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
        m_pd3dDevice->SetSamplerState( i, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
    }

    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );

    // Set miscellaneous render states
    m_pd3dDevice->SetRenderState( D3DRS_DITHERENABLE,   FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_SPECULARENABLE, FALSE );
    m_pd3dDevice->SetRenderState( D3DRS_ZENABLE,        TRUE );
    m_pd3dDevice->SetRenderState( D3DRS_NORMALIZENORMALS,        TRUE );

    // Set the world matrix
    D3DXMATRIX matIdentity;
    D3DXMatrixIdentity( &matIdentity );
    m_pd3dDevice->SetTransform( D3DTS_WORLD,  &matIdentity );

    // Now using granny_camera.

    // We're using D3D coords, not the default OGL ones.
    m_grannycam.OutputZRange = GrannyCameraOutputZZeroToOne;


    // Not quite sure is this is the right way to do this, but it seems to work.
    int iScreenWidth  = m_d3dSettings.Fullscreen_DisplayMode.Width;
    int iScreenHeight = m_d3dSettings.Fullscreen_DisplayMode.Height;
    if ( m_d3dpp.Windowed )
    {
        iScreenWidth  = m_d3dSettings.Windowed_DisplayMode.Width;
        iScreenHeight = m_d3dSettings.Windowed_DisplayMode.Height;
    }
    float fAspect = GrannyGetMostLikelyPhysicalAspectRatio ( iScreenWidth, iScreenHeight );
    GrannySetCameraAspectRatios ( &m_grannycam, fAspect,
                                  (float)iScreenWidth, (float)iScreenHeight,
                                  (float)m_d3dsdBackBuffer.Width, (float)m_d3dsdBackBuffer.Height );

    // And set up the matrices.
    GrannyBuildCameraMatrices ( &m_grannycam );

    D3DXMATRIX matProj = *(D3DXMATRIX *)&m_grannycam.Projection4x4;
    m_scene.MatProjection = matProj;
    m_pd3dDevice->SetTransform( D3DTS_PROJECTION, &matProj );

    D3DXMATRIX matView = *(D3DXMATRIX *)&m_grannycam.View4x4;
    m_scene.MatView = matView;
    m_pd3dDevice->SetTransform( D3DTS_VIEW, &matView );

    // And granny_camera is right-handed, so flip culling from default.
    m_pd3dDevice->SetRenderState( D3DRS_CULLMODE, D3DCULL_CW );

    // Restore the font
    m_pFont->RestoreDeviceObjects();

    if( !m_bWindowed )
    {
        // Create a surface for configuring DInput devices
        if( FAILED( hr = m_pd3dDevice->CreateOffscreenPlainSurface( 640, 480,
                                                                    m_d3dsdBackBuffer.Format, D3DPOOL_DEFAULT,
                                                                    &m_pDIConfigSurface, NULL ) ) )
		{
			return hr;
		}
    }


    for ( int TexNum = 0; TexNum < sizeof(TheTextures)/sizeof(TheTextures[0]); TexNum++ )
    {
        LPDIRECT3DTEXTURE9 *TexPtr = TheTextures[TexNum].TexPtr;
        ASSERT ( *TexPtr == NULL );
        hr = D3DXCreateTextureFromFileEx (
            m_pd3dDevice, TheTextures[TexNum].Filename,
            TheTextures[TexNum].Width, TheTextures[TexNum].Height,
            D3DX_DEFAULT, 0, D3DFMT_A8R8G8B8,
            D3DPOOL_MANAGED, D3DX_DEFAULT, D3DX_DEFAULT, 0,
            NULL, NULL, TexPtr );
        ASSERT ( SUCCEEDED ( hr ) );
    }

    // Restore the scene objects...
    m_scene.CreateD3DObjects(m_pd3dDevice);

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: StaticConfigureInputDevicesCB()
// Desc: Static callback helper to call into CMyD3DApplication class
//-----------------------------------------------------------------------------
BOOL CALLBACK CMyD3DApplication::StaticConfigureInputDevicesCB(
    IUnknown* pUnknown, VOID* pUserData )
{
    CMyD3DApplication* pApp = (CMyD3DApplication*) pUserData;
    return pApp->ConfigureInputDevicesCB( pUnknown );
}




//-----------------------------------------------------------------------------
// Name: ConfigureInputDevicesCB()
// Desc: Callback function for configuring input devices. This function is
//       called in fullscreen modes, so that the input device configuration
//       window can update the screen.
//-----------------------------------------------------------------------------
BOOL CMyD3DApplication::ConfigureInputDevicesCB( IUnknown* pUnknown )
{
    // Get access to the surface
    LPDIRECT3DSURFACE9 pConfigSurface = NULL;
    if( FAILED( pUnknown->QueryInterface( IID_IDirect3DSurface9,
                                          (VOID**)&pConfigSurface ) ) )
        return TRUE;

    // Render the scene, with the config surface blitted on top
    Render();

    RECT  rcSrc;
    SetRect( &rcSrc, 0, 0, 640, 480 );

    POINT ptDst;
    ptDst.x = (m_d3dsdBackBuffer.Width-640)/2;
    ptDst.y = (m_d3dsdBackBuffer.Height-480)/2;

    LPDIRECT3DSURFACE9 pBackBuffer;
    m_pd3dDevice->GetBackBuffer( 0, 0, D3DBACKBUFFER_TYPE_MONO, &pBackBuffer );
    m_pd3dDevice->UpdateSurface( pConfigSurface, &rcSrc, pBackBuffer, &ptDst );
    pBackBuffer->Release();

    // Present the backbuffer contents to the front buffer
    m_pd3dDevice->Present( 0, 0, 0, 0 );

    // Release the surface
    pConfigSurface->Release();

    return TRUE;
}




void ThreePartLerp ( float *pWeight1, float *pWeight2, float *pWeight3, float TargetSpeed, float Speed1, float Speed2, float Speed3, bool Extrapolate = false )
{
    ASSERT ( Speed1 < Speed2 );
    ASSERT ( Speed2 < Speed3 );
    if ( TargetSpeed <= Speed2 )
    {
        if ( ( TargetSpeed <= Speed1 ) && !Extrapolate )
        {
            // Slower than slowest, and not extrapolating - clamp.
            *pWeight1 = 1.0f;
            *pWeight2 = 0.0f;
            *pWeight3 = 0.0f;
        }
        else
        {
            // Between slowest and mid.
            *pWeight1 = ( Speed2 - TargetSpeed ) / ( Speed2 - Speed1 );
            *pWeight2 = 1.0f - *pWeight1;
            *pWeight3 = 0.0f;
        }
    }
    else
    {
        if ( ( TargetSpeed <= Speed3 ) || Extrapolate )
        {
            // Between mid and fastest, or extrapolating.
            *pWeight1 = 0.0f;
            *pWeight2 = ( Speed3 - TargetSpeed ) / ( Speed3 - Speed2 );
            *pWeight3 = 1.0f - *pWeight2;
        }
        else
        {
            // Faster than fastest.
            *pWeight1 = 0.0f;
            *pWeight2 = 0.0f;
            *pWeight3 = 1.0f;
        }
    }
}


float MoveTowardsTarget ( float CurrentValue, float TargetValue, float MaxChange )
{
    if ( CurrentValue > TargetValue )
    {
        CurrentValue -= MaxChange;
        if ( CurrentValue < TargetValue )
        {
            CurrentValue = TargetValue;
        }
    }
    else if ( CurrentValue < TargetValue )
    {
        CurrentValue += MaxChange;
        if ( CurrentValue > TargetValue )
        {
            CurrentValue = TargetValue;
        }
    }
    return CurrentValue;
}


//-----------------------------------------------------------------------------
// Name: FrameMove()
// Desc: Called once per frame, the call is the entry point for animating
//       the scene.
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::FrameMove()
{
    const float GameTimeElapsed = m_speedScale * m_fElapsedTime;

    // Update user input state
    UpdateInput( &m_UserInput );

    // Respond to input
    if( m_UserInput.bDoConfigureInput && m_bWindowed )  // full-screen configure disabled for now
    {
        // One-shot per keypress
        m_UserInput.bDoConfigureInput = FALSE;

        Pause( true );

        // Get access to the list of semantically-mapped input devices
        // to delete all InputDeviceState structs before calling ConfigureDevices()
        CInputDeviceManager::DeviceInfo* pDeviceInfos;
        DWORD dwNumDevices;
        m_pInputDeviceManager->GetDevices( &pDeviceInfos, &dwNumDevices );

        for( DWORD i=0; i<dwNumDevices; i++ )
        {
            InputDeviceState* pInputDeviceState = (InputDeviceState*) pDeviceInfos[i].pParam;
            SAFE_DELETE( pInputDeviceState );
            pDeviceInfos[i].pParam = NULL;
        }

        // Configure the devices (with edit capability)
        if( m_bWindowed )
            m_pInputDeviceManager->ConfigureDevices( m_hWnd, NULL, NULL, DICD_EDIT, NULL );
        else
            m_pInputDeviceManager->ConfigureDevices( m_hWnd,
                                                     m_pDIConfigSurface,
                                                     (VOID*)StaticConfigureInputDevicesCB,
                                                     DICD_EDIT, (LPVOID) this );

        Pause( false );
    }

    if( m_UserInput.bDoConfigureDisplay )
    {
        // One-shot per keypress
        m_UserInput.bDoConfigureDisplay = FALSE;

        Pause(true);

        // Configure the display device
        UserSelectNewDevice();

        Pause(false);
    }

    // Arbitrary numbers that seem to work well.
    float fRadiansPerMickey        = 100.0f;
    float fZoomFactorPerMickey     = 1.1f;
    float fDistancePerMickeyScaler = 10000.0f;

    // The Mouse X and Y are velocities, so turn them into deltas.
    m_UserInput.fAxisMouseX *= m_fElapsedTime;
    m_UserInput.fAxisMouseY *= m_fElapsedTime;

    // Use the GrannyCam instead.
    if ( m_UserInput.bMouseButtonRight )
    {
        // Rotate around the focus.
        m_grannycam.ElevAzimRoll[0] -= m_UserInput.fAxisMouseY * fRadiansPerMickey;
        m_grannycam.ElevAzimRoll[1] -= m_UserInput.fAxisMouseX * fRadiansPerMickey;
    }
    if ( m_UserInput.bMouseButtonMiddle )
    {
        // Pan focus point around.
        float fDistancePerMickey = fDistancePerMickeyScaler * m_grannycam.Offset[2] * m_grannycam.FOVY / (float)m_d3dsdBackBuffer.Height;

        m_grannycam.Position[0] -= m_UserInput.fAxisMouseX * fDistancePerMickey * m_grannycam.InverseView4x4[0][0];
        m_grannycam.Position[1] -= m_UserInput.fAxisMouseX * fDistancePerMickey * m_grannycam.InverseView4x4[0][1];
        m_grannycam.Position[2] -= m_UserInput.fAxisMouseX * fDistancePerMickey * m_grannycam.InverseView4x4[0][2];

        m_grannycam.Position[0] += m_UserInput.fAxisMouseY * fDistancePerMickey * m_grannycam.InverseView4x4[1][0];
        m_grannycam.Position[1] += m_UserInput.fAxisMouseY * fDistancePerMickey * m_grannycam.InverseView4x4[1][1];
        m_grannycam.Position[2] += m_UserInput.fAxisMouseY * fDistancePerMickey * m_grannycam.InverseView4x4[1][2];
    }
    m_grannycam.Offset[2] *= powf ( fZoomFactorPerMickey, -m_UserInput.fAxisMouseZ );

    // If we have a lock instance, then we need to just grab that position, and ignore the
    //  last move...
    if (m_lockInstance != -1)
    {
        // Make sure that the instance hasn't been deleted...
        if (m_lockInstance < int(m_walkingInstances.size()))
        {
            m_walkingInstances[m_lockInstance]->GetCurrentPosition(m_grannycam.Position);
        }
        else
        {
            m_lockInstance = -1;
        }
    }

    if ( m_UserInput.bToggleHelp )
    {
        ShowText = !ShowText;
        m_UserInput.bToggleHelp = false;
    }

    if ( m_UserInput.iChangeInModelCount != 0)
    {
        if (m_UserInput.bRCtrlKey || m_UserInput.bCtrlKey)
            m_UserInput.iChangeInModelCount *= 100;
        if (m_UserInput.bRShiftKey || m_UserInput.bShiftKey)
            m_UserInput.iChangeInModelCount *= 10;

        while (m_UserInput.iChangeInModelCount > 0)
        {
            m_UserInput.iChangeInModelCount--;
            m_walkingInstances.push_back(new WalkingInstance(m_walkingModel, ArenaRadius));
        }
        while (m_UserInput.iChangeInModelCount < 0)
        {
            m_UserInput.iChangeInModelCount++;
            if (!m_walkingInstances.empty())
            {
                delete m_walkingInstances.back();
                m_walkingInstances.pop_back();
            }
        }
    }

    if ( m_UserInput.iChangeInErr != 0)
    {
        if (m_UserInput.bRCtrlKey || m_UserInput.bCtrlKey)
            m_UserInput.iChangeInErr *= 100;
        else if (m_UserInput.bRShiftKey || m_UserInput.bShiftKey)
            m_UserInput.iChangeInErr *= 10;

        m_allowedErr += m_UserInput.iChangeInErr * 0.1f;
        if (m_allowedErr < 0.0f)
            m_allowedErr = 0.0f;

        m_UserInput.iChangeInErr = 0;
    }

    if ( m_UserInput.fChangeInBias != 0.0f)
    {
        if (m_UserInput.bRCtrlKey || m_UserInput.bCtrlKey)
            m_UserInput.fChangeInBias *= 100;
        else if (m_UserInput.bRShiftKey || m_UserInput.bShiftKey)
            m_UserInput.fChangeInBias *= 10;

        m_animLODBias += m_UserInput.fChangeInBias;
        if (m_animLODBias < 0.0f)
            m_animLODBias = 0.0f;

        m_UserInput.fChangeInBias = 0;
    }

    if (m_UserInput.bToggleLODLock)
    {
        m_UserInput.bToggleLODLock = false;
        m_lodPositionLocked = !m_lodPositionLocked;
        if (m_lodPositionLocked)
        {
            m_lodPosition[0] = m_grannycam.InverseView4x4[3][0];
            m_lodPosition[1] = m_grannycam.InverseView4x4[3][1];
            m_lodPosition[2] = m_grannycam.InverseView4x4[3][2];
        }
    }

    if (m_UserInput.bToggleInstanceLock)
    {
        m_UserInput.bToggleInstanceLock = false;

        if (m_lockInstance < 0 && m_walkingInstances.size() > 0)
        {
            // Lock a random instance if we actually have any...
            // Using a mod rand() is technically a pretty bad thing to do, but what
            //  the hey, we're not making a slot machine here...
            m_lockInstance = (int)(rand() % m_walkingInstances.size());
        }
        else
        {
            m_lockInstance = -1;
        }
    }

    if ( m_UserInput.bReset )
    {
        m_UserInput.bReset = false;

        // Set everything to known-good states.
        CreateDefaultCamera(&m_grannycam);
        m_renderingOn = true;
        m_renderingSkelOn = false;
        m_enableSkeletonLOD = true;
        m_enableAnimationLOD = true;
        m_allowedErr = 3.0f;
        m_animLODBias = 0.2f;
        m_speedScale = 1.0f;
        m_lodPositionLocked = false;
        m_lockInstance = -1;
        ShowText = false;

        AppNumThreads = 1;

        // Reset the instances...
        for (size_t i = 0; i < m_walkingInstances.size(); i++)
        {
            delete m_walkingInstances[i];
        }
        m_walkingInstances.resize(0);

        for (int i = 0; i < AppNumInstances; i++)
        {
            m_walkingInstances.push_back(new WalkingInstance(m_walkingModel, ArenaRadius));
        }
    }

    if (m_UserInput.bSpeedTest || m_UserInput.bMax30Fps)
    {
        int numInstances;
        if (m_UserInput.bSpeedTest)
            numInstances = AppNumForSpeedTest;
        else
            numInstances = AppMaxAt30;

        m_UserInput.bSpeedTest = false;
        m_UserInput.bMax30Fps = false;

        // Set everything to known-good states.
        CreateDefaultCamera(&m_grannycam);
        m_renderingOn = false;
        m_renderingSkelOn = false;
        m_enableSkeletonLOD = true;
        m_enableAnimationLOD = true;
        m_allowedErr = 3.0f;
        m_animLODBias = 0.2f;
        m_speedScale = 1.0f;
        m_lodPositionLocked = false;
        m_lockInstance = -1;
        ShowText = true;

        // Setup a large (!) number of instances for rendering test
        const int RenderTestNumInstances = numInstances;
        while ((int)m_walkingInstances.size() > RenderTestNumInstances)
        {
            delete m_walkingInstances.back();
            m_walkingInstances.pop_back();
        }
        while ((int)m_walkingInstances.size() < RenderTestNumInstances)
        {
            m_walkingInstances.push_back(new WalkingInstance(m_walkingModel, ArenaRadius));
        }
    }

    // Set up the view matrix from the input data.
    GrannyBuildCameraMatrices ( &m_grannycam );
    m_scene.MatView = *(D3DXMATRIX *)&m_grannycam.View4x4;
    m_pd3dDevice->SetTransform( D3DTS_VIEW, (D3DXMATRIX *)&m_grannycam.View4x4 );


    // Update the game clock.
    m_gameClock += GameTimeElapsed;

    const float ErrorAtUnitDist =
        GrannyFindAllowedLODError(m_allowedErr,             // error in pixels
                                  m_d3dsdBackBuffer.Height, // screen height
                                  m_grannycam.FOVY,         // fovy
                                  1.0f);                    // distance

    // Tell each instance about the new time.
    granny_triple CameraPos;
    if (m_lodPositionLocked)
    {
        memcpy(&CameraPos, &m_lodPosition, sizeof(CameraPos));
    }
    else
    {
        CameraPos[0] = m_grannycam.InverseView4x4[3][0];
        CameraPos[1] = m_grannycam.InverseView4x4[3][1];
        CameraPos[2] = m_grannycam.InverseView4x4[3][2];
    }

    granny_system_clock GrannyStart;
    GrannyGetSystemSeconds(&GrannyStart);
    for (size_t InstanceNum = 0; InstanceNum < m_walkingInstances.size(); InstanceNum++)
    {
        float AllowedError = 0.0f;
        if (m_enableAnimationLOD || m_enableSkeletonLOD)
        {
            // note that this computes based on the OLD position, but if the framerate
            //  remains reasonable, this isn't a large problem...
            granny_triple Pos;
            m_walkingInstances[InstanceNum]->GetCurrentPosition(Pos);

            float Dist = 0.0f;
            for (int i = 0; i < 3; i++)
                Dist += ((Pos[i] - CameraPos[i]) *
                         (Pos[i] - CameraPos[i]));
            Dist = sqrt(Dist);

            AllowedError = Dist * ErrorAtUnitDist;
        }

        m_walkingInstances[InstanceNum]->SetLODState(m_enableSkeletonLOD,
                                                     m_enableAnimationLOD,
                                                     AllowedError,
                                                     m_animLODBias);
        m_walkingInstances[InstanceNum]->AdvanceClock(GameTimeElapsed);
    }

	if (AppNumThreads == 1)
    {
        for (size_t InstanceNum = 0; InstanceNum < m_walkingInstances.size(); InstanceNum++)
        {
            m_walkingInstances[InstanceNum]->DoThreadWork(GameTimeElapsed);
        }
    }
    else
    {
		// Make sure we have the appropriate number of worker threads.
		AlignThreadCount(this);

		GrannySetAllowGlobalStateChanges(false);

		// We need to collect the DoneEvents so we can wait on them
        int const Portion = int(m_walkingInstances.size() / AppNumThreads);
        vector<HANDLE> EventHandles(AppNumThreads);

        for (int i = 0; i < AppNumThreads; ++i)
        {
            // start threads for instances [i*Portion, (i+1)*Portion)
            s_ThreadArgs[i]->Application = this;
            s_ThreadArgs[i]->StartIdx = i * Portion;
            
			if (i != AppNumThreads-1) s_ThreadArgs[i]->EndIdx = (i+1) * Portion;
			else                      s_ThreadArgs[i]->EndIdx = (int)m_walkingInstances.size();

			s_ThreadArgs[i]->Elapsed = GameTimeElapsed;
			EventHandles[i] = s_ThreadArgs[i]->DoneEvent;
			SetEvent(s_ThreadArgs[i]->GoEvent);
        }

        WaitForMultipleObjects(AppNumThreads, &EventHandles[0], TRUE, INFINITE);

        GrannySetAllowGlobalStateChanges(true);
    }

    granny_system_clock GrannyEnd;
    GrannyGetSystemSeconds(&GrannyEnd);

    m_animTimeLastFrame = GrannyGetSecondsElapsed(&GrannyStart, &GrannyEnd);

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: UpdateInput()
// Desc: Update the user input.  Called once per frame
//-----------------------------------------------------------------------------
void CMyD3DApplication::UpdateInput( UserInput* pUserInput )
{
    if( NULL == m_pInputDeviceManager )
        return;

    // Get access to the list of semantically-mapped input devices
    CInputDeviceManager::DeviceInfo* pDeviceInfos;
    DWORD dwNumDevices;
    m_pInputDeviceManager->GetDevices( &pDeviceInfos, &dwNumDevices );


    // These are relative values, so always reset them to 0 each time.
    pUserInput->fAxisMouseX = 0.0f;
    pUserInput->fAxisMouseY = 0.0f;
    pUserInput->fAxisMouseZ = 0.0f;

    // Loop through all devices and check game input
    for( DWORD i=0; i<dwNumDevices; i++ )
    {
        DIDEVICEOBJECTDATA rgdod[10];
        DWORD   dwItems = 10;
        HRESULT hr;
        LPDIRECTINPUTDEVICE8 pdidDevice = pDeviceInfos[i].pdidDevice;
        InputDeviceState* pInputDeviceState = (InputDeviceState*) pDeviceInfos[i].pParam;

        hr = pdidDevice->Acquire();
        hr = pdidDevice->Poll();
        hr = pdidDevice->GetDeviceData( sizeof(DIDEVICEOBJECTDATA),
                                        rgdod, &dwItems, 0 );
        if( FAILED(hr) )
            continue;

        // Get the sematics codes for the game menu
        for( DWORD j=0; j<dwItems; j++ )
        {
            BOOL  bButtonState = (rgdod[j].dwData==0x80) ? TRUE : FALSE;
            FLOAT fButtonState = (rgdod[j].dwData==0x80) ? 1.0f : 0.0f;
            FLOAT fAxisState   = (FLOAT)((int)rgdod[j].dwData)/100.0f;
            UNREFERENCED_PARAMETER( fButtonState );

            switch( rgdod[j].uAppData )
            {
                // TODO: Handle semantics for the game

                // Handle relative axis data
                case INPUT_ROTATE_AXIS_LR:
                    pInputDeviceState->fAxisRotateLR = -fAxisState;
                    break;
                case INPUT_ROTATE_AXIS_UD:
                    pInputDeviceState->fAxisRotateUD = -fAxisState;
                    break;

                    // Handle one-shot buttons
                case INPUT_CONFIG_INPUT:   if( bButtonState ) pUserInput->bDoConfigureInput = TRUE; break;
                case INPUT_CONFIG_DISPLAY: if( bButtonState ) pUserInput->bDoConfigureDisplay = TRUE; break;

                case INPUT_MOUSE_LR:        pUserInput->fAxisMouseX = fAxisState; break;
                case INPUT_MOUSE_UD:        pUserInput->fAxisMouseY = fAxisState; break;
                case INPUT_MOUSE_SCROLL:    pUserInput->fAxisMouseZ = fAxisState; break;
                case INPUT_MOUSE_LEFT:      pUserInput->bMouseButtonLeft = bButtonState; break;
                case INPUT_MOUSE_MIDDLE:    pUserInput->bMouseButtonMiddle = bButtonState; break;
                case INPUT_MOUSE_RIGHT:     pUserInput->bMouseButtonRight = bButtonState; break;

                case INPUT_SHIFT_KEY:       pUserInput->bShiftKey = bButtonState; break;
                case INPUT_CTRL_KEY:        pUserInput->bCtrlKey = bButtonState; break;
                case INPUT_RSHIFT_KEY:      pUserInput->bRShiftKey = bButtonState; break;
                case INPUT_RCTRL_KEY:       pUserInput->bRCtrlKey = bButtonState; break;

                case INPUT_TOGGLE_RESET:    if( bButtonState ) pUserInput->bReset = TRUE; break;
                case INPUT_SPEEDTEST:       if( bButtonState ) pUserInput->bSpeedTest = TRUE; break;
                case INPUT_30FPS:           if( bButtonState ) pUserInput->bMax30Fps = TRUE; break;
                case INPUT_TOGGLE_HELP:     if( bButtonState ) pUserInput->bToggleHelp = TRUE; break;

                case INPUT_TOGGLE_SKELLOD:
                    if( bButtonState )
                        g_pApp->m_enableSkeletonLOD = !g_pApp->m_enableSkeletonLOD;
                    break;
                case INPUT_TOGGLE_ANIMLOD:
                    if( bButtonState )
                        g_pApp->m_enableAnimationLOD = !g_pApp->m_enableAnimationLOD;
                    break;
                case INPUT_TOGGLE_LODFADE:
                    if( bButtonState )
                    {
                        if (GrannyGetGlobalLODFadingFactor() != 1.0f)
                        {
                            GrannySetGlobalLODFadingFactor(1.0f);
                        }
                        else
                        {
                            GrannySetGlobalLODFadingFactor(0.8f);
                        }
                    }
                    break;

                case INPUT_TOGGLE_LOCKLODPOSITION:
                    if( bButtonState )
                        pUserInput->bToggleLODLock = true;
                    break;

                case INPUT_TOGGLE_INSTANCELOCK:
                    if( bButtonState )
                        pUserInput->bToggleInstanceLock = true;
                    break;

                case INPUT_TOGGLE_RENDERING: if( bButtonState ) m_renderingOn = !m_renderingOn; break;
                case INPUT_TOGGLE_RENDERSKELETON: if( bButtonState ) m_renderingSkelOn = !m_renderingSkelOn; break;

                case INPUT_INCREASE_INSTANCES:
                    if (bButtonState)
                        pUserInput->iChangeInModelCount = 1;
                    break;
                case INPUT_DECREASE_INSTANCES:
                    if (bButtonState)
                        pUserInput->iChangeInModelCount = -1;
                    break;

                case INPUT_INCREASE_TIMESCALE:
                    if (bButtonState)
                    {
                        m_speedScale -= 0.1f;
                        if (m_speedScale < 0.0f)
                            m_speedScale = 0.0f;
                    }
                    break;
                case INPUT_DECREASE_TIMESCALE:
                    if (bButtonState)
                        m_speedScale += 0.1f;
                    break;

                case INPUT_INCREASE_ALLOWEDERR:
                    if (bButtonState)
                        pUserInput->iChangeInErr = 1;
                    break;

                case INPUT_DECREASE_ALLOWEDERR:
                    if (bButtonState)
                        pUserInput->iChangeInErr = -1;
                    break;

                case INPUT_INCREASE_ANIMLODBIAS:
                    if (bButtonState)
                        pUserInput->fChangeInBias = 0.01f;
                    break;
                case INPUT_DECREASE_ANIMLODBIAS:
                    if (bButtonState)
                        pUserInput->fChangeInBias = -0.01f;
                    break;

                case INPUT_DECREASE_THREADS:
                    if (bButtonState)
                    {
                        --AppNumThreads;
                        if (AppNumThreads <= 0)
                            AppNumThreads = 1;
                    }
                    break;
                case INPUT_INCREASE_THREADS:
                    if (bButtonState)
                    {
                        ++AppNumThreads;
                        if (AppNumThreads > AppMaxThreads)
                            AppNumThreads = AppMaxThreads;
                    }
                    break;

            }
        }
    }

    // TODO: change process code as needed

    // Process user input and store result into pUserInput struct
    pUserInput->fAxisRotateLR = 0.0f;
    pUserInput->fAxisRotateUD = 0.0f;

    // Concatenate the data from all the DirectInput devices
    for( unsigned int i=0; i<dwNumDevices; i++ )
    {
        InputDeviceState* pInputDeviceState = (InputDeviceState*) pDeviceInfos[i].pParam;

        // Use the axis data that is furthest from zero
        if( fabs(pInputDeviceState->fAxisRotateLR) > fabs(pUserInput->fAxisRotateLR) )
            pUserInput->fAxisRotateLR = pInputDeviceState->fAxisRotateLR;

        if( fabs(pInputDeviceState->fAxisRotateUD) > fabs(pUserInput->fAxisRotateUD) )
            pUserInput->fAxisRotateUD = pInputDeviceState->fAxisRotateUD;

        // Process the button data
        if( pInputDeviceState->bButtonRotateLeft )
            pUserInput->fAxisRotateLR = 1.0f;
        else if( pInputDeviceState->bButtonRotateRight )
            pUserInput->fAxisRotateLR = -1.0f;

        if( pInputDeviceState->bButtonRotateUp )
            pUserInput->fAxisRotateUD = 1.0f;
        else if( pInputDeviceState->bButtonRotateDown )
            pUserInput->fAxisRotateUD = -1.0f;
    }
}



void CMyD3DApplication::RenderScreenQuad(float left, float right,
                                         float top,  float bottom,
                                         LPDIRECT3DTEXTURE9 texture,
                                         bool               alphaBlend)
{
    my_rhw_vertex Verts[4] = {
        /*                x      y       z       w     diffuse     u     v      */
        /* top left  */ { left,  top,    0.999f, 0.5f, 0xffffffff, 0.0f, 0.0f },
        /* top right */ { right, top,    0.999f, 0.5f, 0xffffffff, 1.0f, 0.0f },
        /* bot left  */ { left,  bottom, 0.999f, 0.5f, 0xffffffff, 0.0f, 1.0f },
        /* bot right */ { right, bottom, 0.999f, 0.5f, 0xffffffff, 1.0f, 1.0f },
    };

    m_pd3dDevice->SetTexture( 0, texture );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_MODULATE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetRenderState ( D3DRS_LIGHTING, FALSE );
    m_pd3dDevice->SetRenderState ( D3DRS_COLORVERTEX, TRUE );
    m_pd3dDevice->SetFVF ( MY_RHW_VERTEX_FVF );

    if (alphaBlend)
    {
        m_pd3dDevice->SetRenderState ( D3DRS_ALPHATESTENABLE, FALSE );
        m_pd3dDevice->SetRenderState ( D3DRS_ALPHABLENDENABLE, TRUE );
        m_pd3dDevice->SetRenderState ( D3DRS_SRCBLEND, D3DBLEND_SRCALPHA );
        m_pd3dDevice->SetRenderState ( D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA );
    }
    else
    {
        m_pd3dDevice->SetRenderState ( D3DRS_ALPHATESTENABLE, FALSE );
        m_pd3dDevice->SetRenderState ( D3DRS_ALPHABLENDENABLE, FALSE );
    }

    const WORD Indices[6] = { 0, 2, 1, 1, 2, 3 };
    m_pd3dDevice->DrawIndexedPrimitiveUP ( D3DPT_TRIANGLELIST,
                                           0, 4,
                                           2,
                                           Indices, D3DFMT_INDEX16,
                                           Verts, sizeof ( Verts[0] ) );
}


void CMyD3DApplication::RenderStage()
{
    // We're not using the texture for the moment, but leave the coords in there...
    my_simple_vertex Verts[4] = {
        /* x                     y       z                   diffuse     u     v      */
        { -ArenaRadius * 1.2f,  -0.02f, -ArenaRadius * 1.2f, 0xff88aaaa, 0.0f, 0.0f },
        {  ArenaRadius * 1.2f,  -0.02f, -ArenaRadius * 1.2f, 0xff88aaaa, 1.0f, 0.0f },
        {  ArenaRadius * 1.2f,  -0.02f,  ArenaRadius * 1.2f, 0xff88aaaa, 0.0f, 1.0f },
        { -ArenaRadius * 1.2f,  -0.02f,  ArenaRadius * 1.2f, 0xff88aaaa, 1.0f, 1.0f },
    };

    m_pd3dDevice->SetTexture( 0, NULL );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG2 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2 );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
    m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
    m_pd3dDevice->SetRenderState ( D3DRS_ALPHATESTENABLE, FALSE );
    m_pd3dDevice->SetRenderState ( D3DRS_ALPHABLENDENABLE, FALSE );
    m_pd3dDevice->SetRenderState ( D3DRS_LIGHTING, FALSE );
    m_pd3dDevice->SetRenderState ( D3DRS_COLORVERTEX, TRUE );

    m_pd3dDevice->SetStreamSource(0, NULL, 0, 0);
    m_pd3dDevice->SetVertexShader(NULL);
    m_pd3dDevice->SetFVF ( MY_SIMPLE_VERTEX_FVF );

    const WORD Indices[12] = { 0, 1, 2, 2, 3, 0, 0, 2, 1, 2, 0, 3 };

    // Set the world matrix
    D3DXMATRIX matIdentity;
    D3DXMatrixIdentity( &matIdentity );
    m_pd3dDevice->SetTransform( D3DTS_WORLD,  &matIdentity );
    m_pd3dDevice->DrawIndexedPrimitiveUP ( D3DPT_TRIANGLELIST,
                                           0, 4,
                                           4,
                                           Indices, D3DFMT_INDEX16,
                                           Verts, sizeof ( Verts[0] ) );
}

//-----------------------------------------------------------------------------
// Name: Render()
// Desc: Called once per frame, the call is the entry point for 3d
//       rendering. This function sets up render states, clears the
//       viewport, and renders the scene.
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::Render()
{
    // Clear the viewport
    m_pd3dDevice->Clear( 0L, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER,
                         0x000000ff, 1.0f, 0L );

    // Begin the scene
    if( SUCCEEDED( m_pd3dDevice->BeginScene() ) )
    {
        // Render the instances if we have rendering turned on...
        if (m_renderingOn)
        {
            // Render the backdrop
            RenderScreenQuad(0, float(m_d3dsdBackBuffer.Width),
                             0, float(m_d3dsdBackBuffer.Height),
                             d3dtexBackdrop, false);

            // Render the stage
            RenderStage();

            for (size_t i = 0; i < m_walkingInstances.size(); i++)
                m_walkingInstances[i]->Render(m_pd3dDevice);
        }

        if (m_renderingSkelOn)
        {
            // Set the world matrix
            D3DXMATRIX matIdentity;
            D3DXMatrixIdentity( &matIdentity );
            m_pd3dDevice->SetTransform( D3DTS_WORLD,  &matIdentity );
            m_pd3dDevice->SetTexture( 0, NULL );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,   D3DTOP_SELECTARG2 );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAOP,   D3DTOP_SELECTARG2 );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG1, D3DTA_TEXTURE );
            m_pd3dDevice->SetTextureStageState( 0, D3DTSS_ALPHAARG2, D3DTA_DIFFUSE );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSU, D3DTADDRESS_CLAMP );
            m_pd3dDevice->SetSamplerState( 0, D3DSAMP_ADDRESSV, D3DTADDRESS_CLAMP );
            m_pd3dDevice->SetTextureStageState( 1, D3DTSS_COLOROP,   D3DTOP_DISABLE );
            m_pd3dDevice->SetTextureStageState( 1, D3DTSS_ALPHAOP,   D3DTOP_DISABLE );
            m_pd3dDevice->SetRenderState ( D3DRS_ALPHATESTENABLE, FALSE );
            m_pd3dDevice->SetRenderState ( D3DRS_ALPHABLENDENABLE, FALSE );
            m_pd3dDevice->SetRenderState ( D3DRS_LIGHTING, FALSE );
            m_pd3dDevice->SetRenderState ( D3DRS_COLORVERTEX, TRUE );
            m_pd3dDevice->SetStreamSource(0, NULL, 0, 0);
            m_pd3dDevice->SetVertexShader(NULL);

            for (size_t i = 0; i < m_walkingInstances.size(); i++)
                m_walkingInstances[i]->RenderSkeleton(m_pd3dDevice);
        }

        // Render the logos
        {
            // These numbers are good for 16:9 aspect, but we nqeed to adapt to other aspects.
            const float HeightScale = ((float)m_d3dsdBackBuffer.Width / (float)m_d3dsdBackBuffer.Height) / (16.0f / 9.0f);

            {
                const float XL1 = 0.8f * m_d3dsdBackBuffer.Width;
                const float XR1 = 1.0f * m_d3dsdBackBuffer.Width;
                const float YT1 = 0.0f * m_d3dsdBackBuffer.Height * HeightScale;
                const float YB1 = 0.2f * m_d3dsdBackBuffer.Height * HeightScale;
                RenderScreenQuad(XL1, XR1, YT1, YB1, d3dtexGrannyLogo, true);

                const float XL2 = 0.0f  * m_d3dsdBackBuffer.Width;
                const float XR2 = 0.2f  * m_d3dsdBackBuffer.Width;
                const float YT2 = 0.05f * m_d3dsdBackBuffer.Height * HeightScale;
                const float YB2 = 0.20f * m_d3dsdBackBuffer.Height * HeightScale;
                RenderScreenQuad(XL2, XR2, YT2, YB2, d3dtexRadLogo, true);
            }

            if (!m_renderingOn && !m_renderingSkelOn)
            {
                const float XL2 = 0.3f  * m_d3dsdBackBuffer.Width;
                const float XR2 = 0.7f  * m_d3dsdBackBuffer.Width;
                const float YT2 = 0.5f * m_d3dsdBackBuffer.Height * HeightScale;
                const float YB2 = 0.65f * m_d3dsdBackBuffer.Height * HeightScale;
                RenderScreenQuad(XL2, XR2, YT2, YB2, d3dtexSpeedTest, true);
            }
        }

        // Render stats and help text
        RenderText();

        // End the scene.
        m_pd3dDevice->EndScene();
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: RenderText()
// Desc: Renders stats and help text to the scene.
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::RenderText()
{
    D3DCOLOR fontColor        = D3DCOLOR_ARGB(255,255,255,0);

    if ( !ShowText )
    {
        m_pFont->DrawText( 2, 10.0f, fontColor, "Press 'F1' to show help text" );
        return S_OK;
    }

    TCHAR szMsg[1000] = TEXT("");
    for ( int Pass = 0; Pass < 2; Pass++ )
    {
        // Output display stats
        float fNextLine = 3.0f;
        float fLeftEdge = 3.0f;
        fontColor = D3DCOLOR_ARGB(255,0,0,0);
        if ( Pass == 1 )
        {
            fNextLine = 2.0f;
            fLeftEdge = 2.0f;
            fontColor = D3DCOLOR_ARGB(255,255,255,0);
        }

        fNextLine = (FLOAT) m_d3dsdBackBuffer.Height;
        if ( Pass == 1 )
        {
            fNextLine -= 1.0f;
        }

        fNextLine -= 20.0f; m_pFont->DrawText(fLeftEdge, fNextLine, fontColor,
                                              "Thanks to Adam Hart-Davis/DHD Multimedia Gallery for the background");

        fNextLine -= 20.0f;
        sprintf(szMsg, "Animating %d instances.  %4.4f ms last frame  (%4.4f in Granny)",
                (int)m_walkingInstances.size(),
                m_fElapsedTime * 1000.0f,
                m_animTimeLastFrame * 1000.0f);
        fNextLine -= 20.0f; m_pFont->DrawText( fLeftEdge, fNextLine, fontColor, szMsg );

        if (!m_renderingOn && !m_renderingSkelOn)
        {
            // Only fair to compute this if we're not doing rendering... :)
            const int bonesPerSec = int((m_walkingInstances.size() *
                                         m_walkingModel->SourceModel->GrannyModel->Skeleton->BoneCount) /
                                        m_animTimeLastFrame);

            const int nanosPerBone = (int)(1000000000.0 / bonesPerSec);

            sprintf(szMsg, "%d bones/sec (%d nanosec/bone)",
                    bonesPerSec, nanosPerBone);
            fNextLine -= 20.0f; m_pFont->DrawText( fLeftEdge, fNextLine, fontColor, szMsg );
        }

        // Lod stats
        {
            int savedBones = 0;
            int totalBones = 0;
            for (size_t i = 0; i < m_walkingInstances.size(); i++)
            {
                totalBones += m_walkingInstances[i]->TotalBones();
                savedBones += m_walkingInstances[i]->SavedBonesLastFrame();
            }

            sprintf(szMsg, "[%d thread(s)] %sAllowedErr: %g (alb: %g) - Animation LOD: %s  Skeletal LOD: %s (%d/%d bones sampled)",
                    AppNumThreads,
                    m_lodPositionLocked ? "[!!! LOD POSITION LOCKED !!!]  " : "",
                    m_allowedErr, m_animLODBias,
                    m_enableAnimationLOD ? "enabled" : "disabled",
                    m_enableSkeletonLOD ? "enabled" : "disabled",
                    totalBones - savedBones, totalBones);
            fNextLine -= 20.0f; m_pFont->DrawText( fLeftEdge, fNextLine, fontColor, szMsg );
        }
        fNextLine -= 20.0f;

        fNextLine -= 20.0f; m_pFont->DrawText( fLeftEdge, fNextLine, fontColor, "'R' Toggle Rendering" );
        fNextLine -= 20.0f; m_pFont->DrawText( fLeftEdge, fNextLine, fontColor, "'T' Toggle Skeleton Display" );
        fNextLine -= 20.0f; m_pFont->DrawText( fLeftEdge, fNextLine, fontColor, "'A' Toggle Animation LOD");
        fNextLine -= 20.0f; m_pFont->DrawText( fLeftEdge, fNextLine, fontColor, "'S' Toggle Skeltal LOD");
        fNextLine -= 20.0f; m_pFont->DrawText( fLeftEdge, fNextLine, fontColor, "'F' Toggle LOD Blending");
        fNextLine -= 20.0f; m_pFont->DrawText( fLeftEdge, fNextLine, fontColor, "'Left/Right' Add or remove Pixomatic Characters");
        fNextLine -= 20.0f; m_pFont->DrawText( fLeftEdge, fNextLine, fontColor, "'Up  /Down'  Speed up or slow down the simulation");
        fNextLine -= 20.0f; m_pFont->DrawText( fLeftEdge, fNextLine, fontColor, "'Backspace'  Reset Demo" );
        fNextLine -= 20.0f; m_pFont->DrawText( fLeftEdge, fNextLine, fontColor, "'~/1'        Speed Tests" );
    }

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: Pause()
// Desc: Called in to toggle the pause state of the app.
//-----------------------------------------------------------------------------
VOID CMyD3DApplication::Pause( bool bPause )
{
    // Get access to the list of semantically-mapped input devices
    // to zero the state of all InputDeviceState structs.  This is needed
    // because when using DISCL_FOREGROUND, the action mapper will not
    // record actions when the focus switches, for example if a dialog appears.
    // This causes a problem when a button held down when loosing focus, and let
    // go when the focus is lost.  The app will not record that the button
    // has been let go, so the state will be incorrect when focus returns.
    // To fix this either use DISCL_BACKGROUND or zero the state when
    // loosing focus.
    CInputDeviceManager::DeviceInfo* pDeviceInfos;
    DWORD dwNumDevices;
    m_pInputDeviceManager->GetDevices( &pDeviceInfos, &dwNumDevices );

    for( DWORD i=0; i<dwNumDevices; i++ )
    {
        InputDeviceState* pInputDeviceState = (InputDeviceState*) pDeviceInfos[i].pParam;
        ZeroMemory( pInputDeviceState, sizeof(InputDeviceState) );
    }

    CD3DApplication::Pause( bPause );
}




//-----------------------------------------------------------------------------
// Name: MsgProc()
// Desc: Overrrides the main WndProc, so the sample can do custom message
//       handling (e.g. processing mouse, keyboard, or menu commands).
//-----------------------------------------------------------------------------
LRESULT CMyD3DApplication::MsgProc( HWND hWnd, UINT msg, WPARAM wParam,
                                    LPARAM lParam )
{
    switch( msg )
    {
        case WM_PAINT:
        {
            if( m_bLoadingApp )
            {
                // Draw on the window tell the user that the app is loading
                // TODO: change as needed
                HDC hDC = GetDC( hWnd );
                TCHAR strMsg[MAX_PATH];
                wsprintf( strMsg, TEXT("Loading... Please wait") );
                RECT rct;
                GetClientRect( hWnd, &rct );
                DrawText( hDC, strMsg, -1, &rct, DT_CENTER|DT_VCENTER|DT_SINGLELINE );
                ReleaseDC( hWnd, hDC );
            }
            break;
        }

        case WM_COMMAND:
        {
            switch( LOWORD(wParam) )
            {
                case IDM_CONFIGINPUT:
                    m_UserInput.bDoConfigureInput = TRUE;
                    break;

                case IDM_CHANGEDEVICE:
                    m_UserInput.bDoConfigureDisplay = TRUE;
                    return 0; // Don't hand off to parent
            }
            break;
        }

    }

    return CD3DApplication::MsgProc( hWnd, msg, wParam, lParam );
}




//-----------------------------------------------------------------------------
// Name: InvalidateDeviceObjects()
// Desc: Invalidates device objects.  Paired with RestoreDeviceObjects()
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::InvalidateDeviceObjects()
{
    // Clean up the Granny objects.
    m_scene.DestroyD3DObjects ( m_pd3dDevice );

    for ( int TexNum = 0; TexNum < sizeof(TheTextures)/sizeof(TheTextures[0]); TexNum++ )
    {
        LPDIRECT3DTEXTURE9 *TexPtr = TheTextures[TexNum].TexPtr;
        if ( *TexPtr != NULL )
        {
            (*TexPtr)->Release();
            *TexPtr = NULL;
        }
    }

    // TODO: Cleanup any objects created in RestoreDeviceObjects()
    m_pFont->InvalidateDeviceObjects();
    SAFE_RELEASE( m_pDIConfigSurface );

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: DeleteDeviceObjects()
// Desc: Paired with InitDeviceObjects()
//       Called when the app is exiting, or the device is being changed,
//       this function deletes any device dependent objects.
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::DeleteDeviceObjects()
{
    // TODO: Cleanup any objects created in InitDeviceObjects()
    m_pFont->DeleteDeviceObjects();

    return S_OK;
}




//-----------------------------------------------------------------------------
// Name: FinalCleanup()
// Desc: Paired with OneTimeSceneInit()
//       Called before the app exits, this function gives the app the chance
//       to cleanup after itself.
//-----------------------------------------------------------------------------
HRESULT CMyD3DApplication::FinalCleanup()
{
    // TODO: Perform any final cleanup needed
    // Cleanup D3D font
    SAFE_DELETE( m_pFont );

    // Cleanup DirectInput
    CleanupDirectInput();

    return S_OK;
}


//-----------------------------------------------------------------------------
// Name: CleanupDirectInput()
// Desc: Cleanup DirectInput
//-----------------------------------------------------------------------------
VOID CMyD3DApplication::CleanupDirectInput()
{
    if( NULL == m_pInputDeviceManager )
        return;

    // Get access to the list of semantically-mapped input devices
    // to delete all InputDeviceState structs
    CInputDeviceManager::DeviceInfo* pDeviceInfos;
    DWORD dwNumDevices;
    m_pInputDeviceManager->GetDevices( &pDeviceInfos, &dwNumDevices );

    for( DWORD i=0; i<dwNumDevices; i++ )
    {
        InputDeviceState* pInputDeviceState = (InputDeviceState*) pDeviceInfos[i].pParam;
        SAFE_DELETE( pInputDeviceState );
        pDeviceInfos[i].pParam = NULL;
    }

    // Cleanup DirectX input objects
    SAFE_DELETE( m_pInputDeviceManager );

}



//-----------------------------------------------------------------------------
//-----------------------------------------------------------------------------
WalkingModel* GetPixoWalkingModel(Scene* TheScene)
{
    struct AnimListEntry {
        const char*      Filename;
        const char*      AnimationNameKey;
        const char*      TrackGroupName;
        EControlAnimType AnimType;
    } PixoAnimations[] = {
        { "media/crowd_scene/run_fast.gr2",        "run_fast",        "grannyRootBone", ControlAnim_RunMed       },
        { "media/crowd_scene/run_slow.gr2",        "run_slow",        "grannyRootBone", ControlAnim_RunSlow      },
        { "media/crowd_scene/run_sprint.gr2",      "run_sprint",      "grannyRootBone", ControlAnim_RunFast      },
        { "media/crowd_scene/walk_fast.gr2",       "walk_fast",       "grannyRootBone", ControlAnim_WalkMed      },
        { "media/crowd_scene/walk_slow.gr2",       "walk_slow",       "grannyRootBone", ControlAnim_WalkSlow     },
        { "media/crowd_scene/walk_veryFast.gr2",   "walk_veryFast",   "grannyRootBone", ControlAnim_WalkFast     }
    };
    const int NumPixoAnimations = sizeof(PixoAnimations) / sizeof(PixoAnimations[0]);


    WalkingModel* pNewModel = new WalkingModel;
    {
        pNewModel->SourceModel = TheScene->FindOrLoadModel("media/crowd_scene/reference.gr2", "grannyRootBone");

        for (int i = 0; i < NumPixoAnimations; i++)
        {
            const AnimListEntry& anim = PixoAnimations[i];

            pNewModel->SourceAnimations[anim.AnimType] =
                TheScene->FindOrLoadAnimation(anim.Filename,
                                              anim.AnimationNameKey,
                                              anim.TrackGroupName);
        }

        if (pNewModel->IsValidWalkingModel() == false)
        {
            FAIL("Couldn't load the pixo walking model!");
            SAFE_DELETE(pNewModel);
        }
    }

    return pNewModel;
}

void CreateDefaultCamera(granny_camera* TheCamera)
{
    GrannyInitializeDefaultCamera ( TheCamera );
    TheCamera->FOVY = 0.25f * 3.14f;
    TheCamera->NearClipPlane = 0.1f;
    TheCamera->FarClipPlane = 1000.0f;

    // EAR = elevation/azimuth/roll
    TheCamera->ElevAzimRoll[0] = D3DX_PI * -0.10f;
    TheCamera->ElevAzimRoll[1] = D3DX_PI * 0.2f;
    TheCamera->ElevAzimRoll[2] = 0.0f;

    // Offset from target.
    TheCamera->Offset[0] = 0.0f;
    TheCamera->Offset[1] = 0.0f;
    TheCamera->Offset[2] = ArenaRadius * 2.75f;

    // And pointing slightly above the origin
    TheCamera->Position[0] = 0.0f;
    TheCamera->Position[1] = 0.2f;
    TheCamera->Position[2] = 0.0f;
}
