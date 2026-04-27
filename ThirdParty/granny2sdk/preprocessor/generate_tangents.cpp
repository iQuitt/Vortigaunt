// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "generate_tangents.h"

#include "stb_image.h"

#include <math.h>
#include <algorithm>
#include <map>
#include <string>
#include <string.h>
#include <vector>

using namespace std;

struct CompVertex
{
    int VertSize;
    CompVertex(int size) : VertSize(size) { }
    CompVertex();

    bool operator()(granny_uint8 const* One,
                    granny_uint8 const* Two) const
    {
        return memcmp(One, Two, VertSize) < 0;
    }
};

#if _MSC_VER
  #pragma warning(disable:4127)
#endif

#define TestName(nm, val) \
  do { if (_strnicmp(Name, (nm), strlen((nm))) == 0) return (val); } while (false)

static bool IsBoneName(char const* Name)
{
    if (!Name)
        return false;

    TestName(GrannyVertexBoneIndicesName, true);
    TestName(GrannyVertexBoneWeightsName, true);

    return false;
}

static bool IsChannelName(char const* Name)
{
    if (!Name)
        return false;

    TestName(GrannyVertexPositionName, false);
    TestName(GrannyVertexBoneWeightsName, false);
    TestName(GrannyVertexBoneIndicesName, false);
    TestName(GrannyVertexNormalName, false);
    TestName(GrannyVertexTangentName, false);
    TestName(GrannyVertexBinormalName, false);
    TestName(GrannyVertexTangentBinormalCrossName, false);

    return true;
}
#undef TestName


granny_data_type_definition*
RemoveTangentsFromType(granny_data_type_definition* Type, granny_memory_arena* Arena)
{
    int const MemberCount = GrannyGetTotalTypeSize(Type) / sizeof(granny_data_type_definition);

    granny_data_type_definition* NewType = PushArray(Arena, MemberCount, granny_data_type_definition);
    granny_int32x NewTypeSize = 0;
    {for (int Idx = 0; Idx < MemberCount; ++Idx)
    {
        if (Type[Idx].Name && (_strnicmp(Type[Idx].Name,
                                         GrannyVertexTangentName,
                                         strlen(GrannyVertexTangentName)) == 0 ||
                               _strnicmp(Type[Idx].Name,
                                         GrannyVertexBinormalName,
                                         strlen(GrannyVertexBinormalName)) == 0 ||
                               _strnicmp(Type[Idx].Name,
                                         GrannyVertexTangentBinormalCrossName,
                                         strlen(GrannyVertexTangentBinormalCrossName)) == 0))
        {
            // don't copy that one.
        }
        else
        {
            NewType[NewTypeSize++] = Type[Idx];
        }
    }}
    pp_assert(NewType[NewTypeSize-1].Type == GrannyEndMember);

    return NewType;
}

granny_data_type_definition*
AddTangentsToType(granny_data_type_definition* Type,
                  bool AddTangents, bool AddBinormals,
                  bool KeepWeights,
                  granny_memory_arena* Arena)
{
    pp_assert(GrannyFindMatchingMember(Type, 0, GrannyVertexNormalName, 0));
    pp_assert(AddTangents || AddBinormals);

    int const MemberCount = GrannyGetTotalTypeSize(Type) / sizeof(granny_data_type_definition);

    granny_data_type_definition* NewType = PushArray(Arena, MemberCount+3, granny_data_type_definition);
    granny_int32x NewTypeSize = 0;
    {for (int Idx = 0; Idx < MemberCount; ++Idx)
    {
        if (!KeepWeights && Type[Idx].Name && IsBoneName(Type[Idx].Name))
            continue;

        NewType[NewTypeSize++] = Type[Idx];

        if (Type[Idx].Name && _strnicmp(Type[Idx].Name,
                                        GrannyVertexNormalName,
                                        strlen(GrannyVertexNormalName)) == 0)
        {
            if (AddTangents)
            {
                granny_data_type_definition& Def = NewType[NewTypeSize++];
                memset(&Def, 0, sizeof(Def));

                Def.Type       = GrannyReal32Member;
                Def.Name       = GrannyVertexTangentName;
                Def.ArrayWidth = 3;
            }

            if (AddBinormals)
            {
                granny_data_type_definition& Def = NewType[NewTypeSize++];
                memset(&Def, 0, sizeof(Def));

                Def.Type       = GrannyReal32Member;
                Def.Name       = GrannyVertexBinormalName;
                Def.ArrayWidth = 3;
            }
        }
    }}
    pp_assert(NewType[NewTypeSize-1].Type == GrannyEndMember);

    return NewType;
}

granny_vertex_data*
StripTangentsFromBuffer(granny_vertex_data*  VertexData,
                        vector<int>&         RemappedIndex,
                        vector<int>&         SourceIndices,
                        granny_memory_arena* Arena)
{
    granny_data_type_definition* NewType = RemoveTangentsFromType(VertexData->VertexType, Arena);

    granny_int32x NewVertexSize  = GrannyGetTotalObjectSize(NewType);
    vector<granny_uint8> VertCopy(VertexData->VertexCount * NewVertexSize);

    GrannyConvertVertexLayouts(VertexData->VertexCount,
                               VertexData->VertexType,
                               VertexData->Vertices,
                               NewType, &VertCopy[0]);

    typedef map< granny_uint8*, int, CompVertex > vertex_map;
    CompVertex VC(NewVertexSize);
    vertex_map VertMap( VC );

    {for (int Idx = 0; Idx < VertexData->VertexCount; ++Idx)
    {
        vertex_map::iterator Itr = VertMap.find(&VertCopy[Idx * NewVertexSize]);
        if (Itr == VertMap.end())
            VertMap[&VertCopy[Idx * NewVertexSize]] = Idx;
    }}

    granny_int32x NewVertexCount = 0;
    granny_uint8* StrippedVerts = PushArray(Arena, (VertexData->VertexCount * NewVertexSize), granny_uint8);

    SourceIndices.clear();
    SourceIndices.resize(VertexData->VertexCount, -1);
    {for (vertex_map::iterator Itr = VertMap.begin(); Itr != VertMap.end(); ++Itr)
    {
        granny_uint8* Dest = StrippedVerts + (NewVertexSize * NewVertexCount);

        SourceIndices[Itr->second] = NewVertexCount++;
        memcpy(Dest, Itr->first, NewVertexSize);
    }}

    RemappedIndex.resize(VertexData->VertexCount);
    {for (int Idx = 0; Idx < VertexData->VertexCount; ++Idx)
    {
        RemappedIndex[Idx] = SourceIndices[VertMap[&VertCopy[Idx * NewVertexSize]]];
        pp_assert(RemappedIndex[Idx] != -1);
    }}

    granny_vertex_data* VertData = PushObject(Arena, granny_vertex_data);
    VertData->VertexType = NewType;
    VertData->VertexCount = NewVertexCount;
    VertData->Vertices = StrippedVerts;
    VertData->VertexComponentNameCount = 0;
    VertData->VertexComponentNames = 0;
    VertData->VertexAnnotationSetCount = 0;
    VertData->VertexAnnotationSets = 0;

    return VertData;
}


granny_vertex_data*
RemoveMorphTangents(granny_vertex_data*  VertexData,
                    vector<int>&         UsedVerts,
                    granny_memory_arena* Arena)
{
    granny_data_type_definition* NewType = RemoveTangentsFromType(VertexData->VertexType, Arena);
    granny_int32x const NewSize = GrannyGetTotalObjectSize(NewType);
    pp_assert(NewSize <= GrannyGetTotalTypeSize(VertexData->VertexType));

    granny_int32x NewVertexSize  = GrannyGetTotalObjectSize(NewType);
    vector<granny_uint8> VertCopy(VertexData->VertexCount * NewVertexSize);
    GrannyConvertVertexLayouts(VertexData->VertexCount,
                               VertexData->VertexType,
                               VertexData->Vertices,
                               NewType, &VertCopy[0]);

    granny_int32x NewVertexCount = 0;
    {for (int VertIdx = 0; VertIdx < VertexData->VertexCount; ++VertIdx)
    {
        if (UsedVerts[VertIdx] == -1)
            continue;

        granny_uint8*       Dest   = VertexData->Vertices + (NewSize * UsedVerts[VertIdx]);
        granny_uint8 const* Source = &VertCopy[NewSize * VertIdx];
        memcpy(Dest, Source, NewSize);
        ++NewVertexCount;
    }}

    granny_vertex_data* VertData = PushObject(Arena, granny_vertex_data);
    VertData->VertexType  = NewType;
    VertData->VertexCount = NewVertexCount;
    VertData->Vertices    = VertexData->Vertices;
    VertData->VertexComponentNameCount = 0;
    VertData->VertexComponentNames = 0;
    VertData->VertexAnnotationSetCount = 0;
    VertData->VertexAnnotationSets = 0;

    return VertData;
}

void
ExtractChannels(granny_vertex_data& VData,
                vector< vector<granny_real32> >& ChannelValues)
{
    for (granny_data_type_definition* Type = VData.VertexType;
         Type->Type != GrannyEndMember; ++Type)
    {
        if (!IsChannelName(Type->Name))
            continue;

        granny_data_type_definition ChannelType[2] = { {GrannyEndMember}, {GrannyEndMember} };
        ChannelType[0] = *Type;
        ChannelType[0].Type = GrannyReal32Member;

        vector<granny_real32> NewChannel(VData.VertexCount * GrannyGetTotalObjectSize(ChannelType)/sizeof(granny_real32));
        GrannyConvertVertexLayouts(VData.VertexCount, VData.VertexType, VData.Vertices,
                                   ChannelType, &NewChannel[0]);
        ChannelValues.push_back(NewChannel);
    }
}

static void
PushVertsIntoBuilder(granny_mesh_builder*  Builder,
                     granny_vertex_data*   OrigVertexData,
                     granny_vertex_data*   MorphVertexData,
                     bool                  MorphIsDeltas,
                     vector<granny_int32>& Indices32)
{
    pp_assert(OrigVertexData->VertexCount == MorphVertexData->VertexCount);

    int const VertexCount = MorphVertexData->VertexCount;

    // Build the positions and bone weights...
    {
        vector<granny_p3_vertex> OrigPositions(VertexCount);
        vector<granny_p3_vertex> MorphPositions(VertexCount);

        GrannyConvertVertexLayouts(VertexCount,
                                   OrigVertexData->VertexType, OrigVertexData->Vertices,
                                   GrannyP3VertexType, &OrigPositions[0]);
        GrannyConvertVertexLayouts(VertexCount,
                                   MorphVertexData->VertexType, MorphVertexData->Vertices,
                                   GrannyP3VertexType, &MorphPositions[0]);

        {for (int Idx = 0; Idx < VertexCount; ++Idx)
        {
            if (MorphIsDeltas)
            {
                MorphPositions[Idx].Position[0] += OrigPositions[Idx].Position[0];
                MorphPositions[Idx].Position[1] += OrigPositions[Idx].Position[1];
                MorphPositions[Idx].Position[2] += OrigPositions[Idx].Position[2];
            }

            GrannySetPosition(Builder,
                              MorphPositions[Idx].Position[0],
                              MorphPositions[Idx].Position[1],
                              MorphPositions[Idx].Position[2]);
            GrannyPushVertex(Builder);
        }}
    }

    // The triangles
    {
        // Abuse the position vertex in order to pull the normals
        vector<granny_p3_vertex> OrigNormals(VertexCount);
        vector<granny_p3_vertex> MorphNormals(VertexCount);
        granny_data_type_definition NormalVertexType[] =
            { {GrannyReal32Member, GrannyVertexNormalName, 0, 3}, {GrannyEndMember} };

        GrannyConvertVertexLayouts(VertexCount, OrigVertexData->VertexType, OrigVertexData->Vertices,
                                   NormalVertexType, &OrigNormals[0]);
        GrannyConvertVertexLayouts(VertexCount, MorphVertexData->VertexType, MorphVertexData->Vertices,
                                   NormalVertexType, &MorphNormals[0]);

        vector< vector< granny_real32 > > OrigChannelValues;
        vector< vector< granny_real32 > > MorphChannelValues;
        ExtractChannels(*OrigVertexData,  OrigChannelValues);
        ExtractChannels(*MorphVertexData, MorphChannelValues);
        pp_assert(OrigChannelValues.size() == MorphChannelValues.size());

        if (MorphIsDeltas)
        {
            for (int Idx = 0; Idx < VertexCount; Idx++)
            {
                MorphNormals[Idx].Position[0] += OrigNormals[Idx].Position[0];
                MorphNormals[Idx].Position[1] += OrigNormals[Idx].Position[1];
                MorphNormals[Idx].Position[2] += OrigNormals[Idx].Position[2];
            }

            for (int chan = 0; chan < (int)OrigChannelValues.size(); ++chan)
            {
                vector<granny_real32>& Orig  = OrigChannelValues[chan];
                vector<granny_real32>& Morph = MorphChannelValues[chan];
                int ChannelSize = int(Orig.size() / OrigVertexData->VertexCount);

                for (int Idx = 0; Idx < VertexCount; Idx++)
                {
                    for (int val = 0; val < ChannelSize; ++val)
                    {
                        Morph[Idx * ChannelSize + val] += Orig[Idx * ChannelSize + val];
                    }
                }
            }
        }


        {for (unsigned int Idx = 0; Idx < Indices32.size(); ++Idx)
        {
            int const Side = (Idx%3);
            granny_int32x const VertexIndex = Indices32[Idx];
            GrannySetVertexIndex(Builder, Side, VertexIndex);

            GrannySetNormal(Builder, Side,
                            MorphNormals[VertexIndex].Position[0],
                            MorphNormals[VertexIndex].Position[1],
                            MorphNormals[VertexIndex].Position[2]);

            // Set the channels
            {for (granny_uint32 v = 0; v < OrigChannelValues.size(); ++v)
            {
                vector<granny_real32>& Channel = MorphChannelValues[v];
                int ChannelSize = int(Channel.size() / OrigVertexData->VertexCount);
                pp_assert((Channel.size() % OrigVertexData->VertexCount) == 0);
                pp_assert(ChannelSize <= 4);

                GrannySetChannel(Builder, Side, v, &Channel[VertexIndex * ChannelSize], ChannelSize);
            }}

            if ((Idx % 3) == 2)
            {
                GrannyPushTriangle(Builder);
            }
        }}
    }
}


granny_mesh*
BuildTangents(granny_mesh*  OldMesh,
              vector<int>&  VertSpawnMap,
              bool          AddTangents,
              bool          AddBinormals,
              granny_memory_arena* Arena)
{
    granny_vertex_data&  VData = *OldMesh->PrimaryVertexData;
    granny_tri_topology& TTopo = *OldMesh->PrimaryTopology;

    granny_data_type_definition* NewType =
        AddTangentsToType(VData.VertexType, AddTangents, AddBinormals, true, Arena);

    granny_mesh_builder* Builder =
        GrannyBeginMesh(GrannyGetMeshVertexCount(OldMesh),
                        GrannyGetMeshTriangleCount(OldMesh),
                        GrannyGetMeshTriangleGroupCount(OldMesh),
                        OldMesh->BoneBindingCount, NewType);

    vector<granny_int32> Indices(GrannyGetMeshIndexCount(OldMesh));
    GrannyCopyMeshIndices(OldMesh, 4, &Indices[0]);

    // Build the positions and bone weights...
    {
        vector<granny_p3_vertex> Positions(GrannyGetMeshVertexCount(OldMesh));
        vector<granny_vertex_weight_arrays> Weights(GrannyGetMeshVertexCount(OldMesh));

        GrannyConvertVertexLayouts(VData.VertexCount, VData.VertexType, VData.Vertices,
                                   GrannyP3VertexType, &Positions[0]);
        GrannyConvertVertexLayouts(VData.VertexCount, VData.VertexType, VData.Vertices,
                                   GrannyVertexWeightArraysType, &Weights[0]);

        {for (int Idx = 0; Idx < VData.VertexCount; ++Idx)
        {
            GrannySetPosition(Builder,
                              Positions[Idx].Position[0],
                              Positions[Idx].Position[1],
                              Positions[Idx].Position[2]);

            int CurrWeight = 0;
            while (Weights[Idx].BoneWeights[CurrWeight] != 0 && CurrWeight < GrannyMaximumWeightCount)
            {
                GrannyAddWeight(Builder,
                                Weights[Idx].BoneIndices[CurrWeight],
                                Weights[Idx].BoneWeights[CurrWeight]);
                ++CurrWeight;
            }

            GrannyPushVertex(Builder);
        }}
    }

    // The triangles
    {
        // Abuse the position vertex in order to pull the normals
        vector<granny_p3_vertex> Normals(VData.VertexCount);
        granny_data_type_definition NormalVertexType[] = { {GrannyReal32Member, GrannyVertexNormalName, 0, 3}, {GrannyEndMember} };
        GrannyConvertVertexLayouts(VData.VertexCount, VData.VertexType, VData.Vertices,
                                   NormalVertexType, &Normals[0]);

        vector< vector< granny_real32 > > ChannelValues;
        ExtractChannels(VData, ChannelValues);

        {for (int GroupIdx = 0; GroupIdx < TTopo.GroupCount; ++GroupIdx)
        {
            granny_tri_material_group& Group = TTopo.Groups[GroupIdx];

            {for (int Tri = Group.TriFirst; Tri < (Group.TriFirst+Group.TriCount); ++Tri)
            {
                {for (int Side = 0; Side < 3; ++Side)
                {
                    granny_int32x const VertexIndex = Indices[Tri*3 + Side];
                    GrannySetVertexIndex(Builder, Side, VertexIndex);

                    GrannySetNormal(Builder, Side,
                                    Normals[VertexIndex].Position[0],
                                    Normals[VertexIndex].Position[1],
                                    Normals[VertexIndex].Position[2]);

                    // Set the channels
                    {for (granny_uint32 Idx = 0; Idx < ChannelValues.size(); ++Idx)
                    {
                        vector<granny_real32>& Channel = ChannelValues[Idx];
                        int ChannelSize = int(Channel.size() / VData.VertexCount);
                        pp_assert((Channel.size() % VData.VertexCount) == 0);
                        pp_assert(ChannelSize <= 4);

                        GrannySetChannel(Builder, Side, Idx, &Channel[VertexIndex * ChannelSize], ChannelSize);
                    }}
                }}

                GrannySetMaterial(Builder, Group.MaterialIndex);
                GrannyPushTriangle(Builder);
            }}
        }}
    }

    // Build the tangents
    GrannyGenerateTangentSpaceFromUVs(Builder);

    VertSpawnMap.clear();
    VertSpawnMap.resize(GrannyGetResultingVertexCount(Builder), -1);
    GrannySerializeResultingCoincidentVertexMap(Builder, &VertSpawnMap[0]);

    GrannyEndMesh(Builder,
                  &OldMesh->PrimaryVertexData,
                  &OldMesh->PrimaryTopology);
    return OldMesh;
}


static void
RedeltifyVertexData(granny_vertex_data* OrigVertexData,
                    granny_vertex_data* NewVData)
{
    pp_assert(OrigVertexData && NewVData);
    pp_assert(OrigVertexData->VertexCount == NewVData->VertexCount);
    pp_assert(OrigVertexData->VertexType && NewVData->VertexType);

    // Very slow, but: eh.
    int const OrigSize = GrannyGetTotalObjectSize(OrigVertexData->VertexType);
    int const NewSize  = GrannyGetTotalObjectSize(NewVData->VertexType);

    {for (int Idx = 0; Idx < OrigVertexData->VertexCount; ++Idx)
    {
        granny_uint8 const* SourceBase = OrigVertexData->Vertices + Idx * OrigSize;
        granny_uint8*       DestBase   = NewVData->Vertices + Idx * NewSize;

        granny_data_type_definition* Member = NewVData->VertexType;
        while (Member->Type != GrannyEndMember)
        {
            if (Member->Type == GrannyReal32Member)
            {
                granny_variant Dest;
                granny_variant Source;
                if (GrannyFindMatchingMember(OrigVertexData->VertexType, SourceBase, Member->Name, &Source) &&
                    GrannyFindMatchingMember(NewVData->VertexType, DestBase, Member->Name, &Dest))
                {
                    if (Dest.Type->Type == GrannyReal32Member && Source.Type->Type == GrannyReal32Member &&
                        Dest.Type->ArrayWidth == Source.Type->ArrayWidth)
                    {
                        granny_real32 const* SourceFloats = (granny_real32 const*)Source.Object;
                        granny_real32*       DestFloats   = (granny_real32*)Dest.Object;

                        int const Width = Dest.Type->ArrayWidth ? Dest.Type->ArrayWidth : 1;
                        {for (int Idx = 0; Idx < Width; ++Idx)
                        {
                            DestFloats[Idx] -= SourceFloats[Idx];
                        }}
                    }
                }
            }

            ++Member;
        }
    }}
}


granny_vertex_data*
BuildMorphTangents(granny_vertex_data*   OrigVertexData,
                   granny_vertex_data*   MorphVertexData,
                   bool                  MorphIsDeltas,
                   vector<granny_int32>& Indices32,
                   vector<int>&          VertSpawnMap,
                   bool                  AddTangents,
                   bool                  AddBinormals,
                   granny_memory_arena*  Arena)
{
    int const OldVertexSize  = GrannyGetTotalObjectSize(MorphVertexData->VertexType);
    int const NewVertexCount = (int)VertSpawnMap.size();
    granny_uint8* NewVertices = PushArray(Arena, int(OldVertexSize*VertSpawnMap.size()), granny_uint8);

    {for (int Idx = 0; Idx < NewVertexCount; ++Idx)
    {
        // We need to find the index that's in the original buffer.
        int OrigVert = Idx;
        while (OrigVert >= MorphVertexData->VertexCount)
        {
            OrigVert = VertSpawnMap[OrigVert];
        }

        granny_uint8* Dest         = NewVertices + (Idx*OldVertexSize);
        granny_uint8 const* Source = MorphVertexData->Vertices + (OrigVert*OldVertexSize);

        memcpy(Dest, Source, OldVertexSize);
    }}

    MorphVertexData->VertexCount = NewVertexCount;
    MorphVertexData->Vertices    = NewVertices;

    granny_data_type_definition* NewType =
        AddTangentsToType(MorphVertexData->VertexType, AddTangents, AddBinormals, false, Arena);
    //int const NewVertexSize  = GrannyGetTotalObjectSize(NewType);

    granny_mesh_builder* Builder =
        GrannyBeginMesh(MorphVertexData->VertexCount,
                        (int)Indices32.size()/3,
                        1, 0, NewType);

    PushVertsIntoBuilder(Builder, OrigVertexData, MorphVertexData, MorphIsDeltas, Indices32);
    GrannyDisableTangentVertexSplits(Builder);
    GrannyGenerateTangentSpaceFromUVs(Builder);

    granny_vertex_data*  NewVData;
    granny_tri_topology* NewTopo;
    GrannyEndMeshInPlace(Builder,
                         PushArray(Arena, GrannyGetResultingVertexDataSize(Builder), granny_uint8), &NewVData,
                         PushArray(Arena, GrannyGetResultingTopologySize(Builder), granny_uint8), &NewTopo);
    pp_assert(NewVData->VertexCount == NewVertexCount);

    if (MorphIsDeltas)
    {
        RedeltifyVertexData(OrigVertexData, NewVData);
    }

    return NewVData;
}

granny_data_type_definition UVVerts[] = {
    { GrannyReal32Member, GrannyVertexTextureCoordinatesName "0", 0, 2 },
    { GrannyEndMember }
};

granny_mesh*
GenTangentsForMesh(granny_mesh* OriginalMesh,
                   bool AddTangents, bool AddBinormals,
                   granny_memory_arena* Arena)
{
    pp_assert(AddTangents || AddBinormals);

    // Normalize the verts first.  What we want is a mesh with no tangents or binormal
    // components, with morph targets that are mapped in the same way.  (Note that
    // stripping the tangents means that we have to re-unify split verts, so we have to be
    // careful to replicate the mapping from the base mesh across all the morphs.)
    {
        vector<int> Remapped;
        vector<int> Used;
        OriginalMesh->PrimaryVertexData = StripTangentsFromBuffer(OriginalMesh->PrimaryVertexData, Remapped, Used, Arena);

        {for (int MorphIdx = 0; MorphIdx < OriginalMesh->MorphTargetCount; ++MorphIdx)
        {
            // We have to be a bit careful to remove exactly the same verts
            OriginalMesh->MorphTargets[MorphIdx].VertexData =
                RemoveMorphTangents(OriginalMesh->MorphTargets[MorphIdx].VertexData, Used, Arena);
        }}

        // TODO: 16-bit fix
        for (int i = 0; i < OriginalMesh->PrimaryTopology->IndexCount; ++i)
            OriginalMesh->PrimaryTopology->Indices[i] = Remapped[OriginalMesh->PrimaryTopology->Indices[i]];
    }

    // Ok, the mesh is in good shape to start.  Build the tangents for the original mesh,
    // preserving the vertex split mapping
    vector<int> VertSpawnMap;
    granny_mesh* NewMesh =
        BuildTangents(OriginalMesh, VertSpawnMap, AddTangents, AddBinormals, Arena);

    vector<granny_int32> Indices32(GrannyGetMeshIndexCount(NewMesh));
    GrannyCopyMeshIndices(NewMesh, 4, &Indices32[0]);

    {for (int MorphIdx = 0; MorphIdx < NewMesh->MorphTargetCount; ++MorphIdx)
    {
        granny_morph_target& Target = NewMesh->MorphTargets[MorphIdx];
        Target.VertexData =
            BuildMorphTangents(NewMesh->PrimaryVertexData,
                               Target.VertexData,
                               !!Target.DataIsDeltas,
                               Indices32, VertSpawnMap,
                               AddTangents, AddBinormals, Arena);
    }}

    return NewMesh;
}

granny_file_info*
GenerateTangents(char const*          OriginalFilename,
                 char const*          OutputFilename,
                 granny_file_info* Info,
                 key_value_pair* KeyValues,
                 granny_int32x NumKeyValues,
                 granny_memory_arena* TempArena)
{
    bool AddTangents  = true;
    bool AddBinormals = true;

    {for (int MeshIdx = 0; MeshIdx < Info->MeshCount; ++MeshIdx)
    {
        granny_mesh* Mesh = Info->Meshes[MeshIdx];
        if (!Mesh)
            continue;

        Info->Meshes[MeshIdx] = GenTangentsForMesh(Mesh, AddTangents, AddBinormals, TempArena);
    }}

    return Info;
}

static const char* HelpString =
    (" GenerateTangents will create or refresh the tangent coordinates\n"
     " for all of the granny_mesh objects in a file, including their\n"
     " morph targets. Really, you probably only want to use this to\n"
     " create valid morph target tangent fields, which is something that\n"
     " Max and Maya have trouble providing in a game-appropriate fashion.\n"
     "\n"
     "    preprocessor GenerateTangents file.gr2 -output file_w_tang.gr2\n");
     
   

// Register the commands
static CommandRegistrar RegGenerateTextures(GenerateTangents,
                                            "GenerateTangents",
                                            "Generates tangents for the meshes in the specified file",
                                            HelpString);

