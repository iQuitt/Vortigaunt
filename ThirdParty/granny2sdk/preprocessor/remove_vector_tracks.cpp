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

granny_file_info*
RemoveVectorTracks(char const*          OriginalFilename,
                   char const*          OutputFilename,
                   granny_file_info*    Info,
                   key_value_pair*      KeyValues,
                   granny_int32x        NumKeyValues,
                   granny_memory_arena* TempArena)
{
    // Remove all vector tracks from each track group
    {for(granny_int32x TGIdx = 0; TGIdx < Info->TrackGroupCount; ++TGIdx)
    {
        granny_track_group* TrackGroup = Info->TrackGroups[TGIdx];
        TrackGroup->VectorTrackCount = 0;
        TrackGroup->VectorTracks = 0;
    }}

    return Info;
}

static const char* HelpString =
    (" RemoveVectorTracks strips any and all granny_vector_tracks that are\n"
     " contained in the file.  It's like a more restricted version of\n"
     " 'KillUncommonData'\n"
     "\n"
     "    preprocessor RemoveVectorTracks file.gr2 -output file_stripped.gr2\n");

static CommandRegistrar RegRemoveVectorTracks(RemoveVectorTracks,
                                              "RemoveVectorTracks",
                                              "Removes any vector tracks contained in the file",
                                              HelpString);

