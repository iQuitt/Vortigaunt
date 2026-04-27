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
#include <stdlib.h>
#include <string.h>

struct AxisMapping
{
    const char*   AxisString;
    granny_triple Axis;
};
static const AxisMapping AxisMap[] = {
    { "X", { 1, 0, 0 } },
    { "Y", { 0, 1, 0 } },
    { "Z", { 0, 0, 1 } },
    { "PosX", { 1, 0, 0 } },
    { "PosY", { 0, 1, 0 } },
    { "PosZ", { 0, 0, 1 } },
    { "NegX", { -1, 0, 0 } },
    { "NegY", { 0, -1, 0 } },
    { "NegZ", { 0, 0, -1 } },

    { NULL, { 0, 0, 0 } }
};

void FindAxisArg(const char* Arg, granny_triple* Axis)
{
    if (Arg == 0)
        return;

    {for(int i = 0; AxisMap[i].AxisString; ++i)
    {
        if (_stricmp(AxisMap[i].AxisString, Arg) == 0)
        {
            memcpy(Axis, &AxisMap[i].Axis, sizeof(granny_triple));
            return;
        }
    }}

}


granny_file_info*
TransformFile(char const*          OriginalFilename,
              char const*          OutputFilename,
              granny_file_info*    Info,
              key_value_pair*      KeyValues,
              granny_int32x        NumKeyValues,
              granny_memory_arena* TempArena)
{
    // Parse out the coordinate system
    granny_real32 UnitsPerMeter = 1.0f;
    granny_triple RightVector = { 1, 0, 0 };
    granny_triple UpVector    = { 0, 1, 0 };
    granny_triple BackVector  = { 0, 0, 1 };
    granny_triple Origin      = { 0, 0, 0 };
    {
        char const* UPMArg = FindFirstValueForKey(KeyValues, NumKeyValues, "unitsPerMeter");
        if (UPMArg != 0)
        {
            UnitsPerMeter = granny_real32(atof(UPMArg));
            if (UnitsPerMeter <= 0.0f)
            {
                ErrOut("units per meter must be > 0 (%f)\n", UnitsPerMeter);
                return false;
            }
        }

        FindAxisArg(FindFirstValueForKey(KeyValues, NumKeyValues, "right"), &RightVector);
        FindAxisArg(FindFirstValueForKey(KeyValues, NumKeyValues, "up"),    &UpVector);
        FindAxisArg(FindFirstValueForKey(KeyValues, NumKeyValues, "back"),  &BackVector);
    }

    // Compute the basis transformation
    granny_real32 Affine3[3];
    granny_real32 Linear3x3[9];
    granny_real32 InverseLinear3x3[9];
    if (GrannyComputeBasisConversion(Info,
                                     UnitsPerMeter,
                                     Origin,
                                     RightVector,
                                     UpVector,
                                     BackVector,
                                     Affine3, Linear3x3, InverseLinear3x3) == false)
    {
        ErrOut("GrannyComputeBasisConversion failed.  Probably bad coordinate system specified\n");
        return 0;
    }

    // Just transform the file...
    GrannyTransformFile(Info,
                        Affine3, Linear3x3, InverseLinear3x3,
                        1e-5f, 1e-5f,
                        GrannyRenormalizeNormals | GrannyReorderTriangleIndices);

    Info->ArtToolInfo->UnitsPerMeter = UnitsPerMeter;
    memcpy(Info->ArtToolInfo->UpVector,    UpVector,    sizeof(UpVector));
    memcpy(Info->ArtToolInfo->RightVector, RightVector, sizeof(RightVector));
    memcpy(Info->ArtToolInfo->BackVector,  BackVector,  sizeof(BackVector));

    return Info;
}

static const char* HelpString =
    (" TransformFile converts to a consistent coordinate system. Saves calling\n"
     " GrannyTransformFile at runtime.\n"
     "\n"
     "    preprocessor TransformFile char.gr2 -output char_xf.gr2 \\\n"
     "        -unitsPerMeter 1.0 -right PosX -back PosZ -up PosY\n"
     "\n"
     " Valid keys for the -right/-back/-up fields are:\n"
     "   PosX NegX\n"
     "   PosY NegY\n"
     "   PosZ NegZ\n");

static CommandRegistrar RegTransformFile(TransformFile,
                                         "TransformFile",
                                         "Converts to a consistent coordinate system.",
                                         HelpString);

