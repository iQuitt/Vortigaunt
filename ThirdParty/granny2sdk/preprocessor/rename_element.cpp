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
#include <ctype.h>
#include <vector>
#include <algorithm>

using namespace std;

bool ExtractElementIdx(char const* ElementAndIdx,
                       char* OutputElement,
                       granny_int32x* OutputIndex)
{
    pp_assert(ElementAndIdx);
    pp_assert(OutputElement);
    pp_assert(OutputIndex);

    const char* LeftBracket  = strchr(ElementAndIdx, '[');
    const char* RightBracket = strchr(ElementAndIdx, ']');

    if (_stricmp(ElementAndIdx, "filepath") == 0)
    {
        *OutputIndex = -1;
        strcpy(OutputElement,ElementAndIdx);
        return true;
    }

    if (!LeftBracket || !RightBracket ||
        RightBracket < LeftBracket || RightBracket == LeftBracket+1)
    {
        printf("RenameElement: Invalid array notation: %s\n", ElementAndIdx);
        return false;
    }

    char* End = std::copy(ElementAndIdx, LeftBracket, OutputElement);
    *End = '\0';

    bool AllNumbers = true;
    for (char const* Test = LeftBracket + 1; Test < RightBracket; ++Test)
    {
        if (!isdigit(*Test))
            AllNumbers = false;
    }

    if (!AllNumbers)
    {
        printf("RenameElement: Invalid array notation: %s\n", ElementAndIdx);
        return false;
    }

    *OutputIndex = atoi(LeftBracket+1);
    return true;
}

bool DoElementRename(granny_file_info* Info,
                     char const* ElementName,
                     granny_int32x Index,
                     char const* NewName)
{
    if (_stricmp(ElementName, "filepath") == 0)
    {
        Info->FromFileName = NewName;
        return true;
    }

    if (Index < 0)
    {
        ErrOut("invalid index (%d)\n", Index);
        return false;
    }

    if (_stricmp(ElementName, "materials") == 0)
    {
        if (Index >= Info->MaterialCount)
        {
            ErrOut("invalid index (%d, only %d materials present)\n",
                    Index, Info->MaterialCount);
            return false;
        }

        if (Info->Materials[Index])
            Info->Materials[Index]->Name = NewName;
    }
    else if (_stricmp(ElementName, "animations") == 0)
    {
        if (Index >= Info->AnimationCount)
        {
            ErrOut("invalid index (%d, only %d animations present)\n",
                    Index, Info->AnimationCount);
            return false;
        }

        if (Info->Animations[Index])
            Info->Animations[Index]->Name = NewName;
    }
    else if (_stricmp(ElementName, "TrackGroups") == 0)
    {
        if (Index >= Info->TrackGroupCount)
        {
            ErrOut("invalid index (%d, only %d track_groups present)\n",
                Index, Info->TrackGroupCount);
            return false;
        }

        if (Info->TrackGroups[Index])
            Info->TrackGroups[Index]->Name = NewName;
    }
    else if (_stricmp(ElementName, "textures") == 0)
    {
        if (Index >= Info->TextureCount)
        {
            ErrOut("invalid index (%d, only %d textures present)\n",
                Index, Info->TextureCount);
            return false;
        }

        if (Info->Textures[Index])
            Info->Textures[Index]->FromFileName = NewName;
    }
    else if (_stricmp(ElementName, "meshes") == 0)
    {
        if (Index >= Info->MeshCount)
        {
            ErrOut("invalid index (%d, only %d meshes present)\n",
                    Index, Info->MeshCount);
            return false;
        }

        if (Info->Meshes[Index])
            Info->Meshes[Index]->Name = NewName;
    }
    else if (_stricmp(ElementName, "models") == 0)
    {
        if (Index >= Info->ModelCount)
        {
            ErrOut("invalid index (%d, only %d materials present)\n",
                Index, Info->ModelCount);
            return false;
        }

        if (Info->Models[Index])
            Info->Models[Index]->Name = NewName;
    }
    else if (_stricmp(ElementName, "skeletons") == 0)
    {
        if (Index >= Info->SkeletonCount)
        {
            ErrOut("invalid index (%d, only %d skeletons present)\n",
                Index, Info->SkeletonCount);
            return false;
        }

        if (Info->Skeletons[Index])
            Info->Skeletons[Index]->Name = NewName;
    }

    return true;
}

granny_file_info*
RenameElement(char const*          OriginalFilename,
              char const*          OutputFilename,
              granny_file_info*    Info,
              key_value_pair*      KeyValues,
              granny_int32x        NumKeyValues,
              granny_memory_arena* TempArena)
{
    char const* TargetName = FindFirstValueForKey(KeyValues, NumKeyValues, "newname");
    if (TargetName == 0)
    {
        ErrOut("must specify a Name value with \"-newname <value>\"\n");
        return false;
    }

    bool FoundAny = false;
    {for (granny_int32x Key = 0; Key < NumKeyValues; ++Key)
    {
        if (_stricmp(KeyValues[Key].Key, "rename") == 0)
        {
            char ElementBuffer[512];
            granny_int32x Index;
            if (ExtractElementIdx(KeyValues[Key].Value, ElementBuffer, &Index))
            {
                FoundAny = true;

                if (!DoElementRename(Info, ElementBuffer, Index, TargetName))
                {
                    ErrOut("Failed to rename: %s[%d] to %s\n",
                            ElementBuffer, Index, TargetName);
                    return false;
                }
            }
        }
    }}

    if (FoundAny)
    {
        return Info;
    }
    else
    {
        ErrOut("unable to find any valid \"-rename\" keys\n");
        return 0;
    }
}


static const char* HelpString =
    (" RenameElement lets you change the name of any top-level item in\n"
     " the granny_file_info structure.  It is especially handy for changing\n"
     " the name of granny_animation objects, which by default are named for\n"
     " the source Max or Maya file.  If there are multiple animations\n"
     " in the source file, or if you'd simply like a more convenient name\n"
     " for the granny_animation, you can use this command to quickly change\n"
     " the string.\n"
     "\n"
     "    preprocessor RenameElement anim.gr2 -output anim.gr2 \\\n"
     "        -rename Animations[0] -newname \"Walk\"\n"
     "\n"
     "    preprocessor RenameElement anim.gr2 -output anim.gr2 \\\n"
     "        -rename FilePath -newname \"NewName\"\n");

static CommandRegistrar RegRenameElement(RenameElement,
                                         "RenameElement",
                                         "Renames granny_file_info members specified by \"-rename Name[idx]\"",
                                         HelpString);
