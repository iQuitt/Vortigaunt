// =======================================================================================
// lod.cpp: Demonstrates using LOD to reduce sampling CPU costs
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "lod.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>

using namespace std;

// Note that we're using extremely simple objects, and a global scene
// database that you'll want to completely ignore when you build your
// own app on top of Granny.  This simply keeps everything very
// straightfoward.
char const* const GlobalSceneFilename     = "lod\\media\\pixo_final.gr2";

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
void Update(granny_real32 const CurrentTime,
            granny_real32 const DeltaTime);


// ---------------------------------------------------------
// Scene instance objects
DemoScene GlobalScene;

/* DEFTUTORIAL EXPGROUP(TutorialLOD) (TutorialLOD_Introduction, Introduction) */
/* Animation LOD is a powerful tool for reducing the CPU load of your animation.
   Especially when you're rendering hundreds or even thousands of characters, every cycle
   counts, but even when you're rendering a few, why spend cycles on something that the
   user can't see?  We discuss the basic mechanisms available to you in $AnimationLod, so
   our goal here is simply to show you how to modify your application to take advantage of
   the feature.  As you'll see below, as few as 5-10 lines of code need to change from the
   sampling paths you already have in place.
   $-

   As always, we're building this example on the bones of our old friend
   $TutorialDX9Rendering, and eliding any code discussed in that tutorial.  If you haven't
   already worked through that example, it's recommended that you examine that code first.
   Once you're comfortable with the way bone matrices flow around between Granny and the
   rendering interface, it'll be easier to understand the changes below.
   $-

   Part of the tutorial is prepping the $animation LOD parameters.  Skeletal LOD is
   computed automatically for you by the exporter, but for determining the error values
   for individual $track_group elements, you need various bits of information that aren't
   necessarily available at export time.  Luckily, the Granny Preprocessor can take care
   of this for you fairly trivially.  In the "media" directory of this sample, there is a
   script: prep_scene.bat that takes the pixo_run.gr2 file from the common media
   directory, and adds the LOD parameters, as well as cleaning the mesh for display.
   There is some commentary on different ways to run the LOD calculation step, so make
   sure to check out that file in your tutorial directory.
   $-

   When you run the demo, you'll see 3 synchronized copys of the Pixomatic model, in his
   standard run cycle.  The center copy is rendered at essentially full detail.  The
   version on the right shows Pixo at a distance, with LODed animations.  The version on
   the left is Pixo rendered at the same size as the full detail version, but with the
   same LOD parameter as the distant copy, to show up close what the LOD system is doing.
   Use the "Page Up" and "Page Down" keys to move the distant model farther away from or
   toward the screen.
   $-

   Prerequisites to this demo:
   $* $TutorialDX9Rendering
   $-
*/


/* DEFTUTORIAL EXPGROUP(TutorialLOD) (TutorialLOD_RenderModel, RenderModel) */
/* The actual rendering calls in this function are no different than those of
   $TutorialDX9Rendering_RenderModel, so I've elided the commentary for those sections.
   The only difference here is that before sampling the $model_instance's animation, we're
   going to compute a LOD factor, and use that to control how many bones we actually work
   with.
   $-

   To compute the LOD parameter, the easiest thing to do is to use the supplied API,
   $FindAllowedLODError.  Because we don't want to assume that you're using a $camera,
   this takes the 3 salient parameters (Viewport height, vertical field of view, and
   distance from the camera) as arguments.  The first parameter is the error in screen
   space that you're willing to accept.  You can get essentially perfect reproduction with
   0.5 pixels of error, but you should dial this parameter until the results are
   acceptable.  The higher the error that you can get away with, the less work Granny will
   do in the sampling path.  We've chosen an estimated screen space error of 1 pixel for
   this demo.  Note that nothing requires you to use a constant error across the entire
   application.  One technique that you can use to squeeze out even more performance is to
   tie the screen error to the computed distance to the object.  So you might allow 0.5
   pixels of error until the model is 25 meters away, and then ramp up the allowed error
   to 2 pixels over the next 100 meters.
   $-

   In this example, we compute the LOD parameter for each model, but if you're concerned
   about the overhead, you can simply compute the LOD parameter for a distance of 1.0
   units, and then scale that factor by the actual distance to your object.  Since the
   projected size of world space distance in screen space scales linearly (more or less)
   with respect to distance from the camera, you can simply store off that Unit Error for
   the camera at the start of the frame, and reuse it until the view changes.
   $-

   The most efficient way to sample a $model_instance's animations is still
   $SampleModelAnimationsAccelerated.  This function has a "LOD enabled" version,
   $SampleModelAnimationsAcceleratedLOD, that simply takes the LOD parameter and does the
   computation for you.  That's it.  You now have a LOD enabled game.
   $-

   If you're not using the $SampleModelAnimationsAccelerated path, you have to do a tiny
   bit more work.  The version of the LOD sampling path that uses
   $SampleModelAnimationsLOD and $BuildWorldPoseLOD is laid out below as well in comment
   form.  It's important to note that while $SampleModelAnimationsLOD takes the truncated
   number of bones to sample, $BuildWorldPoseLOD needs to build a valid world pose for all
   of the $skeleton's bones, so you must pass the normal bone count.  (Unless you know
   from your mesh LOD system that those bones won't be used, but that's a topic for a
   different tutorial...)  $-

   One final note.  Because we want to draw the model several times in this demo, to
   demonstrate what the LODed animation looks like, I've separated the offset that we
   "pretend" the model is at (ComputeOffset) from the offset that we actually render the
   model at (RenderOffset).  To keep it simple, I've also passed the BufferHeight into
   this function, which should, of course, normally not be necessary in your own
   RenderModel style functions, but it lets us keep all of the LOD calls in one function
   for the purposes of the tutorial.
*/
void RenderModel(SimpleModel* Model, int BufferHeight,
                 float* RenderOffset, float* ComputeOffset)
{
    /* Compute the distance from the model to the camera, and the render offset matrix */
    float Dist = 0.0f;
    for (int i = 0; i < 3; i++)
        Dist += ((GlobalScene.DemoCamera.Offset[i] - ComputeOffset[i]) *
                 (GlobalScene.DemoCamera.Offset[i] - ComputeOffset[i]));
    Dist = sqrtf(Dist);

    granny_real32 OffsetMatrix[16];
    memcpy(OffsetMatrix, Model->InitialMatrix, sizeof(OffsetMatrix));
    for (int i = 0; i < 3; i++)
        OffsetMatrix[12 + i] += RenderOffset[i];

    /* Now compute the LODError parameter. */
    const float LODError =
        GrannyFindAllowedLODError(1.0f,                   // error in pixels
                                  BufferHeight,           // screen height
                                  GlobalScene.DemoCamera.FOVY, // fovy
                                  Dist);                  // distance

    /* Comment out the #define line to use the separated Sample/BuildWorldPose path.  The
       two calls are functionally equivalent. */
#define USE_ACCEL_SAMPLE
#ifdef USE_ACCEL_SAMPLE

    GrannySampleModelAnimationsAcceleratedLOD(Model->ModelInstance,
                                              GrannyGetSourceSkeleton(Model->ModelInstance)->BoneCount,
                                              OffsetMatrix,
                                              GlobalScene.SharedLocalPose,
                                              GlobalScene.SharedWorldPose,
                                              LODError);

#else

    granny_skeleton* GrannySkel = GrannyGetSourceSkeleton(Model->ModelInstance);
    const int NumBones          = GrannySkel->BoneCount;

    // Compute the number of bones to sample in the skeleton
    int SampledBones = GrannyGetBoneCountForLOD(GrannySkel, LODError);

    GrannySampleModelAnimationsLOD(Model->ModelInstance,
                                   0, SampledBones,
                                   GlobalScene.SharedLocalPose,
                                   LODError);

    GrannyBuildWorldPoseLOD(GrannySkel,

                            // NB: NOT SampledBones!
                            0, NumBones,

                            // These parameters tell the function which bones in the
                            // skeleton are safe to use.  Since we truncated the sampling,
                            // LODed bones must be replaced with their rest-pose
                            // equivalent.
                            0, SampledBones,

                            GlobalScene.SharedLocalPose,
                            OffsetMatrix, GlobalScene.SharedWorldPose);

#endif

    RenderSimpleModel(Model, GlobalScene.SharedWorldPose, g_pD3DDevice);

    // /* Iterate across all meshes, and render.  This is exactly the same as the DX9 sample,
    //    you can safely ignore everything else in this function. */
    // {for (size_t MeshIndex = 0; MeshIndex < Model->BoundMeshes.size(); ++MeshIndex)
    // {
    //     DemoMesh* Mesh = Model->BoundMeshes[MeshIndex];

    //     // Load the mesh's index buffer.
    //     g_pD3DDevice->SetIndices(Mesh->IndexBuffer);

    //     int const *ToBoneIndices =
    //         GrannyGetMeshBindingToBoneIndices(Mesh->MeshBinding);

    //     if (GrannyMeshIsRigid(Mesh->Mesh))
    //     {
    //         g_pEffect->SetTechnique(g_RigidTechnique);
    //         g_pD3DDevice->SetVertexDeclaration( g_pVertexDecl_Rigid );
    //         g_pD3DDevice->SetStreamSource(0, Mesh->VertexBuffer, 0,
    //                                       sizeof(granny_pnt332_vertex));

    //         __declspec(align(16)) granny_matrix_4x4 CompositeMatrix;
    //         GrannyBuildIndexedCompositeBuffer(
    //             GrannyGetMeshBindingToSkeleton(Mesh->MeshBinding),
    //             GlobalScene.SharedWorldPose,
    //             ToBoneIndices, 1,
    //             &CompositeMatrix);

    //         g_pEffect->SetMatrix(g_hMatObj2World, (D3DXMATRIX const*)CompositeMatrix);
    //     }
    //     else
    //     {
    //         int const NumMeshBones = GrannyGetMeshBindingBoneCount(Mesh->MeshBinding);
    //         assert(NumMeshBones <= 64);

    //         g_pEffect->SetTechnique(g_SkinnedTechnique);
    //         g_pD3DDevice->SetVertexDeclaration( g_pVertexDecl_Skinned );

    //         g_pD3DDevice->SetStreamSource(0, Mesh->VertexBuffer, 0,
    //                                       sizeof(granny_pwnt3432_vertex));

    //         __declspec(align(16)) granny_matrix_4x4 CompositeBuffer[64];

    //         GrannyBuildIndexedCompositeBuffer(
    //             GrannyGetMeshBindingToSkeleton(Mesh->MeshBinding),
    //             GlobalScene.SharedWorldPose,
    //             ToBoneIndices, NumMeshBones,
    //             CompositeBuffer);

    //         g_pEffect->SetMatrixArray(g_hSkinningMatrices,
    //                                   (D3DXMATRIX const*)CompositeBuffer,
    //                                   NumMeshBones);
    //     }

    //     int const VertexCount = GrannyGetMeshVertexCount(Mesh->Mesh);
    //     int GroupCount = GrannyGetMeshTriangleGroupCount(Mesh->Mesh);
    //     granny_tri_material_group *Group =
    //         GrannyGetMeshTriangleGroups(Mesh->Mesh);
    //     while(GroupCount--)
    //     {
    //         if(Group->MaterialIndex < int(Mesh->MaterialBindings.size()) &&
    //            Mesh->MaterialBindings[Group->MaterialIndex] != NULL)
    //         {
    //             DemoTexture* Texture = Mesh->MaterialBindings[Group->MaterialIndex];
    //             g_pEffect->SetTexture(g_hDiffuseTexture, Texture->TextureBuffer);
    //         }
    //         else
    //         {
    //             g_pEffect->SetTexture(g_hDiffuseTexture, NULL);
    //         }

    //         // We know that there's only one pass, but let's be general about this
    //         UINT numPasses;
    //         g_pEffect->Begin(&numPasses, 0);
    //         for (UINT i = 0; i < numPasses; i++)
    //         {
    //             g_pEffect->BeginPass(i);

    //             g_pD3DDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,
    //                                                0,
    //                                                0, VertexCount,
    //                                                3*Group->TriFirst,
    //                                                Group->TriCount);

    //             g_pEffect->EndPass();
    //         }
    //         g_pEffect->End();

    //         ++Group;
    //     }
    // }}
}


/* DEFTUTORIAL EXPGROUP(TutorialLOD) (TutorialLOD_Summary, Summary) */
/* Taking advantage of LOD is absolutely essential if you want to throw up the maximum
   number of characters on screen.  The technique shown here obviously won't help you with
   poly count for far away characters (we'll do that elsewhere), but every CPU cycle
   counts.  Make sure that you check out prep_scene.bat in the "media" directory, so
   you're familiar with the process of computing the animation LODs.
*/
/* DEFTUTORIALEND */


void Update(granny_real32 const CurrentTime,
            granny_real32 const) // DeltaTime
{
    for (size_t Idx = 0; Idx < GlobalScene.Models.size(); Idx++)
    {
        // Set the model clock
        SimpleModel* Model = GlobalScene.Models[Idx];
        GrannySetModelClock(Model->ModelInstance, CurrentTime);
    }
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


bool InitCameraAndLights()
{
    assert(GlobalScene.SceneFileInfo);

    GrannyInitializeDefaultCamera(&GlobalScene.DemoCamera);
    GlobalScene.DemoCamera.NearClipPlane = 50.f;
    GlobalScene.DemoCamera.FarClipPlane = 30000.f;  // this is fairly huge, but we'll be
                                                    // pushing the models well away from
                                                    // the camera
    GlobalScene.DemoCamera.FOVY = 0.2f * 3.14f;
    GlobalScene.DemoCamera.Offset[1] = 40;
    GlobalScene.DemoCamera.Offset[2] = 150;

    /* Setup our hardcoded light.  This points it in the same general direction as the camera, but offset slightly. */
    GlobalScene.DirFromLight[0]  = -0.8660f;
    GlobalScene.DirFromLight[1]  = 0.5f;
    GlobalScene.DirFromLight[2]  = 0;
    GlobalScene.LightColour[0]   = 1;
    GlobalScene.LightColour[1]   = 1;
    GlobalScene.LightColour[2]   = 1;
    GlobalScene.LightColour[3]   = 1;
    GlobalScene.AmbientColour[0] = 0.4f;
    GlobalScene.AmbientColour[1] = 0.4f;
    GlobalScene.AmbientColour[2] = 0.4f;
    GlobalScene.AmbientColour[3] = 0.4f;

    return true;
}


void Render(int Width, int Height)
{
    if (!GlobalRunning)
        return;
    assert(g_pD3DDevice);

    GrannySetCameraAspectRatios(&GlobalScene.DemoCamera,
                                GrannyGetMostLikelyPhysicalAspectRatio(Width, Height),
                                (float)Width, (float)Height,
                                (float)Width, (float)Height);
    GrannyBuildCameraMatrices(&GlobalScene.DemoCamera);

    g_pD3DDevice->Clear(0, NULL, D3DCLEAR_TARGET|D3DCLEAR_ZBUFFER|D3DCLEAR_STENCIL,
                        D3DCOLOR_XRGB(160,160,192), 1.0f, 0);

    if(SUCCEEDED(g_pD3DDevice->BeginScene()))
    {
        SimpleModelSetup(GlobalScene.DemoCamera,
                         GlobalScene.DirFromLight,
                         GlobalScene.LightColour,
                         GlobalScene.AmbientColour);

        g_pD3DDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);

        // Loop over the models, and render their meshes.  We do this three times,
        // to show the full animation, the LODed version at actual size, and what
        // it would look like if drawn at the same scale as the full version.
        float LeftOffset[3]   = { -40, 0, 0 };
        float CenterOffset[3] = { 0, 0, 0 };
        float RightOffset[3]  = { 40 + 75.0f*GlobalScene.LODNotch, 0, -500.0f*GlobalScene.LODNotch };

        float* Passes[3][2] = {
            { LeftOffset,   RightOffset  },
            { CenterOffset, CenterOffset },
            { RightOffset,  RightOffset  }
        };

        for (int i = 0; i < 3; i++)
        {
            for (size_t Idx = 0; Idx < GlobalScene.Models.size(); Idx++)
            {
                RenderModel(GlobalScene.Models[Idx], Height, Passes[i][0], Passes[i][1]);
            }
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
