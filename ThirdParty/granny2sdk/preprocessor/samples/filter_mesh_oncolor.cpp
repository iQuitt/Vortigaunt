// ========================================================================
// $File: //jeffr/granny_29/preprocessor/samples/compress_vert_sample.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <memory.h>
#include <vector>


// yes i am ashamed
char const g_zero_mem[1024] = { 0 };
inline bool
is_mem_zero(granny_uint8 const* ptr, int s)
{
    pp_assert(s < sizeof(g_zero_mem));
    return memcmp(ptr, g_zero_mem, s) == 0;
}


static void
FilterVertexData(granny_vertex_data*              VertexData,
                 std::vector<granny_int32> const& Remapping,
                 int const                        NewVertexCount,
                 granny_memory_arena*             TempArena)
{
    int           VertexSize  = GrannyGetTotalObjectSize(VertexData->VertexType);
    granny_uint8* NewVertices = PushArray(TempArena, NewVertexCount * VertexSize, granny_uint8);

    // Pretty straightforward.
    for (size_t Idx = 0; Idx < Remapping.size(); ++Idx)
    {
        if (Remapping[Idx] == -1)
            continue;

        memcpy(NewVertices + Remapping[Idx]*VertexSize,
			VertexData->Vertices + Idx*VertexSize,
			VertexSize);
    }

    VertexData->VertexCount = NewVertexCount;
    VertexData->Vertices    = NewVertices;
}

static void
FilterTopology(granny_tri_topology*             Topology,
               std::vector<granny_int32> const& OldIndices,
               std::vector<granny_int32> const& Remapping,
               int const                        NewVertexCount,
               int const                        NewIndexCount,
               granny_memory_arena*             TempArena)
{
    granny_int32* NewIndices             = PushArray(TempArena, NewIndexCount, granny_int32);
    granny_tri_material_group *NewGroups = PushArray(TempArena, Topology->GroupCount, granny_tri_material_group);

    // If all of the verts used, emit the triangle with new vertex indices.  Note that we
    // need to keep track of the material groups as we go, so we might as well loop that
    // way.
    int CurrIndex = 0;
    for (int GroupIdx = 0; GroupIdx < Topology->GroupCount; ++GroupIdx)
    {
        NewGroups[GroupIdx] = Topology->Groups[GroupIdx];

        // Reset the tri range
        NewGroups[GroupIdx].TriFirst = CurrIndex / 3;
        NewGroups[GroupIdx].TriCount = 0;

        int BaseIdx = Topology->Groups[GroupIdx].TriFirst * 3;
        int EndIdx  = BaseIdx + Topology->Groups[GroupIdx].TriCount * 3;
        for (int VIdx = BaseIdx; VIdx < EndIdx; VIdx += 3)
        {
            if (Remapping[OldIndices[VIdx + 0]] == -1 ||
                Remapping[OldIndices[VIdx + 1]] == -1 ||
                Remapping[OldIndices[VIdx + 2]] == -1)
            {
                continue;
            }
            
            NewIndices[CurrIndex + 0] = Remapping[OldIndices[VIdx + 0]];
            NewIndices[CurrIndex + 1] = Remapping[OldIndices[VIdx + 1]];
            NewIndices[CurrIndex + 2] = Remapping[OldIndices[VIdx + 2]];
            CurrIndex += 3;
            NewGroups[GroupIdx].TriCount++;
        }
    }

    // Replace the material groups...
    Topology->Groups = NewGroups;

    // And the indices
    Topology->IndexCount = NewIndexCount;
    Topology->Indices    = NewIndices;

    // Zero these out just incase the indices were stored as 16-bit
    Topology->Index16Count = 0;
    Topology->Indices16    = 0;
}



granny_file_info*
FilterMeshesOnColor(char const*          OriginalFilename,
                    char const*          OutputFilename,
                    granny_file_info*    Info,
                    key_value_pair*      KeyValues,
                    granny_int32x        NumKeyValues,
                    granny_memory_arena* TempArena)
{
    char const* ColorName = FindFirstValueForKey(KeyValues, NumKeyValues, "colorname");
    if (!ColorName)
    {
        ErrOut("Must specify the name of the vertex color attribute with -colorname 'name'\n");
        return 0;
    }

    for (int i = 0; i < Info->MeshCount; i++)
    {
        granny_mesh* Mesh = Info->Meshes[i];
        pp_assert(Mesh);

        // Here is our strategy.  We're going to go through each of the vertices, and
        // check the specified color attribute.  We use the name from max/maya/xsi encoded
        // in the "VertexComponentNames" rather than "Diffuse0", but that could change
        // easily to search both arrays.
        int FoundAttr = -1;
        for (int AttrIdx = 0; AttrIdx < Mesh->PrimaryVertexData->VertexComponentNameCount; ++AttrIdx)
        {
            if (_stricmp(Mesh->PrimaryVertexData->VertexComponentNames[AttrIdx], ColorName) == 0)
            {
                FoundAttr = AttrIdx;
                break;
            }
        }

        if (FoundAttr == -1)
        {
            WarnOut("Couldn't find attribute '%s' in mesh '%s' specification, skipping\n", ColorName, Mesh->Name);
            continue;
        }
        
        // Compute the offset and size of the member in the vertex type
        granny_data_type_definition* VertexType = GrannyGetMeshVertexType(Mesh);
        int MemberOffset = 0;
        int MemberSize   = -1;
        {
            int CurrAttr = 0;
            while (CurrAttr < FoundAttr)
            {
                MemberOffset += GrannyGetMemberTypeSize(&VertexType[CurrAttr]);
                ++CurrAttr;
            }
            MemberSize = GrannyGetMemberTypeSize(&VertexType[CurrAttr]);
        }

        granny_int32x const VertexCount = GrannyGetMeshVertexCount(Mesh);
        granny_int32x const VertexSize  = GrannyGetTotalObjectSize(VertexType);
        granny_uint8* const Vertices    = (granny_uint8*)GrannyGetMeshVertices(Mesh);
        std::vector<bool> UsedVertices(VertexCount, false);
        for (int Idx = 0; Idx < VertexCount; ++Idx)
        {
            granny_uint8* const VertexPtr = Vertices + VertexSize*Idx + MemberOffset;
            if (is_mem_zero(VertexPtr, MemberSize))
                UsedVertices[Idx] = true;
        }

        // Ok.  So now we have a list of all the vertices that have a zero color.  The
        // trick now is to take those, and include all vertices that are in a *triangle*
        // with a vertex of non-zero color.  We'll need to do this into a second array to
        // (easily) prevent cascading spurious references.
        std::vector<bool> UsedTriVertices(VertexCount, false);

        // Copy out the indices into a guaranteed uint32 format
        int NewIndexCount = 0;
        std::vector<granny_int32> TriIndices(GrannyGetMeshIndexCount(Mesh));
        GrannyCopyMeshIndices(Mesh, 4, &TriIndices[0]);
        for (size_t Idx = 0; Idx < TriIndices.size(); Idx += 3)
        {
            // any used?
            if (UsedVertices[TriIndices[Idx + 0]] ||
                UsedVertices[TriIndices[Idx + 1]] ||
                UsedVertices[TriIndices[Idx + 2]])
            {
                // Mark them all, take the triangle indices into the new mesh
                NewIndexCount += 3;
                UsedTriVertices[TriIndices[Idx + 0]] = true;
                UsedTriVertices[TriIndices[Idx + 1]] = true;
                UsedTriVertices[TriIndices[Idx + 2]] = true;
            }
        }

        // Tote up the new total vertex count and create the remapping
        int NewVertexCount = 0;
        std::vector<granny_int32> Remapping(VertexCount, -1);
        for (size_t Idx = 0; Idx < UsedTriVertices.size(); ++Idx)
        {
            if (UsedTriVertices[Idx])
            {
                Remapping[Idx] = NewVertexCount++;
            }
        }

        // Perfect.  Ok, at this point we need to filter
        //  - PrimaryVertexData
        //  - MorphTargets
        //  - PrimaryTopology
        //  - vertex annotations (left for another iteration, after validation)

        FilterVertexData(Mesh->PrimaryVertexData, Remapping, NewVertexCount, TempArena);
        FilterTopology(Mesh->PrimaryTopology, TriIndices, Remapping, NewVertexCount, NewIndexCount, TempArena);
        for (int MorphIdx = 0; MorphIdx < Mesh->MorphTargetCount; ++MorphIdx)
        {
            FilterVertexData(Mesh->MorphTargets[MorphIdx].VertexData,
                             Remapping, NewVertexCount, TempArena);
        }
    }

    return Info;
}

static CommandRegistrar RegFilterMeshesOnColor(FilterMeshesOnColor, "FilterMeshesOnColor",
                                               "Filter triangles and vertices based on a color mask");

