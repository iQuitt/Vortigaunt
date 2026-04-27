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
#include <vector>
#include <string>
#include <string.h>
#include <stdlib.h>

using namespace std;

bool LODCalc(input_file*     InputFiles,
             granny_int32x   NumInputFiles,
             key_value_pair* KeyValues,
             granny_int32x   NumKeyValues,
             granny_memory_arena* TempArena)
{
    RequireKey("output", "must specify an output file with \"-output <filename>\"\n");

    if (NumInputFiles != 2)
    {
        ErrOut("must supply model and animation (if they are the same, specify twice)\n");
        return false;
    }

    granny_file_info* ModelInfo = ExtractFileInfo(InputFiles[0]);
    if (ModelInfo == 0)
    {
        ErrOut("unable to obtain a granny_file_info from the input model file\n");
        return false;
    }

    granny_file_info* AnimInfo = ExtractFileInfo(InputFiles[1]);
    if (AnimInfo == 0)
    {
        ErrOut("unable to obtain a granny_file_info from the input animation file\n");
        return false;
    }

    if (ModelInfo->MeshCount == 0)
    {
        ErrOut("ModelFile contains no mesh information with which to make the LOD calcs\n");
        return false;
    }

    if (AnimInfo->AnimationCount == 0)
    {
        ErrOut("AnimFile contains no animations\n");
        return false;
    }

    // Compute mesh bindings for the models...
    granny_int32x NumBindings = 0;
    {for(granny_int32x Model = 0; Model < ModelInfo->ModelCount; ++Model)
    {
        NumBindings += ModelInfo->Models[Model]->MeshBindingCount;
    }}

    // We need to hang onto the BindingModels so we don't get confused
    // later.  The mesh binding only holds the skeleton, so we can't
    // backtrack from there.  Annoying.
    granny_mesh_binding** Bindings = PushArray(TempArena, NumBindings, granny_mesh_binding*);
    granny_model** BindingModels   = PushArray(TempArena, NumBindings, granny_model*);

    NumBindings = 0;
    {for(granny_int32x Model = 0; Model < ModelInfo->ModelCount; ++Model)
    {
        {for(granny_int32 Binding = 0; Binding < ModelInfo->Models[Model]->MeshBindingCount; ++Binding)
        {
            if (ModelInfo->Models[Model]->MeshBindings[Binding].Mesh)
            {
                granny_mesh*  ThisMesh  = ModelInfo->Models[Model]->MeshBindings[Binding].Mesh;
                granny_model* ThisModel = ModelInfo->Models[Model];

                BindingModels[NumBindings] = ThisModel;
                Bindings[NumBindings] =
                    GrannyNewMeshBinding(ThisMesh, ThisModel->Skeleton, ThisModel->Skeleton);
                ++NumBindings;
            }
        }}
    }}


    vector<granny_file*> TempFiles;
    vector<granny_mesh_binding*> TempBindings;
    {for(granny_int32x Anim = 0; Anim < AnimInfo->AnimationCount; ++Anim)
    {
        granny_animation* Animation = AnimInfo->Animations[Anim];

        granny_animation_lod_builder* Builder =
            GrannyCalculateAnimationLODBegin ( Animation, 1.0f );
        for (int i = 0; i < NumBindings; i++)
        {
            GrannyCalculateAnimationLODAddMeshBinding(Builder, BindingModels[i],
                                                      Bindings[i], 1.0f);
        }

        granny_skeleton* Skeleton = ModelInfo->Models[0]->Skeleton;
        char const* StandInFileName = FindFirstValueForKey(KeyValues, NumKeyValues, "standinmodel");
        if (StandInFileName != 0)
        {
            for (int i = 0; i < NumKeyValues; ++i)
            {
                key_value_pair& KeyValue = KeyValues[i];
                if (_stricmp(KeyValue.Key, "standin") == 0)
                {
                    char const* ColonPos = strchr(KeyValue.Value, ':');
                    if (ColonPos == NULL)
                    {
                        ErrOut("standin format '%s' should be formatted as 'bonename:size'\n",
                                KeyValue.Value);
                    }

                    granny_file* StandInFile = GrannyReadEntireFile(StandInFileName);
                    if (StandInFile == 0)
                        break;

                    // We can't allocate this from the arena easily, so we'll manually
                    // manage it...
                    TempFiles.push_back(StandInFile);
                    granny_file_info* StandInInfo = 0;
                    {
                        // Tiny bit of rigamarole to ensure that we use the correct mod/normal path.
                        input_file TempInput;
                        TempInput.GrannyFile = StandInFile;
                        TempInput.Filename = (char*)StandInFileName;  // doesn't actually change...
                        StandInInfo = ExtractFileInfo(TempInput);
                    }
                    pp_assert(StandInInfo);

                    if (StandInInfo->MeshCount == 0)
                        continue;

                    string BoneName(KeyValue.Value, ColonPos);
                    granny_real32 Scale = (granny_real32)atof(ColonPos + 1);

                    granny_int32x BoneIndex;
                    if (GrannyFindBoneByNameLowercase(Skeleton, BoneName.c_str(), &BoneIndex))
                    {
                        granny_real32 Affine[3] = { 0, 0, 0 };
                        granny_real32 Linear[9] = {
                            Scale, 0, 0,
                            0, Scale, 0,
                            0, 0, Scale
                        };
                        granny_real32 InvLinear[9] = {
                            1.0f / Scale, 0, 0,
                            0, 1.0f / Scale, 0,
                            0, 0, 1.0f / Scale
                        };

                        GrannyTransformMesh(StandInInfo->Meshes[0],
                                            Affine,
                                            Linear, InvLinear,
                                            1e-5f, 1e-5f, 0);

                        StandInInfo->Models[0]->Skeleton->Bones[0].Name = Skeleton->Bones[BoneIndex].Name;
                        StandInInfo->Meshes[0]->BoneBindings[0].BoneName = Skeleton->Bones[BoneIndex].Name;

                        granny_mesh_binding* NewBinding =
                            GrannyNewMeshBinding(StandInInfo->Meshes[0],
                                                 StandInInfo->Models[0]->Skeleton,
                                                 Skeleton);

                        GrannyCalculateAnimationLODAddMeshBinding(Builder,
                                                                  ModelInfo->Models[0],
                                                                  NewBinding,
                                                                  1.0);
                    }
                }
            }
        }

        GrannyCalculateAnimationLODEnd(Builder);
    }}

    // Free the bindings
    {for(granny_int32x Binding = 0; Binding < NumBindings; ++Binding)
    {
        GrannyFreeMeshBinding(Bindings[Binding]);
    }}
    {for(granny_int32x Binding = 0; Binding < (int)TempBindings.size(); ++Binding)
    {
        GrannyFreeMeshBinding(TempBindings[Binding]);
    }}

    // Free the temp files...
    {for(granny_int32x File = 0; File < (int)TempFiles.size(); ++File)
    {
        GrannyFreeFile(TempFiles[File]);
    }}

    // Write out the resulting file...
    return WriteInfoPreserve(KeyValues, NumKeyValues, AnimInfo, InputFiles[1].GrannyFile, true, true, true, TempArena);
}

static char const* HelpString =
    (" LODCalc creates or refreshes animation LOD parameters via sampling for\n"
     " a specific animation/model pairing. Call with the model file first,\n"
     " followed by the animation source. These may be the same file.\n"
     "\n"
     "    preprocessor LODCalc model.gr2 anim.gr2 -output anim_lod.gr2\n");

static CommandRegistrar RegLODCalc(LODCalc, "LODCalc",
                                   "Computes LOD values for an animation",
                                   HelpString);
