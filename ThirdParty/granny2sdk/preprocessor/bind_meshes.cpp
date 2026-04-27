// ========================================================================
// $Notice: $
// ========================================================================
#include "preprocessor.h"

#include <stdio.h>
#include <string.h>
#include <vector>

using namespace std;

static bool
UnambiguousMatch(granny_model* Model, granny_mesh* Mesh)
{
    if (!Model || !Mesh || !Model->Skeleton)
        return false;

    if (Mesh->BoneBindingCount == 0)
        return false;

    {for (int Idx = 0; Idx < Mesh->BoneBindingCount; ++Idx)
    {
        granny_int32x Ignored;
        if (!GrannyFindBoneByNameLowercase(Model->Skeleton, Mesh->BoneBindings[Idx].BoneName, &Ignored))
            return false;
    }}

    return true;
}


granny_file_info*
BindMeshes(char const*          OriginalFilename,
           char const*          OutputFilename,
           granny_file_info*    InputInfo,
           key_value_pair*      KeyValuePairs,
           granny_int32x        NumKeyValuePairs,
           granny_memory_arena* TempArena)
{
    vector< vector<granny_model_mesh_binding> > MeshBindings(InputInfo->ModelCount);
    
    {for (int Idx = 0; Idx < InputInfo->MeshCount; ++Idx)
    {
        granny_mesh* Mesh = InputInfo->Meshes[Idx];
        if (!Mesh)
            continue;
        
        int Bound = -1;
        {for (int ModelIdx = 0; ModelIdx < InputInfo->ModelCount; ++ModelIdx)
        {
            if (UnambiguousMatch(InputInfo->Models[ModelIdx], Mesh))
            {
                if (Bound == -1)
                {
                    Bound = ModelIdx;
                }
                else
                {
                    WarnOut("Mesh(%d): '%s' matches more than one model in the file", Idx, Mesh->Name);
                    Bound = -2;
                }
            }
        }}

        if (Bound >= 0)
        {
            granny_model_mesh_binding NewBinding = { Mesh };
            MeshBindings[Bound].push_back(NewBinding);
        }
    }}

    {for (int Idx = 0; Idx < InputInfo->ModelCount; ++Idx)
    {
        InputInfo->Models[Idx]->MeshBindingCount = (granny_int32)MeshBindings[Idx].size();
        if (InputInfo->Models[Idx]->MeshBindingCount)
        {
            InputInfo->Models[Idx]->MeshBindings = PushArray(TempArena, InputInfo->Models[Idx]->MeshBindingCount, granny_model_mesh_binding);
            memcpy(InputInfo->Models[Idx]->MeshBindings, &((MeshBindings[Idx])[0]),
                   sizeof(granny_model_mesh_binding) * InputInfo->Models[Idx]->MeshBindingCount);
        }
        else
        {
            InputInfo->Models[Idx]->MeshBindings = 0;
        }
    }}

    return InputInfo;
}




static char const* HelpString =
    (" BindMeshes is intended to fix the problem that after combining meshes\n"
     " with models in a granny_file_info, the MeshBindings array of the model\n"
     " doesn't refer to those meshes.  BindMeshes will only bind a mesh to a \n"
     " model in the file if the binding is both unambiguous (all of the \n"
     " BoneBindings in the mesh properly refer to bones that exist in the \n"
     " model), and unique (only one granny_model in the file unambiguously\n"
     " matches).  This command is batchable."
     "\n"
     "    preprocessor BindMeshes combined.gr2 -output combined.gr2\n");

static CommandRegistrar RegBindMeshes(BindMeshes, "BindMeshes",
                                      "Combines root level elements of the granny_file_info",
                                      HelpString);

