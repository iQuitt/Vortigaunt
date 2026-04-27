// ========================================================================
// $File: //jeffr/granny_29/preprocessor/remove_namespace.cpp $
// $DateTime: 2012/11/29 13:11:42 $
// $Change: 40490 $
// $Revision: #2 $
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <vector>
#include <algorithm>

using namespace std;

static char const*
ModifyName(char* InputName)
{
    char* LastPipe = strrchr(InputName, '|');
    
    if (!LastPipe)
        return InputName;

    // This isn't super awesome, but if the name /ends/ in a colon, I don't want to create
    // an empty string.  That really should never happen with path markers though...
    if (LastPipe+1 != '\0')
    {
        InputName = LastPipe + 1;
    }
    else
    {
        ErrOut("\"%s\" ends in a '|', leaving it alone\n", InputName);
    }

    return InputName;
}


struct VectorTrackMatch
{
    granny_vector_track* Track;
    granny_skeleton*     Skeleton;
    granny_int32x        BoneIndex;
};

static void
CreateTrackMatches(granny_file_info*         Info,
                   vector<VectorTrackMatch>& TrackMatches)
{
    // Ugly.  There's no way to do this other than brute force, really.

    for (int AnimIdx = 0; AnimIdx < Info->AnimationCount; ++AnimIdx)
    {
        granny_animation* Animation = Info->Animations[AnimIdx];
        if (!Animation)
            continue;

        for (int TrackGroupIdx = 0; TrackGroupIdx < Animation->TrackGroupCount; ++TrackGroupIdx)
        {
            granny_track_group* TrackGroup = Animation->TrackGroups[TrackGroupIdx];
            if (!TrackGroup)
                continue;

            for (int VecIdx = 0; VecIdx < TrackGroup->VectorTrackCount; ++VecIdx)
            {
                granny_vector_track& Track = TrackGroup->VectorTracks[VecIdx];

                bool FoundTrack = false;
                for (int ModelIdx = 0; ModelIdx < Info->ModelCount && !FoundTrack; ++ModelIdx)
                {
                    granny_model* Model = Info->Models[ModelIdx];
                    if (!Model)
                        continue;

                    for (int BoneIdx = 0; BoneIdx < Model->Skeleton->BoneCount; ++BoneIdx)
                    {
                        granny_uint32 Key =
                            GrannyVectorTrackKeyForBone(Model->Skeleton, BoneIdx, Track.Name);

                        if (Key == Track.TrackKey)
                        {
                            VectorTrackMatch vtm = { &Track, Model->Skeleton, BoneIdx };
                            TrackMatches.push_back(vtm);
                            FoundTrack = true;
                            break;
                        }
                    }
                }
            }
        }
    }
}

static void
RefreshTrackKeys(granny_file_info*         Info,
                 vector<VectorTrackMatch>& TrackMatches)
{
    for (size_t Idx = 0; Idx < TrackMatches.size(); ++Idx)
    {
        VectorTrackMatch& Match = TrackMatches[Idx];

        Match.Track->TrackKey =
            GrannyVectorTrackKeyForBone(Match.Skeleton, Match.BoneIndex, Match.Track->Name);
    }
}


struct TrackStrcmp
{
	bool operator()(granny_transform_track const& One, granny_transform_track& Two) const
	{
		return strcmp(One.Name, Two.Name) < 0;
	}
};

static bool
RemovePathsDriver(granny_file_info* Info)
{
    vector<VectorTrackMatch> TrackMatches;
    CreateTrackMatches(Info, TrackMatches);

#define DO_MOD(Name) Name = ModifyName((char*)(Name))
    for(granny_int32x ModelIdx = 0; ModelIdx < Info->ModelCount; ++ModelIdx)
    {
        granny_model* Model = Info->Models[ModelIdx];
        if (!Model)
            continue;

        DO_MOD(Model->Name);

        granny_skeleton* Skeleton = Model->Skeleton;
        if (!Skeleton)
            continue;

        DO_MOD(Skeleton->Name);

        for(granny_int32x BoneIdx = 0; BoneIdx < Skeleton->BoneCount; ++BoneIdx)
        {
            DO_MOD(Skeleton->Bones[BoneIdx].Name);
        }
    }

    for(granny_int32x AnimIdx = 0; AnimIdx < Info->AnimationCount; ++AnimIdx)
    {
        granny_animation* Animation = Info->Animations[AnimIdx];
        if (!Animation)
            continue;

        for(granny_int32x TGIdx = 0; TGIdx < Animation->TrackGroupCount; ++TGIdx)
        {
            granny_track_group* TrackGroup = Animation->TrackGroups[TGIdx];
            if (!TrackGroup)
                continue;

            DO_MOD(TrackGroup->Name);

            for(granny_int32x TTIdx = 0; TTIdx < TrackGroup->TransformTrackCount; ++TTIdx)
            {
                DO_MOD(TrackGroup->TransformTracks[TTIdx].Name);
            }

			// Have to resort these since we've altered the strings
			sort(TrackGroup->TransformTracks, TrackGroup->TransformTracks + TrackGroup->TransformTrackCount, TrackStrcmp());
			TrackGroup->Flags |= GrannyTrackGroupIsSorted;
        }
    }

    for(granny_int32x MeshIdx = 0; MeshIdx < Info->MeshCount; ++MeshIdx)
    {
        granny_mesh* Mesh = Info->Meshes[MeshIdx];
        if (!Mesh)
            continue;

        DO_MOD(Mesh->Name);

        for(granny_int32x BBIdx = 0; BBIdx < Mesh->BoneBindingCount; ++BBIdx)
        {
            DO_MOD(Mesh->BoneBindings[BBIdx].BoneName);
        }
    }

    for(granny_int32x MaterialIdx = 0; MaterialIdx < Info->MaterialCount; ++MaterialIdx)
    {
        granny_material* Mat = Info->Materials[MaterialIdx];
        if (!Mat)
            continue;

        DO_MOD(Mat->Name);
    }
#undef MOD

    RefreshTrackKeys(Info, TrackMatches);

    return true;
}

granny_file_info*
RemoveLongPaths(char const*          OriginalFilename,
                char const*          OutputFilename,
                granny_file_info*    Info,
                key_value_pair*      KeyValues,
                granny_int32x        NumKeyValues,
                granny_memory_arena* TempArena)
{
	// relic of source RemoveNamespace past...
    RemovePathsDriver(Info);
    return Info;
}

static const char* HelpString =
    (" RemoveLongPaths processes Maya files in which ambiguities in\n"
     " in the scene cause Maya to require long form paths when they\n"
     " are known to be unnecessary.\n"
     " -- YOU SHOULD WRITE US BEFORE USING THIS COMMAND --\n"
     "\n"
     "    preprocessor RemoveLongPaths robot.gr2 -output robot.gr2\n"
     "\n"
     " This will turn all strings of the form 'Parent|Bones|thing' into 'thing'\n");

static CommandRegistrar RegRemoveLongPaths(RemoveLongPaths,
                                           "RemoveLongPaths",
                                           "Removes Maya path notations (for certain unusual circumstances)",
                                           HelpString);
