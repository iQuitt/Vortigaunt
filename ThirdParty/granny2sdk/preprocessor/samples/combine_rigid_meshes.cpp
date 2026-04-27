// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"
#include "../utilities.h"
#include <vector>
#include <set>

using namespace std;


// Designates a mesh/material grouping pair to combine
typedef pair<granny_mesh*, int> Submesh;

struct SICmp : public binary_function<char const*, char const*, bool>
{
    bool operator()(char const* One, char const* Two) const
    {
        return _stricmp(One, Two) < 0;
    }
};

typedef set<char const*, SICmp> BoneSet;

struct CombineSpec
{
    granny_material* Material;

    vector<Submesh> Submeshes;

    // This is lazy, but it's an easy way to allow multiple meshes that share a bone to
    // use the same mesh grouping without blowing the bone limit.
    BoneSet UniqueBones;
};


// This function only handles meshes with 32-bit indices.  It outputs pwnt3132 verts.
granny_mesh*
MeshFromSpec(CombineSpec& Spec,
             granny_memory_arena* Arena)
{
    granny_mesh* NewMesh = PushObject(Arena, granny_mesh);
    memset(NewMesh, 0, sizeof(granny_mesh));
    NewMesh->Name = PushString(Arena, "Combined");

    NewMesh->PrimaryVertexData = PushObject(Arena, granny_vertex_data);
    memset(NewMesh->PrimaryVertexData, 0, sizeof(granny_vertex_data));

    NewMesh->PrimaryTopology = PushObject(Arena, granny_tri_topology);
    memset(NewMesh->PrimaryTopology, 0, sizeof(granny_tri_topology));

    NewMesh->MaterialBindingCount = 1;
    NewMesh->MaterialBindings = PushArray(Arena, 1, granny_material_binding);
    NewMesh->MaterialBindings[0].Material = Spec.Material;

    vector<granny_bone_binding>    BoneBindings;
    vector<granny_pwnt3132_vertex> NewVertices;
    vector<granny_int32>           NewIndices;

    {for(int SubIdx = 0; SubIdx < (int)Spec.Submeshes.size(); ++SubIdx)
    {
        Submesh& sub = Spec.Submeshes[SubIdx];

        granny_mesh* OrigMesh = sub.first;
        granny_tri_material_group& Group = OrigMesh->PrimaryTopology->Groups[sub.second];

        int const VertSize = GrannyGetTotalObjectSize(OrigMesh->PrimaryVertexData->VertexType);

        int BindingIdx = -1;
        {for(int Idx = 0; Idx < (int)BoneBindings.size(); ++Idx)
        {
            if (_stricmp(BoneBindings[Idx].BoneName, OrigMesh->BoneBindings[0].BoneName) == 0)
            {
                BindingIdx = Idx;
                break;
            }
        }}
        if (BindingIdx == -1)
        {
            BindingIdx = (int)BoneBindings.size();
            BoneBindings.push_back(OrigMesh->BoneBindings[0]);
            memset(&BoneBindings.back().OBBMin, 0, sizeof(BoneBindings.back().OBBMin));
            memset(&BoneBindings.back().OBBMax, 0, sizeof(BoneBindings.back().OBBMax));
        }


        vector<granny_int32> Indices32(GrannyGetMeshIndexCount(OrigMesh));
        GrannyCopyMeshIndices(OrigMesh, 4, &Indices32[0]);

        vector<int> NewVertLocs(OrigMesh->PrimaryVertexData->VertexCount, -1);
        {for(int Idx = Group.TriFirst * 3; Idx < (Group.TriFirst + Group.TriCount) * 3; ++Idx)
        {
            granny_int32 OldVertLoc = Indices32[Idx];
            granny_int32 NewVertLoc = NewVertLocs[OldVertLoc];
            if (NewVertLoc == -1)
            {
                // Need to copy the vert into the new buffer
                NewVertLocs[OldVertLoc] = (int)NewVertices.size();
                NewVertLoc = NewVertLocs[OldVertLoc];
                NewVertices.resize(NewVertices.size() + 1);

                granny_pwnt3132_vertex* NewVert = &(NewVertices.back());
                granny_uint8*           OldVert = OrigMesh->PrimaryVertexData->Vertices + (OldVertLoc * VertSize);

                GrannyConvertSingleObject(OrigMesh->PrimaryVertexData->VertexType,
                                          OldVert,
                                          GrannyPWNT3132VertexType,
                                          NewVert, 0);

                NewVert->BoneIndex = BindingIdx;
            }

            NewIndices.push_back(NewVertLoc);
        }}
    }}

    NewMesh->PrimaryVertexData->VertexType = GrannyPWNT3132VertexType;
    NewMesh->PrimaryVertexData->Vertices = (granny_uint8*)PushArray(Arena, (int)NewVertices.size(), granny_pwnt3132_vertex);
    memcpy(NewMesh->PrimaryVertexData->Vertices, &NewVertices[0], NewVertices.size() * sizeof(granny_pwnt3132_vertex));
    NewMesh->PrimaryVertexData->VertexCount = (int)NewVertices.size();

    NewMesh->PrimaryTopology->Indices = PushArray(Arena, (int)NewIndices.size(), granny_int32);
    memcpy(NewMesh->PrimaryTopology->Indices, &NewIndices[0], NewIndices.size() * 4);
    NewMesh->PrimaryTopology->IndexCount = (int)NewIndices.size();

    NewMesh->PrimaryTopology->GroupCount = 1;
    NewMesh->PrimaryTopology->Groups = PushArray(Arena, 1, granny_tri_material_group);
    NewMesh->PrimaryTopology->Groups[0].MaterialIndex = 0;
    NewMesh->PrimaryTopology->Groups[0].TriFirst = 0;
    NewMesh->PrimaryTopology->Groups[0].TriCount = (int)(NewIndices.size() / 3);

    NewMesh->BoneBindings = PushArray(Arena, (int)BoneBindings.size(), granny_bone_binding);
    memcpy(NewMesh->BoneBindings, &BoneBindings[0], BoneBindings.size() * sizeof(granny_bone_binding));
    NewMesh->BoneBindingCount = (int)BoneBindings.size();

    return NewMesh;
}



granny_file_info*
CombineRigidMeshes(char const*          OriginalFilename,
                   char const*          OutputFilename,
                   granny_file_info*    Info,
                   key_value_pair*      KeyValues,
                   granny_int32x        NumKeyValues,
                   granny_memory_arena* TempArena)
{
    char const* BoneLimitStr = FindFirstValueForKey(KeyValues, NumKeyValues, "bonelimit");
    int BoneLimit = -1;
    if (BoneLimitStr != 0)
    {
        BoneLimit = atoi(BoneLimitStr);
        if (BoneLimit <= 0)
        {
            ErrOut("Invalid bone limit number (must be > 0)\n");
        }
    }

    vector<granny_mesh*> NewMeshes;
    vector<CombineSpec> Specs;
    {for (int MeshIdx = 0; MeshIdx < Info->MeshCount; ++MeshIdx)
    {
        granny_mesh* Mesh = Info->Meshes[MeshIdx];
        if (Mesh == 0)
            continue;

        if (!GrannyMeshIsRigid(Mesh))
        {
            // Already skinned
            NewMeshes.push_back(Mesh);
            continue;
        }

        // This /is/ a rigid mesh, but it's not one that we can throw into a skin, since
        // there's no bone to reference.
        if (Mesh->BoneBindingCount == 0)
            continue;

        pp_assert(Mesh->PrimaryTopology->GroupCount == Mesh->MaterialBindingCount || Mesh->MaterialBindingCount == 0);
        {for(int GroupIdx = 0; GroupIdx < Mesh->PrimaryTopology->GroupCount; ++GroupIdx)
        {
            granny_material* BindingMaterial = 0;
            if (Mesh->MaterialBindingCount)
            {
                granny_material_binding& Binding = Mesh->MaterialBindings[GroupIdx];
                BindingMaterial = Binding.Material;
            }

            bool Found = false;
            {for(int Idx = 0; Idx < (int)Specs.size(); ++Idx)
            {
                CombineSpec& Spec = Specs[Idx];
                if (Spec.Material != BindingMaterial)
                    continue;

                char const* BoneName = Mesh->BoneBindings[0].BoneName;
                if (BoneLimit > 0)
                {
                    if (Spec.UniqueBones.find(BoneName) == Spec.UniqueBones.end() &&
                        int(Spec.UniqueBones.size()) >= BoneLimit)
                    {
                        continue;
                    }
                }

                Specs[Idx].Submeshes.push_back(Submesh(Mesh, GroupIdx));
                Specs[Idx].UniqueBones.insert(Mesh->BoneBindings[0].BoneName);

                Found = true;
                break;
            }}

            if (!Found)
            {
                // New spec
                CombineSpec spec;
                spec.Material = BindingMaterial;
                spec.Submeshes.push_back(Submesh(Mesh, GroupIdx));
                spec.UniqueBones.insert(Mesh->BoneBindings[0].BoneName);
                Specs.push_back(spec);
            }
        }}
    }}

    {for(int Idx = 0; Idx < (int)Specs.size(); ++Idx)
    {
        granny_mesh* NewMesh = MeshFromSpec(Specs[Idx], TempArena);
        NewMeshes.push_back(NewMesh);
    }}

    // Need to move the NewMeshes into the arena from the vector, which will be going bye-bye...

    Info->MeshCount = (int)NewMeshes.size();
    if (Info->MeshCount != 0)
    {
        Info->Meshes    = PushArray(TempArena, Info->MeshCount, granny_mesh*);
        memcpy(Info->Meshes, &NewMeshes[0], sizeof(granny_mesh*) * Info->MeshCount);
    }
    else
    {
        Info->Meshes = 0;
    }

    Info->Models[0]->MeshBindingCount = (int)NewMeshes.size();
    Info->Models[0]->MeshBindings = PushArray(TempArena, Info->Models[0]->MeshBindingCount, granny_model_mesh_binding);
    for (int Binding = 0; Binding < Info->Models[0]->MeshBindingCount; ++Binding)
        Info->Models[0]->MeshBindings[Binding].Mesh = NewMeshes[Binding];

    // Replace model bindings...

    // Recompute the bone bounds...
    if (RecomputeAllBoneBounds(Info) == false)
        return 0;

    // ===================================================================================
    // ===================================================================================
    return Info;
}


static CommandRegistrar RegCombineRigidMeshes(CombineRigidMeshes, "CombineRigidMeshes",
                                              "Combines rigid meshes sharing a material into skinned meshes");
