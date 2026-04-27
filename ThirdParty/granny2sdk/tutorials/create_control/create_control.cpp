// =======================================================================================
// create_control.cpp: Demonstrates binding an animation to a model
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "granny.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


bool GetTestObjects(granny_file** GrannyFile,
                    granny_animation** Animation,
                    granny_model** Model);

/* DEFTUTORIAL EXPGROUP(TutorialCreateControl) (TutorialCreateControl_Introduction, Introduction) */
/* This tutorial shows you how to bind an animation to a Granny model, and sample the
   running animations.
   $-

   Prerequisites to this demo:
   $* $TutorialBasicLoading
   $-

   New functions and structures discussed in this tutorial:
   $* $control
   $* $model
   $* $model_instance
   $* $animation
   $* $controlled_animation_builder
   $* $local_pose
   $* $world_pose
   $* $InstantiateModel
   $* $FreeModelInstance
   $* $PlayControlledAnimation
   $* $FindTrackGroupForModel
   $* $BeginControlledAnimation
   $* $SetModelClock
   $* $SampleModelAnimations
   $* $BuildWorldPose
   $-

   Functions and structures introduced incidentally:
   $* $NewLocalPose
   $* $NewWorldPose
   $-

   Function index:
   $* $TutorialCreateControl_main
   $-
*/


/* DEFTUTORIAL EXPGROUP(TutorialCreateControl) (TutorialCreateControl_main, main) */
int main(int , char const*[])
{
    // Check the version
    if(!GrannyVersionsMatch)
    {
        printf("Warning: the Granny DLL currently loaded "
               "doesn't match the .h file used during compilation\n");
        return EXIT_FAILURE;
    }

    /* We're going to assume that you've already retrieved the $animation and the $model
       pointers from the $file_info to simplify this demo.  (Check the demo source for the
       details of how we obtain these pointers.) */
    granny_file* GrannyFile;
    granny_animation* Animation;
    granny_model* Model;
    if (GetTestObjects(&GrannyFile, &Animation, &Model) == false)
    {
        printf("Error: Couldn't obtain test objects\n");
        return EXIT_FAILURE;
    }

    /* A $model contains the data about your character that doesn't change.  The
       $skeleton, information about the meshes bound to the skeleton, and the transform of
       the model at export time.  We need to create a $model_instance to hold the mutable
       data about this specific instance of that model.  (Current transform, active
       controls, etc.)

       Animations are bound to a specific $model_instance, rather than the unchanging
       $model.  This allows you to create as many copies of the character as you need
       without duplicating the common information.  Note that unlike a $model, a
       $model_instance is allocated independently from the $file, so you must clean it up
       with $FreeModelInstance. */
    granny_model_instance* ModelInstance = GrannyInstantiateModel(Model);

    /* You have several options for binding the $animation to your $model_instance.  Each
       affords a different level of control over the binding, giving you the option of
       trading simplicity for micro-management if you want.  We'll start with the simplest
       binding method, $PlayControlledAnimation, which simply takes the animation and
       instance as inputs, and hands you the control.  The StartTime parameter to this
       function indicates the $(control)s start offset from the global clock.  (See
       $AnimationOverview for more details on Granny's concept of time.) */
    granny_control* SimpleControl =
        GrannyPlayControlledAnimation(0.0f, Animation, ModelInstance);

    /* For more detail, you can use a $controlled_animation_builder object to tweak every
       detail of the created control.  We'll duplicate the effect of the above call using
       a builder object.  For more details on what you can accomplish using the builder
       (and even more ways to create $control objects), read $AnimationOverview and
       $AnimationInDepth.  The code here is essentially the same as the code that
       implements $PlayControlledAnimation, you can examine the functions in
       granny_controlled_animation.cpp for more details, and ideas. */
    granny_control* DetailedControl = NULL;
    granny_int32x TrackGroupIndex;
    if(GrannyFindTrackGroupForModel(Animation,
                                    Model->Name,
                                    &TrackGroupIndex))
    {
        granny_controlled_animation_builder* Builder =
            GrannyBeginControlledAnimation(0.0f, Animation);
        assert(Builder);

        GrannySetTrackGroupTarget(Builder, TrackGroupIndex, ModelInstance);
        GrannySetTrackGroupLOD(Builder, TrackGroupIndex, true, 1.0f);
        DetailedControl = GrannyEndControlledAnimation(Builder);
    }
    else
    {
        printf("Error: unable to find track group index\n");
        return EXIT_FAILURE;
    }

    /* We'll dump some stats about the first control. */
    printf("SimpleControl [%p]\n"
           "     Duration: %f\n"
           "        Clock: %f\n"
           "   IsComplete: %s\n"
           "       Weight: %f\n\n",
           SimpleControl,
           GrannyGetControlDuration(SimpleControl),
           GrannyGetControlClock(SimpleControl),
           GrannyControlIsComplete(SimpleControl) ? "true" : "false",
           GrannyGetControlWeight(SimpleControl));

    /* OK, we've bound the animation to the model twice, now what?  To sample the
       animation, you need to create two objects, a $local_pose, to represent the
       transforms of the joints in "parent-relative" space, and a $world_pose, which
       represents the transforms in world space.  See $AnimationOverview_LocalPoseSection
       for more details.

       In a real application, of course, you should create cached copies of the
       $local_pose and $world_pose, rather than creating them every frame.  You can even
       share them between $(model_instance)s, as long as you ensure that you're done using
       the results from one sample before beginning the next. */
    granny_local_pose* LocalPose = GrannyNewLocalPose(Model->Skeleton->BoneCount);
    granny_world_pose* WorldPose = GrannyNewWorldPose(Model->Skeleton->BoneCount);

    /* Next, we need to tell Granny which controls should affect the model, and what the
       current clock is.  Since we setup both controls to be identical, we'll just turn
       one off, and set the other's influence to 1.0.  We'll dump the position of the
       "neck00" bone over a few frames, which should give us some good variation in
       position in world space. */
    GrannySetControlWeight(SimpleControl,   1.0f);
    GrannySetControlWeight(DetailedControl, 1.0f);

    granny_int32x NeckBoneIndex;
    if (!GrannyFindBoneByName(Model->Skeleton, "neck00", &NeckBoneIndex))
    {
        printf("Error: unable to find neck bone\n");
        return EXIT_FAILURE;
    }

    granny_real32 const Duration = GrannyGetControlDuration(SimpleControl);
    int const TotalSamples = 20;
    printf("Neck positions:\n");
    {for(int CurrentFrame = 0; CurrentFrame < TotalSamples; ++CurrentFrame)
    {
        granny_real32 CurrT = CurrentFrame * (Duration / (TotalSamples - 1));

        // $SetModelClock simply asks Granny to iterate through each of the controls
        // attached to the model instance, and set their local time based on the passed in
        // application clock value.
        GrannySetModelClock(ModelInstance, CurrT);

        // We could technically restrict the sample to the index of the neck bone, but
        // we'll sample the whole model.
        GrannySampleModelAnimations(ModelInstance,
                                    0, Model->Skeleton->BoneCount,
                                    LocalPose);

        // Take the parent-relative local pose, and turn it into a set of GPU-ready world
        // space matrices.  Passing NULL as the Offset4x4 parameter indicates that we want
        // the model to be at the origin.  Again, we could restrict this to only compute
        // the bones up to the neck index, but we'll build the full pose.
        GrannyBuildWorldPose(Model->Skeleton,
                             0, Model->Skeleton->BoneCount,
                             LocalPose, NULL,
                             WorldPose);

        // Granny's world matrices are column major (though we'll later show you how to
        // create row-major output), so the translation is at indices 12-14.
        granny_real32 const* NeckMatrix = GrannyGetWorldPose4x4(WorldPose,
                                                                NeckBoneIndex);

        printf("  t = %.3f: [% .3f, % .3f, % .3f]\n",
               CurrT, NeckMatrix[12], NeckMatrix[13], NeckMatrix[14]);
    }}


    // Cleanup
    GrannyFreeLocalPose(LocalPose);
    GrannyFreeWorldPose(WorldPose);
    GrannyFreeControl(SimpleControl);
    GrannyFreeControl(DetailedControl);
    GrannyFreeModelInstance(ModelInstance);
    GrannyFreeFile(GrannyFile);
    GrannyFile     = NULL;

    return EXIT_SUCCESS;
}

/* DEFTUTORIAL EXPGROUP(TutorialCreateControl) (TutorialCreateControl_Summary, Summary) */
/* You should now have a grasp of the basics of binding animations.  If you're racing
   through the tutorials trying to get something up on the screen, hang in there!  Just
   one more to go, and you're set.  From here, you may want to check out:

   $* $TutorialDX9Rendering: render an animated model in D3D9.
   $* $TutorialTransformFile: convert Granny files into your application's coordinate
   system.
   $-
*/

/* DEFTUTORIALEND */


// Loads the pixo run file, and extracts the first model and animation.  pixo_run.gr2 is
// intentially simple, with just one model, and one animation.  In a real engine, nothing
// stops you from working with smaller chunks of a file, it can be extremely handy to
// store multiple models and animations in a single .gr2 file.
bool GetTestObjects(granny_file** GrannyFile,
                    granny_animation** Animation,
                    granny_model** Model)
{
    assert(GrannyFile && Animation && Model);

#ifdef _MACOSX
    const char* DefaultFileName = "../../media/pixo_run.gr2";
#else
    const char* DefaultFileName = "media/pixo_run.gr2";
#endif
    *GrannyFile = GrannyReadEntireFile(DefaultFileName);
    if (*GrannyFile == NULL)
        return false;

    granny_file_info* FileInfo = GrannyGetFileInfo(*GrannyFile);
    if (FileInfo == NULL)
        return false;

    if (FileInfo->ModelCount != 1 ||
        FileInfo->AnimationCount != 1)
    {
        return false;
    }

    *Animation = FileInfo->Animations[0];
    *Model     = FileInfo->Models[0];

    return true;
}
