#include "render/CSG.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

struct CVertex {
    glm::dvec3 pos;
    glm::dvec3 normal;
    glm::dvec2 uv;
};

struct CPolygon {
    std::vector<CVertex> verts;
    glm::dvec3 n;
};

struct Tri {
    glm::dvec3 p[3];
    glm::dvec3 n;
    double w = 0.0;
    glm::dvec3 lo, hi;
};

struct Box {
    glm::dvec3 lo = glm::dvec3(1e300), hi = glm::dvec3(-1e300);
    void Add(const glm::dvec3& p) { lo = glm::min(lo, p); hi = glm::max(hi, p); }
    bool Overlaps(const glm::dvec3& a, const glm::dvec3& b, double pad) const {
        return a.x <= hi.x + pad && b.x >= lo.x - pad && a.y <= hi.y + pad && b.y >= lo.y - pad && a.z <= hi.z + pad && b.z >= lo.z - pad;
    }
};

static std::vector<Tri> ToTris(const RawMesh& mesh) {
    std::vector<Tri> out;
    out.reserve(mesh.indices.size() / 3);
    for (size_t k = 0; k + 2 < mesh.indices.size(); k += 3) {
        Tri t;
        for (int j = 0; j < 3; j++) t.p[j] = glm::dvec3(mesh.vertices[mesh.indices[k + j]].position);
        glm::dvec3 n = glm::cross(t.p[1] - t.p[0], t.p[2] - t.p[0]);
        double length = glm::length(n);
        if (length < 1e-30) continue;
        t.n = n / length;
        t.w = glm::dot(t.n, t.p[0]);
        t.lo = glm::min(glm::min(t.p[0], t.p[1]), t.p[2]);
        t.hi = glm::max(glm::max(t.p[0], t.p[1]), t.p[2]);
        out.push_back(t);
    }
    return out;
}

// Grid for fast lookups
struct Grid3 {
    glm::dvec3 lo, cell;
    int n[3] = { 1, 1, 1 };
    std::vector<std::vector<int>> cells;

    void Build(const std::vector<Tri>& tris, const std::vector<int>& use, const Box& box) {
        lo = box.lo;
        glm::dvec3 size = glm::max(box.hi - box.lo, glm::dvec3(1e-12));
        int count = std::max(1, (int)std::cbrt((double)use.size()));
        for (int k = 0; k < 3; k++) n[k] = std::min(64, std::max(1, count));
        cell = size / glm::dvec3(n[0], n[1], n[2]);
        cells.assign((size_t)n[0] * n[1] * n[2], std::vector<int>());
        for (int i : use) {
            int a[3], b[3];
            Range(tris[i].lo, tris[i].hi, a, b);
            for (int x = a[0]; x <= b[0]; x++)
                for (int y = a[1]; y <= b[1]; y++)
                    for (int z = a[2]; z <= b[2]; z++) cells[((size_t)z * n[1] + y) * n[0] + x].push_back(i);
        }
    }

    void Range(const glm::dvec3& p, const glm::dvec3& q, int* a, int* b) const {
        for (int k = 0; k < 3; k++) {
            a[k] = std::max(0, std::min(n[k] - 1, (int)std::floor((p[k] - lo[k]) / cell[k])));
            b[k] = std::max(0, std::min(n[k] - 1, (int)std::floor((q[k] - lo[k]) / cell[k])));
        }
    }

    void Query(const glm::dvec3& p, const glm::dvec3& q, std::vector<int>& out, std::vector<int>& stamp, int mark) const {
        int a[3], b[3];
        Range(p, q, a, b);
        for (int x = a[0]; x <= b[0]; x++)
            for (int y = a[1]; y <= b[1]; y++)
                for (int z = a[2]; z <= b[2]; z++)
                    for (int i : cells[((size_t)z * n[1] + y) * n[0] + x])
                        if (stamp[i] != mark) { stamp[i] = mark; out.push_back(i); }
    }
};

// Rays along one axis
struct RayGrid {
    int axis = 0, u = 1, v = 2;
    double lo[2] = { 0, 0 }, cell[2] = { 1, 1 };
    int n = 1;
    std::vector<std::vector<int>> cells;

    void Build(const std::vector<Tri>& tris, int along, const Box& box) {
        axis = along;
        u = (along + 1) % 3;
        v = (along + 2) % 3;
        n = std::min(256, std::max(1, (int)std::sqrt((double)tris.size() / 2.0)));
        lo[0] = box.lo[u];
        lo[1] = box.lo[v];
        cell[0] = std::max(box.hi[u] - box.lo[u], 1e-12) / n;
        cell[1] = std::max(box.hi[v] - box.lo[v], 1e-12) / n;
        cells.assign((size_t)n * n, std::vector<int>());
        for (int i = 0; i < (int)tris.size(); i++) {
            int a0 = Clamp((tris[i].lo[u] - lo[0]) / cell[0]), b0 = Clamp((tris[i].hi[u] - lo[0]) / cell[0]);
            int a1 = Clamp((tris[i].lo[v] - lo[1]) / cell[1]), b1 = Clamp((tris[i].hi[v] - lo[1]) / cell[1]);
            for (int x = a0; x <= b0; x++)
                for (int y = a1; y <= b1; y++) cells[(size_t)y * n + x].push_back(i);
        }
    }

    int Clamp(double value) const { return std::max(0, std::min(n - 1, (int)std::floor(value))); }

    int Crossings(const std::vector<Tri>& tris, const glm::dvec3& p) const {
        double pu = p[u], pv = p[v];
        double fx = (pu - lo[0]) / cell[0], fy = (pv - lo[1]) / cell[1];
        if (fx < 0 || fy < 0 || fx >= n || fy >= n) return 0;
        int hits = 0;
        for (int i : cells[(size_t)Clamp(fy) * n + Clamp(fx)]) {
            const Tri& t = tris[i];
            if (t.hi[axis] < p[axis]) continue;
            double x0 = t.p[0][u], y0 = t.p[0][v], x1 = t.p[1][u], y1 = t.p[1][v], x2 = t.p[2][u], y2 = t.p[2][v];
            double d = (y1 - y2) * (x0 - x2) + (x2 - x1) * (y0 - y2);
            if (std::fabs(d) < 1e-300) continue;
            double a = ((y1 - y2) * (pu - x2) + (x2 - x1) * (pv - y2)) / d;
            double b = ((y2 - y0) * (pu - x2) + (x0 - x2) * (pv - y2)) / d;
            double c = 1.0 - a - b;
            if (a < 0 || b < 0 || c < 0) continue;
            double depth = a * t.p[0][axis] + b * t.p[1][axis] + c * t.p[2][axis];
            if (depth > p[axis]) hits++;
        }
        return hits;
    }
};

struct Solid {
    std::vector<Tri> tris;
    Box box;
    RayGrid rays[3];
    double nudge = 1e-9;

    void Build(const RawMesh& mesh) {
        tris = ToTris(mesh);
        for (const Tri& t : tris) { box.Add(t.lo); box.Add(t.hi); }
        nudge = std::max(1e-12, glm::length(box.hi - box.lo) * 1e-7);
        for (int k = 0; k < 3; k++) rays[k].Build(tris, k, box);
    }

    bool Inside(glm::dvec3 p) const {
        if (p.x < box.lo.x || p.y < box.lo.y || p.z < box.lo.z || p.x > box.hi.x || p.y > box.hi.y || p.z > box.hi.z) return false;
        // Skew avoids edges
        p += glm::dvec3(0.371, 0.713, 0.529) * nudge;
        int votes = 0;
        for (int k = 0; k < 3; k++) votes += rays[k].Crossings(tris, p) & 1;
        return votes >= 2;
    }
};

static thread_local double epsilon = 1e-9;

static CVertex Lerp(const CVertex& a, const CVertex& b, double t) {
    return { a.pos + (b.pos - a.pos) * t, a.normal + (b.normal - a.normal) * t, a.uv + (b.uv - a.uv) * t };
}

static bool Straddles(const std::vector<CVertex>& verts, const glm::dvec3& n, double w) {
    bool front = false, back = false;
    for (const CVertex& v : verts) {
        double d = glm::dot(n, v.pos) - w;
        if (d > epsilon) front = true;
        else if (d < -epsilon) back = true;
    }
    return front && back;
}

static void SplitByPlane(const CPolygon& poly, const glm::dvec3& n, double w, std::vector<CPolygon>& out) {
    CPolygon f, b;
    f.n = b.n = poly.n;
    size_t count = poly.verts.size();
    std::vector<double> dist(count);
    for (size_t i = 0; i < count; i++) dist[i] = glm::dot(n, poly.verts[i].pos) - w;
    for (size_t i = 0; i < count; i++) {
        size_t j = (i + 1) % count;
        const CVertex& vi = poly.verts[i];
        double di = dist[i], dj = dist[j];
        if (di >= -epsilon) f.verts.push_back(vi);
        if (di <= epsilon) b.verts.push_back(vi);
        if ((di > epsilon && dj < -epsilon) || (di < -epsilon && dj > epsilon)) {
            CVertex v = Lerp(vi, poly.verts[j], di / (di - dj));
            f.verts.push_back(v);
            b.verts.push_back(v);
        }
    }
    if (f.verts.size() >= 3) out.push_back(f);
    if (b.verts.size() >= 3) out.push_back(b);
}

static void Bounds(const CPolygon& poly, glm::dvec3& lo, glm::dvec3& hi) {
    lo = glm::dvec3(1e300);
    hi = glm::dvec3(-1e300);
    for (const CVertex& v : poly.verts) { lo = glm::min(lo, v.pos); hi = glm::max(hi, v.pos); }
}

// Cut along the other
static void Fragment(const CPolygon& source, const Solid& other, const Grid3& grid, std::vector<int>& stamp, int& mark, std::vector<CPolygon>& out) {
    std::vector<CPolygon> pieces = { source };
    glm::dvec3 lo, hi;
    Bounds(source, lo, hi);
    std::vector<int> candidates;
    grid.Query(lo - glm::dvec3(epsilon), hi + glm::dvec3(epsilon), candidates, stamp, ++mark);
    double sourceW = glm::dot(source.n, source.verts[0].pos);
    for (int index : candidates) {
        const Tri& t = other.tris[index];
        if (t.lo.x > hi.x + epsilon || t.hi.x < lo.x - epsilon || t.lo.y > hi.y + epsilon || t.hi.y < lo.y - epsilon || t.lo.z > hi.z + epsilon || t.hi.z < lo.z - epsilon) continue;
        std::vector<CVertex> corners = { { t.p[0] }, { t.p[1] }, { t.p[2] } };
        bool coplanar = std::fabs(std::fabs(glm::dot(t.n, source.n)) - 1.0) < 1e-9 && !Straddles(source.verts, t.n, t.w) &&
                        std::fabs(glm::dot(source.n, t.p[0]) - sourceW) < epsilon * 10.0;
        std::vector<std::pair<glm::dvec3, double>> planes;
        if (coplanar) {
            for (int e = 0; e < 3; e++) {
                glm::dvec3 edge = t.p[(e + 1) % 3] - t.p[e];
                glm::dvec3 n = glm::cross(edge, t.n);
                double length = glm::length(n);
                if (length < 1e-30) continue;
                n /= length;
                planes.push_back({ n, glm::dot(n, t.p[e]) });
            }
        } else {
            if (!Straddles(corners, source.n, sourceW)) continue;
            planes.push_back({ t.n, t.w });
        }
        for (const auto& plane : planes) {
            std::vector<CPolygon> next;
            for (const CPolygon& piece : pieces) {
                glm::dvec3 a, b;
                Bounds(piece, a, b);
                bool near = a.x <= t.hi.x + epsilon && b.x >= t.lo.x - epsilon && a.y <= t.hi.y + epsilon && b.y >= t.lo.y - epsilon && a.z <= t.hi.z + epsilon && b.z >= t.lo.z - epsilon;
                if (near && Straddles(piece.verts, plane.first, plane.second)) SplitByPlane(piece, plane.first, plane.second, next);
                else next.push_back(piece);
            }
            pieces.swap(next);
        }
    }
    for (CPolygon& piece : pieces) out.push_back(std::move(piece));
}

enum Where { Outside, InsideSolid, SameFace, OppositeFace };

static Where Classify(const CPolygon& poly, const Solid& other) {
    glm::dvec3 center(0.0);
    for (const CVertex& v : poly.verts) center += v.pos;
    center /= (double)poly.verts.size();
    double step = other.nudge * 20.0;
    bool front = other.Inside(center + poly.n * step);
    bool back = other.Inside(center - poly.n * step);
    if (front && back) return InsideSolid;
    if (!front && !back) return Outside;
    return back ? SameFace : OppositeFace;
}

static std::vector<CPolygon> ToPolygons(const RawMesh& mesh) {
    std::vector<CPolygon> out;
    out.reserve(mesh.indices.size() / 3);
    for (size_t k = 0; k + 2 < mesh.indices.size(); k += 3) {
        CPolygon poly;
        for (int j = 0; j < 3; j++) {
            const Vertex& v = mesh.vertices[mesh.indices[k + j]];
            poly.verts.push_back({ glm::dvec3(v.position), glm::dvec3(v.normal), glm::dvec2(v.texCoords) });
        }
        glm::dvec3 n = glm::cross(poly.verts[1].pos - poly.verts[0].pos, poly.verts[2].pos - poly.verts[0].pos);
        double length = glm::length(n);
        if (length < 1e-30) continue;
        poly.n = n / length;
        out.push_back(poly);
    }
    return out;
}

static void Emit(RawMesh& out, const CPolygon& poly, bool flip) {
    unsigned int base = (unsigned int)out.vertices.size();
    for (const CVertex& v : poly.verts) {
        glm::dvec3 n = glm::length(v.normal) > 1e-12 ? glm::normalize(v.normal) : poly.n;
        if (flip) n = -n;
        out.vertices.push_back({ glm::vec3(v.pos), glm::vec3(n), glm::vec2(v.uv) });
    }
    for (size_t i = 1; i + 1 < poly.verts.size(); i++) {
        if (flip) out.indices.insert(out.indices.end(), { base, base + (unsigned int)i + 1, base + (unsigned int)i });
        else out.indices.insert(out.indices.end(), { base, base + (unsigned int)i, base + (unsigned int)i + 1 });
    }
}

bool RawMeshBounds(const RawMesh& mesh, glm::vec3& outMin, glm::vec3& outMax) {
    if (mesh.vertices.empty()) return false;
    outMin = outMax = mesh.vertices[0].position;
    for (const Vertex& v : mesh.vertices) {
        outMin = glm::min(outMin, v.position);
        outMax = glm::max(outMax, v.position);
    }
    return true;
}

// What each side keeps
static bool Keep(Where where, bool fromA, CSGOperation operation) {
    if (operation == CSG_Union) return where == Outside || (fromA && where == SameFace);
    if (operation == CSG_Subtract) return fromA ? (where == Outside || where == OppositeFace) : where == InsideSolid;
    return where == InsideSolid || (fromA && where == SameFace);
}

RawMesh CSGCombine(const RawMesh& a, const RawMesh& b, CSGOperation operation) {
    glm::vec3 loA, hiA, loB, hiB;
    bool hasA = RawMeshBounds(a, loA, hiA), hasB = RawMeshBounds(b, loB, hiB);
    if (!hasA) return operation == CSG_Union ? b : RawMesh();
    if (!hasB) return operation == CSG_Intersect ? RawMesh() : a;

    Solid solidA, solidB;
    solidA.Build(a);
    solidB.Build(b);
    double diagonal = glm::length(glm::max(solidA.box.hi, solidB.box.hi) - glm::min(solidA.box.lo, solidB.box.lo));
    epsilon = std::max(1e-12, diagonal * 1e-9);
    double pad = diagonal * 1e-7;

    Box overlap;
    overlap.lo = glm::max(solidA.box.lo, solidB.box.lo) - glm::dvec3(pad);
    overlap.hi = glm::min(solidA.box.hi, solidB.box.hi) + glm::dvec3(pad);
    bool touching = overlap.lo.x <= overlap.hi.x && overlap.lo.y <= overlap.hi.y && overlap.lo.z <= overlap.hi.z;
    if (!touching) {
        if (operation == CSG_Intersect) return RawMesh();
        if (operation == CSG_Subtract) return a;
        RawMesh both = a;
        AppendRawMesh(both, b, glm::mat4(1.0f));
        return both;
    }

    RawMesh out;
    for (int side = 0; side < 2; side++) {
        bool fromA = side == 0;
        const RawMesh& mesh = fromA ? a : b;
        const Solid& other = fromA ? solidB : solidA;
        std::vector<int> near;
        for (int i = 0; i < (int)other.tris.size(); i++)
            if (overlap.Overlaps(other.tris[i].lo, other.tris[i].hi, pad)) near.push_back(i);
        Grid3 grid;
        grid.Build(other.tris, near, overlap);
        std::vector<int> stamp(other.tris.size(), 0);
        int mark = 0;
        bool flip = !fromA && operation == CSG_Subtract;

        for (const CPolygon& poly : ToPolygons(mesh)) {
            glm::dvec3 lo, hi;
            Bounds(poly, lo, hi);
            if (!overlap.Overlaps(lo, hi, pad)) {
                if (Keep(Outside, fromA, operation)) Emit(out, poly, flip);
                continue;
            }
            std::vector<CPolygon> pieces;
            Fragment(poly, other, grid, stamp, mark, pieces);
            for (const CPolygon& piece : pieces)
                if (Keep(Classify(piece, other), fromA, operation)) Emit(out, piece, flip);
        }
    }
    WeldRawMesh(out);
    return out;
}

void WeldRawMesh(RawMesh& mesh) {
    std::map<std::tuple<long long, long long, long long, long long, long long, long long, long long, long long>, unsigned int> lookup;
    glm::vec3 lo, hi;
    if (!RawMeshBounds(mesh, lo, hi)) return;
    double step = std::max(1e-7, 1e-6 * (double)glm::length(hi - lo));
    std::vector<Vertex> vertices;
    std::vector<unsigned int> remap(mesh.vertices.size());
    for (size_t i = 0; i < mesh.vertices.size(); i++) {
        const Vertex& v = mesh.vertices[i];
        auto key = std::make_tuple(llround(v.position.x / step), llround(v.position.y / step), llround(v.position.z / step),
                                   llround(v.normal.x * 1000.0), llround(v.normal.y * 1000.0), llround(v.normal.z * 1000.0),
                                   llround(v.texCoords.x * 100000.0), llround(v.texCoords.y * 100000.0));
        auto found = lookup.find(key);
        if (found != lookup.end()) { remap[i] = found->second; continue; }
        unsigned int index = (unsigned int)vertices.size();
        lookup[key] = index;
        remap[i] = index;
        vertices.push_back(v);
    }
    std::vector<unsigned int> indices;
    for (size_t k = 0; k + 2 < mesh.indices.size(); k += 3) {
        unsigned int a = remap[mesh.indices[k]], b = remap[mesh.indices[k + 1]], c = remap[mesh.indices[k + 2]];
        if (a == b || b == c || a == c) continue;
        indices.insert(indices.end(), { a, b, c });
    }
    mesh.vertices = vertices;
    mesh.indices = indices;
}

void AppendRawMesh(RawMesh& into, const RawMesh& more, const glm::mat4& transform) {
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
    bool mirrored = glm::determinant(glm::mat3(transform)) < 0.0f;
    unsigned int base = (unsigned int)into.vertices.size();
    into.vertices.reserve(into.vertices.size() + more.vertices.size());
    for (Vertex v : more.vertices) {
        v.position = glm::vec3(transform * glm::vec4(v.position, 1.0f));
        glm::vec3 n = normalMatrix * v.normal;
        v.normal = glm::length(n) > 1e-20f ? glm::normalize(n) : v.normal;
        into.vertices.push_back(v);
    }
    for (size_t k = 0; k + 2 < more.indices.size(); k += 3) {
        into.indices.push_back(base + more.indices[k]);
        into.indices.push_back(base + more.indices[mirrored ? k + 2 : k + 1]);
        into.indices.push_back(base + more.indices[mirrored ? k + 1 : k + 2]);
    }
}
