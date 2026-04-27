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
#include <fstream>
#include <strstream>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

struct bone_specification
{
    string BoneName;
    float  LODValue;
};


bool CreateLODSpec(char const* Filename,
                   vector<bone_specification>& Spec)
{
    ifstream input(Filename);
    if (!input.is_open())
        return false;

    // This does no error checking, so the format must be correct...
    while (!input.eof() )
    {
        bone_specification NewEntry;
        input >> NewEntry.BoneName;
        input >> NewEntry.LODValue;

        if (!NewEntry.BoneName.empty())
            Spec.push_back(NewEntry);
    }

    return true;
}


granny_file_info*
OverrideSkeletonLOD(char const*          OriginalFilename,
                    char const*          OutputFilename,
                    granny_file_info*    Info,
                    key_value_pair*      KeyValues,
                    granny_int32x        NumKeyValues,
                    granny_memory_arena* TempArena)
{
    char const* LODSpecFile = FindFirstValueForKey(KeyValues, NumKeyValues, "lodspec");
    if (LODSpecFile == 0)
    {
        ErrOut("must specify a lod specification file with \"-output <filename>\"\n");
        return false;
    }

    vector<bone_specification> Spec;
    if (!CreateLODSpec(LODSpecFile, Spec))
    {
        ErrOut("failed to create a specification from %s\n", LODSpecFile);
        return false;
    }

    float DefaultLOD = 0.0f;
    {
        char const* DefaultLODString = FindFirstValueForKey(KeyValues, NumKeyValues, "defaultlod");
        if (DefaultLODString)
        {
            DefaultLOD = (float)atof(DefaultLODString);
        }
    }


    {for (int ModelIdx = 0; ModelIdx < Info->ModelCount; ++ModelIdx)
    {
        granny_model* Model = Info->Models[ModelIdx];
        if (Model == 0)
            continue;

        granny_skeleton* Skeleton = Model->Skeleton;
        if (Skeleton == 0)
            continue;

        granny_skeleton_builder* Builder = GrannyBeginSkeleton(Skeleton->BoneCount);

        {for (int BoneIdx = 0; BoneIdx < Skeleton->BoneCount; ++BoneIdx)
        {
            GrannyAddBoneWithInverse(Builder,
                                     Skeleton->Bones[BoneIdx].Name,
                                     Skeleton->Bones[BoneIdx].LocalTransform.Position,
                                     Skeleton->Bones[BoneIdx].LocalTransform.Orientation,
                                     (granny_real32*)Skeleton->Bones[BoneIdx].LocalTransform.ScaleShear,
                                     &Skeleton->Bones[BoneIdx].InverseWorld4x4);
            GrannySetBoneParent(Builder, BoneIdx, Skeleton->Bones[BoneIdx].ParentIndex);
            GrannySetBoneLODError(Builder, BoneIdx, DefaultLOD);
        }}

        {for (size_t SpecIdx = 0; SpecIdx < Spec.size(); ++SpecIdx)
        {
            granny_int32x BoneIndex;
            if (GrannyFindBoneByNameLowercase(Skeleton, Spec[SpecIdx].BoneName.c_str(), &BoneIndex))
            {
                GrannySetBoneLODError(Builder, BoneIndex, Spec[SpecIdx].LODValue);
            }
            else
            {
                ErrOut("Couldn't find bone: \"%s\" in the file\n", Spec[SpecIdx].BoneName.c_str());
            }
        }}

        vector<granny_int32x> RemappingTable(Skeleton->BoneCount);
        granny_skeleton* NewSkeleton = GrannyEndSkeleton(Builder, &RemappingTable[0]);
        if (NewSkeleton)
        {
            // Set the name of the new skeleton
            NewSkeleton->Name      = Skeleton->Name;
            Skeleton->ExtendedData = Skeleton->ExtendedData;

            // The remapping table takes old indices to new indices so we can copy over
            // the other attributes of the bones
            {for (int OldBoneIdx = 0; OldBoneIdx < Skeleton->BoneCount; ++OldBoneIdx)
            {
                int NewBoneIdx = RemappingTable[OldBoneIdx];

                granny_bone& NewBone = NewSkeleton->Bones[NewBoneIdx];
                granny_bone& OldBone = Skeleton->Bones[OldBoneIdx];

                // Just the extended data, everything else is created by the skeleton
                // builder.
                NewBone.ExtendedData = OldBone.ExtendedData;
            }}

            Model->Skeleton = NewSkeleton;
        }
        else
        {
            ErrOut("Unable to create new skeleton");
        }
    }}

    return Info;
}

static CommandRegistrar RegOverrideSkelLOD(OverrideSkeletonLOD, "OverrideSkeletonLOD",
                                           "Alters the LOD specification of the models in a granny file");
