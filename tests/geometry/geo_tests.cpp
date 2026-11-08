#include "render/PolyMesh.h"
#include "render/UVUnwrap.h"
#include "render/Modifiers.h"
#include "render/Primitives.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <map>
#include <string>

static int checks = 0, failures = 0;
static std::string current;

static void Check(bool ok, const char* what) {
    checks++;
    if (!ok) { failures++; printf("  FAIL [%s] %s\n", current.c_str(), what); }
}

static PolyMesh Cube() {
    PolyMesh m;
    for (int i = 0; i < 8; i++) m.positions.push_back(glm::vec3((i & 1) ? 1 : -1, (i & 2) ? 1 : -1, (i & 4) ? 1 : -1));
    int quads[6][4] = { {0,2,3,1}, {4,5,7,6}, {0,1,5,4}, {2,6,7,3}, {0,4,6,2}, {1,3,7,5} };
    for (auto& q : quads) {
        PolyFace f;
        f.verts = { q[0], q[1], q[2], q[3] };
        f.uvs = { {0,0}, {1,0}, {1,1}, {0,1} };
        m.faces.push_back(f);
    }
    return m;
}

static PolyMesh Prism(int sides) {
    PolyMesh m;
    for (int i = 0; i < sides; i++) {
        float a = 6.2831853f * i / sides;
        m.positions.push_back(glm::vec3(cosf(a), sinf(a), -1));
        m.positions.push_back(glm::vec3(cosf(a), sinf(a), 1));
    }
    PolyFace bottom, top;
    for (int i = 0; i < sides; i++) {
        int j = (i + 1) % sides;
        PolyFace side;
        side.verts = { i * 2, j * 2, j * 2 + 1, i * 2 + 1 };
        side.uvs.assign(4, glm::vec2(0));
        side.smooth = true;
        m.faces.push_back(side);
        top.verts.push_back(i * 2 + 1);
        bottom.verts.insert(bottom.verts.begin(), i * 2);
    }
    top.uvs.assign(sides, glm::vec2(0));
    bottom.uvs.assign(sides, glm::vec2(0));
    m.faces.push_back(top);
    m.faces.push_back(bottom);
    return m;
}

static PolyMesh Plane() {
    PolyMesh m;
    m.positions = { {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0} };
    PolyFace f;
    f.verts = {0,1,2,3};
    f.uvs.assign(4, glm::vec2(0));
    m.faces.push_back(f);
    return m;
}

static void CheckSound(const PolyMesh& m, bool closed, bool outward) {
    std::map<std::pair<int,int>, int> directed;
    bool facesOk = true;
    for (const PolyFace& f : m.faces) {
        if (f.verts.size() < 3 || f.uvs.size() != f.verts.size()) facesOk = false;
        for (size_t i = 0; i < f.verts.size(); i++) {
            int a = f.verts[i], b = f.verts[(i + 1) % f.verts.size()];
            if (a == b || a < 0 || a >= (int)m.positions.size()) facesOk = false;
            directed[{a, b}]++;
        }
    }
    Check(facesOk, "faces are valid");
    bool unique = true, twins = true;
    for (auto& e : directed) {
        if (e.second != 1) unique = false;
        if (!directed.count({e.first.second, e.first.first})) twins = false;
    }
    Check(unique, "each directed edge used once");
    if (closed) {
        Check(twins, "shape is sealed");
        int edges = (int)directed.size() / 2;
        int euler = (int)m.positions.size() - edges + (int)m.faces.size();
        char msg[96];
        snprintf(msg, sizeof(msg), "Euler V-E+F is 2 (got %d)", euler);
        Check(euler == 2, msg);
    }
    if (outward) {
        glm::vec3 c(0);
        for (auto& p : m.positions) c += p;
        c /= (float)m.positions.size();
        bool ok = true;
        for (size_t f = 0; f < m.faces.size(); f++)
            if (glm::dot(m.FaceNormal((int)f), m.FaceCenter((int)f) - c) <= 0) ok = false;
        Check(ok, "faces point outward");
    }
    std::vector<Vertex> verts;
    std::vector<unsigned int> idx;
    EditMesh edit;
    m.ToRender(verts, idx, edit);
    size_t expected = 0;
    for (auto& f : m.faces) expected += (f.verts.size() - 2) * 3;
    Check(idx.size() == expected, "render triangle count");
    Check(edit.faces.size() == m.faces.size() && verts.size() == edit.renderToUnique.size(), "edit data matches");
}

// Pieces fit and stay true
static void CheckUVs(const PolyMesh& m, bool squares) {
    bool inside = true, facing = true, square = true;
    double area = 0.0;
    for (const PolyFace& f : m.faces) {
        double a = 0.0;
        for (size_t i = 0; i < f.uvs.size(); i++) {
            glm::vec2 p = f.uvs[i], q = f.uvs[(i + 1) % f.uvs.size()];
            if (p.x < -1e-4f || p.x > 1.0001f || p.y < -1e-4f || p.y > 1.0001f) inside = false;
            a += (double)p.x * q.y - (double)q.x * p.y;
        }
        a *= 0.5;
        if (std::fabs(a) < 1e-12) continue;
        if (a >= 0.0) facing = false;
        area += -a;
        if (squares && f.uvs.size() == 4) {
            float s0 = glm::length(f.uvs[1] - f.uvs[0]), s1 = glm::length(f.uvs[2] - f.uvs[1]);
            float d0 = glm::length(f.uvs[2] - f.uvs[0]), d1 = glm::length(f.uvs[3] - f.uvs[1]);
            if (std::fabs(s0 - s1) > 0.02f * s0 || std::fabs(d0 - d1) > 0.02f * d0) square = false;
        }
    }
    Check(inside, "UVs inside 0 to 1");
    Check(facing, "UV pieces are not mirrored");
    if (squares) Check(square, "square faces stay square");
    Check(area <= 1.0 + 1e-3, "pieces do not overlap by area");
}

static RawMesh Raw(const PolyMesh& m, const glm::vec3& offset) {
    RawMesh raw;
    EditMesh unused;
    m.ToRender(raw.vertices, raw.indices, unused);
    for (Vertex& v : raw.vertices) v.position += offset;
    return raw;
}

static double Volume(const RawMesh& mesh) {
    double total = 0.0;
    for (size_t k = 0; k + 2 < mesh.indices.size(); k += 3) {
        glm::dvec3 a = mesh.vertices[mesh.indices[k]].position, b = mesh.vertices[mesh.indices[k + 1]].position, c = mesh.vertices[mesh.indices[k + 2]].position;
        total += glm::dot(a, glm::cross(b, c)) / 6.0;
    }
    return total;
}

static void CheckVolume(const RawMesh& mesh, double expected, const char* what) {
    double got = Volume(mesh);
    char msg[128];
    snprintf(msg, sizeof(msg), "%s volume %.4f (want %.4f)", what, got, expected);
    Check(std::fabs(got - expected) < 0.01 * std::max(1.0, std::fabs(expected)), msg);
}

static void Run(const char* name, const std::function<void()>& body) {
    current = name;
    int before = failures;
    body();
    printf("%s %s\n", failures == before ? "PASS" : "FAIL", name);
}

int main() {
    Run("cube is sound", [] { CheckSound(Cube(), true, true); });
    Run("extrude top face", [] {
        PolyMesh m = Cube(); PolySelection out; glm::vec3 n;
        Check(PolyExtrudeFaces(m, {1}, out, n), "extrude ran");
        Check(m.faces.size() == 10 && m.positions.size() == 12, "4 sides and 4 points added");
        Check(n.z > 0.99f, "normal is up");
        for (int v : out.verts) m.positions[v].z += 1.0f;
        CheckSound(m, true, true);
    });
    Run("extrude two touching faces", [] {
        PolyMesh m = Cube(); PolySelection out; glm::vec3 n;
        PolyExtrudeFaces(m, {1, 5}, out, n);
        for (int v : out.verts) m.positions[v] += n * 0.5f;
        CheckSound(m, true, false);
    });
    Run("extrude whole closed shape is refused", [] {
        PolyMesh m = Cube(); PolySelection out; glm::vec3 n;
        Check(!PolyExtrudeFaces(m, {0,1,2,3,4,5}, out, n), "refused");
    });
    Run("extrude lone plane face closes it", [] {
        PolyMesh m = Plane(); PolySelection out; glm::vec3 n;
        PolyExtrudeFaces(m, {0}, out, n);
        for (int v : out.verts) m.positions[v].z += 1.0f;
        CheckSound(m, true, true);
    });
    Run("extrude open edge of plane", [] {
        PolyMesh m = Plane(); PolySelection out;
        Check(PolyExtrudeEdges(m, {MakeEdge(1,2)}, out), "edge extrude ran");
        Check(m.faces.size() == 2 && out.edges.size() == 1, "one new face");
        CheckSound(m, false, false);
        PolyMesh c = Cube(); PolySelection o2;
        Check(!PolyExtrudeEdges(c, {MakeEdge(0,1)}, o2), "closed edge refused");
    });
    Run("inset top face", [] {
        PolyMesh m = Cube(); PolySelection out;
        PolyInsetFaces(m, {1}, 0.3f, out);
        Check(m.faces.size() == 10, "ring of 4 added");
        CheckSound(m, true, true);
    });
    Run("bevel one edge", [] {
        PolyMesh m = Cube(); PolySelection out;
        Check(PolyBevelEdges(m, {MakeEdge(4,5)}, 0.4f, out), "bevel ran");
        Check(m.faces.size() == 7, "one face added");
        CheckSound(m, true, true);
    });
    Run("bevel all edges", [] {
        PolyMesh m = Cube(); PolySelection out;
        std::set<PolyEdge> all;
        for (auto& f : m.faces) for (size_t i = 0; i < 4; i++) all.insert(MakeEdge(f.verts[i], f.verts[(i + 1) % 4]));
        PolyBevelEdges(m, all, 0.4f, out);
        char msg[64];
        snprintf(msg, sizeof(msg), "26 faces (got %d)", (int)m.faces.size());
        Check(m.faces.size() == 26, msg);
        CheckSound(m, true, true);
    });
    Run("bevel edges meeting at a corner", [] {
        PolyMesh m = Cube(); PolySelection out;
        PolyBevelEdges(m, {MakeEdge(4,5), MakeEdge(5,7)}, 0.3f, out);
        CheckSound(m, true, true);
    });
    Run("loop cut around cube", [] {
        PolyMesh m = Cube(); EdgeRing ring; PolySelection out;
        Check(PolyFindRing(m, 0, 4, ring), "ring found");
        Check(ring.closed && ring.quads.size() == 4, "closed ring of 4");
        PolyLoopCut(m, ring, 1, 0.5f, out);
        Check(m.faces.size() == 10 && out.edges.size() == 4, "4 quads split");
        CheckSound(m, true, true);
    });
    Run("loop cut with 3 cuts and slide", [] {
        PolyMesh m = Cube(); EdgeRing ring; PolySelection out;
        PolyFindRing(m, 0, 1, ring);
        PolyLoopCut(m, ring, 3, 0.5f, out);
        CheckSound(m, true, true);
        PolyMesh s = Cube(); EdgeRing r2; PolySelection o2;
        PolyFindRing(s, 0, 1, r2);
        PolyLoopCut(s, r2, 1, 0.8f, o2);
        CheckSound(s, true, true);
    });
    Run("loop cut on prism side stops at caps", [] {
        PolyMesh m = Prism(6); EdgeRing ring; PolySelection out;
        Check(PolyFindRing(m, 0, 2, ring), "ring found");
        Check(!ring.closed && ring.quads.size() == 1, "open ring through one side");
        PolyLoopCut(m, ring, 1, 0.5f, out);
        CheckSound(m, true, true);
        PolyMesh p = Prism(6); EdgeRing around; PolySelection o2;
        PolyFindRing(p, 0, 1, around);
        Check(around.closed && around.quads.size() == 6, "around the prism");
        PolyLoopCut(p, around, 2, 0.5f, o2);
        CheckSound(p, true, true);
    });
    Run("subdivide one face", [] {
        PolyMesh m = Cube(); PolySelection out;
        PolySubdivideFaces(m, {1}, out);
        Check(out.faces.size() == 4, "4 new faces");
        CheckSound(m, true, true);
    });
    Run("subdivide all faces", [] {
        PolyMesh m = Cube(); PolySelection out;
        PolySubdivideFaces(m, {0,1,2,3,4,5}, out);
        Check(m.faces.size() == 24, "24 faces");
        CheckSound(m, true, true);
    });
    Run("subdivide edges", [] {
        PolyMesh m = Cube(); PolySelection out;
        PolySubdivideEdges(m, {MakeEdge(0,1), MakeEdge(4,5)}, out);
        CheckSound(m, true, true);
    });
    Run("merge a top edge", [] {
        PolyMesh m = Cube(); PolySelection out;
        Check(PolyMergeVertices(m, {4,5}, out), "merge ran");
        Check(m.positions.size() == 7 && out.verts.size() == 1, "one point fewer");
        CheckSound(m, true, false);
    });
    Run("merge by distance", [] {
        PolyMesh m = Cube(); PolySelection out; glm::vec3 n;
        PolyExtrudeFaces(m, {1}, out, n);
        PolySelection keep;
        int merged = PolyMergeByDistance(m, 1e-4f, keep);
        Check(merged == 4, "4 doubles merged");
        Check(m.positions.size() == 8, "back to 8 points");
    });
    Run("delete a face opens the shape", [] {
        PolyMesh m = Cube(); PolySelection out;
        Check(PolyDeleteFaces(m, {1}, out), "deleted");
        Check(m.faces.size() == 5 && m.positions.size() == 8, "5 faces left");
        CheckSound(m, false, false);
        Check(!PolyDeleteFaces(m, {0,1,2,3,4}, out), "cannot delete all");
    });
    Run("mirror copy half shape", [] {
        PolyMesh m;
        m.positions = { {0,0,0}, {1,0,0}, {1,1,0}, {0,1,0}, {0,0,1}, {1,0,1}, {1,1,1}, {0,1,1} };
        int quads[5][4] = { {0,3,2,1}, {4,5,6,7}, {0,1,5,4}, {3,7,6,2}, {1,2,6,5} };
        for (auto& q : quads) { PolyFace f; f.verts = {q[0],q[1],q[2],q[3]}; f.uvs.assign(4, glm::vec2(0)); m.faces.push_back(f); }
        PolySelection out;
        Check(PolyMirrorCopy(m, {0,1,2,3,4}, 0, out), "mirror ran");
        Check(m.positions.size() == 12, "plane points shared");
        CheckSound(m, false, false);
    });
    Run("flip keeps faces outward", [] {
        PolyMesh m = Cube(); PolySelection out;
        PolyFlip(m, {0,1,2,3,4,5,6,7}, 2, out);
        CheckSound(m, true, true);
    });
    Run("join two faces after a loop cut", [] {
        PolyMesh m = Cube(); EdgeRing ring; PolySelection out;
        PolyFindRing(m, 0, 4, ring);
        PolyLoopCut(m, ring, 1, 0.5f, out);
        CheckSound(m, true, true);
        std::set<int> front;
        for (size_t f = 0; f < m.faces.size(); f++)
            if (m.FaceNormal((int)f).y < -0.99f) front.insert((int)f);
        Check(front.size() == 2, "front is split in two");
        PolySelection joined;
        Check(PolyJoinFaces(m, front, joined) == 1, "one join");
        Check(joined.faces.size() == 1, "one face picked");
        CheckSound(m, true, true);
    });
    Run("join faces that do not touch does nothing", [] {
        PolyMesh m = Cube(); PolySelection out;
        Check(PolyJoinFaces(m, {0, 1}, out) == 0, "top and bottom stay apart");
    });
    Run("fill a deleted face back in", [] {
        PolyMesh m = Cube(); PolySelection out;
        PolyDeleteFaces(m, {1}, out);
        PolySelection filled;
        Check(PolyFillFace(m, {4, 5, 6, 7}, filled), "fill ran");
        CheckSound(m, true, true);
        PolySelection again;
        Check(!PolyFillFace(m, {4, 5, 6, 7}, again), "cannot fill twice");
    });
    Run("dissolve loop cut points restores the cube", [] {
        PolyMesh m = Cube(); EdgeRing ring; PolySelection out;
        PolyFindRing(m, 0, 4, ring);
        PolyLoopCut(m, ring, 1, 0.5f, out);
        std::set<PolyEdge> cut = out.edges;
        PolySelection gone;
        Check(PolyDissolveEdges(m, cut, gone) == 4, "4 edges dissolved");
        char msg[64];
        snprintf(msg, sizeof(msg), "back to 6 faces 8 points (got %d %d)", (int)m.faces.size(), (int)m.positions.size());
        Check(m.faces.size() == 6 && m.positions.size() == 8, msg);
        CheckSound(m, true, true);
    });
    Run("dissolve a subdivided center point", [] {
        PolyMesh m = Cube(); PolySelection out;
        PolySubdivideFaces(m, {1}, out);
        int center = -1;
        for (size_t v = 0; v < m.positions.size(); v++)
            if (std::fabs(m.positions[v].x) < 1e-5f && std::fabs(m.positions[v].y) < 1e-5f && m.positions[v].z > 0.99f) center = (int)v;
        PolySelection gone;
        Check(center >= 0 && PolyDissolveVertices(m, {center}, gone) == 1, "center dissolved");
        CheckSound(m, true, true);
    });
    Run("dissolve a corner point", [] {
        PolyMesh m = Cube(); PolySelection gone;
        Check(PolyDissolveVertices(m, {7}, gone) == 1, "corner dissolved");
        CheckSound(m, true, false);
    });
    Run("cube seams come from its picture layout", [] {
        PolyMesh m = Cube();
        Check(PolyUVSeams(m).size() == 12, "all 12 edges are seams");
        int count = 0;
        PolyUVIslands(m, count);
        Check(count == 6, "6 islands");
    });
    Run("unwrap cube into 6 square pieces", [] {
        PolyMesh m = Cube();
        Check(PolyUnwrap(m, {}, PolyUVSeams(m)), "unwrap ran");
        CheckUVs(m, true);
        int count = 0;
        PolyUVIslands(m, count);
        Check(count == 6, "still 6 islands");
    });
    Run("unwrap cube net into one flat piece", [] {
        PolyMesh m = Cube();
        std::set<PolyEdge> seams = { MakeEdge(4,5), MakeEdge(5,7), MakeEdge(7,6), MakeEdge(0,4), MakeEdge(1,5), MakeEdge(3,7), MakeEdge(2,6) };
        Check(PolyUnwrap(m, {}, seams), "unwrap ran");
        int count = 0;
        PolyUVIslands(m, count);
        char msg[64];
        snprintf(msg, sizeof(msg), "one island (got %d)", count);
        Check(count == 1, msg);
        CheckUVs(m, true);
    });
    Run("unwrap prism side into a strip", [] {
        PolyMesh m = Prism(16);
        std::set<PolyEdge> seams = { MakeEdge(0, 1) };
        for (int i = 0; i < 16; i++) {
            int j = (i + 1) % 16;
            seams.insert(MakeEdge(i * 2, j * 2));
            seams.insert(MakeEdge(i * 2 + 1, j * 2 + 1));
        }
        std::set<int> sides;
        for (int i = 0; i < 16; i++) sides.insert(i);
        Check(PolyUnwrap(m, sides, seams), "unwrap ran");
        glm::vec2 lo(1e9f), hi(-1e9f);
        for (int f : sides) for (auto& uv : m.faces[f].uvs) { lo = glm::min(lo, uv); hi = glm::max(hi, uv); }
        float ratio = (hi.x - lo.x) / (hi.y - lo.y);
        float expected = 16.0f * 2.0f * sinf(3.14159265f / 16.0f) / 2.0f;
        char msg[96];
        snprintf(msg, sizeof(msg), "strip is %.2f wide per tall (want %.2f)", ratio, expected);
        Check(std::fabs(ratio - expected) < 0.08f * expected, msg);
        CheckUVs(m, false);
    });
    Run("box projection fills the square", [] {
        PolyMesh m = Cube();
        Check(PolyProject(m, {}, UVProject_Box, nullptr), "project ran");
        bool inside = true;
        for (auto& f : m.faces) for (auto& uv : f.uvs) if (uv.x < -1e-4f || uv.x > 1.0001f || uv.y < -1e-4f || uv.y > 1.0001f) inside = false;
        Check(inside, "all inside 0 to 1");
    });
    Run("cut and join boxes", [] {
        RawMesh a = Raw(Cube(), glm::vec3(0.0f)), b = Raw(Cube(), glm::vec3(1.0f, 0.0f, 0.0f));
        CheckVolume(CSGCombine(a, b, CSG_Union), 12.0, "union");
        CheckVolume(CSGCombine(a, b, CSG_Subtract), 4.0, "subtract");
        CheckVolume(CSGCombine(a, b, CSG_Intersect), 4.0, "intersect");
        RawMesh apart = Raw(Cube(), glm::vec3(5.0f, 0.0f, 0.0f));
        CheckVolume(CSGCombine(a, apart, CSG_Subtract), 8.0, "subtract far away");
    });
    Run("drill a round hole", [] {
        RawMesh box = Raw(Cube(), glm::vec3(0.0f));
        PolyMesh pin = Prism(24);
        for (auto& p : pin.positions) { p.x *= 0.5f; p.y *= 0.5f; p.z *= 2.0f; }
        RawMesh cut = CSGCombine(box, Raw(pin, glm::vec3(0.0f)), CSG_Subtract);
        double hole = 24 * 0.5 * 0.25 * sin(6.2831853 / 24) * 2.0;
        CheckVolume(cut, 8.0 - hole, "box minus pin");
    });
    Run("modifiers keep sensible volume", [] {
        RawMesh cube = Raw(Cube(), glm::vec3(0.0f));
        Modifier array = MakeModifier(Mod_Array);
        array.count = 3;
        CheckVolume(EvaluateModifiers(cube, { array }, 9, nullptr), 24.0, "array of 3");
        Modifier solid = MakeModifier(Mod_Solidify);
        solid.amount = 0.2f;
        double inner = 2.0 - 2.0 * 0.2 / sqrt(3.0);
        CheckVolume(EvaluateModifiers(cube, { solid }, 9, nullptr), 8.0 - inner * inner * inner, "hollow shell");
        Modifier smooth = MakeModifier(Mod_Subdivide);
        double rounded = Volume(EvaluateModifiers(cube, { smooth }, 9, nullptr));
        char roundedMsg[96];
        snprintf(roundedMsg, sizeof(roundedMsg), "smooth subdivide shrinks a little (got %.3f)", rounded);
        Check(rounded > 3.0 && rounded < 4.2, roundedMsg);
        Modifier bevel = MakeModifier(Mod_Bevel);
        double beveled = Volume(EvaluateModifiers(cube, { bevel }, 9, nullptr));
        Check(beveled > 7.0 && beveled < 8.0, "bevel trims corners");
        Modifier boolean = MakeModifier(Mod_Boolean);
        boolean.target = 7;
        auto fetch = [](unsigned int id, RawMesh& out) { if (id != 7) return false; out = Raw(Cube(), glm::vec3(1.0f, 1.0f, 1.0f)); return true; };
        CheckVolume(EvaluateModifiers(cube, { boolean }, 9, fetch), 7.0, "boolean subtract corner");
        Modifier off = boolean;
        off.enabled = false;
        CheckVolume(EvaluateModifiers(cube, { off }, 9, fetch), 8.0, "turned off does nothing");
    });
    Run("every shape is sealed and faces out", [] {
        for (int t = 0; t < Primitive_Count; t++) {
            if (t == Primitive_Plane) continue;
            Mesh mesh = CreatePrimitiveMesh((PrimitiveType)t, 24, 0.3f);
            RawMesh raw;
            raw.vertices = mesh.GetVertices();
            raw.indices = mesh.GetIndices();
            double volume = Volume(raw);
            EditMesh edit;
            edit.Build(raw.vertices, raw.indices);
            PolyMesh poly;
            poly.FromEdit(edit, raw.vertices, raw.indices);
            poly.RemoveUnused(nullptr);
            char msg[128];
            snprintf(msg, sizeof(msg), "%s has positive volume (%.3f)", GetPrimitiveName((PrimitiveType)t), volume);
            Check(volume > 0.01, msg);
            snprintf(msg, sizeof(msg), "%s is sealed", GetPrimitiveName((PrimitiveType)t));
            Check(poly.IsClosed(), msg);
        }
    });
    printf("\n%d checks, %d failed\n", checks, failures);
    return failures ? 1 : 0;
}
