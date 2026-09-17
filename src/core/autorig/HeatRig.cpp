#include "HeatRig.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <unordered_map>

namespace heatrig {
namespace {

// Vector maths uses assimp's aiVector3D, like the rest of the rigging code.
// Mind its operators: a * b is the DOT product and a ^ b the CROSS product,
// only a * scalar scales.

// Closest point on segment [s,e] to p.
aiVector3D ClosestOnSegment(const aiVector3D& p, const aiVector3D& s, const aiVector3D& e) {
    aiVector3D seg = e - s;
    float segLenSq = seg.SquareLength();
    if (segLenSq < 1e-12f) {
        return s;
    }
    float t = ((p - s) * seg) / segLenSq;
    t = std::max(0.0f, std::min(1.0f, t));
    return s + seg * t;
}

struct BoneTable {
    std::vector<int>              boneId;        // slot -> bone index as handed in
    std::vector<aiVector3D>             head;          // slot -> bone world position
    std::vector<std::vector<int>> segments;      // slot -> indices into the input segments
    std::vector<int>              parentSlot;    // slot -> slot of the bone it hangs off, or -1
};

struct WeldedMesh {
    std::vector<aiVector3D> positions;
    std::vector<aiVector3D> normals;
    std::vector<int>  triangles;     // 3 welded indices per triangle
    std::vector<int>  originalToWelded;
    std::vector<std::vector<int>> neighbors;
    std::vector<std::vector<int>> vertexTriangles;
};

struct Bounds {
    aiVector3D mn{ std::numeric_limits<float>::max(),  std::numeric_limits<float>::max(),  std::numeric_limits<float>::max() };
    aiVector3D mx{ -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max(), -std::numeric_limits<float>::max() };

    void Expand(const aiVector3D& p) {
        mn.x = std::min(mn.x, p.x); mn.y = std::min(mn.y, p.y); mn.z = std::min(mn.z, p.z);
        mx.x = std::max(mx.x, p.x); mx.y = std::max(mx.y, p.y); mx.z = std::max(mx.z, p.z);
    }
    void Expand(const Bounds& b) { Expand(b.mn); Expand(b.mx); }
    aiVector3D Center() const { return (mn + mx) * 0.5f; }
    int  WidestAxis() const {
        aiVector3D d = mx - mn;
        if (d.x >= d.y && d.x >= d.z) return 0;
        return (d.y >= d.z) ? 1 : 2;
    }
};

inline float AxisOf(const aiVector3D& v, int axis) { return axis == 0 ? v.x : (axis == 1 ? v.y : v.z); }


struct CellKey {
    int x, y, z;
    bool operator==(const CellKey& o) const { return x == o.x && y == o.y && z == o.z; }
};

struct CellHash {
    size_t operator()(const CellKey& k) const {
        // Three large primes; good enough for the vertex counts GoldSrc models reach.
        size_t h = static_cast<size_t>(static_cast<uint32_t>(k.x)) * 73856093u;
        h ^= static_cast<size_t>(static_cast<uint32_t>(k.y)) * 19349663u;
        h ^= static_cast<size_t>(static_cast<uint32_t>(k.z)) * 83492791u;
        return h;
    }
};



void WeldVertices(const std::vector<float>& positions, float epsilon, WeldedMesh& out) {
    size_t inputCount = positions.size() / 3;
    out.originalToWelded.assign(inputCount, -1);

    float cellSize = std::max(epsilon, 1e-5f);
    float epsSq = epsilon * epsilon;
    std::unordered_map<CellKey, std::vector<int>, CellHash> grid;

    auto cellOf = [cellSize](const aiVector3D& p) -> CellKey {
        return {static_cast<int>(std::floor(p.x / cellSize)),
                static_cast<int>(std::floor(p.y / cellSize)),
                static_cast<int>(std::floor(p.z / cellSize))};
    };

    for (size_t i = 0; i < inputCount; i++) {
        aiVector3D p(positions[i * 3 + 0], positions[i * 3 + 1], positions[i * 3 + 2]);
        CellKey base = cellOf(p);

        int found = -1;
        for (int dx = -1; dx <= 1 && found < 0; dx++) {
            for (int dy = -1; dy <= 1 && found < 0; dy++) {
                for (int dz = -1; dz <= 1 && found < 0; dz++) {
                    auto it = grid.find({base.x + dx, base.y + dy, base.z + dz});
                    if (it == grid.end()) continue;
                    for (int candidate : it->second) {
                        if ((out.positions[candidate] - p).SquareLength() <= epsSq) {
                            found = candidate;
                            break;
                        }
                    }
                }
            }
        }

        if (found < 0) {
            found = static_cast<int>(out.positions.size());
            out.positions.push_back(p);
            grid[base].push_back(found);
        }
        out.originalToWelded[i] = found;
    }
}

void BuildTopology(WeldedMesh& mesh, size_t inputVertexCount) {
    size_t triCount = inputVertexCount / 3;
    mesh.triangles.reserve(triCount * 3);

    for (size_t t = 0; t < triCount; t++) {
        int a = mesh.originalToWelded[t * 3 + 0];
        int b = mesh.originalToWelded[t * 3 + 1];
        int c = mesh.originalToWelded[t * 3 + 2];
        if (a == b || b == c || a == c) {
            continue;  // collapsed by welding, carries no topology
        }
        mesh.triangles.push_back(a);
        mesh.triangles.push_back(b);
        mesh.triangles.push_back(c);
    }

    size_t vertexCount = mesh.positions.size();
    mesh.neighbors.assign(vertexCount, {});
    mesh.vertexTriangles.assign(vertexCount, {});

    for (size_t t = 0; t * 3 < mesh.triangles.size(); t++) {
        int idx[3] = {mesh.triangles[t * 3 + 0], mesh.triangles[t * 3 + 1], mesh.triangles[t * 3 + 2]};
        for (int k = 0; k < 3; k++) {
            mesh.vertexTriangles[idx[k]].push_back(static_cast<int>(t));
            for (int j = 0; j < 3; j++) {
                if (j == k) continue;
                auto& list = mesh.neighbors[idx[k]];
                if (std::find(list.begin(), list.end(), idx[j]) == list.end()) {
                    list.push_back(idx[j]);
                }
            }
        }
    }
}

// Area weighted vertex normals; used to push bones that sit on the wrong side of
// the surface away, the way Blender's heat_source_distance does.
void ComputeNormals(WeldedMesh& mesh) {
    mesh.normals.assign(mesh.positions.size(), aiVector3D());

    for (size_t t = 0; t * 3 < mesh.triangles.size(); t++) {
        const aiVector3D& a = mesh.positions[mesh.triangles[t * 3 + 0]];
        const aiVector3D& b = mesh.positions[mesh.triangles[t * 3 + 1]];
        const aiVector3D& c = mesh.positions[mesh.triangles[t * 3 + 2]];
        aiVector3D n = (b - a) ^ (c - a);  // cross product, length is twice the triangle area
        for (int k = 0; k < 3; k++) {
            int v = mesh.triangles[t * 3 + k];
            mesh.normals[v] = mesh.normals[v] + n;
        }
    }

    for (aiVector3D& n : mesh.normals) {
        n.NormalizeSafe();
    }
}


class TriangleBvh {
public:
    void Build(const WeldedMesh& mesh) {
        m_mesh = &mesh;
        size_t triCount = mesh.triangles.size() / 3;
        m_indices.resize(triCount);
        for (size_t i = 0; i < triCount; i++) {
            m_indices[i] = static_cast<int>(i);
        }
        m_nodes.clear();
        if (triCount == 0) {
            return;
        }
        m_nodes.reserve(triCount * 2);
        BuildNode(0, static_cast<int>(triCount));
    }

    // True if any triangle blocks the straight line from `origin` to `target`.
    // Triangles touching `skipVertex` are ignored so a vertex never occludes itself.
    bool IsOccluded(const aiVector3D& origin, const aiVector3D& target, int skipVertex) const {
        if (m_nodes.empty()) {
            return false;
        }
        aiVector3D dir = target - origin;
        float len = dir.Length();
        if (len < 1e-6f) {
            return false;
        }
        dir = dir * (1.0f / len);

        // Start slightly off the surface, and stop slightly short of the bone.
        const float tMin = std::min(1e-3f, len * 0.01f);
        const float tMax = len * 0.999f;

        aiVector3D invDir(SafeInv(dir.x), SafeInv(dir.y), SafeInv(dir.z));
        return Traverse(0, origin, dir, invDir, tMin, tMax, skipVertex);
    }

private:
    struct Node {
        Bounds bounds;
        int start = 0;
        int count = 0;
        int left = -1;
        int right = -1;
    };

    static float SafeInv(float v) {
        return (std::abs(v) < 1e-12f) ? std::numeric_limits<float>::max() : 1.0f / v;
    }

    Bounds TriangleBounds(int tri) const {
        Bounds b;
        for (int k = 0; k < 3; k++) {
            b.Expand(m_mesh->positions[m_mesh->triangles[tri * 3 + k]]);
        }
        return b;
    }

    int BuildNode(int start, int count) {
        int nodeIndex = static_cast<int>(m_nodes.size());
        m_nodes.push_back(Node{});

        Bounds bounds;
        Bounds centroidBounds;
        for (int i = start; i < start + count; i++) {
            Bounds tb = TriangleBounds(m_indices[i]);
            bounds.Expand(tb);
            centroidBounds.Expand(tb.Center());
        }
        m_nodes[nodeIndex].bounds = bounds;

        constexpr int kLeafSize = 4;
        if (count <= kLeafSize) {
            m_nodes[nodeIndex].start = start;
            m_nodes[nodeIndex].count = count;
            return nodeIndex;
        }

        int axis = centroidBounds.WidestAxis();
        float split = AxisOf(centroidBounds.Center(), axis);

        auto middle = std::partition(m_indices.begin() + start, m_indices.begin() + start + count,
                                     [&](int tri) {
                                         return AxisOf(TriangleBounds(tri).Center(), axis) < split;
                                     });
        int leftCount = static_cast<int>(middle - (m_indices.begin() + start));
        if (leftCount == 0 || leftCount == count) {
            leftCount = count / 2;  // degenerate spread, fall back to a median split
        }

        int left = BuildNode(start, leftCount);
        int right = BuildNode(start + leftCount, count - leftCount);
        m_nodes[nodeIndex].left = left;
        m_nodes[nodeIndex].right = right;
        return nodeIndex;
    }

    static bool SlabTest(const Bounds& b, const aiVector3D& origin, const aiVector3D& invDir, float tMin, float tMax) {
        float t0 = (b.mn.x - origin.x) * invDir.x;
        float t1 = (b.mx.x - origin.x) * invDir.x;
        if (t0 > t1) std::swap(t0, t1);
        tMin = std::max(tMin, t0); tMax = std::min(tMax, t1);
        if (tMin > tMax) return false;

        t0 = (b.mn.y - origin.y) * invDir.y;
        t1 = (b.mx.y - origin.y) * invDir.y;
        if (t0 > t1) std::swap(t0, t1);
        tMin = std::max(tMin, t0); tMax = std::min(tMax, t1);
        if (tMin > tMax) return false;

        t0 = (b.mn.z - origin.z) * invDir.z;
        t1 = (b.mx.z - origin.z) * invDir.z;
        if (t0 > t1) std::swap(t0, t1);
        tMin = std::max(tMin, t0); tMax = std::min(tMax, t1);
        return tMin <= tMax;
    }

    // Moller-Trumbore, double sided: the mesh may not be consistently wound.
    bool RayHitsTriangle(int tri, const aiVector3D& origin, const aiVector3D& dir,
                         float tMin, float tMax, int skipVertex) const {
        int i0 = m_mesh->triangles[tri * 3 + 0];
        int i1 = m_mesh->triangles[tri * 3 + 1];
        int i2 = m_mesh->triangles[tri * 3 + 2];
        if (i0 == skipVertex || i1 == skipVertex || i2 == skipVertex) {
            return false;
        }

        const aiVector3D& v0 = m_mesh->positions[i0];
        aiVector3D edge1 = m_mesh->positions[i1] - v0;
        aiVector3D edge2 = m_mesh->positions[i2] - v0;

        aiVector3D pvec = dir ^ edge2;
        float det = edge1 * pvec;
        if (std::abs(det) < 1e-9f) {
            return false;  // ray parallel to the triangle
        }
        float invDet = 1.0f / det;

        aiVector3D tvec = origin - v0;
        float u = (tvec * pvec) * invDet;
        if (u < 0.0f || u > 1.0f) return false;

        aiVector3D qvec = tvec ^ edge1;
        float v = (dir * qvec) * invDet;
        if (v < 0.0f || u + v > 1.0f) return false;

        float t = (edge2 * qvec) * invDet;
        return t > tMin && t < tMax;
    }

    bool Traverse(int nodeIndex, const aiVector3D& origin, const aiVector3D& dir, const aiVector3D& invDir,
                  float tMin, float tMax, int skipVertex) const {
        const Node& node = m_nodes[nodeIndex];
        if (!SlabTest(node.bounds, origin, invDir, tMin, tMax)) {
            return false;
        }
        if (node.count > 0) {
            for (int i = node.start; i < node.start + node.count; i++) {
                if (RayHitsTriangle(m_indices[i], origin, dir, tMin, tMax, skipVertex)) {
                    return true;
                }
            }
            return false;
        }
        return Traverse(node.left, origin, dir, invDir, tMin, tMax, skipVertex) ||
               Traverse(node.right, origin, dir, invDir, tMin, tMax, skipVertex);
    }

    const WeldedMesh* m_mesh = nullptr;
    std::vector<Node> m_nodes;
    std::vector<int>  m_indices;
};


struct SparseMatrix {
    int n = 0;
    std::vector<int>    rowStart;   // n + 1 entries
    std::vector<int>    colIndex;
    std::vector<double> values;
    std::vector<double> diagonal;
};

void Multiply(const SparseMatrix& m, const std::vector<double>& x, std::vector<double>& out) {
    out.assign(m.n, 0.0);
    for (int i = 0; i < m.n; i++) {
        double sum = 0.0;
        for (int k = m.rowStart[i]; k < m.rowStart[i + 1]; k++) {
            sum += m.values[k] * x[m.colIndex[k]];
        }
        out[i] = sum;
    }
}

bool SolveCG(const SparseMatrix& m, const std::vector<double>& rhs,
             std::vector<double>& x, int maxIterations, double tolerance) {
    int n = m.n;
    x.assign(n, 0.0);

    std::vector<double> residual = rhs;
    std::vector<double> preconditioned(n);
    std::vector<double> direction(n);
    std::vector<double> temp(n);

    double rhsNorm = 0.0;
    for (double v : rhs) rhsNorm += v * v;
    if (rhsNorm < 1e-30) {
        return true;  // zero right hand side, zero solution
    }

    for (int i = 0; i < n; i++) {
        double d = m.diagonal[i];
        preconditioned[i] = (std::abs(d) > 1e-20) ? residual[i] / d : residual[i];
    }
    direction = preconditioned;

    double rz = 0.0;
    for (int i = 0; i < n; i++) rz += residual[i] * preconditioned[i];

    double threshold = tolerance * tolerance * rhsNorm;

    for (int iteration = 0; iteration < maxIterations; iteration++) {
        Multiply(m, direction, temp);

        double dAd = 0.0;
        for (int i = 0; i < n; i++) dAd += direction[i] * temp[i];
        if (std::abs(dAd) < 1e-30) {
            break;
        }

        double alpha = rz / dAd;
        double residualNorm = 0.0;
        for (int i = 0; i < n; i++) {
            x[i] += alpha * direction[i];
            residual[i] -= alpha * temp[i];
            residualNorm += residual[i] * residual[i];
        }
        if (residualNorm < threshold) {
            return true;
        }

        for (int i = 0; i < n; i++) {
            double d = m.diagonal[i];
            preconditioned[i] = (std::abs(d) > 1e-20) ? residual[i] / d : residual[i];
        }

        double rzNext = 0.0;
        for (int i = 0; i < n; i++) rzNext += residual[i] * preconditioned[i];
        if (std::abs(rz) < 1e-30) {
            break;
        }
        double beta = rzNext / rz;
        rz = rzNext;

        for (int i = 0; i < n; i++) {
            direction[i] = preconditioned[i] + beta * direction[i];
        }
    }

    return true;  // an unconverged solve is still a usable approximation here
}

// Cotangent of the angle at `corner` in the triangle (corner, a, b).
double CotangentAt(const aiVector3D& corner, const aiVector3D& a, const aiVector3D& b) {
    aiVector3D u = a - corner;
    aiVector3D v = b - corner;
    double cross = static_cast<double>((u ^ v).Length());
    if (cross < 1e-12) {
        return 0.0;
    }
    return static_cast<double>(u * v) / cross;
}

void BuildLaplacian(const WeldedMesh& mesh, std::vector<std::unordered_map<int, double>>& edgeWeights, std::vector<double>& vertexAreas) {
    int n = static_cast<int>(mesh.positions.size());
    edgeWeights.assign(n, {});
    vertexAreas.assign(n, 0.0);

    size_t triCount = mesh.triangles.size() / 3;
    for (size_t t = 0; t < triCount; t++) {
        int i0 = mesh.triangles[t * 3 + 0];
        int i1 = mesh.triangles[t * 3 + 1];
        int i2 = mesh.triangles[t * 3 + 2];
        const aiVector3D& p0 = mesh.positions[i0];
        const aiVector3D& p1 = mesh.positions[i1];
        const aiVector3D& p2 = mesh.positions[i2];

        double area = 0.5 * static_cast<double>(((p1 - p0) ^ (p2 - p0)).Length());
        if (area < 1e-12) {
            continue;
        }
        double share = area / 3.0;
        vertexAreas[i0] += share;
        vertexAreas[i1] += share;
        vertexAreas[i2] += share;

        // The cotangent at each corner weights the edge opposite to it.
        const int   edgeA[3]  = {i1, i2, i0};
        const int   edgeB[3]  = {i2, i0, i1};
        const aiVector3D* pos[3]    = {&p0, &p1, &p2};
        const aiVector3D* posA[3]   = {&p1, &p2, &p0};
        const aiVector3D* posB[3]   = {&p2, &p0, &p1};

        for (int k = 0; k < 3; k++) {
            double cot = CotangentAt(*pos[k], *posA[k], *posB[k]);
            double w = 0.5 * std::max(0.0, cot);
            if (w <= 0.0) {
                continue;
            }
            edgeWeights[edgeA[k]][edgeB[k]] += w;
            edgeWeights[edgeB[k]][edgeA[k]] += w;
        }
    }

    for (double& a : vertexAreas) {
        a = std::max(a, 1e-8);
    }
}


BoneTable BuildBoneTable(const std::vector<BoneSegment>& segments) {
    BoneTable table;
    std::unordered_map<int, int> idToSlot;

    for (size_t i = 0; i < segments.size(); i++) {
        const BoneSegment& s = segments[i];
        auto it = idToSlot.find(s.boneIndex);
        int slot;
        if (it == idToSlot.end()) {
            slot = static_cast<int>(table.boneId.size());
            idToSlot[s.boneIndex] = slot;
            table.boneId.push_back(s.boneIndex);
            table.head.push_back(s.start);
            table.segments.push_back({});
        } else {
            slot = it->second;
        }
        table.segments[slot].push_back(static_cast<int>(i));
    }

    // A bone is the child of whichever bone has a segment ending on its head.
    table.parentSlot.assign(table.boneId.size(), -1);
    const float jointTolerance = 0.01f;
    for (size_t parent = 0; parent < table.boneId.size(); parent++) {
        for (int segIndex : table.segments[parent]) {
            const aiVector3D& tip = segments[segIndex].end;
            for (size_t child = 0; child < table.boneId.size(); child++) {
                if (child == parent) continue;
                if ((table.head[child] - tip).Length() <= jointTolerance) {
                    table.parentSlot[child] = static_cast<int>(parent);
                }
            }
        }
    }
    return table;
}


float SourceDistance(const aiVector3D& position, const aiVector3D& normal, const BoneTable& table, int slot, const std::vector<BoneSegment>& segments, aiVector3D& outClosest) {

    float best = std::numeric_limits<float>::max();
    outClosest = position;

    for (int segIndex : table.segments[slot]) {
        const BoneSegment& s = segments[segIndex];
        aiVector3D closest = ClosestOnSegment(position, s.start, s.end);
        aiVector3D delta = position - closest;
        float dist = delta.Length();

        float scaled = dist;
        if (dist > 1e-6f) {
            float cosine = (delta * (1.0f / dist)) * normal;
            scaled = dist / (0.5f * (cosine + 1.001f));
        }
        if (scaled < best) {
            best = scaled;
            outClosest = closest;
        }
    }
    return best;
}

} // namespace


Result Solve(const std::vector<float>& positions, const std::vector<BoneSegment>& segments, const Options& options) {
    Result result;

    size_t inputCount = positions.size() / 3;
    if (inputCount == 0) {
        result.error = "No vertices to rig.";
        return result;
    }
    if (segments.empty()) {
        result.error = "No bone segments supplied.";
        return result;
    }

    WeldedMesh mesh;
    WeldVertices(positions, options.weldEpsilon, mesh);
    BuildTopology(mesh, inputCount);
    ComputeNormals(mesh);

    int n = static_cast<int>(mesh.positions.size());
    result.weldedVertexCount = n;
    result.triangleCount = static_cast<int>(mesh.triangles.size() / 3);

    BoneTable table = BuildBoneTable(segments);
    int slotCount = static_cast<int>(table.boneId.size());

    TriangleBvh bvh;
    if (options.useVisibility) {
        bvh.Build(mesh);
    }

    std::vector<std::vector<int>> closestSlots(n);
    std::vector<double> heat(n, 0.0);

    std::vector<std::pair<float, int>> ranked;
    ranked.reserve(slotCount);
    std::vector<aiVector3D> closestPoints(slotCount);

    std::vector<int>   fallbackSlot(n, 0);
    std::vector<float> fallbackDistance(n, 1.0f);

    const float kTieBand = 1.05f;       // sources this close to the nearest share the vertex
    const float kHiddenPenalty = 0.05f; // strength for vertices no bone can see

    for (int v = 0; v < n; v++) {
        const aiVector3D& position = mesh.positions[v];
        const aiVector3D& normal = mesh.normals[v];

        ranked.clear();
        for (int slot = 0; slot < slotCount; slot++) {
            ranked.emplace_back(
                SourceDistance(position, normal, table, slot, segments, closestPoints[slot]), slot);
        }
        std::sort(ranked.begin(), ranked.end());

        float acceptedDistance = -1.0f;
        for (const auto& entry : ranked) {
            if (acceptedDistance > 0.0f && entry.first > acceptedDistance * kTieBand) {
                break;
            }
            if (options.useVisibility && bvh.IsOccluded(position, closestPoints[entry.second], v)) {
                continue;
            }
            if (acceptedDistance < 0.0f) {
                acceptedDistance = std::max(entry.first, 1e-2f);
            }
            closestSlots[v].push_back(entry.second);
        }

        fallbackSlot[v] = ranked.front().second;
        fallbackDistance[v] = std::max(ranked.front().first, 1e-2f);

        if (closestSlots[v].empty()) {
            result.unreachedVertexCount++;
            heat[v] = 0.0;
            continue;
        }

        int count = static_cast<int>(closestSlots[v].size());
        heat[v] = static_cast<double>(count) * static_cast<double>(options.heatStrength) /
                  (static_cast<double>(acceptedDistance) * static_cast<double>(acceptedDistance));
    }

    
    std::vector<int> component(n, -1);
    std::vector<int> stack;
    int componentCount = 0;
    for (int start = 0; start < n; start++) {
        if (component[start] >= 0) {
            continue;
        }
        stack.clear();
        stack.push_back(start);
        component[start] = componentCount;

        double total = 0.0;
        std::vector<int> members;
        while (!stack.empty()) {
            int v = stack.back();
            stack.pop_back();
            members.push_back(v);
            total += heat[v];
            for (int neighbor : mesh.neighbors[v]) {
                if (component[neighbor] < 0) {
                    component[neighbor] = componentCount;
                    stack.push_back(neighbor);
                }
            }
        }
        componentCount++;

        if (total > 0.0) {
            continue;
        }
        for (int v : members) {
            closestSlots[v].push_back(fallbackSlot[v]);
            double d = static_cast<double>(fallbackDistance[v]);
            heat[v] = static_cast<double>(options.heatStrength) * static_cast<double>(kHiddenPenalty) / (d * d);
        }
    }
    

    std::vector<std::unordered_map<int, double>> edgeWeights;
    std::vector<double> vertexAreas;
    BuildLaplacian(mesh, edgeWeights, vertexAreas);

    SparseMatrix matrix;
    matrix.n = n;
    matrix.rowStart.assign(n + 1, 0);
    matrix.diagonal.assign(n, 0.0);

    for (int i = 0; i < n; i++) {
        matrix.rowStart[i + 1] = matrix.rowStart[i] + static_cast<int>(edgeWeights[i].size()) + 1;
    }
    matrix.colIndex.resize(matrix.rowStart[n]);
    matrix.values.resize(matrix.rowStart[n]);

    for (int i = 0; i < n; i++) {
        double diagonal = vertexAreas[i] * heat[i];
        int cursor = matrix.rowStart[i];
        for (const auto& entry : edgeWeights[i]) {
            matrix.colIndex[cursor] = entry.first;
            matrix.values[cursor] = -entry.second;
            diagonal += entry.second;
            cursor++;
        }
        matrix.colIndex[cursor] = i;
        matrix.values[cursor] = diagonal;
        matrix.diagonal[i] = diagonal;
    }

    std::vector<float> bestWeight(n, -1.0f), secondWeight(n, -1.0f);
    std::vector<int>   bestSlot(n, -1), secondSlot(n, -1);

    std::vector<bool> slotActive(slotCount, false);
    for (int v = 0; v < n; v++) {
        for (int slot : closestSlots[v]) {
            slotActive[slot] = true;
        }
    }

    std::vector<double> rhs(n), weights;
    for (int slot = 0; slot < slotCount; slot++) {
        if (!slotActive[slot]) {
            continue;  // no source anywhere, the solution would be all zeroes
        }
        result.solvedBoneCount++;

        for (int v = 0; v < n; v++) {
            const auto& list = closestSlots[v];
            bool isSource = std::find(list.begin(), list.end(), slot) != list.end();
            rhs[v] = isSource ? vertexAreas[v] * heat[v] / static_cast<double>(list.size()) : 0.0;
        }

        SolveCG(matrix, rhs, weights, options.maxIterations, static_cast<double>(options.tolerance));

        for (int v = 0; v < n; v++) {
            float w = static_cast<float>(weights[v]);
            if (w > bestWeight[v]) {
                secondWeight[v] = bestWeight[v];
                secondSlot[v] = bestSlot[v];
                bestWeight[v] = w;
                bestSlot[v] = slot;
            } else if (w > secondWeight[v]) {
                secondWeight[v] = w;
                secondSlot[v] = slot;
            }
        }
    }

    std::vector<int> assigned(n);
    std::vector<float> margin(n);
    for (int v = 0; v < n; v++) {
        assigned[v] = (bestSlot[v] >= 0) ? bestSlot[v] : 0;
        margin[v] = std::max(0.0f, bestWeight[v] - std::max(0.0f, secondWeight[v]));
    }

    for (int pass = 0; pass < options.cleanupPasses; pass++) {
        bool changed = false;
        std::vector<int> next = assigned;
        for (int v = 0; v < n; v++) {
            if (margin[v] >= options.undecidedMargin || mesh.neighbors[v].empty()) {
                continue;
            }
            std::unordered_map<int, int> votes;
            votes[assigned[v]] += 1;
            for (int neighbor : mesh.neighbors[v]) {
                votes[assigned[neighbor]] += 1;
            }
            int winner = assigned[v];
            int winnerVotes = 0;
            for (const auto& entry : votes) {
                if (entry.second > winnerVotes) {
                    winnerVotes = entry.second;
                    winner = entry.first;
                }
            }
            if (winner != assigned[v]) {
                next[v] = winner;
                changed = true;
            }
        }
        assigned.swap(next);
        if (!changed) {
            break;
        }
    }


    if (options.minRegionSize > 0) {
        for (int sweep = 0; sweep < 2; sweep++) {
            std::vector<int> regionOf(n, -1);
            std::vector<std::vector<int>> regions;
            std::vector<int> stack;

            for (int start = 0; start < n; start++) {
                if (regionOf[start] >= 0) {
                    continue;
                }
                int id = static_cast<int>(regions.size());
                regions.push_back({});
                stack.clear();
                stack.push_back(start);
                regionOf[start] = id;
                while (!stack.empty()) {
                    int v = stack.back();
                    stack.pop_back();
                    regions[id].push_back(v);
                    for (int neighbor : mesh.neighbors[v]) {
                        if (regionOf[neighbor] < 0 && assigned[neighbor] == assigned[v]) {
                            regionOf[neighbor] = id;
                            stack.push_back(neighbor);
                        }
                    }
                }
            }

            bool changed = false;
            for (const std::vector<int>& region : regions) {
                if (static_cast<int>(region.size()) >= options.minRegionSize) {
                    continue;
                }
                float marginSum = 0.0f;
                for (int v : region) {
                    marginSum += margin[v];
                }
                if (marginSum / static_cast<float>(region.size()) >= options.undecidedMargin) {
					continue;  // the solver was sure, this is a real small bone like a finger bone, leave it alone
                }

                std::unordered_map<int, int> surrounding;
                for (int v : region) {
                    for (int neighbor : mesh.neighbors[v]) {
                        if (regionOf[neighbor] != regionOf[v]) {
                            surrounding[assigned[neighbor]]++;
                        }
                    }
                }
                if (surrounding.empty()) {
                    continue;  // a free floating shell, nothing to absorb it
                }

                int winner = -1;
                int winnerVotes = 0;
                for (const auto& entry : surrounding) {
                    if (entry.second > winnerVotes) {
                        winnerVotes = entry.second;
                        winner = entry.first;
                    }
                }
                for (int v : region) {
                    assigned[v] = winner;
                }
                result.absorbedRegionCount++;
                changed = true;
            }
            if (!changed) {
                break;
            }
        }
    }



    if (options.pivotSnap > 0.0f) {
        float band = options.undecidedMargin * options.pivotSnap;
        for (int v = 0; v < n; v++) {
            int a = assigned[v];
            int b = secondSlot[v];
            if (b < 0 || a == b || margin[v] >= band) {
                continue;
            }
            int parent = -1, child = -1;
            if (table.parentSlot[b] == a) {
                parent = a; child = b;
            } else if (table.parentSlot[a] == b) {
                parent = b; child = a;
            } else {
                continue;  // not a shared joint, leave the answer from the solver alone
            }

            aiVector3D axis = table.head[child] - table.head[parent];
            axis.NormalizeSafe();
            if (axis.SquareLength() < 0.5f) {
                continue;  // zero length bone, no meaningful cutting plane
            }
            assigned[v] = (((mesh.positions[v] - table.head[child]) * axis) >= 0.0f) ? child : parent;
        }
    }

    result.boneIndices.resize(inputCount);
    result.confidence.resize(inputCount);
    for (size_t i = 0; i < inputCount; i++) {
        int welded = mesh.originalToWelded[i];
        result.boneIndices[i] = table.boneId[assigned[welded]];
        result.confidence[i] = margin[welded];
    }

    result.ok = true;
    return result;
}

} // namespace heatrig
