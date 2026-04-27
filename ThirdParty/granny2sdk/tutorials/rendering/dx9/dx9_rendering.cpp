// =======================================================================================
// dx9_rendering.cpp: Demonstrates rendering an animated mesh in DX9
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "dx9_rendering.h"
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

// ---------------------------------------------------------
// Global D3D Objects: Note that we only have a few shader
// objects, so we'll just make them global.
IDirect3D9*       g_pD3D = NULL;
IDirect3DDevice9* g_pD3DDevice = NULL;

IDirect3DVertexDeclaration9*  g_pVertexDecl_Rigid = NULL;
IDirect3DVertexDeclaration9*  g_pVertexDecl_Skinned = NULL;

ID3DXEffect* g_pEffect = NULL;
D3DXHANDLE   g_RigidTechnique = NULL;
D3DXHANDLE   g_SkinnedTechnique = NULL;

D3DXHANDLE g_hMatObj2World;
D3DXHANDLE g_hMatWorld2View;
D3DXHANDLE g_hMatView2Clip;
D3DXHANDLE g_hDirFromLight;
D3DXHANDLE g_hLightColor;
D3DXHANDLE g_hAmbientColor;
D3DXHANDLE g_hSkinningMatrices;
D3DXHANDLE g_hDiffuseTexture;


// ---------------------------------------------------------
// Win32 objects
HWND g_hwnd = NULL;
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


/* DEFTUTORIAL EXPGROUP(TutorialDX9Rendering) (TutorialDX9Rendering_Introduction, Introduction) */
/* Ok, so this is the tutorial you're really looking for.  We'll walk all the way from
   loading a Granny file to rendering the animated mesh using a hardware vertex shader.
   As we discussed in $TutorialOverview, we don't have tutorial pages of this sort for
   each platform, but you should look for the example for your platform in addition to
   this sample.

   This is going to be a full sample application that loads, preps, and renders a
   complicated scene with multiple models, both skinned and rigid.  We'll handle sending
   textures to the rendering API, control the clock and position the camera using Granny's
   built-in camera utilities.  Because this is a large sample app, it's going to be hard
   to structure the autoextracted tutorial page to read as a cohesive whole from start to
   finish.  Please, work from the source, and use the function index below as a guide when
   reading this page.

   In rough outline, the app will load our scene (pixo_dance.gr2), create instanced
   objects that will hold the information about each $model_instance, it's bound $(mesh)es
   and $(material)s, construct the texture object in DX9 that will hold our image buffers,
   and play the single animation on the scene, which will control all of the demo objects.
   Note that because this demo is playing the equivalant of an in-game cutscene, there is
   no animation blending going on in this tutorial.  See $TutorialBlending for examples of
   that functionality.

   Prerequisites to this demo:
   $* $TutorialBasicLoading
   $* $TutorialCreateControl
   $-

   New functions and structures discussed in this tutorial:
   $* Many
   $-

   Function index:
   $* $TutorialDX9Rendering_WinMain
   $* $TutorialDX9Rendering_CreateDemoTextures
   $* $TutorialDX9Rendering_CreateBoundMesh
   $* $TutorialDX9Rendering_CreateDemoModels
   $* $TutorialDX9Rendering_CreateSharedPoses
   $* $TutorialDX9Rendering_InitCameraAndLights
   $* $TutorialDX9Rendering_Update
   $* $TutorialDX9Rendering_Render
   $* $TutorialDX9Rendering_RenderModel
   $-

*/


/* DEFTUTORIAL EXPGROUP(TutorialDX9Rendering) (TutorialDX9Rendering_WinMain, WinMain) */
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


/* DEFTUTORIAL EXPGROUP(TutorialDX9Rendering) (TutorialDX9Rendering_CreateDemoTextures, CreateDemoTextures) */
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
            /* We select the appropriate 8-bit/channel format based on whether or not the
               image has an alpha channel.  Note that many cards don't actually support a
               24-bit R8G8B8 texture format, so use the X8 version if the texture doesn't
               have alpha. */
            granny_pixel_layout const* GrannyTexFormat = GrannyBGRA8888PixelFormat;

            D3DFORMAT D3DTexFormat;
            if (GrannyTextureHasAlpha(GrannyTexture))
                D3DTexFormat = D3DFMT_A8R8G8B8;
            else
                D3DTexFormat = D3DFMT_X8R8G8B8;

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

/* DEFTUTORIAL EXPGROUP(TutorialDX9Rendering) (TutorialDX9Rendering_CreateBoundMesh, CreateBoundMesh) */
/* This function creates a mesh binding, and the vertex and index buffers used to render
   the model.  We make a distinction in this demo between meshes that are bound only to
   one bone (i.e, rigid meshes) and those bound to multiple bones (skinned meshes).  We
   select a simple vertex format for each case, and convert the Granny vertices to the D3D
   format we specified (in dx9_rendering_helper.cpp) using $CopyMeshVertices.

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

    /* Now create the d3d index buffer */
    D3DFORMAT IndexFormat;
    int BytesPerIndex;
    int IndexCount = GrannyGetMeshIndexCount(GrannyMesh);
    if (GrannyGetMeshBytesPerIndex(GrannyMesh) == 2)
    {
        IndexFormat = D3DFMT_INDEX16;
        BytesPerIndex = 2;
    }
    else
    {
        assert(GrannyGetMeshBytesPerIndex(GrannyMesh) == 4);
        IndexFormat = D3DFMT_INDEX32;
        BytesPerIndex = 4;
    }

    int const IndexBufferSize = BytesPerIndex * IndexCount;
    g_pD3DDevice->CreateIndexBuffer(IndexBufferSize,
                                    D3DUSAGE_WRITEONLY,
                                    IndexFormat,
                                    D3DPOOL_MANAGED,
                                    &NewMesh->IndexBuffer, NULL);
    if (NewMesh->IndexBuffer == 0)
    {
        delete NewMesh;
        return NULL;
    }

    BYTE *Indices;
    NewMesh->IndexBuffer->Lock(0, IndexBufferSize, (void**)&Indices, 0);
    memcpy(Indices, GrannyGetMeshIndices(GrannyMesh), IndexBufferSize);
    NewMesh->IndexBuffer->Unlock();

    /* And the vertex buffer.  You can examine the D3D format specifications that we
       selected for g_pVertexDecl_Rigid and g_pVertexDecl_Skinned in
       dx9_rendering_helpers.cpp.  We chose the formats to correspond to the Granny vertex
       formats $pnt332_vertex (rigid) and $pwnt3432_vertex (skinned).  You are not
       restricted to built-in formats, check the main Granny docs for details on how to
       create your own format.

       For skinned meshes, we also need to check at this point that the number of bone
       bindings in the $mesh doesn't exceed the number of constant register slots we have
       available in our vertex shader.  If a source mesh exceeds the number of constant
       registers available to you on your minimum spec hardware, you can use the Granny
       Preprocessor to break the mesh up into renderable chunks without imposing
       restrictions on your artists. */
    granny_data_type_definition* VertexFormat;
    int VertexBufferSize;
    if (GrannyMeshIsRigid(GrannyMesh))
    {
        VertexFormat = GrannyPNT332VertexType;
        VertexBufferSize = (sizeof(granny_pnt332_vertex) *
                            GrannyGetMeshVertexCount(GrannyMesh));
    }
    else
    {
        VertexFormat = GrannyPWNT3432VertexType;
        VertexBufferSize = (sizeof(granny_pwnt3432_vertex) *
                            GrannyGetMeshVertexCount(GrannyMesh));
    }

    g_pD3DDevice->CreateVertexBuffer(VertexBufferSize,
                                     D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED,
                                     &NewMesh->VertexBuffer, NULL);
    if (NewMesh->VertexBuffer == 0)
    {
        delete NewMesh;
        return NULL;
    }

    BYTE *Vertices;
    NewMesh->VertexBuffer->Lock(0, VertexBufferSize, (void**)&Vertices, 0);
    GrannyCopyMeshVertices(GrannyMesh, VertexFormat, Vertices);
    NewMesh->VertexBuffer->Unlock();

    return NewMesh;
}

/* DEFTUTORIAL EXPGROUP(TutorialDX9Rendering) (TutorialDX9Rendering_CreateDemoModels, CreateDemoModels) */
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

/* DEFTUTORIAL EXPGROUP(TutorialDX9Rendering) (TutorialDX9Rendering_CreateSharedPoses, CreateSharedPoses) */
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


/* DEFTUTORIAL EXPGROUP(TutorialDX9Rendering) (TutorialDX9Rendering_InitCameraAndLights, InitCameraAndLights) */
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

/* DEFTUTORIAL EXPGROUP(TutorialDX9Rendering) (TutorialDX9Rendering_Update, Update) */
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


/* DEFTUTORIAL EXPGROUP(TutorialDX9Rendering) (TutorialDX9Rendering_Render, Render) */
/* This routine just sets up some global camera matrices, clears the framebuffer, and then
   forwards the real work to $TutorialDX9Rendering_RenderModel. */
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
        SetTransposedMatrix4x4(g_hMatWorld2View, (FLOAT*)GlobalScene.DemoCamera.View4x4 );
        SetTransposedMatrix4x4(g_hMatView2Clip, (FLOAT*)GlobalScene.DemoCamera.Projection4x4 );
        g_pEffect->SetFloatArray(g_hDirFromLight, (FLOAT*)GlobalScene.DirFromLight, 3);
        g_pEffect->SetFloatArray(g_hLightColor, (FLOAT*)GlobalScene.LightColour, 3);
        g_pEffect->SetFloatArray(g_hAmbientColor, (FLOAT*)GlobalScene.AmbientColour, 3);

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


/* DEFTUTORIAL EXPGROUP(TutorialDX9Rendering) (TutorialDX9Rendering_RenderModel, RenderModel) */
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
        g_pD3DDevice->SetIndices(Mesh->IndexBuffer);

        /* The ToBoneIndices holds the mapping from the contiguously index mesh bones to
           original source indices in the bound skeleton.  This is necessary for both
           rigid and skinned meshes.  There is an analogous call to get the mapping to the
           source skeleton, $GetMeshBindingFromBoneIndices, but you'll almost always want
           to use $GetMeshBindingToBoneIndices.  In our case, the two mappings are
           identical, but this is not always true. */
        int const *ToBoneIndices =
            GrannyGetMeshBindingToBoneIndices(Mesh->MeshBinding);

        if (GrannyMeshIsRigid(Mesh->Mesh))
        {
            /* It's a rigid mesh, so load the appropriate technique.  Note that this is
               pretty slow, normally you'd order your meshes to minimize the number of
               state switches. */
            g_pEffect->SetTechnique(g_RigidTechnique);
            g_pD3DDevice->SetVertexDeclaration( g_pVertexDecl_Rigid );
            g_pD3DDevice->SetStreamSource(0, Mesh->VertexBuffer, 0,
                                          sizeof(granny_pnt332_vertex));

            /* Now I look up the transform for this mesh, and load it.  (Note that we're
               making the assumption that Bone 0 represents the rigid transform for the
               model.  In most cases, this is a safe assumption.) */
            __declspec(align(16)) granny_matrix_4x4 CompositeMatrix;
            GrannyBuildIndexedCompositeBuffer(
                GrannyGetMeshBindingToSkeleton(Mesh->MeshBinding),
                GlobalScene.SharedWorldPose,
                ToBoneIndices, 1,
                &CompositeMatrix);

            g_pEffect->SetMatrix(g_hMatObj2World, (D3DXMATRIX const*)CompositeMatrix);
        }
        else
        {
            /* It's a skinned mesh, activate the appropriate technique. */
            int const NumMeshBones = GrannyGetMeshBindingBoneCount(Mesh->MeshBinding);
            assert(NumMeshBones <= 64);

            g_pEffect->SetTechnique(g_SkinnedTechnique);
            g_pD3DDevice->SetVertexDeclaration(g_pVertexDecl_Skinned);

            /* Our source vertices are set as stream source 0 */
            g_pD3DDevice->SetStreamSource(0, Mesh->VertexBuffer, 0,
                                          sizeof(granny_pwnt3432_vertex));

            /* Load the matrices into the constant registers.  This is about the simplest
               way to go about this.  Remember that I said we'd be talking about the call
               to $NewWorldPoseNoComposite?  Here's the payoff.  When you call that
               routine, it creates a $world_pose without a Composite array attached, which
               allows Granny to skip those matrices when $BuildWorldPose is called.Since
               we're just going to copy a few of the matrices into the constant buffer, we
               delay until this point, at which we know exactly which matrices we need,
               and exactly where they need to go.  $BuildIndexedCompositeBuffer is
               designed to do this for you.  (If you need the matrices transposed, use
               $BuildIndexedCompositeBufferTransposed.)

               The static buffer is necessary, since you cannot lock a constant buffer in
               DirectX 9.  (Wait for DX10, the most efficient way to do this will change
               again.)  We know that no mesh in our scene will bind to more than 64 bones,
               since we've preprocessed the data to ensure that all meshes are broken up
               into pieces of at most that size. */
            __declspec(align(16)) granny_matrix_4x4 CompositeBuffer[64];
            GrannyBuildIndexedCompositeBuffer(
                GrannyGetMeshBindingToSkeleton(Mesh->MeshBinding),
                GlobalScene.SharedWorldPose,
                ToBoneIndices, NumMeshBones,
                CompositeBuffer);

            g_pEffect->SetMatrixArray(g_hSkinningMatrices,
                                      (D3DXMATRIX const*)CompositeBuffer,
                                      NumMeshBones);
        }

        /* Now both the indices and vertices are loaded, so I can render.  Grab the
           material groups and spin over them, changing to the appropriate texture and
           rendering each batch.  A more savvy rendering loop might have instead built a
           sorted list of material groups to minimize texture changes, etc., but this is
           the most basic way to render. */
        int const VertexCount = GrannyGetMeshVertexCount(Mesh->Mesh);
        int GroupCount = GrannyGetMeshTriangleGroupCount(Mesh->Mesh);
        granny_tri_material_group *Group =
            GrannyGetMeshTriangleGroups(Mesh->Mesh);
        while(GroupCount--)
        {
            if(Group->MaterialIndex < int(Mesh->MaterialBindings.size()) &&
               Mesh->MaterialBindings[Group->MaterialIndex] != NULL)
            {
                DemoTexture* Texture = Mesh->MaterialBindings[Group->MaterialIndex];
                g_pEffect->SetTexture(g_hDiffuseTexture, Texture->TextureBuffer);
            }
            else
            {
                g_pEffect->SetTexture(g_hDiffuseTexture, NULL);
            }

            // We know that there's only one pass, but let's be general about this
            UINT numPasses;
            g_pEffect->Begin(&numPasses, 0);
            for (UINT i = 0; i < numPasses; i++)
            {
                g_pEffect->BeginPass(i);

                g_pD3DDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,
                                                   0,
                                                   0, VertexCount,
                                                   3*Group->TriFirst,
                                                   Group->TriCount);

                g_pEffect->EndPass();
            }
            g_pEffect->End();

            ++Group;
        }
    }}
}


/* DEFTUTORIAL EXPGROUP(TutorialDX9Rendering) (TutorialDX9Rendering_Summary, Summary) */
/* Whew, that was a long one.  You now have an example of a full DX9 application to start
   from.  From here it should be relatively easy to add elaborations (more interesting
   materials, lighting, etc), or work out how to plug models and animation into your
   existing app.  I'll emphasize again that the optimal path for each platform may be
   slightly different than what is presented here.  Mostly the variations deal with how to
   setup and work with the skinning matrices, so don't worry, all of this is still
   relevant.  You should check out the correct platform example to find out what is
   different on your hardware.
   $-
   This tutorial represents the end of the "Rendering an Animated Model" chain in
   $TutorialOverview, you will want to head back to that document to figure out where to
   go next.
*/

/* DEFTUTORIALEND */

