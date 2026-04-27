#pragma once
// ========================================================================
// $File: //jeffr/granny_29/gstate/gstate_character_info.cpp $
// $DateTime: 2014/03/05 11:20:23 $
// $Change: 46779 $
// $Revision: #9 $
//
// $Notice: $
// ========================================================================
#include "../gstate_header_prefix.h"

#ifndef GSTATE_BASE_H
#include "../gstate_base.h"
#endif


typedef granny_file_info* GSTATE_CALLBACK gstate_slot_callback(gstate_character_info* BindingInfo,
                                                               char const*            SlotName,
                                                               char const*            Set0Filename,
                                                               void*                  UserData);
bool GStateBindCharacterSlots(gstate_character_info* Info,
                              gstate_slot_callback*  SlotCallback,
                              void*                  UserData);
/*
  Binds all source references for Info, by slot name

  $:Info The character info to bind
  $:SlotCallback Callback function to load the granny_file_infos
  $:UserData Arbitrary context pointer

  This is a slightly altered version of the binding procedure Note that if multiple animation sets
  are present in the $gstate_character_info, this function will only load a single set.  You are
  passed the filename for set0 as a helper to the slot name.
*/


#include "../gstate_header_postfix.h"
