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

granny_file_info*
KillUncommonData(char const*       OriginalFilename,
                 char const*       OutputFilename,
                 granny_file_info* Info,
                 key_value_pair* KeyValues,
                 granny_int32x   NumKeyValues,
                 granny_memory_arena* TempArena)
{
    // Remove exporter details
    Info->ExporterInfo = 0;

    // Remove arttool extended info...
    if (Info->ArtToolInfo != 0)
    {
        Info->ArtToolInfo->ExtendedData.Object = 0;
        Info->ArtToolInfo->ExtendedData.Type = 0;
    }

    // Remove bone extended data
    {for(granny_int32x Skel = 0; Skel < Info->SkeletonCount; ++Skel)
    {
        granny_skeleton* Skeleton = Info->Skeletons[Skel];
        {for(granny_int32x Bone = 0; Bone < Skeleton->BoneCount; ++Bone)
        {
            Skeleton->Bones[Bone].ExtendedData.Type = 0;
            Skeleton->Bones[Bone].ExtendedData.Object = 0;
        }}
    }}

    // Remove vertex annotations and component names
    {for(granny_int32x VData = 0; VData < Info->VertexDataCount; ++VData)
    {
        Info->VertexDatas[VData]->VertexComponentNameCount = 0;
        Info->VertexDatas[VData]->VertexComponentNames = 0;
        Info->VertexDatas[VData]->VertexAnnotationSetCount = 0;
        Info->VertexDatas[VData]->VertexAnnotationSets = 0;
    }}

    // Remove all triangle annotations from the topologies
    {for(granny_int32x Topo = 0; Topo < Info->TriTopologyCount; ++Topo)
    {
        Info->TriTopologies[Topo]->VertexToVertexCount = 0;
        Info->TriTopologies[Topo]->VertexToVertexMap = 0;
        Info->TriTopologies[Topo]->VertexToTriangleCount = 0;
        Info->TriTopologies[Topo]->VertexToTriangleMap = 0;
        Info->TriTopologies[Topo]->SideToNeighborCount = 0;
        Info->TriTopologies[Topo]->SideToNeighborMap = 0;
        Info->TriTopologies[Topo]->BonesForTriangleCount = 0;
        Info->TriTopologies[Topo]->BonesForTriangle = 0;
        Info->TriTopologies[Topo]->TriangleToBoneCount = 0;
        Info->TriTopologies[Topo]->TriangleToBoneIndices = 0;
        Info->TriTopologies[Topo]->TriAnnotationSetCount = 0;
        Info->TriTopologies[Topo]->TriAnnotationSets = 0;
    }}

    // Remove extended data from the meshes
    {for(granny_int32x Mesh = 0; Mesh < Info->MeshCount; ++Mesh)
    {
        Info->Meshes[Mesh]->ExtendedData.Type = 0;
        Info->Meshes[Mesh]->ExtendedData.Object = 0;
    }}

    // Remove extended data from the track_groups
    {for(granny_int32x TrackGroup = 0; TrackGroup < Info->TrackGroupCount; ++TrackGroup)
    {
        Info->TrackGroups[TrackGroup]->ExtendedData.Type = 0;
        Info->TrackGroups[TrackGroup]->ExtendedData.Object = 0;
    }}

    // Remove extended data from the file_info itself
    Info->ExtendedData.Type = 0;
    Info->ExtendedData.Object = 0;

    // Write out the modified Info
    return Info;
}

static const char* HelpString =
    (" KillUncommonData removes data mostly useful in toolchains rather\n"
     " than in the runtime to save space. This includes ExtendedData members\n"
     " on granny_bones and granny_meshes, granny_vector_tracks, etc. This\n"
     " command is useful more as a guide to creating your own tailored version\n"
     " that removes elements that your engine doesn't care about; it paints\n"
     " with a very broad brush.\n"
     "\n"
     "    preprocessor KillUncommonData file.gr2 -output file_stripped.gr2\n"
     "    preprocessor KillUncommonData file.gr2 -output file.gr2\n");

static CommandRegistrar RegKillUncommonData(KillUncommonData,
                                            "KillUncommonData",
                                            "Removes data mostly useful in toolchains rather than the runtime to save space",
                                            HelpString);

