// =======================================================================================
// dx10_rendering.cpp: Demonstrates rendering an animated mesh in DX9
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "dx10_rendering.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>

using namespace std;

// Note that we're using extremely simple objects, and a global scene
// database that you'll want to completely ignore when you build your
// own app on top of Granny.  This simply keeps everything very
// straightfoward.
char const* const GlobalSceneFilename     = "media\\pixo_dance.gr2";

// Must match the define in the shader.  Better to pass this as a
// constant to the effect compile, actually.
#define MAX_BONE_MATRICES 256

// ---------------------------------------------------------
// Global D3D Objects: Note that we only have a few shader
// objects, so we'll just make them global.
IDXGISwapChain* g_pSwapChain = 0;
ID3D10Device* g_pD3DDevice = 0;

ID3D10RasterizerState* g_pRenderState = 0;

ID3D10RenderTargetView* g_pRenderTargetView = 0;
ID3D10DepthStencilView* g_pDepthView = 0;

ID3D10Effect* g_pEffect10 = 0;
ID3D10EffectTechnique* g_pSkinnedTechnique = 0;
ID3D10EffectTechnique* g_pRigidTechnique = 0;

ID3D10EffectVectorVariable* g_pLightDir = 0;
ID3D10EffectVectorVariable* g_pLightColor = 0;
ID3D10EffectVectorVariable* g_pAmbientColor = 0;

ID3D10EffectMatrixVariable* g_pMatObj2World = 0;
ID3D10EffectMatrixVariable* g_pMatWorld2View = 0;
ID3D10EffectMatrixVariable* g_pMatView2Clip = 0;

ID3D10EffectShaderResourceVariable* g_ptxDiffuse = 0;

ID3D10EffectMatrixVariable* g_pmBoneArray = 0;

ID3D10InputLayout* g_pSkinnedVertexLayout = 0;
ID3D10InputLayout* g_pRigidVertexLayout = 0;


// ---------------------------------------------------------
// Win32 objects
const char* MainWindowTitle = "DirectX10 GrannySample";
HWND g_hwnd = 0;
bool GlobalRunning = false;

bool CreateDemoTextures();
bool CreateDemoModels();
bool CreateSharedPoses();
bool InitCameraAndLights();
void RenderModel(DemoModel*);
void Update(granny_real32 const CurrentTime,
            granny_real32 const DeltaTime);


// ---------------------------------------------------------
// Scene instance objects
DemoScene GlobalScene;


/* DEFTUTORIAL EXPGROUP(TutorialDX10Rendering) (TutorialDX10Rendering_Introduction, Introduction) */
/* Placeholder Text: To Be replaced.
*/


/* DEFTUTORIAL EXPGROUP(TutorialDX10Rendering) (TutorialDX10Rendering_WinMain, WinMain) */
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
               "DX10 compliant renderer required.\n");
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
    if (CreateDemoTextures() == false ||
        CreateDemoModels()   == false ||
        CreateSharedPoses()  == false ||
        InitCameraAndLights() == false)
    {
        return EXIT_FAILURE;
    }


    /* Mark the start time so we can compute update intervals */
    granny_system_clock StartClock;
    GrannyGetSystemSeconds(&StartClock);
    granny_system_clock LastClock = StartClock;

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
        Update(CurrentTime, DeltaTime);

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


/* DEFTUTORIAL EXPGROUP(TutorialDX10Rendering) (TutorialDX10Rendering_CreateDemoTextures, CreateDemoTextures) */
/* This function creates the Direct3D texture objects for every texture listed in the
   Granny file.  We're going to make some assumptions about the textures contained in the
   demo file, namely that the FromFileName field of the $texture uniquely identifies the
   texture, and that there is only one $texture_image per $texture.  Neither of these are
   hard requirements in Granny, cube maps for instance have 6 images per $texture, but
   making them removes a lot of non-essential details from this function.

   We'll also be ignoring the initial format of the $(texture_image)s.  We'll demonstrate
   how to use S3TC textures in another sample.  We use $CopyTextureImage to convert the
   texture from the stored format to a standard RGB8 or RGBA8 format.
*/
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
            D3D10_TEXTURE2D_DESC texDesc = {
                (UINT)GrannyTexture->Width,       // Width
                (UINT)GrannyTexture->Height,      // Height
                1,                          // Miplevels
                1,                          // Array size
                DXGI_FORMAT_R8G8B8A8_UNORM, // Format
                { 1, 0 },                   // Default quality level
                D3D10_USAGE_DYNAMIC,        // Usage: for convenience, we'll lock below
                D3D10_BIND_SHADER_RESOURCE,
                D3D10_CPU_ACCESS_WRITE,
                0
            };

            /* Create and lock the texture surface, and use $CopyTextureImage to move the
               pixels.  This function will handle format conversion, if necessary,
               including adding or removing alpha components, decompressing from Bink or
               S3TC formats, etc. */
            DemoTexture* NewTex = new DemoTexture;
            g_pD3DDevice->CreateTexture2D(&texDesc, 0, &NewTex->TextureBuffer);
            if (!NewTex->TextureBuffer)
                return false;

            D3D10_MAPPED_TEXTURE2D Locked;
            NewTex->TextureBuffer->Map(0, D3D10_MAP_WRITE_DISCARD, 0, &Locked);
            GrannyCopyTextureImage(GrannyTexture, 0, 0,
                                   GrannyRGBA8888PixelFormat,
                                   GrannyTexture->Width,
                                   GrannyTexture->Height,
                                   Locked.RowPitch,
                                   Locked.pData);
            NewTex->TextureBuffer->Unmap(0);

            /* Add the texture to our demo list, after copying the file name. */
            NewTex->Name = new char[strlen(GrannyTexture->FromFileName) + 1];
            strcpy(NewTex->Name, GrannyTexture->FromFileName);

            // Create a shader resource view
            D3D10_SHADER_RESOURCE_VIEW_DESC shaderViewDesc;
            shaderViewDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            shaderViewDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D ;
            shaderViewDesc.Texture2D.MipLevels = 1;
            shaderViewDesc.Texture2D.MostDetailedMip = 1;
            g_pD3DDevice->CreateShaderResourceView(NewTex->TextureBuffer, 0, &NewTex->TextureView);

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

/* DEFTUTORIAL EXPGROUP(TutorialDX10Rendering) (TutorialDX10Rendering_CreateBoundMesh, CreateBoundMesh) */
/* This function creates a mesh binding, and the vertex and index buffers used to render
   the model.  We make a distinction in this demo between meshes that are bound only to
   one bone (i.e, rigid meshes) and those bound to multiple bones (skinned meshes).  We
   select a simple vertex format for each case, and convert the Granny vertices to the D3D
   format we specified (in dx10_rendering_helper.cpp) using $CopyMeshVertices.

   The index buffer is similarly initialized by copying the result of $GetMeshIndices.
   Note that to demonstrate how to use the 16-bit index buffer support in Granny, we check
   to see what the width of the index array is before creating the buffer.  You can
   standardize on 32- or 16-bit D3D index buffer object if you wish, just make sure to use
   $CopyMeshIndices instead, to perform any necessary width adjustments automatically.
*/
DemoMesh* CreateBoundMesh(granny_mesh* GrannyMesh,
                          granny_model_instance* ModelInstance)
{
    assert(GrannyMesh);
    assert(ModelInstance);
    assert(g_pD3DDevice);

    granny_model* SourceModel = GrannyGetSourceModel(ModelInstance);

    /* Create the binding.  $NewMeshBinding takes two $skeleton parameters to allow you to
       attach the mesh to a $skeleton other than the one it was modeled on.  Check
       $BindingAnimationsToMeshes_BindingMeshes for more details.  In this case, we're
       binding the mesh to the original $skeleton, so we just pass it twice. */
    DemoMesh* NewMesh = new DemoMesh;
    NewMesh->Mesh = GrannyMesh;
    NewMesh->MeshBinding = GrannyNewMeshBinding(GrannyMesh,
                                                SourceModel->Skeleton,
                                                SourceModel->Skeleton);

    /* Lookup the texture bindings in the list we created in CreateDemoTextures.  Note
       that this process can insert NULL pointers in to the MaterialBindings array, which
       we will handle in Render(). */
    {for(granny_int32x MatIdx = 0; MatIdx < GrannyMesh->MaterialBindingCount; ++MatIdx)
    {
        granny_material* Material = GrannyMesh->MaterialBindings[MatIdx].Material;
        DemoTexture* Found = NULL;
        if (Material->MapCount >= 1)
        {
            {for(granny_int32x MapIdx = 0; MapIdx < Material->MapCount; ++MapIdx)
            {
                granny_material_map& Map = Material->Maps[MapIdx];

                if ((_stricmp(Map.Usage, "color") == 0 || _stricmp(Map.Usage, "Diffuse color") == 0) &&
                    Map.Material->Texture)
                {
                    {for(granny_uint32x i = 0; i < GlobalScene.Textures.size(); ++i)
                    {
                        if (_stricmp(Map.Material->Texture->FromFileName, GlobalScene.Textures[i]->Name) == 0)
                        {
                            Found = GlobalScene.Textures[i];
                            break;
                        }
                    }}
                }
            }}
        }

        NewMesh->MaterialBindings.push_back(Found);
    }}

    /* Create the d3d index buffer */
    {
        int const BytesPerIndex   = GrannyGetMeshBytesPerIndex(GrannyMesh);
        int const IndexCount      = GrannyGetMeshIndexCount(GrannyMesh);
        int const IndexBufferSize = BytesPerIndex * IndexCount;

        D3D10_BUFFER_DESC idxBuffDesc;
        idxBuffDesc.ByteWidth      = IndexBufferSize;
        idxBuffDesc.Usage          = D3D10_USAGE_IMMUTABLE;
        idxBuffDesc.BindFlags      = D3D10_BIND_INDEX_BUFFER;
        idxBuffDesc.CPUAccessFlags = 0;
        idxBuffDesc.MiscFlags      = 0;

        D3D10_SUBRESOURCE_DATA idxBuffData;
        idxBuffData.pSysMem          = GrannyGetMeshIndices(GrannyMesh);
        idxBuffData.SysMemPitch      = 0;
        idxBuffData.SysMemSlicePitch = 0;

        g_pD3DDevice->CreateBuffer(&idxBuffDesc, &idxBuffData, &NewMesh->IndexBuffer);
        if (NewMesh->IndexBuffer == 0)
        {
            delete NewMesh;
            return NULL;
        }

        if (BytesPerIndex == 2)
        {
            NewMesh->IndexBufferFormat = DXGI_FORMAT_R16_UINT;
        }
        else
        {
            assert(BytesPerIndex == 4);
            NewMesh->IndexBufferFormat = DXGI_FORMAT_R32_UINT;
        }
    }

    /* Create the vertex buffer. */
    {
        int const VertexCount = GrannyGetMeshVertexCount(GrannyMesh);

        vector<granny_uint8> InitBytes;
        if (GrannyMeshIsRigid(GrannyMesh))
        {
            InitBytes.resize(sizeof(granny_pnt332_vertex) * VertexCount);
            GrannyCopyMeshVertices(GrannyMesh, GrannyPNT332VertexType, &InitBytes[0]);
        }
        else
        {
            InitBytes.resize(sizeof(granny_pwnt3432_vertex) * VertexCount);
            GrannyCopyMeshVertices(GrannyMesh, GrannyPWNT3432VertexType, &InitBytes[0]);
        }

        D3D10_BUFFER_DESC vertBuffDesc;
        vertBuffDesc.ByteWidth      = (UINT)InitBytes.size();
        vertBuffDesc.Usage          = D3D10_USAGE_IMMUTABLE;
        vertBuffDesc.BindFlags      = D3D10_BIND_VERTEX_BUFFER;
        vertBuffDesc.CPUAccessFlags = 0;
        vertBuffDesc.MiscFlags      = 0;

        D3D10_SUBRESOURCE_DATA vertBuffData;
        vertBuffData.pSysMem          = &InitBytes[0];
        vertBuffData.SysMemPitch      = 0;
        vertBuffData.SysMemSlicePitch = 0;

        g_pD3DDevice->CreateBuffer(&vertBuffDesc, &vertBuffData, &NewMesh->VertexBuffer);
        if (NewMesh->VertexBuffer == 0)
        {
            delete NewMesh;
            return NULL;
        }
    }

    return NewMesh;
}

/* DEFTUTORIAL EXPGROUP(TutorialDX10Rendering) (TutorialDX10Rendering_CreateDemoModels, CreateDemoModels) */
/* This function creates the $model_instance objects that we'll bind animations to and
   render.  We're only going to concern ourselves with models that are bound to renderable
   meshes in this function.  Real games may want to look a little closer, using the data
   stored on the $(model)s to create lights, cameras, etc.
*/
bool CreateDemoModels()
{
    assert(GlobalScene.SceneFileInfo);
    assert(g_pD3DDevice);

    /* Iterate across all the models */
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

                DemoMesh* NewMesh = CreateBoundMesh(GrannyMesh, NewModel->ModelInstance);
                assert(NewMesh);
                NewModel->BoundMeshes.push_back(NewMesh);
            }}

            /* We're going to use a "fire-and-forget" animation technique here, since
               we're simply going to loop a single animation until the application
               finishes.  Once we create the control, we tell Granny that it's allowed to
               free it when we free the $model_instance, and set the loop count to 0 to
               indicate that we want it to loop forever. */
            {for(granny_int32x AnimIdx = 0; AnimIdx < Info->AnimationCount; ++AnimIdx)
            {
                granny_animation* Animation = Info->Animations[AnimIdx];

                // Simple control, starting at t = 0.0f
                granny_control *Control =
                    GrannyPlayControlledAnimation(0.0f, Animation,
                                                  NewModel->ModelInstance);
                if (Control)
                {
                    // Loop forever
                    GrannySetControlLoopCount(Control, 0);

                    // Allow Granny to free the control when we delete the instance
                    GrannyFreeControlOnceUnused(Control);

                    // Only bind one animation to the model
                    break;
                }
            }}
        }
    }}

    return true;
}

/* DEFTUTORIAL EXPGROUP(TutorialDX10Rendering) (TutorialDX10Rendering_CreateSharedPoses, CreateSharedPoses) */
/* This is a small utility function that scans our $(model_instance)s, looking for the
   maximum bone count.  If we create a $local_pose and a $world_pose that have at least
   this many bones, we can share it among all of the instances, since we're sampling
   serially. */
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


/* DEFTUTORIAL EXPGROUP(TutorialDX10Rendering) (TutorialDX10Rendering_InitCameraAndLights, InitCameraAndLights) */
/* Our test scene contains a model "persp" that contains the camera information that we'll
   extract, and set into our $camera utility object.  This demo uses a simple directional
   light that is hardcoded here, but you can easily extract light info from exported
   models and use that instead.

   We define a structure and $data_type_definition array for the camera fields we want to
   extract.  In the case of the pixo_dance.gr2 source file, the original source art is a
   Maya file, so the parameter names will be different if you're starting with a Max or
   XSI file. */

struct CameraFields
{
    // Ignore everything but the clip planes, position will be extracted from the
    // skeleton, and the FOV will be manually set
    granny_real32 nearClipPlane;
    granny_real32 farClipPlane;
};
granny_data_type_definition CameraFieldsType[] =
{
    { GrannyReal32Member, "nearClipPlane" },
    { GrannyReal32Member, "farClipPlane"  },
    { GrannyEndMember }
};


bool InitCameraAndLights()
{
    assert(GlobalScene.SceneFileInfo);
    granny_file_info* Info = GlobalScene.SceneFileInfo;

    bool FoundCamera = false;
    {for(granny_int32x ModelIdx = 0; ModelIdx < Info->ModelCount; ++ModelIdx)
    {
        if (_stricmp("persp", Info->Models[ModelIdx]->Name) == 0)
        {
            granny_skeleton* CameraSkel = Info->Models[ModelIdx]->Skeleton;

            granny_variant CameraVariant;
            if (CameraSkel->BoneCount == 1 &&
                GrannyFindMatchingMember(CameraSkel->Bones[0].ExtendedData.Type,
                                         CameraSkel->Bones[0].ExtendedData.Object,
                                         "CameraInfo",
                                         &CameraVariant))
            {
                // This should be a reference to the data
                assert(CameraVariant.Type[0].Type == GrannyReferenceMember);

                granny_data_type_definition const *CameraStoredType = CameraVariant.Type[0].ReferenceType;
                void const *CameraObject = *((void const **)(CameraVariant.Object));

                CameraFields CameraFields;
                GrannyConvertSingleObject(CameraStoredType, CameraObject,
                                          CameraFieldsType, &CameraFields, 0);

                // Ok, now we can setup the camera...
                GrannyInitializeDefaultCamera(&GlobalScene.DemoCamera);
                GlobalScene.DemoCamera.NearClipPlane = CameraFields.nearClipPlane;
                GlobalScene.DemoCamera.FarClipPlane  = CameraFields.farClipPlane;
                GlobalScene.DemoCamera.FOVY = 0.2f * 3.14f;
                GlobalScene.DemoCamera.OutputZRange = GrannyCameraOutputZZeroToOne;

                memcpy(GlobalScene.DemoCamera.Orientation,
                       Info->Models[ModelIdx]->InitialPlacement.Orientation,
                       sizeof(GlobalScene.DemoCamera.Orientation));
                memcpy(GlobalScene.DemoCamera.Position,
                       Info->Models[ModelIdx]->InitialPlacement.Position,
                       sizeof(GlobalScene.DemoCamera.Position));

                FoundCamera = true;
                break;
            }
        }
    }}

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

    return FoundCamera;
}

/* DEFTUTORIAL EXPGROUP(TutorialDX10Rendering) (TutorialDX10Rendering_Update, Update) */
void Update(granny_real32 const CurrentTime,
            granny_real32 const) // DeltaTime
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
}


/* DEFTUTORIAL EXPGROUP(TutorialDX10Rendering) (TutorialDX10Rendering_Render, Render) */
/* This routine just sets up some global camera matrices, clears the framebuffer, and then
   forwards the real work to $TutorialDX10Rendering_RenderModel. */
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
    {
        FLOAT ClearColor[] = {0, 0, 1, 1};
        g_pD3DDevice->ClearRenderTargetView(g_pRenderTargetView, ClearColor);
        g_pD3DDevice->ClearDepthStencilView(g_pDepthView, D3D10_CLEAR_DEPTH, 1, 0);
    }

    // Setup our render state
    {
        g_pD3DDevice->RSSetState(g_pRenderState);

        g_pLightDir->SetFloatVector( GlobalScene.DirFromLight );
        g_pLightColor->SetFloatVector( GlobalScene.LightColour);
        g_pAmbientColor->SetFloatVector( GlobalScene.AmbientColour);

        g_pMatWorld2View->SetMatrix((FLOAT*)GlobalScene.DemoCamera.View4x4);
        g_pMatView2Clip->SetMatrix((FLOAT*)GlobalScene.DemoCamera.Projection4x4);
    }

    // Loop over the models, and render their meshes
    for (size_t Idx = 0; Idx < GlobalScene.Models.size(); Idx++)
    {
        RenderModel(GlobalScene.Models[Idx]);
    }

    // Present the backbuffer contents to the display
    g_pSwapChain->Present(1, 0);
}


/* DEFTUTORIAL EXPGROUP(TutorialDX10Rendering) (TutorialDX10Rendering_RenderModel, RenderModel) */
/* This is the meat of the rendering portion of the app.  The only piece of interest here
   as far as learning Granny is setting up the bone constants for the vertex shaded
   skinned meshes.  Everything else is vanilla D3D code. */
void RenderModel(DemoModel* Model)
{
    /* We sample the model's animations "just in time", delaying until we know that we're
       going to render the $model_instance.  In the case of this demo app, we render all
       of the models, all of the time, but if you cull at the object level, you can avoid
       doing a lot of work by simply never calling $SampleModelAnimations (or similar
       functions) on the culled $model_instance.  Here, since we don't need the
       $local_pose, we use $SampleModelAnimationsAccelerated to build the $world_pose in
       whatever Granny decides is the most efficient manner.

       We've enabled "move to origin" for all these matrices (which is a very good idea,
       see: $RootMotion), so we need to send in a matrix representing the initial
       placement to ensure that the models show up in the expected location.  We computed
       that when we created the DemoModel object.  */
    GrannySampleModelAnimationsAccelerated(
        Model->ModelInstance,
        GrannyGetSourceSkeleton(Model->ModelInstance)->BoneCount,
        Model->InitialMatrix,
        GlobalScene.SharedLocalPose,
        GlobalScene.SharedWorldPose);

    /* Iterate across all meshes */
    {for (size_t MeshIndex = 0; MeshIndex < Model->BoundMeshes.size(); ++MeshIndex)
    {
        DemoMesh* Mesh = Model->BoundMeshes[MeshIndex];

        // Load the mesh's index buffer.
        g_pD3DDevice->IASetIndexBuffer(Mesh->IndexBuffer, Mesh->IndexBufferFormat, 0);

        /* The ToBoneIndices holds the mapping from the contiguously index mesh bones to
           original source indices in the bound skeleton.  This is necessary for both
           rigid and skinned meshes.  There is an analogous call to get the mapping to the
           source skeleton, $GetMeshBindingFromBoneIndices, but you'll almost always want
           to use $GetMeshBindingToBoneIndices.  In our case, the two mappings are
           identical, but this is not always true. */
        int const *ToBoneIndices = GrannyGetMeshBindingToBoneIndices(Mesh->MeshBinding);

        ID3D10EffectTechnique* Technique = 0;
        if (GrannyMeshIsRigid(Mesh->Mesh))
        {
            /* It's a rigid mesh, so load the appropriate technique.  Note that this is
               pretty slow, normally you'd order your meshes to minimize the number of
               state switches. */
            Technique = g_pRigidTechnique;
            g_pD3DDevice->IASetInputLayout( g_pRigidVertexLayout );

            UINT Strides[1];
            UINT Offsets[1];
            Strides[0] = sizeof(granny_pnt332_vertex);
            Offsets[0] = 0;
            g_pD3DDevice->IASetVertexBuffers( 0, 1, &Mesh->VertexBuffer, Strides, Offsets );

            /* Now I look up the transform for this mesh, and load it.  (Note that we're
               making the assumption that Bone 0 represents the rigid transform for the
               model.  In most cases, this is a safe assumption.) */
            __declspec(align(16)) granny_matrix_4x4 CompositeMatrix;
            GrannyBuildIndexedCompositeBuffer(GrannyGetMeshBindingToSkeleton(Mesh->MeshBinding),
                                              GlobalScene.SharedWorldPose,
                                              ToBoneIndices, 1,
                                              &CompositeMatrix);

            g_pMatObj2World->SetMatrix((float*)CompositeMatrix);
        }
        else
        {
            Technique = g_pSkinnedTechnique;
            g_pD3DDevice->IASetInputLayout( g_pSkinnedVertexLayout );

            UINT Strides[1];
            UINT Offsets[1];
            Strides[0] = sizeof(granny_pwnt3432_vertex);
            Offsets[0] = 0;
            g_pD3DDevice->IASetVertexBuffers( 0, 1, &Mesh->VertexBuffer, Strides, Offsets );

            int const NumMeshBones = GrannyGetMeshBindingBoneCount(Mesh->MeshBinding);
            assert(NumMeshBones <= MAX_BONE_MATRICES);

            /* TEXT TO BE REPLACED */
            __declspec(align(16)) granny_matrix_4x4 CompositeBuffer[MAX_BONE_MATRICES];
            GrannyBuildIndexedCompositeBuffer(
                GrannyGetMeshBindingToSkeleton(Mesh->MeshBinding),
                GlobalScene.SharedWorldPose,
                ToBoneIndices, NumMeshBones,
                CompositeBuffer);

            g_pmBoneArray->SetMatrixArray( ( float* )CompositeBuffer, 0, NumMeshBones );
        }

        /* Now both the indices and vertices are loaded, so I can render.  Grab the
           material groups and spin over them, changing to the appropriate texture and
           rendering each batch.  A more savvy rendering loop might have instead built a
           sorted list of material groups to minimize texture changes, etc., but this is
           the most basic way to render. */
        int GroupCount = GrannyGetMeshTriangleGroupCount(Mesh->Mesh);
        granny_tri_material_group *Group = GrannyGetMeshTriangleGroups(Mesh->Mesh);
        while(GroupCount--)
        {
            D3D10_TECHNIQUE_DESC techDesc;
            Technique->GetDesc( &techDesc );

            if(Group->MaterialIndex < int(Mesh->MaterialBindings.size()) &&
               Mesh->MaterialBindings[Group->MaterialIndex] != NULL)
            {
                DemoTexture* Texture = Mesh->MaterialBindings[Group->MaterialIndex];
                g_pD3DDevice->PSSetShaderResources(0, 1, &Texture->TextureView);
                HRESULT hr = g_ptxDiffuse->SetResource(Texture->TextureView);
                hr = hr;
            }
            else
            {
                g_ptxDiffuse->SetResource(0);
            }

            // We know that there's only one pass, but let's be general about this
            for (UINT i = 0; i < techDesc.Passes; i++)
            {
                g_pD3DDevice->IASetPrimitiveTopology( D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST  );

                Technique->GetPassByIndex( i )->Apply( 0 );
                g_pD3DDevice->DrawIndexed(3*Group->TriCount, 3*Group->TriFirst, 0);
            }

            ++Group;
        }
    }}
}


/* DEFTUTORIAL EXPGROUP(TutorialDX10Rendering) (TutorialDX10Rendering_Summary, Summary) */
/* TEXT TO BE REPLACED
 */

/* DEFTUTORIALEND */

