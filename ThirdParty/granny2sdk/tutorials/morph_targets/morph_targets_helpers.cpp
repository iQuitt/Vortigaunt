// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "morph_targets.h"
#include <assert.h>

DemoMesh*
CreateBoundMesh(granny_mesh* GrannyMesh,
                granny_model_instance* ModelInstance,
                granny_file_info* GrannyFile);

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

IDirect3DVertexDeclaration9*  g_pVertexDecl_Morph = NULL;

IDirect3DVertexShader9*       g_pVertexShader_Morph = NULL;
IDirect3DPixelShader9*        g_pPixelShader = NULL;

// These must match the constant declarations in the vertex shaders...
#define MATRIX_CONSTANT_OBJ2WORLD  0
#define MATRIX_CONSTANT_WORLD2VIEW 4
#define MATRIX_CONSTANT_VIEW2CLIP  8
#define VEC3_DIRFROMLIGHT          12
#define VEC4_LIGHTCOLOUR           13
#define VEC4_AMBIENTCOLOUR         14
#define SKINNING_MATRICES          15

DemoScene GlobalScene;

// ---------------------------------------------------------------------------------------
// These functions may be of interest, but aren't directly relevant to the tutorial, so we
// stuff them out of the documentation autogenerate section.
// ---------------------------------------------------------------------------------------

char const* const PixelShaderFile         = "morph_targets\\media\\PixelShader.xpu";
char const* const VertexShaderMorphFile   = "morph_targets\\media\\MorphVS.xvu";

#define HARDCODED_DISPLAY_WIDTH  1280
#define HARDCODED_DISPLAY_HEIGHT 720


#define CHECKED_RELEASE(x) { if (x) { (x)->Release(); } (x) = NULL; } 0

//-------------------------------------------------------------------------------------
// Loads a shader from an .?pu file
//-------------------------------------------------------------------------------------
#define LOAD_SHADER(file, create_fn, shader_var)                    \
    {                                                               \
        FILE* pFile = fopen(file, "rb");                            \
        if (!pFile) return false;                                   \
        fseek(pFile, 0, SEEK_END);                                  \
        int const fileSize = ftell(pFile);                          \
        fseek(pFile, 0, SEEK_SET);                                  \
        void* Buff = malloc(fileSize);                              \
        if (!fread(Buff, 1, fileSize, pFile))                       \
        {                                                           \
            fclose(pFile); free(Buff);                              \
            return false;                                           \
        }                                                           \
        g_pD3DDevice-> ## create_fn( (DWORD*)Buff, & shader_var );  \
        free(Buff);                                                 \
        fclose(pFile);                                              \
    } 0

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

    // Create our shaders...
    LOAD_SHADER(PixelShaderFile,         CreatePixelShader,  g_pPixelShader);
    LOAD_SHADER(VertexShaderMorphFile,   CreateVertexShader, g_pVertexShader_Morph);

    // Create the vertex declarations
    {
        D3DVERTEXELEMENT9 VertexElementsMorph[] = {
            {0,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
            {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   0},
            {0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
            {1,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 1},
            {1, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   1},
            {2,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 2},
            {2, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   2},
            {3,  0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 3},
            {3, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL,   3},
            D3DDECL_END()
        };

        g_pD3DDevice->CreateVertexDeclaration(VertexElementsMorph, &g_pVertexDecl_Morph);
    }

    return true;
}


void CleanupD3D()
{
    CHECKED_RELEASE(g_pPixelShader);
    CHECKED_RELEASE(g_pVertexShader_Morph);
    CHECKED_RELEASE(g_pVertexDecl_Morph);
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

void SetTransposedMatrix4x4(LPDIRECT3DDEVICE9 D3DDevice,
                            int ShaderConstant,
                            float const *Matrix )
{
    float TMatrix[16];
    memcpy(TMatrix, Matrix, sizeof(TMatrix));
    TransposedMatrix4x4(TMatrix);

    D3DDevice->SetVertexShaderConstantF( ShaderConstant, TMatrix, 4 );
}

void SetTransposedMatrix4x3(LPDIRECT3DDEVICE9 D3DDevice,
                            int ShaderConstant,
                            float const *Matrix )
{
    float TMatrix[16];
    memcpy(TMatrix, Matrix, sizeof(TMatrix));
    TransposedMatrix4x4(TMatrix);

    D3DDevice->SetVertexShaderConstantF( ShaderConstant, TMatrix, 3 );
}


bool CreateDemoModels()
{
    assert(GlobalScene.SceneFileInfo);
    assert(g_pD3DDevice);

    /* Iterate across all the textures */
    granny_file_info* Info = GlobalScene.SceneFileInfo;
    {for(granny_int32x ModelIdx = 0; ModelIdx < Info->ModelCount; ++ModelIdx)
    {
        granny_model* GrannyModel = Info->Models[ModelIdx];
        assert(GrannyModel);

        if (GrannyModel->MeshBindingCount > 0)
        {
            DemoModel* NewModel = new DemoModel;
            GlobalScene.Models.push_back(NewModel);

            NewModel->ModelInstance = GrannyInstantiateModel(GrannyModel);
            GrannyBuildCompositeTransform4x4(&GrannyModel->InitialPlacement,
                                             NewModel->InitialMatrix);

            /* Create the meshes that are bound to this model. */
            {for(granny_int32x MeshIdx = 0;
                 MeshIdx < GrannyModel->MeshBindingCount;
                 ++MeshIdx)
            {
                granny_mesh* GrannyMesh = GrannyModel->MeshBindings[MeshIdx].Mesh;
                assert(GrannyMesh);

                DemoMesh* NewMesh =
                    CreateBoundMesh(GrannyMesh, NewModel->ModelInstance, Info);
                assert(NewMesh);

                NewModel->BoundMeshes.push_back(NewMesh);
            }}
        }
    }}

    return true;
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

    {for(granny_uint32x i = 0; i < MorphBuffers.size(); ++i)
    {
        CHECKED_RELEASE(MorphBuffers[i]);
    }}
    MorphBuffers.clear();
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
    MaxBoneCount(-1),
    CurrentTime(0.0f)
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

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

// Since these functions are not really related to the TrackMask demo, I've moved them in
// here.  If this is the first time you've encountered these, you should probably check
// out the versions in the DX9 Rendering tutorial.

// -----------------------------------------------------------------------------
// -----------------------------------------------------------------------------

bool CreateDemoTextures()
{
    assert(GlobalScene.SceneFileInfo);
    assert(g_pD3DDevice);

    /* Iterate across all the textures */
    granny_file_info* Info = GlobalScene.SceneFileInfo;
    {for(granny_int32x TexIdx = 0; TexIdx < Info->TextureCount; ++TexIdx)
    {
        granny_texture* GrannyTexture = Info->Textures[TexIdx];
        assert(GrannyTexture);

        if (GrannyTexture->TextureType == GrannyColorMapTextureType &&
            GrannyTexture->ImageCount == 1)
        {
            /* We select the appropriate 8-bit/channel format based on whether or not the
               image has an alpha channel. */
            D3DFORMAT D3DTexFormat;
            granny_pixel_layout const* GrannyTexFormat;
            if (GrannyTextureHasAlpha(GrannyTexture))
            {
                D3DTexFormat = D3DFMT_A8R8G8B8;
                GrannyTexFormat = GrannyBGRA8888PixelFormat;
            }
            else
            {
                D3DTexFormat = D3DFMT_R8G8B8;
                GrannyTexFormat = GrannyBGR888PixelFormat;
            }

            /* Create and lock the texture surface, and use $CopyTextureImage to move the
               pixels.  This function will handle format conversion, if necessary,
               including adding or removing alpha components, decompressing from Bink or
               S3TC formats, etc. */
            DemoTexture* NewTex = new DemoTexture;
            g_pD3DDevice->CreateTexture(GrannyTexture->Width, GrannyTexture->Height,
                                        1,
                                        0, D3DTexFormat,
                                        D3DPOOL_MANAGED,
                                        &NewTex->TextureBuffer, NULL);
            if (!NewTex->TextureBuffer)
                return false;

            D3DLOCKED_RECT LockedRect;
            NewTex->TextureBuffer->LockRect(0, &LockedRect, 0, 0);

            GrannyCopyTextureImage(GrannyTexture, 0, 0,
                                   GrannyTexFormat,
                                   GrannyTexture->Width,
                                   GrannyTexture->Height,
                                   LockedRect.Pitch,
                                   LockedRect.pBits);

            NewTex->TextureBuffer->UnlockRect(0);


            /* Add the texture to our demo list, after copying the file name. */
            NewTex->Name = new char[strlen(GrannyTexture->FromFileName) + 1];
            strcpy(NewTex->Name, GrannyTexture->FromFileName);

            GlobalScene.Textures.push_back(NewTex);
        }
    }}

    /* Now that we've created all the surfaces required to texture our scene, we can get
       rid of the copy of the pixels still stored in the $file_info.  We'll free the file
       section that contains pixel data.  By default, the exporter creates a $file section
       exclusively for the pixel data ($StandardTextureSection), and stores the $texture
       metadata elsewhere.  So we can free the bulk data, but still make use of the Name,
       Height, Width, etc.

       Once we've freed the file section, we obviously can't access the pixels without
       reloading the file, in a real application, you will need a plan to handle lost
       surfaces, or similar events. */
    if (GlobalScene.SceneFile->SectionCount >= GrannyStandardTextureSection)
    {
        GrannyFreeFileSection(GlobalScene.SceneFile,
                              GrannyStandardTextureSection);
    }

    return true;
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
        /* Setup the camera and lighting parameters, which don't change over the course of
           the scene.  Note that Granny matrices are column-major, by default, while D3D
           matrices are row-major, which is why we call a utility function for the set
           here. */
        SetTransposedMatrix4x4(g_pD3DDevice, MATRIX_CONSTANT_WORLD2VIEW,
                               (FLOAT*)GlobalScene.DemoCamera.View4x4 );
        SetTransposedMatrix4x4(g_pD3DDevice, MATRIX_CONSTANT_VIEW2CLIP,
                               (FLOAT*)GlobalScene.DemoCamera.Projection4x4 );
        g_pD3DDevice->SetVertexShaderConstantF(VEC3_DIRFROMLIGHT,
                                               (FLOAT*)GlobalScene.DirFromLight, 1 );
        g_pD3DDevice->SetVertexShaderConstantF(VEC4_LIGHTCOLOUR,
                                               (FLOAT*)GlobalScene.LightColour, 1 );
        g_pD3DDevice->SetVertexShaderConstantF(VEC4_AMBIENTCOLOUR,
                                               (FLOAT*)GlobalScene.AmbientColour, 1 );
        g_pD3DDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

        // Loop over the models, and render their meshes
        for (size_t Idx = 0; Idx < GlobalScene.Models.size(); Idx++)
        {
            RenderModel(GlobalScene.Models[Idx]);
        }
        g_pD3DDevice->EndScene();
    }

    // Present the backbuffer contents to the display
    g_pD3DDevice->Present( NULL, NULL, NULL, NULL );
}


void Update(granny_real32 const CurrentTime,
            granny_real32 const /*DeltaTime*/)
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
        DemoModel* Model = GlobalScene.Models[Idx];
        GrannySetModelClock(Model->ModelInstance, CurrentTime);
    }

    GlobalScene.CurrentTime = CurrentTime;
}
