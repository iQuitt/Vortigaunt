// =======================================================================================
// transform_file.cpp: Demonstrates converting a Granny file's coordinate system
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "granny.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


bool LoadTestFile(granny_file** OutputFile,
                  granny_file_info** OutputFileInfo);


/* DEFTUTORIAL EXPGROUP(TutorialTransformFile) (TutorialTransformFile_Introduction, Introduction) */
/* This tutorial shows you how to load a Granny file and convert it to your engine's
   coordinate system.
   $-

   Prerequisites to this demo:
   $* $TutorialBasicLoading
   $-

   New functions and structures discussed in this tutorial:
   $* $ComputeBasisConversion
   $* $TransformFile
   $-

   Function index:
   $* $TutorialTransformFile_main
   $-
*/


/* DEFTUTORIAL EXPGROUP(TutorialTransformFile) (TutorialTransformFile_main, main) */
int main(int, char const*[])
{
    // Ensure the API version matches
    if(!GrannyVersionsMatch)
    {
        printf("Warning: the Granny DLL currently loaded "
               "doesn't match the .h file used during compilation\n");
        return EXIT_FAILURE;
    }

    // Load the test file
    granny_file* TestFile = NULL;
    granny_file_info* TestFileInfo = NULL;
    if (LoadTestFile(&TestFile, &TestFileInfo) == false)
    {
        printf("Error: can't load the test file!\n");
        return EXIT_FAILURE;
    }

    /* There are two ways to compute a basis conversion.  First, and easiest, is to use
       the information in the $file_info ArtToolInfo member to compute the transform from
       the current space of the file to the desired space.  Simply specify the the
       coordinate system, and the desired number of units per meter.  We'll convert the
       test file to a Z-up, NegY-back coordinate system from the Maya default of Y up,
       Z-back */
    granny_real32 DesiredUnitsPerMeter = 1.0f;
    granny_real32 DesiredOrigin[]      = { 0, 0, 0 };
    granny_real32 DesiredRight[]       = { -1,  0, 0 };
    granny_real32 DesiredUp[]          = {  0,  0, 1 };
    granny_real32 DesiredBack[]        = {  0, -1, 0 };

    granny_real32 Affine3[3];
    granny_real32 Linear3x3[9];
    granny_real32 InverseLinear3x3[9];
    if (GrannyComputeBasisConversion(TestFileInfo,
                                     DesiredUnitsPerMeter,
                                     DesiredOrigin,
                                     DesiredRight,
                                     DesiredUp,
                                     DesiredBack,
                                     Affine3, Linear3x3, InverseLinear3x3) == false)
    {
        // This can fail if the ArtToolInfo member is missing, either because it was
        // turned off in the exporter, or stripped by the preprocessor.
        printf("Error: can't compute a basis conversion from the file info\n");
        return EXIT_FAILURE;
    }

    /* The second way to compute a basis conversion is simply to manually compute the
       Linear and Affine transform components.  Affine3 is the positional offset of the
       new coordinate system.  Linear3x3 is the transformation to apply to rotate and
       scale the coordinate frame, InverseLinear3x3 is the inverse of this transform.
       We'll not use a manual transform in this sample, but you can imagine what the
       matrices might look like.  Examine the code for $ComputeBasisConversion for details
       on how the math works.  (Found in granny_basis_conversion.cpp)

       Now that we have a basis conversion transform, we simply call $TransformFile to ask
       Granny to change the coordinate system of all models, animations, and meshes in the
       file.  Note that we pass 2 tolerance magic values to allow Granny to detect
       transforms that are coordinate swaps, rather than full rotations.  This allows a
       more efficient conversion of the file.  In general, 1e-5f is fine for both of these
       values.

       The flags passed to $TransformFile ask Granny to ensure that vertex normals are
       correctly scaled after applying a non-length-preserving transform and to rewind
       triangles if the transform changes the "handedness" of the file's coordinate
       system. */
    granny_real32 const Tolerance = 1e-5f;

    GrannyTransformFile(TestFileInfo,
                        Affine3, Linear3x3, InverseLinear3x3,
                        Tolerance, Tolerance,
                        GrannyRenormalizeNormals | GrannyReorderTriangleIndices);

    /* That's it, the file is now converted to our hypothetical engine coordinates.
       Remember that if you apply a $TransformFile operation to a file that contains
       meshes and models, you have to apply the same transformation to the files that
       contain it's animations, or you won't get correct playback. */

    // Cleanup our allocated structures
    GrannyFreeFile(TestFile);
    TestFileInfo = NULL;
    TestFile = NULL;

    return EXIT_SUCCESS;
}

/* DEFTUTORIAL EXPGROUP(TutorialTransformFile) (TutorialTransformFile_Summary, Summary) */
/* You can now ensure that any loaded Granny file conforms to the coordinate system that
   your engine expects.  From here, you might want to explore:

   $* $TutorialCreateControl: bind an animation to model for playback
   $* $TutorialPreprocessor: use the Granny preprocessor for common toolchain tasks.
   $-
*/

/* DEFTUTORIALEND */


// Simple function to prep a test file
bool LoadTestFile(granny_file** OutputFile,
                  granny_file_info** OutputFileInfo)
{
    assert(OutputFile && OutputFileInfo);

#ifdef _MACOSX
    const char* TestFilename = "../../media/pixo_run.gr2";
#else
    const char* TestFilename = "media/pixo_run.gr2";
#endif

    *OutputFile = GrannyReadEntireFile(TestFilename);
    if (*OutputFile == NULL)
    {
        printf("Error in ReadEntireFile(\"%s\")\n", TestFilename);
        return false;
    }

    *OutputFileInfo = GrannyGetFileInfo(*OutputFile);
    if (*OutputFileInfo == NULL)
    {
        printf("Error in GetFileInfo() for \"%s\"\n", TestFilename);
        return false;
    }

    return true;
}
