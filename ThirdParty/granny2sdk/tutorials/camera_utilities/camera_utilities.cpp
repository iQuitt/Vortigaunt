// =======================================================================================
// camera_utilities.cpp: Demonstrates using the Granny camera utility API
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "camera_utilities.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>

#define _USE_MATH_DEFINES
#include <math.h>

using namespace std;

char const* const GlobalSceneFilename     = "media/pixo_dance.gr2";

// ---------------------------------------------------------
// Scene instance objects
extern DemoScene GlobalScene;

// Horribly simple keydown handling.
const float CameraSpeed = 30.0f;
bool WKeyDown = false;
bool AKeyDown = false;
bool SKeyDown = false;
bool DKeyDown = false;

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


/* DEFTUTORIAL EXPGROUP(TutorialCameraUtils) (TutorialCameraUtils_Introduction, Introduction) */
/* Granny provides you with a utility API to help setup camera matrices, and update aim,
   position, and orientation.  In this tutorial, we'll walk through some of the basics,
   and describe the meaning of the (admittedly somewhat cryptic) members of the $camera
   structure.

   As a base, we'll start with the DX9 demo from ($TutorialDX9Rendering).  I've stripped
   out all of the non-camera related operations from this document as much as possible.
   If you're interested, you can find the gory details in the camera_utilities_helper.cpp
   file, though you'll be much better off examining them in the context of the DX9
   tutorial.  The only changed elements are that we've seperated the light initialization
   from the camera initialization so we can consider the $camera in isolation, and some
   very (!)  basic input handling code has been added so we can move our demo camera
   around.

   There is addition information on the camera utilities to be found at
   $UtilityOverview_Camera.  Think of this as a practical version of the material found
   there.

   Prerequisites to this demo:
   $* $TutorialDX9Rendering (Helpful, but not really required)
   $-

   New functions and structures discussed in this tutorial:
   $* $camera
   $* $BuildCameraMatrices
   $* $InitializeDefaultCamera
   $* $MoveCameraRelative
   $* $SetCameraAspectRatios
   $-

   Function index:
   $* $TutorialCameraUtils_InitCamera
   $* $TutorialCameraUtils_UpdateFrameMovement
   $* $TutorialCameraUtils_HandleMouseMove
   $* $TutorialCameraUtils_FinalizeCamera
   $-
*/


/* DEFTUTORIAL EXPGROUP(TutorialCameraUtils) (TutorialCameraUtils_InitCamera, InitCamera) */
/* If you've worked through the $TutorialExtendedData tutorial, this code will look very
   familiar to you.  We're doing the same thing here, extracting some parameters from the
   camera specification we find in our demo scene.  This time through though, instead of
   focusing on the extraction code, we'll talk about the parameters we're setting in the
   camera.

   Move along, nothing to see here.  Just finding the parameters we want.
*/
bool InitCamera()
{
    assert(GlobalScene.SceneFileInfo);
    granny_file_info* Info = GlobalScene.SceneFileInfo;

    granny_camera& Camera = GlobalScene.DemoCamera;

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

                /* Ok, now we can setup the camera.
                   $-

                   From our Maya scene, we've extracted the camera's base position, and
                   the near and far clip planes.  Before setting those parameters, it's a
                   good idea to call $InitializeDefaultCamera just to fill the camera with
                   sensible default values.
                */
                GrannyInitializeDefaultCamera(&Camera);

                /* These parameters don't need much explaining.  Near and far planes, just
                   as you'd expect.
                   $-

                   We're not going to mess with the other two clip plane related
                   parameters, $camera::ZRangeEpsilon and $camera::OutputZRange.  They are
                   covered adequately in the $UtilityOverview_Camera document section, and
                   they don't help us in our demo.
                 */
                Camera.NearClipPlane = CameraFields.nearClipPlane;
                Camera.FarClipPlane  = CameraFields.farClipPlane;

                /* Position is also fairly self explanitory. */
                memcpy(Camera.Position,
                       Info->Models[ModelIdx]->InitialPlacement.Position,
                       sizeof(Camera.Position));

                /* Orientation deserves a little more consideration, because $camera gives
                   you two ways to specify the orientation of the camera.  You can specify
                   the initial orientation of the camera either with a quaternion, setting
                   the $camera::Orientation field, and setting
                   $camera::UseQuaternionOrientation to true.  In this case, setting the
                   flag isn't strictly necessary, since $InitializeDefaultCamera defaults
                   to using quaternion orientation.

                   Equivalently, you can use the $camera::OrientationMatrix parameter, and
                   clear $camera::UseQuaternionOrientation, if you'd prefer to work with
                   3x3 matrix rotations instead.  Quaternion orientations are obviously
                   preferred if you're slaving the camera to a bone in a $local_pose
                   somewhere, but if you're pulling the orientation out of a physics
                   simulation, or a IK iteration on a $world_pose, it can be handy to
                   specify the $(camera)'s rotation in matrix form.
                */
                Camera.UseQuaternionOrientation = true;
                memcpy(Camera.Orientation,
                       Info->Models[ModelIdx]->InitialPlacement.Orientation,
                       sizeof(Camera.Orientation));


                /* Maya provides us with an FOV, but we're going to ignore it.  FOV for
                   the Granny camera is vertical, rather than horizontal.  For no
                   particular reason, we've chosen Pi/4, or 45 degrees as our FOV for the
                   demo.*/
                Camera.FOVY = float(M_PI / 4.0);

                /* That's it for now.  We'll leave the rest of $(camera)'s fields set to
                   the defaults until the app can gather user input, and determine the
                   final size of the rendered surface.
                */

                FoundCamera = true;
                break;
            }
        }
    }}

    return FoundCamera;
}


/* DEFTUTORIAL EXPGROUP(TutorialCameraUtils) (TutorialCameraUtils_UpdateFrameMovement, UpdateFrameMovement) */
/* Our demo has, well, there's no other way to describe it, really crappy keyboard
   handling.  However, it's plenty for our purposes.  We get 4 flags set by the input
   loop, that tell us which of the standard WASD keys are down.  From this, we can derive
   a Forward/Back, Left/Right velocity, and use that to update the $(camera)'s position.

   We're using $MoveCameraRelative here to translate our motion into camera-relative
   offsets.  The standard Granny camera axes describe a right-handed coordinate system,
   with:
   $* Positive Y: up
   $* Negative Z: forward
   $* Positive X: right
   $-

   When you pass XYZ offsets to $MoveCameraRelative, this moves the camera in the
   direction that the up/right/forward axes are current pointing.  If you want to offset
   the camera in absolute coordinates, you can simply make changes to $camera::Position
   directly.
*/
void UpdateFrameMovement(const float DeltaTime)
{
    const float MovementThisFrame = DeltaTime * CameraSpeed;

    // Note: because the NegZ axis is forward, we have to invert the way you'd normally
    // think about the 'W' or 'S' key's action.  Also, we don't have a key for moving the
    // camera up and down, but it should be clear how to add one.
    const float ForwardSpeed = (WKeyDown ? -1 : 0.0f) + (SKeyDown ? 1 : 0.0f);
    const float RightSpeed   = (AKeyDown ? -1 : 0.0f) + (DKeyDown ? 1 : 0.0f);

    GrannyMoveCameraRelative(&GlobalScene.DemoCamera,
                             MovementThisFrame * RightSpeed,
                             0.0f,
                             MovementThisFrame * ForwardSpeed);
}


/* DEFTUTORIAL EXPGROUP(TutorialCameraUtils) (TutorialCameraUtils_HandleMouseMove, HandleMouseMove) */
/* When the left button is depressed, we have a standard Azimuth/Elevation set of camera
   controls.  Personally, I always have a rough time remembering the interpretation of the
   action, the easiest way seems to be to think of the sequence of actions that yields the
   final camera transform.
   $* $camera::Position sets the base translation
   $* $camera::Orientation (or OrientationMatrix) sets the base orientation
   $* $camera::EAR: Elevation, Azimuth, Roll is applied to the orientation.  All of these
      rotations are rotations around one of the positive camera axes.
   $* Finally, $camera::Offset is applied.  This works exactly as if you had called
      $MoveCameraRelative, i.e, the camera is displaced along it's local up/right/forward
      axes.
   $-

   Our camera will only do an Azimuth/Elevation manipulation.  I've chosen the control
   orientation that feels right to me, in your game of course, you can invert these just
   by fiddling sign of the movement constant.
   $-

   Everytime we change the EAR variable, we'll rebuild the camera matrices.
   $MoveCameraRelative only works against orientations that have been "baked" into the
   camera matrices ($camera::View4x4, $camera::Projection4x4), so you should ensure that
   the matrices are up to date if you're not sure when translation events are going to
   occur.  (We actually know in this demo, but we'll pretend we don't.)

   After the call to $BuildCameraMatrices the following variables are valid:
   $* $camera::View4x4
   $* $camera::InverseView4x4
   $* $camera::Projection4x4
   $* $camera::InverseProjection4x4
   $-

   In most cases, you only care about the View and Projection matrices, but if you're
   building normal transformation matrices for objects with non-uniform scale or shear,
   they can come in handy.
*/
void HandleMouseMove(int deltaX, int deltaY)
{
    float DeltaAzim = -granny_real32(deltaX * M_PI/180.0);
    float DeltaElev = -granny_real32(deltaY * M_PI/180.0);

    GlobalScene.DemoCamera.ElevAzimRoll[0] += DeltaElev;
    GlobalScene.DemoCamera.ElevAzimRoll[1] += DeltaAzim;

    GrannyBuildCameraMatrices(&GlobalScene.DemoCamera);
}


/* DEFTUTORIAL EXPGROUP(TutorialCameraUtils) (TutorialCameraUtils_FinalizeCamera, FinalizeCamera) */
/* FinalizeCameraForFrame is called from the Render function, just before the camera
   matrices are downloaded to the hardware.  This gives us a change to finalize the
   matrices with one last $BuildCameraMatrices, to bake in any changes from
   UpdateFrameMovement.

   We're also encountering our last undescribed variables in the $camera structure,
   $camera::WpOverHp, $camera::WrOverHr, and $camera::WwOverHw.  Catchy names, right?  Now
   that you've seen them, you can promptly forget them entirely, since you shouldn't touch
   those variables by hand.  They essentially control the various screen width to height
   ratios for the backbuffer, the current window into the back buffer, and the window the
   back buffer will eventually be shown to.  You should use $SetCameraAspectRatios to set
   these variables.

   We set the camera aspect ratios every frame, because the width and height of the window
   may have changed which affects the aspect ratio correction.  However, if you're a full
   screen game and you know when you're changing screen modes and such, then you'd only
   have to call $SetCameraAspectRatios when you actually change modes.

   Unless you're on a console, it can be fairly hard to determine what the physical aspect
   ratio of your device is.  For instance, your back buffer may be 1024x768, but your user
   is displaying your pretty graphics on a 16:10 aspect ratio laptop, stretching them out
   horribly.  Sigh.  Getting device aspect ratios is beyond the scope of this tutorial,
   but if you don't want to mess with the problem, you can just use Granny's best guess,
   which you can get from $GetMostLikelyPhysicalAspectRatio.  You won't be guarded from
   that 16:10 problem, but at least you won't spend much time thinking about it.  The
   supported guesses can be found in granny.h, just search for "AspectRatio", they're all
   clustered together.
*/
void FinalizeCameraForFrame(int Width, int Height)
{
    GrannySetCameraAspectRatios(&GlobalScene.DemoCamera,
                                GrannyGetMostLikelyPhysicalAspectRatio(Width, Height),
                                (granny_real32)Width, (granny_real32)Height,
                                (granny_real32)Width, (granny_real32)Height);

    // Make sure the matrices are up to date before rendering.
    GrannyBuildCameraMatrices(&GlobalScene.DemoCamera);
}



/* DEFTUTORIALEND */

INT WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR, INT )
{
    /* Check the versions, initialize the windows, and enter the update loop.  You can
       safely skip over this code it's the same as that in the DX9 Tutorial
    */
    if(!GrannyVersionsMatch)
    {
        printf("Warning: the Granny DLL currently loaded "
               "doesn't match the .h file used during compilation\n");
        return EXIT_FAILURE;
    }

    if (InitializeD3D() == false)
        return EXIT_FAILURE;

    if ((GlobalScene.SceneFile = GrannyReadEntireFile(GlobalSceneFilename)) == NULL ||
        (GlobalScene.SceneFileInfo = GrannyGetFileInfo(GlobalScene.SceneFile)) == NULL)
    {
        return EXIT_FAILURE;
    }

    if (CreateSimpleTextures(g_pD3DDevice, GlobalScene.SceneFileInfo, GlobalScene.Textures) == false ||
        CreateSimpleModels(g_pD3DDevice, GlobalScene.SceneFileInfo, GlobalScene.Textures, GlobalScene.Models)   == false ||
        CreateSharedPoses() == false || InitCamera() == false ||
        InitLights() == false)
    {
        return EXIT_FAILURE;
    }

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


