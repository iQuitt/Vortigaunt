// =======================================================================================
// morph_targets.cpp: Demonstrates using morph targets to blend meshes
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "morph_targets.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

using namespace std;

// Uncomment this #define to switch between the Maya and Max example scenes
//#define USE_MAX_EXAMPLE

#ifdef USE_MAX_EXAMPLE
char const* const GlobalSceneFilename = "morph_targets\\media\\morpher_test.gr2";  // max morpher example
#else
char const* const GlobalSceneFilename = "morph_targets\\media\\blendshape.gr2";  // maya blendshape example
#endif


static IDirect3DVertexBuffer9*
CreateVBFromVertexData(granny_data_type_definition const* VertexFormat,
                       granny_int32x const BufferSize,
                       granny_vertex_data const* VertexData);
static bool DoBasicInitialization();
static bool DoUninterestingBindingBits(DemoMesh* NewMesh,
                                       granny_mesh* GrannyMesh,
                                       granny_model* SourceModel);

// ---------------------------------------------------------
// Scene instance objects
extern DemoScene GlobalScene;


/* DEFTUTORIAL EXPGROUP(TutorialMorphTargets) (TutorialMorphTargets_Introduction, Introduction) */
/* Starting with the 2.7.0.15 release of Granny, we've added more detailed support for
   mesh morphing animations.  The Morpher modifier of Max, and the BlendShape modifier in
   Maya will now export animations separate from the skeletal track that you can easily
   sample to reproduce the effects in your game.  In addition, you're no longer restricted
   to doing frame-by-frame sampling of your morphs, the exporter can now read the links
   stored in the morph modifiers to extract the meshes directly.  There's more information
   on the exporter interface to this functionality in $MorphTargets.

   In this tutorial, we'll be taking an example file with a very simple morph animation
   specified in Max with the Morpher modifier, and showing you how to sample the run it
   through a vertex shader.  You can find the source art for the model and animation we're
   using in (morph_targets/media/morpher test.max).

   Like most of the other tutorials that render bits, this is based on a version of the
   DX9 tutorial.  As always, if you need information on how to runder in Direct X with
   Granny, you're better off examining the code in that tutorial instead of here.  You
   should be noticing that in the more advanced tutorials, we're explaining those bits
   less and less.
   $-

   In particular, we're shaving off huge sections of code from the extracted tutorial that
   are unchanged from the actual rendering tutorial.  In the case of this morph tutorial,
   the only interesting bits of code are the RenderModel function, where we set up the
   shader parameters, and the mesh initialization, where we find and setup vertex buffers
   for our morph targets.

   Prerequisites to this demo:
   $* $TutorialBasicLoading
   $* $TutorialDX9Rendering
   $-

   New functions and structures discussed in this tutorial:
   $* $vector_track
   $* $EvaluateCurveAtT
   $-
*/

/* DEFTUTORIAL EXPGROUP(TutorialMorphTargets) (TutorialMorphTargets_CreateBoundMesh, CreateBoundMesh) */
/* When you create the hardware resources (index and vertex buffers) for morph targets,
   obviously you have a little extra work to do.  In our case, we're going to take the
   morph vertices, and download only the positional deltas.  I'm doing this in an
   extremely unfancy way, you could theoretically profitably combine everything into one
   vertex buffer, or play other sorts of memory tricks, but as always, we're going for
   simplicity here.
*/
DemoMesh*
CreateBoundMesh(granny_mesh* GrannyMesh,
                granny_model_instance* ModelInstance,
                granny_file_info* GrannyFile)
{
    assert(GrannyMesh);
    assert(GrannyMeshIsRigid(GrannyMesh));
    assert(ModelInstance);
    assert(g_pD3DDevice);
    granny_model* SourceModel = GrannyGetSourceModel(ModelInstance);


    /* Just what it says.  These are the boring bits that get in the way of the point I'm
       trying to make.  You can find them at the bottom of the tutorial source code if you
       really want to look at them.
    */
    DemoMesh* NewMesh = new DemoMesh;
    if (DoUninterestingBindingBits(NewMesh, GrannyMesh, SourceModel) == false)
    {
        delete NewMesh;
        return NULL;
    }

    /* Create the vertex buffer for the base data.  This should look very familiar.  Note
       that since we'll be doing a little extra copying, we've pulled out the call to
       $ConvertVertexLayouts into it's own helper function.
    */
    int const VertexBufferSize = (sizeof(granny_pnt332_vertex) *
                                  GrannyGetMeshVertexCount(GrannyMesh));

    NewMesh->VertexBuffer = CreateVBFromVertexData(GrannyPNT332VertexType,
                                                   VertexBufferSize,
                                                   GrannyMesh->PrimaryVertexData);

    /* Create the morph target buffers.  Obviously very similar, but notice that the type
       is different.  Where the base mesh contains both normals and textures, we're using
       the $PN33VertexType specification here to shear off the texture coordinates.
    */
    int const MorphBufferSize = (sizeof(granny_pn33_vertex) *
                                 GrannyGetMeshVertexCount(GrannyMesh));
    {for(granny_int32x MorphIdx = 0;
         MorphIdx < GrannyMesh->MorphTargetCount;
         ++MorphIdx)
    {
        NewMesh->MorphBuffers.push_back(
            CreateVBFromVertexData(GrannyPN33VertexType,
                                   MorphBufferSize,
                                   GrannyMesh->MorphTargets[MorphIdx].VertexData));
    }}
    NewMesh->MorphTracks.resize(NewMesh->MorphBuffers.size(), NULL);


    /* When the Granny exporter encounters a Morpher modifier, it creates a separate
       $animation entry in the $file_info::Animations array with a name equal to that of
       the mesh that contains the morph targets.

       This is the first time in the tutorial series that we've run into the need to use
       the $vector_track entries in a $track_group, so it's worth talking about them
       briefly.  In this case, the $vector_track object is encoding the animated morph
       values from the modifier.  A $vector_track can encode a curve of (nearly) arbitrary
       dimension, though here, it's obviously a 1-dimensional value.  It's essentially the
       same as a $transform_track, but without the implied transformation grouping.  Both
       $(vector_track)s and $(transform_track)s share the same underlying $curve2
       representation, and are sampled using the same high-speed core.

       The only major difference is that while Granny contains a lot of machinery to
       handle sampling $(transform_track)s en masse ($SampleModelAnimations for instance),
       $(vector_track)s are considered something that you'll want to be intimately
       involved with.  You'll need to acquaint yourself with the low-level $curve2
       sampling functions $EvaluateCurveAtT and the like.

       So let's go grab the $vector_track objects that have encoded the time series of
       morph percentages that we specified in Max.  For a Morpher animation, there will be
       just one $track_group with the name "MorphCurves", which will contain only
       $vector_track objects that have names that align with the names of the
       $morph_target meshes.  It's important to match these up, because while the morph
       targets are simply placed in the mesh in the order that they're encountered,
       $track_group elements are sorted by name.  So unless you name everything Mesh01,
       Mesh02, etc., there will be disagreement over ordering.  In this case, since we
       know that the memory is never going away, we'll just pull out the pointer to the
       appropriate $vector_track.

       Note that there is a subtlety here in that $FindVectorTrack requires a TrackKey
       that helps disambiguate the track in the case that multiple similar
       $(vector_track)s are present on different $(bone)s in a $skeleton.  Here, there is
       no chance of ambiguity, since there is one $track_group per morphable $mesh.  So we
       pass 0 as the TrackKey.  This is not obvious or ideal, but there is less chance of
       confusion that allowing a GrannyFindVectorTrackByName to return an incorrect
       $vector_track in the $skeleton case.
    */
    {for(granny_int32x Idx = 0; Idx < GrannyFile->AnimationCount; ++Idx)
    {
        granny_animation* Animation = GrannyFile->Animations[Idx];
        if (_stricmp(Animation->Name, GrannyMesh->Name) == 0)
        {
            assert(Animation->TrackGroupCount == 1);
            assert(_stricmp(Animation->TrackGroups[0]->Name, "MorphCurves") == 0);

            granny_track_group* TrackGroup = Animation->TrackGroups[0];
            assert(TrackGroup->TransformTrackCount == 0);
            assert(TrackGroup->VectorTrackCount == GrannyMesh->MorphTargetCount);

            {for(granny_int32x MeshIdx = 0;
                 MeshIdx < GrannyMesh->MorphTargetCount;
                 ++MeshIdx)
            {
                granny_int32x TrackIdx;
                if (GrannyFindVectorTrack(TrackGroup,
                                          GrannyMesh->MorphTargets[MeshIdx].ScalarName, 0,
                                          &TrackIdx))
                {
                    assert(MeshIdx < granny_int32x(NewMesh->MorphTracks.size()));
                    assert(_stricmp(TrackGroup->VectorTracks[TrackIdx].Name,
                                   GrannyMesh->MorphTargets[MeshIdx].ScalarName) == 0);

                    NewMesh->MorphTracks[MeshIdx] = &TrackGroup->VectorTracks[TrackIdx];
                }
                else
                {
                    assert(!"Missing vector track for a morph target!");
                }
            }}

            NewMesh->MorphAnim = Animation->TrackGroups[0];
            NewMesh->AnimDuration = Animation->Duration;
            break;
        }
    }}

    return NewMesh;
}


/* DEFTUTORIAL EXPGROUP(TutorialMorphTargets) (TutorialMorphTargets_RenderModel, RenderModel) */
void RenderModel(DemoModel* Model)
{
    g_pD3DDevice->SetPixelShader( g_pPixelShader );
    {for (size_t MeshIndex = 0; MeshIndex < Model->BoundMeshes.size(); ++MeshIndex)
    {
        DemoMesh* Mesh = Model->BoundMeshes[MeshIndex];
        assert(Mesh->MorphBuffers.size() == 3); // Ensure that we've got the art we expect...

        /* Setup the shader, and the data sources for the render.  I'm exploiting the fact
           that I know the mesh in the sample application is rigid to simplify the setup.
           In this context "rigid" is a bit of a misnomer, just read that as: has no
           indexed transform animations.
           $-

           The code that describes the vertex declaration isn't contained in the extracted
           tutorial document, but if you examine morph_targets_helpers.cpp, you can see
           that we've setup the base mesh verts on stream 0, and the positional deltas for
           the morph targets on streams 1, 2, and 3.  As you saw in CreateBoundMesh, we've
           sliced off the texture coordinate component of the morph targets.  Granny
           encodes normal deltas, UV deltas, or whatever else you might have changed in
           the editing tool, but for tutorial purposes, we're going to keep it simple.
        */
        g_pD3DDevice->SetVertexShader( g_pVertexShader_Morph );
        g_pD3DDevice->SetVertexDeclaration( g_pVertexDecl_Morph );

        g_pD3DDevice->SetIndices(Mesh->IndexBuffer);
        g_pD3DDevice->SetStreamSource(0, Mesh->VertexBuffer, 0,
                                      sizeof(granny_pnt332_vertex));
        {for(granny_uint32 Idx = 0; Idx < Mesh->MorphBuffers.size(); ++Idx)
        {
            g_pD3DDevice->SetStreamSource(Idx + 1,
                                          Mesh->MorphBuffers[Idx], 0,
                                          sizeof(granny_pn33_vertex));
        }}

        SetTransposedMatrix4x4(g_pD3DDevice,
                               MATRIX_CONSTANT_OBJ2WORLD,
                               Model->InitialMatrix);

        /* Now sample the morph constants for this mesh.  Doing it in the RenderModel
           function is slightly wierd, but it makes the tutorial read a little better.
           You could do this in your Update() function, and it would make more sense.  See
           the $TutorialMorphTargets_CreateBoundMesh function for a discussion of where
           and how the morph curves are stored.
           $-

           I'm using $EvaluateCurveAtT here to sample the morph animation curves that we
           found in CreateBoundMesh.  You can create a more efficient sampling routine
           here by tracking the current knot (using $FindKnot and $FindCloseKnot), and
           then calling $EvaluateCurveAtKnotIndex instead.  In most cases, using the basic
           curve evaluator is fine.
           $-

           I'm sampling all of the constants, since I know that the test scene that we're
           using contains only a base mesh and 3 morph targets.  In a real application,
           you might have to sample more morph targets than will fit (or perform well) in
           your shaders, and then select the morph weights and meshes that had the
           greatest current value.
           $-
        */
        static granny_real32 Id = 0.0f;
        granny_real32 MorphConstants[4] = { 0, 0, 0, 0 };
        {for(granny_uint32x Idx = 0; Idx < Mesh->MorphBuffers.size(); ++Idx)
        {
            granny_curve2 const* Curve = &Mesh->MorphTracks[Idx]->ValueCurve;
            GrannyEvaluateCurveAtT(1, false,
                                   true,
                                   Curve,
                                   true,
                                   Mesh->AnimDuration,
                                   fmod(GlobalScene.CurrentTime, Mesh->AnimDuration),
                                   &MorphConstants[Idx],
                                   &Id);

            // The curves from the Morpher modifier are set on the range [0, 100], rather
            // than [0, 1].  We could account for that in the shader, but it's just as
            // easy to do it here.  If you're using the Maya test file, you should remove
            // this line, the Maya blendshape normalizes to 1.0 by default.
#ifdef USE_MAX_EXAMPLE
            MorphConstants[Idx] *= 0.01f;
#endif

            // Curve compression error can cause this to go outside the [0,1] range that
            // we want for the shader.  Clamp the value to make sure nothing weird slips
            // through.
            if (MorphConstants[Idx] < 0)
                MorphConstants[Idx] = 0;
            else if (MorphConstants[Idx] > 1)
                MorphConstants[Idx] = 1;
        }}

        /* Send the sampled values to the hardware */
        g_pD3DDevice->SetVertexShaderConstantF(MORPH_CONSTANTS, MorphConstants, 1);


        /* All over but the shouting, now. */
        int const VertexCount = GrannyGetMeshVertexCount(Mesh->Mesh);
        int GroupCount = GrannyGetMeshTriangleGroupCount(Mesh->Mesh);
        granny_tri_material_group *Group =
            GrannyGetMeshTriangleGroups(Mesh->Mesh);
        while(GroupCount--)
        {
            g_pD3DDevice->DrawIndexedPrimitive(D3DPT_TRIANGLELIST,
                                               0,
                                               0, VertexCount,
                                               3*Group->TriFirst,
                                               Group->TriCount);
            ++Group;
        }
    }}
}




/* DEFTUTORIAL EXPGROUP(TutorialMorphTargets) (TutorialMorphTargets_Summary, Summary) */
/* As always, we've stripped this down to the absolute simplest possible example of the
   concept.  More exotic morph animations are possible, of course, as well as other ways
   you can control the percentages of the blend.  Skinned morph targets, where the
   deformations are applied to a mesh that is itself skeletally animated are the obvious
   next step.
 */

/* DEFTUTORIALEND */

bool InitCameraAndLights()
{
    assert(GlobalScene.SceneFileInfo);
#ifdef USE_MAX_EXAMPLE
    bool FoundCamera = true;

    GrannyInitializeDefaultCamera(&GlobalScene.DemoCamera);
    GlobalScene.DemoCamera.NearClipPlane = 0.1f;
    GlobalScene.DemoCamera.FarClipPlane  = 1000.0f;
    GlobalScene.DemoCamera.FOVY = 0.2f * 3.14f;
    GlobalScene.DemoCamera.Offset[0] = 25;
    GlobalScene.DemoCamera.Offset[1] = 25;
    GlobalScene.DemoCamera.Offset[2] = 200;
    GlobalScene.DemoCamera.ElevAzimRoll[0] = 1;

#else
    granny_file_info* Info = GlobalScene.SceneFileInfo;

    bool FoundCamera = false;
    {for(granny_int32x ModelIdx = 0; ModelIdx < Info->ModelCount; ++ModelIdx)
    {
        granny_model* Model = Info->Models[ModelIdx];
        {for(granny_int32x BoneIdx = 0; BoneIdx < Model->Skeleton->BoneCount; ++BoneIdx)
        {
            if (_stricmp("persp", Model->Skeleton->Bones[BoneIdx].Name) == 0)
            {
                granny_variant CameraVariant;
                if (GrannyFindMatchingMember(Model->Skeleton->Bones[BoneIdx].ExtendedData.Type,
                                             Model->Skeleton->Bones[BoneIdx].ExtendedData.Object,
                                             "CameraInfo",
                                             &CameraVariant))
                {
                    // This should be a reference to the data
                    assert(CameraVariant.Type[0].Type == GrannyReferenceMember);

                    // Ok, now we can setup the camera...
                    GrannyInitializeDefaultCamera(&GlobalScene.DemoCamera);
                    GlobalScene.DemoCamera.NearClipPlane = 0.1f;
                    GlobalScene.DemoCamera.FarClipPlane  = 1000.0f;
                    GlobalScene.DemoCamera.FOVY = 0.2f * 3.14f;

                    memcpy(GlobalScene.DemoCamera.Orientation,
                           Model->InitialPlacement.Orientation,
                           sizeof(GlobalScene.DemoCamera.Orientation));
                    memcpy(GlobalScene.DemoCamera.Position,
                           Model->InitialPlacement.Position,
                           sizeof(GlobalScene.DemoCamera.Position));

                    FoundCamera = true;
                    break;
                }
            }
        }}
    }}

#endif // USE_MAX_EXAMPLE

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




INT WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR, INT )
{
    if (DoBasicInitialization() == false)
        return EXIT_FAILURE;

    /* Enter our "game" loop.  This can be mostly ignored if you've already seen this loop in $TutorialDX9Rendering. */
    granny_system_clock StartClock;
    GrannyGetSystemSeconds(&StartClock);
    granny_system_clock LastClock = StartClock;

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



static IDirect3DVertexBuffer9*
CreateVBFromVertexData(granny_data_type_definition const* VertexFormat,
                       granny_int32x const BufferSize,
                       granny_vertex_data const* VertexData)
{
    IDirect3DVertexBuffer9* RetVal;

    g_pD3DDevice->CreateVertexBuffer(BufferSize,
                                     D3DUSAGE_WRITEONLY, 0, D3DPOOL_MANAGED,
                                     &RetVal, NULL);
    if (RetVal == 0)
        return NULL;

    BYTE *Vertices;
    RetVal->Lock(0, BufferSize, (void**)&Vertices, 0);

    GrannyConvertVertexLayouts(VertexData->VertexCount,
                               VertexData->VertexType,
                               VertexData->Vertices,
                               VertexFormat, Vertices);
    RetVal->Unlock();

    return RetVal;
}

static bool DoUninterestingBindingBits(DemoMesh* NewMesh,
                                       granny_mesh* GrannyMesh,
                                       granny_model* SourceModel)
{
    NewMesh->Mesh = GrannyMesh;
    NewMesh->MeshBinding = GrannyNewMeshBinding(GrannyMesh,
                                                SourceModel->Skeleton,
                                                SourceModel->Skeleton);

    {for(granny_int32x MatIdx = 0; MatIdx < GrannyMesh->MaterialBindingCount; ++MatIdx)
    {
        granny_material* Material = GrannyMesh->MaterialBindings[MatIdx].Material;
        if (Material == NULL)
            continue;

        DemoTexture* Found = NULL;
        if (Material->MapCount >= 1)
        {
            {for(granny_int32x MapIdx = 0; MapIdx < Material->MapCount; ++MapIdx)
            {
                granny_material_map& Map = Material->Maps[MapIdx];

                if (_stricmp(Map.Usage, "color") == 0 && Map.Material->Texture)
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
        return false;
    }

    BYTE *Indices;
    NewMesh->IndexBuffer->Lock(0, IndexBufferSize, (void**)&Indices, 0);
    memcpy(Indices, GrannyGetMeshIndices(GrannyMesh), IndexBufferSize);
    NewMesh->IndexBuffer->Unlock();

    return true;
}


static bool
DoBasicInitialization()
{
    if(!GrannyVersionsMatch)
    {
        printf("Warning: the Granny DLL currently loaded "
               "doesn't match the .h file used during compilation\n");
        return false;
    }

    if (InitializeD3D() == false)
        return false;

    if ((GlobalScene.SceneFile = GrannyReadEntireFile(GlobalSceneFilename)) == NULL ||
        (GlobalScene.SceneFileInfo = GrannyGetFileInfo(GlobalScene.SceneFile)) == NULL)
    {
        return false;
    }

    if (CreateDemoTextures() == false || CreateDemoModels() == false ||
        CreateSharedPoses() == false  || InitCameraAndLights() == false)
    {
        return false;
    }

    return true;
}
