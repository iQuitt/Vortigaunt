// ========================================================================
// $Notice: $
// ========================================================================
#include "gstate_bind_by_slot.h"

#include "../gstate_character_info.h"
#include "../gstate_telemetry.h"

#define GSTATE_INTERNAL_HEADER 1
#include "../gstate_character_internal.h"

#include "../gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

bool
GStateBindCharacterSlots(EXPIN gstate_character_info* Info,
                         EXPIN gstate_slot_callback*  SlotCallback,
                         EXPIN void*                  UserData)
{
    GSTATE_AUTO_ZONE_FN();

    GStateCheckPtrNotNULL(Info, return false);
    GStateCheckPtrNotNULL(SlotCallback, return false);
    GStateAssert(Info->AnimationSetCount > 0);
    GStateAssert(Info->AnimationSets[0]->AnimationSpecCount == Info->AnimationSlotCount);
    
    for (int SlotIdx = 0; SlotIdx < Info->AnimationSlotCount; ++SlotIdx)
    {
        animation_slot*  Slot    = Info->AnimationSlots[SlotIdx];
        animation_spec*  Spec    = &Info->AnimationSets[0]->AnimationSpecs[SlotIdx];
        source_file_ref* FileRef = Spec->SourceReference;

        granny_file_info* FileInfo =
            (*SlotCallback)(Info, Slot->Name, FileRef->SourceFilename, UserData);

        if (FileRef->SourceInfo == 0)
        {
            // New reference
            if (FileRef->ExpectedAnimCount != 0 && FileRef->AnimCRC != 0)
            {
                if (ValidateReference(FileRef, FileInfo))
                {
                    // All fine.
                    FileRef->SourceInfo = FileInfo;
                }
                else
                {
                    GStateError("Invalid file_info for slot: %s (slot0 fname: %s)\n",
                                Slot->Name, FileRef->SourceFilename);
                    return false;
                }
            }
            else
            {
                // Just take the info, and update the reference, nothing we can do right
                // here.
                FileRef->ExpectedAnimCount = FileInfo->AnimationCount;
                FileRef->AnimCRC           = ComputeAnimCRC(FileInfo);
                FileRef->SourceInfo        = FileInfo;
            }
        }
        else if (FileRef->SourceInfo != FileInfo)
        {
            GStateError("Mismatch found for file reference loaded by slot! '%s'", Slot->Name);

            // This is bad, we DON'T want this to be silent, since it would imply overwriting
            // another file reference that might cause problems down the road.
            return false;
        }
    }

    return true;
}

