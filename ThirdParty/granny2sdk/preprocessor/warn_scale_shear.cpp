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


void IssueWarning(char const* Filename,
                  char const* Type,
                  char const* Name,
                  char const* Subname)
{
    fprintf(stderr, "%s: %s \"%s\" (%s) contains non-uniform scale/shear\n",
            Filename, Type, Name, Subname);
}


bool ExamineFileInfo(granny_file_info* Info,
                     char const* Filename)
{
    bool ComponentsValid = true;

    // Loop over all skeletons
    {for(granny_int32x Idx = 0; Idx < Info->SkeletonCount; ++Idx)
    {
        granny_skeleton* Skeleton = Info->Skeletons[Idx];
        if (!Skeleton)  // odd, but allowed
            continue;

        {for(granny_int32x BoneIdx = 0; BoneIdx < Skeleton->BoneCount; ++BoneIdx)
        {
            granny_bone& Bone = Skeleton->Bones[BoneIdx];

            if (Bone.LocalTransform.Flags & GrannyHasScaleShear)
            {
                IssueWarning(Filename, "Skeleton", Skeleton->Name, Bone.Name);
                ComponentsValid = false;
            }
        }}
    }}

    // Loop over all track groups
    {for(granny_int32x Idx = 0; Idx < Info->TrackGroupCount; ++Idx)
    {
        granny_track_group* TrackGroup = Info->TrackGroups[Idx];
        if (!TrackGroup)  // odd, but allowed
            continue;

        {for(granny_int32x XformTrackIdx = 0;
             XformTrackIdx < TrackGroup->TransformTrackCount;
             ++XformTrackIdx)
        {
            granny_transform_track& Track = TrackGroup->TransformTracks[XformTrackIdx];

            if (!GrannyCurveIsIdentity(&Track.ScaleShearCurve))
            {
                IssueWarning(Filename, "TransformTrack", TrackGroup->Name, Track.Name);
                ComponentsValid = false;
            }
        }}
    }}

    return ComponentsValid;
}

bool WarnScaleShear(input_file&     InputFile,
                    key_value_pair* KeyValues,
                    granny_int32x   NumKeyValues,
                    granny_memory_arena*)
{
    granny_file_info* Info = ExtractFileInfo(InputFile);
    if (Info == 0)
    {
        ErrOut("unable to obtain a granny_file_info from %s\n", InputFile.Filename);
        return false;
    }

    return ExamineFileInfo(Info, InputFile.Filename);
}

static const char* HelpString =
    (" WarnScaleShear will look at all Granny transforms in the file (Bones,\n"
     " animations, etc.), and return an error if any of them contain scale\n"
     " or shear components other than identity.  This is primarily an example\n"
     " of how to create a preprocessor command for data verification\n"
     "\n"
     "   preprocessor WarnScaleShear file.gr2\n");

static CommandRegistrar RegWarnScaleShear(eSingleFile_MultiRun,
                                          WarnScaleShear,
                                          "WarnScaleShear",
                                          "Warns (and returns failure) if any components with non-identity scale/shear are found",
                                          HelpString);

