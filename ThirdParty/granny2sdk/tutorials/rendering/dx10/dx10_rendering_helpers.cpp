// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "dx10_rendering.h"

// ---------------------------------------------------------------------------------------
// These functions may be of interest, but aren't directly relevant to the tutorial, so we
// stuff them out of the documentation autogenerate section.
// ---------------------------------------------------------------------------------------
char const* const ShaderFile = "media\\Shaders\\DX10_Shaders.fx";

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
        WindowClass.lpszClassName = "DX10Basic_SimpleClass";

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

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory( &sd, sizeof(sd) );
    sd.BufferCount = 1;
    sd.BufferDesc.Width  = HARDCODED_DISPLAY_WIDTH;
    sd.BufferDesc.Height = HARDCODED_DISPLAY_HEIGHT;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = g_hwnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;

    if (FAILED(D3D10CreateDeviceAndSwapChain(NULL, D3D10_DRIVER_TYPE_HARDWARE, NULL,
                                             0, D3D10_SDK_VERSION, &sd, &g_pSwapChain, &g_pD3DDevice)))
    {
        return false;
    }

    // Create depth stencil texture
    {
        ID3D10Texture2D* pDepthStencil = NULL;
        D3D10_TEXTURE2D_DESC descDepth;
        descDepth.Width              = sd.BufferDesc.Width;
        descDepth.Height             = sd.BufferDesc.Height;
        descDepth.MipLevels          = 1;
        descDepth.ArraySize          = 1;
        descDepth.Format             = DXGI_FORMAT_D24_UNORM_S8_UINT ;
        descDepth.SampleDesc         = sd.SampleDesc;
        descDepth.Usage              = D3D10_USAGE_DEFAULT;
        descDepth.BindFlags          = D3D10_BIND_DEPTH_STENCIL;
        descDepth.CPUAccessFlags     = 0;
        descDepth.MiscFlags          = 0;
        if( FAILED( g_pD3DDevice->CreateTexture2D( &descDepth, NULL, &pDepthStencil ) ) )
            return false;

        // Create the depth stencil view
        D3D10_DEPTH_STENCIL_VIEW_DESC descDSV;
        descDSV.Format = descDepth.Format;
        if( descDepth.SampleDesc.Count > 1 )
            descDSV.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2DMS;
        else
            descDSV.ViewDimension = D3D10_DSV_DIMENSION_TEXTURE2D;
        descDSV.Texture2D.MipSlice = 0;
        if( FAILED( g_pD3DDevice->CreateDepthStencilView( pDepthStencil, &descDSV, &g_pDepthView ) ) )
            return false;
        pDepthStencil->Release();
    }

    // Create a render target view
    {
        ID3D10Texture2D *pBackBuffer;
        if (FAILED(g_pSwapChain->GetBuffer( 0, __uuidof( ID3D10Texture2D ), (LPVOID*)&pBackBuffer)))
            return FALSE;
        HRESULT hr = g_pD3DDevice->CreateRenderTargetView(pBackBuffer, NULL, &g_pRenderTargetView);
        pBackBuffer->Release();
        if( FAILED( hr ) )
            return FALSE;
        g_pD3DDevice->OMSetRenderTargets( 1, &g_pRenderTargetView, g_pDepthView );
    }

    // Bind a viewport
    {
        D3D10_VIEWPORT vp;
        vp.Width  = HARDCODED_DISPLAY_WIDTH;
        vp.Height = HARDCODED_DISPLAY_HEIGHT;
        vp.MinDepth = 0.0f;
        vp.MaxDepth = 1.0f;
        vp.TopLeftX = 0;
        vp.TopLeftY = 0;
        g_pD3DDevice->RSSetViewports( 1, &vp );
    }

    // Create our render state object
    {
        D3D10_RASTERIZER_DESC stateDesc;
        memset(&stateDesc, 0, sizeof(stateDesc));

        stateDesc.FillMode = D3D10_FILL_SOLID;
        stateDesc.CullMode = D3D10_CULL_NONE;
        stateDesc.DepthClipEnable = true;

        g_pD3DDevice->CreateRasterizerState(&stateDesc, &g_pRenderState);
    }

    // Load the shader file
    std::vector<granny_uint8> SkinnedBytes;
    {
        granny_file_reader* Reader = GrannyCreatePlatformFileReader(ShaderFile);

        granny_int32x FileSize;
        GrannyGetReaderSize(Reader, &FileSize);
        SkinnedBytes.resize(FileSize);

        GrannyReadAtMost(Reader, 0, FileSize, &SkinnedBytes[0]);
        GrannyCloseFileReader(Reader);
    }


    // Compile the effect
    ID3D10Blob* pErrorBlob   = 0;
    if (FAILED(D3DX10CreateEffectFromFile(ShaderFile, NULL, NULL, "fx_4_0",
                                          D3D10_SHADER_ENABLE_STRICTNESS|D3D10_SHADER_DEBUG|D3D10_SHADER_SKIP_OPTIMIZATION,
                                          0, g_pD3DDevice, NULL,
                                          NULL, &g_pEffect10, &pErrorBlob, NULL)))
    {
        OutputDebugString((char*)pErrorBlob->GetBufferPointer());
        pErrorBlob->Release();
        return false;
    }

    g_pSkinnedTechnique = g_pEffect10->GetTechniqueByName( "Skinned" );
    g_pRigidTechnique   = g_pEffect10->GetTechniqueByName( "Rigid" );

    g_pLightDir      = g_pEffect10->GetVariableByName( "DirFromLight" )->AsVector();
    g_pLightColor    = g_pEffect10->GetVariableByName( "LightColour" )->AsVector();
    g_pAmbientColor  = g_pEffect10->GetVariableByName( "AmbientColour" )->AsVector();
    g_pMatObj2World  = g_pEffect10->GetVariableByName( "ObjToWorld" )->AsMatrix();
    g_pMatWorld2View = g_pEffect10->GetVariableByName( "WorldToView" )->AsMatrix();
    g_pMatView2Clip  = g_pEffect10->GetVariableByName( "ViewToClip" )->AsMatrix();
    g_ptxDiffuse     = g_pEffect10->GetVariableByName( "diffuse_texture" )->AsShaderResource();
    g_pmBoneArray    = g_pEffect10->GetVariableByName( "g_mConstBoneWorld" )->AsMatrix();

    // *** Create our vertex input layouts ****

    // Skinned
    {
        // Correspond to granny_pwnt3432_vertex
        const D3D10_INPUT_ELEMENT_DESC skinned_layout[] =
        {
            { "POSITION",     0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDWEIGHTS", 0, DXGI_FORMAT_R8G8B8A8_UNORM,  0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
            { "BLENDINDICES", 0, DXGI_FORMAT_R8G8B8A8_UINT,   0, 16, D3D10_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",       0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D10_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD",     0, DXGI_FORMAT_R32G32_FLOAT,    0, 32, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        };

        D3D10_PASS_DESC PassDesc;
        g_pSkinnedTechnique->GetPassByIndex( 0 )->GetDesc( &PassDesc );
        g_pD3DDevice->CreateInputLayout(skinned_layout, 5, PassDesc.pIAInputSignature,
                                        PassDesc.IAInputSignatureSize, &g_pSkinnedVertexLayout );
    }

    // Rigid
    {
        // Correspond to granny_pnt332_vertex
        const D3D10_INPUT_ELEMENT_DESC rigid_layout[] =
        {
            { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D10_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD",  0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0 },
        };

        D3D10_PASS_DESC PassDesc;
        g_pRigidTechnique->GetPassByIndex( 0 )->GetDesc( &PassDesc );
        g_pD3DDevice->CreateInputLayout(rigid_layout, 3, PassDesc.pIAInputSignature,
                                        PassDesc.IAInputSignatureSize, &g_pRigidVertexLayout );
    }


    return true;
}


void CleanupD3D()
{
    CHECKED_RELEASE(g_pD3DDevice);
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


DemoTexture::DemoTexture()
  : Name(NULL),
    TextureBuffer(NULL),
    TextureView(NULL)
{

}

DemoTexture::~DemoTexture()
{
    delete [] Name;
    Name = NULL;

    CHECKED_RELEASE(TextureBuffer);
    CHECKED_RELEASE(TextureView);
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
