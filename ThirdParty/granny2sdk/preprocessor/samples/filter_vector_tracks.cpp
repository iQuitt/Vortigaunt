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
#include <vector>

using namespace std;

// Command details:
//  -output key must be present
//
//  At least one -filter key must be specified.
//
//  -noconstant [any] passed on the command-line will filter out constant or identity
//  -tracks, even if they match the filter

//  -removeemptytracks will filter out track_groups that don't have any vector_tracks that
//  -matched the filter.  (Only if they don't have transform_tracks, of course.)


bool
MatchesFilter(char const* TrackName, char const* Filter)
{
    pp_assert(TrackName);
    pp_assert(Filter);

    // Very (!) simple matching rule, we just check to see if the track name begins with
    // the filter string.  This can be replaced with regexp matching, dos glob style
    // matching, etc.
    size_t const len = strlen(Filter);

    return _strnicmp(TrackName, Filter, len) == 0;
}


static void
FilterAnimation(granny_animation*    Animation,
                key_value_pair*      KeyValues,
                granny_int32x        NumKeyValues,
                granny_memory_arena* TempArena)
{
    pp_assert(Animation);

    vector<granny_track_group*> TrackGroups;
    {for (int TGIdx = 0; TGIdx < Animation->TrackGroupCount; ++TGIdx)
    {
        granny_track_group* TrackGroup = Animation->TrackGroups[TGIdx];
        if (!TrackGroup)
            continue;

        if (TrackGroup->VectorTrackCount == 0)
        {
            TrackGroups.push_back(TrackGroup);
            continue;
        }

        vector<granny_vector_track> VecTracks;
        {for (int VTIdx = 0; VTIdx < TrackGroup->VectorTrackCount; ++VTIdx)
        {
            if (GrannyCurveIsConstantOrIdentity(&TrackGroup->VectorTracks[VTIdx].ValueCurve) &&
                FindFirstValueForKey(KeyValues, NumKeyValues, "noconstant"))
            {
                continue;
            }

            {for (int Idx = 0; Idx < NumKeyValues; ++Idx)
            {
                if (_stricmp(KeyValues[Idx].Key, "filter") != 0)
                    continue;

                if (MatchesFilter(TrackGroup->VectorTracks[VTIdx].Name,
                                  KeyValues[Idx].Value))
                {
                    VecTracks.push_back(TrackGroup->VectorTracks[VTIdx]);
                    break;
                }
            }}
        }}

        if (VecTracks.size() == 0 && TrackGroup->TransformTrackCount == 0 &&
            FindFirstValueForKey(KeyValues, NumKeyValues, "removeemptytracks"))
        {
            continue;
        }

        TrackGroup->VectorTrackCount = (granny_int32)VecTracks.size();
        memcpy(TrackGroup->VectorTracks, &VecTracks[0], sizeof(granny_vector_track)*VecTracks.size());
        TrackGroups.push_back(TrackGroup);
    }}

    Animation->TrackGroupCount = (granny_int32)TrackGroups.size();
    memcpy(Animation->TrackGroups, &TrackGroups[0], sizeof(granny_track_group*)*TrackGroups.size());
}


granny_file_info*
FilterVectorTracks(char const*          OriginalFilename,
                   char const*          OutputFilename,
                   granny_file_info*    Info,
                   key_value_pair*      KeyValues,
                   granny_int32x        NumKeyValues,
                   granny_memory_arena* TempArena)
{
    if (FindFirstValueForKey(KeyValues, NumKeyValues, "filter") == 0)
    {
        ErrOut("must specify at least one -filter <string>\n");
        return false;
    }

    {for (int AnimIdx = 0; AnimIdx < Info->AnimationCount; ++AnimIdx)
    {
        granny_animation* Animation = Info->Animations[AnimIdx];
        if (!Animation)
            continue;

        FilterAnimation(Animation, KeyValues, NumKeyValues, TempArena);
    }}

    return Info;
}


static CommandRegistrar RegFilterVectorTracks(FilterVectorTracks, "FilterVectorTracks",
                                              "Removes tracks that don't string(s) specified with -filter.");

