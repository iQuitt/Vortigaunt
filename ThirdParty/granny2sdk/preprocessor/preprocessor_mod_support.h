// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(PREPROCESSOR_MOD_SUPPORT_H)

#if BUILDING_GRANNY_FOR_MODS

#ifndef MOD_PREPROCESSOR_NAME
  #error "MOD_PREPROCESSOR_NAME must indicate what will be displayed to the user"
  // #define MOD_PREPROCESSOR_NAME "YourGame Processor"
#endif

// Must match MOD_FILE_EXTENSION in granny_mod_support.h The preprocessor is built with
// access to only the binary interface, so we cannot depend on that header being present
// here.
#ifndef MOD_FILE_EXTENSION
  #error "MOD_FILE_EXTENSION should be defined"
  // #define MOD_FILE_EXTENSION "MGF"
#endif

// Similarly, must match it's counterpart in granny_mod_support.h
#ifndef MOD_UNIQUE_PREFIX
   #error "MOD_UNIQUE_PREFIX must be a short unique string for your application"
   // #define MOD_UNIQUE_PREFIX "UniqShortString"
#endif


// Set this to 1 to force the preprocessor to *always* write MOD files.
#ifndef PREPROCESSOR_ONLY_WRITE_MOD
   #define PREPROCESSOR_ONLY_WRITE_MOD 0
#endif

#endif

#define PREPROCESSOR_MOD_SUPPORT_H
#endif /* PREPROCESSOR_MOD_SUPPORT_H */
