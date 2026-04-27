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

static granny_file_info*
MoveMeshesToOrigin(char const*          OriginalFilename,
                   char const*          OutputFilename,
                   granny_file_info*    Info,
                   key_value_pair*      KeyValues,
                   granny_int32x        NumKeyValues,
                   granny_memory_arena* TempArena)
{
    {for (int ModelIdx = 0; ModelIdx < Info->ModelCount; ++ModelIdx)
    {
        granny_model* Model = Info->Models[ModelIdx];
        if (!Model)
            continue;

        if (!Model->Skeleton)
            continue;

        granny_transform Initial;
        GrannyMultiply(&Initial, &Model->InitialPlacement, &Model->Skeleton->Bones[0].LocalTransform);

        granny_transform InvInitial;
        GrannyBuildInverse(&InvInitial, &Model->InitialPlacement);

        granny_real32 Affine3[3] = { InvInitial.Position[0],
                                     InvInitial.Position[1],
                                     InvInitial.Position[2] };

        granny_real32 Linear3x3[9];
        granny_real32 InvLinear3x3[9];
        GrannyBuildCompositeTransform(&Model->InitialPlacement, 3, Linear3x3);
        GrannyBuildCompositeTransform(&InvInitial, 3, InvLinear3x3);

#if 1
        {for (int MeshIdx = 0; MeshIdx < Model->MeshBindingCount; ++MeshIdx)
        {
            granny_mesh* Mesh = Model->MeshBindings[MeshIdx].Mesh;
            if (!Mesh)
                continue;

            // Since we're going to move the skeleton, preserve the bone bounding boxes...
            vector<granny_bone_binding> Bindings(Mesh->BoneBindingCount);
            memcpy(&Bindings[0], Mesh->BoneBindings, sizeof(granny_bone_binding) * Mesh->BoneBindingCount);

            GrannyTransformMesh(Mesh,
                                Affine3, Linear3x3, InvLinear3x3,
                                1e-5f, 1e-5f,
                                GrannyRenormalizeNormals | GrannyReorderTriangleIndices);

            memcpy(Mesh->BoneBindings, &Bindings[0], sizeof(granny_bone_binding) * Mesh->BoneBindingCount);
        }}
#endif

        granny_transform OldPlacement = Model->InitialPlacement;
        GrannyMultiply(&Model->InitialPlacement,
            &OldPlacement,
            &Model->Skeleton->Bones[0].LocalTransform);
        GrannyMakeIdentity(&Model->Skeleton->Bones[0].LocalTransform);

        vector<granny_transform> XForms(Model->Skeleton->BoneCount);
        GrannyBuildSkeletonRelativeTransforms(
            sizeof(granny_bone), &Model->Skeleton->Bones[0].LocalTransform,
            sizeof(granny_bone), &Model->Skeleton->Bones[0].ParentIndex,
            Model->Skeleton->BoneCount,
            sizeof(granny_transform),
            &XForms[0]);

        {for (int Bone = 0; Bone < Model->Skeleton->BoneCount; ++Bone)
        {
            granny_transform inv;
            GrannyBuildInverse(&inv, &XForms[Bone]);

            GrannyBuildCompositeTransform4x4(&inv, (granny_real32*)Model->Skeleton->Bones[Bone].InverseWorld4x4);
        }}
    }}

    return Info;
}

static CommandRegistrar RegCompressVertSample(MoveMeshesToOrigin, "MoveMeshesToOrigin",
                                              "Zeros the starting position of any meshes referenced by a model with a non-identity InitialPlacement");
