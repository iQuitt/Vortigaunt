// =======================================================================================
// blending.cpp: Demonstrates using control weights to blend animations
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "blending.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

using namespace std;

char const* const GlobalSceneFilename = "media\\pixo_run_walk.gr2";

// ---------------------------------------------------------
// Scene instance objects
extern DemoScene GlobalScene;

float Clamp(float const Input, float const Min, float const Max)
{
    assert(Min <= Max);

    if (Input < Min)
        return Min;
    else if (Input > Max)
        return Max;

    return Input;
}

/* DEFTUTORIAL EXPGROUP(TutorialBlending) (TutorialBlending_Introduction, Introduction) */
/* An application that plays only one animation on a model at all times isn't likely to be
   particularly interesting.  To smoothly move between attacks, movements, and poses, you
   need to be able to sample multiple animations simultaneously and blend the results into
   a realistic combined pose.  In this tutorial, we'll be showing you the most basic
   blending primitive built into Granny, $SetControlWeight, which will give you a grasp of
   how intermediate animation results are combined to yield a final $local_pose that
   represents the current animation state.

   Our test scene, like most of the others, is a Pixomatic model.  I've used the
   preprocessor to combine the model, mesh, and two movement cycles (walk_slow and
   run_fast).  Since we need to manipulate the $control structures we get back from
   $PlayControlledAnimation, our initialization code will be slightly more complicated
   than the "fire-and-forget" setup we've used in previous tutorials, but as you'll see,
   it's not terribly different, or more difficult.

   Prerequisites to this demo:
   $* $TutorialBasicLoading
   $* $TutorialCreateControl
   $* $TutorialDX9Rendering (Helpful, but not really required)
   $-

   New functions and structures discussed in this tutorial:
   $* $SetControlWeight
   $-
*/

INT WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR, INT )
{
    /* Check the versions, and initialize the windows.  You can safely skip over this code
       it's the same as that in the DX9 Tutorial */
    {
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
            CreateSimpleModels(g_pD3DDevice, GlobalScene.SceneFileInfo, GlobalScene.Textures, GlobalScene.Models, false)   == false ||
            CreateSharedPoses() == false  || InitCameraAndLights() == false)
        {
            return EXIT_FAILURE;
        }
    }

    /* Create our two controls.  We're going to make the assumption that there's only one
       model in the file, and two animations.  By default, $(control)s start with a weight
       of 1.0, which we'll leave in place for now.  We'll mess with it later. */
    granny_control* Controls[2] = { NULL, NULL };
    assert(GlobalScene.Models.size() == 1);
    assert(GlobalScene.SceneFileInfo->AnimationCount == 2);
    {
        SimpleModel* Model = GlobalScene.Models[0];

        {for(granny_int32x AnimIdx = 0;
             AnimIdx < GlobalScene.SceneFileInfo->AnimationCount;
             ++AnimIdx)
        {
            granny_animation* Animation = GlobalScene.SceneFileInfo->Animations[AnimIdx];

            Controls[AnimIdx] =
                GrannyPlayControlledAnimation(0.0f, Animation, Model->ModelInstance);

            assert(Controls[AnimIdx] != NULL);
            GrannySetControlLoopCount(Controls[AnimIdx], 0);
            GrannyFreeControlOnceUnused(Controls[AnimIdx]);
        }}
    }

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
        assert(Controls[0] && Controls[1]);

        /* Ok, so this is the important bit.  In our demo, (if you haven't messed with the
           contents of combined.gr2) there are two animations, walking, and running.  In
           your own application, of course, knowing which is which is quite important, but
           for our purposes, we dont' really care, we just want to see a transition
           between the two.  We're going to pick the simplest thing we can, a sin curve
           (shifted to the range [0, 1], and slightly clamped to give some time at 100%
           for both anims) that controls the weight of the first control, with the second
           control adjusted to make the total weight of both controls sum to 1.0.  $-

           Note that the normalization that we do to 1.0 here is so the appearance moves
           between walking and running.  There are no non-negative values that you can set
           here that are invalid, Granny will always correctly normalize the total weights
           applied on a per-bone basis.
           $-
        */
        float SineWeight = Clamp(float(sin(CurrentTime) * 0.65) + 0.5f, 0.0f, 1.0f);

        GrannySetControlWeight(Controls[0], SineWeight);
        GrannySetControlWeight(Controls[1], 1.0f - SineWeight);

        /* That's actually it, as far as our demo is concerned.  The rest of the blending
           machinery is subsumed in the call to $SampleModelAnimations, which is
           uninteresting, for our purposes here.  Hardly seems worth setting up a whole
           tutorial just for 3 lines of code, right?  The real idea is to give you a
           ready-made platform to play around with the blending controls, without having
           to go through all the messy setup.  You should definitely get into this
           tutorial in Visual Studio and mess with the blending logic yourself.

           The rest of this is boilerplate code to update the world state, and dispatch
           Win32 messages.  Nothing to see here, move along. */
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


/* DEFTUTORIAL EXPGROUP(TutorialBlending) (TutorialBlending_Summary, Summary) */
/* A two-control blend is the simplest possible way to combine animations in Granny.
   You'll have noticed, in running the application, that the appearance could be improved
   considerably by talking into account the properties of the animations themselves.
   (Foot down events, total time scale, gaits, etc.)  However, once you have this simple
   technique under your belt you can move on to more advanced blends: time sensitive gait
   blending, blends of $(control)s that are derived from pose targets, constructing
   intermediate results on disparate $(model_instance)s to combine them with
   $ModulationCompositeLocalPose, or whatever other animation combining operation you need
   for your game.

   Also, don't forget that manually constructing these blends gives you maximum
   flexibility (and potential efficiency), but costs you time spent working out and
   debugging your blend routines.  Check out the new Granny Animation Studio for a
   way to graphically assemble your characters!
*/

/* DEFTUTORIALEND */

