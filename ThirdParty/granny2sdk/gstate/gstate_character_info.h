// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GSTATE_INFO_H)
#include "gstate_header_prefix.h"

#ifndef GSTATE_BASE_H
#include "gstate_base.h"
#endif

EXPGROUP(CharacterInfoGroup)

EXPTYPE struct gstate_character_info;
/*
  The container format for a state machine.  Note that unlike granny_file_info, this is an
  opaque structure in the basic API.  Be sure to check out the list of functions that you
  can query this structure with.
*/


extern granny_data_type_definition GStateCharacterInfoType[];

EXPAPI gstate_character_info* GStateGetCharacterInfo(granny_file* File);
/*
  Extracts a $gstate_character_info from the file container.

  $:File A granny_file, usually from GrannyReadEntireFile(...)
  $:return NULL on error, or a valid $gstate_character_info
*/

EXPAPI bool GStateReadCharacterInfo(EXPIN  char const*             Filename,
                                    EXPOUT granny_file*&           FilePtr,
                                    EXPOUT gstate_character_info*& InfoPtr);
/*
  Loading and extraction in one function.

  $:Filename The file to read
  $:FilePtr A reference to a granny_file*.  Release this with GrannyFreeFile when all references to the $gstate_character_info are released.
  $:InfoPtr The output $gstate_character_info.
  $:return true on success, false on failure (both pointers will be set to NULL).

  This function is a convenient way to read and extract a $gstate_character_info.  GState
  will handle calling GrannyReadEntireFile and managing any errors.  Once all characters
  are through referring to InfoPtr, make sure to free the underlying memory with
  GrannyFreeFile.
*/


EXPAPI bool GStateReadCharacterInfoFromReader(EXPIN  granny_file_reader*     Reader,
                                              EXPOUT granny_file*&           FilePtr,
                                              EXPOUT gstate_character_info*& InfoPtr);
/*
  Similar to $GStateReadCharacterInfo, but loads from a granny_file_reader

  $:Reader The stream to load from
  $:FilePtr A reference to a granny_file*.  Release this with GrannyFreeFile when all references to the $gstate_character_info are released.
  $:InfoPtr The output $gstate_character_info.
  $:return true on success, false on failure (both pointers will be set to NULL).
*/

EXPAPI typedef granny_file_info* GSTATE_CALLBACK gstate_file_ref_callback(EXPIN gstate_character_info* BindingInfo,
                                                                          EXPIN char const*            SourceFileName,
                                                                          EXPIN void*                  UserData);
/*
  Callback to load a source in $GStateBindCharacterFileReferences.

  $:BindingInfo Pointer to the structure we are binding (opaque, but you can use this as a DB key, if necessary)
  $:SourceFileName The name of the source file desired
  $:UserData Context information passed to $GStateBindCharacterFileReferences

  This callback will be repeatedly invoked by $GStateBindCharacterFileReferences to load
  the underlying sources for a state machine. SourceFileName by default is stored as a
  relative path to the GSF, so if you have "c:\characters\file.gsf" that refers to
  animations in "c:\characters\anims\anim.gr2", SourceFileName will come through as
  "anims\anim.gr2".

  The intention is that your game will cache the results of the load so that multiple
  references to the same GR2 can resolve to the same granny_file_info* without being
  loaded multiple times.
*/

EXPAPI granny_int32x GStateGetNumAnimationSets(EXPIN gstate_character_info* Info);
/*
  Returns the number of animation sets specified in the $gstate_character_info.
*/

EXPAPI char const* GStateGetAnimationSetName(EXPIN gstate_character_info* Info,
                                             EXPIN granny_int32x SetIndex);
/*
  Returns the name of the animation set specified.

  $:Info The info to examine
  $:SetIndex The animation set to look at.  Note that this must be [0, $GStateGetNumAnimationSets(Info))
  $:return The name of the set. This pointer must be left untouched!
  
*/

EXPAPI granny_int32x GStateSetIndexFromSetName(EXPIN gstate_character_info* Info,
                                               EXPIN char const*            SetName);
/*
  Finds the index of the animation set named

  $:Info The info to examine
  $:SetName The name to look for
  $:return The index of the animation set, or -1 on failure.
*/
                                       
EXPAPI bool GStateBindCharacterFileReferences(EXPIN gstate_character_info*   Info,
                                              EXPIN gstate_file_ref_callback RefCallback,
                                              EXPIN void*                    UserData);
/*
  Binds all source references for Info.

  $:Info The character info to bindl
  $:RefCallback Callback function to load the granny_file_infos
  $:UserData Arbitrary context pointer

  Note that if multiple animation sets are present in the $gstate_character_info, this
  function will load all of them.  If you want to load only one specific character
  variant, use $GStateBindCharacterFileReferencesForSetIndex.
*/

EXPAPI bool GStateBindCharacterFileReferencesForSetIndex(EXPIN gstate_character_info*   Info,
                                                         EXPIN granny_int32x            SetIndex,
                                                         EXPIN gstate_file_ref_callback RefCallback,
                                                         EXPIN void*                    UserData);
/*
  Binds source references for a specific animation set in Info.

  $:Info The character info to bind
  $:SetIndex The animation set to load.
  $:RefCallback Callback function to load the granny_file_infos
  $:UserData Arbitrary context pointer

  Essentially the same as $GStateBindCharacterFileReferences, but restricts its action to
  a single animation set.  This can be useful if you need to load only a given subset of
  the character for speed reasons.
*/

EXPAPI void
WalkAndResetEventEdges(EXPIN granny_data_type_definition* Type,
                       EXPIN void*                        Object);


EXPTYPE enum gstate_character_info_tag
{
    GStateCIUserTag     = 0x40000000, // Mask for User-specified formats (i.e, those containing custom nodes)
    GStateCIStandardTag = 0x80657473  // Granny's standard tag base value
};
/*
  Tag values for determining file age.
*/

#define GStateCharacterInfoTag ((granny_uint32)(GStateCIStandardTag + 122)) EXPMACRO
/*
  Changing the file format, adding a class?  Don't forget to increment this tag.  For user
  Graph formats (i.e, modified versions of the library with extra nodes, don't forget to
  "or" in GStateCIUserTag from $gstate_character_info_tag.
*/

#include "gstate_header_postfix.h"
#define GSTATE_INFO_H
#endif /* GSTATE_INFO_H */
