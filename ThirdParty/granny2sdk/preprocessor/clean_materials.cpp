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
#include <set>
#include <algorithm>

using namespace std;

typedef set<granny_material*> MaterialSet;
typedef set<granny_texture*>  TextureSet;


void
CleanUnreferencedMaterials(granny_file_info*    FileInfo,
                           granny_memory_arena* TempArena)
{
    MaterialSet MeshReferencedMaterials;

    // Loop through all the meshes, checking references
    {for(granny_int32x Idx = 0; Idx < FileInfo->MeshCount; ++Idx)
    {
        granny_mesh* Mesh = FileInfo->Meshes[Idx];

        // and all the material bindings on the mesh
        {for(granny_int32x Binding = 0; Binding < Mesh->MaterialBindingCount; ++Binding)
        {
            if (Mesh->MaterialBindings[Binding].Material != NULL)
                MeshReferencedMaterials.insert(Mesh->MaterialBindings[Binding].Material);
        }}
    }}

    // This is really a suck-tastic way to walk the material tree, but it's easy to verify
    // that it works.
    bool inserted;
    MaterialSet FinalMaterials;
    TextureSet FinalTextures;
    do
    {
        inserted = false;

        MaterialSet TempMaterials;
        {for(MaterialSet::iterator itr = MeshReferencedMaterials.begin();
             itr != MeshReferencedMaterials.end();
             ++itr)
        {
            granny_material* MeshMaterial = *itr;
            if (FinalMaterials.insert(MeshMaterial).second)
            {
                inserted = true;
                if (MeshMaterial->Texture != NULL)
                    FinalTextures.insert(MeshMaterial->Texture);

                {for(granny_int32x Map = 0; Map < MeshMaterial->MapCount; ++Map)
                {
                    granny_material* SubMaterial = MeshMaterial->Maps[Map].Material;
                    TempMaterials.insert(SubMaterial);
                }}

                // And check the ExtendedData of each material to make sure that we
                // haven't missed any textures hiding in there.  Check the first level
                // only.
                if (MeshMaterial->ExtendedData.Type &&
                    MeshMaterial->ExtendedData.Object)
                {
                    granny_uint8* ObjectPtr = (granny_uint8*)MeshMaterial->ExtendedData.Object;
                    for (granny_data_type_definition* TypeItr = MeshMaterial->ExtendedData.Type;
                         TypeItr->Type != GrannyEndMember;
                         ++TypeItr)
                    {
                        if (TypeItr->Type == GrannyReferenceMember)
                        {
                            void* Pointer = *(void**)ObjectPtr;
                            if (Pointer)
                            {
                                if (find(FileInfo->Textures,
                                         FileInfo->Textures + FileInfo->TextureCount,
                                         (granny_texture*)Pointer)
                                    != (FileInfo->Textures + FileInfo->TextureCount))
                                {
                                    FinalTextures.insert((granny_texture*)Pointer);
                                }
                            }
                        }
                        else if (TypeItr->Type == GrannyArrayOfReferencesMember)
                        {
                            granny_int32 ArrayCount = *(granny_int32*)ObjectPtr;
                            void** PointerArray = *(void***)(ObjectPtr + sizeof(granny_int32));

                            for (int i = 0; i < ArrayCount; ++i)
                            {
                                void* Pointer = *PointerArray;

                                if (find(FileInfo->Textures,
                                         FileInfo->Textures + FileInfo->TextureCount,
                                         (granny_texture*)Pointer)
                                    != (FileInfo->Textures + FileInfo->TextureCount))
                                {
                                    FinalTextures.insert((granny_texture*)Pointer);
                                }
                                ++PointerArray;
                            }
                        }

                        ObjectPtr += GrannyGetMemberTypeSize(TypeItr);
                    }
                }
            }
        }}

        MeshReferencedMaterials.insert(TempMaterials.begin(), TempMaterials.end());
    } while (inserted);

    // Ok, create the final material array...
    FileInfo->MaterialCount = (granny_int32)FinalMaterials.size();
    if (FileInfo->MaterialCount != 0)
    {
        FileInfo->Materials =
            PushArray(TempArena, FileInfo->MaterialCount, granny_material*);

        granny_int32x Current = 0;
        {for(MaterialSet::iterator itr = FinalMaterials.begin();
             itr != FinalMaterials.end();
             ++itr)
        {
            FileInfo->Materials[Current++] = *itr;
        }}
    }
    else
    {
        FileInfo->Materials = 0;
    }

    // ...and the final texture array
    FileInfo->TextureCount = (granny_int32)FinalTextures.size();
    if (FileInfo->TextureCount != 0)
    {
        FileInfo->Textures =
            PushArray(TempArena, FileInfo->TextureCount, granny_texture*);

        granny_int32x Current = 0;
        {for(TextureSet::iterator itr = FinalTextures.begin();
             itr != FinalTextures.end();
             ++itr)
        {
            FileInfo->Textures[Current++] = *itr;
        }}
    }
    else
    {
        FileInfo->Textures = 0;
    }
}

granny_file_info*
CleanMaterials(char const*          OriginalFilename,
               char const*          OutputFilename,
               granny_file_info*    Info,
               key_value_pair*      KeyValues,
               granny_int32x        NumKeyValues,
               granny_memory_arena* TempArena)
{
    // Remove unused materials and textures
    CleanUnreferencedMaterials(Info, TempArena);
    return Info;
}


static char const* HelpString =
    (" Granny exports every available material from the Maya and Max material\n"
     " graphs in typical usage. This command loops through the meshes, and\n"
     " only preserves material objects that are directly reachable from a\n"
     " granny_mesh, which can save space at runtime.\n"
     "\n"
     "    preprocessor CleanMaterials char.gr2 -output char_proc.gr2\n"
     "    preprocessor CleanMaterials char.gr2 -output char.gr2\n");

static CommandRegistrar RegCleanMaterials(CleanMaterials,
                                          "CleanMaterials",
                                          "Removes unreferenced materials from the file",
                                          HelpString);

