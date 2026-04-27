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

#include "../stb_image_write.h"


using namespace std;

bool DumpMeshTexture(input_file&     InputFile,
                     key_value_pair* KeyValues,
                     granny_int32x   NumKeyValues,
                     granny_memory_arena* TempArena)
{
    char const* OutputPrefix = FindFirstValueForKey(KeyValues, NumKeyValues, "prefix");
    if (OutputPrefix == 0)
    {
        fprintf(stderr, "must specify an prefix \"-prefix <string>\"\n");
        return false;
    }

    granny_file_info* Info = ExtractFileInfo(InputFile);
    if (Info == 0)
    {
        fprintf(stderr, "DumpMeshTexture: unable to obtain a granny_file_info from the input file\n");
        return false;
    }

    // Optimize all meshes for the pre/post-transform cache
    {for(granny_int32x i = 0; i < Info->MeshCount; i++)
    {
        granny_mesh* Mesh = Info->Meshes[i];
        if (!Mesh)
            continue;

        vector<float> Positions;
        vector<float> Normals;
        vector<float> TexCoords;

        granny_variant Ignore;
        if (GrannyFindMatchingMember(GrannyGetMeshVertexType(Mesh),
                                     GrannyGetMeshVertices(Mesh),
                                     GrannyVertexPositionName,
                                     &Ignore))
        {
            granny_data_type_definition PosType[] = {
                { GrannyReal32Member, GrannyVertexPositionName, 0, 3 },
                { GrannyEndMember }
            };
            Positions.resize(GrannyGetMeshVertexCount(Mesh) * 3);
            GrannyCopyMeshVertices(Mesh, PosType, &Positions[0]);
        }
        if (GrannyFindMatchingMember(GrannyGetMeshVertexType(Mesh),
                                     GrannyGetMeshVertices(Mesh),
                                     GrannyVertexNormalName,
                                     &Ignore))
        {
            granny_data_type_definition NormalType[] = {
                { GrannyReal32Member, GrannyVertexNormalName, 0, 3 },
                { GrannyEndMember }
            };
            Normals.resize(GrannyGetMeshVertexCount(Mesh) * 3);
            GrannyCopyMeshVertices(Mesh, NormalType, &Normals[0]);
        }
        if (GrannyFindMatchingMember(GrannyGetMeshVertexType(Mesh),
                                     GrannyGetMeshVertices(Mesh),
                                     GrannyVertexTextureCoordinatesName "0",
                                     &Ignore))
        {
            granny_data_type_definition TexCoordType[] = {
                { GrannyReal32Member, GrannyVertexTextureCoordinatesName "0", 0, 2 },
                { GrannyEndMember }
            };
            TexCoords.resize(GrannyGetMeshVertexCount(Mesh) * 2);
            GrannyCopyMeshVertices(Mesh, TexCoordType, &TexCoords[0]);
        }

        char buffer[512];
        sprintf(buffer, "%s_mesh_%d.obj", OutputPrefix, i);
        FILE* file = fopen(buffer, "w");
        for (size_t p = 0; p < Positions.size(); p += 3)
            fprintf(file, "v %f %f %f\n", Positions[p + 0], Positions[p + 1], Positions[p + 2]);
        fprintf(file, "# %d vertices\n\n", (int)Positions.size() / 3);

        for (size_t n = 0; n < Normals.size(); n += 3)
            fprintf(file, "vn %f %f %f\n", Normals[n + 0], Normals[n + 1], Normals[n + 2]);
        fprintf(file, "# %d vertex normals\n\n", (int)Normals.size() / 3);

        for (size_t t = 0; t < TexCoords.size(); t += 2)
            fprintf(file, "vt %f %f\n", TexCoords[t + 0], 1.0 - TexCoords[t + 1]);
        fprintf(file, "# %d vertex texture coords\n\n", (int)TexCoords.size() / 2);

        fprintf(file, "g %s\n", Mesh->Name);
        vector<granny_int32> MeshIndices(GrannyGetMeshIndexCount(Mesh));
        GrannyCopyMeshIndices(Mesh, 4, &MeshIndices[0]);

        for (size_t mi = 0; mi < MeshIndices.size(); mi += 3)
        {
            // Silly test to eliminate strictly degenerate tris (they confuse maya)
            {
                int v0 = MeshIndices[mi + 0];
                int v1 = MeshIndices[mi + 1];
                int v2 = MeshIndices[mi + 2];
                if (v0 == v1 || v0 == v2 || v1 == v2)
                    continue;
            }
            
            fprintf(file, "f ");
            for (int tv = 0; tv < 3; ++tv)
            {
                int Index = MeshIndices[mi + tv] + 1;
                fprintf(file, "%d", Index);

                if (!TexCoords.empty())
                    fprintf(file, "/%d", Index);
                else
                    fprintf(file, "/");

                if (!Normals.empty())
                    fprintf(file, "/%d ", Index);
                else
                    fprintf(file, "/ ");
            }
            fprintf(file, "\n");
        }
        fclose(file);
    }}

    {for(int TextureIdx = 0; TextureIdx < Info->TextureCount; ++TextureIdx)
    {
        granny_texture* Texture = Info->Textures[TextureIdx];
        if (!Texture)
            continue;

        vector<char> Image;
        Image.resize(Texture->Width * Texture->Height * 3);
        if (!GrannyCopyTextureImage(Texture, 0, 0, GrannyRGB888PixelFormat,
                                    Texture->Width, Texture->Height, Texture->Width * 3,
                                    &Image[0]))
            continue;

        char buffer[512];
        sprintf(buffer, "%s_tex_%d.bmp", OutputPrefix, TextureIdx);
        stbi_write_bmp(buffer, Texture->Width, Texture->Height, 3, &Image[0]);
    }}

    return true;
}

static CommandRegistrar RegDumpMeshTexture(eSingleFile_NoMultiRun,
                                            DumpMeshTexture,
                                            "DumpMeshTexture",
                                            "Dumps a .GR2 file with meshes as OBJ files, textures as BMP.");

