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
RemoveTextTracks(char const*          OriginalFilename,
                 char const*          OutputFilename,
                 granny_file_info*    Info,
                 key_value_pair*      KeyValues,
                 granny_int32x        NumKeyValues,
                 granny_memory_arena* TempArena)
{
    // Remove all text tracks from each track group
    {for(granny_int32x TGIdx = 0; TGIdx < Info->TrackGroupCount; ++TGIdx)
    {
        granny_track_group* TrackGroup = Info->TrackGroups[TGIdx];
        TrackGroup->TextTrackCount = 0;
        TrackGroup->TextTracks = 0;
    }}

    return Info;
}

static const char* HelpString =
    (" RemoveTextTracks strips any and all granny_text_tracks that are\n"
     " contained in the file.  It's like a more restricted version of\n"
     " 'KillUncommonData'\n"
     "\n"
     "    preprocessor RemoveTextTracks file.gr2 -output file_stripped.gr2\n");

static CommandRegistrar RegRemoveTextTracks(RemoveTextTracks,
                                            "RemoveTextTracks",
                                            "Removes any text tracks contained in the file",
                                            HelpString);
