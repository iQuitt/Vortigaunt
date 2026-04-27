// ========================================================================
// $File: //jeffr/granny_29/preprocessor/samples/write_raw_file.cpp $
// $DateTime: 2013/05/07 17:02:03 $
// $Change: 42946 $
// $Revision: #3 $
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <vector>
#include <algorithm>

using namespace std;

bool SwapRawEndianness(char const* File);


void MakeFakeInfo(granny_file_info* Info)
{
    Info->ExtendedData = Info->Skeletons[0]->Bones[0].ExtendedData;
    // Info->ArtToolInfo      = 0;
    Info->ExporterInfo     = 0;
    Info->FromFileName     = 0;
    Info->TextureCount     = 0;
    Info->Textures         = 0;
    Info->MaterialCount    = 0;
    Info->Materials        = 0;
    Info->SkeletonCount    = 0;
    Info->Skeletons        = 0;
    Info->VertexDataCount  = 0;
    Info->VertexDatas      = 0;
    Info->TriTopologyCount = 0;
    Info->TriTopologies    = 0;
    Info->MeshCount        = 0;
    Info->Meshes           = 0;
    Info->ModelCount       = 0;
    Info->Models           = 0;
    Info->TrackGroupCount  = 0;
    Info->TrackGroups      = 0;
    Info->AnimationCount   = 0;
    Info->Animations       = 0;
}

#include <windows.h>
struct NullTracking
{
	vector<granny_uint32>* ModifiedLocations;
	char*                  BaseAddress;
};

void NoteNull(void* Data, void* loc)
{
	NullTracking* Tracking = (NullTracking*)Data;
	granny_intaddrx Delta = (char*)loc - Tracking->BaseAddress;
	assert(Delta >= 0);

	// Just to be sure.  The tree-walking code should not visit
	// locations multiple times, but we'll guard against that
	// in debug mode.
	assert(find(Tracking->ModifiedLocations->begin(),
		        Tracking->ModifiedLocations->end(),
		        Delta) ==
		   Tracking->ModifiedLocations->end());
	Tracking->ModifiedLocations->push_back(granny_uint32(Delta));
}


bool WriteRawFileSidecar(input_file&     InputFile,
                         key_value_pair* KeyValues,
                         granny_int32x   NumKeyValues,
                         granny_memory_arena* Arena)
{
    bool success = true;

    char const* OutputFile = FindFirstValueForKey(KeyValues, NumKeyValues, "output");
    if (OutputFile == 0)
    {
        ErrOut("must specify an output file with \"-output <filename>\"\n");
        return false;
    }

    const char * RawFilename = NULL;

    char const* DebugOutputFile = FindFirstValueForKey(KeyValues, NumKeyValues, "debug_output");
    if (DebugOutputFile != 0)
    {
        granny_file_info* Info = ExtractFileInfo(InputFile);
        if (Info == 0)
        {
            ErrOut("unable to obtain a granny_file_info from the input file\n");
            return false;
        }

        if (memcmp(GrannyGRNFileMV_ThisPlatform, GrannyGRNFileMV_32Bit_LittleEndian, sizeof(granny_grn_file_magic_value)) != 0)
        {
            ErrOut("can only do this from 32-bit le for now\n");
            return false;
        }
        
        // Write out the resulting database always as 32/le...
        granny_file_builder* Builder = GrannyBeginFile(1,
                                                       GrannyCurrentGRNStandardTag,
                                                       GrannyGRNFileMV_ThisPlatform,
                                                       GrannyGetTemporaryDirectory(),
                                                       "Prefix");
        granny_file_data_tree_writer* Writer =
            GrannyBeginFileDataTreeWriting(GrannyFileInfoType, Info, 0, 0);
        GrannySetFileDataTreeFlags(Writer, GrannyExcludeTypeTree);
        GrannyWriteDataTreeToFileBuilder(Writer, Builder);
        GrannyEndFileDataTreeWriting(Writer);

        success = GrannyEndFileRaw(Builder, DebugOutputFile);
        RawFilename = DebugOutputFile;
    }
    else
    {
        RawFilename = InputFile.Filename;
    }


    // Ok, we need to determine where the fixup locations are.  They will always be at 4byte
    // boundaries, and they'll be touched by RebasePointers, so we can find them all very easily.
    vector<granny_uint8> RawFile;
    {
        FILE* f = fopen(RawFilename, "rb");
        if (!f)
        {
            ErrOut("unable to open raw file after writing\n");
            return false;
        }

        fseek(f, 0, SEEK_END);
        size_t RawSize = ftell(f);
        fseek(f, 0, SEEK_SET);

        RawFile.resize(RawSize);
        fread(&RawFile[0], RawSize, 1, f);
        fclose(f);
    }

    // Pristine copy
    vector<granny_uint8> RawCopy = RawFile;

    // Rebase the pointers
    vector<granny_uint32> ModifiedLocations;
	NullTracking tracking;
	tracking.ModifiedLocations = &ModifiedLocations;
	tracking.BaseAddress = (char*)&RawFile[0];
	GrannyRebasePointersTrackNulls(GrannyFileInfoType,
                         		   &RawFile[0],
                         		   (granny_intaddrx)&RawFile[0],
                         		   true, NoteNull, &tracking);

    granny_uint32 const* ModAsInt = (granny_uint32 const*)&RawFile[0];
    granny_uint32 const* OldAsInt = (granny_uint32 const*)&RawCopy[0];
    for (size_t Idx = 0; Idx < RawFile.size()/4; ++Idx)
    {
        if (ModAsInt[Idx] != OldAsInt[Idx])
        {
            // Store as byte locations for the moment.
            ModifiedLocations.push_back(granny_uint32(Idx*4));
        }
    }

	// Nulls are in first, sort the whole she-bang
	sort(ModifiedLocations.begin(), ModifiedLocations.end());

	// deltas compress better, but leave that out for now.
#if 0
	for (size_t Idx = ModifiedLocations.size()-1; Idx > 0; --Idx)
	{
		ModifiedLocations[Idx] -= ModifiedLocations[Idx-1];
	}
#endif

    // And dump out the modified point locations.
    {
        FILE* ModPoints = fopen(OutputFile, "wb");
        if (!ModPoints)
        {
            ErrOut("Unable to open '%s' for writing", OutputFile);
            return false;
        }

        fwrite(&ModifiedLocations[0], sizeof(granny_uint32)*ModifiedLocations.size(), 1, ModPoints);
        fclose(ModPoints);
    }

    return success;
}

static char const* HelpString =
    (" Reads a 32 bit little endian raw granny file and generates a pointer \n"
     " fixup table for load-time converting it to 64 bit.\n"
     "\n"
     " Call WriteRawFileSidecar input.dat -output file.pnt to write the .pnt\n"
     " pointer fixup file for your raw file\n"
     "\n"
     " You can optionally specify '-debug_output file', then it'll expect\n"
     " a gr2 as input instead, and write a raw file for that using\n"
     " -output filename.dat, then read it back and generate a pnt file for\n"
     " testing.\n"
     "\n"
     "    preprocessor WriteRawFileSidecar file.dat -output file.pnt\n"
     "    preprocessor WriteRawFileSidecar file.gr2 -debug_output file.dat -output file.pnt\n"
     );

static CommandRegistrar RegWriteRawFileSidecar(eSingleFile_NoMultiRun,
                                               WriteRawFileSidecar,
                                               "WriteRawFileSidecar",
                                               "Write a raw file with sidecar data for pointer size correction",
                                               HelpString);
