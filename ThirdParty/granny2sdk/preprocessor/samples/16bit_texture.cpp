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
#include <memory.h>

enum ECompressedTextureType
{
    eHalfFloat,
    eZeroToOne,
    eNegOneToOne
};

static char const* CompTextureSuffix = ".CompTexture";

// This function scans through the type array looking for texture members and replaces
// them in the new array with a marker for the new compressed member.
static granny_data_type_definition*
CreateCompressedType(granny_data_type_definition* VertexType,
                     granny_memory_arena*         TempArena,
                     ECompressedTextureType       CompressionType)
{
    pp_assert(VertexType);
    pp_assert(TempArena);

    granny_int32x const TypeSize  = GrannyGetTotalTypeSize(VertexType);
    granny_int32x const TypeCount = TypeSize / sizeof(granny_data_type_definition);
    pp_assert(TypeSize != 0 && (TypeSize % sizeof(granny_data_type_definition)) == 0);

    granny_data_type_definition* NewType =
        PushArray(TempArena, TypeCount, granny_data_type_definition);
    memset(NewType, 0, TypeSize);

    {for(int Idx = 0; Idx < (TypeCount-1); ++Idx)
    {
        pp_assert(VertexType[Idx].Type != GrannyEndMember);
        pp_assert(VertexType[Idx].Name != 0);

        if (_strnicmp(VertexType[Idx].Name,
                      GrannyVertexTextureCoordinatesName,
                      strlen(GrannyVertexTextureCoordinatesName)) == 0)
        {
            pp_assert(VertexType[Idx].Type == GrannyReal32Member);

            // compressed texture
            switch (CompressionType)
            {
                case eHalfFloat:   NewType[Idx].Type = GrannyReal16Member; break;
                case eZeroToOne:   NewType[Idx].Type = GrannyUInt16Member; break;
                case eNegOneToOne: NewType[Idx].Type = GrannyUInt16Member; break;
            }

            NewType[Idx].ArrayWidth = VertexType[Idx].ArrayWidth;
            NewType[Idx].Name = PushArray(TempArena,
                                          int(strlen(VertexType[Idx].Name) +
                                              strlen(CompTextureSuffix) + 1),
                                          char);

            strcpy((char*)NewType[Idx].Name, VertexType[Idx].Name);
            strcat((char*)NewType[Idx].Name, CompTextureSuffix);
        }
        else
        {
            // uncompressed component
            NewType[Idx] = VertexType[Idx];
        }
    }}

    NewType[TypeCount-1].Type = GrannyEndMember;

    return NewType;
}


// Step through the vertex arrays, copying uncompressed texture coordinates to the new
// compressed location
static bool
CompressTexCoord(int VertexCount, int NumComponents,
                 void* Vertices, int OffsetInOrig, int StrideOrig,
                 void* NewVertices, int OffsetInComp, int StrideComp,
                 ECompressedTextureType CompressionType)
{
    // Only 2 or 4 for now to maintain DWORD alignment..
    pp_assert(NumComponents == 2 || NumComponents == 4);

    granny_real32* Original   = (granny_real32*)((granny_uint8*)Vertices + OffsetInOrig);
    granny_real16* Compressed = (granny_real16*)((granny_uint8*)NewVertices + OffsetInComp);

    bool AllInside = true;
    {for(int Idx = 0; Idx < VertexCount; ++Idx)
    {
        {for(int j = 0; j < NumComponents; ++j)
        {
            switch (CompressionType)
            {
                case eHalfFloat:
                {
                    granny_real16* Comp = (granny_real16*)Compressed;
                    Comp[j] = GrannyReal32ToReal16(Original[j]);
                } break;

                case eZeroToOne:
                {
                    granny_uint16* Comp = (granny_real16*)Compressed;
                    if (Original[j] < 0.0f || Original[j] > 1.0f)
                        AllInside = false;

                    Comp[j] = (granny_uint16)(Original[j] * 65535.0f + 0.5f);
                } break;

                case eNegOneToOne:
                {
                    granny_uint16* Comp = (granny_real16*)Compressed;
                    if (Original[j] < -1.0f || Original[j] > 1.0f)
                        AllInside = false;

                    Comp[j] = (granny_uint16)(((Original[j] + 1)/2.0f) * 65535.0f + 0.5f);
                } break;
            }
        }}

        Original   = (granny_real32*)((granny_uint8*)Original +   StrideOrig);
        Compressed = (granny_real16*)((granny_uint8*)Compressed + StrideComp);
    }}

    return AllInside;
}


// Loop through the old and new type.  When a compressed type is encountered, translate
// the data from the old to the new array.  We've marked the compressed members with
// distinct names in this sample, but they could be discovered by examining the old and
// new granny_member_type values as well.
static bool
CopyCompressedMembers(int VertexCount,
                      granny_uint8* Vertices,    granny_data_type_definition* Type,
                      granny_uint8* NewVertices, granny_data_type_definition* NewType,
                      ECompressedTextureType CompressionType)
{
    int OldVertexSize = GrannyGetTotalObjectSize(Type);
    int NewVertexSize = GrannyGetTotalObjectSize(NewType);

    bool AllInside = true;

    granny_int32x OffsetInOrig = 0;
    granny_int32x OffsetInComp = 0;
    {for(int Idx = 0; Type[Idx].Type != GrannyEndMember; ++Idx)
    {
        pp_assert(NewType[Idx].Type != GrannyEndMember);

        if (strstr(NewType[Idx].Name, CompTextureSuffix))
        {
            if (!CompressTexCoord(VertexCount, NewType[Idx].ArrayWidth,
                                  Vertices, OffsetInOrig, OldVertexSize,
                                  NewVertices, OffsetInComp, NewVertexSize,
                                  CompressionType))
            {
                AllInside = false;
            }
        }

        OffsetInOrig += GrannyGetMemberTypeSize(&Type[Idx]);
        OffsetInComp += GrannyGetMemberTypeSize(&NewType[Idx]);
    }}

    return AllInside;
}

// This is the command called by the preprocessor.  The driver iterates through the
// meshes, computing a new compressed format type for each mesh, and replaces the vertex
// array with the new version.
granny_file_info*
Create16BitTextureCoords(char const*          OriginalFilename,
                         char const*          OutputFilename,
                         granny_file_info*    Info,
                         key_value_pair*      KeyValues,
                         granny_int32x        NumKeyValues,
                         granny_memory_arena* TempArena)
{
    // Validate the input.  Our requirements for this command:
    //  -output key must be present
    //  The input $file must contain a valid $file_info object
    RequireKey("type",   "must specify coordinate type with \"-type [HalfFloat|ZeroToOne|NegOneToOne\"\n");

    char const* CompType = FindFirstValueForKey(KeyValues, NumKeyValues, "type");
    pp_assert(CompType);

    ECompressedTextureType CompressionType;
    if (_stricmp(CompType, "HalfFloat") == 0)
        CompressionType = eHalfFloat;
    else if (_stricmp(CompType, "ZeroToOne") == 0)
        CompressionType = eZeroToOne;
    else if (_stricmp(CompType, "NegOneToOne") == 0)
        CompressionType = eNegOneToOne;
    else
    {
        ErrOut("unable to determine the texture coordinate type, use HalfFloat, ZeroToOne, or NegOneToOne\n");
        return 0;
    }

    for (int i = 0; i < Info->MeshCount; i++)
    {
        granny_mesh* Mesh = Info->Meshes[i];
        pp_assert(Mesh);

        // Assume there are no morph targets.  We can modify this easily to support them
        // if required.
        if (GrannyGetMeshMorphTargetCount(Mesh) != 0)
        {
            ErrOut("mesh %s contains morph targets, failure!\n", Mesh->Name);
            return false;
        }

        // Make a modified copy of the vertex type array so we can merge unaltered data
        // from the uncompressed vertices.
        granny_data_type_definition* VertexType = GrannyGetMeshVertexType(Mesh);
        pp_assert(VertexType);

        void* Vertices = GrannyGetMeshVertices(Mesh);

        granny_data_type_definition* CompressedType =
            CreateCompressedType(VertexType, TempArena, CompressionType);

        granny_int32x const VertexCount   = GrannyGetMeshVertexCount(Mesh);
        granny_int32x const NewVertexSize = GrannyGetTotalObjectSize(CompressedType);

        granny_uint8* NewVertices =
            PushArray(TempArena, (VertexCount * NewVertexSize), granny_uint8);

        // Since we've changed the name of our compressed members, this will copy over the
        // unchanged data, and zero-fill the new members.
        GrannyConvertVertexLayouts(VertexCount, VertexType, Vertices,
                                   CompressedType, NewVertices);

        // Ok, now we actually need to do the compression.  This will leave the unchanged
        // members alone, and modify only the compressed entries.
        if (!CopyCompressedMembers(VertexCount,
                                   (granny_uint8*)Vertices, VertexType,
                                   (granny_uint8*)NewVertices, CompressedType,
                                   CompressionType))
        {
            WarnOut("Warning, at least one texture coordinate in mesh: \"%s\" was out of bounds\n",
                    Mesh->Name);
        }

        // Replace the vertex data with our new arrays...
        Mesh->PrimaryVertexData->VertexType = CompressedType;
        Mesh->PrimaryVertexData->Vertices   = (granny_uint8*)NewVertices;

        // We can leave the annotation and the VertexCount fields
        // alone, they don't change.
    }

    return Info;
}

static CommandRegistrar RegCreate16BitTextureCoords(Create16BitTextureCoords, "Create16BitTexCoords",
                                                    "Creates 16-bit texture coordinates");

