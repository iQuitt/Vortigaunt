// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "replace_file_api_helper.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

// This is extremely simple, and obviously not particularly robust.
granny_uint8* GlobalVFSData      = NULL;
char**        GlobalVFSFilenames = NULL;
int*          GlobalVFSOffset    = NULL;
int*          GlobalVFSSize      = NULL;
int           GlobalNumFiles     = -1;

bool InitializeVFS(const char** Filenames,
                   const int    NumFilenames)
{
    assert(Filenames);
    assert(NumFilenames > 0);
    {for(int i = 0; i < NumFilenames; ++i)
    {
        assert(Filenames[i]);
    }}

    bool Success = true;
    FILE** Filehandles = new FILE*[NumFilenames];
    {for(int i = 0; i < NumFilenames; ++i)
    {
        Filehandles[i] = fopen(Filenames[i], "rb");
        if (!Filehandles[i])
            Success = false;
    }}

    if (Success)
    {
        // GlobalVFSData after we know the size
        GlobalVFSFilenames = new char*[NumFilenames];
        GlobalVFSOffset    = new int[NumFilenames];
        GlobalVFSSize      = new int[NumFilenames];
        GlobalNumFiles     = NumFilenames;

        int TotalSize = 0;
        {for(int i = 0; i < NumFilenames; ++i)
        {
            fseek(Filehandles[i], 0, SEEK_END);
            GlobalVFSSize[i] = ftell(Filehandles[i]);
            fseek(Filehandles[i], 0, SEEK_SET);

            GlobalVFSOffset[i] = TotalSize;
            TotalSize += GlobalVFSSize[i];

            GlobalVFSFilenames[i] = new char[strlen(Filenames[i]) + 1];
            strcpy(GlobalVFSFilenames[i], Filenames[i]);
        }}

        GlobalVFSData = new granny_uint8[TotalSize];
        {for(int i = 0; i < NumFilenames; ++i)
        {
            fread(GlobalVFSData + GlobalVFSOffset[i],
                  GlobalVFSSize[i], 1, Filehandles[i]);
            fclose(Filehandles[i]);
        }}
    }
    else
    {
        // Failed, cleanup
        {for(int i = 0; i < NumFilenames; ++i)
        {
            if (Filehandles[i])
                fclose(Filehandles[i]);
        }}
    }

    return Success;
}

void CleanupVFS()
{
    delete [] GlobalVFSData;
    GlobalVFSData = NULL;

    if (GlobalVFSFilenames)
    {
        {for(int i = 0; i < GlobalNumFiles; ++i)
        {
            delete [] GlobalVFSFilenames[i];
        }}
        delete [] GlobalVFSFilenames;
    }

    delete [] GlobalVFSOffset;
    GlobalVFSOffset = NULL;
    delete [] GlobalVFSSize;
    GlobalVFSSize = NULL;

    GlobalNumFiles = -1;
}

bool LookupVFSFile(const char* Filename,
                   void** OutputFiledata,
                   int*   OutputFilesize)
{
    assert(Filename);
    assert(OutputFiledata);
    assert(OutputFilesize);

    *OutputFiledata = NULL;
    *OutputFilesize = -1;

    {for(int i = 0; i < GlobalNumFiles; ++i)
    {
        if (strstr(GlobalVFSFilenames[i], Filename) != NULL)
        {
            *OutputFiledata = GlobalVFSData + GlobalVFSOffset[i];
            *OutputFilesize = GlobalVFSSize[i];
            return true;
        }
    }}

    return false;
}


// This function is the same as the function in basic_loading, more or less.
void PrintFileStatistics(granny_file* GrannyFile)
{
    assert(GrannyFile);
    printf("\n" "granny_file\n" "-----------\n");

    granny_grn_section* SectionArray = GrannyGetGRNSectionArray(GrannyFile->Header);
    assert(SectionArray);

    printf("File contains: %d sections.\n", GrannyFile->SectionCount);
    {for(granny_int32x Section = 0; Section < GrannyFile->SectionCount; ++Section)
    {
        if (GrannyFile->Sections[Section])
        {
            printf("  Section %d: present", Section);
            if (SectionArray[Section].DataSize ==
                SectionArray[Section].ExpandedDataSize)
            {
                printf(" (uncompressed)\n");
            }
            else
            {
                printf(" (compressed)\n");
            }
        }
        else
        {
            // This will likely never happen, since we haven't called GrannyFreeFileSection
            printf("  Section %d: already freed\n", Section);
        }
    }}
    printf("\n");
}

// This function is the same as the function in basic_loading, more or less.
void PrintFileInfoStatistics(granny_file_info* GrannyFileInfo)
{
    assert(GrannyFileInfo);
    printf("\n" "granny_file_info\n" "----------------\n");

    if (GrannyFileInfo->ArtToolInfo)
    {
        granny_art_tool_info* ToolInfo = GrannyFileInfo->ArtToolInfo;

        printf("File created by %s (%d.%d)\n",
               ToolInfo->FromArtToolName,
               ToolInfo->ArtToolMajorRevision,
               ToolInfo->ArtToolMinorRevision);
        printf("  Coordinate system\n"
               "    Units per meter: %f\n"
               "       Right Vector: (%f %f %f)\n"
               "          Up Vector: (%f %f %f)\n"
               "        Back Vector: (%f %f %f)\n",
               ToolInfo->UnitsPerMeter,
               ToolInfo->RightVector[0], ToolInfo->RightVector[1], ToolInfo->RightVector[2],
               ToolInfo->UpVector[0],    ToolInfo->UpVector[1],    ToolInfo->UpVector[2],
               ToolInfo->BackVector[0],  ToolInfo->BackVector[1],  ToolInfo->BackVector[2]);
    }
    else
    {
        printf("No Art tool information, GrannyComputeBasisConversion won't work on this file.\n");
    }

    if (GrannyFileInfo->ExporterInfo)
    {
        printf("File created with version %d.%d.%d.%d of Granny.\n",
               GrannyFileInfo->ExporterInfo->ExporterMajorRevision,
               GrannyFileInfo->ExporterInfo->ExporterMinorRevision,
               GrannyFileInfo->ExporterInfo->ExporterBuildNumber,
               GrannyFileInfo->ExporterInfo->ExporterCustomization);
    }

    if (GrannyFileInfo->FromFileName)
    {
        printf("Source file: %s\n", GrannyFileInfo->FromFileName);
    }

    printf("Number of    Textures: %d\n"
           "            Materials: %d\n"
           "               Meshes: %d\n"
           "               Models: %d\n"
           "           Animations: %d\n",
           GrannyFileInfo->TextureCount,
           GrannyFileInfo->MaterialCount,
           GrannyFileInfo->MeshCount,
           GrannyFileInfo->ModelCount,
           GrannyFileInfo->AnimationCount);
    printf("\n");
}
