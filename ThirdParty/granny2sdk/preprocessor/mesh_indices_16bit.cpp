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


granny_file_info*
MeshIndices16Bit(char const*          OriginalFilename,
                 char const*          OutputFilename,
                 granny_file_info*    Info,
                 key_value_pair*      KeyValues,
                 granny_int32x        NumKeyValues,
                 granny_memory_arena* TempArena)
{
    // Optimize all meshes for the pre/post-transform cache
    {for(granny_int32x i = 0; i < Info->MeshCount; i++)
    {
        granny_mesh* Mesh = Info->Meshes[i];
        if (!Mesh)
            continue;

        if (GrannyGetMeshVertexCount(Mesh) <= (1 << 16))
        {
            // Replace the 32-bit indices with 16-bit indices
            granny_int32x  IndexCount   = GrannyGetMeshIndexCount(Mesh);
            granny_uint16* NewIndices16 =
                PushArray(TempArena, IndexCount, granny_uint16);

            GrannyCopyMeshIndices(Mesh, 2, NewIndices16);
            Mesh->PrimaryTopology->IndexCount = 0;
            Mesh->PrimaryTopology->Indices = 0;
            Mesh->PrimaryTopology->Index16Count = IndexCount;
            Mesh->PrimaryTopology->Indices16 = NewIndices16;
        }
        else
        {
            // error here potentially?
        }
    }}

    // Write out the resulting file...
    return Info;
}


static const char* HelpString =
    (" Where possible, MeshIndices16Bit will change the storage format\n"
     " of the granny_meshes contained in a gr2 file to 16-bit unsigned\n"
     " indices.  This can save a significant amount of space in the\n"
     " file, and at runtime\n"
     "\n"
     "    preprocessor MeshIndices16Bit mesh.gr2 -output mesh_16.gr2\n");

static CommandRegistrar RegMeshIndices16Bit(MeshIndices16Bit,
                                            "MeshIndices16Bit",
                                            "Converts Meshes to 16-bit indices where possible (i.e, vert counts of less than 65536)",
                                            HelpString);

