// =======================================================================================
// dx9_rendering.cpp: Demonstrates rendering an animated mesh in DX9
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "ik_2bone.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <vector>

using namespace std;

// ---------------------------------------------------------
// Scene instance objects
DemoScene GlobalScene;


/* DEFTUTORIAL EXPGROUP(TutorialIK) (TutorialIK_Introduction, Introduction) */
/* We're going to set a record for short tutorials here.  A real IK fixup subsystem in a
   real game is a complicated thing.  You have to track FOOTDOWN and FOOTUP events,
   interact with your collision database to find out where your feet or hands should be
   placed to prevent penetration of solid geometry, and potentially track multiple bone
   chains simultaneously on your characters to make sure that all of your visual goals are
   being met.
   $-
   We're going to pretend none of that is necessary.
   $-
   What we're going to demo here is the simplest possible way to interface with the Granny
   IK routines, while completely ignoring any interaction with the larger application,
   which is inevitably going to be extremely game-specific.  The problem we're going to
   solve is this: We have a character, and we're going to animate his left and right leg
   with the 2 Granny IK interfaces, $IKUpdate and $IKUpdate2Bone.

   Prerequisites to this demo:
   $* $TutorialBasicLoading
   $* $TutorialDX9Rendering
   $-
*/

/* DEFTUTORIAL EXPGROUP(TutorialIK) (TutorialIK_ComputeLegPosition, ComputeLegPosition) */
void ComputeLegPosition(SimpleModel* Model)
{
    /* Because the IK routines work with both the $local_pose and $world_pose of the
       model, we can't use $SampleModelAnimationsAccelerated, since it might not build a
       valid $local_pose.  In fact, since we don't really care about the animation in this
       example, we'll just build default poses using the rest pose of the model.
    */
    GrannyBuildRestLocalPose(GrannyGetSourceSkeleton(Model->ModelInstance),
                             0, GrannyGetSourceSkeleton(Model->ModelInstance)->BoneCount,
                             GlobalScene.SharedLocalPose);
    GrannyBuildRestWorldPose(GrannyGetSourceSkeleton(Model->ModelInstance),
                             0, GrannyGetSourceSkeleton(Model->ModelInstance)->BoneCount,
                             NULL, GlobalScene.SharedWorldPose);

    /* Ok, now move the right leg with the 2 bone ik routine.  The 2-bone routine take as
       argument the bone index of the end-effector, the target position the end-effector
       is supposed to reach, and the constraint plane, which is the normal of the plane in
       *world* space that the middle joint, in this case the knee, should rotate in.
       You'll know when you've gotten this plane the wrong way 'round, because you'll
       break the kneecaps of your characters.
       $-

       The last two parameters control the hyperextension softening routine in the IK.
       You can look at the source for the 2-bone solver (in granny_ik.cpp), if you need to
       know exactly what's going on, but the brief version is as follows.  As the "knee"
       straightens, small motions of the end effector can lead to rapid changes in the
       angular velocity of the knee.  The hyperextension parameters basically modify the
       behavior of the routine near this point, and try to soften the transition a bit.
       You should probably simply leave these defaults as is.
       $-

       The bone we want here is R_foot, which is a part of the chain: R_foot-R_knee-R_hip.
       Normally we would cache out the index of the bone of course, but we're keeping it
       simple.
       $-

       We'll drive the position around with a couple of simple sin curves.  I'm
       intentionally making the range larger than it is possible to reach with the leg to
       show what happens when the goal is too far.
    */
    granny_int32x R_foot;
    if (GrannyFindBoneByNameLowercase(GrannyGetSourceSkeleton(Model->ModelInstance),
                                      "R_foot", &R_foot))
    {
        float Pos3[3]  = { -5,
                           7 * sin(0.5f * GlobalScene.CurrentTime) + 8,   // vertical
                           20 * sin(5.0f * GlobalScene.CurrentTime) };    // forward/back
        float Norm3[3] = { 1, 0, 0 };
        GrannyIKUpdate2Bone(R_foot, Pos3, Norm3,
                            GrannyGetSourceSkeleton(Model->ModelInstance),
                            NULL,
                            GlobalScene.SharedLocalPose,
                            GlobalScene.SharedWorldPose,
                            0.1f, 0.5f);
    }

    /* Ok, you're asking yourself: "Why is this next bit commented out?"  Well, here's the
       deal.  The general purpose IK routine, $IKUpdate, is a full ball-joint Cyclic
       Coordinate Descent (CCD), routine, which mean that every joint in the chain is free
       to move as far as possible towards the end-effector goal at each iteration.  That
       can be extremely handy, when your hand absolutely, positively *has* to hit that
       doorknob, but it works best for doing small fixups of a mostly correct animation.
       In this case, we're again driving the position of a leg on a sin wave, which is not
       a small motion.  So it's going to look a little funny.  If you want to see how
       funny, just uncomment this.  Feet don't usually do that, right?
       $-

       Since the CCD solver is iterative, you pass in a number of iterations that the CCD
       routine.  In general the routine will reach the target in a very small number of
       iterations.  Each iteration is a significant amount of computation, so you should
       experiment to find the smallest value that gives you good results.
    */
#if 0
    granny_int32x L_football;
    if (GrannyFindBoneByNameLowercase(GrannyGetSourceSkeleton(Model->ModelInstance),
                                      "L_football", &L_football))
    {
        float Pos3[3]  = { 5,
                           7 * sin(0.5f * GlobalScene.CurrentTime) + 8,  // vertical
                           20 * sin(5.0f * GlobalScene.CurrentTime) };   // forward/back
        GrannyIKUpdate(5, L_football, Pos3, 4,
                       GrannyGetSourceSkeleton(Model->ModelInstance),
                       NULL,
                       GlobalScene.SharedLocalPose,
                       GlobalScene.SharedWorldPose);
    }
#endif
}


/* DEFTUTORIAL EXPGROUP(TutorialIK) (TutorialIK_Summary, Summary) */
/* There's not actually much to say about this example.  The IK routines themselves are
   fairly straightforward, as you can see.  Most of the complexity lives out in
   application-land, tracking the events that engage and disengage the fixup routines.  In
   almost every case, you'll want to use the 2-bone IK routine in the absence of a
   compelling reason to use the general CCD routine.  Setting aside appearance issues,
   it's much faster, since it's a closed form solution rather than an iteration.
*/

/* DEFTUTORIALEND */

