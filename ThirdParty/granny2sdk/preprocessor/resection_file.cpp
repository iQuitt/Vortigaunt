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

bool ResectionFile(input_file&     InputFile,
                   key_value_pair* KeyValues,
                   granny_int32x   NumKeyValues,
                   granny_memory_arena* TempArena)
{
    RequireKey("output", "must specify an output file with \"-output <filename>\"\n");
    char const* OutputFile = FindFirstValueForKey(KeyValues, NumKeyValues, "output");
    pp_assert(OutputFile);

    granny_file_info* FileInfo = ExtractFileInfo(InputFile);
    if (FileInfo == 0)
    {
        ErrOut("unable to obtain a granny_file_info from the input model file\n");
        return false;
    }

    AttachHistoryEntry(FileInfo, TempArena);

    // Use the granny standard sections (granny_standard_section_index)
    granny_file_builder* Builder = GrannyBeginFile(GrannyStandardSectionCount,
                                                   GrannyCurrentGRNStandardTag,
                                                   GrannyGRNFileMV_ThisPlatform,
                                                   GrannyGetTemporaryDirectory(),
                                                   "Prefix");

    granny_file_data_tree_writer* Writer = CreateTreeWriter(OutputFile, FileInfo);
    pp_assert(Writer);

    // The specific section assignments that the exporter creates by default are are as
    // follows:
    //  - Vertices: vertex_data::Vertices
    //     - GrannyStandardRigidVertexSection if non-skinned
    //     - GrannyStandardDeformableVertexSection if skinned
    //
    //  - Indices: tri_topology::Indices and Indices16
    //     - GrannyStandardRigidIndexSection if non-skinned
    //     - GrannyStandardDeformableIndexSection if skinned
    //
    //  - Textures: texture::Images::MIPLevels::PixelBytes
    //     - GrannyTextureSection

    // We need to walk the meshes to set the vertex data properly, since the vertex_data
    // and tri_topology structures don't contain enough info for us to easily determine if
    // they represent skinned or non-skinned data.
    {for(granny_int32x MeshIdx = 0; MeshIdx < FileInfo->MeshCount; ++MeshIdx)
    {
        granny_mesh* Mesh = FileInfo->Meshes[MeshIdx];

        granny_int32x VertexSection, IndexSection;
        if (Mesh->BoneBindingCount <= 1)
        {
            VertexSection = GrannyStandardRigidVertexSection;
            IndexSection  = GrannyStandardRigidIndexSection;
        }
        else
        {
            VertexSection = GrannyStandardDeformableVertexSection;
            IndexSection  = GrannyStandardDeformableIndexSection;
        }

        // Set the topology...
        if (Mesh->PrimaryTopology->Indices != 0)
            GrannySetFileSectionForObject(Writer,
                                          Mesh->PrimaryTopology->Indices,
                                          IndexSection);
        if (Mesh->PrimaryTopology->Indices16 != 0)
            GrannySetFileSectionForObject(Writer,
                                          Mesh->PrimaryTopology->Indices16,
                                          IndexSection);

        // Mark the primary vertex data
        if (Mesh->PrimaryVertexData->Vertices != 0)
            GrannySetFileSectionForObject(Writer,
                                          Mesh->PrimaryVertexData->Vertices,
                                          VertexSection);

        // And the morph target data, if it exists...
        {for(granny_int32x MorphIdx = 0; MorphIdx < Mesh->MorphTargetCount; ++MorphIdx)
        {
            granny_vertex_data* MorphData = Mesh->MorphTargets[MorphIdx].VertexData;
            if (MorphData == 0)
                continue;

            if (MorphData->Vertices != 0)
                GrannySetFileSectionForObject(Writer,
                                              MorphData->Vertices,
                                              VertexSection);
        }}
    }}

    // Now walk the textures...
    {for(granny_int32x TexIdx = 0; TexIdx < FileInfo->TextureCount; ++TexIdx)
    {
        granny_texture* Texture = FileInfo->Textures[TexIdx];

        {for(granny_int32x ImageIdx = 0; ImageIdx < Texture->ImageCount; ++ImageIdx)
        {
            granny_texture_image& Image = Texture->Images[ImageIdx];

            {for(granny_int32x MIPIdx = 0; MIPIdx < Image.MIPLevelCount; ++MIPIdx)
            {
                granny_texture_mip_level& MIP = Image.MIPLevels[MIPIdx];

                GrannySetFileSectionForObject(Writer,
                                              MIP.PixelBytes,
                                              GrannyStandardTextureSection);
            }}
        }}
    }}


    // Now we can finish writing out the file
    GrannyWriteDataTreeToFileBuilder(Writer, Builder);
    GrannyEndFileDataTreeWriting(Writer);

    bool success = GrannyEndFile(Builder, OutputFile);
    if (!success)
    {
        ErrOut("failed to write to %s\n", OutputFile);
    }

    return success;
}

static const char* HelpString =
    (" ResectionFile restores default sectioning to the Granny file, which\n"
     " can be altered by preprocessing. Where possible, object sectioning\n"
     " is preserved in the default preprocessing commands, but for new data,\n"
     " the intended section is unknown. This command will apply the default\n"
     " sectioning to everything in a .gr2.\n"
     "\n"
     "    preprocessor ResectionFile char.gr2 -output char_sectioned.gr2\n");

static CommandRegistrar RegResectionFile(eSingleFile_NoMultiRun,
                                         ResectionFile,
                                         "ResectionFile",
                                         "Restores default sectioning to the Granny file",
                                         HelpString);

