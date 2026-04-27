// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "preprocessor.h"
#include "utilities.h"
#include <algorithm>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <vector>

using namespace std;

bool BreakMesh(granny_file_info*     Info,
               granny_mesh*          OriginalMesh,
               granny_int32x         BoneLimit,
               vector<granny_mesh*>& NewMeshes,
               bool                  AddNamePostfix,
               granny_memory_arena*  TempArena);

bool RemapModelBindings(granny_file_info* Info,
                        granny_mesh*      OriginalMesh,
                        vector<granny_mesh*>& NewMeshes,
                        granny_memory_arena* TempArena);
bool RemapVertexDatas(granny_file_info* Info,
                      granny_mesh*      OriginalMesh,
                      vector<granny_mesh*>& NewMeshes,
                      granny_memory_arena* TempArena);
bool RemapTopologies(granny_file_info* Info,
                     granny_mesh*      OriginalMesh,
                     vector<granny_mesh*>& NewMeshes,
                     granny_memory_arena* TempArena);


granny_file_info*
BonesPerMesh(char const*          /*OriginalFilename*/,
             char const*          /*OutputFilename*/,
             granny_file_info*    InputInfo,
             key_value_pair*      KeyValues,
             granny_int32x        NumKeyValues,
             granny_memory_arena* TempArena)
{
    bool const AddNamePostfix =
        (FindFirstValueForKey(KeyValues, NumKeyValues, "modname") != 0);

    granny_int32x BoneLimit;
    char const* BoneLimitString = FindFirstValueForKey(KeyValues, NumKeyValues, "bonelimit");
    if (BoneLimitString == 0)
    {
        ErrOut("must specify an bone count limit with \"-bonelimit N\"\n");
        return 0;
    }
    BoneLimit = atoi(BoneLimitString);
    if (BoneLimit < 2)
    {
        ErrOut("invalid bonelimit (%d), must be at least 4\n", BoneLimit);
        return 0;
    }

    vector<granny_mesh*> NewMeshes;
    {for(granny_int32x i = 0; i < InputInfo->MeshCount; i++)
    {
        granny_mesh* OriginalMesh = InputInfo->Meshes[i];
        if (!OriginalMesh)
            continue;

        if (BreakMesh(InputInfo, OriginalMesh, BoneLimit, NewMeshes, AddNamePostfix, TempArena) == false)
        {
            ErrOut("failed to break mesh %s\n", OriginalMesh->Name);
            return 0;
        }
    }}

    // Move NewMeshes into the file info as the Mesh array...
    InputInfo->MeshCount = (granny_int32)NewMeshes.size();
    InputInfo->Meshes = PushArray(TempArena, InputInfo->MeshCount, granny_mesh*);
    memcpy(InputInfo->Meshes, &NewMeshes[0], sizeof(granny_mesh*) * InputInfo->MeshCount);


    // Recompute the bounds
    if (RecomputeAllBoneBounds(InputInfo) == false)
        return 0;

    return InputInfo;
}

// =====================================================================
// =====================================================================

// 8 is probably overkill, but what the hey, memory is cheap...
static const int MaxNumWeights = 8;

// A vert that allows us to pull just the weights from the vertex
// data...
struct WeightVertex
{
    granny_uint8 BoneWeights[MaxNumWeights];
    granny_uint8 BoneIndices[MaxNumWeights];
    granny_int32 BonesUsed;
};

granny_data_type_definition WeightVertexType[] =
{
    {GrannyNormalUInt8Member, GrannyVertexBoneWeightsName, 0, MaxNumWeights},
    {GrannyUInt8Member,       GrannyVertexBoneIndicesName, 0, MaxNumWeights},
    {GrannyInt32Member,       "BonesUsed" },
    {GrannyEndMember},
};

struct TriWeightData
{
    granny_uint8 UsedIndices[MaxNumWeights * 3];
    granny_int32 NumUsedBones;

    int VertIndices[3];
};


struct NewMeshIntermediate
{
    int const NumOrigBones;
    bool* BoneAdded;
    int   TotalBones;

    // NumVerts wide.  Contains the new index of a vertex if it's
    // added to the new mesh, or -1 if not.
    int const NumOrigVerts;
    int*  NewVertIndices;
    int   NumNewVerts;

    // NumTris*3 wide.  Note that this refers to an index in the /new/
    // mesh, i.e, no lookup through NewVertIndices is necessary
    int const NumOrigTris;
    int*  TriIndices;
    int   NumIndicesUsed;

    // NumTris wide.  Note that these fields are only cleared by
    // Clear(), not ClearSubMesh()
    bool* TriAdded;
    int   TotalTris;

    NewMeshIntermediate(int NumBones, int NumVerts, int NumTris);
    ~NewMeshIntermediate();

    NewMeshIntermediate const& operator=(NewMeshIntermediate&);  // not allowed, silence, compiler!
    void ClearSubMesh();
    void ClearAll();
};


static char*
MakePostfixName(char const*          Name,
                int                  ThisMeshCount,
                granny_memory_arena* TempArena)
{
    char NumBuffer[128];
    sprintf(NumBuffer, "_%d", ThisMeshCount);

    return GrannyMemoryArenaPushConcat(TempArena, Name, NumBuffer);
}


static granny_vertex_data*
CreateModifiedVertData(granny_vertex_data const* VertData,
                       granny_int32x  NumOrigVerts,
                       granny_int32x  NumNewVerts,
                       granny_int32x* NewVertIndices,
                       granny_int32x  BoneBindingCount,
                       granny_int32x* BoneRemaps,
                       granny_memory_arena* TempArena)
{
    granny_int32x const VertSize = GrannyGetTotalObjectSize(VertData->VertexType);

    granny_vertex_data* Result = PushObject(TempArena, granny_vertex_data);
    memset(Result, 0, sizeof(granny_vertex_data));

    Result->VertexType  = VertData->VertexType;
    Result->VertexCount = NumNewVerts;
    Result->Vertices    = PushArray(TempArena,
                                    VertSize * Result->VertexCount,
                                    granny_uint8);

    Result->VertexComponentNameCount = VertData->VertexComponentNameCount;
    Result->VertexComponentNames     = VertData->VertexComponentNames;

    // Copy the raw vertex data
    granny_uint8 const* OldVertices = VertData->Vertices;
    {for(int i = 0; i < NumOrigVerts; ++i)
    {
        if (NewVertIndices[i] == -1)
            continue;

        granny_uint8 const* OldVert = OldVertices + (VertSize * i);
        granny_uint8* NewVert = Result->Vertices + (VertSize * NewVertIndices[i]);
        memcpy(NewVert, OldVert, VertSize);
    }}

    // Ok, the verts are in the correct order, now we need to
    // remap the bone indices.  This is a little ticklish, since
    // we need to scan the data_type_definition to find the
    // correct offset
    {
        int IndexOffset = 0;
        int NumTypeIndices = 0;

        granny_data_type_definition* Walk = Result->VertexType;
        while (Walk && Walk->Type != GrannyEndMember)
        {
            if (_stricmp(Walk->Name, GrannyVertexBoneIndicesName) == 0)
            {
                NumTypeIndices = Walk->ArrayWidth ? Walk->ArrayWidth : 1;
                break;
            }

            IndexOffset += GrannyGetMemberTypeSize(Walk);
            ++Walk;
        }

        int WeightOffset = 0;
        int NumTypeWeights = 0;

        Walk = Result->VertexType;
        while (Walk && Walk->Type != GrannyEndMember)
        {
            if (_stricmp(Walk->Name, GrannyVertexBoneWeightsName) == 0)
            {
                NumTypeWeights = Walk->ArrayWidth ? Walk->ArrayWidth : 1;
                break;
            }

            WeightOffset += GrannyGetMemberTypeSize(Walk);
            ++Walk;
        }

        // Must have indices.
        // May have either *no* weights, or a matching number of weights.
        if (NumTypeIndices == 0 ||
            (NumTypeWeights != 0 && (NumTypeWeights != NumTypeIndices)))
        {
            ErrOut("Unable to find correctly formatted bone indices/weights in the vertex type (%d %d)\n", NumTypeIndices, NumTypeWeights);
            return 0;
        }

        granny_uint8* CurrDestVert = Result->Vertices;
        {for(int i = 0; i < NumNewVerts; ++i)
        {
            granny_uint8* Weights = NumTypeWeights ? (CurrDestVert + WeightOffset) : 0; // only valid if NumTypeWeights != 0
			pp_unused(Weights);

			granny_uint8* Indices = CurrDestVert + IndexOffset;
            {for(int j = 0; j < NumTypeIndices; j++)
            {
                pp_assert(Indices[j] < BoneBindingCount);

                // The simple pp_assert doesn't always work here.
                // There is a special case to handle in older
                // data, where unused bone indices were set to 0,
                // rather than BoneIndices[0].  We'll manually
                // handle that case...
                // pp_assert(BoneRemaps[Indices[j]] != -1);

                if (BoneRemaps[Indices[j]] != -1)
                {
                    Indices[j] = granny_uint8(BoneRemaps[Indices[j]]);
                }
                else
                {
                    // If these arent't true, then the special case
                    // doesn't apply
                    pp_assert(j != 0);
                    pp_assert(NumTypeWeights == 0 || Weights[j] == 0);

                    // Just copy the first index...
                    Indices[j] = Indices[0];
                }
            }}

            // Advance the vertex
            CurrDestVert += VertSize;
        }}
    }

    return Result;
}



bool KickoffMesh(granny_mesh*               OriginalMesh,
                 granny_tri_material_group& MaterialGroup,
                 NewMeshIntermediate*       NewMesh,
                 vector<granny_mesh*>&      NewMeshes,
                 bool                       AddNamePostfix,
                 int                        ThisMeshCount,
                 granny_memory_arena*       TempArena)
{
    // Copy the bits that we care about.  Note that we explicitly NULL
    // the bits we don't want...
    granny_mesh* NewGrannyMesh = PushObject(TempArena, granny_mesh);
    NewGrannyMesh->Name = OriginalMesh->Name;
    NewGrannyMesh->MorphTargetCount = OriginalMesh->MorphTargetCount;

    if (OriginalMesh->MorphTargetCount)
    {
        NewGrannyMesh->MorphTargets = PushArray(TempArena, OriginalMesh->MorphTargetCount, granny_morph_target);
        {for (int Idx = 0; Idx < OriginalMesh->MorphTargetCount; ++Idx)
        {
            NewGrannyMesh->MorphTargets[Idx].ScalarName = OriginalMesh->MorphTargets[Idx].ScalarName;
            NewGrannyMesh->MorphTargets[Idx].DataIsDeltas = OriginalMesh->MorphTargets[Idx].DataIsDeltas;
        }}
    }
    else
    {
        NewGrannyMesh->MorphTargets = 0;
    }

    NewGrannyMesh->MaterialBindingCount = 1;
    NewGrannyMesh->MaterialBindings = OriginalMesh->MaterialBindings + MaterialGroup.MaterialIndex;

    if (AddNamePostfix)
    {
        NewGrannyMesh->Name = MakePostfixName(OriginalMesh->Name, ThisMeshCount, TempArena);
    }

    // We copy the extendeddata, which is questionable, but about the
    // only thing we can reasonably do without losing it entirely.
    NewGrannyMesh->ExtendedData = OriginalMesh->ExtendedData;

    // Need to set:
    //  PrimaryVertexData
    //  PrimaryTopology
    //  BoneBindings

    // Start with bone_bindings.  Note that we don't setup the OBB or
    // TriangleIndices fields in the bindings.  These can be computed
    // later if necessary
    vector<int> BoneRemaps(OriginalMesh->BoneBindingCount, -1);
    {
        int CurrentBinding = 0;
        {for (int i = 0; i < OriginalMesh->BoneBindingCount; i++)
        {
            if (NewMesh->BoneAdded[i])
            {
                BoneRemaps[i] = CurrentBinding++;
            }
        }}

        NewGrannyMesh->BoneBindingCount = CurrentBinding;
        NewGrannyMesh->BoneBindings = PushArray(TempArena, CurrentBinding, granny_bone_binding);
        memset(NewGrannyMesh->BoneBindings, 0, sizeof(granny_bone_binding) * CurrentBinding);

        {for (int i = 0; i < OriginalMesh->BoneBindingCount; i++)
        {
            if (BoneRemaps[i] != -1)
            {
                granny_bone_binding const& OldBinding = OriginalMesh->BoneBindings[i];
                granny_bone_binding& NewBinding = NewGrannyMesh->BoneBindings[BoneRemaps[i]];
                NewBinding.BoneName = OldBinding.BoneName;
            }
        }}
    }

    // Now the topology.  Note that we always store indices here as
    // 32-bit, they can be converted with a followup preprocessor
    // command if necessary.  This is an extremely simple topology, we
    // ignore the vertex maps, annotations, etc.  These can also be
    // added later.  (Although losing the tri_annotations is bad, we
    // may want to reconsider that...)
    {
        NewGrannyMesh->PrimaryTopology = PushObject(TempArena, granny_tri_topology);
        memset(NewGrannyMesh->PrimaryTopology, 0, sizeof(granny_tri_topology));

        NewGrannyMesh->PrimaryTopology->GroupCount = 1;
        NewGrannyMesh->PrimaryTopology->Groups = PushObject(TempArena, granny_tri_material_group);
        NewGrannyMesh->PrimaryTopology->Groups[0].MaterialIndex = 0;
        NewGrannyMesh->PrimaryTopology->Groups[0].TriFirst = 0;
        NewGrannyMesh->PrimaryTopology->Groups[0].TriCount = NewMesh->NumIndicesUsed / 3;

        NewGrannyMesh->PrimaryTopology->IndexCount = NewMesh->NumIndicesUsed;
        NewGrannyMesh->PrimaryTopology->Indices =
            PushArray(TempArena, NewGrannyMesh->PrimaryTopology->IndexCount, granny_int32);
        pp_assert(sizeof(int) == sizeof(granny_int32));
        memcpy(NewGrannyMesh->PrimaryTopology->Indices, NewMesh->TriIndices,
               sizeof(int) * NewGrannyMesh->PrimaryTopology->IndexCount);
    }

    // Ok, now the hard part, the vertex data.  Again, we're ignoring
    // the vertex annotation.
    {
        NewGrannyMesh->PrimaryVertexData =
            CreateModifiedVertData(OriginalMesh->PrimaryVertexData,
                                   NewMesh->NumOrigVerts,
                                   NewMesh->NumNewVerts,
                                   NewMesh->NewVertIndices,
                                   OriginalMesh->BoneBindingCount,
                                   &BoneRemaps[0], TempArena);
        if (NewGrannyMesh->PrimaryVertexData == 0)
        {
            // Don't try to recover from this one.  Log will have already spilled from
            // inside this function
            return false;
        }

        {for (int Idx = 0; Idx < OriginalMesh->MorphTargetCount; ++Idx)
        {
            NewGrannyMesh->MorphTargets[Idx].VertexData =
                CreateModifiedVertData(OriginalMesh->MorphTargets[Idx].VertexData,
                                       NewMesh->NumOrigVerts,
                                       NewMesh->NumNewVerts,
                                       NewMesh->NewVertIndices,
                                       OriginalMesh->BoneBindingCount,
                                       &BoneRemaps[0], TempArena);
        }}
    }

    // Add the mesh
    NewMeshes.push_back(NewGrannyMesh);

    return true;
}


bool
BreakMesh(granny_file_info*     Info,
          granny_mesh*          OriginalMesh,
          granny_int32x         BoneLimit,
          vector<granny_mesh*>& NewMeshes,
          bool                  AddNamePostfix,
          granny_memory_arena*  TempArena)
{
    // Get the number of bones, and handle the easy case...
    int const NumOrigBones = OriginalMesh->BoneBindingCount;
    if (NumOrigBones <= BoneLimit)
    {
        NewMeshes.push_back(OriginalMesh);

        // We don't need to replace Model bindings, since we're
        // keeping the mesh as-is...
        return true;
    }

    vector<granny_mesh*> LocalNewMeshes;

    // We only need to be concerned with the maximum tris in any
    // individual tri-grouping
    int const NumOrigTris = GrannyGetMeshTriangleCount(OriginalMesh);

    // We first need to create a representation of the verts that
    // lists the bones used
    int const NumOrigVerts    = GrannyGetMeshVertexCount(OriginalMesh);
    WeightVertex *WeightVerts = new WeightVertex[NumOrigVerts];
    {
        memset(WeightVerts, 0, sizeof(WeightVerts) * NumOrigVerts);
        GrannyConvertVertexLayouts(NumOrigVerts,
                                   GrannyGetMeshVertexType ( OriginalMesh ),
                                   GrannyGetMeshVertices ( OriginalMesh ),
                                   WeightVertexType,
                                   WeightVerts );
        GrannyOneNormalizeWeights(NumOrigVerts, WeightVertexType, WeightVerts);

        // Now scan the verts, and count the non-zero weights
        {for(int i = 0; i < NumOrigVerts; ++i)
        {
            for (int w = 0; w < MaxNumWeights; ++w)
            {
                if (WeightVerts[i].BoneWeights[w] == 0)
                    break;

                WeightVerts[i].BonesUsed++;
            }
        }}
    }

    // Create a 4-byte representation of the indices
    int const NumMeshIndices = GrannyGetMeshIndexCount(OriginalMesh);
    int* OriginalIndices = new int[NumMeshIndices];
    GrannyCopyMeshIndices(OriginalMesh, 4, OriginalIndices);

    // Create the array describing which bones each tri uses
    int const NumMeshTris = GrannyGetMeshTriangleCount(OriginalMesh);
    TriWeightData* TriWeights = new TriWeightData[NumMeshTris];
    {for(int Tri = 0; Tri < NumMeshTris; ++Tri)
    {
        TriWeightData& TriData = TriWeights[Tri];
        TriData.NumUsedBones = 0;
        {for(int Vert = 0; Vert < 3; ++Vert)
        {
            TriData.VertIndices[Vert] = OriginalIndices[Tri*3 + Vert];
            WeightVertex& WVert = WeightVerts[OriginalIndices[Tri*3 + Vert]];

            {for(int BoneIdx = 0; BoneIdx < WVert.BonesUsed; ++BoneIdx)
            {
                granny_uint8 Bone = WVert.BoneIndices[BoneIdx];

                if (find(TriData.UsedIndices,
                         TriData.UsedIndices + TriData.NumUsedBones,
                         Bone) == TriData.UsedIndices + TriData.NumUsedBones)
                {
                    TriData.UsedIndices[TriData.NumUsedBones++] = Bone;
                }
            }}
        }}
    }}

    // We create a mesh intermediate structure to make it easy to pass
    // bits around
    NewMeshIntermediate* NewMesh =
        new NewMeshIntermediate(NumOrigBones, NumOrigVerts, NumOrigTris);

    // Loop over the material groups, and do the work
    int NumKickedOff = 0;
    {for(granny_int32x Group = 0;
         Group < GrannyGetMeshTriangleGroupCount(OriginalMesh);
         ++Group)
    {
        granny_tri_material_group& ThisGroup = GrannyGetMeshTriangleGroups(OriginalMesh)[Group];

        // Clear our mesh representation
        NewMesh->ClearAll();

        while (NewMesh->TotalTris < ThisGroup.TriCount)
        {
            // Keep adding verts and tris until we run out of things
            // to add, then kick off the new mesh
            for (;;)
            {
                int BestTri = -1;
                int BestNumAdds = 3 * (MaxNumWeights + 1);
                int BestSharedBones = -1;
                {for(granny_int32x TriIdx = 0; TriIdx < ThisGroup.TriCount; ++TriIdx)
                {
                    granny_int32x RealTriIdx = TriIdx + ThisGroup.TriFirst;
                    if (NewMesh->TriAdded[RealTriIdx])
                        continue;

                    TriWeightData& TriData = TriWeights[RealTriIdx];

                    int MissingBones = 0;
                    int PresentBones = 0;
                    {for(granny_int32x BoneIdx = 0; BoneIdx < TriData.NumUsedBones; ++BoneIdx)
                    {
                        if (!NewMesh->BoneAdded[TriData.UsedIndices[BoneIdx]])
                            MissingBones++;
                        else
                            PresentBones++;
                    }}

                    if (MissingBones == 0)
                    {
                        // If there are no bones missing, we can just
                        // add the triangle directly to the new
                        // mesh...
                        {for(int i = 0; i < 3; i++)
                        {
                            if (NewMesh->NewVertIndices[TriData.VertIndices[i]] == -1)
                            {
                                NewMesh->NewVertIndices[TriData.VertIndices[i]] = NewMesh->NumNewVerts++;
                            }

                            NewMesh->TriIndices[NewMesh->NumIndicesUsed++] = NewMesh->NewVertIndices[TriData.VertIndices[i]];
                        }}

                        NewMesh->TriAdded[RealTriIdx] = true;
                        NewMesh->TotalTris++;
                    }
                    else if (MissingBones < BestNumAdds ||
                             (MissingBones == BestNumAdds && BestSharedBones < PresentBones))
                    {
                        BestTri     = RealTriIdx;
                        BestNumAdds = MissingBones;
                        BestSharedBones = PresentBones;
                    }
                }}

                // No more tris?
                if (BestTri == -1)
                    break;

                // To many added bones?
                if (NewMesh->TotalBones + BestNumAdds > BoneLimit)
                    break;

                // Add the bones from BestTri, since we know that it's valid...
                TriWeightData& BestTriData = TriWeights[BestTri];
                {for(granny_int32x BoneIdx = 0; BoneIdx < BestTriData.NumUsedBones; ++BoneIdx)
                {
                    if (!NewMesh->BoneAdded[BestTriData.UsedIndices[BoneIdx]])
                    {
                        NewMesh->BoneAdded[BestTriData.UsedIndices[BoneIdx]] = true;
                        NewMesh->TotalBones++;
                    }
                }}

                // Just loop around, we'll add in all the tris that
                // are handled by these bone adds in the
                // NumBonesMissing == 0 case above...
            }

            if (KickoffMesh(OriginalMesh, ThisGroup, NewMesh, LocalNewMeshes,
                            AddNamePostfix, NumKickedOff, TempArena) == false)
            {
                ErrOut("failed to kickoff a submesh\n");
                return false;
            }
            else
            {
                ++NumKickedOff;
            }

            // Clear the submesh vars, and keep going...
            NewMesh->ClearSubMesh();
        }
    }}

    if (RemapModelBindings(Info, OriginalMesh, LocalNewMeshes, TempArena) == false)
    {
        ErrOut("failure while remapping model bindings for mesh %s\n", OriginalMesh->Name);
        return false;
    }

    if (RemapVertexDatas(Info, OriginalMesh, LocalNewMeshes, TempArena) == false)
    {
        ErrOut("failure while remapping vertex datas for mesh %s\n", OriginalMesh->Name);
        return false;
    }

    if (RemapTopologies(Info, OriginalMesh, LocalNewMeshes, TempArena) == false)
    {
        ErrOut("failure while remapping topologies for mesh %s\n", OriginalMesh->Name);
        return false;
    }

    // Into the full array...
    NewMeshes.insert(NewMeshes.end(), LocalNewMeshes.begin(), LocalNewMeshes.end());

    // Clean up our temp variables
    delete NewMesh;
    delete [] WeightVerts;
    delete [] TriWeights;
    delete [] OriginalIndices;

    return true;
}

bool RemapModelBindings(granny_file_info* Info,
                        granny_mesh*      OriginalMesh,
                        vector<granny_mesh*>& NewMeshes,
                        granny_memory_arena* TempArena)
{
    {for(granny_int32x ModelIdx = 0; ModelIdx < Info->ModelCount; ++ModelIdx)
    {
        granny_model* Model = Info->Models[ModelIdx];

        // Do we need to touch the bindings?
        bool found = false;
        {for(granny_int32x Binding = 0; Binding < Model->MeshBindingCount; ++Binding)
        {
            if (Model->MeshBindings[Binding].Mesh == OriginalMesh)
            {
                found = true;
                break;
            }
        }}

        if (found)
        {
            granny_int32 NewBindingCount =
                ((Model->MeshBindingCount - 1) + (int)NewMeshes.size());

            granny_model_mesh_binding* NewBindings =
                PushArray(TempArena, NewBindingCount, granny_model_mesh_binding);

            int current = 0;
            {for(granny_int32x Binding = 0; Binding < Model->MeshBindingCount; ++Binding)
            {
                if (Model->MeshBindings[Binding].Mesh == OriginalMesh)
                    continue;

                NewBindings[current++] = Model->MeshBindings[Binding];
            }}

            {for(size_t i = 0; i < NewMeshes.size(); ++i)
            {
                NewBindings[current++].Mesh = NewMeshes[i];
            }}

            Model->MeshBindingCount = NewBindingCount;
            Model->MeshBindings = NewBindings;
        }
    }}

    return true;
}


bool RemapVertexDatas(granny_file_info* Info,
                      granny_mesh*      OriginalMesh,
                      vector<granny_mesh*>& NewMeshes,
                      granny_memory_arena* TempArena)
{
    // Do we need to touch the array?
    bool found = false;
    {for(granny_int32x VDIdx = 0; VDIdx < Info->VertexDataCount; ++VDIdx)
    {
        if (Info->VertexDatas[VDIdx] == OriginalMesh->PrimaryVertexData)
        {
            found = true;
            break;
        }
    }}

    if (found)
    {
        granny_int32 NewArrayCount = (Info->VertexDataCount - 1) + (int)NewMeshes.size();
        granny_vertex_data** NewArray =
            PushArray(TempArena, NewArrayCount, granny_vertex_data*);

        int current = 0;
        {for(granny_int32x VDIdx = 0; VDIdx < Info->VertexDataCount; ++VDIdx)
        {
            if (Info->VertexDatas[VDIdx] == OriginalMesh->PrimaryVertexData)
                continue;

            NewArray[current++] = Info->VertexDatas[VDIdx];
        }}

        {for(size_t i = 0; i < NewMeshes.size(); ++i)
        {
            NewArray[current++] = NewMeshes[i]->PrimaryVertexData;
        }}

        Info->VertexDataCount = NewArrayCount;
        Info->VertexDatas = NewArray;
    }

    return true;
}


bool RemapTopologies(granny_file_info* Info,
                     granny_mesh*      OriginalMesh,
                     vector<granny_mesh*>& NewMeshes,
                     granny_memory_arena* TempArena)
{
    // Do we need to touch the array?
    bool found = false;
    {for(granny_int32x VDIdx = 0; VDIdx < Info->TriTopologyCount; ++VDIdx)
    {
        if (Info->TriTopologies[VDIdx] == OriginalMesh->PrimaryTopology)
        {
            found = true;
            break;
        }
    }}

    if (found)
    {
        granny_int32 NewArrayCount = (Info->TriTopologyCount - 1) + (int)NewMeshes.size();
        granny_tri_topology** NewArray =
            PushArray(TempArena, NewArrayCount, granny_tri_topology*);

        int current = 0;
        {for(granny_int32x VDIdx = 0; VDIdx < Info->TriTopologyCount; ++VDIdx)
        {
            if (Info->TriTopologies[VDIdx] == OriginalMesh->PrimaryTopology)
                continue;

            NewArray[current++] = Info->TriTopologies[VDIdx];
        }}

        {for(size_t i = 0; i < NewMeshes.size(); ++i)
        {
            NewArray[current++] = NewMeshes[i]->PrimaryTopology;
        }}

        Info->TriTopologyCount = NewArrayCount;
        Info->TriTopologies = NewArray;
    }

    return true;
}

// ======================================================================
// ======================================================================

NewMeshIntermediate::NewMeshIntermediate(int NumBones,
                                         int NumVerts,
                                         int NumTris)
  : NumOrigBones(NumBones),
    NumOrigVerts(NumVerts),
    NumOrigTris(NumTris)

{
    BoneAdded  = new bool[NumBones];
    TotalBones = 0;

    NewVertIndices = new int[NumVerts];
    NumNewVerts    = 0;

    TriIndices = new int[NumTris * 3];
    NumIndicesUsed = 0;

    TriAdded = new bool[NumTris];
    NumTris = 0;
}

NewMeshIntermediate::~NewMeshIntermediate()
{
    delete [] BoneAdded;
    delete [] NewVertIndices;
    delete [] TriIndices;
    delete [] TriAdded;
}

void NewMeshIntermediate::ClearSubMesh()
{
    memset(BoneAdded, 0, sizeof(bool) * NumOrigBones);

    {for(int i = 0; i < NumOrigVerts; ++i)
    {
        NewVertIndices[i] = -1;
    }}
    {for(int i = 0; i < NumOrigTris*3; ++i)
    {
        TriIndices[i] = 0;
    }}

    TotalBones = 0;
    NumNewVerts = 0;
    NumIndicesUsed = 0;
}

void NewMeshIntermediate::ClearAll()
{
    ClearSubMesh();
    memset(TriAdded, 0, sizeof(bool) * NumOrigTris);
    TotalTris = 0;
}


static char const* HelpString =
    (" BonesPerMesh breaks up granny_mesh objects so that they can be rendered\n"
     " without requiring more than a set number of bone matrices.  This is\n"
     " useful when your source art refers to 100 bones, but you only have\n"
     " constant register space for 50 matrices, for instance\n"
     "\n"
     " Specify the maximum number of bones with -bonelimit <N>.  Meshes that\n"
     " have fewer than N bones will be untouched.\n"
     " \n"
     "    preprocessor BonesPerMesh char.gr2 -output char_proc.gr2 -bonelimit 50\n"
     "    preprocessor BonesPerMesh char.gr2 -output char.gr2 -bonelimit 50\n"
     );

static CommandRegistrar RegBonesPerMesh(BonesPerMesh,
                                        "BonesPerMesh",
                                        "Limits meshes to the number of bones specified with \"-bonelimit N\"",
                                        HelpString);


