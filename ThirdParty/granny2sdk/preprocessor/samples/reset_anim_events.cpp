// ========================================================================
// $File: //jeffr/granny_29/preprocessor/reset_anim_events.cpp $
// $DateTime: 2012/09/05 10:21:32 $
// $Change: 39146 $
// $Revision: #2 $
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"
#include "gstate_anim_source.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>


bool
ResetAnimationEvents(input_file& InputFile,
                  key_value_pair* KeyValues,
                  granny_int32x NumKeyValues,
                  granny_memory_arena* TempArena)
{
    gstate_character_info* CharacterInfo = GStateGetCharacterInfo(InputFile.GrannyFile);
    if (!CharacterInfo)
    {
        ErrOut("Unable to get gstate_character_info from file.");
        return false;
    }

    // Run the special case for the anim_source nodes...
    WalkAndResetEventEdges(CharacterInfo->StateMachine.Type,
                           CharacterInfo->StateMachine.Object);

    return SimpleWriteCharacterInfo(KeyValues, NumKeyValues,
                                    CharacterInfo, TempArena);
}

static const char* HelpString =
    (" ResetAnimationEvents resets all animation nodes events to none\n");

static CommandRegistrar RegResetAnimationEvents(eSingleFile_NoMultiRun,
                                        ResetAnimationEvents,
                                        "ResetAnimationEvents",
                                        "resets all animation nodes events to none.",
                                        HelpString);

