// =======================================================================================
// track_mask.cpp: Demonstrates using a granny_track_mask to control rendered animations
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "track_mask.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>

using namespace std;

char const* const GlobalSceneFilename     = "media\\pixo_dance.gr2";

// ---------------------------------------------------------
// Scene instance objects
extern DemoScene GlobalScene;


/* DEFTUTORIAL EXPGROUP(TutorialTrackMask) (TutorialTrackMask_Introduction, Introduction) */
/* A $track_mask is nothing more than list of scalars that controls, at a per-bone level,
   the weight of an animation.  This is somewhat like the weight of a $control, but
   obviously much finer-grained.  Using a $track_mask allows you to restrict animations to
   one part of the $skeleton, either permanently, or temporarily.  For instance, you might
   want to turn off the effect of your "walk" animation on your character's right arm
   while playing a "wave" animation as well.

   Granny gives you several tools to construct $(track_mask)s for your models.  At the
   lowest level, you can simply create a new mask with $NewTrackMask, and set the values
   per-bone with $SetTrackMaskBoneWeight.  If you know that you want to affect either all
   bones that are children of a specific $bone, or all bones that are parents of a given
   $bone, you can use $SetSkeletonTrackMaskChainUpwards or
   $SetSkeletonTrackMaskChainDownwards.  We'll use these functions in this demo to
   construct a $track_mask that only activates the upper body.

   If you want to allow your artists to generate $(track_mask)s, Granny provides a simple
   UI for doing so in Max and Maya.  The weights are stored in the ExtendedData field of
   each $bone, where they can be retrieved with $ExtractTrackMask, or manually, if you
   want.

   When you need to playback an animation with a mask applied, you have two options.  You
   can use the $controlled_animation_builder to create the control with the $track_mask
   pre-attached, or you can attach the mask to an existing $control $SetControlModelMask
   or $SetControlTrackMask.  You might want to reprise $TutorialCreateControl for the
   pluses and minuses of $BeginControlledAnimation versus $PlayControlledAnimation.

   One terminology related item you should be aware of is that Granny understands two
   types of track masks, each of which are implemented as a $track_mask object.  The first
   is a "Model TrackMask", which is what you'll most commonly be dealing with.  The second
   is a "Track TrackMask".  (I agree that the names aren't the best.)  The only difference
   between the two is the ordering of the values in the $(track_mask)s weight array.  In a
   "Track TrackMask", the weights are ordered in the same way that $(transform_track)s are
   ordered in the relevant $track_group.  In a "Model TrackMask", the weights are in the
   same order as the $(bone)s in the $(model)'s $skeleton.  When you apply the $track_mask
   to the animation, you specify either $SetTrackGroupModelMask/$SetControlModelMask, or
   $SetTrackGroupTrackMask/$SetControlTrackMask.  The meaning of those should now be
   clear, but the naming convention is tricky, so watch out!

   The example below is simple the code from $TutorialDX9Rendering, altered so that
   everything not relating to creating and applying the $track_mask is moved out of the
   tutorial file.  You can find the implementation of the rendering, update, etc in
   track_mask_helpers.cpp, but I recommend you look at $TutorialDX9Rendering instead, if
   you need clarification.  The expository comments are more complete in that project.

   Prerequisites to this demo:
   $* $TutorialBasicLoading
   $* $TutorialCreateControl
   $* $TutorialDX9Rendering (Helpful, but not really required)
   $-

   New functions and structures discussed in this tutorial:
   $* $track_mask
   $* $SetTrackGroupModelMask
   $* $SetTrackGroupTrackMask
   $* $NewTrackMask
   $* $SetSkeletonTrackMaskChainUpwards
   $* $SetSkeletonTrackMaskChainDownwards
   $* $SetTrackMaskBoneWeight
   $* $ExtractTrackMask
   $-
*/

INT WINAPI WinMain( HINSTANCE, HINSTANCE, LPSTR, INT )
{
    /* Check the versions, and initialize the windows.  You can safely skip over this code
       it's the same as that in the DX9 Tutorial
    */
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
            CreateSimpleModels(g_pD3DDevice, GlobalScene.SceneFileInfo, GlobalScene.Textures, GlobalScene.Models, false) == false ||
            CreateSharedPoses() == false || InitCameraAndLights() == false)
        {
            return EXIT_FAILURE;
        }
    }

    /* Ok, this next chunk is the interesting stuff.  In the scene we're using, there are
       several models, but we're only going to work with the Pixomatic character.  We'll
       create a track mask that selects out just the upper body.  When we play back the
       animation, the moves for the upper body will be exactly the same as those in
       $TutorialDX9Rendering, but the legs will stay immobile.  Normally, you'd be playing
       a separate animation, or running an IK chain on the legs to blend with the upper
       body, but to demonstrate the process, we'll just leave them static.

       When you use a $track_mask, you're responsible for making sure that the object is
       valid for the lifetime of the control.  In this demo, we're just going to leak the
       reference, since we're not trying to demonstrate memory management, but in a real
       app, you need to make sure that you wait until after GrannyFreeControl to delete
       the $track_mask.  (Using $FreeTrackMask, of course!)
    */
    {for(granny_int32x ModelIdx = 0;
         ModelIdx < (granny_int32x)GlobalScene.Models.size();
         ++ModelIdx)
    {
        SimpleModel* Model = GlobalScene.Models[ModelIdx];

        if (_stricmp(GrannyGetSourceModel(Model->ModelInstance)->Name, "pixo:root") == 0)
        {
            {for(granny_int32x AnimIdx = 0; AnimIdx < GlobalScene.SceneFileInfo->AnimationCount; ++AnimIdx)
            {
                granny_animation* Animation = GlobalScene.SceneFileInfo->Animations[AnimIdx];

                // Find the correct index for the pixo trackgroup
                granny_int32x TrackGroupIndex;
                if (GrannyFindTrackGroupForModel(Animation,
                                                 GrannyGetSourceModel(Model->ModelInstance)->Name,
                                                 &TrackGroupIndex))
                {
                    granny_skeleton* Skeleton = GrannyGetSourceSkeleton(Model->ModelInstance);

                    /* Time to make the donuts.  The first parameter is the default weight
                       in the weight array.  Since we're trying to mask in the upper body,
                       we use 0.0 to make sure that everything else is cleared.
                    */
                    granny_track_mask* TrackMask = GrannyNewTrackMask(0.0f, Skeleton->BoneCount);

                    /* The Pixomatic model contains a bone called "pixo:upperBody" that
                       seperates the legs from the torso.  Find the index of that bone,
                       and set every child bone "south" of it in the $skeleton to 1.0 in
                       the $track_mask.  Note that the commented out Upwards call will
                       include all the bones between the upperBody bone and the root,
                       which will cause the characters root motion to be controlled by the
                       animation.  I want him to stand still for now, but you can
                       experiment with enabling different parts of his $skeleton.
                    */
                    granny_int32x UpperBody;
                    GrannyFindBoneByNameLowercase(Skeleton, "pixo:upperBody", &UpperBody);

                    GrannySetSkeletonTrackMaskChainDownwards(TrackMask, Skeleton, UpperBody, 1.0);
                    //GrannySetSkeletonTrackMaskChainUpwards(TrackMask, Skeleton, UpperBody, 1.0);

                    /* We will use the newer $SetControlModelMask API, since this allows
                       us to use the simpler $PlayControlledAnimation API.*/
                    granny_control* Control = GrannyPlayControlledAnimation(0, Animation,
                                                                            Model->ModelInstance);

                    // Here's the key call.  Because we setup the track mask in skeleton
                    // bone order, use the GrannySetControlModelMask function.  As noted
                    // above, the other option here, GrannySetControlTrackMask is for
                    // track masks ordered according to transform track order in a track
                    // group.
                    GrannySetControlModelMask(Control, Model->ModelInstance, TrackMask);

                    /* You can safely ignore the rest of the function.  We're just going
                       to stop looking at the models now that we've set up our control,
                       and enter the update loop. */
                    assert(Control != NULL);
                    // Loop forever, and stop looking for animations.
                    GrannySetControlLoopCount(Control, 0);
                    GrannyFreeControlOnceUnused(Control);
                    break;
                }
            }}
        }
    }}

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


/* DEFTUTORIAL EXPGROUP(TutorialTrackMask) (TutorialTrackMask_Summary, Summary) */
/* Well, if you've run the test app, you know that looked pretty strange.  Once you get
   more comfortable with the idea of playing animation only on certain parts of the model,
   you can try creating some effects that don't look odd.  In general, the playing only
   one animation with a $track_mask isn't all that useful, except, as in this case, to
   illustrate how to do it.

   Remember also that since Granny refers to the $track_mask by pointer, you can change
   the way the mask works dynamically by just setting the weights over time.  So we could
   drive the upper/lower split above with a sinusoidal function, or blend different parts
   of an animation in and out as other animations become active, etc.
*/

/* DEFTUTORIALEND */

