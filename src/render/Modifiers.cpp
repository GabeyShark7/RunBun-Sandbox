#include "render/Modifiers.h"
#include "render/PolyMesh.h"
#include "render/EditMesh.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <unordered_map>

const char* GetModifierName(int type) {
    switch (type) {
        case Mod_Boolean: return "Boolean";
        case Mod_Mirror: return "Mirror";
        case Mod_Array: return "Array";
        case Mod_Bevel: return "Bevel";
        case Mod_Subdivide: return "Smooth Subdivide";
        case Mod_Solidify: return "Solidify";
        case Mod_Smooth: return "Smooth Shading";
        default: return "Modifier";
    }
}

const char* GetModifierHelp(int type) {
    switch (type) {
        case Mod_Boolean: return "Cut, join or keep the overlap with another object";
        case Mod_Mirror: return "Add a mirrored copy across the object middle";
        case Mod_Array: return "Repeat the shape in a row";
        case Mod_Bevel: return "Round off sharp edges";
        case Mod_Subdivide: return "Split faces and round the whole shape";
        case Mod_Solidify: return "Give thin or open shapes a wall thickness, or hollow out solid ones";
        case Mod_Smooth: return "Shade the surface smoothly instead of in flat facets";
        default: return "";
    }
}

Modifier MakeModifier(int type) {
    Modifier m;
    m.type = type;
    if (type == Mod_Array) m.amount = 1.1f;
    if (type == Mod_Solidify) m.amount = 0.02f;
    if (type == Mod_Bevel) m.amount = 0.2f;
    return m;
}

static PolyMesh ToPoly(const RawMesh& raw) {
    EditMesh edit;
    edit.Build(raw.vertices, raw.indices);
    PolyMesh poly;
    poly.FromEdit(edit, raw.vertices, raw.indices);
    poly.RemoveUnused(nullptr);
    return poly;
}

static RawMesh ToRaw(const PolyMesh& poly) {
    RawMesh raw;
    EditMesh unused;
    poly.ToRender(raw.vertices, raw.indices, unused);
    return raw;
}

static std::map<PolyEdge, std::vector<int>> EdgeFaces(const PolyMesh& mesh) {
    std::map<PolyEdge, std::vector<int>> map;
    for (size_t f = 0; f < mesh.faces.size(); f++) {
        const std::vector<int>& loop = mesh.faces[f].verts;
        for (size_t i = 0; i < loop.size(); i++) map[MakeEdge(loop[i], loop[(i + 1) % loop.size()])].push_back((int)f);
    }
    return map;
}

// One smoothing step
static void CatmullClark(PolyMesh& mesh) {
    size_t vertexCount = mesh.positions.size();
    std::map<PolyEdge, std::vector<int>> edgeFaces = EdgeFaces(mesh);
    std::vector<glm::vec3> facePoints(mesh.faces.size());
    for (size_t f = 0; f < mesh.faces.size(); f++) facePoints[f] = mesh.FaceCenter((int)f);

    std::vector<glm::vec3> next = mesh.positions;
    std::map<PolyEdge, int> edgePoint;
    std::vector<glm::vec3> faceSum(vertexCount, glm::vec3(0.0f)), midSum(vertexCount, glm::vec3(0.0f)), boundarySum(vertexCount, glm::vec3(0.0f));
    std::vector<int> faceCount(vertexCount, 0), edgeCount(vertexCount, 0), boundaryCount(vertexCount, 0);

    for (const auto& entry : edgeFaces) {
        int a = entry.first.first, b = entry.first.second;
        glm::vec3 mid = (mesh.positions[a] + mesh.positions[b]) * 0.5f;
        glm::vec3 point = mid;
        if (entry.second.size() == 2) point = (mesh.positions[a] + mesh.positions[b] + facePoints[entry.second[0]] + facePoints[entry.second[1]]) * 0.25f;
        edgePoint[entry.first] = (int)next.size();
        next.push_back(point);
        for (int v : { a, b }) {
            midSum[v] += mid;
            edgeCount[v]++;
            if (entry.second.size() == 1) {
                boundarySum[v] += mesh.positions[v == a ? b : a];
                boundaryCount[v]++;
            }
        }
    }
    for (size_t f = 0; f < mesh.faces.size(); f++)
        for (int v : mesh.faces[f].verts) { faceSum[v] += facePoints[f]; faceCount[v]++; }

    for (size_t v = 0; v < vertexCount; v++) {
        glm::vec3 p = mesh.positions[v];
        if (boundaryCount[v] == 2) next[v] = (boundarySum[v] + p * 6.0f) / 8.0f;
        else if (boundaryCount[v] > 0 || faceCount[v] == 0 || edgeCount[v] < 3) next[v] = p;
        else {
            float n = (float)edgeCount[v];
            next[v] = (faceSum[v] / (float)faceCount[v] + 2.0f * midSum[v] / n + (n - 3.0f) * p) / n;
        }
    }

    std::vector<PolyFace> faces;
    for (size_t f = 0; f < mesh.faces.size(); f++) {
        const PolyFace& face = mesh.faces[f];
        int center = (int)next.size();
        next.push_back(facePoints[f]);
        glm::vec2 uvCenter(0.0f);
        for (const glm::vec2& uv : face.uvs) uvCenter += uv;
        uvCenter /= (float)face.uvs.size();
        size_t n = face.verts.size();
        for (size_t i = 0; i < n; i++) {
            size_t after = (i + 1) % n, before = (i + n - 1) % n;
            PolyFace quad;
            quad.smooth = true;
            quad.verts = { face.verts[i], edgePoint[MakeEdge(face.verts[i], face.verts[after])], center, edgePoint[MakeEdge(face.verts[before], face.verts[i])] };
            quad.uvs = { face.uvs[i], (face.uvs[i] + face.uvs[after]) * 0.5f, uvCenter, (face.uvs[before] + face.uvs[i]) * 0.5f };
            faces.push_back(quad);
        }
    }
    mesh.positions = next;
    mesh.faces = faces;
}

static void Solidify(PolyMesh& mesh, float thickness) {
    size_t vertexCount = mesh.positions.size();
    std::vector<glm::vec3> normals(vertexCount, glm::vec3(0.0f));
    for (size_t f = 0; f < mesh.faces.size(); f++) {
        glm::vec3 n = mesh.FaceNormal((int)f);
        for (int v : mesh.faces[f].verts) normals[v] += n;
    }
    for (size_t v = 0; v < vertexCount; v++) {
        glm::vec3 n = glm::length(normals[v]) > 1e-20f ? glm::normalize(normals[v]) : glm::vec3(0.0f, 0.0f, 1.0f);
        mesh.positions.push_back(mesh.positions[v] - n * thickness);
    }
    std::unordered_map<uint64_t, int> directed;
    auto key = [](int a, int b) { return ((uint64_t)(uint32_t)a << 32) | (uint32_t)b; };
    for (const PolyFace& face : mesh.faces)
        for (size_t i = 0; i < face.verts.size(); i++) directed[key(face.verts[i], face.verts[(i + 1) % face.verts.size()])] = 1;

    size_t faceCount = mesh.faces.size();
    for (size_t f = 0; f < faceCount; f++) {
        PolyFace inner = mesh.faces[f];
        std::reverse(inner.verts.begin(), inner.verts.end());
        std::reverse(inner.uvs.begin(), inner.uvs.end());
        for (int& v : inner.verts) v += (int)vertexCount;
        const PolyFace& outer = mesh.faces[f];
        for (size_t i = 0; i < outer.verts.size(); i++) {
            size_t j = (i + 1) % outer.verts.size();
            int a = outer.verts[i], b = outer.verts[j];
            if (directed.count(key(b, a))) continue;
            PolyFace rim;
            rim.verts = { b, a, a + (int)vertexCount, b + (int)vertexCount };
            rim.uvs = { outer.uvs[j], outer.uvs[i], outer.uvs[i], outer.uvs[j] };
            mesh.faces.push_back(rim);
        }
        mesh.faces.push_back(inner);
    }
}

static void ArrayCopies(PolyMesh& mesh, int axis, int count, float spacing) {
    if (mesh.positions.empty() || count < 2) return;
    float lo = 1e30f, hi = -1e30f;
    for (const glm::vec3& p : mesh.positions) { lo = std::min(lo, p[axis]); hi = std::max(hi, p[axis]); }
    float step = (hi - lo) * spacing;
    size_t vertexCount = mesh.positions.size(), faceCount = mesh.faces.size();
    for (int k = 1; k < count; k++) {
        int offset = (int)mesh.positions.size();
        for (size_t v = 0; v < vertexCount; v++) {
            glm::vec3 p = mesh.positions[v];
            p[axis] += step * k;
            mesh.positions.push_back(p);
        }
        for (size_t f = 0; f < faceCount; f++) {
            PolyFace copy = mesh.faces[f];
            for (int& v : copy.verts) v += offset;
            mesh.faces.push_back(copy);
        }
    }
}

static void BevelSharp(PolyMesh& mesh, float amount, float angle) {
    std::map<PolyEdge, std::vector<int>> edgeFaces = EdgeFaces(mesh);
    float limit = cosf(angle * 3.14159265f / 180.0f);
    std::set<PolyEdge> sharp;
    for (const auto& entry : edgeFaces) {
        if (entry.second.size() != 2) continue;
        if (glm::dot(mesh.FaceNormal(entry.second[0]), mesh.FaceNormal(entry.second[1])) < limit) sharp.insert(entry.first);
    }
    PolySelection out;
    if (!sharp.empty()) PolyBevelEdges(mesh, sharp, amount, out);
}

RawMesh EvaluateModifiers(const RawMesh& base, const std::vector<Modifier>& modifiers, size_t count, const ModifierTargetFetch& fetch) {
    RawMesh current = base;
    for (size_t i = 0; i < modifiers.size() && i < count; i++) {
        const Modifier& m = modifiers[i];
        if (!m.enabled || current.indices.empty()) continue;
        if (m.type == Mod_Boolean) {
            RawMesh other;
            if (m.target == 0 || !fetch || !fetch(m.target, other)) continue;
            current = CSGCombine(current, other, (CSGOperation)m.operation);
            continue;
        }
        PolyMesh poly = ToPoly(current);
        int axis = m.axis < 0 ? 0 : (m.axis > 2 ? 2 : m.axis);
        switch (m.type) {
        case Mod_Mirror: {
            std::set<int> all;
            for (size_t f = 0; f < poly.faces.size(); f++) all.insert((int)f);
            PolySelection out;
            PolyMirrorCopy(poly, all, axis, out);
            break;
        }
        case Mod_Array:
            ArrayCopies(poly, axis, std::max(1, std::min(m.count, 200)), m.amount);
            break;
        case Mod_Bevel:
            BevelSharp(poly, std::max(0.01f, std::min(m.amount, 0.95f)), m.angle);
            break;
        case Mod_Subdivide:
            for (int level = 0; level < std::max(1, std::min(m.levels, 4)); level++) CatmullClark(poly);
            break;
        case Mod_Solidify:
            Solidify(poly, m.amount);
            break;
        case Mod_Smooth:
            for (PolyFace& face : poly.faces) face.smooth = true;
            break;
        default:
            break;
        }
        current = ToRaw(poly);
    }
    return current;
}
