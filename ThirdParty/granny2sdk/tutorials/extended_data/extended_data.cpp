// =======================================================================================
// custom_format.cpp: Demonstrates using Granny's file API with your own data
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "granny.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <math.h>
#include <string.h>

// Forward decls.
void ExtractLightInfo(char const* ModelName,
                      granny_bone& Bone,
                      granny_variant& LightInfo);

void ExtractCameraInfo(char const* ModelName,
                       granny_bone& Bone,
                       granny_variant& CameraInfo);

// Macosx has strcasecmp rather than _stricmp
#if defined(_MACOSX) || defined(LINUX)
#define _stricmp strcasecmp
#endif


/* DEFTUTORIAL EXPGROUP(TutorialExtendedData) (TutorialExtendedData_Introduction, Introduction) */
/* Granny can't know everything about the models that you work with in Maya and Max.  Even
   if we could, imagine what the Granny structures would look like if we tried to take
   every possible object field into account!  Ouch.  However, we do try to export every
   last piece of data from the tools that we can find.  In most cases, these custom
   attributes wind up in the ExtendedData fields attached to the base Granny structures.
   For instance $bone contains an ExtendedData field that we use to store information
   about Camera or Light parameters, if the bone represents one of these objects, a
   $(material)'s ExtendedData field might contain shader parameters, and so forth.

   This tutorial will show you how to extract that data so you can work with it in your
   toolchain or your engine.  The example we'll use is the extraction of Camera and Light
   parameters from a Granny file originally exported from Maya.  (You'll see some of this
   put to use in $TutorialDX9Rendering.)  The source art for the demo can be found in the
   "media" directory of this tutorial, if you want to see the file setup.

   Before we begin, you might want to refer to the documentation on $(variant)s.  As long
   as you understand that a $variant is nothing more than an chunk of object data along
   with a $data_type_definition array that describes the data, the tutorial will make
   sense.  Granny uses $(variant)s whenever we can't (or don't want to) build a fixed
   structure to hold extracted information.  In the case of LightInfo, for example, we
   could in theory build a granny_light structure that held common fields shared across
   all content packages, but we prefer to simply extract as much information as we can
   from the content package and leave the interpretation up to you.  This prevents us from
   inadvertently restricting your freedom by either imposing our own interpretation on a
   field, or leaving it out entirely because it's not common to all of the content
   packages.  At the end of this tutorial, you should be able to write small routines to
   extract whatever you want from the ExtendedData fields in just a few minutes, which is
   a pretty fair tradeoff for having the full expressive power of your DCC tool available
   to you.

   Prerequisites to this demo:
   $* $TutorialBasicLoading
   $-

   New functions and structures discussed in this tutorial:
   $* $data_type_definition
   $* $variant
   $* $GetTotalObjectSize
   $* $ConvertSingleObject
   $-

   Functions and structures introduced incidentally:
   $- $bone
   $-

   Function index:
   $* $TutorialExtendedData_main
   $* $TutorialExtendedData_ExtractLightInfo
   $* $TutorialExtendedData_ExtractCameraInfo
   $-
*/

/* DEFTUTORIAL EXPGROUP(TutorialExtendedData) (TutorialExtendedData_main, main) */
int main(int, char const*[])
{
    // Ensure the API version matches
    if(!GrannyVersionsMatch)
    {
        printf("Warning: the Granny DLL currently loaded "
               "doesn't match the .h file used during compilation\n");
        return EXIT_FAILURE;
    }

    const char* LightsCameraFilename = "media/extended_data/lights_and_camera.gr2";

    // Load the test files
    granny_file* LightsCameraFile = GrannyReadEntireFile(LightsCameraFilename);
    if (!LightsCameraFile)
        return EXIT_FAILURE;

    granny_file_info* LightsCameraInfo = GrannyGetFileInfo(LightsCameraFile);
    if (!LightsCameraInfo)
        return EXIT_FAILURE;

    /* Walk through the models on the lights and cameras file, looking for bones that have
       the appropriate ExtendedData entries.  These objects are encoded as bones (which
       may be attached to other models), with an ExtendedData field named "LightInfo" or
       "CameraInfo".  Forward matching bones to the appropriate extraction function. */
    {for(granny_int32x ModelIdx = 0; ModelIdx < LightsCameraInfo->ModelCount; ++ModelIdx)
    {
        granny_model* Model = LightsCameraInfo->Models[ModelIdx];

        // Walk the bones
        {for(granny_int32x BoneIdx = 0; BoneIdx < Model->Skeleton->BoneCount; ++BoneIdx)
        {
            granny_bone& Bone = Model->Skeleton->Bones[BoneIdx];

            /* Light and Camera info are stored in a $ReferenceMember in the ExtendedData.
               This means that the ExtendedData stores a pointer to the collected data,
               rather than containing it inline.  If we find the member, we just forward
               it to our extraction function.  Note that it's pretty unlikely that a bone
               would be both a Camera <i>and</i> a Light, but we allow for that
               possibility here. */
            granny_variant LightInfo;
            if (GrannyFindMatchingMember(Bone.ExtendedData.Type,
                                         Bone.ExtendedData.Object,
                                         "LightInfo",
                                         &LightInfo))
            {
                ExtractLightInfo(Model->Name, Bone, LightInfo);
            }

            granny_variant CameraInfo;
            if (GrannyFindMatchingMember(Bone.ExtendedData.Type,
                                         Bone.ExtendedData.Object,
                                         "CameraInfo",
                                         &CameraInfo))
            {
                ExtractCameraInfo(Model->Name, Bone, CameraInfo);
            }
        }}
    }}

    // Cleanup
    GrannyFreeFile(LightsCameraFile);

    return EXIT_SUCCESS;
}

/* DEFTUTORIAL EXPGROUP(TutorialExtendedData) (TutorialExtendedData_ExtractLightInfo, ExtractLightInfo) */
/* The simplest way to extract data from a $variant is to define a structure that
   describes the fields that are interesting to you, and use $ConvertSingleObject to have
   Granny do the translation for you.  There are other methods, such as using the
   $data_type_definition objects directly to iterate over the members, which are handy if
   you need to get your fingers all the way into the type information, but this is
   sufficient for our purposes.

   We'll extract the type of the light, it's color, and some spotlight parameters, if they
   exist.  The position and orientation of each light can be extracted from the $bone
   itself, but we'll ignore that for the purposes of the tutorial.
*/
struct ImportantLightFields
{
    char const*   typeName;

    granny_real32 colorR;
    granny_real32 colorG;
    granny_real32 colorB;

    // Only valid if typeName == "spotLight"
    granny_real32 coneAngle;
    granny_real32 penumbraAngle;
    granny_real32 dropoff;
};

granny_data_type_definition ImportantLightFieldsType[] = {
    { GrannyStringMember, "typeName" },

    { GrannyReal32Member, "colorR" },
    { GrannyReal32Member, "colorG" },
    { GrannyReal32Member, "colorB" },

    { GrannyReal32Member, "coneAngle" },
    { GrannyReal32Member, "penumbraAngle" },
    { GrannyReal32Member, "dropoff" },

    { GrannyEndMember }
};


void ExtractLightInfo(char const* ModelName,
                      granny_bone& Bone,
                      granny_variant& LightInfo)
{
    printf("Light found in model %s, bone: %s\n", ModelName, Bone.Name);

    /* Check that the size of our structure agrees with what Granny thinks it is.  You
       have a check like this somewhere in your code every time you use a
       $data_type_definition array to describe one of your own structures.  This prevents
       alignment problems, or missing fields from causing bugs in your code that can be
       pretty tricky to nail down. */
    assert(sizeof(ImportantLightFields) ==
           GrannyGetTotalObjectSize(ImportantLightFieldsType));

    /* Do the actual object conversion.  $ConvertSingleObject will extract any matching
       members in our light fields type, and zero out any fields that aren't present. */
    ImportantLightFields LightFields;
    {
        // The light info should be a reference to the structure that contains the custom info.
        assert(LightInfo.Type[0].Type == GrannyReferenceMember);

        granny_data_type_definition* LightType = LightInfo.Type[0].ReferenceType;
        void const* LightObject = *((void const**)(LightInfo.Object));

        GrannyConvertSingleObject(LightType, LightObject,
                                  ImportantLightFieldsType,
                                  &LightFields, 0);
    }

    printf("  light class: %s\n", LightFields.typeName);
    printf("  light color: [% f % f % f]\n",
           LightFields.colorR,
           LightFields.colorG,
           LightFields.colorB);

    // Dump the spotlight parameters if this is the correct type
    if (_stricmp(LightFields.typeName, "spotLight") == 0)
    {
        printf("      cone angle: %f\n", LightFields.coneAngle);
        printf("  penumbra angle: %f\n", LightFields.penumbraAngle);
        printf("         dropoff: %f\n", LightFields.dropoff);
    }

    printf("\n");
}

/* DEFTUTORIAL EXPGROUP(TutorialExtendedData) (TutorialExtendedData_ExtractCameraInfo, ExtractCameraInfo) */
/* More or less the same as $TutorialExtendedData_ExtractLightInfo.  See the comments for
   that function for details */
struct ImportantCameraFields
{
    char const* typeName;

    granny_real32 nearClipPlane;
    granny_real32 farClipPlane;

    // Rough equivalent of FOV
    granny_real32 horizontalFilmAperture;
};

granny_data_type_definition ImportantCameraFieldsType[] = {
    { GrannyStringMember, "typeName" },

    { GrannyReal32Member, "nearClipPlane" },
    { GrannyReal32Member, "farClipPlane" },
    { GrannyReal32Member, "horizontalFilmAperture" },

    { GrannyEndMember }
};

void ExtractCameraInfo(char const* ModelName,
                       granny_bone& Bone,
                       granny_variant& CameraInfo)
{
    printf("Camera found in model %s, bone: %s\n", ModelName, Bone.Name);

    // Check that the size of our structure agrees with what Granny thinks it is.
    assert(sizeof(ImportantCameraFields) ==
           GrannyGetTotalObjectSize(ImportantCameraFieldsType));


    ImportantCameraFields CameraFields;
    {
        // The light info should be a reference to the structure that contains the custom info.
        assert(CameraInfo.Type[0].Type == GrannyReferenceMember);

        granny_data_type_definition* CameraType = CameraInfo.Type[0].ReferenceType;
        void const* CameraObject = *((void const**)(CameraInfo.Object));

        GrannyConvertSingleObject(CameraType, CameraObject,
                                  ImportantCameraFieldsType,
                                  &CameraFields, 0);
    }

    printf("     camera class: %s\n", CameraFields.typeName);
    printf("  near clip plane: %f\n", CameraFields.nearClipPlane);
    printf("   far clip plane: %f\n", CameraFields.farClipPlane);
    printf("   horizontal FOV: %f rad, %f deg\n",
           CameraFields.horizontalFilmAperture,
           CameraFields.horizontalFilmAperture * 180.0f / 3.141592653589f);
    printf("\n");
}


/* DEFTUTORIAL EXPGROUP(TutorialExtendedData) (TutorialExtendedData_Summary, Summary) */
/* From here, you might want to explore:

   $* $TutorialPreprocessor: use the Granny preprocessor for common toolchain tasks.
   $* $TutorialTreeWalker: walk the Granny metadata structures.
   $* $TutorialCustomFormat: take full control of the granny file format
   $-
*/

/* DEFTUTORIALEND */
