// ========================================================================
// $File$
// $DateTime$
// $Change$
// $Revision$
//
// $Notice: $
// ========================================================================
#include "../preprocessor.h"

#include "../stb_image.h"

#include <stdio.h>
#include <string.h>

#include <fstream>
#include <strstream>
#include <iostream>
#include <string>
#include <vector>
using namespace std;


// ---------------------------------------------------------------------------------------
// Usage:
//   preprocessor OBJImport -obj <file.obj> -output <file.gr2> [-texture <tex.jpg>]
//
// If the optional -texture flag is provided, the mesh will have a diffuse material bound
// to it using that texture as the source.
// ---------------------------------------------------------------------------------------

struct Point3
{
    granny_real32 Val[3];
};

struct Point2
{
    granny_real32 Val[2];
};

struct Triangle
{
    int Position[3];
    int Normal[3];   // -1 = no normal
    int UV[3];       // -1 = no uv
};


static void
ExtractVert(string& vert, int& Pos, int& Norm, int& UV)
{
    // Format for a vert description is n, n/n, n//n, n/n/n
    istrstream str(vert.c_str());

    Pos  = -1;
    Norm = -1;
    UV   = -1;

    str >> Pos;
    if (str.eof())
        return;

    char slash;
    str >> slash;

    str >> UV;
    if (str.eof())
        return;

    str >> slash;
    str >> Norm;
}


static granny_mesh*
BuildMeshFromArrays(granny_memory_arena* TempArena,
                    vector<Point3>& Positions,
                    vector<Point3>& Normals,
                    vector<Point2>& UVs,
                    vector<Triangle>& Tris)
{
    bool const HasNormals = !Normals.empty();
    bool const HasUVs     = !UVs.empty();

    granny_data_type_definition* VertType = 0;
    {
        if (HasNormals && HasUVs)
            VertType = GrannyPNGT3332VertexType;
        else if (HasNormals)
            VertType = GrannyPNG333VertexType;
        else if (HasUVs)
            VertType = GrannyPT32VertexType;
        else
            VertType = GrannyP3VertexType;
    }
    granny_mesh_builder* Builder =
        GrannyBeginMesh((int)Positions.size(), (int)Tris.size(), 1, 0, VertType);

    for (size_t i = 0; i < Positions.size(); ++i)
    {
        GrannySetPosition(Builder,
                          Positions[i].Val[0],
                          Positions[i].Val[1],
                          Positions[i].Val[2]);
        GrannyPushVertex(Builder);
    }

    for (size_t TriIdx = 0; TriIdx < Tris.size(); ++TriIdx)
    {
        Triangle& Tri = Tris[TriIdx];

        for (int edge = 0; edge < 3; ++edge)
        {
            GrannySetVertexIndex(Builder, edge, Tri.Position[edge] - 1);

            if (HasUVs)
                GrannySetChannel(Builder, edge, 0, UVs[Tri.UV[edge] - 1].Val, 2);

            if (HasNormals)
            {
                Point3& Norm = Normals[Tri.Normal[edge] - 1];
                GrannySetNormal(Builder, edge, Norm.Val[0], Norm.Val[1], Norm.Val[2]);
            }
        }

        GrannySetMaterial(Builder, 0);
        GrannyPushTriangle(Builder);
    }

    // Build tangents if we have normals
    if (HasNormals)
        GrannyGenerateTangentSpaceFromUVs(Builder);

    granny_vertex_data* VData = 0;
    granny_tri_topology* Topo = 0;
    GrannyEndMeshInPlace(Builder,
                         GrannyMemoryArenaPush(TempArena,
                                               GrannyGetResultingVertexDataSize(Builder)),
                         &VData,
                         GrannyMemoryArenaPush(TempArena,
                                               GrannyGetResultingTopologySize(Builder)),
                         &Topo);

    granny_mesh* Mesh = 0;
    if (VData && Topo)
    {
        Mesh = PushObject(TempArena, granny_mesh);
        memset(Mesh, 0, sizeof(*Mesh));

        Mesh->PrimaryVertexData = VData;
        Mesh->PrimaryTopology   = Topo;
    }

    return Mesh;
}

granny_texture*
CreateTextureFor(granny_memory_arena* TempArena, char const* TextureName)
{
    int x, y, comp;
    stbi_uc* ImageBytes = stbi_load(TextureName, &x, &y, &comp, 4);
    if (!ImageBytes)
        return 0;

    granny_texture_builder* Builder = GrannyBeginBinkTexture(x, y, 10, 0);
    if (!Builder)
    {
        stbi_image_free(ImageBytes);
        return 0;
    }

    GrannyEncodeImage(Builder, x, y, x * 4, 1, ImageBytes);
    granny_texture *ResultingTexture =
        GrannyEndTextureInPlace(Builder,
                                PushArray(TempArena, GrannyGetResultingTextureSize(Builder),
                                               granny_uint8));
    ResultingTexture->FromFileName = TextureName;

    return ResultingTexture;
}


bool OBJImport(key_value_pair* KeyValues,
               granny_int32x   NumKeyValues,
               granny_memory_arena* TempArena)
{
    RequireKey("output", "must specify an output file with \"-output <filename>\"\n");

    char const* OBJFile = FindFirstValueForKey(KeyValues, NumKeyValues, "obj");
    if (OBJFile == 0)
    {
        ErrOut("must specify input file with -obj <filename>\n");
        return false;
    }

    char const* MaterialSource = FindFirstValueForKey(KeyValues, NumKeyValues, "texture");

    vector<Point3>   Positions;
    vector<Point3>   Normals;
    vector<Point2>   UVs;
    vector<Triangle> Tris;
    {
        ifstream objfile(OBJFile);
        if (!objfile.is_open())
        {
            ErrOut("can't open file: %s\n", OBJFile);
            return false;
        }

        // This is super lame.  It doesn't handle multiple objects, grouping, smoothing, etc.
        // But it get's the job done for simple objects.  More complicated things can be
        // layered on top
        while (!objfile.eof() )
        {
            string line;
            getline(objfile, line);

            if (line.compare(0, 2, "v ") == 0)
            {
                Point3 pos;
                sscanf(line.c_str() + 2, "%f %f %f", &pos.Val[0], &pos.Val[1], &pos.Val[2]);
                Positions.push_back(pos);
            }
            else if (line.compare(0, 3, "vt ") == 0)
            {
                Point2 uv = { { 0, 0 } };
                sscanf(line.c_str() + 3, "%f %f", &uv.Val[0], &uv.Val[1]);

                // reverse uv.  todo: in all cases?
                uv.Val[1] = 1.0f - uv.Val[1];

                UVs.push_back(uv);
            }
            else if (line.compare(0, 3, "vn ") == 0)
            {
                Point3 norm;
                sscanf(line.c_str() + 3, "%f %f %f", &norm.Val[0], &norm.Val[1], &norm.Val[2]);
                Normals.push_back(norm);
            }
            else if (line.compare(0, 2, "f ") == 0)
            {
                // quite slow
                vector<string> verts;

                size_t curr = 2;
                for (;;)
                {
                    size_t next = line.find_first_of(' ', curr);

                    if (next == string::npos)
                    {
						while (curr < line.length() && line[curr] == ' ')
							++curr;
                        
						if (curr < line.length())
							verts.push_back(line.substr(curr));
                        break;
                    }
                    else
                    {
                        verts.push_back(line.substr(curr, next - curr));
                        curr = next + 1;
                    }
                }

                Triangle NewTri;
                ExtractVert(verts[0], NewTri.Position[0], NewTri.Normal[0], NewTri.UV[0]);
                ExtractVert(verts[1], NewTri.Position[1], NewTri.Normal[1], NewTri.UV[1]);
                for (size_t i = 2; i < verts.size(); ++i)
                {
                    ExtractVert(verts[i], NewTri.Position[2], NewTri.Normal[2], NewTri.UV[2]);
                    Tris.push_back(NewTri);

                    NewTri.Position[1] = NewTri.Position[2];
                    NewTri.Normal[1]   = NewTri.Normal[2];
                    NewTri.UV[1]       = NewTri.UV[2];
                }
            }
        }
        objfile.close();
    }

    granny_mesh* Mesh = BuildMeshFromArrays(TempArena, Positions, Normals, UVs, Tris);

    granny_file_info Converted;
    memset(&Converted, 0, sizeof(Converted));

    Converted.MeshCount = 1;
    Converted.Meshes = &Mesh;

    granny_material_binding Binding = { 0 };
    Mesh->MaterialBindingCount = 1;
    Mesh->MaterialBindings = &Binding;
    if (MaterialSource != 0)
    {
        // Build a texture, a material that references it, and use the mesh binding to
        // point to it...
        granny_texture* Texture = CreateTextureFor(TempArena, MaterialSource);
        Converted.TextureCount = 1;
        Converted.Textures = &Texture;

        granny_material* DifTex = PushObject(TempArena, granny_material);
        memset(DifTex, 0, sizeof(*DifTex));

        DifTex->Name = MaterialSource;
        DifTex->Texture = Texture;

        granny_material* MeshMat = PushObject(TempArena, granny_material);
        memset(MeshMat, 0, sizeof(*MeshMat));

        MeshMat->Name = "Mesh";
        MeshMat->MapCount = 1;
        MeshMat->Maps = PushArray(TempArena, 1, granny_material_map);
        MeshMat->Maps->Usage = "Diffuse";
        MeshMat->Maps->Material = DifTex;

        Converted.MaterialCount = 2;
        Converted.Materials = PushArray(TempArena, 2, granny_material*);
        Converted.Materials[0] = DifTex;
        Converted.Materials[1] = MeshMat;

        Binding.Material = MeshMat;
    }

    // Write out the result
    return SimpleWriteInfo(KeyValues, NumKeyValues, &Converted, TempArena);
}

static CommandRegistrar RegOBJImport(OBJImport, "OBJImport",
                                     "Tries to import a single mesh from an OBJ format file");
