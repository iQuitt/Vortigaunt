// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "preprocessor.h"
#include <stdio.h>
#include <string.h>

bool CombineObjectElement(granny_file_info* BaseObject,
                          granny_file_info** Secondaries,
                          granny_int32x SecondaryCount,
                          char const* ElementName,
                          granny_memory_arena* TempArena);


bool CombineElements(input_file*     InputFiles,
                     granny_int32x   NumInputFiles,
                     key_value_pair* KeyValues,
                     granny_int32x   NumKeyValues,
                     granny_memory_arena* TempArena)
{
    RequireKey("output", "must specify an output file with \"-output <filename>\"\n");

    if (NumInputFiles < 2)
    {
        ErrOut("only valid for more than one input file\n");
        return false;
    }

    granny_file_info* Info = ExtractFileInfo(InputFiles[0]);
    if (Info == 0)
    {
        ErrOut("unable to obtain a granny_file_info from the input file\n");
        return false;
    }

    granny_file_info** Secondaries = new granny_file_info*[NumInputFiles];

    {for(granny_int32x File = 1; File < NumInputFiles; ++File)
    {
        Secondaries[File-1] = ExtractFileInfo(InputFiles[File]);
        if (Secondaries[File-1] == 0)
        {
            ErrOut("unable to obtain a granny_file_info from: %s\n", InputFiles[File].Filename);
            return false;
        }
    }}

    {for(granny_int32x i = 0; i < NumKeyValues; ++i)
    {
        if (_stricmp(KeyValues[i].Key, "combine") == 0)
        {
            if (CombineObjectElement(Info, Secondaries,
                                     NumInputFiles - 1, KeyValues[i].Value,
                                     TempArena) == false)
            {
                ErrOut("failed to combine element: %s\n", KeyValues[i].Value);
                return false;
            }
        }
    }}

    delete [] Secondaries;

    // Write out the resulting file...
    return WriteInfoPreserve(KeyValues, NumKeyValues, Info, InputFiles[0].GrannyFile, true, true, true, TempArena);
}


// Handy little utilities for dealing with ArrayOfRef members
#if _MSC_VER
  #pragma warning(disable:4127)
#endif

#define AOR_ExtractCountAndPtr(Count, Pointer, Memory)          \
    do {                                                    \
        Count   = *((granny_int32*)(Memory));                   \
        Pointer = *((void**)((granny_uint8*)Memory + 4));       \
    } while (false)

#define AOR_SetCountAndPtr(Count, Pointer, Memory)          \
    do {                                                    \
        *((granny_int32*)(Memory)) = Count;                 \
        *((void**)((granny_uint8*)(Memory) + 4)) = Pointer; \
    } while (false)

bool CombineArrayOfRefMember(granny_file_info* BaseObject,
                             granny_file_info** Secondaries,
                             granny_int32x SecondaryCount,
                             granny_int32x DataOffset,
                             granny_memory_arena* TempArena)
{
    granny_int32 TotalObjects;
    void* Ignored;

    // Extract the current count from the BaseObject
    AOR_ExtractCountAndPtr(TotalObjects, Ignored,
                           ((granny_uint8*)BaseObject) + DataOffset);

    {for(granny_int32x Idx = 0; Idx < SecondaryCount; ++Idx)
    {
        granny_int32 Count;
        AOR_ExtractCountAndPtr(Count, Ignored,
                               ((granny_uint8*)(Secondaries[Idx])) + DataOffset);

        TotalObjects += Count;
    }}

    if (TotalObjects == 0)
    {
        // No objects, including in the base object, we're done...
        return true;
    }

    void** NewReferences = PushArray(TempArena, TotalObjects, void*);
    granny_int32x CurrentRef = 0;

    // Recopy the members from the base
    {
        granny_int32 Count;
        void* ArrayLocation;
        AOR_ExtractCountAndPtr(Count, ArrayLocation,
                               ((granny_uint8*)BaseObject) + DataOffset);

        void** Array = (void**)ArrayLocation;
        {for(granny_int32x Idx  = 0; Idx < Count; ++Idx)
        {
            NewReferences[CurrentRef++] = Array[Idx];
        }}
    }

    // And the new members from the secondaries...
    {for(granny_int32x Idx = 0; Idx < SecondaryCount; ++Idx)
    {
        granny_int32 Count;
        void* ArrayLocation;
        AOR_ExtractCountAndPtr(Count, ArrayLocation,
                               ((granny_uint8*)Secondaries[Idx]) + DataOffset);

        void** Array = (void**)ArrayLocation;
        {for(granny_int32x Idx  = 0; Idx < Count; ++Idx)
        {
            NewReferences[CurrentRef++] = Array[Idx];
        }}
    }}

    AOR_SetCountAndPtr(TotalObjects, NewReferences,
                       ((granny_uint8*)BaseObject) + DataOffset);

    return true;
}

bool CombineRefToVarArrayMember(granny_file_info* BaseObject,
                                granny_file_info** Secondaries,
                                granny_int32x SecondaryCount,
                                granny_int32x DataOffset,
                                granny_memory_arena* TempArena)
{
    // We don't actually handle these yet.  There aren't any in the
    // default granny_file_info structure.  Straightforward to add,
    // though it will be required that the variant types for all the
    // objects match.
    return false;
}


// Actually does the work of finding the specified element, and
// combining it.
bool CombineObjectElement(granny_file_info* BaseObject,
                          granny_file_info** Secondaries,
                          granny_int32x SecondaryCount,
                          char const* ElementName,
                          granny_memory_arena* TempArena)
{
    granny_int32x Offset = 0;

    granny_data_type_definition* CurrentType = GrannyFileInfoType;
    while (CurrentType->Type != GrannyEndMember)
    {
        if (_stricmp(CurrentType->Name, ElementName) == 0)
        {
            switch (CurrentType->Type)
            {
                case GrannyArrayOfReferencesMember:
                {
                    return CombineArrayOfRefMember(BaseObject,
                                                   Secondaries, SecondaryCount,
                                                   Offset, TempArena);
                } break;

                case GrannyReferenceToVariantArrayMember:
                {
                    return CombineRefToVarArrayMember(BaseObject,
                                                      Secondaries, SecondaryCount,
                                                      Offset, TempArena);
                } break;

                // We can't remove members of these types
                case GrannyReferenceToArrayMember:
                case GrannyReferenceMember:
                case GrannyStringMember:
                case GrannyVariantReferenceMember:
                case GrannyInlineMember:
                case GrannyUnsupportedMemberType_Remove:
                case GrannyTransformMember:
                case GrannyReal32Member:
                case GrannyInt8Member:
                case GrannyUInt8Member:
                case GrannyBinormalInt8Member:
                case GrannyNormalUInt8Member:
                case GrannyInt16Member:
                case GrannyUInt16Member:
                case GrannyBinormalInt16Member:
                case GrannyNormalUInt16Member:
                case GrannyInt32Member:
                case GrannyUInt32Member:
                    return false;

                    // Bad Type value...
                default:
                    pp_assert(false);
                    return false;
            }
        }

        // Advance in the object, and the type array
        Offset += GrannyGetMemberTypeSize(CurrentType);
        ++CurrentType;
    }

    return false;
}

static char const* HelpString =
    (" CombineElements is an all-purpose file combiner.  Using the first file\n"
     " specified, it will create an output file that represents that first\n"
     " file, plus a specified set of elements from subsequent files.\n"
     " The elements that may be combined are any of the array elements at\n"
     " the top level of granny_file_info.  (e.g. Textures, Animations, etc.)\n"
     "\n"
     " A common and handy way to use this command is to create a combined\n"
     " character gr2 from a mesh/model plus a list of animations.\n"
     "\n"
     "    preprocessor CombineElements model.gr2 anim0.gr2 anim1.gr2 \\\n"
     "                 -combine Animations\n");



static CommandRegistrar RegCombineElements(CombineElements, "CombineElements",
                                           "Combines root level elements of the granny_file_info",
                                           HelpString);

