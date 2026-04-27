// =======================================================================================
// string_table.cpp: demonstrate how to replace common strings during Granny File I/O
//
// Copyright 1999-2006 by RAD Game Tools, Inc., All Rights Reserved.
// =======================================================================================
#include "granny.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <map>
#include <string>
#include <sys/stat.h>


GRANNY_CALLBACK(granny_uint32) StringCallback(void * Data, char const * String);
GRANNY_CALLBACK(char *) RetrieveStringCallback(void* Data, granny_uint32 Lookup);

typedef std::map<granny_uint32, std::string> string_table_type;

void InfoDump(granny_file_info* Info, const char* Description)
{
    printf("\n");
    printf("%s: exported by \"%s\""
           "         from \"%s\"\n",
           Description,
           Info->ExporterInfo->ExporterName,
           Info->ArtToolInfo->FromArtToolName);

    if (Info->ModelCount != 0)
    {
        printf("  Model names:\n");
        {for(granny_int32 Idx = 0; Idx < Info->ModelCount; Idx++)
        {
            printf("   %d: %s\n", Idx, Info->Models[Idx]->Name);
        }}
    }

    if (Info->MeshCount != 0)
    {
        printf("  Mesh names:\n");
        {for(granny_int32 Idx = 0; Idx < Info->MeshCount; Idx++)
        {
            printf("   %d: %s\n", Idx, Info->Meshes[Idx]->Name);
        }}
    }

    printf("\n");
}


/* DEFTUTORIAL EXPGROUP(TutorialStringTable) (TutorialStringTable_Introduction, Introduction) */
/* One of the nicest features of Granny is that most element matching and versioning is
   done by string matching.  This allows you to ignore problems like mismatched array
   indices across multiple files, data type ordering issues when upgrading file versions,
   and similar problems.  One of the downsides of this flexibility is that there is a lot
   of duplicated string data in any given Granny file.
   $-

   For instance, a $skeleton contains an array of $bone objects, each of which has a
   "Name" field.  This is used to match against the "Name" field in the $transform_track
   objects contained in $animation objects, which allows these arrays to be in different
   orders without causing problems.  However, if you start figuring out how much space is
   taken up by these names ...(20 bytes/bone, 100 bones/character ~= 2000 bytes per
   character per file)...  It adds up.
   $-

   Luckily, you can do something about this problem.  Granny allows you to specify a
   callback function on file writes and file reads that is used to replace the string
   literals stored by default with table indices that lookup the strings in a shared
   database of strings.  That way, each of your bone names is represented once and only
   once, each $data_type_definition field name is represented only once, and so forth.
   $-

   With the release of Granny 2.7.0.17, we've made using the string replacement API much
   easier, and integrated the feature with the preprocessor.  You can use the preprocessor
   commands "ExtractStrings" and "RemapStrings" to build a $string_database object, and
   remap the string literals in your files to indices in the shared database.  At runtime,
   you simply load the file containing the $string_database, and after loading a
   $file_info, simply replace the string indices with the correct pointers, and you're
   set!
   $-

   <b>Note:</b> while we've made this as easy as possible in newer versions of the
   library, this is still considered an "advanced" topic in Granny, and something you
   should only undertake if you're prepared to get into the inner workings of the Granny
   file and type systems.  Still with us?  Ok, let's dive in.
   $-

   So there's a little bit of prep work that we do with the preprocessor to make this
   work.  We're going to extract the strings from two of our example files to create the
   string database, then remap the strings to the appropriate indices in that table.
   Since that happens at the command-line, rather than in the program, I've provided the
   batch file to run the correct commands, you can find it in the media directory of the
   string_table tutorial as "prep_files.bat".  The output of this script consists of three
   files, "database.gsd", "gryphon_remap.gr2", and "pixo_run_remap.gr2".  In general, you
   should name your $(string_database)s files with the extension "GSD" to distinguish them
   from files encoding $file_info structures.  The results are already present in the
   distribution, but you can rerun the script at anytime to get the idea.

   Prerequisites to understand this demo:
   $* $TutorialBasicLoading
   $-

   New functions and types discussed in this tutorial:
   $* $string_database
   $* $GetStringDatabase
   $* $RemapFileStrings
   $-
*/

/* DEFTUTORIAL EXPGROUP(TutorialStringTable) (TutorialStringTable_main, main) */
int main(int, char const*[])
{
    // Check the version
    if(!GrannyVersionsMatch)
    {
        printf("Warning: the Granny DLL currently loaded "
               "doesn't match the .h file used during compilation\n");
        return EXIT_FAILURE;
    }

#ifdef _MACOSX
    #define BASE_DIR "../../media/"
#else
    #define BASE_DIR "string_table/media/"
#endif

    /* The first step is to simply create the $file objects as you normally would.  Note
       that a ".gsd" file is loaded exactly as a ".gr2" file at this stage. */
    granny_file* StringsFile = GrannyReadEntireFile(BASE_DIR "database.gsd");
    granny_file* PixoFile    = GrannyReadEntireFile(BASE_DIR "pixo_run_remap.gr2");
    granny_file* GryphonFile = GrannyReadEntireFile(BASE_DIR "gryphon_remap.gr2");
    if (!StringsFile || !PixoFile || !GryphonFile)
    {
        printf("Couldn't load one of the source files\n");
        return EXIT_FAILURE;
    }

    /* Get the $string_database object from the strings file.  Because the file that
       contains the strings database is always written out with it's type information
       intact, containing full string information, we don't have to be concerned with
       versioning issues which we discuss later for $file_info objects.
    */
    granny_string_database* StringDB = GrannyGetStringDatabase(StringsFile);
    if (!StringDB)
    {
        printf("Failed to obtain a granny_string_database from the specified file.\n");
        return EXIT_FAILURE;
    }

    printf("String database contains %d strings.  First few strings follow:\n",
           StringDB->StringCount);
    {for(granny_int32 Idx = 0; Idx < 10 && Idx < StringDB->StringCount; ++Idx)
    {
        printf("  Idx %d: \"%s\"\n", Idx, StringDB->Strings[Idx]);
    }}

    /* Now we obtain the $file_info objects for our Pixo runcycle, and the Gryphon.
       There's a subtlety here that you have to handle correctly.  If the version of
       Granny matches the version of the file, you could simply call $GetFileInfo, and use
       the $RebasePointersStringCallback to patch the file up afterwards.  This still
       works, but we strongly recommend you use the new API for patching strings at the
       $file level.  The reason for this is to correctly support versioning.
       $-

       The Granny file versioning essentially works by comparing the metadata stored in
       the file as $data_type_definition entries, with the current version of the type
       that you're converting to, in this case $file_info.  The problem is that member
       matching, in the case of updated or changed structures is done by field name.  With
       remapped strings, the name fields in the $(data_type_definition)s of the old file
       are indices, which cannot be directly compared to the strings in the in-memory copy
       of the $file_info type information.  You could envision a hack where we register a
       string callback, and look at the pointer values in the comparison to see if they
       fall within the reasonable index range, but that's hardly industrial strength.  So
       we've provided an API, $RemapFileStrings, which accepts a $file object and replaces
       the string entries in its encoded type tree with the entries from a
       $string_database.  This ensures that if a version upgrade is required in
       $GetFileInfo, the type information has proper strings to match against.
     */
    int success = EXIT_SUCCESS;
    if (GrannyRemapFileStrings(PixoFile, StringDB) &&
        GrannyRemapFileStrings(GryphonFile, StringDB))
    {
        /* Now that the strings are remapped, it's safe to obtain the $file_info objects */
        granny_file_info* PixoInfo = GrannyGetFileInfo(PixoFile);
        granny_file_info* GryphonInfo = GrannyGetFileInfo(GryphonFile);

        /* Dump some random information about the files that include strings, to verify that
           everything came over correctly.
        */
        InfoDump(PixoInfo, "Pixo Run Cycle");
        InfoDump(GryphonInfo, "Gryphon Model");
    }
    /* $RemapFileStrings will return false if the database passed in doesn't match the tag
       stored in the file header to prevent errors arising from using older or newer
       databases with already remapped files.
     */
    else
    {
        printf("Unable to remap file strings, possible database mismatch?\n");
        success = EXIT_FAILURE;
    }

    GrannyFreeFile(PixoFile);
    GrannyFreeFile(GryphonFile);

    return success;
}


/* DEFTUTORIAL EXPGROUP(TutorialStringTable) (TutorialStringTable_Summary, Summary) */
/* String replacement can be a powerful tool for reducing the runtime memory cost of your
   Granny files without giving up the flexibility of string-matched bones and tracks.  You
   do have to be careful about object lifetimes and the points at which a $file object
   contains indices or actual pointers, but with care, it can be done.  If you're having
   trouble, please write us at <a
   href="mailto:granny3@radgametools.com">granny3@radgametools.com</a> for more
   information.

   $- If you haven't already, you might want to check out:
   $* $TutorialReplaceFileAPI: replace Granny's file IO system
   $* $TutorialPreprocessor: Learn how to modify the Granny preprocessing tool.
   $-
*/

/* DEFTUTORIALEND */
