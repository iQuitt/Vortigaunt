// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"
#include <stdio.h>
#include <string.h>
#include <vector>

using namespace std;

bool SwapRawEndianness(char const* File);

bool WriteRawFile(input_file&     InputFile,
                  key_value_pair* KeyValues,
                  granny_int32x   NumKeyValues,
                  granny_memory_arena* Arena)
{
    char const* OutputFile = FindFirstValueForKey(KeyValues, NumKeyValues, "output");
    if (OutputFile == 0)
    {
        ErrOut("must specify an output file with \"-output <filename>\"\n");
        return false;
    }

    bool LittleEndian = true;
    char const* EndianValue = FindFirstValueForKey(KeyValues, NumKeyValues, "endian");
    if (EndianValue != 0)
    {
        if (_stricmp(EndianValue, "big") == 0)
            LittleEndian = false;
        else if (_stricmp(EndianValue, "little") == 0)
            LittleEndian = true;
        else
        {
            ErrOut("unable to parse endian key, must be \"big\" or \"little\"\n");
            return false;
        }
    }

    granny_file_info* Info = ExtractFileInfo(InputFile);
    if (Info == 0)
    {
        ErrOut("unable to obtain a granny_file_info from the input file\n");
        return false;
    }

    // Write out the resulting database...
    granny_file_builder *Builder = GrannyBeginFile(1,
                                                   GrannyCurrentGRNStandardTag,
                                                   GrannyGRNFileMV_ThisPlatform,
                                                   GrannyGetTemporaryDirectory(),
                                                   "Prefix");
    granny_file_data_tree_writer *Writer =
        GrannyBeginFileDataTreeWriting(GrannyFileInfoType, Info, 0, 0);
    GrannySetFileDataTreeFlags(Writer, GrannyExcludeTypeTree);
    GrannyWriteDataTreeToFileBuilder(Writer, Builder);
    GrannyEndFileDataTreeWriting(Writer);

    bool success = GrannyEndFileRaw(Builder, OutputFile);

    if (success && !LittleEndian)
    {
        success = SwapRawEndianness(OutputFile);
    }

    if (!success)
    {
        ErrOut("failed to write raw file to %s\n", OutputFile);
    }

    return success;
}

inline granny_uint32
Swap32(granny_uint32 Value)
{
    return (((Value >> 24) & 0x000000ff) |
            ((Value >> 8)  & 0x0000ff00) |
            ((Value << 8)  & 0x00ff0000) |
            ((Value << 24) & 0xff000000));
}

inline granny_uint16
Swap16(granny_uint16 Value)
{
    return (((Value >> 8) & 0x00ff) |
            ((Value << 8) & 0xff00));
}

struct FlipContext
{
    granny_intaddrx BufferSize;
    granny_uint8*   LittleBuffer;
    granny_uint8*   BigBuffer;

    granny_pointer_hash *ObjectHash;
    granny_pointer_hash *TypeHash;

    FlipContext(granny_intaddrx Size, granny_uint8* Little, granny_uint8* Big)
      : BufferSize(Size),
        LittleBuffer(Little),
        BigBuffer(Big),
        ObjectHash(GrannyNewPointerHash()),
        TypeHash(GrannyNewPointerHash())
    {
        pp_assert(LittleBuffer);
        pp_assert(BigBuffer);
        pp_assert(ObjectHash);
        pp_assert(TypeHash);
    }

    ~FlipContext()
    {
        GrannyDeletePointerHash(ObjectHash);
        GrannyDeletePointerHash(TypeHash);

        BufferSize = 0;
        LittleBuffer = 0;
        BigBuffer = 0;
        ObjectHash = 0;
        TypeHash = 0;
    }
};

void FlipLocation16(granny_uint8* Location,
                    FlipContext& Context,
                    int NumEntries = 1)
{
    granny_intaddrx Offset = Location - Context.LittleBuffer;
    pp_assert(Offset >= 0 && Offset < Context.BufferSize);

    granny_uint16* BigLocation = (granny_uint16*)(Context.BigBuffer + Offset);
    for (int i = 0; i < NumEntries; ++i)
        BigLocation[i] = Swap16(BigLocation[i]);
}

void FlipLocation32(granny_uint8* Location,
                    FlipContext& Context,
                    int NumEntries = 1)
{
    granny_intaddrx Offset = Location - Context.LittleBuffer;
    pp_assert(Offset >= 0 && Offset < Context.BufferSize);

    granny_uint32* BigLocation = (granny_uint32*)(Context.BigBuffer + Offset);
    for (int i = 0; i < NumEntries; ++i)
        BigLocation[i] = Swap32(BigLocation[i]);
}

void FlipType(granny_data_type_definition* InputType,
              FlipContext& Context)
{
    if (!InputType || GrannyHashedPointerKeyExists(Context.TypeHash, InputType))
        return;

    GrannyAddPointerToHash(Context.TypeHash, InputType, 0);
    pp_assert(sizeof(InputType->Type) == 4);

    // In a type, we need to reverse just a couple of offsets.
    //  Type
    //  ArrayWidth
    //  Extra[3]
    while (InputType->Type != GrannyEndMember)
    {
        FlipLocation32((granny_uint8*)&InputType->Type, Context);
        FlipLocation32((granny_uint8*)&InputType->ArrayWidth, Context);
        FlipLocation32((granny_uint8*)&InputType->Extra[0], Context);
        FlipLocation32((granny_uint8*)&InputType->Extra[1], Context);
        FlipLocation32((granny_uint8*)&InputType->Extra[2], Context);

        if (InputType->ReferenceType)
        {
            granny_intaddrx Offset = ((granny_uint8*)InputType->ReferenceType) - Context.LittleBuffer;
            pp_assert(Offset >= 0 && Offset < Context.BufferSize);
			pp_unused(Offset);

            FlipType(InputType->ReferenceType, Context);
        }

        ++InputType;
    }

    // Make sure to get the end member too.  Only need to flip the type for that one...
    pp_assert(InputType->Type == GrannyEndMember);
    FlipLocation32((granny_uint8*)&InputType->Type, Context);
}

void TraverseForFlip(granny_uint8* InputObject,
                     granny_data_type_definition* InputType,
                     FlipContext& Context,
                     bool IgnoreHashing = false)
{
    if (!InputObject || !InputType)
        return;

    if (IgnoreHashing == false)
    {
        if (GrannyHashedPointerKeyExists(Context.ObjectHash, InputObject))
            return;
        else
            GrannyAddPointerToHash(Context.ObjectHash, InputObject, 0);
    }

    granny_uint8 *ObjectPtr = (granny_uint8 *)InputObject;
    {for(granny_data_type_definition const *WalkType = InputType;
         WalkType && WalkType->Type != GrannyEndMember;
         ObjectPtr += GrannyGetMemberTypeSize(WalkType), ++WalkType)
    {
        switch(WalkType->Type)
        {
            case GrannyReferenceMember:
            {
                if (*(granny_uint8 **)ObjectPtr)
                    TraverseForFlip(*(granny_uint8 **)ObjectPtr, WalkType->ReferenceType, Context);
            } break;

            case GrannyReferenceToArrayMember:
            {
                int ArrayCount = *(granny_int32 *)ObjectPtr;
                FlipLocation32(ObjectPtr, Context);

                int ObjectSize = GrannyGetTotalObjectSize(WalkType->ReferenceType);
                granny_uint8 *WalkObject =
                    *(granny_uint8 **)(ObjectPtr + sizeof(granny_int32));

                for (int i = 0; i < ArrayCount; ++i)
                {
                    TraverseForFlip(WalkObject, WalkType->ReferenceType, Context, true);
                    WalkObject += ObjectSize;
                }
            } break;

            case GrannyArrayOfReferencesMember:
            {
                int ArrayCount = *(granny_int32 *)ObjectPtr;
                FlipLocation32(ObjectPtr, Context);

                granny_uint8 **WalkObjects =
                    *(granny_uint8 ***)(ObjectPtr + sizeof(granny_int32));

                if (WalkObjects)
                {
                    {for(int ArrayIndex = 0;
                         ArrayIndex < ArrayCount;
                         ++ArrayIndex)
                    {
                        if(*WalkObjects)
                            TraverseForFlip(*WalkObjects, WalkType->ReferenceType, Context);

                        ++WalkObjects;
                    }}
                }
            } break;

            case GrannyVariantReferenceMember:
            {
                granny_data_type_definition *VariantType = *(granny_data_type_definition **)ObjectPtr;
                granny_uint8 *VarObject = *(granny_uint8 **)(ObjectPtr + sizeof(granny_data_type_definition *));
                if (VarObject)
                {
//                  if (stricmp(VariantType[0].Name, "CurveDataHeader_D3I1K16uC16u") == 0)
//                      __asm int 3;
                    FlipType(VariantType, Context);
                    TraverseForFlip(VarObject, VariantType, Context);
                }
            } break;

            case GrannyReferenceToVariantArrayMember:
            {
                granny_data_type_definition *VariantType = *(granny_data_type_definition **)ObjectPtr;
                if(VariantType)
                {
                    FlipType(VariantType, Context);
                }

                int Count = *(int *)(ObjectPtr + sizeof(granny_data_type_definition *));
                FlipLocation32(ObjectPtr + sizeof(granny_data_type_definition *), Context);

                granny_uint8 *WalkObject = *(granny_uint8 **)(ObjectPtr + sizeof(granny_data_type_definition *) + sizeof(int));
                int ObjectSize = GrannyGetTotalObjectSize(VariantType);
                for (int i = 0; i < Count; ++i)
                {
                    TraverseForFlip(WalkObject, VariantType, Context);
                    WalkObject += ObjectSize;
                }
            } break;

            case GrannyInlineMember:
            {
                TraverseForFlip(ObjectPtr, WalkType->ReferenceType, Context, true);
            } break;

            case GrannyTransformMember:
            {
                FlipLocation32(ObjectPtr, Context, (sizeof(granny_transform) / 4));
            } break;

            case GrannyReal16Member:
            case GrannyInt16Member:
            case GrannyBinormalInt16Member:
            case GrannyUInt16Member:
            case GrannyNormalUInt16Member:
            {
                FlipLocation16(ObjectPtr, Context, WalkType->ArrayWidth == 0 ? 1 : WalkType->ArrayWidth);
            } break;

            case GrannyReal32Member:
            case GrannyUInt32Member:
            case GrannyInt32Member:
            {
                FlipLocation32(ObjectPtr, Context, WalkType->ArrayWidth == 0 ? 1 : WalkType->ArrayWidth);
            } break;

            case GrannyStringMember:
            case GrannyUInt8Member:
            case GrannyNormalUInt8Member:
            case GrannyInt8Member:
            case GrannyBinormalInt8Member:
                break;

            /* Catch any bogus values in the Type field.  We should never actually execute
               this code.
            */
            default:
            {
pp_assert(false);
                return;
            } break;
        }
    }}
}


bool SwapRawEndianness(char const* Filename)
{
    // Read in the file
    vector<granny_uint8> LittleBuffer;
    size_t FileSize = 0;
    {
        int const ChunkSize = 64 << 10;
        FILE* f = fopen(Filename, "rb");
        if (!f)
            return false;

        for(;;)
        {
            LittleBuffer.resize(FileSize + ChunkSize);
            size_t ThisSize = fread(&LittleBuffer[FileSize], 1, ChunkSize, f);
            FileSize += ThisSize;
            if ((int)ThisSize != ChunkSize)
                break;
        }
        LittleBuffer.resize(FileSize);

        fclose(f);
    }
    pp_assert((FileSize % 4) == 0);

    // Create a copy that will hold the swapped file
    vector<granny_uint8> BigBuffer = LittleBuffer;

    // Rebase the pointers in the little-endian copy...
    GrannyRebasePointers(GrannyFileInfoType,
                         &LittleBuffer[0], (granny_intaddrx)&LittleBuffer[0],
                         true);

    // Ok.  First, let's go swap the offsets in the big buffer that correspond to pointer
    // fixups.  We'll find these by looking for differences between the two buffers.
    {
        granny_uint32* Little = (granny_uint32*)&LittleBuffer[0];
        granny_uint32* Big    = (granny_uint32*)&BigBuffer[0];

        for (size_t i = 0; i < FileSize/4; ++i)
        {
            if (Little[i] != Big[i])
                Big[i] = Swap32(Big[i]);
        }
    }

    // Now we need to traverse the file and reverse the actual data.  We're only concerned
    // with int32/int16/real32 members
    {
        FlipContext Context(FileSize, &LittleBuffer[0], &BigBuffer[0]);
        TraverseForFlip(&LittleBuffer[0], GrannyFileInfoType, Context);
    }

    // Write out the new big buffer
    bool success;
    {
        FILE* f = fopen(Filename, "wb");
        if (!f)
            return false;

        success = fwrite(&BigBuffer[0], 1, FileSize, f) == FileSize;
        fclose(f);
    }

    return success;
}

static CommandRegistrar RegWriteRawFile(eSingleFile_NoMultiRun,
                                        WriteRawFile,
                                        "WriteRawFile",
                                        "Write a raw file (possibly in big endian form)");
