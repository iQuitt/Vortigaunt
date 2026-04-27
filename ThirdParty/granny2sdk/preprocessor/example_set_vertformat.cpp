// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "preprocessor.h"
#include <stdio.h>


char const* PWNT3232VertexComponentNames[] = {
    GrannyVertexPositionName,
    GrannyVertexBoneWeightsName,
    GrannyVertexBoneIndicesName,
    GrannyVertexNormalName,
    GrannyVertexTextureCoordinatesName "0",
};
int const PWNT3232NumVertexComponents = 5;

bool ExampleSetVertFormat(input_file&     InputFile,
                          key_value_pair* KeyValues,
                          granny_int32x   NumKeyValues,
                          granny_memory_arena* TempArena);


static const char* HelpString =
    (" ExampleSetVertFormat is the tutorial for demonstrating\n"
     " how to create a custom preprocessor command.  Please read\n"
     " the description in the Granny documentation for more\n"
     " information\n");

/* DEFTUTORIAL EXPGROUP(TutorialPreprocessor) (TutorialPreprocessor_Introduction, Introduction) */
/* This tutorial will show you how to create custom operations that integrate with the
   Granny preprocessor to make it a full-fledged member of your toolchain.  The default
   commands in the preprocessor cover a wide range of the transformations you might want
   to apply to your Granny files, but we can't anticipate everything.  The preprocessor is
   designed to allow you to easily create and add your own operations to the base
   executable.  Most of the boilerplate operations (loading the input files, parsing the
   command-line, etc.) are handled for you, which allows you to create small, easy to
   maintain routines that automate common tasks.  (Some of this material is also covered
   in $Preprocessor, including detailed instructions for invoking the tool, and
   integrating your new commands, which we'll omit in this tutorial.)

   The operation we've chosen to illustrate here is normalizing the vertex format of the
   $mesh data in an input file.  You can also do this at runtime with $CopyMeshVertices
   obviously, but why waste time on that routine when you can simply make sure that every
   input file has the expected vertex format automatically in your toolchain?  Unexpected
   vertex formats can slip into the data stream in a number of ways.  Artists can
   accidentally request too many texture coordinates or color channels in the exporter,
   tangent space might be included that you don't want in your runtime, or you may use the
   extra data for preprocessing earlier in the toolchain, and you simply don't need it
   anymore.  In any event, after running this command, you can be certain that all of your
   $(mesh)es share a known vertex format.

   To keep things simple, we're only going to change the vertex format of skinned meshes,
   i.e, those for which $MeshIsRigid returns false.  In this demo, $pwnt3232_vertex is
   chosen as the output format, but you can easily modify this to select any of the Granny
   vertex types, or your own custom vert structure.  This is especially handy if you want
   to compress the texture coordinates to 16-bit integers, or compress the normals to a
   11-11-10 compressed format, or in general, perform any data transformation that Granny
   doesn't directly support.
   $-
   Prerequisites to this demo:
   $* $TutorialBasicLoading
   $-

   New functions and structures discussed in this tutorial:
   $* $file_builder
   $* $file_data_tree_writer
   $* $memory_arena
   $* $BeginFile
   $-

   New functions and structures incidental to this tutorial
   $* $ConvertVertexLayouts
   $* $OneNormalizeWeights
   $-

   Function index:
   $* $TutorialPreprocessor_ExampleSetVertFormat
   $-
*/


/* DEFTUTORIAL EXPGROUP(TutorialPreprocessor) (TutorialPreprocessor_ExampleSetVertFormat, ExampleSetVertFormat) */
/* A preprocessor command takes only 3 inputs, input file(s), a list of key value
   pairs that you use to setup the command options, and a $memory_arena as a convenience,
   to allow you to allocate temporary memory that will be freed after the command
   completes.  Typical operation for a command is to take one input file, and produce one
   output file, but you can certainly create commands that work with multiple input files.
   (See the CombineElements command for an example.)  The convention adopted by the
   default Granny commands for functions of this sort is to require an "-output" key/value
   pair to specify where the modified file should be written.
   $-

   Note that the inputs to your command are $file objects, not $(file_info)s.  This is so
   you can pass your own custom files through preprocessor commands.  The driver doesn't
   force you to try to stuff everything into a $file_info.  If you you're working with
   vanilla Granny files (as we are here), just call $GetFileInfo on them.  You can find
   more infomation on storing and processing your own data using the Granny file API in
   $TutorialCustomFormat.

   KeyValues and NumKeyValues specify the options that were passed on the command line as
   "-key value" pairings.  In our example, the only pair we care about is the "output"
   key, so we verify that it's present, and ignore any extra parameters.

   For our example, we're showing a command that works on a single file at a time, and
   cannot be called multiple times with the same key/value list.  (Because we're using an
   -output key to specify the destination filename, rather than a string transformation or
   another method.)  The CommandRegistrar object below tells the preprocessor driver about
   these properties so that error handling and warnings can be handled by the driver,
   rather than duplicated here.  It is possible to create commands that take multiple
   input files (for example, the CombineElements command), or take a single file, but may
   be called multiple times with the same argument list (for example, the WarnScaleShear
   command).  Check those modules to see how to setup those command types.
*/

// Uncomment to enable this command
static CommandRegistrar RegExampleSetVertFormat(eSingleFile_NoMultiRun,
                                                ExampleSetVertFormat,
                                                "ExampleSetVertFormat",
                                                "Sets the format of any skinned meshes to granny_pwnt3232_vertex", HelpString);


bool ExampleSetVertFormat(input_file&     InputFile,
                          key_value_pair* KeyValues,
                          granny_int32x   NumKeyValues,
                          granny_memory_arena* TempArena)
{

    /* Validate the input.  Our requirements for this command:
       $* -output key must be present
       $* The input $file must contain a valid $file_info object
       $-
    */
    RequireKey("output", "must specify an output file with \"-output <filename>\"\n");

    /* Note that normally you would call $GetFileInfo, but in the preprocessor, there is a
       small utility that helps when you are using Granny to build a game that supports
       user mods.  You may safely ignore this in almost all cases.  Either line below will
       work fine. */
    // granny_file_info* Info = GrannyGetFileInfo(InputFile.GrannyFile);
    granny_file_info* Info = ExtractFileInfo(InputFile);
    if (Info == 0)
    {
        ErrOut("unable to obtain a granny_file_info from the input file\n");
        return false;
    }

    /* Now we loop through the meshes, finding skinned data, and apply our modifier.
       You'll notice that as the loop progresses, we actually simply overwrite the
       original pointer values in the $file_info.  Because of the way Granny loads and
       allocates files, there is no need to free, or notify Granny that this has happened.
       The memory is freed in a block when the $file is destroyed. */
    for (int i = 0; i < Info->MeshCount; i++)
    {
        granny_mesh* Mesh = Info->Meshes[i];
        pp_assert(Mesh);

        /* When we find a skinned mesh, allocate our array of converted vertices from the
           input $memory_arena.  An arena is essentially just a controllable heap that you
           can push allocations onto, and free all at once.  The preprocessor.h header
           provide the helper macros ArenaPushArray and ArenaPushObject to make using the
           temp arena a little easier.  The driver takes care of cleaning up the arena, so
           we can just continue to generate new allocations without worrying about
           tracking the memory for clean up. */
        if (!GrannyMeshIsRigid(Mesh))
        {
            {
                granny_int32x const VertexCount = GrannyGetMeshVertexCount(Mesh);
                granny_pwnt3232_vertex* NewVertices =
                    PushArray(TempArena, VertexCount, granny_pwnt3232_vertex);

                /* The meat of the command.  Note that if you are changing the number of
                   weights, you should always call $OneNormalizeWeights to make sure that the
                   weights of each vert sum to 1. */
                GrannyConvertVertexLayouts(VertexCount,
                                           GrannyGetMeshVertexType(Mesh),
                                           GrannyGetMeshVertices(Mesh),
                                           GrannyPWNT3232VertexType,
                                           NewVertices);
                GrannyOneNormalizeWeights(VertexCount,
                                          GrannyPWNT3232VertexType,
                                          NewVertices);

                /* Note that I've hardcoded the names of the vertex components, since we know
                   which format we're using, but you can easily make this dynamic. */
                Mesh->PrimaryVertexData->VertexType = GrannyPWNT3232VertexType;
                Mesh->PrimaryVertexData->Vertices   = (granny_uint8*)NewVertices;
                Mesh->PrimaryVertexData->VertexComponentNameCount = PWNT3232NumVertexComponents;
                Mesh->PrimaryVertexData->VertexComponentNames     = PWNT3232VertexComponentNames;
            }

            // We can leave the annotation and the VertexCount fields
            // alone, they don't change.

            /* Do the same thing to the morph target's vertex datas.*/
            for (int MorphIdx = 0; MorphIdx < GrannyGetMeshMorphTargetCount(Mesh); MorphIdx++)
            {
                granny_int32x const VertexCount = GrannyGetMeshMorphVertexCount(Mesh,MorphIdx);
                granny_pwnt3232_vertex* NewVertices =
                    PushArray(TempArena, VertexCount, granny_pwnt3232_vertex);

                /* The meat of the command.  Note that if you are changing the number of
                   weights, you should always call $OneNormalizeWeights to make sure that the
                   weights of each vert sum to 1. */
                GrannyConvertVertexLayouts(VertexCount,
                                           GrannyGetMeshMorphVertexType(Mesh,MorphIdx),
                                           GrannyGetMeshMorphVertices(Mesh,MorphIdx),
                                           Mesh->PrimaryVertexData->VertexType,
                                           NewVertices);
                GrannyOneNormalizeWeights(VertexCount,
                                          Mesh->PrimaryVertexData->VertexType,
                                          NewVertices);

                /* Note that I've hardcoded the names of the vertex components, since we know
                   which format we're using, but you can easily make this dynamic. */
                Mesh->MorphTargets[MorphIdx].VertexData->VertexType = Mesh->PrimaryVertexData->VertexType;
                Mesh->MorphTargets[MorphIdx].VertexData->Vertices   = (granny_uint8*)NewVertices;
                Mesh->MorphTargets[MorphIdx].VertexData->VertexComponentNameCount = Mesh->PrimaryVertexData->VertexComponentNameCount;
                Mesh->MorphTargets[MorphIdx].VertexData->VertexComponentNames     = Mesh->PrimaryVertexData->VertexComponentNames;
            }
        }
    }

    /* Ok, all of the skinned meshes have been modified, let's write out the new file.
       There are a couple of ways to do this, but the simplest is to use a $file_builder
       object.  You simply specify the desired number of file sections, the version tag to
       apply so Granny knows if the file needs to be updated when loaded by a new version
       of the library (or your game, in the case of custom file formats).  Here we're
       going to write everything into a single section, rather than try to guess what the
       correct object placement strategy should be.  You can always restore the default
       sectioning created by the exporter by running the ResectionFile command.

       The other important argument to $BeginFile is the magic value that tells Granny
       which platform to optimize the file for.  (The full list is in the docs under
       $grn_file_magic_value.)  In typical operation, you would simply pass
       $GRNFileMV_ThisPlatform, which writes the file out with the pointer-size and
       endianess of the tool platform, but you can easily specify that the file should be
       setup for no-fixup loading on the Xenon, or Win64 by selecting the correct platform
       value here.  See the "PlatformConvert" command for an example.  In this case, we'll
       preserve whatever the platform was for the file on input. */
    char const* OutputFile = FindFirstValueForKey(KeyValues, NumKeyValues, "output");
    if (OutputFile == 0)
    {
        ErrOut("must specify an output file with \"-output <filename>\"\n");
        return false;
    }

    /* Next, we're going to use the WriteInfoPreserve interface to output the file simply,
       but you could use a $file_data_tree_writer to flatten and encode the $file_info
       hierarchy with full control.  If you need to tightly control sectioning, or other
       output options, here's the place.  (See $FileManipulation for some examples.) */
    return WriteInfoPreserve(KeyValues, NumKeyValues, Info, InputFile.GrannyFile,
                             true, true, true, TempArena);

    // granny_file_builder *Builder =
    //     GrannyBeginFile(GrannyStandardSectionCount, GrannyCurrentGRNStandardTag,
    //                     InputFile.GrannyFile->SourceMagicValue,
    //                     GrannyGetTemporaryDirectory(),
    //                     "Prefix");
    //
    // granny_file_data_tree_writer *Writer =
    //     GrannyBeginFileDataTreeWriting(GrannyFileInfoType, Info,
    //                                    GrannyStandardDiscardableSection, 0);
    // GrannyWriteDataTreeToFileBuilder(Writer, Builder);
    // GrannyEndFileDataTreeWriting(Writer);
    //
    // bool success = GrannyEndFile(Builder, OutputFile);
    // if (!success)
    // {
    //     ErrOut("failed to write to %s\n", OutputFile);
    // }
    // return success;
}

/* DEFTUTORIAL EXPGROUP(TutorialPreprocessor) (TutorialPreprocessor_Summary, Summary) */
/* That's pretty much it!  The driver is responsible for application cleanup, so you only
   have to worry about the operations that are important to your tool chain.  You should
   now be able to mold the preprocessor to fit into whatever you need for your toolchain.
   For more interesting data paths, explore some of the other default commands.  For
   instance, CombineElements takes multiple input files, pastes together certain pieces of
   the input files, and creates a single output.  WarnScaleShear produces no output, but
   can warn artists that they've included scale/shear components in either the skeleton or
   the animation if your run-time engine doesn't support that feature.  LODCalc takes an
   animation and a model, and computes the sampled Animation LOD (see: $AnimationLod) for
   the pairing.
   $-
   From here, you may want to check out:
   $* $TutorialCustomFormat or $TutorialTreeWalker: create custom formats, and walk the
   granny metadata structures.
   $-
*/

/* DEFTUTORIALEND */
