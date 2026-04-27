// =======================================================================================
// attachment.cpp: Demonstrates the Granny attachment API
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "attachment.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

using namespace std;

char const* const GlobalCharacterFilename = "media\\pixo_run_walk.gr2";
char const* const GlobalBatonFilename     = "media\\attachment\\baton.gr2";
char const* const GlobalCapeFilename     = "media\\attachment\\cape.gr2";

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

/* DEFTUTORIAL EXPGROUP(TutorialAttachment) (TutorialAttachment_Introduction, Introduction) */
/* This tutorial will demonstrate a working (if simple) attachment system for sticking a
   model onto a specific bone on another model.  The Granny document
   $AnimationInDepth_AttachmentSection goes into some detail about how to make this
   process more efficient on a larger scale by caching static results, etc.  What we're
   trying to accomplish here is just to show you how to use the $GetAttachmentOffset API,
   and to cascade the attachment offset into the $world_pose of the attached model.

   To that end, we'll bring back our favorite demo character, Pixo, and put a baton in his
   left hand, and attach a tiny cape to his neck.  Advance apologies for the programmer
   art contained in this demo.  :)

   Prerequisites to this demo:
   $* $TutorialBasicLoading
   $* $TutorialCreateControl
   $* $TutorialDX9Rendering
   $-

   New functions and structures discussed in this tutorial:
   $* $GetAttachmentOffset
   $-
*/

/* DEFTUTORIAL EXPGROUP(TutorialAttachment) (TutorialAttachement_CreateDemoModels, CreateDemoModels) */
/* To keep this tutorial simple, we're going to hard-code it for 3 objects.  A character playing a run
   cycle, a baton for his left hand, and a cape for his neck.  The DemoModel class has been altered 
   from the base DirectX 9 tutorial in the following manner: if the "AttachedTo" member is a non-NULL
   pointer to another model, we render the model through the $TutorialAttachement_RenderAttachment
   function below.
*/
bool CreateDemoModels()
{
    assert(GlobalScene.CharacterFileInfo);
    assert(g_pD3DDevice);

    /* Instantiate the base character model */
    {
        GlobalScene.Character = new DemoModel(GlobalScene.CharacterFileInfo->Models[0]);

        // And play it's animation...
        granny_control* Control =
            GrannyPlayControlledAnimation(0.0f,
                                          GlobalScene.CharacterFileInfo->Animations[0],
                                          GlobalScene.Character->ModelInstance);
        GrannySetControlSpeed(Control, 0.25f);
        GrannySetControlLoopCount(Control, 0);
        GrannyFreeControlOnceUnused(Control);
    }

    /* Create a baton model, attached to the left hand */
    {
        GlobalScene.Baton = new DemoModel(GlobalScene.BatonFileInfo->Models[0]);

        // Attach the root of the baton at the L_hand bone of the character model
        GlobalScene.Baton->AttachedTo = GlobalScene.Character;

        // Attach to the root of the baton
        GlobalScene.Baton->AttachModelBone = 0;

        // Attach to L_hand of the character
        GrannyFindBoneByNameLowercase(GrannyGetSourceSkeleton(GlobalScene.Character->ModelInstance),
                                      "L_hand",
                                      &GlobalScene.Baton->BaseModelBone);
    }

    /* Create the cape, worn at the neck */
    {
        GlobalScene.Cape = new DemoModel(GlobalScene.CapeFileInfo->Models[0]);

        // Attach the root of the baton at the L_hand bone of the character model
        GlobalScene.Cape->AttachedTo = GlobalScene.Character;

        // Attach to the root of the cape
        GlobalScene.Cape->AttachModelBone = 0;

        // Attach to a neck bone
        GrannyFindBoneByNameLowercase(GrannyGetSourceSkeleton(GlobalScene.Character->ModelInstance),
                                      "neckMid",
                                      &GlobalScene.Cape->BaseModelBone);

        // And play it's animation...
        granny_control* Control = GrannyPlayControlledAnimation(0.0f,
                                                                GlobalScene.CapeFileInfo->Animations[0],
                                                                GlobalScene.Cape->ModelInstance);
        GrannySetControlSpeed(Control, 2.f);
        GrannySetControlLoopCount(Control, 0);
        GrannyFreeControlOnceUnused(Control);
    }

    return true;
}


/* DEFTUTORIAL EXPGROUP(TutorialAttachment) (TutorialAttachement_RenderAttachment, RenderAttachedModel) */
/* In the assocated file attachment_helpers.cpp, any models that are attached are routed
   to this function so we can consider the problem in isolation.  The attachment method
   here is as simple as possible, which means that it doesn't cache intermediate results
   that are potentially constant.  We'll note potentially cachable intermedate results in
   the code, and the document section $AnimationInDepth_AttachmentSection goes into more
   detail about when, for instance, storing the result of $GetAttachmentOffset is
   appropriate.

   In the course of this function, we'll refer to the character that the attachment binds
   to as the "base" model.
*/
void RenderAttachedModel(DemoModel* AttachedModel)
{
    assert(AttachedModel && AttachedModel->AttachedTo);
    assert(AttachedModel->BaseModelBone != -1);
    assert(AttachedModel->AttachModelBone != -1);

    /* We rebuild the base model's WorldPose matrix for the attached bone.  In your
       application, you may already have constructed a $local_pose or $world_pose for the
       base, using that result will be faster, of course. */
    granny_skeleton* BaseSkeleton =
        GrannyGetSourceSkeleton(AttachedModel->AttachedTo->ModelInstance);
    assert(AttachedModel->BaseModelBone >= 0 &&
           AttachedModel->BaseModelBone < BaseSkeleton->BoneCount);

    GrannySampleModelAnimations(AttachedModel->AttachedTo->ModelInstance,
                                0, BaseSkeleton->BoneCount,
                                GlobalScene.SharedLocalPose);

    // Retrieve the world matrix for the bone we care about...
    granny_matrix_4x4 BaseMatrix;
    GrannyGetWorldMatrixFromLocalPose(BaseSkeleton,
                                      AttachedModel->BaseModelBone,
                                      GlobalScene.SharedLocalPose,
                                      AttachedModel->AttachedTo->InitialMatrix,
                                      (granny_real32*)BaseMatrix,
                                      NULL, NULL);

    /* At this point, we have all the information we need from the $local_pose for the
       base model, so we can safely overwrite it with the animated state of the attached
       model */
    granny_skeleton* AttachSkeleton =
        GrannyGetSourceSkeleton(AttachedModel->ModelInstance);
    GrannySampleModelAnimations(AttachedModel->ModelInstance,
                                0, AttachSkeleton->BoneCount,
                                GlobalScene.SharedLocalPose);

    /* Compute the attachment offset for the attached model.  Note that if the attachment
       model is not animating, then this matrix is static, and may be cached.  Similarly,
       if the attachment bone is the root of the model, then there is no need to compute
       it in the first place. */
    granny_matrix_4x4 AttachmentMatrix;
    GrannyGetAttachmentOffset(AttachSkeleton,
                              AttachedModel->AttachModelBone,
                              GlobalScene.SharedLocalPose,
                              NULL,
                              (granny_real32*)AttachmentMatrix,
                              NULL, NULL);

    /* Now build the world pose using the attachment offset multiplied by the matrix of
       the base model bone we're attaching to.  As noted in
       $AnimationInDepth_AttachmentSection, you can use either your own matrix routines,
       or the provided Granny routine $ColumnMatrixMultiply4x3 to construct the Offset4x4
       matrix. */
    granny_matrix_4x4 Offset4x4;
    GrannyColumnMatrixMultiply4x3((granny_real32*)Offset4x4,
                                  (granny_real32*)AttachmentMatrix,
                                  (granny_real32*)BaseMatrix);

    GrannyBuildWorldPose(AttachSkeleton, 0, AttachSkeleton->BoneCount,
                         GlobalScene.SharedLocalPose,
                         (granny_real32*)Offset4x4,
                         GlobalScene.SharedWorldPose);

    /* And that's it!  Applying SharedWorldPose to our mesh bindings will cause them to
       render at the correct position. */
    RenderMeshBindings(AttachedModel, GlobalScene.SharedWorldPose);
}



/* DEFTUTORIAL EXPGROUP(TutorialAttachment) (TutorialAttachement_Summary, Summary) */
/* $GetAttachmentOffset has a bit more power than we've demonstrated here.  In particular,
   the attachment offset can be computed for any arbitrary bone in the attachment model
   (think of grabbing a snake by its midpoint), while we've attached both of our sample
   models by their root.  Refer to $AnimationInDepth_AttachmentSection for more detail.
 */

/* DEFTUTORIALEND */

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

        if ((GlobalScene.CharacterFile     = GrannyReadEntireFile(GlobalCharacterFilename)) == NULL ||
            (GlobalScene.CharacterFileInfo = GrannyGetFileInfo(GlobalScene.CharacterFile)) == NULL)
        {
            return EXIT_FAILURE;
        }

        if ((GlobalScene.BatonFile     = GrannyReadEntireFile(GlobalBatonFilename)) == NULL ||
            (GlobalScene.BatonFileInfo = GrannyGetFileInfo(GlobalScene.BatonFile)) == NULL)
        {
            return EXIT_FAILURE;
        }

        if ((GlobalScene.CapeFile     = GrannyReadEntireFile(GlobalCapeFilename)) == NULL ||
            (GlobalScene.CapeFileInfo = GrannyGetFileInfo(GlobalScene.CapeFile)) == NULL)
        {
            return EXIT_FAILURE;
        }

        if (CreateDemoTextures() == false || CreateDemoModels() == false ||
            CreateSharedPoses() == false  || InitCameraAndLights() == false)
        {
            return EXIT_FAILURE;
        }
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

        // Update the world (Slow down Pixo's run a bit)
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
