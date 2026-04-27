// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"
#include <stdio.h>
#include <string.h>
#include <memory.h>

#include <vector>
using namespace std;

static bool
MeshesCompatible(granny_mesh* One, granny_mesh* Two)
{
    // No skinned meshes for now
    if (!GrannyMeshIsRigid(One) || !GrannyMeshIsRigid(Two))
        return false;

    // We only support merging meshes with one material that is exactly the same (in this
    // version)
    {
        if (One->MaterialBindingCount == 1 && Two->MaterialBindingCount == 1)
        {
            if (One->MaterialBindings[0].Material != Two->MaterialBindings[0].Material)
                return false;

            // Acceptable (fall through)
        }
        else if (One->MaterialBindingCount == 0 && Two->MaterialBindingCount == 0)
        {
            // Acceptable (fall through)
        }
        else
        {
            // No good.
            return false;
        }
    }

    // We need to ensure that the vertex types match
    if (!GrannyDataTypesAreEqualWithNames(GrannyGetMeshVertexType(One),
                                          GrannyGetMeshVertexType(Two)))
    {
        return false;
    }

    return true;
}


static granny_mesh*
CreateMergedMesh(vector<granny_mesh*>& MergeList,
                 granny_memory_arena* TempArena,
                 vector<const char*>& CollapsedBones)
{
    int const VertexSize = GrannyGetTotalObjectSize(GrannyGetMeshVertexType(MergeList[0]));

    int TotalVertices = 0;
    int TotalIndices  = 0;
    for (granny_uint32 Idx = 0; Idx < MergeList.size(); ++Idx)
    {
        TotalVertices += GrannyGetMeshVertexCount(MergeList[Idx]);
        TotalIndices  += GrannyGetMeshIndexCount(MergeList[Idx]);
    }

    granny_uint8* NewVertices = PushArray(TempArena, TotalVertices*VertexSize, granny_uint8);
    granny_int32* NewIndices  = PushArray(TempArena, TotalIndices, granny_int32);

    int CurrentVPos = 0;
    int CurrentIPos = 0;
    for (granny_uint32 Idx = 0; Idx < MergeList.size(); ++Idx)
    {
        int const VertCount  = GrannyGetMeshVertexCount(MergeList[Idx]);
        int const IndexCount = GrannyGetMeshIndexCount(MergeList[Idx]);

        memcpy(&NewVertices[CurrentVPos * VertexSize],
               GrannyGetMeshVertices(MergeList[Idx]),
               VertexSize * VertCount);

        GrannyCopyMeshIndices(MergeList[Idx], 4, &NewIndices[CurrentIPos]);
        for (int Ind = CurrentIPos; Ind < (CurrentIPos + IndexCount); ++Ind)
        {
            NewIndices[Ind] += CurrentVPos;
        }

        CurrentVPos += VertCount;
        CurrentIPos += IndexCount;

        //Remember any bones this mesh referenced so we can try to delete them later
        granny_mesh* MergeMesh = MergeList[Idx];
        for( granny_int32 BindingIdx=0; BindingIdx < MergeMesh->BoneBindingCount; BindingIdx++ )
        {
            CollapsedBones.push_back(MergeMesh->BoneBindings[BindingIdx].BoneName);
        }
    }
    pp_assert((CurrentIPos % 3) == 0);

    granny_tri_material_group* Group = PushObject(TempArena, granny_tri_material_group);
    {
        Group->MaterialIndex = 0;
        Group->TriFirst = 0;
        Group->TriCount = (CurrentIPos / 3);
    }

    granny_vertex_data* VD = PushObject(TempArena, granny_vertex_data);
    {
        VD->VertexType = MergeList[0]->PrimaryVertexData->VertexType;
        VD->VertexCount = CurrentVPos;
        VD->Vertices = NewVertices;
        VD->VertexComponentNameCount = 0;
        VD->VertexComponentNames = 0;
        VD->VertexAnnotationSetCount = 0;
        VD->VertexAnnotationSets = 0;
    }

    granny_tri_topology* TT = PushObject(TempArena, granny_tri_topology);
    {
        memset(TT, 0, sizeof(*TT));
        TT->GroupCount = 1;
        TT->Groups = Group;
        TT->IndexCount = CurrentIPos;
        TT->Indices = NewIndices;
    }

    granny_mesh* Mesh = new granny_mesh;
    {
        memset(Mesh, 0, sizeof(granny_mesh));

        Mesh->Name = "Foo";
        Mesh->PrimaryVertexData = VD;
        Mesh->PrimaryTopology = TT;

        Mesh->MaterialBindingCount = MergeList[0]->MaterialBindingCount;
        Mesh->MaterialBindings = MergeList[0]->MaterialBindings;

        Mesh->BoneBindingCount = 1;
        Mesh->BoneBindings = PushObject(TempArena, granny_bone_binding);
        memset(Mesh->BoneBindings, 0, sizeof(granny_bone_binding));
        Mesh->BoneBindings[0].BoneName = MergeList[0]->BoneBindings[0].BoneName;
    }

    return Mesh;
}


granny_file_info*
MergeRigidMeshes(char const*          OriginalFilename,
                 char const*          OutputFilename,
                 granny_file_info*    Info,
                 key_value_pair*      KeyValues,
                 granny_int32x        NumKeyValues,
                 granny_memory_arena* TempArena)
{
    vector<granny_mesh*> FileMeshes;
    vector<const char*> CollapsedBones;
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
                granny_mesh* NewMesh = CreateMergedMesh(MergeList, TempArena, CollapsedBones);
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

    //Push the topologies and vertex datas into the file info. These aren't really useful (they're 
    //just a mirror of what's in the meshes), but do this anyway for consistency. 
    Info->VertexDataCount = Info->MeshCount;
    Info->VertexDatas = PushArray(TempArena, Info->MeshCount, granny_vertex_data*);
    Info->TriTopologyCount = Info->MeshCount;
    Info->TriTopologies = PushArray(TempArena, Info->MeshCount, granny_tri_topology*);
    for( granny_int32 MeshIdx=0; MeshIdx<Info->MeshCount; MeshIdx++ )
    {
        Info->VertexDatas[MeshIdx] = Info->Meshes[MeshIdx]->PrimaryVertexData;
        Info->TriTopologies[MeshIdx] = Info->Meshes[MeshIdx]->PrimaryTopology;
    }

    //CollapsedBones are the bones we will potentially remove
    //Remove any CollapsedBones not referenced by mesh bone bindings

    vector<const char*> CollapsedBonesToRemove;
    for (granny_uint32 CollapseBoneIdx=0; CollapseBoneIdx<CollapsedBones.size(); CollapseBoneIdx++)
    {
        bool CollapsedBoneReferenced = false;
        for (granny_int32 MeshIdx = 0; MeshIdx<Info->MeshCount && !CollapsedBoneReferenced; MeshIdx++)
        {
            for( granny_int32 BoneBindingIdx=0; BoneBindingIdx < Info->Meshes[MeshIdx]->BoneBindingCount; BoneBindingIdx++ )
            {
                if( _stricmp(CollapsedBones[CollapseBoneIdx],Info->Meshes[MeshIdx]->BoneBindings[BoneBindingIdx].BoneName) == 0)
                {
                    CollapsedBoneReferenced = true;
                    break;
                }
            }
        }

        if( !CollapsedBoneReferenced )
        {
            CollapsedBonesToRemove.push_back(CollapsedBones[CollapseBoneIdx]);
        }
    }

    for( granny_int32 SkeletonIdx=0; SkeletonIdx < Info->SkeletonCount; SkeletonIdx++ )
    {
        granny_skeleton * Skeleton = Info->Skeletons[SkeletonIdx];

        granny_bone* NewBones = PushArray(TempArena, (int)Skeleton->BoneCount, granny_bone);
        granny_int32 NewBoneCount = 0;

        for( granny_int32 BoneIdx=0; BoneIdx < Skeleton->BoneCount; BoneIdx++ )
        {
            bool RemoveThisBone = false;
            for (granny_uint32 RemoveBoneIdx=0; RemoveBoneIdx<CollapsedBonesToRemove.size(); RemoveBoneIdx++)
            {
                const char * RemoveName = CollapsedBonesToRemove[RemoveBoneIdx];
                if( _stricmp(Skeleton->Bones[BoneIdx].Name,RemoveName) == 0 )
                {
                    RemoveThisBone = true;
                    break;
                }
            }

            if( !RemoveThisBone )
            {
                NewBones[NewBoneCount] = Skeleton->Bones[BoneIdx];
                NewBoneCount++;
            }
        }

        Skeleton->BoneCount = NewBoneCount;
        Skeleton->Bones = NewBones;
    }

    return Info;
}

static CommandRegistrar RegCompressVertSample(MergeRigidMeshes, "MergeRigidMeshes",
                                              "Merges compatible rigid meshes into a single object");

