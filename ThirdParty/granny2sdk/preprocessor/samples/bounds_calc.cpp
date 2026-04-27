// ========================================================================
// $File: //jeffr/granny_29/preprocessor/lod_calc.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"
#include <stdio.h>
#include <assert.h>
#include <vector>
#include <algorithm>
#include <string>
#include <string.h>
#include <stdlib.h>

using namespace std;

bool BoundsCalc(input_file*     InputFiles,
                granny_int32x   NumInputFiles,
                key_value_pair* KeyValues,
                granny_int32x   NumKeyValues,
                granny_memory_arena* TempArena)
{
    RequireKey("output", "must specify an output file with \"-output <filename>\"\n");

    if (NumInputFiles != 2)
    {
        ErrOut("must supply model and animation (if they are the same, specify twice)\n");
        return false;
    }

    granny_file_info* ModelInfo = ExtractFileInfo(InputFiles[0]);
    if (ModelInfo == 0)
    {
        ErrOut("unable to obtain a granny_file_info from the input model file\n");
        return false;
    }

    granny_file_info* AnimInfo = ExtractFileInfo(InputFiles[1]);
    if (AnimInfo == 0)
    {
        ErrOut("unable to obtain a granny_file_info from the input animation file\n");
        return false;
    }

    if (ModelInfo->MeshCount == 0)
    {
        ErrOut("ModelFile contains no mesh information with which to make the LOD calcs\n");
        return false;
    }

    if (AnimInfo->AnimationCount == 0)
    {
        ErrOut("AnimFile contains no animations\n");
        return false;
    }

	// We'll use this in the variant builder...
    granny_string_table* StringTable = GrannyNewArenaStringTable(TempArena);

    // Compute mesh bindings for the models...
    granny_int32x NumBindings = 0;
    {for(granny_int32x Model = 0; Model < ModelInfo->ModelCount; ++Model)
    {
        NumBindings += ModelInfo->Models[Model]->MeshBindingCount;
    }}

    // We need to hang onto the BindingModels so we don't get confused
    // later.  The mesh binding only holds the skeleton, so we can't
    // backtrack from there.  Annoying.
    granny_mesh_binding** Bindings = PushArray(TempArena, NumBindings, granny_mesh_binding*);
    granny_model** BindingModels   = PushArray(TempArena, NumBindings, granny_model*);

    NumBindings = 0;
    {for(granny_int32x Model = 0; Model < ModelInfo->ModelCount; ++Model)
    {
        {for(granny_int32 Binding = 0; Binding < ModelInfo->Models[Model]->MeshBindingCount; ++Binding)
        {
            if (ModelInfo->Models[Model]->MeshBindings[Binding].Mesh)
            {
                granny_mesh*  ThisMesh  = ModelInfo->Models[Model]->MeshBindings[Binding].Mesh;
                granny_model* ThisModel = ModelInfo->Models[Model];

                BindingModels[NumBindings] = ThisModel;
                Bindings[NumBindings] =
                    GrannyNewMeshBinding(ThisMesh, ThisModel->Skeleton, ThisModel->Skeleton);
                ++NumBindings;
            }
        }}
    }}

    // This allocates more model_instances than necessary, but it's simpler to follow
    granny_real32 GlobalMinPoint[3] = {  1e38f,   1e38f,   1e38f };
    granny_real32 GlobalMaxPoint[3] = { -1e38f,  -1e38f,  -1e38f };
    for(granny_int32x Anim = 0; Anim < AnimInfo->AnimationCount; ++Anim)
    {
        granny_animation* Animation = AnimInfo->Animations[Anim];

		bool BoundsValid = false;
        for (int BindIdx = 0; BindIdx < NumBindings; ++BindIdx)
        {
            granny_int32x BoneCount         = BindingModels[BindIdx]->Skeleton->BoneCount;
            granny_local_pose*     ScrLocal = GrannyNewLocalPose(BoneCount);
            granny_world_pose*     ScrWorld = GrannyNewWorldPose(BoneCount);
            granny_model_instance* Instance = GrannyInstantiateModel(BindingModels[BindIdx]);

            // Known input and output vertex format
            granny_mesh* Mesh = GrannyGetMeshBindingSourceMesh(Bindings[BindIdx]);
            granny_control* Control = GrannyPlayControlledAnimation(0, Animation, Instance);
            if (Control)
            {
				bool LocalBoundValid = false;
                granny_real32 MinPoint[3] = {  1e38f,   1e38f,   1e38f };
                granny_real32 MaxPoint[3] = { -1e38f,  -1e38f,  -1e38f };

                granny_real32 t = 0;
                while (t < Animation->Duration)
                {
                    GrannySetModelClock(Instance, t);
                    GrannySampleModelAnimationsAccelerated(Instance, BoneCount, 0, ScrLocal, ScrWorld);

                    for (int BoneIdx = 0; BoneIdx < Mesh->BoneBindingCount; ++BoneIdx)
                    {
                        granny_bone_binding& Binding = Mesh->BoneBindings[BoneIdx];
                        int const MatrixIdx = GrannyGetMeshBindingFromBoneIndices(Bindings[BindIdx])[BoneIdx];

                        granny_real32 const* BoneToWorld = GrannyGetWorldPose4x4(ScrWorld, MatrixIdx);

                        // GrannyTransformBoundingBox takes [3x3] + [3], sadly.  Copy them out.
                        granny_real32 Linear3[9] = {
                            BoneToWorld[0], BoneToWorld[1], BoneToWorld[ 2], 
                            BoneToWorld[4], BoneToWorld[5], BoneToWorld[ 6], 
                            BoneToWorld[8], BoneToWorld[9], BoneToWorld[10]
                        };
                        granny_real32 Affine3[3] = {
                            BoneToWorld[12], BoneToWorld[13], BoneToWorld[14]
                        };

                        granny_real32 OBBMin[3] = { Binding.OBBMin[0], Binding.OBBMin[1], Binding.OBBMin[2] };
                        granny_real32 OBBMax[3] = { Binding.OBBMax[0], Binding.OBBMax[1], Binding.OBBMax[2] };
                        GrannyTransformBoundingBox(Affine3, Linear3, OBBMin, OBBMax);

                        for (int d = 0; d < 3; ++d)
                        {
							LocalBoundValid = true;
                            MinPoint[d] = min(MinPoint[d], OBBMin[d]);
                            MaxPoint[d] = max(MaxPoint[d], OBBMax[d]);
                        }
                    }

                    t += Animation->TimeStep;
                }

				if (LocalBoundValid)
				{
					BoundsValid = true;
					for (int d = 0; d < 3; ++d)
					{
						GlobalMinPoint[d] = min(MinPoint[d], GlobalMinPoint[d]);
						GlobalMaxPoint[d] = max(MaxPoint[d], GlobalMaxPoint[d]);
					}
				}

                GrannyFreeControl(Control);
            }

            GrannyFreeModelInstance(Instance);
            GrannyFreeLocalPose(ScrLocal);
            GrannyFreeWorldPose(ScrWorld);
        }

        if (BoundsValid)
        {
            granny_variant_builder* VariantBuilder = GrannyBeginVariant(StringTable);

            GrannyAddScalarArrayMember(VariantBuilder, "AnimAABBMin", 3, GlobalMinPoint);
            GrannyAddScalarArrayMember(VariantBuilder, "AnimAABBMax", 3, GlobalMaxPoint);

            granny_int32x TypeSize = GrannyGetResultingVariantTypeSize(VariantBuilder);
            granny_int32x ObjSize  = GrannyGetResultingVariantObjectSize(VariantBuilder);

            GrannyEndVariantInPlace(VariantBuilder,
                                    PushArray(TempArena, TypeSize, granny_uint8),
                                    &Animation->ExtendedData.Type,
                                    PushArray(TempArena, ObjSize, granny_uint8),
                                    &Animation->ExtendedData.Object);
        }
        else
        {
            WarnOut("skipping Animation: '%s'.  No meshes with vertices found.\n", Animation->Name);
        }
    }

	// Since we're using an Arena-based string table, we can release it at this point, the
    // strings are allocated into TempArena, and will remain valid.
    GrannyFreeStringTable(StringTable);

    // Free the bindings
    {for(granny_int32x Binding = 0; Binding < NumBindings; ++Binding)
    {
        GrannyFreeMeshBinding(Bindings[Binding]);
    }}

    // Write out the result
    return SimpleWriteInfo(KeyValues, NumKeyValues, AnimInfo, TempArena);
}

static char const* HelpString =
    (" BoundsCalc displays mesh bounding information for a given model/mesh\n"
     " animation pairing.  Call with the model/mesh file first, followed\n"
     " by the animation source. These may be the same file.\n"
     "\n"
     "    preprocessor BoundsCalc model.gr2 anim.gr2\n");

static CommandRegistrar RegBoundsCalc(BoundsCalc, "BoundsCalc",
                                      "Demonstrates Computing mesh bounds for an animation",
                                      HelpString);
