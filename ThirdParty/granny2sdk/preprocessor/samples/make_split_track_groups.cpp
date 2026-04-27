// ========================================================================
// $File: //jeffr/granny_29/preprocessor/samples/dump_mesh_texture.cpp $
// $DateTime: 2013/03/15 12:40:36 $
// $Change: 41836 $
// $Revision: #4 $
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"
#include <stdio.h>
#include <vector>

// Utility for paging animation files...

using namespace std;

granny_file_info*
MakeSplitTrackGroups(char const*          OriginalFilename,
                     char const*          OutputFilename,
                     granny_file_info*    Info,
                     key_value_pair*      KeyValues,
                     granny_int32x        NumKeyValues,
                     granny_memory_arena* TempArena)
{
    char const* OutputData = FindFirstValueForKey(KeyValues, NumKeyValues, "data");
    if (OutputData == 0)
    {
        fprintf(stderr, "must specify an data output \"-data <string>\"\n");
        return false;
    }

    if (Info->AnimationCount == 0)
    {
        fprintf(stderr, "MakeSplitTrackGroups: no animations in this file\n");
        return false;
    }


    // Write out the data file first.  This is very easy, it's *just* the animation array from the
    // original file.
    {
        granny_file_info DataInfo;
        memset(&DataInfo, 0, sizeof(DataInfo));

        DataInfo.AnimationCount = Info->AnimationCount;
        DataInfo.Animations = Info->Animations;

        granny_file_builder *Builder =
            GrannyBeginFile(GrannyStandardSectionCount, GrannyCurrentGRNStandardTag,
                            GrannyGRNFileMV_ThisPlatform,
                            GrannyGetTemporaryDirectory(),
                            "Prefix");
        
        granny_file_data_tree_writer *Writer = CreateTreeWriter(OutputData, &DataInfo);
        pp_assert(Writer);

        GrannyWriteDataTreeToFileBuilder(Writer, Builder);
        GrannyEndFileDataTreeWriting(Writer);
        
        bool success = GrannyEndFile(Builder, OutputData);
        if (!success)
        {
            fprintf(stderr, "Failed to write granny_file_info to %s\n", OutputData);
            return false;
        }
    }
    
    // Now write out the modified animation, which is just the same file, with all curves stripped out of the track_groups.
    for(granny_int32x i = 0; i < Info->AnimationCount; i++)
    {
        granny_animation* Anim = Info->Animations[i];
        if (!Anim)
            continue;

        for (int TGIdx = 0; TGIdx < Anim->TrackGroupCount; ++TGIdx)
        {
            granny_track_group* TrackGroup = Anim->TrackGroups[TGIdx];
            // This would be shocking, but hey.
            if (!TrackGroup)
                continue;

            // Zero out the transform curves...
            for (int TTIdx = 0; TTIdx < TrackGroup->TransformTrackCount; ++TTIdx)
            {
                TrackGroup->TransformTracks[TTIdx].OrientationCurve.CurveData.Object = 0;
                TrackGroup->TransformTracks[TTIdx].OrientationCurve.CurveData.Object = 0;
                TrackGroup->TransformTracks[TTIdx].PositionCurve.CurveData.Object    = 0;
                TrackGroup->TransformTracks[TTIdx].PositionCurve.CurveData.Object    = 0;
                TrackGroup->TransformTracks[TTIdx].ScaleShearCurve.CurveData.Object  = 0;
                TrackGroup->TransformTracks[TTIdx].ScaleShearCurve.CurveData.Object  = 0;
            }

            // Text tracks and vectors don't get sampled into to the animation_binding cache, so we
            // can just nuke the array...
            TrackGroup->VectorTrackCount = 0;
            TrackGroup->VectorTracks     = 0;

            TrackGroup->TextTrackCount = 0;
            TrackGroup->TextTracks     = 0;

            // NEED TO DOCUMENT THIS HEAVILY ON THE TP SIDE.  THIS MARKS THE TRACK_GROUP AS NON-RESIDENT
            TrackGroup->Flags |= (1 << 15);
        }
    }

    return Info;
}

static CommandRegistrar RegMakeSplitTrackGroups(MakeSplitTrackGroups,
                                                "MakeSplitTrackGroups",
                                                "Dumps a .GR2 file with meshes as OBJ files, textures as BMP.");

