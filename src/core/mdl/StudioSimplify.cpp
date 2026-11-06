// Copyright (C) 2026 iQuit (https://github.com/iQuitt)
// Additional terms under GPL section 7: see ADDITIONAL-TERMS.md

#include "core/mdl/StudioSimplify.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <queue>
#include <set>

namespace {

using Vec = aiVector3t<double>;
// a quadric as the 10 values of a symmetric 4x4
using Quadric = std::array<double, 10>;

struct Face
{
    int mesh = 0;
    bool chosen = false;
    bool alive = true;
    StudioTriVertex corner[3];
};

long long edgeKey(int a, int b)
{
    return (static_cast<long long>(std::min(a, b)) << 32) | static_cast<unsigned>(std::max(a, b));
}

Quadric planeQuadric(const Vec& n, double d, double weight)
{
    return Quadric{ n.x * n.x * weight, n.x * n.y * weight, n.x * n.z * weight, n.x * d * weight, n.y * n.y * weight,
                    n.y * n.z * weight, n.y * d * weight,  n.z * n.z * weight, n.z * d * weight,  d * d * weight };
}

void add(Quadric& a, const Quadric& b)
{
    for (int i = 0; i < 10; ++i) a[i] += b[i];
}

double error(const Quadric& q, const Vec& v)
{
    return q[0] * v.x * v.x + 2 * q[1] * v.x * v.y + 2 * q[2] * v.x * v.z + 2 * q[3] * v.x + q[4] * v.y * v.y + 2 * q[5] * v.y * v.z +
           2 * q[6] * v.y + q[7] * v.z * v.z + 2 * q[8] * v.z + q[9];
}

// a vertex that can move has one texture place; its corners keep their own normals
bool sameCorner(const StudioTriVertex& a, const StudioTriVertex& b)
{
    return std::abs(a.u - b.u) < 1e-6f && std::abs(a.v - b.v) < 1e-6f;
}

Vec faceNormal(const std::vector<Vec>& place, const Face& face)
{
    const Vec& a = place[face.corner[0].vertex];
    return (place[face.corner[1].vertex] - a) ^ (place[face.corner[2].vertex] - a);
}

void simplifySubModel(StudioSubModel& sub, const std::vector<aiMatrix4x4>& world, int texture, float keep, bool smoothNormals,
                      bool keepJoints, SimplifyResult& result)
{
    const int vertexCount = static_cast<int>(sub.vertices.size());
    const int normalCount = static_cast<int>(sub.normals.size());
    std::vector<Face> faces;
    int chosen = 0;
    bool broken = false;
    for (size_t m = 0; m < sub.meshes.size(); ++m)
    {
        const StudioMesh& mesh = sub.meshes[m];
        const bool picked = texture < 0 || mesh.texture == texture;
        for (size_t i = 0; i + 2 < mesh.triangles.size(); i += 3)
        {
            Face face;
            face.mesh = static_cast<int>(m);
            face.chosen = picked;
            for (int k = 0; k < 3; ++k)
            {
                face.corner[k] = mesh.triangles[i + k];
                broken = broken || face.corner[k].vertex < 0 || face.corner[k].vertex >= vertexCount;
            }
            chosen += picked;
            faces.push_back(face);
        }
    }
    result.before += chosen;
    const int target = std::max(1, static_cast<int>(std::lround(chosen * keep)));
    if (broken || chosen <= target)
    {
        result.after += chosen;
        return;
    }

    auto boneOf = [&sub](int v) { return v < static_cast<int>(sub.vertexBones.size()) ? sub.vertexBones[v] : -1; };
    auto matrixOf = [&world](int bone) { return bone >= 0 && bone < static_cast<int>(world.size()) ? world[bone] : aiMatrix4x4(); };
    // measured in the rest pose, where the parts of the model meet
    std::vector<Vec> place(vertexCount);
    for (int v = 0; v < vertexCount; ++v)
    {
        const aiVector3D p = matrixOf(boneOf(v)) * sub.vertices[v];
        place[v] = Vec(p.x, p.y, p.z);
    }

    // fixed: touches faces left out, so it stays where and as it is; seam: more than one texture place, so it
    // stays where it is and goes only along its seam; free: moves and slides its texture place
    std::vector<char> fixed(vertexCount, 0), seam(vertexCount, 0), seen(vertexCount, 0);
    std::vector<StudioTriVertex> look(vertexCount);
    std::vector<int> meshOf(vertexCount, -1);
    for (const Face& face : faces)
        for (const StudioTriVertex& c : face.corner)
        {
            const int v = c.vertex;
            if (!face.chosen)
                fixed[v] = 1;
            else if (!seen[v])
            {
                seen[v] = 1;
                look[v] = c;
                meshOf[v] = face.mesh;
            }
            else if (meshOf[v] != face.mesh)
                fixed[v] = 1;
            else if (!sameCorner(look[v], c))
                seam[v] = 1;
        }
    auto isFree = [&](int v) { return !fixed[v] && !seam[v]; };

    std::vector<Quadric> quadrics(vertexCount, Quadric{});
    std::vector<std::vector<int>> around(vertexCount);
    std::map<long long, std::vector<int>> edgeFaces;
    for (size_t f = 0; f < faces.size(); ++f)
    {
        const Face& face = faces[f];
        if (!face.chosen) continue;
        const Vec n = faceNormal(place, face);
        const double area = n.Length() * 0.5;
        const Vec unit = area > 0 ? n / (2 * area) : Vec(0, 0, 1);
        const Quadric q = planeQuadric(unit, -(unit * place[face.corner[0].vertex]), area);
        for (int k = 0; k < 3; ++k)
        {
            const int v = face.corner[k].vertex;
            add(quadrics[v], q);
            around[v].push_back(static_cast<int>(f));
            edgeFaces[edgeKey(v, face.corner[(k + 1) % 3].vertex)].push_back(static_cast<int>(f));
        }
    }
    auto cornerOf = [](const Face& face, int v) -> const StudioTriVertex* {
        for (const StudioTriVertex& c : face.corner)
            if (c.vertex == v) return &c;
        return nullptr;
    };
    // open borders and seams keep their lines: a heavy plane along them, upright to the face
    for (const auto& [key, list] : edgeFaces)
    {
        const int a = static_cast<int>(key >> 32), b = static_cast<int>(key & 0xffffffff);
        bool line = list.size() != 2;
        if (!line)
            for (int v : { a, b })
                line = line || !sameCorner(*cornerOf(faces[list[0]], v), *cornerOf(faces[list[1]], v));
        if (!line) continue;
        const Vec along = place[b] - place[a];
        for (int f : list)
        {
            const Vec n = faceNormal(place, faces[f]).Normalize();
            const Vec side = (along ^ n).Normalize();
            const Quadric q = planeQuadric(side, -(side * place[a]), 1000.0 * (along * along));
            add(quadrics[a], q);
            add(quadrics[b], q);
        }
    }

    // b goes into a; t is how far a moves toward b
    struct Candidate
    {
        double cost;
        int a, b, versionA, versionB;
        Vec to;
        double t;
        bool operator<(const Candidate& o) const { return cost > o.cost; }
    };
    std::vector<int> version(vertexCount, 0);
    std::priority_queue<Candidate> queue;
    auto consider = [&](int a, int b) {
        // across a joint the vertex left keeps its own bone
        if ((keepJoints && boneOf(a) != boneOf(b)) || (fixed[a] && fixed[b])) return;
        // the one that goes is never fixed, and free before a seam
        if (fixed[b] || (isFree(a) && !isFree(b))) std::swap(a, b);
        // a seam goes only into another seam or a fixed vertex
        if (seam[b] && isFree(a)) return;
        Quadric q = quadrics[a];
        add(q, quadrics[b]);
        Candidate best{ error(q, place[a]), a, b, version[a], version[b], place[a], 0.0 };
        if (isFree(a) && isFree(b))
            for (double t : { 1.0, 0.5 })
            {
                const Vec to = place[a] + (place[b] - place[a]) * t;
                const double cost = error(q, to);
                if (cost < best.cost) best.cost = cost, best.to = to, best.t = t;
            }
        best.cost = std::max(0.0, best.cost);
        queue.push(best);
    };
    for (const auto& [key, list] : edgeFaces) consider(static_cast<int>(key >> 32), static_cast<int>(key & 0xffffffff));

    auto neighbours = [&](int v) {
        std::set<int> out;
        for (int f : around[v])
            if (faces[f].alive)
                for (const StudioTriVertex& c : faces[f].corner)
                    if (c.vertex != v) out.insert(c.vertex);
        return out;
    };
    std::vector<char> moved(vertexCount, 0);
    int count = chosen;
    while (count > target && !queue.empty())
    {
        const Candidate c = queue.top();
        queue.pop();
        if (c.versionA != version[c.a] || c.versionB != version[c.b]) continue;
        // still an edge, and the mesh stays a surface: the two ends share only the corners of the faces on the edge
        std::vector<int> shared;
        for (int f : around[c.a])
            if (faces[f].alive && cornerOf(faces[f], c.b)) shared.push_back(f);
        if (shared.empty()) continue;
        const std::set<int> na = neighbours(c.a), nb = neighbours(c.b);
        size_t common = 0;
        for (int v : na) common += nb.count(v);
        if (common != shared.size()) continue;
        // each texture place of b becomes the place a has beside it on a face of the edge
        std::vector<std::pair<StudioTriVertex, StudioTriVertex>> becomes;
        bool mapped = true;
        for (int f : shared)
        {
            const StudioTriVertex from = *cornerOf(faces[f], c.b), to = *cornerOf(faces[f], c.a);
            for (const auto& [was, now] : becomes)
                if (sameCorner(was, from) && !sameCorner(now, to)) mapped = false;
            becomes.push_back({ from, to });
        }
        auto placeFor = [&](const StudioTriVertex& corner) -> const StudioTriVertex* {
            for (const auto& [was, now] : becomes)
                if (sameCorner(was, corner)) return &now;
            return nullptr;
        };
        for (int f : around[c.b])
            if (mapped && faces[f].alive && !placeFor(*cornerOf(faces[f], c.b))) mapped = false;
        if (!mapped) continue;
        // no face turns over or folds flat
        bool fine = true;
        for (int v : { c.a, c.b })
            for (int f : around[v])
            {
                if (!faces[f].alive || std::find(shared.begin(), shared.end(), f) != shared.end()) continue;
                const Face& face = faces[f];
                Vec p[3];
                for (int k = 0; k < 3; ++k)
                    p[k] = face.corner[k].vertex == c.a || face.corner[k].vertex == c.b ? c.to : place[face.corner[k].vertex];
                const Vec before = faceNormal(place, face);
                const Vec after = (p[1] - p[0]) ^ (p[2] - p[0]);
                if (after.Length() < 1e-9 || before * after < 0.3 * before.Length() * after.Length()) fine = false;
            }
        if (!fine) continue;

        for (int f : shared)
        {
            faces[f].alive = false;
            count--;
        }
        // two free ends: a moves and its one texture place slides with it
        const bool slide = isFree(c.a) && isFree(c.b);
        StudioTriVertex slid = look[c.a];
        if (slide)
        {
            slid.u = static_cast<float>(look[c.a].u + (look[c.b].u - look[c.a].u) * c.t);
            slid.v = static_cast<float>(look[c.a].v + (look[c.b].v - look[c.a].v) * c.t);
            look[c.a] = slid;
            place[c.a] = c.to;
            moved[c.a] = 1;
        }
        for (int v : { c.a, c.b })
            for (int f : around[v])
            {
                if (!faces[f].alive) continue;
                for (StudioTriVertex& corner : faces[f].corner)
                {
                    if (corner.vertex == c.b)
                    {
                        const StudioTriVertex now = slide ? slid : *placeFor(corner);
                        corner.vertex = c.a;
                        corner.u = now.u;
                        corner.v = now.v;
                        corner.normal = now.normal;
                    }
                    else if (corner.vertex == c.a && slide)
                        corner.u = slid.u, corner.v = slid.v;
                }
                if (v == c.b) around[c.a].push_back(f);
            }
        around[c.b].clear();
        add(quadrics[c.a], quadrics[c.b]);
        version[c.a]++;
        version[c.b]++;
        for (int v : neighbours(c.a)) consider(c.a, v);
    }
    result.after += count;

    // back to the bone's space
    for (int v = 0; v < vertexCount; ++v)
        if (moved[v])
        {
            aiMatrix4x4 inverse = matrixOf(boneOf(v));
            inverse.Inverse();
            sub.vertices[v] = inverse * aiVector3D(static_cast<float>(place[v].x), static_cast<float>(place[v].y), static_cast<float>(place[v].z));
        }

    // normals only the chosen faces use, as the mean of their faces
    if (smoothNormals)
    {
        std::vector<char> own(normalCount, 1);
        std::vector<Vec> sum(normalCount, Vec());
        for (const Face& face : faces)
        {
            if (!face.alive) continue;
            const Vec n = faceNormal(place, face);
            for (const StudioTriVertex& corner : face.corner)
            {
                if (corner.normal < 0 || corner.normal >= normalCount) continue;
                if (!face.chosen) own[corner.normal] = 0;
                sum[corner.normal] += n;
            }
        }
        for (int n = 0; n < normalCount; ++n)
        {
            if (!own[n] || sum[n].Length() <= 0.0) continue;
            const int bone = n < static_cast<int>(sub.normalBones.size()) ? sub.normalBones[n] : -1;
            aiMatrix3x3 turn(matrixOf(bone));
            turn.Inverse();
            const Vec unit = sum[n].Normalize();
            sub.normals[n] = (turn * aiVector3D(static_cast<float>(unit.x), static_cast<float>(unit.y), static_cast<float>(unit.z))).Normalize();
        }
    }

    // the faces left, and only the vertices and normals they use
    std::vector<int> vertexIndex(vertexCount, -1), normalIndex(normalCount, -1);
    for (const Face& face : faces)
        if (face.alive)
            for (const StudioTriVertex& corner : face.corner)
            {
                vertexIndex[corner.vertex] = 0;
                if (corner.normal >= 0 && corner.normal < normalCount) normalIndex[corner.normal] = 0;
            }
    auto compact = [](std::vector<int>& index, auto& values, std::vector<int>& bones) {
        int next = 0;
        for (size_t i = 0; i < index.size(); ++i)
        {
            if (index[i] < 0) continue;
            index[i] = next;
            values[next] = values[i];
            if (i < bones.size()) bones[next] = bones[i];
            ++next;
        }
        values.resize(next);
        if (bones.size() > static_cast<size_t>(next)) bones.resize(next);
    };
    compact(vertexIndex, sub.vertices, sub.vertexBones);
    compact(normalIndex, sub.normals, sub.normalBones);
    for (StudioMesh& mesh : sub.meshes) mesh.triangles.clear();
    for (const Face& face : faces)
    {
        if (!face.alive) continue;
        for (StudioTriVertex corner : face.corner)
        {
            corner.vertex = vertexIndex[corner.vertex];
            if (corner.normal >= 0 && corner.normal < normalCount) corner.normal = normalIndex[corner.normal];
            sub.meshes[face.mesh].triangles.push_back(corner);
        }
    }
}

}  // namespace

SimplifyResult SimplifyStudioModel(StudioModel& model, int part, int subModel, int texture, float keep, bool smoothNormals, bool keepJoints)
{
    SimplifyResult result;
    const std::vector<aiMatrix4x4> world = model.BoneTransforms(-1, 0);
    keep = std::clamp(keep, 0.0f, 1.0f);
    for (int p = 0; p < static_cast<int>(model.bodyParts.size()); ++p)
    {
        if (part >= 0 && p != part) continue;
        std::vector<StudioSubModel>& models = model.bodyParts[p].models;
        for (int s = 0; s < static_cast<int>(models.size()); ++s)
            if (subModel < 0 || s == subModel) simplifySubModel(models[s], world, texture, keep, smoothNormals, keepJoints, result);
    }
    return result;
}

int CountStudioTriangles(const StudioModel& model, int part, int subModel, int texture)
{
    size_t corners = 0;
    for (int p = 0; p < static_cast<int>(model.bodyParts.size()); ++p)
    {
        if (part >= 0 && p != part) continue;
        const std::vector<StudioSubModel>& models = model.bodyParts[p].models;
        for (int s = 0; s < static_cast<int>(models.size()); ++s)
        {
            if (subModel >= 0 && s != subModel) continue;
            for (const StudioMesh& mesh : models[s].meshes)
                if (texture < 0 || mesh.texture == texture) corners += mesh.triangles.size() / 3;
        }
    }
    return static_cast<int>(corners);
}
