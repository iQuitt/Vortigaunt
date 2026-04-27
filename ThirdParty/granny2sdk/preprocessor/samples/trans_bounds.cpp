// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"
#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <vector>

using namespace std;

struct Vec3
{
    granny_real32 x, y, z;
};

inline void UpdateMin(Vec3& Base, Vec3 const& New)
{
    Base.x = min(Base.x, New.x);
    Base.y = min(Base.y, New.y);
    Base.z = min(Base.z, New.z);
}

inline void UpdateMax(Vec3& Base, Vec3 const& New)
{
    Base.x = max(Base.x, New.x);
    Base.y = max(Base.y, New.y);
    Base.z = max(Base.z, New.z);
}

inline void GetPositionFromArray(granny_real32 const* WPMatrix,
                                 Vec3& Position)
{
    Position.x = WPMatrix[12];
    Position.y = WPMatrix[13];
    Position.z = WPMatrix[14];
}


granny_file_info*
TranslationBounds(char const*          OriginalFilename,
                  char const*          OutputFilename,
                  granny_file_info*    Info,
                  key_value_pair*      KeyValues,
                  granny_int32x        NumKeyValues,
                  granny_memory_arena* TempArena)
{
    if (Info->AnimationCount != 1 || Info->Animations[0] == 0)
    {
        ErrOut("this version of the command can't handle zero or multiple animations\n");
        return false;
    }

    if (Info->ModelCount == 0)
    {
        ErrOut("this version of the command requires at least one model to work with\n");
        return false;
    }

    // We'll use this in the variant builder...
    granny_string_table* StringTable = GrannyNewStringTable();

    granny_animation* Animation = Info->Animations[0];
    for (int ModelIdx = 0; ModelIdx < Info->ModelCount; ++ModelIdx)
    {
        granny_model* Model = Info->Models[ModelIdx];
        if (Model == 0)
            continue;

        granny_int32x TGIdx;
        if (!GrannyFindTrackGroupForModel(Animation, Model->Name, &TGIdx))
            continue;

        granny_skeleton* Skeleton = Model->Skeleton;
        pp_assert(Skeleton);

        // Create the instance, and play the animation on it
        granny_model_instance* Instance = GrannyInstantiateModel(Model);
        {
            granny_controlled_animation_builder* Builder =
                GrannyBeginControlledAnimation(0.0f, Animation);
            GrannySetTrackGroupTarget(Builder, TGIdx, Instance);

            // Since we want to get the results for the root bone as well, turn off any
            // motion extraction
            GrannySetTrackGroupAccumulation(Builder, TGIdx, GrannyNoAccumulation);
            granny_control* Control = GrannyEndControlledAnimation(Builder);

            GrannySetControlLoopCount(Control, 0);
            GrannyFreeControlOnceUnused(Control);
        }

        granny_local_pose* LocalPose = GrannyNewLocalPose(Skeleton->BoneCount);
        granny_world_pose* WorldPose = GrannyNewWorldPose(Skeleton->BoneCount);

        Vec3* InitialPos = PushArray(TempArena, Skeleton->BoneCount, Vec3);
        Vec3* MinPos = PushArray(TempArena, Skeleton->BoneCount, Vec3);
        Vec3* MaxPos = PushArray(TempArena, Skeleton->BoneCount, Vec3);

        for (granny_real32 Time = 0.0f; Time <= Animation->Duration; Time += Animation->TimeStep)
        {
            GrannySetModelClock(Instance, Time);
            GrannySampleModelAnimations(Instance, 0, Skeleton->BoneCount, LocalPose);
            GrannyBuildWorldPose(Model->Skeleton,
                                 0, Skeleton->BoneCount,
                                 LocalPose, NULL, WorldPose);
            if (Time == 0.0f)
            {
                // First loop, init bounds and the init_pos
                for (int i = 0; i < Skeleton->BoneCount; ++i)
                {
                    Vec3 Pos;
                    GetPositionFromArray(GrannyGetWorldPose4x4(WorldPose, i), Pos);
                    InitialPos[i] = Pos;
                    MinPos[i] = Pos;
                    MaxPos[i] = Pos;
                }
            }
            else
            {
                // Update the bounds
                for (int i = 0; i < Skeleton->BoneCount; ++i)
                {
                    Vec3 Pos;
                    GetPositionFromArray(GrannyGetWorldPose4x4(WorldPose, i), Pos);
                    UpdateMin(MinPos[i], Pos);
                    UpdateMax(MaxPos[i], Pos);
                }
            }
        }

        // Ok, we now have the three arrays we want.  Create a variant that we'll attach
        // to the track_group
        {
            granny_variant_builder* VariantBuilder = GrannyBeginVariant(StringTable);

            pp_assert(sizeof(InitialPos[0]) == sizeof(granny_triple));
            GrannyAddDynamicArrayMember(VariantBuilder, "InitialPositions",
                                        Skeleton->BoneCount, GrannyTripleType,
                                        &InitialPos[0]);
            GrannyAddDynamicArrayMember(VariantBuilder, "MinPositions",
                                        Skeleton->BoneCount, GrannyTripleType,
                                        &MinPos[0]);
            GrannyAddDynamicArrayMember(VariantBuilder, "MaxPositions",
                                        Skeleton->BoneCount, GrannyTripleType,
                                        &MaxPos[0]);

            granny_int32x TypeSize = GrannyGetResultingVariantTypeSize(VariantBuilder);
            granny_int32x ObjSize  = GrannyGetResultingVariantObjectSize(VariantBuilder);

            granny_track_group* TrackGroup = Animation->TrackGroups[TGIdx];
            pp_assert(TrackGroup);

            GrannyEndVariantInPlace(VariantBuilder,
                                    PushArray(TempArena, TypeSize, granny_uint8),
                                    &TrackGroup->ExtendedData.Type,
                                    PushArray(TempArena, ObjSize, granny_uint8),
                                    &TrackGroup->ExtendedData.Object);
        }

        // Cleanup
        GrannyFreeLocalPose(LocalPose);
        GrannyFreeWorldPose(WorldPose);
        GrannyFreeModelInstance(Instance);
    }

	return Info;
}


static CommandRegistrar RegTranslationBounds(TranslationBounds,
                                             "TranslationBounds",
                                             "Attached bounds information for the input animation");
