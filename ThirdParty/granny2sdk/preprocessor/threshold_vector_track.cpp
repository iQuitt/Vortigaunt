// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
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

struct TTTimeCmp : public binary_function<granny_text_track_entry const&, granny_text_track_entry const&, bool>
{
    bool operator()(granny_text_track_entry const& One, granny_text_track_entry const& Two) const
    {
        return One.TimeStamp < Two.TimeStamp;
    }
};


void InsertEvent(vector<granny_text_track_entry>& Entries,
                 char const* String, granny_real32 Time)
{
    granny_text_track_entry Entry = { Time, String };
    Entries.push_back(Entry);
}

void ThresholdVecTrack(granny_vector_track& Track,
                       granny_real32 Duration,
                       granny_real32 TimeStep,
                       granny_real32 Threshold,
                       vector<granny_text_track_entry>& Entries,
                       granny_memory_arena* Arena)
{
    // First, let's get some things out of the way.  If the Track is constant or identity,
    // or not dimension 1, we can't deal with it
    if (GrannyCurveIsConstantOrIdentity(&Track.ValueCurve) || Track.Dimension != 1)
        return;

    char const* HiString = 0;
    char const* LoString = 0;
    {
        const int BufferSize = 512;
        char TempBuffer[BufferSize];

        sprintf(TempBuffer, "%s_hi", Track.Name);
        HiString = PushString(Arena, TempBuffer);

        sprintf(TempBuffer, "%s_lo", Track.Name);
        LoString = PushString(Arena, TempBuffer);
    }

    // Handle the first sample specially, we need to establish the first value
    // (Do we want to encode a special entry for the initial value?)
    granny_real32 LastValue;
    GrannyEvaluateCurveAtT(Track.Dimension, false, false, &Track.ValueCurve, false,
                           Duration, Duration,
                           &LastValue, NULL);

    granny_real32 CurrT = 0.0f;
    while (CurrT <= Duration)
    {
        granny_real32 NewValue;
        GrannyEvaluateCurveAtT(Track.Dimension, false, false, &Track.ValueCurve, false,
                               Duration, CurrT,
                               &NewValue, NULL);

        bool LastAbove = LastValue >= Threshold;
        bool NewAbove  = NewValue >= Threshold;

        // Is this an edge?
        if (LastAbove != NewAbove)
        {
            // Hi or Lo?
            if (NewAbove)
                InsertEvent(Entries, HiString, CurrT);
            else
                InsertEvent(Entries, LoString, CurrT);
        }

        LastValue = NewValue;
        CurrT += TimeStep;
    }
}

granny_file_info*
ThresholdVectorTrack(char const*          OriginalFilename,
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

    float Threshold = 0.5f;
    {
        char const* ThresholdValue = FindFirstValueForKey(KeyValues, NumKeyValues, "threshold");
        if (ThresholdValue)
            Threshold = (float)atof(ThresholdValue);
    }

    char const* TrackName = FindFirstValueForKey(KeyValues, NumKeyValues, "trackname");
    if (TrackName == 0)
    {
        ErrOut("must provide a name for the new track with -trackname\n");
        return 0;
    }

    set<char const*, CIStrcmp> Tracknames;
    {
        {for(int Key = 0; Key < NumKeyValues; ++Key)
        {
            if (_stricmp(KeyValues[Key].Key, "track") == 0)
                Tracknames.insert(KeyValues[Key].Value);
        }}
    }

    if (Tracknames.empty())
    {
        ErrOut("no vectortracks to threshold\n");
        return 0;
    }

    {for (int AnimIdx = 0; AnimIdx < Info->AnimationCount; ++AnimIdx)
    {
        granny_animation* Anim = Info->Animations[AnimIdx];
        pp_assert(Anim);

        {for(int TGIdx = 0; TGIdx < Anim->TrackGroupCount; ++TGIdx)
        {
            granny_track_group* TrackGroup = Anim->TrackGroups[TGIdx];
            pp_assert(TrackGroup);

            if (TrackGroup->VectorTrackCount == 0)
                continue;

            int NumNewVecTracks = 0;
            granny_vector_track* NewVecTracks =
                PushArray(TempArena, TrackGroup->VectorTrackCount, granny_vector_track);

            int NumNewTextTracks = TrackGroup->TextTrackCount + 1;
            granny_text_track* NewTextTracks =
                PushArray(TempArena, NumNewTextTracks, granny_text_track);
            memcpy(NewTextTracks, TrackGroup->TextTracks,
                   sizeof(granny_text_track) * TrackGroup->TextTrackCount);

            vector<granny_text_track_entry> TextEntries;
            {for(int VTIdx = 0; VTIdx < TrackGroup->VectorTrackCount; ++VTIdx)
            {
                if (Tracknames.find(TrackGroup->VectorTracks[VTIdx].Name) == Tracknames.end())
                {
                    NewVecTracks[NumNewVecTracks++] = TrackGroup->VectorTracks[VTIdx];
                    pp_assert(NumNewVecTracks <= TrackGroup->VectorTrackCount);
                }
                else
                {
                    // This needs to be thresholded and entered into the texttrack.  Don't
                    // put it back into the vector tracks...
                    ThresholdVecTrack(TrackGroup->VectorTracks[VTIdx],
                                      Anim->Duration, Anim->TimeStep, Threshold,
                                      TextEntries, TempArena);
                }
            }}

            // Sort the entries by time
            sort(TextEntries.begin(), TextEntries.end(), TTTimeCmp());

            // At the end, create the new vector track...
            granny_text_track& NewTrack = NewTextTracks[TrackGroup->TextTrackCount];
            NewTrack.Name = TrackName;
            NewTrack.EntryCount = (granny_int32)TextEntries.size();
            NewTrack.Entries = (granny_text_track_entry*)
                GrannyMemoryArenaPushBinary(TempArena, NewTrack.EntryCount * sizeof(granny_text_track_entry),
                                            &TextEntries[0]);

            TrackGroup->VectorTrackCount = NumNewVecTracks;
            TrackGroup->VectorTracks = NewVecTracks;

            TrackGroup->TextTrackCount = NumNewTextTracks;
            TrackGroup->TextTracks = NewTextTracks;
        }}
    }}

    return Info;
}

static const char* HelpString =
    (" ThresholdVectorTrack converts a granny_vector_track into a \n"

     " granny_text_track.  Maya provides no built-in facility for creating\n"
     " granny_text_track entities, although Granny has adopted the\n"
     " convention that keyed Enum Attributes are text keys.  Max has the\n"
     " Note Track, which is converted into a granny_text_track on export.\n"
     " ThresholdVectorTrack allows you to threshold a custom attribute\n"
     " animation into a discrete set of events.  We'll use the example of a\n"
     " footdown track, which is setup in Maya such that 0.0 means\n"
     " \"foot is up\" and 1.0 means \"foot is locked to the ground\".\n"
     "\n"
     "    preprocessor ThresholdVectorTrack run_cycle.gr2    \\\n"
     "        -output run_cycle_event.gr2                    \\\n"
     "        -trackname FootTrack                           \\\n"
     "        -track l_foot_down                             \\\n"
     "        -track -r_foot_down                            \\\n"
     "        -threshold 0.9\n"
     "\n"

     " This will convert the granny_vector_tracks named \"l_foot_down\" and\n"
     " \"r_foot_down\" into a single granny_text_track named \"FootTrack\".\n"
     " It will contain events named \"l_foot_down_lo\", \"l_foot_down_hi\",\n"
     " (and the equivalent r_foot elements) and the trigger times for each.\n"
     " The \"_lo\" and \"_hi\" should be interpreted as signal\n"
     " descriptions.  I.e, if l_foot_down_hi triggers, the foot has just\n"
     " locked to the ground in this example.  The threshold parameter\n"
     " indicates at what level in the underlying attribute the transition\n"
     " should trigger.\n");

static CommandRegistrar RegThresholdVectorTrack(ThresholdVectorTrack,
                                                "ThresholdVectorTrack",
                                                "Make text_tracks from vector_tracks",
                                                HelpString);

