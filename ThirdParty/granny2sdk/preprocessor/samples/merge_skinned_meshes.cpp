// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"

#include <memory.h>
#include <stdio.h>
#include <string.h>

#include <vector>
#include <set>
#include <string>
#include <algorithm>
#include <functional>

using namespace std;

static bool
MaterialsCompatible(granny_material* One, granny_material* Two)
{
    // Super basic for now.  If these meshes are coming from separate files, it might pay
    // off to do a more thorough comparison to see if two materials with separate pointers
    // are actually equivalent...
    return (One == Two);
}

static bool
MeshesCompatible(granny_mesh* One, granny_mesh* Two)
{
    // No skinned meshes for now
    if (GrannyMeshIsRigid(One) || GrannyMeshIsRigid(Two))
        return false;

    // We need to ensure that the vertex types match
    if (!GrannyDataTypesAreEqualWithNames(GrannyGetMeshVertexType(One),
                                          GrannyGetMeshVertexType(Two)))
    {
        return false;
    }

    return true;
}

struct CmpIStr : public binary_function<char const*, char const*, bool>
{
    bool operator()(char const* One, char const* Two) const
    {
        return _stricmp(One, Two) < 0;
    }
};

struct EqIStr : public binary_function<char const*, char const*, bool>
{
    bool operator()(char const* One, char const* Two) const
    {
        return _stricmp(One, Two) == 0;
    }
};


static void
RemapVertexBoneIndices(granny_uint8* Vertices,
                       granny_int32x VertCount,
                       granny_int32x VertexSize,
                       granny_data_type_definition* VertexType,
                       vector<int> const& Remaps)
{
    // Find the offset of the BoneIndices field in the first vertex.  We can then just
    // step forward to the next vert
    granny_variant Variant;
    if (!GrannyFindMatchingMember(VertexType, Vertices,
                                  GrannyVertexBoneIndicesName, &Variant))
    {
        // That's really bad, was this not skinned after all?
        ErrOut("Big trouble in little function!");
        pp_assert(false);
        return;
    }

    // Only handle bytes for the moment...
    pp_assert(Variant.Type[0].Type == GrannyUInt8Member);

    // 0 default should not show up here, but guard anyways.
    int NumIndices = Variant.Type[0].ArrayWidth ? Variant.Type[0].ArrayWidth : 1;

    granny_uint8* BoneIndicesBase = (granny_uint8*)Variant.Object;
    {for (int Vert = 0; Vert < VertCount; ++Vert)
    {
        granny_uint8* TheseIndices = BoneIndicesBase;
        {for (int Idx = 0; Idx < NumIndices; ++Idx)
        {
            // paranoia...
            pp_assert(TheseIndices[Idx] < int(Remaps.size()));
            pp_assert(Remaps[TheseIndices[Idx]] < 256);

            TheseIndices[Idx] = (granny_uint8)Remaps[TheseIndices[Idx]];
        }}

        // Step to the next vert
        BoneIndicesBase += VertexSize;
    }}
}


static granny_mesh*
CreateMergedMesh(vector<granny_mesh*>& MergeList,
                 granny_memory_arena* TempArena)
{
    // The first thing we need to do is to create a list of the unique bones touched by
    // these meshes.  We'll STL it up just to keep it simple
    typedef vector<char const*> BoneList;
    typedef set<char const*, CmpIStr> BoneSet;

    BoneList MergedBoneNames;
    {
        BoneSet BoneNames;
        {for (size_t Idx = 0; Idx < MergeList.size(); ++Idx)
        {
            granny_mesh* Mesh = MergeList[Idx];
            {for (int BIdx = 0; BIdx < Mesh->BoneBindingCount; ++BIdx)
            {
                BoneNames.insert(Mesh->BoneBindings[BIdx].BoneName);
            }}
        }}

        {for(BoneSet::iterator itr = BoneNames.begin(); itr != BoneNames.end(); ++itr)
        {
            MergedBoneNames.push_back(*itr);
        }}
    }

    // Not *technically* a problem, but most vertex destination formats have only a byte
    // for the index field, which means that this is troublesome.
    if (MergedBoneNames.size() > 256)
    {
        ErrOut("This will take some work to support properly.");
        return false;
    }

    // Next, a mapping from the mesh-to-be-merged's bone indices to the global unique list
    // vector<vector<>> is crazy, but hey!  Reserve the correct number so at least the
    // copies aren't causing problems.
    vector< vector< int > > BoneRemaps(MergeList.size());
    {for (size_t Idx = 0; Idx < MergeList.size(); ++Idx)
    {
        granny_mesh* Mesh   = MergeList[Idx];
        vector<int>& Remaps = BoneRemaps[Idx];
        Remaps.resize(Mesh->BoneBindingCount, -1);

        {for (int BIdx = 0; BIdx < Mesh->BoneBindingCount; ++BIdx)
        {
            char const* BoneName = Mesh->BoneBindings[BIdx].BoneName;
            BoneList::iterator itr = find_if(MergedBoneNames.begin(),
                                             MergedBoneNames.end(),
                                             bind2nd(EqIStr(), BoneName));
            pp_assert(itr != MergedBoneNames.end());

            Remaps[BIdx] = int(itr - MergedBoneNames.begin());
        }}
    }}

    // Ok, now the fun part.  We can now allocate all of our vertices and indices.  The
    // basic structure here will be in two passes.  First, we'll put all of the meshes
    // together into separate material groups, remapping the bone indices to the global
    // list.  Next, we'll run a pass over the separated vertex/index buffers, combining
    // those meshes with compatible materials.  Finally, we'll dump all of that into the
    // mesh.
    int const VertexSize = GrannyGetTotalObjectSize(GrannyGetMeshVertexType(MergeList[0]));

    // Again, vector<vector<>> is insane, but it keeps things simple.
    vector< vector<granny_uint8> >  RemappedMeshVertices(MergeList.size());
    vector< vector<granny_int32> > MeshIndices(MergeList.size());
    {for (granny_uint32 Idx = 0; Idx < MergeList.size(); ++Idx)
    {
        granny_mesh* Mesh = MergeList[Idx];

        int const VertCount  = GrannyGetMeshVertexCount(Mesh);
        int const IndexCount = GrannyGetMeshIndexCount(Mesh);

        vector<int>           Remaps   = BoneRemaps[Idx];
        vector<granny_uint8>& Vertices = RemappedMeshVertices[Idx];
        vector<granny_int32>& Indices  = MeshIndices[Idx];

        Vertices.resize(VertCount * VertexSize);
        Indices.resize(IndexCount);

        memcpy(&Vertices[0], GrannyGetMeshVertices(Mesh), VertexSize * VertCount);
        GrannyCopyMeshIndices(Mesh, 4, &Indices[0]);

        // Here is the slightly tricky part.  We need to locate the "BoneIndices" member
        // of this vertex type, extract the weight count, and then perform the mapping.
        // For simplicity, I'm lofting that out into it's own function which doesn't share
        // state across meshes.  This could be optimized, since we've already required
        // that the vertex types are identical across merged meshes.  Probably not a big
        // deal though.
        RemapVertexBoneIndices(&Vertices[0],
                               VertCount, VertexSize,
                               GrannyGetMeshVertexType(Mesh),
                               BoneRemaps[Idx]);
    }}

    // Almost there!  At this point, we have:
    //  - Final vertices
    //  - Provisional indices

    // Now, let's examine the meshes to see if any of them have compatible materials.
    // We'll combine them at this stage, which will be substantially easier.
    //
    // Note that at this point, we're going to basically abandon the idea of the input
    // source meshes, and move to material groups.  So pull the material bindings out of
    // the source meshes.
    vector< granny_uint8 >     FinalVertices;
    vector< granny_material* > MeshMaterials;
    vector< vector<int> >      MaterialGroupIndices;
    {
        // Size up the group indices array so we don't copy them unnecessarily.  Get the
        // size of the final vertex buffer at the same time.
        int TotalMaterialGroups = 0;
        int TotalVertexSize = 0;
        {for (size_t Idx = 0; Idx < MergeList.size(); ++Idx)
        {
            granny_mesh* Mesh = MergeList[Idx];
            pp_assert(Mesh->MaterialBindingCount > 0);  // uh...

            TotalMaterialGroups += Mesh->MaterialBindingCount;
            TotalVertexSize += int(RemappedMeshVertices[Idx].size());
        }}
        MaterialGroupIndices.reserve(TotalMaterialGroups);

        // Start putting the vertices together, and creating the new separated indices
        int CurrentStartVertex = 0;
        {for (size_t Idx = 0; Idx < MergeList.size(); ++Idx)
        {
            granny_mesh* Mesh = MergeList[Idx];
            for (int Mat = 0; Mat < Mesh->MaterialBindingCount; ++Mat)
            {
                MeshMaterials.push_back(Mesh->MaterialBindings[Mat].Material);

                vector<int> GroupIndices;
                // In practice, the material groups are going to be in the same order, but
                // in theory, they can differ, so let's just double check in case someone
                // has done something odd.
                {for (int GroupIdx = 0; GroupIdx < Mesh->PrimaryTopology->GroupCount; ++GroupIdx)
                {
                    granny_tri_material_group& Group = Mesh->PrimaryTopology->Groups[GroupIdx];
                    if (Group.MaterialIndex != Mat)
                        continue;

                    // Bueno.  BTW, I'm just going to stop apologizing for STL at this
                    // point.
                    GroupIndices.insert(GroupIndices.end(),
                                        MeshIndices[Idx].begin() + (Group.TriFirst*3),
                                        MeshIndices[Idx].begin() + ((Group.TriFirst + Group.TriCount)*3));
                }}

                // GroupIndices *must be remapped* to the new FinalVertices layout.
                // Luckily, that's very easy.
                {for (size_t IIDx = 0; IIDx < GroupIndices.size(); ++IIDx)
                {
                    GroupIndices[IIDx] += CurrentStartVertex;
                }}

                // I mentioned I would stop apologizing, right?
                MaterialGroupIndices.push_back(GroupIndices);
            }

            // Copy out the vertices and advance the CurrentStartVertex
            FinalVertices.insert(FinalVertices.end(),
                                 RemappedMeshVertices[Idx].begin(),
                                 RemappedMeshVertices[Idx].end());
            pp_assert((RemappedMeshVertices[Idx].size() % VertexSize) == 0);

            CurrentStartVertex += int(RemappedMeshVertices[Idx].size() / VertexSize);
            pp_assert(CurrentStartVertex == int(FinalVertices.size()/VertexSize));
        }}
    }

    // Merge the materials that are compatible
    {for (size_t Idx = 0; Idx < MeshMaterials.size(); ++Idx)
    {
        // take no pointers here, we'll be modifying the array as we go
        {for (size_t Consider = MeshMaterials.size()-1; Consider > Idx; --Consider)
        {
            if (MaterialsCompatible(MeshMaterials[Idx], MeshMaterials[Consider]))
            {
                // Merge 'em.  This basically means:
                //  1. Tack the indices onto the end of ours
                MaterialGroupIndices[Idx].insert(MaterialGroupIndices[Idx].end(),
                                                 MaterialGroupIndices[Consider].begin(),
                                                 MaterialGroupIndices[Consider].end());

                //  2. erase the materials and indices from the holding arrays
                MeshMaterials.erase(MeshMaterials.begin() + Consider);
                MaterialGroupIndices.erase(MaterialGroupIndices.begin() + Consider);
            }
        }}
    }}


    // Now we have everything we need to create the final mesh.  As we go through the
    // material groups, assemble the final index buffer
    vector<granny_int32> FinalIndices;

    granny_vertex_data* VD = PushObject(TempArena, granny_vertex_data);
    {
        // All of the verts have the same type, of course
        VD->VertexType = MergeList[0]->PrimaryVertexData->VertexType;

        pp_assert((FinalVertices.size() % VertexSize) == 0);
        VD->VertexCount = int(FinalVertices.size() / VertexSize);

        VD->Vertices = PushArray(TempArena, FinalVertices.size(), granny_uint8);
        memcpy(VD->Vertices, &FinalVertices[0], FinalVertices.size());

        // All of these are not brought over.
        VD->VertexComponentNameCount = 0;
        VD->VertexComponentNames     = 0;
        VD->VertexAnnotationSetCount = 0;
        VD->VertexAnnotationSets     = 0;
    }

    granny_tri_topology* TT = PushObject(TempArena, granny_tri_topology);
    {
        memset(TT, 0, sizeof(*TT));

        TT->GroupCount = int(MeshMaterials.size());
        TT->Groups     = PushArray(TempArena, MeshMaterials.size(), granny_tri_material_group);
        {for (size_t Idx = 0; Idx < MeshMaterials.size(); ++Idx)
        {
            pp_assert((FinalIndices.size() % 3) == 0);
            pp_assert((MaterialGroupIndices[Idx].size() % 3) == 0);

            TT->Groups[Idx].MaterialIndex = int(Idx);
            TT->Groups[Idx].TriFirst = int(FinalIndices.size() / 3);
            TT->Groups[Idx].TriCount = int(MaterialGroupIndices[Idx].size() / 3);

            FinalIndices.insert(FinalIndices.end(),
                                MaterialGroupIndices[Idx].begin(),
                                MaterialGroupIndices[Idx].end());
        }}

        // Copy out the indices
        TT->IndexCount = int(FinalIndices.size());
        TT->Indices = PushArray(TempArena, FinalIndices.size(), granny_int32);
        memcpy(TT->Indices, &FinalIndices[0], TT->IndexCount * sizeof(granny_int32));

    }

    granny_material_binding* MB = PushArray(TempArena, MeshMaterials.size(), granny_material_binding);
    {for (size_t Idx = 0; Idx < MeshMaterials.size(); ++Idx)
    {
        MB[Idx].Material = MeshMaterials[Idx];
    }}

    granny_mesh* Mesh = new granny_mesh;
    {
        memset(Mesh, 0, sizeof(granny_mesh));

        // Not really a good way to do this, just take the zeroth name...
        Mesh->Name = MergeList[0]->Name;
        Mesh->PrimaryVertexData = VD;
        Mesh->PrimaryTopology   = TT;

        Mesh->MaterialBindingCount = int(MeshMaterials.size());
        Mesh->MaterialBindings     = MB;

        Mesh->BoneBindingCount = int(MergedBoneNames.size());
        Mesh->BoneBindings = PushArray(TempArena, Mesh->BoneBindingCount, granny_bone_binding);
        {for (int Idx = 0; Idx < Mesh->BoneBindingCount; ++Idx)
        {
            // Zero out the fields, we might need to compute the bounding volumes in the
            // future...
            memset(&Mesh->BoneBindings[Idx], 0, sizeof(granny_bone_binding));
            Mesh->BoneBindings[Idx].BoneName = MergedBoneNames[Idx];
        }}
    }

    // dtors should handle the cleanup for us...

    return Mesh;
}


granny_file_info*
MergeSkinnedMeshes(char const*          OriginalFilename,
                   char const*          OutputFilename,
                   granny_file_info*    Info,
                   key_value_pair*      KeyValues,
                   granny_int32x        NumKeyValues,
                   granny_memory_arena* TempArena)
{
    vector<granny_mesh*> FileMeshes;
    for (int i = 0; i < Info->ModelCount; i++)
    {
        granny_model* Model = Info->Models[i];
        if (!Model)
            continue;

        vector<granny_mesh*> NewMeshes;
        vector<granny_mesh*> AllMeshes;
        {for (int Idx = 0; Idx < Model->MeshBindingCount; ++Idx)
        {
            AllMeshes.push_back(Model->MeshBindings[Idx].Mesh);
        }}

        while (AllMeshes.empty() == false)
        {
            vector<granny_mesh*> MergeList;
            MergeList.push_back(AllMeshes.back());
            AllMeshes.pop_back();

            {for (granny_uint32 Comp = 0; Comp < AllMeshes.size(); /*no inc*/)
            {
                if (MeshesCompatible(AllMeshes[Comp], MergeList.front()))
                {
                    MergeList.push_back(AllMeshes[Comp]);
                    AllMeshes.erase(AllMeshes.begin() + Comp);
                }
                else
                {
                    ++Comp;
                }
            }}

            if (MergeList.size() == 1)
            {
                NewMeshes.push_back(MergeList[0]);
            }
            else
            {
                granny_mesh* NewMesh = CreateMergedMesh(MergeList, TempArena);
                NewMeshes.push_back(NewMesh);
            }
        }

        for (granny_uint32 Idx = 0; Idx < NewMeshes.size(); ++Idx)
        {
            Model->MeshBindings[Idx].Mesh = NewMeshes[Idx];
            FileMeshes.push_back(NewMeshes[Idx]);
        }
        Model->MeshBindingCount = (int)NewMeshes.size();
    }

    Info->MeshCount = (int)FileMeshes.size();
    Info->Meshes = PushArray(TempArena, (int)FileMeshes.size(), granny_mesh*);
    memcpy(Info->Meshes, &FileMeshes[0], FileMeshes.size() * sizeof(granny_mesh*));

    return Info;
}

static CommandRegistrar RegCompressVertSample(MergeSkinnedMeshes, "MergeSkinnedMeshes",
                                              "Merges compatible skinned meshes into a single object");



