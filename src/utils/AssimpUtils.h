// Copyright (C) 2026 iQuit (https://github.com/iQuitt)
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#pragma once

#include "utils/util.hpp"

#include <assimp/scene.h>

#include <string>

namespace AssimpUtils {

// the node that draws a mesh, searched from node down
inline const aiNode* FindMeshNode(const aiNode* node, unsigned int mesh)
{
    if (!node)
        return nullptr;
    for (unsigned int i = 0; i < node->mNumMeshes; ++i)
        if (node->mMeshes[i] == mesh)
            return node;
    for (unsigned int i = 0; i < node->mNumChildren; ++i)
        if (const aiNode* found = FindMeshNode(node->mChildren[i], mesh))
            return found;
    return nullptr;
}

// a mesh's name: its material's (as Blender shows glTF meshes), else its own; its node's when that is empty or
// generic; empty when none has one
inline std::string MeshDisplayName(const aiScene* scene, unsigned int mesh)
{
    const aiMesh* drawn = scene->mMeshes[mesh];
    std::string name;
    if (drawn->mMaterialIndex < scene->mNumMaterials)
    {
        aiString material;
        if (scene->mMaterials[drawn->mMaterialIndex]->Get(AI_MATKEY_NAME, material) == AI_SUCCESS)
        {
            const std::string named = material.C_Str();
            if (!named.empty() && named != "DefaultMaterial" && !String_StartsWithNoCase(named, "material"))
                name = named;
        }
    }
    if (name.empty())
        name = drawn->mName.C_Str();
    if (name.empty() || String_StartsWithNoCase(name, "mesh"))
        if (const aiNode* node = FindMeshNode(scene->mRootNode, mesh); node && node->mName.length > 0)
            name = node->mName.C_Str();
    return name;
}

}  // namespace AssimpUtils
