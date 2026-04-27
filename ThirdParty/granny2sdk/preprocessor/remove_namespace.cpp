// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "preprocessor.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <vector>
#include <algorithm>

using namespace std;

static char const*
ModifyName(char* InputName,
           bool RemovingAll,
           char const* TargetNamespace)
{
    if (RemovingAll)
    {
        char* LastColon = strrchr(InputName, ':');

        if (!LastColon)
            return InputName;

        // This isn't super awesome, but if the name /ends/ in a colon, I don't want to create an empty string...
        if (LastColon+1 != '\0')
        {
            InputName = LastColon + 1;
        }
        else
        {
            ErrOut("\"%s\" ends in a colon, leaving it alone\n", InputName);
        }
    }
    else if (TargetNamespace)
    {
        char* Position = InputName;
        granny_int32x const TargetLen = (granny_int32x)strlen(TargetNamespace);
        granny_int32x InputLen  = (granny_int32x)strlen(InputName) + 1;

        while (*Position)
        {
            if (_strnicmp(Position, TargetNamespace, TargetLen) == 0)
            {
                memmove(Position, Position + TargetLen, InputLen-TargetLen);
                InputLen -= TargetLen;
            }
            else
            {
                ++Position;
                --InputLen;
            }
        }
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

    {for (int AnimIdx = 0; AnimIdx < Info->AnimationCount; ++AnimIdx)
    {
        granny_animation* Animation = Info->Animations[AnimIdx];
        if (!Animation)
            continue;

        {for (int TrackGroupIdx = 0; TrackGroupIdx < Animation->TrackGroupCount; ++TrackGroupIdx)
        {
            granny_track_group* TrackGroup = Animation->TrackGroups[TrackGroupIdx];
            if (!TrackGroup)
                continue;

            {for (int VecIdx = 0; VecIdx < TrackGroup->VectorTrackCount; ++VecIdx)
            {
                granny_vector_track& Track = TrackGroup->VectorTracks[VecIdx];

                bool FoundTrack = false;
                {for (int ModelIdx = 0; ModelIdx < Info->ModelCount && !FoundTrack; ++ModelIdx)
                {
                    granny_model* Model = Info->Models[ModelIdx];
                    if (!Model)
                        continue;

                    {for (int BoneIdx = 0; BoneIdx < Model->Skeleton->BoneCount; ++BoneIdx)
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
                    }}
                }}
            }}
        }}
    }}
}

static void
RefreshTrackKeys(granny_file_info*         Info,
                 vector<VectorTrackMatch>& TrackMatches)
{
    {for (size_t Idx = 0; Idx < TrackMatches.size(); ++Idx)
    {
        VectorTrackMatch& Match = TrackMatches[Idx];

        Match.Track->TrackKey =
            GrannyVectorTrackKeyForBone(Match.Skeleton, Match.BoneIndex, Match.Track->Name);
    }}
}


static bool
RemoveNamespaceDriver(granny_file_info* Info,
                      bool RemovingAll,
                      char const* TargetNamespace)
{
    vector<VectorTrackMatch> TrackMatches;
    CreateTrackMatches(Info, TrackMatches);

#define DO_MOD(Name) Name = ModifyName((char*)(Name), RemovingAll, TargetNamespace)
    {for(granny_int32x ModelIdx = 0; ModelIdx < Info->ModelCount; ++ModelIdx)
    {
        granny_model* Model = Info->Models[ModelIdx];
        if (!Model)
            continue;

        DO_MOD(Model->Name);

        granny_skeleton* Skeleton = Model->Skeleton;
        if (!Skeleton)
            continue;

        DO_MOD(Skeleton->Name);

        {for(granny_int32x BoneIdx = 0; BoneIdx < Skeleton->BoneCount; ++BoneIdx)
        {
            DO_MOD(Skeleton->Bones[BoneIdx].Name);
        }}
    }}

    {for(granny_int32x AnimIdx = 0; AnimIdx < Info->AnimationCount; ++AnimIdx)
    {
        granny_animation* Animation = Info->Animations[AnimIdx];
        if (!Animation)
            continue;

        {for(granny_int32x TGIdx = 0; TGIdx < Animation->TrackGroupCount; ++TGIdx)
        {
            granny_track_group* TrackGroup = Animation->TrackGroups[TGIdx];
            if (!TrackGroup)
                continue;

            DO_MOD(TrackGroup->Name);

            {for(granny_int32x TTIdx = 0; TTIdx < TrackGroup->TransformTrackCount; ++TTIdx)
            {
                DO_MOD(TrackGroup->TransformTracks[TTIdx].Name);
            }}
        }}
    }}

    {for(granny_int32x MeshIdx = 0; MeshIdx < Info->MeshCount; ++MeshIdx)
    {
        granny_mesh* Mesh = Info->Meshes[MeshIdx];
        if (!Mesh)
            continue;

        DO_MOD(Mesh->Name);

        {for(granny_int32x BBIdx = 0; BBIdx < Mesh->BoneBindingCount; ++BBIdx)
        {
            DO_MOD(Mesh->BoneBindings[BBIdx].BoneName);
        }}
    }}
#undef MOD

    GrannyResortAllFileTrackGroups(Info);
    RefreshTrackKeys(Info, TrackMatches);

    return true;
}

granny_file_info*
RemoveAllNamespaces(char const*          OriginalFilename,
                    char const*          OutputFilename,
                    granny_file_info*    Info,
                    key_value_pair*      KeyValues,
                    granny_int32x        NumKeyValues,
                    granny_memory_arena* TempArena)

{
    RemoveNamespaceDriver(Info, true, 0);

    return Info;
}

granny_file_info*
RemoveNamespace(char const*          OriginalFilename,
                char const*          OutputFilename,
                granny_file_info*    Info,
                key_value_pair*      KeyValues,
                granny_int32x        NumKeyValues,
                granny_memory_arena* TempArena)
{
    char const* TargetName = FindFirstValueForKey(KeyValues, NumKeyValues, "namespace");
    if (TargetName == 0)
    {
        ErrOut("must specify a NameSpace to remove with \"-namespace <value>\"\n");
        return false;
    }

    RemoveNamespaceDriver(Info, false, TargetName);

    return Info;
}

static const char* HelpString =
    (" RemoveNamespace and RemoveAllNamespaces are utilities for processing\n"
     " Maya files in which scene references have introduced namespace\n"
     " prefixes to object names.\n"
     "\n"
     " If you know the specific namespace you want to remove from the file,\n"
     " say, 'Robot:', then you can use RemoveNamespace (note the quoting):\n"
     "\n"
     "    preprocessor RemoveNamespace robot.gr2 -output robot.gr2 -namespace \"Robot:\"\n"
     "\n"
     " If you'd prefer to simply remove *all* namespaces in the file,\n"
     " regardless of string, then use:\n"
     "\n"
     "    preprocessor RemoveAllNamespaces robot.gr2 -output robot.gr2\n"
     "\n"
     " This will turn all strings of the form '*:thing' into 'thing'\n");

static CommandRegistrar RegRemoveNamespace(RemoveNamespace,
                                           "RemoveNamespace",
                                           "Removes a Maya namespace prefix string from animation/model/skeleton entries",
                                           HelpString);

static CommandRegistrar RegRemoveAllNamespaces(RemoveAllNamespaces,
                                               "RemoveAllNamespaces",
                                               "Removes all Maya namespaces from animation/model/skeleton entries",
                                               HelpString);
