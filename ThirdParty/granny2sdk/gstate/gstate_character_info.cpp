// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "gstate_character_info.h"
#include "gstate_token_context.h"

#include "gstate_anim_source.h"
#include "gstate_position_ik.h"
#include "gstate_blend_graph.h"
#include "gstate_container.h"
#include "gstate_state_machine.h"
#include "gstate_telemetry.h"

#define GSTATE_INTERNAL_HEADER 1
#include "gstate_character_internal.h"

#include <string.h>

#include "gstate_cpp_settings.h"
USING_GSTATE_NAMESPACE;

// =============================================================================
// Type arrays
// =============================================================================

static granny_data_type_definition SourceFileRefType[] =
{
    { GrannyStringMember, "SourceFilename" },
    { GrannyInt32Member,  "ExpectedAnimCount" },  // 0 == old file
    { GrannyUInt32Member, "AnimCRC" },            // 0 == no crc computed

    { GrannyEmptyReferenceMember, "SourceInfo" },

    { GrannyEndMember }
};

static granny_data_type_definition AnimationSpecType[] =
{
    { GrannyReferenceMember, "SourceReference", SourceFileRefType },
    { GrannyInt32Member,     "AnimationIndex" },
    { GrannyStringMember,    "ExpectedName" },

    { GrannyEndMember }
};

granny_data_type_definition GSTATE AnimationSlotType[] =
{
    { GrannyStringMember,    "Name" },
    { GrannyInt32Member,     "SlotIndex" },

    { GrannyEndMember }
};

granny_data_type_definition GSTATE AnimationSetType[] =
{
    { GrannyStringMember,            "Name" },
    { GrannyArrayOfReferencesMember, "SourceFileReferences", SourceFileRefType },
    { GrannyReferenceToArrayMember,  "AnimationSpecs",       AnimationSpecType },
      
    { GrannyEndMember }
};

granny_data_type_definition GStateCharacterInfoType[] =
{
    // Note that the TouchVersion command looks for this member, make sure to update that
    // preprocessor command if this moves.
    { GrannyInt32Member,             "NumUniqueTokenized" },

    { GrannyVariantReferenceMember,  "StateMachine" },
    { GrannyArrayOfReferencesMember, "AnimationSlots",         AnimationSlotType },
    { GrannyArrayOfReferencesMember, "AnimationSets",          AnimationSetType },

    { GrannyStringMember, "ModelNameHint" },
    { GrannyInt32Member,  "ModelIndexHint" },

    { GrannyStringMember, "RetargetSourceModelNameHint" },
    { GrannyInt32Member,  "RetargetSourceModelIndexHint" },

    { GrannyVariantReferenceMember,  "EditorData" },

    { GrannyVariantReferenceMember,  "ExtendedData" },

    { GrannyEndMember }
};

// =============================================================================
// Detects and enforces invariants for tokens that have been upgraded.  Right now,
// we're just looking for anim_source tokens.  To keep this simple (i.e, not tracking
// edges directly, just nodes so we don't have to worry about graph cycles), read out
// the container tokens directly, and use those to recurse.
static void
WalkAndConvertInContainer(granny_data_type_definition* Type,
                          void*                        Object)
{
    GSTATE_AUTO_ZONE_FN();

    if (Type == 0 || Object == 0)
        return;

    
    granny_variant ContainerToken;
    if (!GrannyFindMatchingMember(Type, Object, "containerToken", &ContainerToken))
        return;


    granny_variant ChildTokenArray;
    if (!GrannyFindMatchingMember(ContainerToken.Type->ReferenceType, *(void**)ContainerToken.Object, "ChildTokens", &ChildTokenArray))
        return;

    GStateAssert(ChildTokenArray.Type->Type == GrannyReferenceToArrayMember);

    granny_int32&   Count = *(granny_int32*)(ChildTokenArray.Object);
    granny_variant* Array = *(granny_variant**)((granny_uint8*)ChildTokenArray.Object + sizeof(granny_int32));

    for (int Idx = 0; Idx < Count; ++Idx)
    {
        granny_variant& Token = Array[Idx];

        // Correct
        if (strcmp(Token.Type->Name, "ContextUID_anim_source") == 0)
        {
            anim_source::FixTokenEdges((anim_source_token*)Token.Object);
        }
        else if (strcmp(Token.Type->Name, "ContextUID_position_ik") == 0)
        {
            position_ik::UpgradePositionIKNode((position_ik_token*)Token.Object);
        }
        else if (strcmp(Token.Type->Name, "ContextUID_state_machine") == 0 ||
                 strcmp(Token.Type->Name, "ContextUID_blend_graph") == 0)
        {
            // Recurse...
            WalkAndConvertInContainer(Token.Type, Token.Object);
        }
    }
}

void
WalkAndResetEventEdges(granny_data_type_definition* Type,
                       void*                        Object)
{
    GSTATE_AUTO_ZONE_FN();

    if (Type == 0 || Object == 0)
        return;


    granny_variant ContainerToken;
    if (!GrannyFindMatchingMember(Type, Object, "containerToken", &ContainerToken))
        return;


    granny_variant ChildTokenArray;
    if (!GrannyFindMatchingMember(ContainerToken.Type->ReferenceType, *(void**)ContainerToken.Object, "ChildTokens", &ChildTokenArray))
        return;

    GStateAssert(ChildTokenArray.Type->Type == GrannyReferenceToArrayMember);

    granny_int32&   Count = *(granny_int32*)(ChildTokenArray.Object);
    granny_variant* Array = *(granny_variant**)((granny_uint8*)ChildTokenArray.Object + sizeof(granny_int32));

    for (int Idx = 0; Idx < Count; ++Idx)
    {
        granny_variant& Token = Array[Idx];

        // Correct
        if (strcmp(Token.Type->Name, "ContextUID_anim_source") == 0)
        {
            anim_source::ResetEventEdges((anim_source_token*)Token.Object);
        }
        else if (strcmp(Token.Type->Name, "ContextUID_state_machine") == 0 ||
            strcmp(Token.Type->Name, "ContextUID_blend_graph") == 0)
        {
            // Recurse...
            WalkAndResetEventEdges(Token.Type, Token.Object);
        }
    }
}

gstate_character_info*
GStateGetCharacterInfo(granny_file* File)
{
    if (File == 0)
        return 0;

    GSTATE_AUTO_ZONE_FN();

    gstate_character_info* InfoPtr = 0;

    granny_variant Root;
    GrannyGetDataTreeFromFile(File, &Root);

    granny_uint32 TypeTag = File->Header->TypeTag;
    if (TypeTag == GStateCharacterInfoTag)
    {
        InfoPtr = (gstate_character_info*)Root.Object;
    }
    else
    {
        // Ok, we need to do the basic upgrade, then scan through and look for
        // variants that we need to update separately.  So let's get the size of the
        // conversion buffer first, then look for our custom bits.
        if (!File->ConversionBuffer)
        {
            GStateWarning("File has run-time type tag of 0x%x, which doesn't match this "
                          "version of GState (0x%x).  Automatic conversion will "
                          "be attempted.", TypeTag, GStateCharacterInfoTag);

            File->ConversionBuffer =
                GrannyConvertTree(Root.Type, Root.Object,
                                  GStateCharacterInfoType,
                                  &token_context::UpdateVariantHandler,
                                  &File->ConversionBufferSize);
        }

        InfoPtr = (gstate_character_info *)File->ConversionBuffer;
        if (InfoPtr)
        {
            // Run the special case for the anim_source nodes...
            WalkAndConvertInContainer(InfoPtr->StateMachine.Type,
                                      InfoPtr->StateMachine.Object);
        }
    }

    return InfoPtr;
}

bool
GStateReadCharacterInfoFromReader(granny_file_reader*     Reader,
                                  granny_file*&           FilePtr,
                                  gstate_character_info*& InfoPtr)
{
    // Clear out the result variables
    FilePtr = GrannyReadEntireFileFromReader(Reader);
    InfoPtr = GStateGetCharacterInfo(FilePtr);

    return (InfoPtr != 0);
}

bool
GStateReadCharacterInfo(char const*             Filename,
                        granny_file*&           FilePtr,
                        gstate_character_info*& InfoPtr)
{
    // Clear out the result variables
    InfoPtr = 0;
    FilePtr = 0;

    granny_file_reader* Reader = GrannyCreatePlatformFileReader(Filename);
    if (Reader == 0)
        return false;

    bool Success = GStateReadCharacterInfoFromReader(Reader, FilePtr, InfoPtr);
    GrannyCloseFileReader(Reader);

    return Success;
}

// =============================================================================
granny_int32x
GStateGetNumAnimationSets(gstate_character_info* Info)
{
    return Info->AnimationSetCount;
}

char const*
GStateGetAnimationSetName(gstate_character_info* Info, granny_int32x SetIndex)
{
    GStateCheckIndex(SetIndex, Info->AnimationSetCount, return 0);

    return Info->AnimationSets[SetIndex]->Name;
}



granny_int32x
GStateSetIndexFromSetName(gstate_character_info*   Info,
                          char const*              SetName)
{
    GStateCheckPtrNotNULL(Info, return false);
    GStateCheckPtrNotNULL(SetName, return false);
    
    // Find SetName in the AnimationSets....
    {for (int Idx = 0; Idx < Info->AnimationSetCount; ++Idx)
    {
        GStateCheckPtrNotNULL(Info->AnimationSets[Idx], continue);
        
        if (_stricmp(Info->AnimationSets[Idx]->Name, SetName) == 0)
        {
            return Idx;
        }
    }}

    GStateWarning("%s: Set '%s' not found\n", __FUNCTION__, SetName);
    return -1;
}

bool GSTATE
ValidateReference(source_file_ref*  Ref,
                  granny_file_info* NewInfo)
{
    granny_uint32 NewCRC = ComputeAnimCRC(NewInfo);
    if (Ref->ExpectedAnimCount != NewInfo->AnimationCount)
    {
        return false;
    }
    else if (Ref->AnimCRC != NewCRC)
    {
        return false;
    }

    return true;
}


bool
GStateBindCharacterFileReferencesForSetIndex(gstate_character_info*   Info,
                                             granny_int32x            SetIndex,
                                             gstate_file_ref_callback RefCallback,
                                             void*                    UserData)
{
    GSTATE_AUTO_ZONE_FN();

    GStateCheckPtrNotNULL(Info, return false);
    GStateCheckPtrNotNULL(RefCallback, return false);
    GStateCheckIndex(SetIndex, Info->AnimationSetCount, return false);

    animation_set* ThisSet = Info->AnimationSets[SetIndex];
    
    bool AllSucceeded = true;
    for (int SourceFileReferenceIdx = 0; SourceFileReferenceIdx < ThisSet->SourceFileReferenceCount; ++SourceFileReferenceIdx)
    {
        source_file_ref* Ref = ThisSet->SourceFileReferences[SourceFileReferenceIdx];
        GStateAssert(Ref);

        if (Ref->SourceInfo == 0)
        {
            granny_file_info* NewInfo = RefCallback(Info, Ref->SourceFilename, UserData);
            if (NewInfo == 0)
            {
                AllSucceeded = false;
                GStateWarning("Expected to find source '%s'", Ref->SourceFilename);
                continue;
            }

            if (Ref->ExpectedAnimCount != 0 && Ref->AnimCRC != 0)
            {
                if (ValidateReference(Ref, NewInfo))
                {
                    // All fine.
                    Ref->SourceInfo = NewInfo;
                }
                else
                {
                    GStateError("Animation count or names in '%s' changed since the GSF was changed. Re-indexing. This is slow. Open and re-save the GSF to fix it.", 
                        Ref->SourceFilename);

                    if( NewInfo )
                    {
                        for (int AnimationSpecIdx = 0; AnimationSpecIdx < ThisSet->AnimationSpecCount; ++AnimationSpecIdx)
                        {
                            animation_spec& Spec = ThisSet->AnimationSpecs[AnimationSpecIdx];

                            if( _stricmp(Ref->SourceFilename, Spec.SourceReference->SourceFilename) == 0 )
                            {                                    
                                if (Spec.ExpectedName != 0)
                                {
                                    int PreviousAnimationIndex = Spec.AnimationIndex;
                                    int NewAnimationIndex = -1;
                                    for(int AnimIdx=0; AnimIdx<NewInfo->AnimationCount; AnimIdx++)
                                    {
                                        if( _stricmp(NewInfo->Animations[AnimIdx]->Name, Spec.ExpectedName) == 0 )
                                        {
                                            NewAnimationIndex = AnimIdx;
                                            if( NewAnimationIndex != PreviousAnimationIndex )
                                            {
                                                GStateWarning("Animation Index for [%s] changed in [%s]. Fixing with a slow runtime lookup. RE-SAVE THE GSF to store the proper index for runtime use.", Spec.ExpectedName, Spec.SourceReference->SourceFilename);
                                            }
                                            break;
                                        }
                                    }

                                    Spec.AnimationIndex = NewAnimationIndex;
                                    if( NewAnimationIndex == -1 )
                                    {
                                        //We do not set Spec.AnimationIndex here. Just because we could not load the animation does not mean we should clear out those values. 
                                        //Note: this does not affect AllSucceeded. GState can handle it if it loses an index to something, so there's no need to error out. 
                                        GStateError("Animation [%s] missing in [%s].", Spec.ExpectedName, Spec.SourceReference->SourceFilename);
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // Update the reference
            Ref->ExpectedAnimCount = NewInfo->AnimationCount;
            Ref->AnimCRC           = ComputeAnimCRC(NewInfo);
            Ref->SourceInfo        = NewInfo;
        }
    }
    return AllSucceeded;
}

bool
GStateBindCharacterFileReferences(gstate_character_info*   Info,
                                  gstate_file_ref_callback RefCallback,
                                  void*                    UserData)
{
    GSTATE_AUTO_ZONE_FN();

    GStateCheckPtrNotNULL(Info, return false);
    GStateCheckPtrNotNULL(RefCallback, return false);

    bool AllSucceeded = true;

    {for (int Idx = 0; Idx < Info->AnimationSetCount; ++Idx)
    {
        AllSucceeded =
            GStateBindCharacterFileReferencesForSetIndex(
                Info, Idx,
                RefCallback, UserData)
            && AllSucceeded;
    }}

    return AllSucceeded;
}

bool GStateBindCharacterFileReferences(gstate_character_info*   Info,
                                       gstate_file_ref_callback RefCallback,
                                       void*                    UserData);
/*
  Binds all source references for Info.

  $:Info The character info to bind
  $:RefCallback Callback function to load the granny_file_infos
  $:UserData Arbitrary context pointer

  Note that if multiple animation sets are present in the $gstate_character_info, this
  function will load all of them.  If you want to load only one specific character
  variant, use $GStateBindCharacterFileReferencesForSetIndex.
*/



granny_uint32 GSTATE
ComputeAnimCRC(granny_file_info* FileInfo)
{
    // Handy little function to do a quick check that the animation names/count are what
    // we expect from the file.  This allows us to hopefully detect when a source file has
    // changed underneath us
    granny_uint32 CRC = 0x1;
    GrannyBeginCRC32(&CRC);
    for (granny_int32 Idx = 0; Idx < FileInfo->AnimationCount; ++Idx)
    {
        GrannyAddToCRC32(&CRC, sizeof(Idx), &Idx);
        if (FileInfo->Animations[Idx]->Name)
            GrannyAddToCRC32(&CRC, strlen(FileInfo->Animations[Idx]->Name), FileInfo->Animations[Idx]->Name);
    }

    GrannyEndCRC32(&CRC);
    return CRC;
}
