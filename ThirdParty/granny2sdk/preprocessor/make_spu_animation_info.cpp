// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "preprocessor.h"
#include <granny.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <algorithm>
#include <functional>

using namespace std;

static void
ExtractTruncatedOneOverKnotScale(granny_real32* OOKS,
                                 granny_uint16 OOKSTrunc)
{
    *(granny_uint32*)(OOKS) = ((granny_uint32)OOKSTrunc)<<16;
}

struct sorted_track
{
    granny_int32            OriginalIndex;
    granny_transform_track* Track;
    granny_real32           LODValue;

    bool operator>(sorted_track const& Other) const
    {
        return LODValue > Other.LODValue;
    }
};

enum track_type
{
    Position,
    Orientation,
    ScaleShear
};

// Unscaled, raw 32-bit curves
bool ConvertDak32f(granny_curve2& OldCurve, track_type, vector<granny_uint8>& CurveBuffer);
bool ConvertD3I1k32f(granny_curve2& OldCurve, track_type, vector<granny_uint8>& CurveBuffer);

// Linear 16 bit curves
bool ConvertDak16u(granny_curve2& OldCurve, track_type, vector<granny_uint8>& CurveBuffer);
bool ConvertD3k16u(granny_curve2& OldCurve, track_type, vector<granny_uint8>& CurveBuffer);
bool ConvertD3I1k16u(granny_curve2& OldCurve, track_type, vector<granny_uint8>& CurveBuffer);
bool ConvertD9I3k16u(granny_curve2& OldCurve, track_type, vector<granny_uint8>& CurveBuffer);

// Linear 8 bit curves
bool ConvertDak8u(granny_curve2& OldCurve, track_type, vector<granny_uint8>& CurveBuffer);
bool ConvertD3k8u(granny_curve2& OldCurve, track_type, vector<granny_uint8>& CurveBuffer);
bool ConvertD3I1k8u(granny_curve2& OldCurve, track_type, vector<granny_uint8>& CurveBuffer);
bool ConvertD9I3k8u(granny_curve2& OldCurve, track_type, vector<granny_uint8>& CurveBuffer);

// Normalized orientation curves
bool ConvertD4nk8u(granny_curve2& OldCurve, track_type, vector<granny_uint8>& CurveBuffer);
bool ConvertD4nk16u(granny_curve2& OldCurve, track_type, vector<granny_uint8>& CurveBuffer);


#define HANDLE_CURVE_TYPE(TestType, TrackType, Function)        \
    if (GrannyDataTypesAreEqualWithNames(CurveType, TestType))  \
    {                                                           \
        Success = Function (OldCurve, TrackType, CurveBuffer);  \
    }

bool ConvertTrack(granny_uint8** TrackMemory,
                  granny_int32*  TrackMemorySize,
                  granny_curve2& OldCurve,
                  track_type TrackType,
                  granny_memory_arena* Arena)
{
    pp_assert(TrackMemory);
    pp_assert(TrackMemorySize);

    granny_data_type_definition const* CurveType =
        GrannyCurveGetDataTypeDefinition(&OldCurve);

    bool Success = false;
    vector<granny_uint8> CurveBuffer;

    HANDLE_CURVE_TYPE(GrannyCurveDataDaK32fC32fType,   TrackType, ConvertDak32f);
    HANDLE_CURVE_TYPE(GrannyCurveDataD3I1K32fC32fType, TrackType, ConvertD3I1k32f);
    HANDLE_CURVE_TYPE(GrannyCurveDataD4nK8uC7uType,    TrackType, ConvertD4nk8u);
    HANDLE_CURVE_TYPE(GrannyCurveDataD4nK16uC15uType,  TrackType, ConvertD4nk16u);
    HANDLE_CURVE_TYPE(GrannyCurveDataDaK16uC16uType,   TrackType, ConvertDak16u);
    HANDLE_CURVE_TYPE(GrannyCurveDataD3K16uC16uType,   TrackType, ConvertD3k16u);
    HANDLE_CURVE_TYPE(GrannyCurveDataD3I1K16uC16uType, TrackType, ConvertD3I1k16u);
    HANDLE_CURVE_TYPE(GrannyCurveDataD9I3K16uC16uType, TrackType, ConvertD9I3k16u);
    HANDLE_CURVE_TYPE(GrannyCurveDataDaK8uC8uType,     TrackType, ConvertDak8u);
    HANDLE_CURVE_TYPE(GrannyCurveDataD3K8uC8uType,     TrackType, ConvertD3k8u);
    HANDLE_CURVE_TYPE(GrannyCurveDataD3I1K8uC8uType,   TrackType, ConvertD3I1k8u);
    HANDLE_CURVE_TYPE(GrannyCurveDataD9I3K8uC8uType,   TrackType, ConvertD9I3k8u);

    if (Success)
    {
        pp_assert((CurveBuffer.size() % 16) == 0);
        *TrackMemorySize = (granny_int32)CurveBuffer.size();
        *TrackMemory = PushArray(Arena, *TrackMemorySize, granny_uint8);
        memcpy(*TrackMemory, &CurveBuffer[0], *TrackMemorySize);
    }
    else
    {
        *TrackMemorySize = 0;
        *TrackMemory = 0;
    }

    return Success;
}


granny_spu_animation* ConvertAnim(granny_animation* Anim,
                                  granny_memory_arena* Arena)
{
    granny_spu_animation* NewAnim = PushObject(Arena, granny_spu_animation);

    NewAnim->Name = Anim->Name;
    NewAnim->Duration = Anim->Duration;
    NewAnim->TimeStep = Anim->TimeStep;
    NewAnim->Oversampling = Anim->Oversampling;

    NewAnim->TrackGroups =
        PushArray(Arena, Anim->TrackGroupCount, granny_spu_track_group*);
    NewAnim->TrackGroupCount = 0;

    {for(granny_int32x TGroup = 0; TGroup < Anim->TrackGroupCount; ++TGroup)
    {
        granny_track_group* OldGroup = Anim->TrackGroups[TGroup];
        if (OldGroup->TransformTrackCount == 0)
        {
            NewAnim->TrackGroups[TGroup] = 0;
            continue;
        }

        granny_spu_track_group* Group = PushObject(Arena, granny_spu_track_group);
        memset(Group, 0, sizeof(*Group));
        NewAnim->TrackGroups[NewAnim->TrackGroupCount++] = Group;

        Group->Name = OldGroup->Name;
        Group->Flags = OldGroup->Flags;
        Group->TrackNameCount = OldGroup->TransformTrackCount;
        Group->TrackNames = PushArray(Arena, Group->TrackNameCount, char const*);
        {for(int NameIdx = 0; NameIdx < Group->TrackNameCount; ++NameIdx)
        {
            Group->TrackNames[NameIdx] = OldGroup->TransformTracks[NameIdx].Name;
        }}


        // Sort by lod.
        vector<sorted_track> SortedTracks(OldGroup->TransformTrackCount);
        {
            {for(granny_int32x TTrack = 0; TTrack < OldGroup->TransformTrackCount; ++TTrack)
            {
                SortedTracks[TTrack].OriginalIndex = TTrack;
                SortedTracks[TTrack].Track = &OldGroup->TransformTracks[TTrack];

                if (OldGroup->TransformLODErrorCount)
                    SortedTracks[TTrack].LODValue = OldGroup->TransformLODErrors[TTrack];
                else
                {
                    if (GrannyTransformTrackIsAnimated(SortedTracks[TTrack].Track))
                        SortedTracks[TTrack].LODValue = 1e38f;
                    else
                        SortedTracks[TTrack].LODValue = 0.0f;
                }
            }}

            sort(SortedTracks.begin(), SortedTracks.end(), greater<sorted_track>());
        }

        // Allocate the track components, and the constants
        {
            Group->TransformTrackCount = OldGroup->TransformTrackCount;
            if (OldGroup->TransformTrackCount)
            {
                Group->TransformTracks =
                    PushArray(Arena, Group->TransformTrackCount, granny_spu_transform_track);
            }
        }

        vector<granny_uint8> AllCurveBytes;
        {for(granny_int32x TTrack = 0; TTrack < Group->TransformTrackCount; ++TTrack)
        {
            granny_transform_track& Track = *SortedTracks[TTrack].Track;
            granny_spu_transform_track& CurrTrack = Group->TransformTracks[TTrack];

            CurrTrack.FromNameIndex = SortedTracks[TTrack].OriginalIndex;
            CurrTrack.Flags         = SortedTracks[TTrack].Track->Flags;
            CurrTrack.AnimLODValue  = SortedTracks[TTrack].LODValue;
            GrannySampleTrackUUULocalAtTime0(SortedTracks[TTrack].Track,
                                             &CurrTrack.LODTransform);

            CurrTrack.PosCurveOffset = GrannySPUTransformTrackNoCurve;
            if (!GrannyCurveIsConstantOrIdentity(&Track.PositionCurve))
            {
                granny_uint8* CurveBytes = NULL;
                granny_int32  CurveByteCount = 0;
                ConvertTrack(&CurveBytes, &CurveByteCount,
                             Track.PositionCurve,
                             Position,
                             Arena);

                if (CurveByteCount != 0)
                {
                    CurrTrack.PosCurveOffset = (granny_uint32)AllCurveBytes.size();
                    AllCurveBytes.insert(AllCurveBytes.end(),
                                         CurveBytes,
                                         CurveBytes + CurveByteCount);
                }
            }

            CurrTrack.OriCurveOffset = GrannySPUTransformTrackNoCurve;
            if (!GrannyCurveIsConstantOrIdentity(&Track.OrientationCurve))
            {
                granny_uint8* CurveBytes = NULL;
                granny_int32  CurveByteCount = 0;
                ConvertTrack(&CurveBytes, &CurveByteCount,
                             Track.OrientationCurve,
                             Orientation,
                             Arena);

                if (CurveByteCount != 0)
                {
                    CurrTrack.OriCurveOffset = (granny_uint32)AllCurveBytes.size();
                    AllCurveBytes.insert(AllCurveBytes.end(),
                                         CurveBytes,
                                         CurveBytes + CurveByteCount);
                }
            }

            CurrTrack.SSCurveOffset = GrannySPUTransformTrackNoCurve;
            if (!GrannyCurveIsConstantOrIdentity(&Track.ScaleShearCurve))
            {
                granny_uint8* CurveBytes = NULL;
                granny_int32  CurveByteCount = 0;
                ConvertTrack(&CurveBytes, &CurveByteCount,
                             Track.ScaleShearCurve,
                             ScaleShear,
                             Arena);
                if (CurveByteCount != 0)
                {
                    CurrTrack.SSCurveOffset = (granny_uint32)AllCurveBytes.size();
                    AllCurveBytes.insert(AllCurveBytes.end(),
                                         CurveBytes,
                                         CurveBytes + CurveByteCount);
                }
            }
        }}

        // Put the bytes into the track_group
        Group->CurveByteCount = (granny_int32)AllCurveBytes.size();
        if (Group->CurveByteCount)
        {
            Group->CurveBytes = PushArray(Arena, Group->CurveByteCount, granny_uint8);
            memcpy(Group->CurveBytes, &AllCurveBytes[0], Group->CurveByteCount);
        }
        else
        {
            Group->CurveByteCount = 0;
            Group->CurveBytes = 0;
        }

        // Recompute the RequiredCurveBytes, and check that all the tracks are in-bounds
        GrannyRecomputeSPUTrackRequiredBytes(Group);
        {
            bool AllInbounds = true;
            {for (int Idx = 0; Idx < Group->TransformTrackCount; ++Idx)
            {
                if (Group->TransformTracks[Idx].BytesRequiredToSample > GrannyMaxSPUTransformTrackSize)
                {
                    fprintf(stderr, "MakeSPUAnimation: Track: %s is larger than allowed: (%d bytes, max: %d)\n",
                            Group->TrackNames[Group->TransformTracks[Idx].FromNameIndex],
                            Group->TransformTracks[Idx].BytesRequiredToSample,
                            GrannyMaxSPUTransformTrackSize);
                    AllInbounds = false;
                }
            }}
            if (!AllInbounds)
                return 0;
        }
    }}

    return NewAnim;
}

bool MakeSPUAnimation(input_file&     InputFile,
                      key_value_pair* KeyValues,
                      granny_int32x   NumKeyValues,
                      granny_memory_arena* Arena)
{
    RequireKey("output", "must specify an output file with \"-output <filename>\"\n");

    granny_file_info* Info = ExtractFileInfo(InputFile);
    if (Info == 0)
    {
        ErrOut("unable to obtain a granny_file_info from the input file\n");
        return false;
    }
    if (Info->AnimationCount == 0)
    {
        ErrOut("input file contains no animations\n");
        return false;
    }

    granny_spu_animation_info* SPUInfo = PushObject(Arena, granny_spu_animation_info);

    SPUInfo->SPUAnimationCount = Info->AnimationCount;
    SPUInfo->SPUAnimations =
        PushArray(Arena, SPUInfo->SPUAnimationCount, granny_spu_animation*);

    SPUInfo->ExtendedData.Object = 0;
    SPUInfo->ExtendedData.Type = 0;

    {for(granny_int32x Anim = 0; Anim < Info->AnimationCount; ++Anim)
    {
        granny_spu_animation* Converted = ConvertAnim(Info->Animations[Anim], Arena);
        if (Converted == 0)
        {
            ErrOut("%d (%s)\n",
                    Anim, Info->Animations[Anim] ? "NULL" : Info->Animations[Anim]->Name);
            return false;
        }

        SPUInfo->SPUAnimations[Anim] = Converted;
    }}

    enum Sections
    {
        DefaultSection   = 0,
        AlignmentSection = 1,
        StringSection    = 2,
        TypeSection      = GrannyStandardDiscardableSection
    };

    // Write out the modified Info
    granny_int32x FinalSectionCount = GrannyStandardSectionCount;
    granny_file_builder *Builder = GrannyBeginFile(FinalSectionCount,
                                                   GrannyCurrentGRNStandardTag,
                                                   GrannyGRNFileMV_32Bit_BigEndian,  // just put it in ps3 format right away
                                                   GrannyGetTemporaryDirectory(),
                                                   "Prefix");
    granny_file_data_tree_writer *Writer =
        GrannyBeginFileDataTreeWriting(GrannySPUAnimationInfoType, SPUInfo,
                                       TypeSection, DefaultSection);

    {for(granny_int32x Anim = 0; Anim < Info->AnimationCount; ++Anim)
    {
        granny_spu_animation* Converted = SPUInfo->SPUAnimations[Anim];
        {for(granny_int32x Track = 0; Track < Converted->TrackGroupCount; ++Track)
        {
            granny_spu_track_group* TrackGroup = Converted->TrackGroups[Track];
            GrannySetFileSectionForObject(Writer, TrackGroup->TransformTracks, AlignmentSection);
            GrannySetFileSectionForObject(Writer, TrackGroup->CurveBytes,      AlignmentSection);
        }}
    }}

    GrannySetFileSectionForObjectsOfType(Writer, GrannyStringType, StringSection);
    GrannySetFileSectionFormat(Builder, DefaultSection, GrannyNoCompression, 16);
    GrannySetFileSectionFormat(Builder, AlignmentSection, GrannyNoCompression, 128);

    GrannyWriteDataTreeToFileBuilder(Writer, Builder);
    GrannyEndFileDataTreeWriting(Writer);

    char const* OutputFile = FindFirstValueForKey(KeyValues, NumKeyValues, "output");
    pp_assert(OutputFile);

    bool success = GrannyEndFile(Builder, OutputFile);
    if (!success)
    {
        ErrOut("failed to write to %s\n", OutputFile);
    }

    return success;
}

// =============================================================================
// Curve Conversions
// =============================================================================
#define COMPILE_FOR_PS3 1

granny_real32 FakeIdentityVector[9] = { 0 };

inline granny_uint32 Reverse32(granny_uint32 Value)
{
#if COMPILE_FOR_PS3
    return((Value << 24)               |
           ((Value & 0x0000FF00) << 8) |
           ((Value & 0x00FF0000) >> 8) |
           (Value >> 24));
#else
    return Value;
#endif
}

inline granny_uint16 Reverse16(granny_uint16 Value)
{
#if COMPILE_FOR_PS3
    return(granny_uint16)(((Value & (granny_uint16)0x00FF) << 8) |
                          ((Value & (granny_uint16)0xFF00) >> 8));
#else
    return Value;
#endif
}

void Reverse32(int Count, granny_uint32* Buffer)
{
    while (Count--)
    {
        *Buffer = Reverse32(*Buffer);
        ++Buffer;
    }
}

void Reverse16(int Count, granny_uint16* Buffer)
{
    while (Count--)
    {
        *Buffer = Reverse16(*Buffer);
        ++Buffer;
    }
}


bool Convert16BitLinearCurve(int                         CurveDegree,
                             granny_uint16 const*        Knots,
                             granny_int32                KnotCount,
                             granny_uint16 const*        Controls,
                             granny_int32x               ControlDim,
                             granny_int32x               ScaleOffsetCount,
                             granny_real32 const*        ScaleOffsets,
                             granny_real32               OneOverKnotScale,
                             granny_spu_replication_type ReplType,
                             vector<granny_uint8>&       CurveBuffer)
{
    int TotalSize = sizeof(granny_spu_curve_header_quantized);

    // Each dimension has a scale/offset pair that directly follows the header.
    int ScaleOffsetEntries = TotalSize;
    TotalSize += sizeof(granny_real32) * ScaleOffsetCount * 2;

    // We need degree extra knots on either side of the curve.  Do this a little
    // oddly, since we have to get the offset to the real knots.
    int KnotControlStart = TotalSize;
    TotalSize += CurveDegree * sizeof(granny_uint16);
    int RealKnotStart = TotalSize;
    TotalSize += (KnotCount + CurveDegree) * sizeof(granny_uint16);

    // Account for the controls
    int ControlStart = TotalSize;
    TotalSize += sizeof(granny_uint16) * KnotCount * ControlDim;

    int TotalKnotsAndControls = ((CurveDegree * 2 + KnotCount) +
                                 KnotCount * ControlDim);

    // And align the final size to 16 bytes...
    TotalSize = (TotalSize + 15) & ~15;

    // We only allow curve of up to 64k
    if (TotalSize >= (1 << 16))
        return false;

    // Allocate
    CurveBuffer.resize(TotalSize);

    // Create the pointers we'll use in the creation process
    granny_spu_curve_header_quantized* CurveHeader = (granny_spu_curve_header_quantized*)&CurveBuffer[0];
    granny_real32* RealScaleOffsets = (granny_real32*)&CurveBuffer[ScaleOffsetEntries];
    granny_uint16* RealKnots = (granny_uint16*)&CurveBuffer[RealKnotStart];
    granny_uint16* RealControls =  (granny_uint16*)&CurveBuffer[ControlStart];

    memcpy(RealScaleOffsets, ScaleOffsets, sizeof(granny_real32) * ScaleOffsetCount * 2);
    memcpy(RealKnots, Knots, KnotCount * sizeof(granny_uint16));
    memcpy(RealControls, Controls, KnotCount*ControlDim*sizeof(granny_uint16));

    // Replicate the correct knot values for looping behavior
    {
        granny_uint16  Offset   = 65535 - RealKnots[KnotCount - 1];
        granny_uint16* CopyFrom = &RealKnots[KnotCount - 2];
        granny_uint16* CopyTo   = RealKnots - 1;
        {for(int Idx = 0; Idx < CurveDegree; ++Idx)
        {
            *CopyTo = (*CopyFrom) + Offset;
            --CopyFrom;
            --CopyTo;
        }}

        CopyFrom = RealKnots+1;
        pp_assert(RealKnots[0] == 0);
        CopyTo   = RealKnots + KnotCount;
        {for(int Idx = 0; Idx < CurveDegree; ++Idx)
        {
            *CopyTo = *CopyFrom;
            ++CopyFrom;
            ++CopyTo;
        }}
    }

    // Here's an annoyance.  We're not describing this with Granny data_type_definition
    // structures for the moment, but we need to endian reverse this.
    {
        // Now setup the header
        CurveHeader->BasicHeader.TotalSize = Reverse16((granny_uint16)TotalSize);
        CurveHeader->BasicHeader.CurveType = Reverse16(GrannySPUCurveTypeK16);
        CurveHeader->BasicHeader.Degree    = (granny_uint8)CurveDegree;
        CurveHeader->BasicHeader.Dimension = (granny_uint8)ControlDim;
        CurveHeader->BasicHeader.KnotCount = Reverse16((granny_uint16)KnotCount);
        CurveHeader->BasicHeader.KnotStart = Reverse16((granny_uint16)RealKnotStart);
        CurveHeader->BasicHeader.ControlStart  = Reverse16((granny_uint16)ControlStart);
        CurveHeader->BasicHeader.ControlStride = 0;
        CurveHeader->OneOverKnotScale = OneOverKnotScale;

        CurveHeader->BasicHeader.ReplicationType = Reverse16((granny_uint16)ReplType);

        // Reverse the knot scale
        Reverse32(1, (granny_uint32*)&CurveHeader->OneOverKnotScale);

        // Reverse the ScaleOffsets
        Reverse32(ScaleOffsetCount*2, (granny_uint32*)RealScaleOffsets);

        // Knots/controls are uint16s, reverse them...
        Reverse16(TotalKnotsAndControls, (granny_uint16*)&CurveBuffer[KnotControlStart]);
    }

    return true;
}

bool Convert8BitLinearCurve(int                         CurveDegree,
                            granny_uint8 const*         Knots,
                            granny_int32                KnotCount,
                            granny_uint8 const*         Controls,
                            granny_int32x               ControlDim,
                            granny_int32x               ScaleOffsetCount,
                            granny_real32 const*        ScaleOffsets,
                            granny_real32               OneOverKnotScale,
                            granny_spu_replication_type ReplType,
                            vector<granny_uint8>&       CurveBuffer)
{
    int TotalSize = sizeof(granny_spu_curve_header_quantized);

    // Each dimension has a scale/offset pair that directly follows the header.
    int ScaleOffsetEntries = TotalSize;
    TotalSize += sizeof(granny_real32) * ScaleOffsetCount * 2;

    // We need degree extra knots on either side of the curve.  Do this a little
    // oddly, since we have to get the offset to the real knots.
    TotalSize += CurveDegree * sizeof(granny_uint8);
    int RealKnotStart = TotalSize;
    TotalSize += (KnotCount + CurveDegree) * sizeof(granny_uint8);

    // Account for the controls
    int ControlStart = TotalSize;
    TotalSize += sizeof(granny_uint8) * KnotCount * ControlDim;

    // And align the final size to 16 bytes...
    TotalSize = (TotalSize + 15) & ~15;

    // We only allow curve of up to 64k
    if (TotalSize >= (1 << 16))
        return false;

    // Allocate
    CurveBuffer.resize(TotalSize);

    // Create the pointers we'll use in the creation process
    granny_spu_curve_header_quantized* CurveHeader = (granny_spu_curve_header_quantized*)&CurveBuffer[0];
    granny_real32* RealScaleOffsets = (granny_real32*)&CurveBuffer[ScaleOffsetEntries];
    granny_uint8* RealKnots = (granny_uint8*)&CurveBuffer[RealKnotStart];
    granny_uint8* RealControls =  (granny_uint8*)&CurveBuffer[ControlStart];

    memcpy(RealScaleOffsets, ScaleOffsets, sizeof(granny_real32) * ScaleOffsetCount * 2);
    memcpy(RealKnots, Knots, KnotCount * sizeof(granny_uint8));
    memcpy(RealControls, Controls, KnotCount*ControlDim*sizeof(granny_uint8));

    // Replicate the correct knot values for looping behavior
    {
        granny_uint8* CopyFrom = &RealKnots[KnotCount - 1];
        granny_uint8* CopyTo   = RealKnots - 1;
        {for(int Idx = 0; Idx < CurveDegree; ++Idx)
        {
            *CopyTo = *CopyFrom;
            --CopyFrom;
            --CopyTo;
        }}

        CopyFrom = RealKnots+1;
        pp_assert(RealKnots[0] == 0);
        CopyTo   = RealKnots + KnotCount;
        {for(int Idx = 0; Idx < CurveDegree; ++Idx)
        {
            *CopyTo = *CopyFrom;
            ++CopyFrom;
            ++CopyTo;
        }}
    }

    // Here's an annoyance.  We're not describing this with Granny data_type_definition
    // structures for the moment, but we need to endian reverse this.
    {
        // Now setup the header
        CurveHeader->BasicHeader.TotalSize = Reverse16((granny_uint16)TotalSize);
        CurveHeader->BasicHeader.CurveType = Reverse16(GrannySPUCurveTypeK8);
        CurveHeader->BasicHeader.Degree    = (granny_uint8)CurveDegree;
        CurveHeader->BasicHeader.Dimension = (granny_uint8)ControlDim;
        CurveHeader->BasicHeader.KnotCount = Reverse16((granny_uint16)KnotCount);
        CurveHeader->BasicHeader.KnotStart = Reverse16((granny_uint16)RealKnotStart);
        CurveHeader->BasicHeader.ControlStart  = Reverse16((granny_uint16)ControlStart);
        CurveHeader->BasicHeader.ControlStride = 0;
        CurveHeader->OneOverKnotScale = OneOverKnotScale;

        CurveHeader->BasicHeader.ReplicationType = Reverse16((granny_uint16)ReplType);

        // Reverse the knot scale
        Reverse32(1, (granny_uint32*)&CurveHeader->OneOverKnotScale);

        // Reverse the ScaleOffsets
        Reverse32(ScaleOffsetCount*2, (granny_uint32*)RealScaleOffsets);

        // Knots/controls are uint8s leave them alone...
    }

    return true;
}


bool ConvertD4nk8u(granny_curve2& OldCurve,
                   track_type TrackType,
                   vector<granny_uint8>& CurveBuffer)
{
    pp_assert(TrackType == Orientation);

    int CurveDegree    = GrannyCurveGetDegree(&OldCurve);
    int CurveDimension = GrannyCurveGetDimension(&OldCurve);
    int CurveKnotCount = GrannyCurveGetKnotCount(&OldCurve);
    pp_assert(CurveDimension == 4);

    granny_curve_data_d4n_k8u_c7u const* CurveObject =
        (granny_curve_data_d4n_k8u_c7u*)OldCurve.CurveData.Object;
    granny_uint8 const* CurveKnots = CurveObject->KnotsControls;
    granny_uint8 const* CurveControls = CurveObject->KnotsControls + CurveKnotCount;

    // Converting a dak32 is pretty easy...
    int TotalSize = sizeof(granny_spu_curve_header_quantized4n);

    // We need degree extra knots on either side of the curve.  Do this a little
    // oddly, since we have to get the offset to the real knots.
    TotalSize += CurveDegree * sizeof(granny_uint8);
    int RealKnotStart = TotalSize;
    TotalSize += (CurveKnotCount + CurveDegree) * sizeof(granny_uint8);

    // Account for the controls
    int ControlStart = TotalSize;
    int ControlCount = GrannyCurveGetKnotCount(&OldCurve);
    TotalSize += sizeof(granny_uint8) * 3 * ControlCount;

    // And align the final size to 16 bytes...
    TotalSize = (TotalSize + 15) & ~15;

    // We only allow curve of up to 64k
    if (TotalSize >= (1 << 16))
        return false;

    // Allocate
    CurveBuffer.resize(TotalSize);

    // Create the pointers we'll use in the creation process
    granny_spu_curve_header_quantized4n* CurveHeader = (granny_spu_curve_header_quantized4n*)&CurveBuffer[0];
    granny_uint8* RealKnots = (granny_uint8*)&CurveBuffer[RealKnotStart];
    granny_uint8* Controls =  (granny_uint8*)&CurveBuffer[ControlStart];

    memcpy(RealKnots, CurveKnots, CurveKnotCount * sizeof(granny_uint8));
    memcpy(Controls, CurveControls, CurveKnotCount * 3 * sizeof(granny_uint8));

    // Extract the knots and controls.  Note that we can fake the ID vector, since we know
    // this isn't an identity curve...

    // Replicate the correct knot values for looping behavior
    {
        granny_uint8  Offset   = 255 - RealKnots[CurveKnotCount - 1];
        granny_uint8* CopyFrom = &RealKnots[CurveKnotCount - 2];
        granny_uint8* CopyTo   = RealKnots - 1;
        {for(int Idx = 0; Idx < CurveDegree; ++Idx)
        {
            *CopyTo = (*CopyFrom) + Offset;
            --CopyFrom;
            --CopyTo;
        }}

        CopyFrom = RealKnots+1;
        pp_assert(RealKnots[0] == 0);
        CopyTo   = RealKnots + CurveKnotCount;
        {for(int Idx = 0; Idx < CurveDegree; ++Idx)
        {
            *CopyTo = *CopyFrom;
            ++CopyFrom;
            ++CopyTo;
        }}
    }

    // Here's an annoyance.  We're not describing this with Granny data_type_definition
    // structures for the moment, but we need to endian reverse this.
    {
        // Now setup the header
        CurveHeader->BasicHeader.TotalSize = Reverse16((granny_uint16)TotalSize);
        CurveHeader->BasicHeader.CurveType = Reverse16(GrannySPUCurveType4nK8);
        CurveHeader->BasicHeader.Degree    = (granny_uint8)CurveDegree;
        CurveHeader->BasicHeader.Dimension = (granny_uint8)CurveDimension;
        CurveHeader->BasicHeader.KnotCount = Reverse16((granny_uint16)CurveKnotCount);
        CurveHeader->BasicHeader.KnotStart = Reverse16((granny_uint16)RealKnotStart);
        CurveHeader->BasicHeader.ControlStart  = Reverse16((granny_uint16)ControlStart);
        CurveHeader->BasicHeader.ControlStride = 0;
        CurveHeader->ScaleOffsetTableEntries   = Reverse32(CurveObject->ScaleOffsetTableEntries);
        CurveHeader->OneOverKnotScale          = CurveObject->OneOverKnotScale;

        CurveHeader->BasicHeader.ReplicationType = Reverse16(GrannySPUReplicationNormOri);

        // Reverse the knot scale
        Reverse32(1, (granny_uint32*)&CurveHeader->OneOverKnotScale);

        // Knots are uint8s, nothing to do here...
    }

    return true;
}


bool ConvertD4nk16u(granny_curve2& OldCurve,
                    track_type TrackType,
                    vector<granny_uint8>& CurveBuffer)
{
    pp_assert(TrackType == Orientation);

    int CurveDegree    = GrannyCurveGetDegree(&OldCurve);
    int CurveDimension = GrannyCurveGetDimension(&OldCurve);
    int CurveKnotCount = GrannyCurveGetKnotCount(&OldCurve);
    pp_assert(CurveDimension == 4);

    granny_curve_data_d4n_k16u_c15u const* CurveObject =
        (granny_curve_data_d4n_k16u_c15u*)OldCurve.CurveData.Object;
    granny_uint16 const* CurveKnots = CurveObject->KnotsControls;
    granny_uint16 const* CurveControls = CurveObject->KnotsControls + CurveKnotCount;

    int TotalSize = sizeof(granny_spu_curve_header_quantized4n);

    // We need degree extra knots on either side of the curve.  Do this a little
    // oddly, since we have to get the offset to the real knots.
    int KnotControlStart = TotalSize;
    TotalSize += CurveDegree * sizeof(granny_uint16);
    int RealKnotStart = TotalSize;
    TotalSize += (CurveKnotCount + CurveDegree) * sizeof(granny_uint16);

    // Account for the controls
    int ControlStart = TotalSize;
    int ControlCount = GrannyCurveGetKnotCount(&OldCurve);
    TotalSize += sizeof(granny_uint16) * 3 * ControlCount;

    int TotalKnotsAndControls = ((CurveDegree * 2 + CurveKnotCount) + CurveKnotCount * 3);

    // And align the final size to 16 bytes...
    TotalSize = (TotalSize + 15) & ~15;

    // We only allow curve of up to 64k
    if (TotalSize >= (1 << 16))
        return false;

    // Allocate
    CurveBuffer.resize(TotalSize);

    // Create the pointers we'll use in the creation process
    granny_spu_curve_header_quantized4n* CurveHeader = (granny_spu_curve_header_quantized4n*)&CurveBuffer[0];
    granny_uint16* RealKnots = (granny_uint16*)&CurveBuffer[RealKnotStart];
    granny_uint16* Controls =  (granny_uint16*)&CurveBuffer[ControlStart];

    memcpy(RealKnots, CurveKnots, CurveKnotCount * sizeof(granny_uint16));
    memcpy(Controls, CurveControls, CurveKnotCount * 3 * sizeof(granny_uint16));

    // Extract the knots and controls.  Note that we can fake the ID vector, since we know
    // this isn't an identity curve...

    // Replicate the correct knot values for looping behavior
    {
        granny_uint16  Offset   = 65535 - RealKnots[CurveKnotCount - 1];
        granny_uint16* CopyFrom = &RealKnots[CurveKnotCount - 2];
        granny_uint16* CopyTo   = RealKnots - 1;
        {for(int Idx = 0; Idx < CurveDegree; ++Idx)
        {
            *CopyTo = (*CopyFrom) + Offset;
            --CopyFrom;
            --CopyTo;
        }}

        CopyFrom = RealKnots+1;
        pp_assert(RealKnots[0] == 0);
        CopyTo   = RealKnots + CurveKnotCount;
        {for(int Idx = 0; Idx < CurveDegree; ++Idx)
        {
            *CopyTo = *CopyFrom;
            ++CopyFrom;
            ++CopyTo;
        }}
    }

    // Here's an annoyance.  We're not describing this with Granny data_type_definition
    // structures for the moment, but we need to endian reverse this.
    {
        // Now setup the header
        CurveHeader->BasicHeader.TotalSize = Reverse16((granny_uint16)TotalSize);
        CurveHeader->BasicHeader.CurveType = Reverse16(GrannySPUCurveType4nK16);
        CurveHeader->BasicHeader.Degree    = (granny_uint8)CurveDegree;
        CurveHeader->BasicHeader.Dimension = (granny_uint8)CurveDimension;
        CurveHeader->BasicHeader.KnotCount = Reverse16((granny_uint16)CurveKnotCount);
        CurveHeader->BasicHeader.KnotStart = Reverse16((granny_uint16)RealKnotStart);
        CurveHeader->BasicHeader.ControlStart  = Reverse16((granny_uint16)ControlStart);
        CurveHeader->BasicHeader.ControlStride = 0;
        CurveHeader->ScaleOffsetTableEntries   = Reverse32(CurveObject->ScaleOffsetTableEntries);
        CurveHeader->OneOverKnotScale          = CurveObject->OneOverKnotScale;

        CurveHeader->BasicHeader.ReplicationType = Reverse16(GrannySPUReplicationNormOri);

        // Reverse the knot scale
        Reverse32(1, (granny_uint32*)&CurveHeader->OneOverKnotScale);

        // Knots/controls are uint16s, reverse them...
        Reverse16(TotalKnotsAndControls, (granny_uint16*)&CurveBuffer[KnotControlStart]);
    }

    return true;
}


bool ConvertDak16u(granny_curve2& OldCurve,
                   track_type TrackType,
                   vector<granny_uint8>& CurveBuffer)
{
    int CurveDegree    = GrannyCurveGetDegree(&OldCurve);
    int CurveDimension = GrannyCurveGetDimension(&OldCurve);
    int CurveKnotCount = GrannyCurveGetKnotCount(&OldCurve);

    granny_curve_data_da_k16u_c16u const* CurveObject =
        (granny_curve_data_da_k16u_c16u*)OldCurve.CurveData.Object;
    granny_uint16 const* CurveKnots = CurveObject->KnotsControls;
    granny_uint16 const* CurveControls = CurveObject->KnotsControls + CurveKnotCount;
    granny_real32* CurveScaleOffsets = CurveObject->ControlScaleOffsets;

    granny_real32 OneOverKnotScale;
    ExtractTruncatedOneOverKnotScale(&OneOverKnotScale,
                                     CurveObject->OneOverKnotScaleTrunc);

    granny_spu_replication_type ReplType;
    switch (TrackType)
    {
        case Position:
            pp_assert(CurveDimension == 3);
            ReplType = GrannySPUReplicationRaw;
            break;

        case Orientation:
            pp_assert(CurveDimension == 4);
            ReplType = GrannySPUReplicationNormOri;
            break;

        case ScaleShear:
            pp_assert(CurveDimension == 9);
            ReplType = GrannySPUReplicationRaw;
            break;

        default:
            pp_assert(false);
            return false;
    }

    return Convert16BitLinearCurve(CurveDegree,
                                   CurveKnots,
                                   CurveKnotCount,
                                   CurveControls,
                                   CurveDimension,
                                   CurveDimension,
                                   CurveScaleOffsets,
                                   OneOverKnotScale,
                                   ReplType,
                                   CurveBuffer);
}

bool ConvertD3k16u(granny_curve2& OldCurve,
                   track_type TrackType,
                   vector<granny_uint8>& CurveBuffer)
{
    int CurveDegree    = GrannyCurveGetDegree(&OldCurve);
    int CurveDimension = GrannyCurveGetDimension(&OldCurve);
    int CurveKnotCount = GrannyCurveGetKnotCount(&OldCurve);
    pp_assert(CurveDimension == 3);

    granny_curve_data_d3_k16u_c16u const* CurveObject =
        (granny_curve_data_d3_k16u_c16u*)OldCurve.CurveData.Object;
    granny_uint16 const* CurveKnots    = CurveObject->KnotsControls;
    granny_uint16 const* CurveControls = CurveObject->KnotsControls + CurveKnotCount;

    // Collapse to sososos
    granny_real32 CurveScaleOffsets[6] = {
        CurveObject->ControlScales[0], CurveObject->ControlOffsets[0],
        CurveObject->ControlScales[1], CurveObject->ControlOffsets[1],
        CurveObject->ControlScales[2], CurveObject->ControlOffsets[2]
    };

    granny_real32 OneOverKnotScale;
    ExtractTruncatedOneOverKnotScale(&OneOverKnotScale,
                                     CurveObject->OneOverKnotScaleTrunc);

    pp_assert(TrackType == Position);
    granny_spu_replication_type ReplType = GrannySPUReplicationRaw;

    return Convert16BitLinearCurve(CurveDegree,
                                   CurveKnots,
                                   CurveKnotCount,
                                   CurveControls,
                                   CurveDimension,
                                   CurveDimension,
                                   CurveScaleOffsets,
                                   OneOverKnotScale,
                                   ReplType,
                                   CurveBuffer);
}


bool ConvertD3I1k16u(granny_curve2& OldCurve,
                     track_type TrackType,
                     vector<granny_uint8>& CurveBuffer)
{
    int CurveDegree    = GrannyCurveGetDegree(&OldCurve);
    int CurveDimension = GrannyCurveGetDimension(&OldCurve);
    int CurveKnotCount = GrannyCurveGetKnotCount(&OldCurve);
    pp_assert(CurveDimension == 3);
	pp_unused(CurveDimension);

    granny_curve_data_d3i1_k16u_c16u const* CurveObject =
        (granny_curve_data_d3i1_k16u_c16u*)OldCurve.CurveData.Object;
    granny_uint16 const* CurveKnots    = CurveObject->KnotsControls;
    granny_uint16 const* CurveControls = CurveObject->KnotsControls + CurveKnotCount;

    // Collapse to sososos
    granny_real32 CurveScaleOffsets[6] = {
        CurveObject->ControlScales[0], CurveObject->ControlOffsets[0],
        CurveObject->ControlScales[1], CurveObject->ControlOffsets[1],
        CurveObject->ControlScales[2], CurveObject->ControlOffsets[2]
    };

    granny_real32 OneOverKnotScale;
    ExtractTruncatedOneOverKnotScale(&OneOverKnotScale,
                                     CurveObject->OneOverKnotScaleTrunc);

    pp_assert(TrackType == Position);
    granny_spu_replication_type ReplType = GrannySPUReplicationRaw;

    return Convert16BitLinearCurve(CurveDegree,
                                   CurveKnots,
                                   CurveKnotCount,
                                   CurveControls,
                                   1,
                                   3,
                                   CurveScaleOffsets,
                                   OneOverKnotScale,
                                   ReplType,
                                   CurveBuffer);
}

bool ConvertD9I3k16u(granny_curve2& OldCurve,
                     track_type TrackType,
                     vector<granny_uint8>& CurveBuffer)
{
    int CurveDegree    = GrannyCurveGetDegree(&OldCurve);
    int CurveDimension = GrannyCurveGetDimension(&OldCurve);
    int CurveKnotCount = GrannyCurveGetKnotCount(&OldCurve);
    pp_assert(CurveDimension == 9);
	pp_unused(CurveDimension);

    granny_curve_data_d9i3_k16u_c16u const* CurveObject =
        (granny_curve_data_d9i3_k16u_c16u*)OldCurve.CurveData.Object;
    granny_uint16 const* CurveKnots    = CurveObject->KnotsControls;
    granny_uint16 const* CurveControls = CurveObject->KnotsControls + CurveKnotCount;

    // Collapse to sososos
    granny_real32 CurveScaleOffsets[6] = {
        CurveObject->ControlScales[0], CurveObject->ControlOffsets[0],
        CurveObject->ControlScales[1], CurveObject->ControlOffsets[1],
        CurveObject->ControlScales[2], CurveObject->ControlOffsets[2]
    };

    granny_real32 OneOverKnotScale;
    ExtractTruncatedOneOverKnotScale(&OneOverKnotScale,
                                     CurveObject->OneOverKnotScaleTrunc);

    pp_assert(TrackType == ScaleShear);
    granny_spu_replication_type ReplType = GrannySPUReplicationDiagonalSS;

    return Convert16BitLinearCurve(CurveDegree,
                                   CurveKnots,
                                   CurveKnotCount,
                                   CurveControls,
                                   3,
                                   3,
                                   CurveScaleOffsets,
                                   OneOverKnotScale,
                                   ReplType,
                                   CurveBuffer);
}


bool ConvertDak8u(granny_curve2& OldCurve,
                  track_type TrackType,
                  vector<granny_uint8>& CurveBuffer)
{
    int CurveDegree    = GrannyCurveGetDegree(&OldCurve);
    int CurveDimension = GrannyCurveGetDimension(&OldCurve);
    int CurveKnotCount = GrannyCurveGetKnotCount(&OldCurve);

    granny_curve_data_da_k8u_c8u const* CurveObject =
        (granny_curve_data_da_k8u_c8u*)OldCurve.CurveData.Object;
    granny_uint8 const* CurveKnots = CurveObject->KnotsControls;
    granny_uint8 const* CurveControls = CurveObject->KnotsControls + CurveKnotCount;
    granny_real32* CurveScaleOffsets = CurveObject->ControlScaleOffsets;

    granny_real32 OneOverKnotScale;
    ExtractTruncatedOneOverKnotScale(&OneOverKnotScale,
                                     CurveObject->OneOverKnotScaleTrunc);

    granny_spu_replication_type ReplType;
    switch (TrackType)
    {
        case Position:
            pp_assert(CurveDimension == 3);
            ReplType = GrannySPUReplicationRaw;
            break;

        case Orientation:
            pp_assert(CurveDimension == 4);
            ReplType = GrannySPUReplicationNormOri;
            break;

        case ScaleShear:
            pp_assert(CurveDimension == 9);
            ReplType = GrannySPUReplicationRaw;
            break;

        default:
            pp_assert(false);
            return false;
    }

    return Convert8BitLinearCurve(CurveDegree,
                                  CurveKnots,
                                  CurveKnotCount,
                                  CurveControls,
                                  CurveDimension,
                                  CurveDimension,
                                  CurveScaleOffsets,
                                  OneOverKnotScale,
                                  ReplType,
                                  CurveBuffer);
}

bool ConvertD3k8u(granny_curve2& OldCurve,
                  track_type TrackType,
                  vector<granny_uint8>& CurveBuffer)
{
    int CurveDegree    = GrannyCurveGetDegree(&OldCurve);
    int CurveDimension = GrannyCurveGetDimension(&OldCurve);
    int CurveKnotCount = GrannyCurveGetKnotCount(&OldCurve);
    pp_assert(CurveDimension == 3);

    granny_curve_data_d3_k8u_c8u const* CurveObject =
        (granny_curve_data_d3_k8u_c8u*)OldCurve.CurveData.Object;
    granny_uint8 const* CurveKnots    = CurveObject->KnotsControls;
    granny_uint8 const* CurveControls = CurveObject->KnotsControls + CurveKnotCount;

    // Collapse to sososos
    granny_real32 CurveScaleOffsets[6] = {
        CurveObject->ControlScales[0], CurveObject->ControlOffsets[0],
        CurveObject->ControlScales[1], CurveObject->ControlOffsets[1],
        CurveObject->ControlScales[2], CurveObject->ControlOffsets[2]
    };

    granny_real32 OneOverKnotScale;
    ExtractTruncatedOneOverKnotScale(&OneOverKnotScale,
                                     CurveObject->OneOverKnotScaleTrunc);

    pp_assert(TrackType == Position);
    granny_spu_replication_type ReplType = GrannySPUReplicationRaw;

    return Convert8BitLinearCurve(CurveDegree,
                                  CurveKnots,
                                  CurveKnotCount,
                                  CurveControls,
                                  CurveDimension,
                                  CurveDimension,
                                  CurveScaleOffsets,
                                  OneOverKnotScale,
                                  ReplType,
                                  CurveBuffer);
}


bool ConvertD3I1k8u(granny_curve2& OldCurve,
                    track_type TrackType,
                    vector<granny_uint8>& CurveBuffer)
{
    int CurveDegree    = GrannyCurveGetDegree(&OldCurve);
    int CurveDimension = GrannyCurveGetDimension(&OldCurve);
    int CurveKnotCount = GrannyCurveGetKnotCount(&OldCurve);
    pp_assert(CurveDimension == 3);
	pp_unused(CurveDimension);

    granny_curve_data_d3i1_k8u_c8u const* CurveObject =
        (granny_curve_data_d3i1_k8u_c8u*)OldCurve.CurveData.Object;
    granny_uint8 const* CurveKnots    = CurveObject->KnotsControls;
    granny_uint8 const* CurveControls = CurveObject->KnotsControls + CurveKnotCount;

    // Collapse to sososos
    granny_real32 CurveScaleOffsets[6] = {
        CurveObject->ControlScales[0], CurveObject->ControlOffsets[0],
        CurveObject->ControlScales[1], CurveObject->ControlOffsets[1],
        CurveObject->ControlScales[2], CurveObject->ControlOffsets[2]
    };

    granny_real32 OneOverKnotScale;
    ExtractTruncatedOneOverKnotScale(&OneOverKnotScale,
                                     CurveObject->OneOverKnotScaleTrunc);

    pp_assert(TrackType == Position);
    granny_spu_replication_type ReplType = GrannySPUReplicationRaw;

    return Convert8BitLinearCurve(CurveDegree,
                                  CurveKnots,
                                  CurveKnotCount,
                                  CurveControls,
                                  1,
                                  3,
                                  CurveScaleOffsets,
                                  OneOverKnotScale,
                                  ReplType,
                                  CurveBuffer);
}

bool ConvertD9I3k8u(granny_curve2& OldCurve,
                    track_type TrackType,
                    vector<granny_uint8>& CurveBuffer)
{
    int CurveDegree    = GrannyCurveGetDegree(&OldCurve);
    int CurveDimension = GrannyCurveGetDimension(&OldCurve);
    int CurveKnotCount = GrannyCurveGetKnotCount(&OldCurve);
    pp_assert(CurveDimension == 9);
	pp_unused(CurveDimension);

    granny_curve_data_d9i3_k8u_c8u const* CurveObject =
        (granny_curve_data_d9i3_k8u_c8u*)OldCurve.CurveData.Object;
    granny_uint8 const* CurveKnots    = CurveObject->KnotsControls;
    granny_uint8 const* CurveControls = CurveObject->KnotsControls + CurveKnotCount;

    // Collapse to sososos
    granny_real32 CurveScaleOffsets[6] = {
        CurveObject->ControlScales[0], CurveObject->ControlOffsets[0],
        CurveObject->ControlScales[1], CurveObject->ControlOffsets[1],
        CurveObject->ControlScales[2], CurveObject->ControlOffsets[2]
    };

    granny_real32 OneOverKnotScale;
    ExtractTruncatedOneOverKnotScale(&OneOverKnotScale,
                                     CurveObject->OneOverKnotScaleTrunc);

    pp_assert(TrackType == ScaleShear);
    granny_spu_replication_type ReplType = GrannySPUReplicationDiagonalSS;

    return Convert8BitLinearCurve(CurveDegree,
                                  CurveKnots,
                                  CurveKnotCount,
                                  CurveControls,
                                  3,
                                  3,
                                  CurveScaleOffsets,
                                  OneOverKnotScale,
                                  ReplType,
                                  CurveBuffer);
}


bool ConvertFloatLinearCurve(int                         CurveDegree,
                             granny_real32 const*        Knots,
                             granny_int32                KnotCount,
                             granny_real32 const*        Controls,
                             granny_int32x               ControlDim,
                             granny_int32x               ScaleOffsetCount,
                             granny_real32 const*        ScaleOffsets,
                             granny_spu_replication_type ReplType,
                             vector<granny_uint8>&       CurveBuffer)
{
    int TotalSize = sizeof(granny_spu_curve_header_basic);

    // It's possible that there are scale/offset entries following the header
    int ScaleOffsetEntries = TotalSize;
    TotalSize += sizeof(granny_real32) * ScaleOffsetCount * 2;

    // We need degree extra knots on either side of the curve.  Do this a little
    // oddly, since we have to get the offset to the real knots.
    int KnotControlStart = TotalSize;
    TotalSize += CurveDegree * sizeof(granny_real32);
    int RealKnotStart = TotalSize;
    TotalSize += (KnotCount + CurveDegree) * sizeof(granny_real32);

    // Account for the controls
    int ControlStart = TotalSize;
    TotalSize += sizeof(granny_real32) * KnotCount * ControlDim;

    int TotalKnotsAndControls = ((CurveDegree * 2 + KnotCount) +
                                 KnotCount * ControlDim);

    // And align the final size to 16 bytes...
    TotalSize = (TotalSize + 15) & ~15;

    // We only allow curve of up to 64k
    if (TotalSize >= (1 << 16))
        return false;

    // Allocate
    CurveBuffer.resize(TotalSize);

    // Create the pointers we'll use in the creation process
    granny_spu_curve_header_basic* CurveHeader = (granny_spu_curve_header_basic*)&CurveBuffer[0];
    granny_real32* RealScaleOffsets = (granny_real32*)&CurveBuffer[ScaleOffsetEntries];
    granny_real32* RealKnots        = (granny_real32*)&CurveBuffer[RealKnotStart];
    granny_real32* RealControls     = (granny_real32*)&CurveBuffer[ControlStart];

    memcpy(RealScaleOffsets, ScaleOffsets, sizeof(granny_real32) * ScaleOffsetCount * 2);
    memcpy(RealKnots, Knots, KnotCount * sizeof(granny_real32));
    memcpy(RealControls, Controls, KnotCount*ControlDim*sizeof(granny_real32));

    // Replicate the correct knot values for looping behavior
    {
        granny_real32 Base      = RealKnots[0];
        granny_real32 Offset    = RealKnots[KnotCount - 1];
        granny_real32* CopyFrom = &RealKnots[KnotCount - 2];
        granny_real32* CopyTo   = RealKnots - 1;
        {for(int Idx = 0; Idx < CurveDegree; ++Idx)
        {
            *CopyTo = Base + (*CopyFrom - Offset);
            --CopyFrom;
            --CopyTo;
        }}

        Base     = RealKnots[KnotCount - 1];
        Offset   = RealKnots[0];
        CopyFrom = RealKnots+1;
        CopyTo   = RealKnots + KnotCount;
        {for(int Idx = 0; Idx < CurveDegree; ++Idx)
        {
            *CopyTo = Base + (*CopyFrom - Offset);
            ++CopyFrom;
            ++CopyTo;
        }}
    }

    // Here's an annoyance.  We're not describing this with Granny data_type_definition
    // structures for the moment, but we need to endian reverse this.
    {
        // Now setup the header
        CurveHeader->TotalSize = Reverse16((granny_uint16)TotalSize);
        CurveHeader->CurveType = Reverse16(GrannySPUCurveTypeReal32);
        CurveHeader->Degree    = (granny_uint8)CurveDegree;
        CurveHeader->Dimension = (granny_uint8)ControlDim;
        CurveHeader->KnotCount = Reverse16((granny_uint16)KnotCount);
        CurveHeader->KnotStart = Reverse16((granny_uint16)RealKnotStart);
        CurveHeader->ControlStart  = Reverse16((granny_uint16)ControlStart);
        CurveHeader->ControlStride = 0;
        CurveHeader->ReplicationType = Reverse16((granny_uint16)ReplType);

        // Reverse the ScaleOffsets
        Reverse32(ScaleOffsetCount*2, (granny_uint32*)RealScaleOffsets);

        // Knots/controls are uint16s, reverse them...
        Reverse32(TotalKnotsAndControls, (granny_uint32*)&CurveBuffer[KnotControlStart]);
    }

    return true;
}


bool ConvertDak32f(granny_curve2& OldCurve,
                   track_type TrackType,
                   vector<granny_uint8>& CurveBuffer)
{
    int CurveDegree    = GrannyCurveGetDegree(&OldCurve);
    int CurveDimension = GrannyCurveGetDimension(&OldCurve);

    granny_spu_replication_type ReplicationType;
    switch (TrackType)
    {
        case Position:
            ReplicationType = GrannySPUReplicationRaw;
            break;
        case Orientation:
            ReplicationType = GrannySPUReplicationNormOri;
            break;
        case ScaleShear:
            ReplicationType = GrannySPUReplicationRaw;
            break;

        default: pp_assert(false);
            return false;
    }

    granny_curve_data_da_k32f_c32f* CurveData =
        (granny_curve_data_da_k32f_c32f*)OldCurve.CurveData.Object;
    return ConvertFloatLinearCurve(CurveDegree,
                                   CurveData->Knots,
                                   CurveData->KnotCount,
                                   CurveData->Controls,
                                   CurveDimension,
                                   0, NULL,
                                   ReplicationType, CurveBuffer);
}

// Handle this curve by turning it into a quantized 16-bit version.  Should never have
// existed in the first place, frankly.
bool ConvertD3I1k32f(granny_curve2& OldCurve,
                     track_type TrackType,
                     vector<granny_uint8>& CurveBuffer)
{
    pp_assert(TrackType == Position);

    granny_curve_data_d3i1_k32f_c32f* CurveData =
        (granny_curve_data_d3i1_k32f_c32f*)OldCurve.CurveData.Object;

    // Collapse to sososos
    granny_real32 CurveScaleOffsets[6] = {
        CurveData->ControlScales[0], CurveData->ControlOffsets[0],
        CurveData->ControlScales[1], CurveData->ControlOffsets[1],
        CurveData->ControlScales[2], CurveData->ControlOffsets[2]
    };

    int CurveDegree = GrannyCurveGetDegree(&OldCurve);
    return ConvertFloatLinearCurve(CurveDegree,
                                   CurveData->KnotsControls,
                                   CurveData->KnotControlCount/2,
                                   CurveData->KnotsControls + (CurveData->KnotControlCount/2),
                                   1,
                                   3, CurveScaleOffsets,
                                   GrannySPUReplicationRaw, CurveBuffer);
}

static const char* HelpString =
    (" MakeSPUAnimation converts a normal Granny animation into an\n"
     " efficient layout for DMA transfer to the SPUs on the PS3 platform.\n"
     " Mail us at granny3@rad for more details.\n"
     "\n"
     "    preprocessor MakeSPUAnimation anim.gr2 -output anim_spu.gr2\n");


static CommandRegistrar RegMakeSPUAnimation(eSingleFile_NoMultiRun,
                                            MakeSPUAnimation,
                                            "MakeSPUAnimation",
                                            "Convert a normal Granny animation into an animation that SPUs can sample.",
                                            HelpString);

