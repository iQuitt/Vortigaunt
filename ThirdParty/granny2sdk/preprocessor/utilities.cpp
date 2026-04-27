// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "utilities.h"
#include <granny.h>
#include <math.h>
#include <vector>
#include <algorithm>

using namespace std;

static granny_real32
SquaredDistanceBetweenN(granny_int32x ElementCount,
                        granny_real32 const* Vector0,
                        granny_real32 const* Vector1)
{
    granny_real32 Result = 0.0f;
    while(ElementCount--)
    {
        granny_real32 ElemDist = (*Vector0++ - *Vector1++);
        Result += ElemDist * ElemDist;
    }

    return Result;
}

float
GetPosError(granny_transform& One, granny_transform& Two)
{
    return sqrt(SquaredDistanceBetweenN(3, One.Position, Two.Position));
}

float
GetOriError(granny_transform& One, granny_transform& Two)
{
    float InnerProd = 0.0f;
    for (int i = 0; i < 4; i++)
        InnerProd += One.Orientation[i] * Two.Orientation[i];

    // We want the neighborhooded distance
    InnerProd = fabsf(InnerProd);

    return ((1.0f - InnerProd) * (1.0f - InnerProd));
}

float
GetSSError(granny_transform& One, granny_transform& Two)
{
    return sqrt(SquaredDistanceBetweenN(9, (granny_real32*)One.ScaleShear, (granny_real32*)Two.ScaleShear));
}

void
TransformByMatrix(granny_p3_vertex&  Vert,
                  granny_matrix_4x4& Matrix)
{
    granny_p3_vertex NewVert = { { 0 } };
    for (int i = 0; i < 3; ++i)
    {
        NewVert.Position[i] = (Vert.Position[0] * Matrix[0][i] +
                               Vert.Position[1] * Matrix[1][i] +
                               Vert.Position[2] * Matrix[2][i] +
                               Matrix[3][i]);
    }

    Vert = NewVert;
}


void
AddToBounds(granny_bone_binding& Binding,
            granny_pwn343_vertex& Vert,
            granny_matrix_4x4& Matrix)
{
    granny_p3_vertex NewVert;
    for (int i = 0; i < 3; ++i)
    {
        NewVert.Position[i] = (Vert.Position[0] * Matrix[0][i] +
                               Vert.Position[1] * Matrix[1][i] +
                               Vert.Position[2] * Matrix[2][i] +
                               Matrix[3][i]);
    }

    for (int i = 0; i < 3; ++i)
    {
        Binding.OBBMin[i] = min(Binding.OBBMin[i], NewVert.Position[i]);
        Binding.OBBMax[i] = max(Binding.OBBMax[i], NewVert.Position[i]);
    }
}

bool
RecomputeMeshBoneBounds(granny_model* Model, granny_mesh* Mesh)
{
    int VertexCount = GrannyGetMeshVertexCount(Mesh);
    if (VertexCount == 0)
        return true;

    if (GrannyMeshIsRigid(Mesh))
    {
        if (Mesh->BoneBindingCount != 1)
        {
            return false;
        }

        // This one is easy, since there won't be any weights.  We can just extract the
        // position, bound the verts, and we're done.
        vector<granny_p3_vertex> Vertices(VertexCount);
        GrannyCopyMeshVertices(Mesh, GrannyP3VertexType, &Vertices[0]);

        granny_int32x BoneIndex;
        if (!GrannyFindBoneByNameLowercase(Model->Skeleton,
                                           Mesh->BoneBindings[0].BoneName,
                                           &BoneIndex))
        {
            return false;
        }

        for (int i = 0; i < VertexCount; ++i)
        {
            TransformByMatrix(Vertices[i], Model->Skeleton->Bones[BoneIndex].InverseWorld4x4);
        }

        granny_p3_vertex Min = Vertices[0];
        granny_p3_vertex Max = Vertices[0];
        for (int i = 1; i < VertexCount; ++i)
        {
            for (int e = 0; e < 3; ++e)
            {
                Min.Position[e] = min(Min.Position[e], Vertices[i].Position[e]);
                Max.Position[e] = max(Max.Position[e], Vertices[i].Position[e]);
            }
        }

        for (int e = 0; e < 3; ++e)
        {
            Mesh->BoneBindings[0].OBBMin[e] = Min.Position[e];
            Mesh->BoneBindings[0].OBBMax[e] = Max.Position[e];
        }
    }
    else
    {
        // A lot of dupe here, of course, and we only support up to 4 weights...

        vector<granny_pwn343_vertex> Vertices(VertexCount);
        GrannyCopyMeshVertices(Mesh, GrannyPWN343VertexType, &Vertices[0]);
        GrannyOneNormalizeWeights(VertexCount, GrannyPWN343VertexType, &Vertices[0]);

        for (int Binding = 0; Binding < Mesh->BoneBindingCount; Binding++)
        {
            Mesh->BoneBindings[Binding].OBBMin[0] =  1e38f;
            Mesh->BoneBindings[Binding].OBBMin[1] =  1e38f;
            Mesh->BoneBindings[Binding].OBBMin[2] =  1e38f;
            Mesh->BoneBindings[Binding].OBBMax[0] = -1e38f;
            Mesh->BoneBindings[Binding].OBBMax[1] = -1e38f;
            Mesh->BoneBindings[Binding].OBBMax[2] = -1e38f;
        }

        granny_mesh_binding * Binding = GrannyNewMeshBinding(Mesh, Model->Skeleton, Model->Skeleton);
        granny_int32x const* FromIndices = GrannyGetMeshBindingFromBoneIndices(Binding);

        for (int i = 0; i < VertexCount; ++i)
        {
            granny_pwn343_vertex& Vert = Vertices[i];

            for (int w = 0; w < 4; ++w)
            {
                if (Vert.BoneWeights[w] == 0)
                    continue;

                int BoneIndex = Vert.BoneIndices[w];
                AddToBounds(Mesh->BoneBindings[BoneIndex], Vert,
                            Model->Skeleton->Bones[FromIndices[BoneIndex]].InverseWorld4x4);
            }
        }

        for (int Binding = 0; Binding < Mesh->BoneBindingCount; Binding++)
        {
            if (Mesh->BoneBindings[Binding].OBBMin[0] == 1e38f)
            {
                memset(Mesh->BoneBindings[Binding].OBBMin, 0, 3*sizeof(float));
                memset(Mesh->BoneBindings[Binding].OBBMax, 0, 3*sizeof(float));
            }
        }
    }

    return true;
}

bool RecomputeAllBoneBounds(granny_file_info* Info)
{
    vector<bool> BoundsUpdated(Info->MeshCount, false);
    {for (int Idx = 0; Idx < Info->ModelCount; ++Idx)
    {
        granny_model* Model = Info->Models[Idx];
        {for (int MeshIdx = 0; MeshIdx < Model->MeshBindingCount; ++MeshIdx)
        {
            granny_mesh* Mesh = Model->MeshBindings[MeshIdx].Mesh;
            pp_assert(Mesh);

            int Index = -1;
            {for (int FIdx = 0; FIdx < Info->MeshCount; ++FIdx)
            {
                if (Mesh == Info->Meshes[FIdx])
                {
                    Index = FIdx;
                    break;
                }
            }}
            pp_assert(Index != -1);

            if (Index == -1 || BoundsUpdated[Index])
            {
                ErrOut("Bad mesh bindings found!");
                return false;
            }

            if (!RecomputeMeshBoneBounds(Model, Mesh))
            {
                ErrOut("Error recomputing bounds!");
                return false;
            }

            BoundsUpdated[Index] = true;
        }}
    }}

    {for (int Idx = 0; Idx < Info->MeshCount; ++Idx)
    {
        if (!BoundsUpdated[Idx])
        {
            WarnOut("Unable to update mesh bounds for %s\n", Info->Meshes[Idx]->Name);
        }
    }}

    return true;
}
