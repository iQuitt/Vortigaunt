#if !defined(REPLACE_FILE_API_HELPER_H)
// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "granny.h"

bool InitializeVFS(const char** Filenames,
                   const int    NumFilenames);
void CleanupVFS();

bool LookupVFSFile(const char* Filename,
                   void** OutputFiledata,
                   int*   OutputFilesize);

void PrintFileStatistics(granny_file* GrannyFile);
void PrintFileInfoStatistics(granny_file_info* GrannyFileInfo);



#define REPLACE_FILE_API_HELPER_H
#endif
