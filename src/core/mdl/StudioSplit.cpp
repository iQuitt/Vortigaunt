// Copyright (C) 2026 iQuit (https://github.com/iQuitt)
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "core/mdl/StudioSplit.h"

#include <algorithm>
#include <deque>

namespace
{

// MAXSTUDIOBODYPARTS of the engine
constexpr int kMaxBodyParts = 32;

std::pair<int, int> countOf(const StudioSubModel& sub)
{
    std::vector<char> usedVertex(sub.vertices.size(), 0);
    int vertices = 0, normals = 0;
    for (const StudioMesh& mesh : sub.meshes)
    {
        std::vector<char> usedNormal(sub.normals.size(), 0);
        for (const StudioTriVertex& corner : mesh.triangles)
        {
            if (corner.vertex >= 0 && corner.vertex < static_cast<int>(usedVertex.size()) && !usedVertex[corner.vertex]) usedVertex[corner.vertex] = 1, ++vertices;
            if (corner.normal >= 0 && corner.normal < static_cast<int>(usedNormal.size()) && !usedNormal[corner.normal]) usedNormal[corner.normal] = 1, ++normals;
        }
    }
    return { vertices, normals };
}

std::string numbered(const std::string& name, int piece)
{
    const std::string suffix = "_" + std::to_string(piece + 1);
    return name.substr(0, 63 - suffix.size()) + suffix;
}


std::vector<StudioSubModel> splitSubModel(const StudioSubModel& sub)
{
    struct Face
    {
        int mesh;
        size_t first;
    };
    std::vector<Face> faces;
    std::vector<std::vector<int>> facesOf(sub.vertices.size());
    for (size_t k = 0; k < sub.meshes.size(); ++k)
        for (size_t t = 0; t + 2 < sub.meshes[k].triangles.size(); t += 3)
        {
            for (int c = 0; c < 3; ++c)
            {
                const int v = sub.meshes[k].triangles[t + c].vertex;
                if (v >= 0 && v < static_cast<int>(facesOf.size())) facesOf[v].push_back(static_cast<int>(faces.size()));
            }
            faces.push_back({ static_cast<int>(k), t });
        }

    // per face its piece; a vertex or a mesh's normal is counted in a piece the first time the piece takes it
    std::vector<int> pieceOf(faces.size(), -1);
    std::vector<int> vertexIn(sub.vertices.size(), -1);
    std::vector<int> normalIn(sub.meshes.size() * sub.normals.size(), -1);
    int piece = 0, vertices = 0, normals = 0;
    std::deque<int> frontier;
    for (size_t seed = 0; seed < faces.size(); ++seed)
    {
        if (pieceOf[seed] >= 0) continue;
        frontier.push_back(static_cast<int>(seed));
        pieceOf[seed] = -2;  // queued
        while (!frontier.empty())
        {
            const int f = frontier.front();
            frontier.pop_front();
            const Face& face = faces[f];
            const StudioMesh& mesh = sub.meshes[face.mesh];
            auto newIn = [&](int p, int& newVertices, int& newNormals) {
                newVertices = newNormals = 0;
                for (int c = 0; c < 3; ++c)
                {
                    const StudioTriVertex& corner = mesh.triangles[face.first + c];
                    bool repeat = false;
                    for (int d = 0; d < c; ++d) repeat = repeat || mesh.triangles[face.first + d].vertex == corner.vertex;
                    if (!repeat && vertexIn[corner.vertex] != p) ++newVertices;
                    repeat = false;
                    for (int d = 0; d < c; ++d) repeat = repeat || mesh.triangles[face.first + d].normal == corner.normal;
                    if (!repeat && normalIn[face.mesh * sub.normals.size() + corner.normal] != p) ++newNormals;
                }
            };
            int newVertices = 0, newNormals = 0;
            newIn(piece, newVertices, newNormals);
            if (vertices + newVertices > kStudioMaxVertices || normals + newNormals > kStudioMaxVertices)
            {
                ++piece;
                vertices = normals = 0;
                newIn(piece, newVertices, newNormals);
            }
            vertices += newVertices, normals += newNormals;
            pieceOf[f] = piece;
            for (int c = 0; c < 3; ++c)
            {
                const StudioTriVertex& corner = mesh.triangles[face.first + c];
                vertexIn[corner.vertex] = piece;
                normalIn[face.mesh * sub.normals.size() + corner.normal] = piece;
                for (int next : facesOf[corner.vertex])
                    if (pieceOf[next] == -1)
                    {
                        pieceOf[next] = -2;
                        frontier.push_back(next);
                    }
            }
        }
    }

    // each piece a submodel of its own vertices, normals and meshes
    std::vector<StudioSubModel> pieces(static_cast<size_t>(piece) + 1);
    for (size_t p = 0; p < pieces.size(); ++p)
    {
        StudioSubModel& out = pieces[p];
        out.name = p == 0 ? sub.name : numbered(sub.name, static_cast<int>(p));
        std::vector<int> vertexAt(sub.vertices.size(), -1), normalAt(sub.normals.size(), -1);
        for (size_t k = 0; k < sub.meshes.size(); ++k)
        {
            StudioMesh mesh;
            mesh.texture = sub.meshes[k].texture;
            mesh.skinRef = sub.meshes[k].skinRef;
            for (size_t f = 0; f < faces.size(); ++f)
            {
                if (faces[f].mesh != static_cast<int>(k) || pieceOf[f] != static_cast<int>(p)) continue;
                for (int c = 0; c < 3; ++c)
                {
                    StudioTriVertex corner = sub.meshes[k].triangles[faces[f].first + c];
                    if (vertexAt[corner.vertex] < 0)
                    {
                        vertexAt[corner.vertex] = static_cast<int>(out.vertices.size());
                        out.vertices.push_back(sub.vertices[corner.vertex]);
                        out.vertexBones.push_back(sub.vertexBones[corner.vertex]);
                    }
                    if (normalAt[corner.normal] < 0)
                    {
                        normalAt[corner.normal] = static_cast<int>(out.normals.size());
                        out.normals.push_back(sub.normals[corner.normal]);
                        out.normalBones.push_back(sub.normalBones[corner.normal]);
                    }
                    corner.vertex = vertexAt[corner.vertex];
                    corner.normal = normalAt[corner.normal];
                    mesh.triangles.push_back(corner);
                }
            }
            // the first piece keeps every mesh, empty or not, as it is saved over the submodel it was
            if (!mesh.triangles.empty() || p == 0) out.meshes.push_back(std::move(mesh));
        }
    }
    return pieces;
}

}  // namespace

std::vector<StudioOversize> FindOversizedSubModels(const StudioModel& model)
{
    std::vector<StudioOversize> found;
    for (size_t b = 0; b < model.bodyParts.size(); ++b)
        for (size_t m = 0; m < model.bodyParts[b].models.size(); ++m)
        {
            const auto [vertices, normals] = countOf(model.bodyParts[b].models[m]);
            if (vertices <= kStudioMaxVertices && normals <= kStudioMaxVertices) continue;
            found.push_back({ static_cast<int>(b), static_cast<int>(m), vertices, normals, model.bodyParts[b].models.size() == 1 });
        }
    return found;
}

SplitResult SplitOversizedSubModels(StudioModel& model)
{
    SplitResult result;
    std::vector<StudioBodyPart> parts = model.bodyParts;
    std::vector<StudioBodyPart> added;
    for (const StudioOversize& over : FindOversizedSubModels(model))
    {
        if (!over.splittable)
        {
            ++result.skipped;
            continue;
        }
        std::vector<StudioSubModel> pieces = splitSubModel(model.bodyParts[over.part].models[over.model]);
        parts[over.part].models[over.model] = std::move(pieces[0]);
        for (size_t p = 1; p < pieces.size(); ++p)
        {
            StudioBodyPart part;
            part.name = numbered(model.bodyParts[over.part].name, static_cast<int>(p));
            part.models.push_back(std::move(pieces[p]));
            added.push_back(std::move(part));
        }
        ++result.split;
    }
    if (parts.size() + added.size() > static_cast<size_t>(kMaxBodyParts))
    {
        result = {};
        result.error = "Splitting would need " + std::to_string(parts.size() + added.size()) + " body parts; GoldSrc takes " + std::to_string(kMaxBodyParts) + ".";
        return result;
    }
    result.partsAdded = static_cast<int>(added.size());
    parts.insert(parts.end(), std::make_move_iterator(added.begin()), std::make_move_iterator(added.end()));
    model.bodyParts = std::move(parts);
    return result;
}
