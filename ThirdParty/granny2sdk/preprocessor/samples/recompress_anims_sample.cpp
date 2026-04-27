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
#include <vector>

using namespace std;

#define ArrayLength(x) (sizeof(x) / sizeof((x)[0]))

// Replace
granny_real32 PositionTolerance    = 0.1f;
granny_real32 OrientationTolerance = GrannyOrientationToleranceFromDegrees(0.1f);  // important! :)
granny_real32 ScaleShearTolerance  = 0.1f;

static granny_data_type_definition* PositionCurveFormats[] = {
    GrannyCurveDataDaK32fC32fType,
    GrannyCurveDataD3I1K8uC8uType,
    GrannyCurveDataD3I1K16uC16uType,
    GrannyCurveDataD3K8uC8uType,
    GrannyCurveDataD3K16uC16uType,
    GrannyCurveDataD3I1K32fC32fType,
};

static granny_data_type_definition* OrientationCurveFormats[] = {
    GrannyCurveDataDaK32fC32fType,
    GrannyCurveDataD4nK8uC7uType,
    GrannyCurveDataD4nK16uC15uType,
};

static granny_data_type_definition *ScaleShearCurveFormats[] = {
    GrannyCurveDataDaK32fC32fType,
    GrannyCurveDataD9I1K8uC8uType,
    GrannyCurveDataD9I3K8uC8uType,
    GrannyCurveDataDaK8uC8uType,
    GrannyCurveDataDaK16uC16uType,
    GrannyCurveDataD9I1K16uC16uType,
    GrannyCurveDataD9I3K16uC16uType,
};


static void
RecompressCurve(char const* ComponentName, granny_real32 Tolerance,
                granny_curve2& Curve, granny_real32 dT, bool AsQuats,
                granny_data_type_definition** CurveFormats, int NumFormats,
                granny_data_type_definition* ConstantType,
                granny_real32* IdentityVector)
{
    pp_assert(GrannyCurveIsKeyframed(&Curve));

    int KnotCount = GrannyCurveGetKnotCount(&Curve);
    int Dimension = GrannyCurveGetDimension(&Curve);

    vector<granny_real32> Data(KnotCount * Dimension);

    GrannyCurveExtractKnotValues(&Curve, 0, KnotCount, 0, &Data[0], GrannyCurveIdentityPosition);

    if( AsQuats)
    {
      GrannyEnsureQuaternionContinuity(KnotCount, &Data[0]);
    }

    granny_int32x SolverFlags =
        (GrannyBSplineSolverExtraDOFKnotZero |
         GrannyBSplineSolverForceEndpointAlignment |
         (AsQuats ? GrannyBSplineSolverEvaluateAsQuaternions : 0));

    granny_compress_curve_parameters Params;
    Params.DesiredDegree = 2;
    Params.AllowDegreeReduction = true;
    Params.AllowReductionOnMissedTolerance = true;
    Params.ErrorTolerance = Tolerance;     // !!!
    Params.C0Threshold = 0.0f;
    Params.C1Threshold = 0.0f;
    Params.PossibleCompressionTypes = CurveFormats;
    Params.PossibleCompressionTypesCount = NumFormats;
    Params.ConstantCompressionType = ConstantType;
    Params.IdentityCompressionType = GrannyCurveDataDaIdentityType;
    Params.IdentityVector = IdentityVector;

    granny_bspline_solver *Solver = GrannyAllocateBSplineSolver(2, KnotCount, Dimension);

    bool AcheivedTol;
    granny_curve2* NewCurve =
        GrannyCompressCurve(Solver, SolverFlags, &Params,
                            &Data[0], Dimension, KnotCount,
                            dT, &AcheivedTol);

    if (NewCurve)
    {
        Curve.CurveData.Type   = NewCurve->CurveData.Type;
        Curve.CurveData.Object = NewCurve->CurveData.Object;
    }
    else
    {
        // note error in compression
    }

    GrannyDeallocateBSplineSolver(Solver);
}

granny_file_info*
RecompressAnims(char const*          OriginalFilename,
                char const*          OutputFilename,
                granny_file_info*    Info,
                key_value_pair*      KeyValues,
                granny_int32x        NumKeyValues,
                granny_memory_arena* TempArena)
{
    {for(int AnimIdx = 0; AnimIdx < Info->AnimationCount; ++AnimIdx)
    {
        granny_animation* Animation = Info->Animations[AnimIdx];
        if (!Animation)
            continue;

        {for(int TGIdx = 0; TGIdx < Animation->TrackGroupCount; ++TGIdx)
        {
            granny_track_group* TrackGroup = Animation->TrackGroups[TGIdx];
            if (!TrackGroup)
                continue;

            {for(int TTIdx = 0; TTIdx < TrackGroup->TransformTrackCount; ++TTIdx)
            {
                granny_transform_track& Track = TrackGroup->TransformTracks[TTIdx];

                // Assumption: either the track is keyframed, or it's already had the
                // compression we want applied to it.  This also prevents us from
                // considering tracks that were reduced in the export process to constant
                // or identity.
                if (GrannyCurveIsKeyframed(&Track.PositionCurve))
                {
                    RecompressCurve("Pos", PositionTolerance,
                                    Track.PositionCurve, Animation->TimeStep, false,
                                    PositionCurveFormats, ArrayLength(PositionCurveFormats),
                                    GrannyCurveDataD3Constant32fType,
                                    GrannyCurveIdentityPosition);
                }

                if (GrannyCurveIsKeyframed(&Track.OrientationCurve))
                {
                    RecompressCurve("Ori", OrientationTolerance,
                                    Track.OrientationCurve, Animation->TimeStep, true,
                                    OrientationCurveFormats, ArrayLength(OrientationCurveFormats),
                                    GrannyCurveDataD4Constant32fType,
                                    GrannyCurveIdentityOrientation);
                }

                if (GrannyCurveIsKeyframed(&Track.ScaleShearCurve))
                {
                    RecompressCurve("ScS", ScaleShearTolerance,
                                    Track.ScaleShearCurve, Animation->TimeStep, false,
                                    ScaleShearCurveFormats, ArrayLength(ScaleShearCurveFormats),
                                    GrannyCurveDataDaConstant32fType,
                                    GrannyCurveIdentityScaleShear);
                }
            }}
        }}
    }}

    return Info;
}

static char const* HelpString =
    (" RecompressAnims is an example command that will load a file with keyframed\n"
     " curves, and recompress them according to pre-decided specifications.\n"
     " It is intended to illustrate the concept only, you should create your own\n"
     " version of the command for production use\n"
     "\n"
     "   preprocessor RecompressAnims anim.gr2 -output anim_proc.gr2\n");


static CommandRegistrar RegRecompressAnims(RecompressAnims, "RecompressAnims",
                                           "Convert keyframe curves to compressed curves",
                                           HelpString);

