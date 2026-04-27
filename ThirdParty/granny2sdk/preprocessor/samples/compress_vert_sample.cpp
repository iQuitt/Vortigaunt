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
#include <stdlib.h>
#include <memory.h>

static char const* CompNormalSuffix  = ".CompNormal";
static char const* CompTextureSuffix = ".CompTexture";
static char const* CompColorSuffix   = ".CompColor";

static granny_real32
Clamp(granny_real32 Min, granny_real32 Val, granny_real32 Max)
{
    if (Val <= Min)
        return Min;
    else if (Val >= Max)
        return Max;

    return Val;
}


// This function scans through the type array looking for normal members (Normal, Tangent,
// Binormal), and replaces them in the new array with a marker for the new compressed
// member.  Similarly, it will replace any TextureCoordinate* members with compressed
// 16bit float members.
static granny_data_type_definition*
CreateCompressedType(bool CompTex, bool CompNorm, bool CompColor,
                     granny_data_type_definition* VertexType,
                     granny_memory_arena*         TempArena)
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

        if (CompNorm &&
            (_stricmp(VertexType[Idx].Name, GrannyVertexNormalName) == 0 ||
             _stricmp(VertexType[Idx].Name, GrannyVertexTangentName) == 0 ||
             _stricmp(VertexType[Idx].Name, GrannyVertexBinormalName) == 0))
        {
            pp_assert(VertexType[Idx].Type == GrannyReal32Member);
            pp_assert(VertexType[Idx].ArrayWidth == 3);

            // compressed normal
            NewType[Idx].Type = GrannyUInt32Member;
            NewType[Idx].Name = PushArray(TempArena,
                                          int(strlen(VertexType[Idx].Name) +
                                              strlen(CompNormalSuffix) + 1),
                                          char);
            strcpy((char*)NewType[Idx].Name, VertexType[Idx].Name);
            strcat((char*)NewType[Idx].Name, CompNormalSuffix);
        }
        else if (CompTex &&
                 (_strnicmp(VertexType[Idx].Name,
                            GrannyVertexTextureCoordinatesName,
                            strlen(GrannyVertexTextureCoordinatesName))) == 0)
        {
            pp_assert(VertexType[Idx].Type == GrannyReal32Member);

            // compressed normal
            NewType[Idx].Type       = GrannyReal16Member;
            NewType[Idx].ArrayWidth = VertexType[Idx].ArrayWidth;

            NewType[Idx].Name = PushArray(TempArena,
                                          int(strlen(VertexType[Idx].Name) +
                                              strlen(CompTextureSuffix) + 1),
                                          char);

            strcpy((char*)NewType[Idx].Name, VertexType[Idx].Name);
            strcat((char*)NewType[Idx].Name, CompTextureSuffix);
        }
        else if (CompColor &&
                 (_strnicmp(VertexType[Idx].Name,
                            GrannyVertexDiffuseColorName,
                            strlen(GrannyVertexDiffuseColorName)) == 0 ||
                  _strnicmp(VertexType[Idx].Name,
                            GrannyVertexSpecularColorName,
                            strlen(GrannyVertexSpecularColorName)) == 0))
        {
            pp_assert(VertexType[Idx].Type == GrannyReal32Member);

            // compressed normal
            NewType[Idx].Type       = GrannyUInt32Member;
            NewType[Idx].ArrayWidth = 0;

            NewType[Idx].Name = PushArray(TempArena,
                                          int(strlen(VertexType[Idx].Name) +
                                              strlen(CompColorSuffix) + 1),
                                          char);

            strcpy((char*)NewType[Idx].Name, VertexType[Idx].Name);
            strcat((char*)NewType[Idx].Name, CompColorSuffix);
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


// Step through the vertex arrays, copying uncompressed normals to the new compressed
// location
static void
CompressNormals(int VertexCount,
                void* Vertices, int OffsetInOrig, int StrideOrig,
                void* NewVertices, int OffsetInComp, int StrideComp)
{
    granny_uint8* Original   = (granny_uint8*)Vertices + OffsetInOrig;
    granny_uint8* Compressed = (granny_uint8*)NewVertices + OffsetInComp;

    {for(int Idx = 0; Idx < VertexCount; ++Idx)
    {
        float* Norm = (float*)Original;
        granny_uint32& Comp = *((granny_uint32*)Compressed);

        pp_assert(Norm[0] >= -1.0f && Norm[0] <= 1.0f);
        pp_assert(Norm[1] >= -1.0f && Norm[1] <= 1.0f);
        pp_assert(Norm[2] >= -1.0f && Norm[2] <= 1.0f);

        // D3DDECLTYPE_DHEN3N
        //
        // Normalized, 3D signed 10 11 11 format expanded to (value/511.0, value/1023.0,
        // value/1023.0, 1). The Packed 10:11:11 Format section describes how the bits are
        // packed before they are expanded.
        //
        // To generate a packed 32-bit value that can be used with type such as
        // D3DDECLTYPE_UDHEN3, use the following macro:
#define MakePacked_10_11_11(x,y,z) ((x) + ((y)<<10) + ((z)<<21))

        Comp = MakePacked_10_11_11(int(Norm[0] * 511.0f  + 511.0f),
                                   int(Norm[1] * 1023.0f + 1023.0f),
                                   int(Norm[2] * 1023.0f + 1023.0f));

#undef MakePacked_10_11_11

        Original   += StrideOrig;
        Compressed += StrideComp;
    }}
}


// Step through the vertex arrays, copying uncompressed texture coordinates to the new
// compressed location
static void
CompressTexCoord(int VertexCount, int NumComponents,
                 void* Vertices, int OffsetInOrig, int StrideOrig,
                 void* NewVertices, int OffsetInComp, int StrideComp)
{
    // Only 2 or 4 for now to maintain DWORD alignment..
    pp_assert(NumComponents == 2 || NumComponents == 4);

    granny_real32* Original   = (granny_real32*)((granny_uint8*)Vertices + OffsetInOrig);
    granny_real16* Compressed = (granny_real16*)((granny_uint8*)NewVertices + OffsetInComp);

    {for(int Idx = 0; Idx < VertexCount; ++Idx)
    {
        granny_real16* Comp = (granny_real16*)Compressed;

        {for(int j = 0; j < NumComponents; ++j)
        {
            Comp[j] = GrannyReal32ToReal16(Original[j]);
        }}

        Original   = (granny_real32*)((granny_uint8*)Original + StrideOrig);
        Compressed = (granny_real16*)((granny_uint8*)Compressed + StrideComp);
    }}
}


// Step through the vertex arrays, copying uncompressed texture coordinates to the new
// compressed location
static void
CompressColor(int VertexCount, int NumComponents,
              void* Vertices, int OffsetInOrig, int StrideOrig,
              void* NewVertices, int OffsetInComp, int StrideComp)
{
    // Only 3 or 4 for colors
    pp_assert(NumComponents == 3 || NumComponents == 4);

    granny_real32* Original   = (granny_real32*)((granny_uint8*)Vertices + OffsetInOrig);
    granny_uint32* Compressed = (granny_uint32*)((granny_uint8*)NewVertices + OffsetInComp);

    {for(int Idx = 0; Idx < VertexCount; ++Idx)
    {
        float* Color = (float*)Original;
        granny_uint32& Comp = *((granny_uint32*)Compressed);

        Comp = (granny_uint32(granny_uint8(Clamp(0, Color[0], 1) * 255.0f + 0.5f)) << 16  |   // r
                granny_uint32(granny_uint8(Clamp(0, Color[1], 1) * 255.0f + 0.5f)) << 8   |   // g
                granny_uint32(granny_uint8(Clamp(0, Color[2], 1) * 255.0f + 0.5f)) << 0);     // b

        if (NumComponents == 4)
            Comp |= granny_uint32(granny_uint8(Clamp(0, Color[3], 1) * 255.0f + 0.5f)) << 24; // a

        Original   = (granny_real32*)((granny_uint8*)Original   + StrideOrig);
        Compressed = (granny_uint32*)((granny_uint8*)Compressed + StrideComp);
    }}
}


// Loop through the old and new type.  When a compressed type is encountered, translate
// the data from the old to the new array.  We've marked the compressed members with
// distinct names in this sample, but they could be discovered by examining the old and
// new granny_member_type values as well.
static void
CopyCompressedMembers(int VertexCount,
                      granny_uint8* Vertices,    granny_data_type_definition* Type,
                      granny_uint8* NewVertices, granny_data_type_definition* NewType)
{
    int OldVertexSize = GrannyGetTotalObjectSize(Type);
    int NewVertexSize = GrannyGetTotalObjectSize(NewType);

    granny_int32x OffsetInOrig = 0;
    granny_int32x OffsetInComp = 0;
    {for(int Idx = 0; Type[Idx].Type != GrannyEndMember; ++Idx)
    {
        pp_assert(NewType[Idx].Type != GrannyEndMember);

        if (strstr(NewType[Idx].Name, CompNormalSuffix))
        {
            CompressNormals(VertexCount,
                            Vertices, OffsetInOrig, OldVertexSize,
                            NewVertices, OffsetInComp, NewVertexSize);
        }
        else if (strstr(NewType[Idx].Name, CompTextureSuffix))
        {
            CompressTexCoord(VertexCount, Type[Idx].ArrayWidth,
                             Vertices, OffsetInOrig, OldVertexSize,
                             NewVertices, OffsetInComp, NewVertexSize);
        }
        else if (strstr(NewType[Idx].Name, CompColorSuffix))
        {
            CompressColor(VertexCount, Type[Idx].ArrayWidth,
                          Vertices, OffsetInOrig, OldVertexSize,
                          NewVertices, OffsetInComp, NewVertexSize);
        }

        OffsetInOrig += GrannyGetMemberTypeSize(&Type[Idx]);
        OffsetInComp += GrannyGetMemberTypeSize(&NewType[Idx]);
    }}
}

// This is the command called by the preprocessor.  The driver iterates through the
// meshes, computing a new compressed format type for each mesh, and replaces the vertex
// array with the new version.
granny_file_info*
CompressVertSample(char const*          OriginalFilename,
                   char const*          OutputFilename,
                   granny_file_info*    Info,
                   key_value_pair*      KeyValues,
                   granny_int32x        NumKeyValues,
                   granny_memory_arena* TempArena)
{
    bool CompTex   = true;
    bool CompNorm  = true;
    bool CompColor = true;
    {
        char const* DoTexFlag = FindFirstValueForKey(KeyValues, NumKeyValues, "tex");
        if (DoTexFlag && atoi(DoTexFlag) == 0)
            CompTex = false;

        char const* DoNormFlag = FindFirstValueForKey(KeyValues, NumKeyValues, "norm");
        if (DoNormFlag && atoi(DoNormFlag) == 0)
            CompNorm = false;

        char const* DoColorFlag = FindFirstValueForKey(KeyValues, NumKeyValues, "color");
        if (DoColorFlag && atoi(DoColorFlag) == 0)
            CompColor = false;
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
            CreateCompressedType(CompTex, CompNorm, CompColor,
                                 VertexType, TempArena);

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
        CopyCompressedMembers(VertexCount,
                              (granny_uint8*)Vertices, VertexType,
                              (granny_uint8*)NewVertices, CompressedType);

        // Replace the vertex data with our new arrays...
        Mesh->PrimaryVertexData->VertexType = CompressedType;
        Mesh->PrimaryVertexData->Vertices   = (granny_uint8*)NewVertices;

        // We can leave the annotation and the VertexCount fields
        // alone, they don't change.
    }

    return Info;
}

static CommandRegistrar RegCompressVertSample(CompressVertSample, "CompressVertSample",
                                              "Compress normals and texture coordinates");

