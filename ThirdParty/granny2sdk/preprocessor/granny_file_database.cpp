// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "granny_file_database.h"
#include <stdarg.h>
#include <stdio.h>

static granny_data_type_definition GR2FileType[] =
{
    { GrannyStringMember,    "Filename" },
    { GrannyReferenceMember, "FileInfo", GrannyFileInfoType },
    { GrannyEndMember }
};

static granny_data_type_definition GSFFileType[] =
{
    { GrannyStringMember,    "Filename" },
    { GrannyReferenceMember, "CharacterInfo", GStateCharacterInfoType },
    { GrannyEndMember }
};

granny_data_type_definition GrannyFileDatabaseType[] =
{
    { GrannyReferenceToArrayMember, "GR2Files", GR2FileType },
    { GrannyReferenceToArrayMember, "GSFFiles", GSFFileType },
    { GrannyEndMember }
};


static void
LogForward(char const* File, int Line,
           granny_log_message_type Type,
           char const* Message, ...)
{
    va_list args;
    va_start(args, Message);

    char MessageBuffer[512];
#if !UNSAFE_VSNPRINTF
    vsnprintf_s(MessageBuffer, sizeof(MessageBuffer), _TRUNCATE, Message, args);
#else
    vsnprintf(MessageBuffer, sizeof(MessageBuffer), Message, args);
#endif

    va_end(args);

    granny_log_callback Callback;
    GrannyGetLogCallback(&Callback);
    if (Callback.Function)
    {
        (*Callback.Function)(Type, GrannyApplicationLogMessage,
                             File, Line,
                             MessageBuffer, Callback.UserData);
    }
}


granny_file_database*
GrannyGetFileDatabase(granny_file* File)
{
    granny_variant Root;
    GrannyGetDataTreeFromFile(File, &Root);

    granny_uint32 TypeTag = File->Header->TypeTag;
    if (TypeTag == GrannyCurrentFDBTag)
    {
        return((granny_file_database *)Root.Object);
    }
    else
    {
        if (!File->ConversionBuffer)
        {
            LogForward(__FILE__, __LINE__, GrannyWarningLogMessage,
                       "File has run-time type tag of 0x%x, which doesn't match this "
                       "version of Granny (0x%x).  Automatic conversion will "
                       "be attempted.",TypeTag, GrannyCurrentFDBTag);

            File->ConversionBuffer =
                GrannyConvertTree(Root.Type, Root.Object, GrannyFileDatabaseType, 0, 0);

            // Extract the portion of the type tag that represents the CharacterInfoTag.
            // This is 10 bits, shifted up 6 in the current version
            granny_uint32 TypeCharInfo    = (TypeTag >> 6) & 0x3ff;
            granny_uint32 CurrentCharInfo = (GrannyCurrentFDBTag >> 6) & 0x3ff;
            if (CurrentCharInfo != (GStateCharacterInfoTag & 0x3ff))
            {
                LogForward(__FILE__, __LINE__, GrannyErrorLogMessage,
                           "VERY UNEXPECTED!  Character Info Tag miscomputed in granny_file_database.\n");
            }

            if (TypeCharInfo != CurrentCharInfo)
            {
                granny_file_database* FileDB = (granny_file_database*)File->ConversionBuffer;
                if (FileDB && FileDB->GSFFileCount)
                {
                    LogForward(__FILE__, __LINE__, GrannyErrorLogMessage,
                               "Mismatched gstate_character_info tag in GrannyGetFileDatabase.  Granny does\n"
                               "not currently support updating GSF files inside a granny_file_database.\n"
                               "*** THESE ELEMENTS WILL BE ZEROED OUT ***\n");
                    FileDB->GSFFileCount = 0;
                    FileDB->GSFFiles     = 0;
                }
            }
        }

        return((granny_file_database *)File->ConversionBuffer);
    }
}

