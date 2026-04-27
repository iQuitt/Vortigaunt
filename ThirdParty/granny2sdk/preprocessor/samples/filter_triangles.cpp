// ========================================================================
// $File: //jeffr/granny_29/preprocessor/samples/merge_rigid_meshes.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"
#include <stdio.h>
#include <string.h>
#include <memory.h>
#include <assert.h>

#include <vector>
using namespace std;


// Very silly predicate, intended to be replaced.
// If the triangle crosses the y == 40 line, remove it.
static bool
TrianglePredicate(granny_real32 const* Vert0,
                  granny_real32 const* Vert1,
                  granny_real32 const* Vert2)
{
    int AboveCount = 0;
    if (Vert0[1] > 40) ++AboveCount;
    if (Vert1[1] > 40) ++AboveCount;
    if (Vert2[1] > 40) ++AboveCount;

    return (AboveCount != 1 && AboveCount != 2);
}

granny_file_info*
FilterTriangles(char const*          OriginalFilename,
                char const*          OutputFilename,
                granny_file_info*    Info,
                key_value_pair*      KeyValues,
                granny_int32x        NumKeyValues,
                granny_memory_arena* TempArena)
{
    for (int MeshIdx = 0; MeshIdx < Info->MeshCount; ++MeshIdx)
    {
        granny_mesh* Mesh = Info->Meshes[MeshIdx];
        if (!Mesh)
            continue;

        // Doesn't handle morphs
        if (Mesh->MorphTargetCount != 0)
        {
            ErrOut("doesn't handle morph targets yet");
            return 0;
        }

        // We could loop through the indices directly, but we'll stick with the group
        // structure, since it makes it easier to track the TriCount members
        granny_vertex_data*  VData = Mesh->PrimaryVertexData;
        granny_tri_topology* Topo  = Mesh->PrimaryTopology;

        vector<granny_p3_vertex> PosVerts(VData->VertexCount);
        GrannyCopyMeshVertices(Mesh, GrannyP3VertexType, &PosVerts[0]);

        // Force 4-byte indices for the scan.
        vector<granny_int32> Indices(GrannyGetMeshIndexCount(Mesh));
        GrannyCopyMeshIndices(Mesh, 4, &Indices[0]);

        vector<granny_int32> ResultIndices;
        ResultIndices.reserve(GrannyGetMeshIndexCount(Mesh));

        for (int Idx = 0; Idx < Topo->GroupCount; ++Idx)
        {
            granny_tri_material_group& Group = Topo->Groups[Idx];

            granny_int32x TriFirst = Group.TriFirst;
            granny_int32x TriCount = Group.TriCount;
            for (int TriIdx = 3 * TriFirst; TriIdx < 3 * (TriFirst + TriCount); TriIdx += 3)
            {
                if (TrianglePredicate(PosVerts[Indices[TriIdx + 0]].Position,
                                      PosVerts[Indices[TriIdx + 1]].Position,
                                      PosVerts[Indices[TriIdx + 2]].Position))
                {
                    // Keep it
                    ResultIndices.push_back(Indices[TriIdx + 0]);
                    ResultIndices.push_back(Indices[TriIdx + 1]);
                    ResultIndices.push_back(Indices[TriIdx + 2]);
                }
                else
                {
                    // Leave those verts out, dec tricount
                    --Group.TriCount;
                }
            }
        }

        // Fix up the "TriFirst" entries in the Groups
        {
            granny_int32 Current = 0;
            for (int Idx = 0; Idx < Topo->GroupCount; ++Idx)
            {
                Topo->Groups[Idx].TriFirst = Current;
                Current += Topo->Groups[Idx].TriCount;
            }
        }

        // Ok, so ResultIndices contains the triangles we want to keep.  We potentially
        // have some holes in the vertex data array, but a pass with VertexCacheOptimize
        // will fix that up if required...
        if (Topo->Indices != 0)
        {
            // We can stuff the 4 byte indices back in.  We know there's enough space,
            // since ResultIndices is strictly <= Indices in size...
            assert(ResultIndices.size() <= Indices.size());
            assert((ResultIndices.size() % 3) == 0);
            assert(Topo->Index16Count == 0);
            assert(Topo->Indices16 == 0);

            memcpy(Topo->Indices, &ResultIndices[0], ResultIndices.size() * sizeof(granny_int32));
            Topo->IndexCount = (granny_int32)ResultIndices.size();
        }
        else
        {
            // Gotta convert into 16 bit.  Just loop through, again, we know there's space...
            for (size_t Idx = 0; Idx < ResultIndices.size(); ++Idx)
            {
                assert(ResultIndices[Idx] >= 0 && ResultIndices[Idx] <= 0xffff);
                Topo->Indices16[Idx] = (granny_uint16)ResultIndices[Idx];
            }

            Topo->Index16Count = (granny_int32)ResultIndices.size();
        }
    }

    return Info;
}

static CommandRegistrar RegCompressVertSample(FilterTriangles, "FilterTriangles",
                                              "Removes triangles from the file that fail a predicate");

