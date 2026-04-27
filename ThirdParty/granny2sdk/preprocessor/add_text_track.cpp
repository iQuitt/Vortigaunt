// ========================================================================
// $File: //jeffr/granny_29/preprocessor/threshold_vector_track.cpp $
// $DateTime: 2011/12/06 12:28:06 $
// $Change: 35917 $
// $Revision: #1 $
//
// $Notice: $
// ========================================================================
#include "preprocessor.h"
#include "utilities.h"

#include <algorithm>
#include <set>
#include <stdio.h>
#include <string>
#include <vector>

using namespace std;

granny_file_info*
AddTextTrack(char const*          OriginalFilename,
                char const*          OutputFilename,
                granny_file_info*    Info,
                key_value_pair*      KeyValues,
                granny_int32x        NumKeyValues,
                granny_memory_arena* TempArena)
{
    if (Info->AnimationCount == 0)
    {
        ErrOut("File contains no animations\n");
        return 0;
    }
    if (Info->AnimationCount != 1)
    {
        ErrOut("As written, this utility assumes a single animation\n");
        return 0;
    }

    char const* TrackGroupName = FindFirstValueForKey(KeyValues, NumKeyValues, "trackgroup");
    if (TrackGroupName == 0)
    {
        ErrOut("must provide a name for the track group with -trackgroup\n");
        return 0;
    }

    char const* TrackName = FindFirstValueForKey(KeyValues, NumKeyValues, "track");
    if (TrackName == 0)
    {
        ErrOut("must provide the track name to add/update with -track\n");
        return 0;
    }

    char const* KeyName = FindFirstValueForKey(KeyValues, NumKeyValues, "text");
    if (KeyName == 0)
    {
        ErrOut("must provide the key name to add with -text\n");
        return 0;
    }

    char const* KeyTimestamp = FindFirstValueForKey(KeyValues, NumKeyValues, "time");
    if (KeyTimestamp == 0)
    {
        ErrOut("must provide the key time stamp to add with -time\n");
        return 0;
    }

    granny_animation* Anim = Info->Animations[0];
    pp_assert(Anim);

    granny_int32x TGIdx;
    if (!GrannyFindTrackGroupForModel(Anim, TrackGroupName, &TGIdx))
    {
        ErrOut("Unable to find trackgroup: '%s'\n", TrackGroupName);
        return 0;
    }

    granny_track_group* TrackGroup = Anim->TrackGroups[TGIdx];

    granny_int32x TrackIdx = 0;
    if( !GrannyFindTextTrackByName(TrackGroup,TrackName,&TrackIdx) )
    {
        // Make space for the new track
        int NewTextTrackCount = TrackGroup->TextTrackCount + 1;
        granny_text_track* NewTextTracks = PushArray(TempArena, NewTextTrackCount, granny_text_track);

        // Copy in the old tracks, if any
        memcpy(NewTextTracks, TrackGroup->TextTracks, sizeof(granny_text_track) * TrackGroup->TextTrackCount);

        // Put the new array back
        TrackGroup->TextTracks     = NewTextTracks;
        TrackGroup->TextTrackCount = NewTextTrackCount;

        // Fill in the new track
        granny_text_track& NewTrack = NewTextTracks[TrackGroup->TextTrackCount-1];

        NewTrack.Name = TrackName;
        NewTrack.EntryCount = 0;
        NewTrack.Entries = 0;

        TrackIdx = TrackGroup->TextTrackCount-1;
    }

    granny_text_track& Track = TrackGroup->TextTracks[TrackIdx];
    granny_text_track_entry * OldEntries = Track.Entries;

    Track.Entries = PushArray(TempArena, Track.EntryCount+1, granny_text_track_entry);
    Track.EntryCount++;

    //Copy old entries across. 
    memcpy(Track.Entries,OldEntries,(Track.EntryCount-1)*sizeof(granny_text_track_entry));

    //Add the new entry
    Track.Entries[Track.EntryCount-1].TimeStamp = (granny_real32)atof(KeyTimestamp);
    Track.Entries[Track.EntryCount-1].Text      = KeyName;

    return Info;
}

static const char* HelpString =
    (" AddTextTrack adds elements to a granny_text_track and adds it\n"
     " to the named granny_track_group.\n"
     "\n"
     "    preprocessor AddTextTrack run_cycle.gr2         \\\n"
     "        -trackgroup Bip001                             \\\n"
     "        -track footsteps                         \\\n"
     "        -text down                          \\\n"
     "        -time 0.6                          \\\n"
     "        -output run_cycle_with_footstep.gr2 \\\n" 
     "\n");

static CommandRegistrar RegAddTextTrack(AddTextTrack,
                                           "AddTextTrack",
                                           "Add text_tracks from the command line",
                                           HelpString);
