// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "dx9_rendering.h"

// ---------------------------------------------------------------------------------------
// These functions may be of interest, but aren't directly relevant to the tutorial, so we
// stuff them out of the documentation autogenerate section.
// ---------------------------------------------------------------------------------------

#define ShaderFile TEXT("media\\Shaders\\SimpleModelShader.hlsl")

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
        WindowClass.lpszClassName = TEXT("DX9Basic_SimpleClass");

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


    ID3DXBuffer* ErrorBuffer = NULL;
    if (D3DXCreateEffectFromFile(g_pD3DDevice,
                                 ShaderFile,
                                 NULL,
                                 NULL,
#if !defined(DEBUG_VS)
                                 0,
#else
                                 D3DXSHADER_DEBUG | D3DXSHADER_SKIPOPTIMIZATION,
#endif
                                 NULL,
                                 &g_pEffect, &ErrorBuffer) != S_OK)
    {
        OutputDebugString((LPCTSTR)ErrorBuffer->GetBufferPointer());
        CHECKED_RELEASE(ErrorBuffer);
        return false;
    }

    g_hMatObj2World     = g_pEffect->GetParameterByName ( NULL, "ObjToWorld" );
    g_hMatWorld2View    = g_pEffect->GetParameterByName ( NULL, "WorldToView" );
    g_hMatView2Clip     = g_pEffect->GetParameterByName ( NULL, "ViewToClip" );
    g_hDirFromLight     = g_pEffect->GetParameterByName ( NULL, "DirFromLight" );
    g_hLightColor       = g_pEffect->GetParameterByName ( NULL, "LightColour" );
    g_hAmbientColor     = g_pEffect->GetParameterByName ( NULL, "AmbientColour" );
    g_hSkinningMatrices = g_pEffect->GetParameterByName ( NULL, "BoneMatrices" );
    g_hDiffuseTexture   = g_pEffect->GetParameterByName ( NULL, "diffuse_texture" );

    g_SkinnedTechnique = g_pEffect->GetTechniqueByName ( "Skinned" );
    if (FAILED(g_pEffect->ValidateTechnique(g_SkinnedTechnique)))
        return false;

    g_RigidTechnique = g_pEffect->GetTechniqueByName ( "Rigid" );
    if (FAILED(g_pEffect->ValidateTechnique(g_RigidTechnique)))
        return false;


    // Create the rigid and skinned vertex declarations
    {
        D3DVERTEXELEMENT9 VertexElementsRigid[] = {
            { 0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0 },
            { 0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0 },
            { 0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0 },
            D3DDECL_END()
        };

        // Note the position[1-3] elements are pulled from vertex
        // stream 1, rather than 0.  We'll be binding stream 1 to our
        // matrix palette.  Essentially we're hijacking those vertex
        // semantics to represent a set of 4x3 matrices.
        const D3DVERTEXELEMENT9 VertexElementsSkinned[] = {
            { 0,  0, D3DDECLTYPE_FLOAT3,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION,     0 },
            { 0, 12, D3DDECLTYPE_UBYTE4N, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDWEIGHT,  0 },
            { 0, 16, D3DDECLTYPE_UBYTE4,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BLENDINDICES, 0 },
            { 0, 20, D3DDECLTYPE_FLOAT3,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,       0 },
            { 0, 32, D3DDECLTYPE_FLOAT2,  D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD,     0 },
            D3DDECL_END()
        };

        g_pD3DDevice->CreateVertexDeclaration( VertexElementsRigid,   &g_pVertexDecl_Rigid   );
        g_pD3DDevice->CreateVertexDeclaration( VertexElementsSkinned, &g_pVertexDecl_Skinned );
    }

    return true;
}


void CleanupD3D()
{
    CHECKED_RELEASE(g_pEffect);
    CHECKED_RELEASE(g_pVertexDecl_Skinned);
    CHECKED_RELEASE(g_pVertexDecl_Rigid);
    CHECKED_RELEASE(g_pD3DDevice);
    CHECKED_RELEASE(g_pD3D);
}


void TransposedMatrix4x4(granny_real32 *Matrix)
{
    granny_real32 TMatrix[16];
    static const int remaps[16] = {
        0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14, 3, 7, 11, 15
    };

    for (int i = 0; i < 16; i++)
        TMatrix[i] = Matrix[remaps[i]];
    memcpy(Matrix, TMatrix, sizeof(TMatrix));
}

void SetTransposedMatrix4x4(D3DXHANDLE ShaderConstant,
                            float const *Matrix )
{
    float TMatrix[16];
    memcpy(TMatrix, Matrix, sizeof(TMatrix));
    TransposedMatrix4x4(TMatrix);

    g_pEffect->SetMatrix(ShaderConstant, (D3DXMATRIX const*)Matrix);
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

    return Result;
}


DemoTexture::DemoTexture()
  : Name(NULL),
    TextureBuffer(NULL)
{

}

DemoTexture::~DemoTexture()
{
    delete [] Name;
    Name = NULL;

    CHECKED_RELEASE(TextureBuffer);
}

DemoMesh::DemoMesh()
  : Mesh(NULL),
    MeshBinding(NULL),
    IndexBuffer(NULL),
    VertexBuffer(NULL)
{

}

DemoMesh::~DemoMesh()
{
    // We don't own these
    Mesh = NULL;
    MaterialBindings.clear();

    // We do own these
    if (MeshBinding)
    {
        GrannyFreeMeshBinding(MeshBinding);
        MeshBinding = NULL;
    }

    CHECKED_RELEASE(IndexBuffer);
    CHECKED_RELEASE(VertexBuffer);
}

DemoModel::DemoModel()
  : ModelInstance(NULL)
{

}

DemoModel::~DemoModel()
{
    if (ModelInstance)
    {
        GrannyFreeModelInstance(ModelInstance);
        ModelInstance = NULL;
    }

    {for(granny_uint32x i = 0; i < BoundMeshes.size(); ++i)
    {
        delete BoundMeshes[i];
        BoundMeshes[i] = NULL;
    }}

    BoundMeshes.clear();
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
