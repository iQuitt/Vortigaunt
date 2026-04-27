// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#if !defined(GRANNY_FILE_DATABASE_H)

#include "granny.h"
#include "gstate.h"

struct granny_filedb_gr2_entry
{
    char const*       Filename;
    granny_file_info* FileInfo;
};

struct granny_filedb_gsf_entry
{
    char const*            Filename;
    gstate_character_info* CharacterInfo;
};

struct granny_file_database
{
    granny_int32             GR2FileCount;
    granny_filedb_gr2_entry* GR2Files;

    // Note that because GSF files have a custom upgrade path, we don't currently support
    // updating these in place.  The bits of the FDBTag that represent the CharInfoTag must
    // match perfectly for GetFileDataBase to return these.
    granny_int32             GSFFileCount;
    granny_filedb_gsf_entry* GSFFiles;
};

extern granny_data_type_definition GrannyFileDatabaseType[];

// I am taking advantage of the fact that I know that CurrentGRNStandardTag and GStateCIStandardTag
//  have the form:
//  CurrentGRNStandardTag (0x80000000 + 55)
//  GStateCharacterInfoTag (0x80657473 + 14)
//
// Essentially, I'm going to move the lowest (varying, in other words) bits of those
// macros into known locations in my own tag.  It's unlikely that they will get into the
// 1024 range without a major file system rewrite, so 10 bits will do for now.  That
// leaves 6 bits at the bottom for this tag to vary, which for such a simple format, is
// more than enough.
//
#define GrannyCurrentFDBTag                             \
    ((0x82000000 |                                      \
      (((GrannyCurrentGRNStandardTag) & 0x3ff) << 16) | \
      (((GStateCharacterInfoTag) & 0x3ff) <<  6)) +     \
                                                        \
     1)  // current version
    

granny_file_database* GrannyGetFileDatabase(granny_file* File);

#define GRANNY_FILE_DATABASE_H
#endif /* GRANNY_FILE_DATABASE_H */
