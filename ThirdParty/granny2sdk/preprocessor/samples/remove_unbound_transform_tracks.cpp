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
#include <vector>

using namespace std;

// Command details:
//  -output key must be present
//
//  RemoveUnboundTransformTracks mesh_model.gr2 anim.gr2 -output new_anim.gr2


static void
FilterAnimationForModel(granny_model*        Model,
                        granny_animation*    Animation,
                        granny_memory_arena* TempArena)
{
    granny_skeleton* Skeleton = Model->Skeleton;
    std::vector<bool> Used(Skeleton->BoneCount, false);

    {for (int Idx = 0; Idx < Model->MeshBindingCount; ++Idx)
    {
        granny_mesh* Mesh = Model->MeshBindings[Idx].Mesh;
        if (!Mesh)
            continue;

        for (int i = 0; i < Mesh->BoneBindingCount; ++i)
        {
            granny_int32x WalkIdx;
            if (GrannyFindBoneByNameLowercase(Skeleton, Mesh->BoneBindings[i].BoneName, &WalkIdx))
            {
                while (WalkIdx != GrannyNoParentBone)
                {
                    Used[WalkIdx] = true;
                    WalkIdx = Skeleton->Bones[WalkIdx].ParentIndex;
                }
            }
        }
    }}

    granny_int32x TTIdx;
    if (GrannyFindTrackGroupForModel(Animation, Model->Name, &TTIdx))
    {
        granny_track_group* TG = Animation->TrackGroups[TTIdx];

        granny_transform_track* Tracks =
            PushArray(TempArena, TG->TransformTrackCount, granny_transform_track);
        granny_int32x UsedTracks = 0;

        {for (int Idx = 0; Idx < TG->TransformTrackCount; ++Idx)
        {
            granny_int32x BoneIdx;
            if (GrannyFindBoneByNameLowercase(Skeleton, TG->TransformTracks[Idx].Name, &BoneIdx))
            {
                if (Used[BoneIdx] == true)
                {
                    Tracks[UsedTracks++] = TG->TransformTracks[Idx];
                }
                else
                {
                    printf("Removing: %s\n", TG->TransformTracks[Idx].Name);
                }
            }
        }}

        TG->TransformTracks = Tracks;
        TG->TransformTrackCount = UsedTracks;
    }
}


bool
RemoveUnboundTransformTracks(input_file*     InputFiles,
                             granny_int32x   NumInputFiles,
                             key_value_pair* KeyValues,
                             granny_int32x   NumKeyValues,
                             granny_memory_arena* TempArena)
{
    RequireKey("output", "must specify an output file with \"-output <filename>\"\n");

    if (NumInputFiles != 2)
    {
        ErrOut("must specify a mesh/model and a anim file (they may be the same.)\n");
        return false;
    }

    granny_file_info* MeshInfo = ExtractFileInfo(InputFiles[0]);
    granny_file_info* AnimInfo = ExtractFileInfo(InputFiles[1]);
    if (MeshInfo == 0 || AnimInfo == 0)
    {
        ErrOut("unable to get granny_file_info from inputs\n");
        return false;
    }

    if (AnimInfo->AnimationCount == 0)
    {
        ErrOut("no animation?\n");
        return false;
    }

    bool SomeModelHasMeshes = false;
    {for (int Idx = 0; Idx < MeshInfo->ModelCount; ++Idx)
    {
        if (MeshInfo->Models[Idx]->MeshBindingCount != 0)
            SomeModelHasMeshes = true;
    }}

    if (SomeModelHasMeshes == false)
    {
        ErrOut("no models with bound meshes!\n");
        return false;
    }

    {for (int MeshIdx = 0; MeshIdx < MeshInfo->ModelCount; ++MeshIdx)
    {
        {for (int AnimIdx = 0; AnimIdx < AnimInfo->AnimationCount; ++AnimIdx)
        {
            FilterAnimationForModel(MeshInfo->Models[MeshIdx],
                                    AnimInfo->Animations[AnimIdx],
                                    TempArena);
        }}
    }}

    // Write out the result
    return SimpleWriteInfo(KeyValues, NumKeyValues, AnimInfo, TempArena);
}


static CommandRegistrar RegLODCalc(RemoveUnboundTransformTracks, "RemoveUnboundTransformTracks",
                                   "Given a model file and an animation file, strips tracks that cannot affect the bound meshes");

