// =======================================================================================
// basic_loading.cpp: Demonstrates loading a Granny File
//
// To keep things simple, we're just going to load the specified file, and print a few basic
// statistics about it.
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "granny.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

void PrintFileStatistics(granny_file*);
void PrintFileInfoStatistics(granny_file_info*);



/* DEFTUTORIAL EXPGROUP(TutorialBasicLoading) (TutorialBasicLoading_Introduction, Introduction) */
/* This tutorial shows you how to load a Granny file, and dump some basic information
   about it.  Granny provides a completely structure-driven interface to its files, so
   there are no accessor calls to remember, no strange RIFF-style parsing that occurs; you
   just get back a pointer to the in-memory version of the file, and all the data is
   accessible by walking the structures directly.  Pointer fixup, endian-swapping, and
   version upgrading are all handled internally, so once you've obtained the $file and
   $file_info structures, you can access the data directly on any platform.

   To keep the example small, since this is probably the first thing you're looking at,
   we're not going to exhaustively walk the data tree.  In later examples (such as:
   $TutorialTreeWalker), we'll show you how to use Granny's object metadata to process
   every last bit of data in the file.  You should note that while we're loading a
   standard Granny file here, nothing prevents you from creating your own file format, and
   using the same Granny API calls to load, process, and platform convert your own custom
   structures.  We'll show you how to do that in $TutorialCustomFormat.

   Prerequisites to this demo:
   $* None.
   $-

   New functions and structures discussed in this tutorial:
   $* $file
   $* $file_info
   $* $ReadEntireFile
   $* $GetFileInfo
   $* $FreeFile
   $-

   Functions and structures introduced incidentally:
   $* $grn_section
   $* $GetGRNSectionArray
   $* $FreeFileSection
   $* $art_tool_info
   $-

   Function index:
   $* $TutorialBasicLoading_main
   $* $TutorialBasicLoading_PrintFileStatistics
   $* $TutorialBasicLoading_PrintFileInfoStatistics
   $-
*/


/* DEFTUTORIAL EXPGROUP(TutorialBasicLoading) (TutorialBasicLoading_main, main) */
int main(int argc, char const* argv[])
{
    /* The very first thing any Granny app should do is make sure the .h file they used during
       compilation matches the DLL they're running with.  This macro does it automatically. */
    if(!GrannyVersionsMatch)
    {
        printf("Warning: the Granny DLL currently loaded "
               "doesn't match the .h file used during compilation\n");
        return EXIT_FAILURE;
    }

    // Take the default file unless an argument was passed
    char const* GrannyFilename = "media/gryphon.gr2";
    if (argc == 2)
    {
        GrannyFilename = argv[1];
    }


    /* $ReadEntireFile allocates the memory required for the file, and loads it from disk,
       decompressing it if necessary. */
    granny_file* GrannyFile = GrannyReadEntireFile(GrannyFilename);
    if (GrannyFile == 0)
    {
        printf("Error: unable to load %s as a Granny file.\n", GrannyFilename);
        return EXIT_FAILURE;
    }
    else
    {
        PrintFileStatistics(GrannyFile);
    }


    /* $GetFileInfo  allocates the memory required for the file, and loads it from disk,
       decompressing it if necessary. */
    granny_file_info* GrannyFileInfo = GrannyGetFileInfo(GrannyFile);
    if (GrannyFileInfo == 0)
    {
        printf("Could not extract a granny_file_info from the file.\n");
        return EXIT_FAILURE;
    }
    else
    {
        PrintFileInfoStatistics(GrannyFileInfo);
    }


    /* We don't need to delete GrannyFileInfo, but it is necessary to free the $file we
       obtained from $ReadEntireFile.  Note that after the call to $FreeFile, and
       structures that originated in this file are invalid, so you need to make sure that
       all models, animations, textures, etc. are cleaned up before you release the
       memory */
    GrannyFreeFile(GrannyFile);
    GrannyFileInfo = NULL;
    GrannyFile     = NULL;

    return EXIT_SUCCESS;
}


/* DEFTUTORIAL EXPGROUP(TutorialBasicLoading) (TutorialBasicLoading_PrintFileStatistics, PrintFileStatistics) */
/* It's not often necessary to access the fields in the $file, since you'll mostly be
   working with the $file_info to obtain the structures necessary to create animations and
   so forth.  However, if you're writing tools that meddle in the internals of the
   exported files, you can always access all of the relevant info that Granny itself works
   with.

   This function simply dumps some of the information to demonstrate the level of detail
   you can obtain if it's necessary. */
void PrintFileStatistics(granny_file* GrannyFile)
{
    assert(GrannyFile);
    printf("granny_file\n-----------\n");

    /* Each Granny file contains 1 or more sections that may be loaded, freed, and
       compressed independantly.  By default, the exporter creates several, in order to
       try to segregate ephemeral data that is loaded into the rendering API (for
       instance, vertex buffers and textures) from data that Granny needs to access at
       runtime.  This allows you to release the memory for the ephemeral data if you need
       to save on memory at runtime.  Use the $FreeFileSection API to do this.
     */
    granny_grn_section* SectionArray = GrannyGetGRNSectionArray(GrannyFile->Header);
    assert(SectionArray);

    printf("File contains: %d sections.\n", GrannyFile->SectionCount);
    {for(granny_int32x Section = 0; Section < GrannyFile->SectionCount; ++Section)
    {
        if ( GrannyFile->Sections[Section])
        {
            printf("  Section %d: present", Section);

            // DataSize is the sizeof the data on disk, ExpandedDataSize is the size in memory.  If
            // they match, this section was written without Oodle1 compression.
            if (SectionArray[Section].DataSize ==
                SectionArray[Section].ExpandedDataSize)
            {
                printf(" (uncompressed)\n");
            }
            else
            {
                printf(" (compressed)\n");
            }
        }
        else
        {
            // This will likely never happen, since we haven't called GrannyFreeFileSection
            printf("  Section %d: already freed (or empty)\n", Section);
        }
    }}

    printf("\n");
}

/* DEFTUTORIAL EXPGROUP(TutorialBasicLoading) (TutorialBasicLoading_PrintFileInfoStatistics, PrintFileInfoStatistics) */
/* For a standard Granny file, the $file_info holds all of the animations, models,
   textures, etc.  We're going to dump some basic information about the data, for
   instance, which tool created it, the version of Granny that was used to create the
   file, and how many animations, models, etc. are present.

   It is possible to go into far greater detail than we're going to here, see
   $TutorialTreeWalker for an example that demonstrates how to walk the extended data
   fields and dump the metadata for the file. */
void PrintFileInfoStatistics(granny_file_info* GrannyFileInfo)
{
    assert(GrannyFileInfo);
    printf("granny_file_info\n----------------\n");

    /* If the $art_tool_info is present, show some relevant facts about the file.  See
       $TutorialTransformFile for details on how Granny uses this information to compute
       automatic basis conversion transforms. */
    if (GrannyFileInfo->ArtToolInfo)
    {
        granny_art_tool_info* ToolInfo = GrannyFileInfo->ArtToolInfo;

        printf("File created by %s (%d.%d)\n",
               ToolInfo->FromArtToolName,
               ToolInfo->ArtToolMajorRevision,
               ToolInfo->ArtToolMinorRevision);
        printf("  Coordinate system\n"
               "    Units per meter: %f\n"
               "       Right Vector: (%f %f %f)\n"
               "          Up Vector: (%f %f %f)\n"
               "        Back Vector: (%f %f %f)\n\n",
               ToolInfo->UnitsPerMeter,
               ToolInfo->RightVector[0], ToolInfo->RightVector[1], ToolInfo->RightVector[2],
               ToolInfo->UpVector[0],    ToolInfo->UpVector[1],    ToolInfo->UpVector[2],
               ToolInfo->BackVector[0],  ToolInfo->BackVector[1],  ToolInfo->BackVector[2]);
    }
    else
    {
        printf("No Art tool information, GrannyComputeBasisConversion won't work on this file.\n");
    }

    /* The ExporterInfo member of $file_info encodes information about the version of Granny
       that created this file.  Note that you can remove this member with the preprocessor
       or your own tools without affecting Granny's ability to load files created with
       older versions of the API. */
    if (GrannyFileInfo->ExporterInfo)
    {
        printf("File created with version %d.%d.%d.%d of Granny.\n",
               GrannyFileInfo->ExporterInfo->ExporterMajorRevision,
               GrannyFileInfo->ExporterInfo->ExporterMinorRevision,
               GrannyFileInfo->ExporterInfo->ExporterBuildNumber,
               GrannyFileInfo->ExporterInfo->ExporterCustomization);
    }

    /* FromFileName encodes the name of the source file that was used to create the file.
       For instance, in the case of the default file, Gryphon.mb, which is the Maya file
       containing the model for the Gryphon example model. */
    if (GrannyFileInfo->FromFileName)
    {
        printf("Source file: %s\n\n", GrannyFileInfo->FromFileName);
    }

    /* Now dump the number of some key structures contained in the $file_info.  Note that
       for convenience, several more arrays are exposed at the $file_info level.  For
       instance, $(skeleton)s can be found in this structure.  We're not dumping these
       because they are slightly redundant, you can find them as sub-members of the major
       structures.  Skeletons can be found as sub-members of the Models, for example. */
    printf("Number of    Textures: %d\n"
           "            Materials: %d\n"
           "               Meshes: %d\n"
           "               Models: %d\n"
           "           Animations: %d\n",
           GrannyFileInfo->TextureCount,
           GrannyFileInfo->MaterialCount,
           GrannyFileInfo->MeshCount,
           GrannyFileInfo->ModelCount,
           GrannyFileInfo->AnimationCount);
    printf("\n");
}

/* DEFTUTORIAL EXPGROUP(TutorialBasicLoading) (TutorialBasicLoading_Summary, Summary) */
/* You should now have a grasp of the basics of loading Granny files.  From here, there
   are a few logical places to go next.

   $* $TutorialCreateControl: bind an animation to model for playback
   $* $TutorialCustomFormat or $TutorialTreeWalker: create custom formats, and walk the
   granny metadata structures.
   $* $TutorialTransformFile: convert Granny files into your application's coordinate
   system.
   $* $TutorialReplaceFileAPI: override Granny's disk access mechanisms
   $-
*/

/* DEFTUTORIALEND */
