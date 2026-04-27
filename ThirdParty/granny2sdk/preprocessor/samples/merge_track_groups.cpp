// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"
#include <vector>
#include <set>

using namespace std;

bool
MergeTrackGroups(input_file*     InputFiles,
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

    granny_file_info* BaseInfo    = ExtractFileInfo(InputFiles[0]);
    granny_file_info* ReplaceInfo = ExtractFileInfo(InputFiles[1]);
    if (!BaseInfo || !ReplaceInfo)
    {
        ErrOut("unable to get granny_file_info from inputs\n");
        return false;
    }

    if (BaseInfo->AnimationCount != 1 || ReplaceInfo->AnimationCount != 1)
    {
        ErrOut("each input file must have one and only one animation\n");
        return false;
    }

    {for (int TrackIdx = 0; TrackIdx < BaseInfo->AnimationCount; ++TrackIdx)
    {
        granny_track_group* TrackGroup = BaseInfo->Animations[0]->TrackGroups[TrackIdx];
        if (!TrackGroup)
            continue;

        granny_int32x OtherIdx;
        if (!GrannyFindTrackGroupForModel(ReplaceInfo->Animations[0], TrackGroup->Name, &OtherIdx))
        {
            // No matching track group
            continue;
        }

        granny_track_group* OtherGroup = ReplaceInfo->Animations[0]->TrackGroups[OtherIdx];
        {for (int TTIdx = 0; TTIdx < OtherGroup->TransformTrackCount; ++TTIdx)
        {
            granny_transform_track const& NewTrack = OtherGroup->TransformTracks[TTIdx];

            granny_int32x BaseTTIdx;
            if (!GrannyFindTrackByName(TrackGroup, NewTrack.Name, &BaseTTIdx))
                continue;

            granny_transform_track& BaseTrack = TrackGroup->TransformTracks[BaseTTIdx];

            // Just replace the flags and the curves...
            BaseTrack.Flags            = NewTrack.Flags;
            BaseTrack.OrientationCurve = NewTrack.OrientationCurve;
            BaseTrack.PositionCurve    = NewTrack.PositionCurve;
            BaseTrack.ScaleShearCurve  = NewTrack.ScaleShearCurve;
        }}
    }}

    // Write out the result
    return SimpleWriteInfo(KeyValues, NumKeyValues, BaseInfo, TempArena);
}


static CommandRegistrar RegMergeTG(MergeTrackGroups, "MergeTrackGroups",
                                   "Given two animations, replace transform tracks in the first with matching tracks from the second");

