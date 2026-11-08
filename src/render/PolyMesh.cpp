#include "render/PolyMesh.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <map>
#include <unordered_map>

static uint64_t DirKey(int a, int b) {
    return ((uint64_t)(uint32_t)a << 32) | (uint32_t)b;
}

static int Next(const PolyFace& face, int i) { return (i + 1) % (int)face.verts.size(); }
static int Prev(const PolyFace& face, int i) { return (i + (int)face.verts.size() - 1) % (int)face.verts.size(); }

// Directed edge to face
static std::unordered_map<uint64_t, int> DirectedEdges(const PolyMesh& mesh) {
    std::unordered_map<uint64_t, int> map;
    for (size_t f = 0; f < mesh.faces.size(); f++) {
        const PolyFace& face = mesh.faces[f];
        for (size_t i = 0; i < face.verts.size(); i++)
            map[DirKey(face.verts[i], face.verts[Next(face, (int)i)])] = (int)f;
    }
    return map;
}

static int FindCorner(const PolyFace& face, int v) {
    for (size_t i = 0; i < face.verts.size(); i++)
        if (face.verts[i] == v) return (int)i;
    return -1;
}

static glm::vec3 NewellNormal(const std::vector<glm::vec3>& positions, const std::vector<int>& loop) {
    glm::vec3 n(0.0f);
    for (size_t i = 0; i < loop.size(); i++) {
        const glm::vec3& a = positions[loop[i]];
        const glm::vec3& b = positions[loop[(i + 1) % loop.size()]];
        n.x += (a.y - b.y) * (a.z + b.z);
        n.y += (a.z - b.z) * (a.x + b.x);
        n.z += (a.x - b.x) * (a.y + b.y);
    }
    return n;
}

glm::vec3 PolyMesh::FaceNormal(int face) const {
    glm::vec3 n = NewellNormal(positions, faces[face].verts);
    float length = glm::length(n);
    return length > 1e-20f ? n / length : glm::vec3(0.0f, 0.0f, 1.0f);
}

glm::vec3 PolyMesh::FaceCenter(int face) const {
    glm::vec3 sum(0.0f);
    for (int v : faces[face].verts) sum += positions[v];
    return faces[face].verts.empty() ? sum : sum / (float)faces[face].verts.size();
}

bool PolyMesh::IsClosed() const {
    std::unordered_map<uint64_t, int> map = DirectedEdges(*this);
    for (const auto& entry : map) {
        int a = (int)(entry.first >> 32), b = (int)(uint32_t)entry.first;
        if (!map.count(DirKey(b, a))) return false;
    }
    return true;
}

void PolyMesh::FromEdit(const EditMesh& edit, const Mesh& mesh) {
    FromEdit(edit, mesh.GetVertices(), mesh.GetIndices());
}

void PolyMesh::FromEdit(const EditMesh& edit, const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
    positions = edit.positions;
    faces.clear();

    for (const EditFace& editFace : edit.faces) {
        std::map<PolyEdge, int> counts;
        for (int t : editFace.triangles) {
            const glm::ivec3& tri = edit.triangles[t];
            counts[MakeEdge(tri.x, tri.y)]++;
            counts[MakeEdge(tri.y, tri.z)]++;
            counts[MakeEdge(tri.z, tri.x)]++;
        }

        std::map<int, glm::vec2> cornerUV;
        glm::vec3 renderNormal(0.0f);
        std::vector<glm::vec3> cornerNormals;
        std::map<int, int> nextOf;
        bool ok = true;
        for (int t : editFace.triangles) {
            const glm::ivec3& tri = edit.triangles[t];
            int corners[3] = { tri.x, tri.y, tri.z };
            for (int k = 0; k < 3; k++) {
                unsigned int render = indices[t * 3 + k];
                cornerUV.emplace(corners[k], vertices[render].texCoords);
                renderNormal += vertices[render].normal;
                cornerNormals.push_back(vertices[render].normal);
                int a = corners[k], b = corners[(k + 1) % 3];
                if (counts[MakeEdge(a, b)] != 1) continue;
                if (nextOf.count(a)) ok = false;
                nextOf[a] = b;
            }
        }

        std::vector<int> loop;
        if (ok && !nextOf.empty()) {
            int start = nextOf.begin()->first, v = start;
            do {
                loop.push_back(v);
                auto found = nextOf.find(v);
                if (found == nextOf.end() || loop.size() > nextOf.size()) { ok = false; break; }
                v = found->second;
            } while (v != start);
            if (loop.size() != nextOf.size()) ok = false;
        }

        if (!ok || loop.size() < 3) {
            // Odd shapes stay triangles
            for (int t : editFace.triangles) {
                const glm::ivec3& tri = edit.triangles[t];
                PolyFace face;
                face.verts = { tri.x, tri.y, tri.z };
                for (int k = 0; k < 3; k++) face.uvs.push_back(vertices[indices[t * 3 + k]].texCoords);
                face.source = (int)(&editFace - edit.faces.data());
                faces.push_back(face);
            }
            continue;
        }

        PolyFace face;
        face.source = (int)(&editFace - edit.faces.data());
        face.verts = loop;
        for (int v : loop) face.uvs.push_back(cornerUV[v]);
        glm::vec3 normal = NewellNormal(positions, loop);
        if (glm::dot(normal, renderNormal) < 0.0f) {
            std::reverse(face.verts.begin(), face.verts.end());
            std::reverse(face.uvs.begin(), face.uvs.end());
            normal = -normal;
        }
        float length = glm::length(normal);
        if (length > 1e-20f) normal /= length;
        for (const glm::vec3& n : cornerNormals) {
            if (glm::length(n) > 0.5f && glm::dot(glm::normalize(n), normal) < 0.999f) { face.smooth = true; break; }
        }
        faces.push_back(face);
    }
}

// Ear clip in face plane
static void Triangulate(const std::vector<glm::vec3>& positions, const std::vector<int>& loop, const glm::vec3& normal, std::vector<int>& out) {
    size_t n = loop.size();
    if (n == 3) { out.insert(out.end(), { 0, 1, 2 }); return; }
    glm::vec3 axisU = std::fabs(normal.x) < 0.9f ? glm::normalize(glm::cross(normal, glm::vec3(1, 0, 0))) : glm::normalize(glm::cross(normal, glm::vec3(0, 1, 0)));
    glm::vec3 axisV = glm::cross(normal, axisU);
    std::vector<glm::vec2> p(n);
    for (size_t i = 0; i < n; i++) p[i] = glm::vec2(glm::dot(positions[loop[i]], axisU), glm::dot(positions[loop[i]], axisV));

    auto cross = [](glm::vec2 a, glm::vec2 b, glm::vec2 c) { return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x); };
    bool convex = true;
    for (size_t i = 0; i < n && convex; i++)
        if (cross(p[i], p[(i + 1) % n], p[(i + 2) % n]) < -1e-12f) convex = false;
    if (convex) {
        for (size_t i = 1; i + 1 < n; i++) out.insert(out.end(), { 0, (int)i, (int)i + 1 });
        return;
    }
    std::vector<int> remaining(n);
    for (size_t i = 0; i < n; i++) remaining[i] = (int)i;
    int guard = 0;
    while (remaining.size() > 3 && guard++ < (int)(n * n)) {
        bool clipped = false;
        size_t count = remaining.size();
        for (size_t i = 0; i < count; i++) {
            int a = remaining[(i + count - 1) % count], b = remaining[i], c = remaining[(i + 1) % count];
            if (cross(p[a], p[b], p[c]) <= 1e-12f) continue;
            bool inside = false;
            for (int other : remaining) {
                if (other == a || other == b || other == c) continue;
                if (cross(p[a], p[b], p[other]) >= 0 && cross(p[b], p[c], p[other]) >= 0 && cross(p[c], p[a], p[other]) >= 0) { inside = true; break; }
            }
            if (inside) continue;
            out.insert(out.end(), { a, b, c });
            remaining.erase(remaining.begin() + i);
            clipped = true;
            break;
        }
        if (!clipped) break;
    }
    // Fan if clipping fails
    for (size_t i = 1; i + 1 < remaining.size(); i++) out.insert(out.end(), { remaining[0], remaining[i], remaining[i + 1] });
}

void PolyMesh::ToRender(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices, EditMesh& edit) const {
    vertices.clear();
    indices.clear();
    edit.positions = positions;
    edit.renderToUnique.clear();
    edit.normalGroup.clear();
    edit.triangles.clear();
    edit.triangleFace.clear();
    edit.edges.clear();
    edit.faces.assign(faces.size(), EditFace());

    std::vector<glm::vec3> faceNormals(faces.size());
    std::vector<glm::vec3> smoothNormals(positions.size(), glm::vec3(0.0f));
    for (size_t f = 0; f < faces.size(); f++) {
        glm::vec3 n = NewellNormal(positions, faces[f].verts);
        if (faces[f].smooth) for (int v : faces[f].verts) smoothNormals[v] += n;
        float length = glm::length(n);
        faceNormals[f] = length > 1e-20f ? n / length : glm::vec3(0.0f, 0.0f, 1.0f);
    }

    std::map<PolyEdge, int> edgeIndex;
    int flatGroup = (int)positions.size();
    for (size_t f = 0; f < faces.size(); f++) {
        const PolyFace& face = faces[f];
        EditFace& editFace = edit.faces[f];
        unsigned int base = (unsigned int)vertices.size();
        for (size_t i = 0; i < face.verts.size(); i++) {
            int v = face.verts[i];
            Vertex vertex;
            vertex.position = positions[v];
            vertex.texCoords = i < face.uvs.size() ? face.uvs[i] : glm::vec2(0.0f);
            glm::vec3 n = faceNormals[f];
            if (face.smooth && glm::length(smoothNormals[v]) > 1e-20f) n = glm::normalize(smoothNormals[v]);
            vertex.normal = n;
            vertices.push_back(vertex);
            edit.renderToUnique.push_back(v);
            edit.normalGroup.push_back(face.smooth ? v : flatGroup++);
        }

        std::vector<int> local;
        Triangulate(positions, face.verts, faceNormals[f], local);
        for (size_t k = 0; k + 2 < local.size(); k += 3) {
            int t = (int)edit.triangles.size();
            indices.insert(indices.end(), { base + local[k], base + local[k + 1], base + local[k + 2] });
            edit.triangles.push_back(glm::ivec3(face.verts[local[k]], face.verts[local[k + 1]], face.verts[local[k + 2]]));
            edit.triangleFace.push_back((int)f);
            editFace.triangles.push_back(t);
        }

        editFace.vertices = face.verts;
        std::sort(editFace.vertices.begin(), editFace.vertices.end());
        editFace.vertices.erase(std::unique(editFace.vertices.begin(), editFace.vertices.end()), editFace.vertices.end());
        for (size_t i = 0; i < face.verts.size(); i++) {
            PolyEdge key = MakeEdge(face.verts[i], face.verts[Next(face, (int)i)]);
            auto found = edgeIndex.find(key);
            int index;
            if (found == edgeIndex.end()) {
                index = (int)edit.edges.size();
                edgeIndex[key] = index;
                edit.edges.push_back(glm::ivec2(key.first, key.second));
            } else {
                index = found->second;
            }
            editFace.edges.push_back(index);
        }
    }
}

void PolyMesh::RemoveUnused(PolySelection* keep, std::vector<int>* vertexMap) {
    std::vector<PolyFace> kept;
    std::vector<int> faceMap(faces.size(), -1);
    for (size_t f = 0; f < faces.size(); f++) {
        PolyFace face;
        face.smooth = faces[f].smooth;
        const PolyFace& source = faces[f];
        for (size_t i = 0; i < source.verts.size(); i++) {
            if (source.verts[i] == source.verts[Next(source, (int)i)]) continue;
            face.verts.push_back(source.verts[i]);
            face.uvs.push_back(i < source.uvs.size() ? source.uvs[i] : glm::vec2(0.0f));
        }
        std::vector<int> unique = face.verts;
        std::sort(unique.begin(), unique.end());
        unique.erase(std::unique(unique.begin(), unique.end()), unique.end());
        if (unique.size() < 3) continue;
        faceMap[f] = (int)kept.size();
        kept.push_back(face);
    }
    faces = kept;

    std::vector<int> used(positions.size(), -1);
    std::vector<glm::vec3> nextPositions;
    for (PolyFace& face : faces) {
        for (int& v : face.verts) {
            if (used[v] < 0) { used[v] = (int)nextPositions.size(); nextPositions.push_back(positions[v]); }
            v = used[v];
        }
    }
    positions = nextPositions;
    if (vertexMap) *vertexMap = used;

    if (!keep) return;
    PolySelection mapped;
    for (int v : keep->verts) if (v >= 0 && v < (int)used.size() && used[v] >= 0) mapped.verts.insert(used[v]);
    for (int f : keep->faces) if (f >= 0 && f < (int)faceMap.size() && faceMap[f] >= 0) mapped.faces.insert(faceMap[f]);
    for (const PolyEdge& e : keep->edges) {
        if (e.first >= (int)used.size() || e.second >= (int)used.size()) continue;
        if (used[e.first] >= 0 && used[e.second] >= 0) mapped.edges.insert(MakeEdge(used[e.first], used[e.second]));
    }
    *keep = mapped;
}

static int AddVertex(PolyMesh& mesh, const glm::vec3& p) {
    mesh.positions.push_back(p);
    return (int)mesh.positions.size() - 1;
}

bool PolyExtrudeFaces(PolyMesh& mesh, const std::set<int>& faces, PolySelection& out, glm::vec3& outNormal) {
    if (faces.empty()) return false;
    std::unordered_map<uint64_t, int> directed = DirectedEdges(mesh);

    struct Boundary { int a, b; glm::vec2 uvA, uvB; };
    std::vector<Boundary> boundary;
    std::vector<int> openFaces;
    for (int f : faces) {
        const PolyFace& face = mesh.faces[f];
        bool open = false;
        for (size_t i = 0; i < face.verts.size(); i++) {
            int a = face.verts[i], b = face.verts[Next(face, (int)i)];
            auto twin = directed.find(DirKey(b, a));
            if (twin == directed.end()) open = true;
            if (twin == directed.end() || !faces.count(twin->second))
                boundary.push_back({ a, b, face.uvs[i], face.uvs[Next(face, (int)i)] });
        }
        if (open) openFaces.push_back(f);
    }
    if (boundary.empty()) return false;

    for (int f : openFaces) {
        PolyFace copy = mesh.faces[f];
        std::reverse(copy.verts.begin(), copy.verts.end());
        std::reverse(copy.uvs.begin(), copy.uvs.end());
        mesh.faces.push_back(copy);
    }

    std::map<int, int> moved;
    for (int f : faces)
        for (int v : mesh.faces[f].verts)
            if (!moved.count(v)) moved[v] = AddVertex(mesh, mesh.positions[v]);

    for (const Boundary& edge : boundary) {
        PolyFace side;
        side.verts = { edge.a, edge.b, moved[edge.b], moved[edge.a] };
        side.uvs = { edge.uvA, edge.uvB, edge.uvB, edge.uvA };
        mesh.faces.push_back(side);
    }

    outNormal = glm::vec3(0.0f);
    for (int f : faces) {
        for (int& v : mesh.faces[f].verts) v = moved[v];
        outNormal += NewellNormal(mesh.positions, mesh.faces[f].verts);
        out.faces.insert(f);
    }
    float length = glm::length(outNormal);
    outNormal = length > 1e-20f ? outNormal / length : glm::vec3(0.0f, 0.0f, 1.0f);
    for (const auto& entry : moved) out.verts.insert(entry.second);
    return true;
}

bool PolyExtrudeEdges(PolyMesh& mesh, const std::set<PolyEdge>& edges, PolySelection& out) {
    std::unordered_map<uint64_t, int> directed = DirectedEdges(mesh);
    std::map<int, int> moved;
    bool any = false;
    for (const PolyEdge& edge : edges) {
        int a = edge.first, b = edge.second;
        bool forward = directed.count(DirKey(a, b)) > 0, backward = directed.count(DirKey(b, a)) > 0;
        if (forward == backward) continue;
        if (!forward) std::swap(a, b);
        const PolyFace& face = mesh.faces[directed[DirKey(a, b)]];
        int ca = FindCorner(face, a), cb = FindCorner(face, b);
        glm::vec2 uvA = face.uvs[ca], uvB = face.uvs[cb];
        if (!moved.count(a)) moved[a] = AddVertex(mesh, mesh.positions[a]);
        if (!moved.count(b)) moved[b] = AddVertex(mesh, mesh.positions[b]);
        PolyFace side;
        side.verts = { b, a, moved[a], moved[b] };
        side.uvs = { uvB, uvA, uvA, uvB };
        mesh.faces.push_back(side);
        out.edges.insert(MakeEdge(moved[a], moved[b]));
        out.verts.insert(moved[a]);
        out.verts.insert(moved[b]);
        any = true;
    }
    return any;
}

bool PolyInsetFaces(PolyMesh& mesh, const std::set<int>& faces, float amount, PolySelection& out) {
    if (faces.empty()) return false;
    for (int f : faces) {
        PolyFace original = mesh.faces[f];
        glm::vec3 center = mesh.FaceCenter(f);
        glm::vec2 uvCenter(0.0f);
        for (const glm::vec2& uv : original.uvs) uvCenter += uv;
        uvCenter /= (float)original.uvs.size();

        size_t n = original.verts.size();
        std::vector<int> inner(n);
        std::vector<glm::vec2> innerUV(n);
        for (size_t i = 0; i < n; i++) {
            inner[i] = AddVertex(mesh, glm::mix(mesh.positions[original.verts[i]], center, amount));
            innerUV[i] = glm::mix(original.uvs[i], uvCenter, amount);
        }
        for (size_t i = 0; i < n; i++) {
            size_t j = (i + 1) % n;
            PolyFace ring;
            ring.verts = { original.verts[i], original.verts[j], inner[j], inner[i] };
            ring.uvs = { original.uvs[i], original.uvs[j], innerUV[j], innerUV[i] };
            ring.smooth = original.smooth;
            mesh.faces.push_back(ring);
        }
        mesh.faces[f].verts = inner;
        mesh.faces[f].uvs = innerUV;
        out.faces.insert(f);
        out.verts.insert(inner.begin(), inner.end());
    }
    return true;
}

bool PolyBevelEdges(PolyMesh& mesh, const std::set<PolyEdge>& edges, float amount, PolySelection& out) {
    std::unordered_map<uint64_t, int> directed = DirectedEdges(mesh);
    std::set<PolyEdge> bevel;
    for (const PolyEdge& e : edges)
        if (directed.count(DirKey(e.first, e.second)) && directed.count(DirKey(e.second, e.first))) bevel.insert(e);
    if (bevel.empty()) return false;

    std::set<int> touched;
    for (const PolyEdge& e : bevel) { touched.insert(e.first); touched.insert(e.second); }

    std::map<int, float> reach;
    for (const PolyFace& face : mesh.faces) {
        for (size_t i = 0; i < face.verts.size(); i++) {
            int a = face.verts[i], b = face.verts[Next(face, (int)i)];
            float length = glm::length(mesh.positions[a] - mesh.positions[b]);
            for (int v : { a, b }) {
                if (!touched.count(v)) continue;
                auto found = reach.find(v);
                if (found == reach.end() || length < found->second) reach[v] = length;
            }
        }
    }

    std::map<std::pair<int, int>, int> edgePoints;
    std::map<std::pair<int, int>, int> cornerPoints;
    std::map<int, int> owner;
    std::map<int, glm::vec2> pointUV;
    auto pointOnEdge = [&](int v, int w, bool create) -> int {
        auto key = std::make_pair(v, w);
        auto found = edgePoints.find(key);
        if (found != edgePoints.end()) return found->second;
        if (!create) return -1;
        float d = reach[v] * 0.5f * amount;
        glm::vec3 dir = mesh.positions[w] - mesh.positions[v];
        float length = glm::length(dir);
        int id = AddVertex(mesh, mesh.positions[v] + (length > 1e-20f ? dir / length * d : glm::vec3(0.0f)));
        edgePoints[key] = id;
        owner[id] = v;
        return id;
    };

    struct Corner { std::vector<int> points; std::vector<glm::vec2> uvs; bool done = false; };
    std::vector<std::vector<Corner>> corners(mesh.faces.size());
    size_t faceCount = mesh.faces.size();

    for (int pass = 0; pass < 2; pass++) {
        for (size_t f = 0; f < faceCount; f++) {
            const PolyFace& face = mesh.faces[f];
            corners[f].resize(face.verts.size());
            for (size_t i = 0; i < face.verts.size(); i++) {
                Corner& corner = corners[f][i];
                if (corner.done) continue;
                int v = face.verts[i], prev = face.verts[Prev(face, (int)i)], next = face.verts[Next(face, (int)i)];
                glm::vec2 uv = face.uvs[i], uvPrev = face.uvs[Prev(face, (int)i)], uvNext = face.uvs[Next(face, (int)i)];
                bool prevBevel = bevel.count(MakeEdge(prev, v)) > 0, nextBevel = bevel.count(MakeEdge(v, next)) > 0;
                float d = touched.count(v) ? reach[v] * 0.5f * amount : 0.0f;
                float lengthPrev = glm::length(mesh.positions[prev] - mesh.positions[v]);
                float lengthNext = glm::length(mesh.positions[next] - mesh.positions[v]);
                float fracPrev = lengthPrev > 1e-20f ? d / lengthPrev : 0.0f;
                float fracNext = lengthNext > 1e-20f ? d / lengthNext : 0.0f;

                if (pass == 0) {
                    if (prevBevel && nextBevel) {
                        glm::vec3 p = mesh.positions[v] + (mesh.positions[prev] - mesh.positions[v]) * fracPrev + (mesh.positions[next] - mesh.positions[v]) * fracNext;
                        int id = AddVertex(mesh, p);
                        cornerPoints[std::make_pair(v, (int)f)] = id;
                        owner[id] = v;
                        corner.points = { id };
                        corner.uvs = { uv + (uvPrev - uv) * fracPrev + (uvNext - uv) * fracNext };
                        corner.done = true;
                    } else if (prevBevel) {
                        corner.points = { pointOnEdge(v, next, true) };
                        corner.uvs = { uv + (uvNext - uv) * fracNext };
                        corner.done = true;
                    } else if (nextBevel) {
                        corner.points = { pointOnEdge(v, prev, true) };
                        corner.uvs = { uv + (uvPrev - uv) * fracPrev };
                        corner.done = true;
                    }
                } else {
                    int onPrev = touched.count(v) ? pointOnEdge(v, prev, false) : -1;
                    int onNext = touched.count(v) ? pointOnEdge(v, next, false) : -1;
                    if (onPrev >= 0) { corner.points.push_back(onPrev); corner.uvs.push_back(uv + (uvPrev - uv) * fracPrev); }
                    if (onNext >= 0) { corner.points.push_back(onNext); corner.uvs.push_back(uv + (uvNext - uv) * fracNext); }
                    if (corner.points.empty()) { corner.points.push_back(v); corner.uvs.push_back(uv); }
                    corner.done = true;
                }
                for (size_t k = 0; k < corner.points.size(); k++) pointUV.emplace(corner.points[k], corner.uvs[k]);
            }
        }
    }

    // One point per bevel side
    auto sidePoint = [&](int f, int v, glm::vec2& uv) {
        int c = FindCorner(mesh.faces[f], v);
        uv = corners[f][c].uvs[0];
        return corners[f][c].points[0];
    };

    std::vector<PolyFace> added;
    for (const PolyEdge& e : bevel) {
        int a = e.first, b = e.second;
        int f1 = directed[DirKey(a, b)], f2 = directed[DirKey(b, a)];
        PolyFace quad;
        glm::vec2 uv1b, uv1a, uv2a, uv2b;
        int p1b = sidePoint(f1, b, uv1b), p1a = sidePoint(f1, a, uv1a), p2a = sidePoint(f2, a, uv2a), p2b = sidePoint(f2, b, uv2b);
        quad.verts = { p1b, p1a, p2a, p2b };
        quad.uvs = { uv1b, uv1a, uv2a, uv2b };
        added.push_back(quad);
    }

    for (size_t f = 0; f < faceCount; f++) {
        PolyFace rebuilt;
        rebuilt.smooth = mesh.faces[f].smooth;
        for (const Corner& corner : corners[f]) {
            rebuilt.verts.insert(rebuilt.verts.end(), corner.points.begin(), corner.points.end());
            rebuilt.uvs.insert(rebuilt.uvs.end(), corner.uvs.begin(), corner.uvs.end());
        }
        mesh.faces[f] = rebuilt;
    }
    size_t firstAdded = mesh.faces.size();
    mesh.faces.insert(mesh.faces.end(), added.begin(), added.end());

    // Fill holes at corners
    std::unordered_map<uint64_t, int> after = DirectedEdges(mesh);
    std::map<int, std::map<int, int>> holeNext;
    for (const auto& entry : after) {
        int x = (int)(entry.first >> 32), y = (int)(uint32_t)entry.first;
        if (after.count(DirKey(y, x))) continue;
        auto ox = owner.find(x), oy = owner.find(y);
        if (ox == owner.end() || oy == owner.end() || ox->second != oy->second) continue;
        holeNext[ox->second][y] = x;
    }
    for (auto& entry : holeNext) {
        std::map<int, int>& next = entry.second;
        while (!next.empty()) {
            int start = next.begin()->first, v = start;
            PolyFace fill;
            bool closed = false;
            while (next.count(v)) {
                fill.verts.push_back(v);
                fill.uvs.push_back(pointUV.count(v) ? pointUV[v] : glm::vec2(0.0f));
                int to = next[v];
                next.erase(v);
                v = to;
                if (v == start) { closed = true; break; }
            }
            if (closed && fill.verts.size() >= 3) mesh.faces.push_back(fill);
        }
    }

    for (size_t f = firstAdded; f < mesh.faces.size(); f++) out.faces.insert((int)f);
    mesh.RemoveUnused(&out);
    return true;
}

bool PolyFindRing(const PolyMesh& mesh, int a, int b, EdgeRing& ring) {
    std::unordered_map<uint64_t, int> directed = DirectedEdges(mesh);
    auto faceOf = [&](int x, int y, int not_) {
        auto found = directed.find(DirKey(x, y));
        return found != directed.end() && found->second != not_ ? found->second : -1;
    };
    int faceOne = faceOf(a, b, -1), faceTwo = faceOf(b, a, -1);
    if (faceOne < 0 && faceTwo < 0) return false;

    // Quad i sits past edge i
    auto walk = [&](int face, std::vector<int>& outA, std::vector<int>& outB, std::vector<int>& quads) {
        int ca = a, cb = b;
        while (face >= 0) {
            const PolyFace& f = mesh.faces[face];
            if (f.verts.size() != 4 || std::find(quads.begin(), quads.end(), face) != quads.end()) return false;
            int ia = FindCorner(f, ca), ib = FindCorner(f, cb);
            int na = f.verts[Next(f, ia)] == cb ? f.verts[Prev(f, ia)] : f.verts[Next(f, ia)];
            int nb = f.verts[Next(f, ib)] == ca ? f.verts[Prev(f, ib)] : f.verts[Next(f, ib)];
            quads.push_back(face);
            if (MakeEdge(na, nb) == MakeEdge(a, b)) return true;
            outA.push_back(na);
            outB.push_back(nb);
            int other = faceOf(na, nb, face);
            if (other < 0) other = faceOf(nb, na, face);
            ca = na;
            cb = nb;
            face = other;
        }
        return false;
    };

    ring = EdgeRing();
    std::vector<int> forwardA, forwardB, forwardQuads;
    if (faceOne >= 0 && walk(faceOne, forwardA, forwardB, forwardQuads)) {
        ring.sideA.push_back(a);
        ring.sideB.push_back(b);
        ring.sideA.insert(ring.sideA.end(), forwardA.begin(), forwardA.end());
        ring.sideB.insert(ring.sideB.end(), forwardB.begin(), forwardB.end());
        ring.quads = forwardQuads;
        ring.closed = true;
        return true;
    }

    std::vector<int> backA, backB, backQuads;
    if (faceTwo >= 0 && walk(faceTwo, backA, backB, backQuads)) return false;
    for (size_t i = backA.size(); i-- > 0;) { ring.sideA.push_back(backA[i]); ring.sideB.push_back(backB[i]); }
    for (size_t i = backQuads.size(); i-- > 0;) ring.quads.push_back(backQuads[i]);
    ring.sideA.push_back(a);
    ring.sideB.push_back(b);
    ring.sideA.insert(ring.sideA.end(), forwardA.begin(), forwardA.end());
    ring.sideB.insert(ring.sideB.end(), forwardB.begin(), forwardB.end());
    ring.quads.insert(ring.quads.end(), forwardQuads.begin(), forwardQuads.end());
    return !ring.quads.empty();
}

bool PolyLoopCut(PolyMesh& mesh, const EdgeRing& ring, int cuts, float slide, PolySelection& out) {
    if (ring.sideA.empty() || cuts < 1) return false;
    size_t count = ring.sideA.size();
    std::map<PolyEdge, std::vector<int>> cutPoints;
    std::vector<std::vector<int>> points(count);
    for (size_t i = 0; i < count; i++) {
        int a = ring.sideA[i], b = ring.sideB[i];
        points[i].push_back(a);
        for (int k = 0; k < cuts; k++) {
            float t = cuts == 1 ? slide : (k + 1) / (float)(cuts + 1);
            points[i].push_back(AddVertex(mesh, glm::mix(mesh.positions[a], mesh.positions[b], t)));
        }
        points[i].push_back(b);
        std::vector<int> inner(points[i].begin() + 1, points[i].end() - 1);
        cutPoints[MakeEdge(a, b)] = a < b ? inner : std::vector<int>(inner.rbegin(), inner.rend());
    }
    auto pointT = [&](int k) { return cuts == 1 ? (k == 0 ? 0.0f : (k == 2 ? 1.0f : slide)) : k / (float)(cuts + 1); };

    std::set<int> ringQuads(ring.quads.begin(), ring.quads.end());
    std::vector<PolyFace> added;
    for (size_t q = 0; q < ring.quads.size(); q++) {
        size_t i = q, j = (q + 1) % count;
        const PolyFace& quad = mesh.faces[ring.quads[q]];
        int ia = FindCorner(quad, ring.sideA[i]);
        bool forward = quad.verts[Next(quad, ia)] == ring.sideB[i];
        glm::vec2 uvAi = quad.uvs[ia], uvBi = quad.uvs[FindCorner(quad, ring.sideB[i])];
        glm::vec2 uvAj = quad.uvs[FindCorner(quad, ring.sideA[j])], uvBj = quad.uvs[FindCorner(quad, ring.sideB[j])];
        for (int k = 0; k <= cuts; k++) {
            float t0 = pointT(k), t1 = pointT(k + 1);
            PolyFace strip;
            strip.smooth = quad.smooth;
            if (forward) {
                strip.verts = { points[i][k], points[i][k + 1], points[j][k + 1], points[j][k] };
                strip.uvs = { glm::mix(uvAi, uvBi, t0), glm::mix(uvAi, uvBi, t1), glm::mix(uvAj, uvBj, t1), glm::mix(uvAj, uvBj, t0) };
            } else {
                strip.verts = { points[i][k + 1], points[i][k], points[j][k], points[j][k + 1] };
                strip.uvs = { glm::mix(uvAi, uvBi, t1), glm::mix(uvAi, uvBi, t0), glm::mix(uvAj, uvBj, t0), glm::mix(uvAj, uvBj, t1) };
            }
            added.push_back(strip);
        }
        for (int k = 1; k <= cuts; k++) out.edges.insert(MakeEdge(points[i][k], points[j][k]));
    }

    for (size_t f = 0; f < mesh.faces.size(); f++) {
        if (ringQuads.count((int)f)) continue;
        PolyFace& face = mesh.faces[f];
        PolyFace rebuilt;
        rebuilt.smooth = face.smooth;
        for (size_t i = 0; i < face.verts.size(); i++) {
            int x = face.verts[i], y = face.verts[Next(face, (int)i)];
            glm::vec2 uvX = face.uvs[i], uvY = face.uvs[Next(face, (int)i)];
            rebuilt.verts.push_back(x);
            rebuilt.uvs.push_back(uvX);
            auto found = cutPoints.find(MakeEdge(x, y));
            if (found == cutPoints.end()) continue;
            std::vector<int> inserts = found->second;
            if (x > y) std::reverse(inserts.begin(), inserts.end());
            for (size_t k = 0; k < inserts.size(); k++) {
                glm::vec3 p = mesh.positions[inserts[k]];
                float span = glm::length(mesh.positions[y] - mesh.positions[x]);
                float t = span > 1e-20f ? glm::length(p - mesh.positions[x]) / span : 0.5f;
                rebuilt.verts.push_back(inserts[k]);
                rebuilt.uvs.push_back(glm::mix(uvX, uvY, t));
            }
        }
        face = rebuilt;
    }

    std::vector<PolyFace> kept;
    for (size_t f = 0; f < mesh.faces.size(); f++)
        if (!ringQuads.count((int)f)) kept.push_back(mesh.faces[f]);
    kept.insert(kept.end(), added.begin(), added.end());
    mesh.faces = kept;
    for (size_t i = 0; i < count; i++)
        for (int k = 1; k <= cuts; k++) out.verts.insert(points[i][k]);
    return true;
}

static void InsertMidpoints(PolyMesh& mesh, const std::map<PolyEdge, int>& mids, const std::set<int>& skip) {
    for (size_t f = 0; f < mesh.faces.size(); f++) {
        if (skip.count((int)f)) continue;
        PolyFace& face = mesh.faces[f];
        PolyFace rebuilt;
        rebuilt.smooth = face.smooth;
        for (size_t i = 0; i < face.verts.size(); i++) {
            int n = Next(face, (int)i);
            rebuilt.verts.push_back(face.verts[i]);
            rebuilt.uvs.push_back(face.uvs[i]);
            auto found = mids.find(MakeEdge(face.verts[i], face.verts[n]));
            if (found == mids.end()) continue;
            rebuilt.verts.push_back(found->second);
            rebuilt.uvs.push_back((face.uvs[i] + face.uvs[n]) * 0.5f);
        }
        face = rebuilt;
    }
}

bool PolySubdivideFaces(PolyMesh& mesh, const std::set<int>& faces, PolySelection& out) {
    if (faces.empty()) return false;
    std::map<PolyEdge, int> mids;
    for (int f : faces) {
        const PolyFace& face = mesh.faces[f];
        for (size_t i = 0; i < face.verts.size(); i++) {
            PolyEdge e = MakeEdge(face.verts[i], face.verts[Next(face, (int)i)]);
            if (!mids.count(e)) mids[e] = AddVertex(mesh, (mesh.positions[e.first] + mesh.positions[e.second]) * 0.5f);
        }
    }

    std::vector<PolyFace> added;
    for (int f : faces) {
        const PolyFace& face = mesh.faces[f];
        int center = AddVertex(mesh, mesh.FaceCenter(f));
        glm::vec2 uvCenter(0.0f);
        for (const glm::vec2& uv : face.uvs) uvCenter += uv;
        uvCenter /= (float)face.uvs.size();
        size_t n = face.verts.size();
        for (size_t i = 0; i < n; i++) {
            size_t next = (i + 1) % n, prev = (i + n - 1) % n;
            PolyFace quad;
            quad.smooth = face.smooth;
            quad.verts = { face.verts[i], mids[MakeEdge(face.verts[i], face.verts[next])], center, mids[MakeEdge(face.verts[prev], face.verts[i])] };
            quad.uvs = { face.uvs[i], (face.uvs[i] + face.uvs[next]) * 0.5f, uvCenter, (face.uvs[prev] + face.uvs[i]) * 0.5f };
            added.push_back(quad);
        }
    }
    InsertMidpoints(mesh, mids, faces);

    std::vector<PolyFace> kept;
    for (size_t f = 0; f < mesh.faces.size(); f++)
        if (!faces.count((int)f)) kept.push_back(mesh.faces[f]);
    size_t first = kept.size();
    kept.insert(kept.end(), added.begin(), added.end());
    mesh.faces = kept;
    for (size_t f = first; f < mesh.faces.size(); f++) out.faces.insert((int)f);
    return true;
}

bool PolySubdivideEdges(PolyMesh& mesh, const std::set<PolyEdge>& edges, PolySelection& out) {
    if (edges.empty()) return false;
    std::map<PolyEdge, int> mids;
    for (const PolyEdge& e : edges) {
        int mid = AddVertex(mesh, (mesh.positions[e.first] + mesh.positions[e.second]) * 0.5f);
        mids[e] = mid;
        out.edges.insert(MakeEdge(e.first, mid));
        out.edges.insert(MakeEdge(mid, e.second));
        out.verts.insert(mid);
    }
    InsertMidpoints(mesh, mids, std::set<int>());
    return true;
}

bool PolyMergeVertices(PolyMesh& mesh, const std::set<int>& verts, PolySelection& out) {
    if (verts.size() < 2) return false;
    glm::vec3 center(0.0f);
    for (int v : verts) center += mesh.positions[v];
    center /= (float)verts.size();
    int keep = *verts.begin();
    mesh.positions[keep] = center;
    for (PolyFace& face : mesh.faces)
        for (int& v : face.verts)
            if (verts.count(v)) v = keep;
    out.verts.insert(keep);
    mesh.RemoveUnused(&out);
    return true;
}

int PolyMergeByDistance(PolyMesh& mesh, float distance, PolySelection& out) {
    size_t n = mesh.positions.size();
    std::vector<int> target(n);
    for (size_t i = 0; i < n; i++) target[i] = (int)i;
    float cell = distance > 1e-12f ? distance : 1e-12f;
    std::unordered_map<uint64_t, std::vector<int>> grid;
    auto cellKey = [&](long long x, long long y, long long z) {
        return (uint64_t)((x & 0x1FFFFF) | ((y & 0x1FFFFF) << 21) | ((z & 0x1FFFFF) << 42));
    };
    int merged = 0;
    for (size_t i = 0; i < n; i++) {
        glm::vec3 p = mesh.positions[i];
        long long cx = (long long)floor(p.x / cell), cy = (long long)floor(p.y / cell), cz = (long long)floor(p.z / cell);
        int found = -1;
        for (long long dx = -1; dx <= 1 && found < 0; dx++)
            for (long long dy = -1; dy <= 1 && found < 0; dy++)
                for (long long dz = -1; dz <= 1 && found < 0; dz++) {
                    auto bucket = grid.find(cellKey(cx + dx, cy + dy, cz + dz));
                    if (bucket == grid.end()) continue;
                    for (int other : bucket->second)
                        if (glm::length(mesh.positions[other] - p) <= distance) { found = other; break; }
                }
        if (found >= 0) { target[i] = found; merged++; }
        else grid[cellKey(cx, cy, cz)].push_back((int)i);
    }
    if (merged == 0) return 0;
    for (PolyFace& face : mesh.faces)
        for (int& v : face.verts) v = target[v];
    PolySelection mapped;
    for (int v : out.verts) mapped.verts.insert(target[v]);
    out = mapped;
    mesh.RemoveUnused(&out);
    return merged;
}

bool PolyDeleteFaces(PolyMesh& mesh, const std::set<int>& faces, PolySelection& out) {
    if (faces.empty() || faces.size() >= mesh.faces.size()) return false;
    std::vector<PolyFace> kept;
    for (size_t f = 0; f < mesh.faces.size(); f++)
        if (!faces.count((int)f)) kept.push_back(mesh.faces[f]);
    mesh.faces = kept;
    out = PolySelection();
    mesh.RemoveUnused(nullptr);
    return true;
}

bool PolyMirrorCopy(PolyMesh& mesh, const std::set<int>& faces, int axis, PolySelection& out) {
    if (faces.empty() || axis < 0 || axis > 2) return false;
    glm::vec3 boundsMin(1e30f), boundsMax(-1e30f);
    for (const glm::vec3& p : mesh.positions) { boundsMin = glm::min(boundsMin, p); boundsMax = glm::max(boundsMax, p); }
    float tolerance = std::max(glm::length(boundsMax - boundsMin) * 1e-4f, 1e-6f);

    std::map<int, int> mirrored;
    auto mirrorOf = [&](int v) {
        if (std::fabs(mesh.positions[v][axis]) <= tolerance) return v;
        auto found = mirrored.find(v);
        if (found != mirrored.end()) return found->second;
        glm::vec3 p = mesh.positions[v];
        p[axis] = -p[axis];
        int id = AddVertex(mesh, p);
        mirrored[v] = id;
        return id;
    };

    bool any = false;
    for (int f : faces) {
        PolyFace source = mesh.faces[f];
        bool onPlane = true;
        for (int v : source.verts) if (std::fabs(mesh.positions[v][axis]) > tolerance) onPlane = false;
        if (onPlane) continue;
        PolyFace copy;
        copy.smooth = source.smooth;
        for (size_t i = source.verts.size(); i-- > 0;) {
            copy.verts.push_back(mirrorOf(source.verts[i]));
            copy.uvs.push_back(source.uvs[i]);
        }
        out.faces.insert((int)mesh.faces.size());
        mesh.faces.push_back(copy);
        any = true;
    }
    for (const auto& entry : mirrored) out.verts.insert(entry.second);
    return any;
}

bool PolyFlip(PolyMesh& mesh, const std::set<int>& verts, int axis, PolySelection& out) {
    if (verts.empty() || axis < 0 || axis > 2) return false;
    glm::vec3 center(0.0f);
    for (int v : verts) center += mesh.positions[v];
    center /= (float)verts.size();
    for (int v : verts) mesh.positions[v][axis] = 2.0f * center[axis] - mesh.positions[v][axis];
    for (PolyFace& face : mesh.faces) {
        bool all = true;
        for (int v : face.verts) if (!verts.count(v)) { all = false; break; }
        if (!all) continue;
        std::reverse(face.verts.begin(), face.verts.end());
        std::reverse(face.uvs.begin(), face.uvs.end());
    }
    out.verts = verts;
    return true;
}

// One loop around a region
static bool RegionLoop(const PolyMesh& mesh, const std::set<int>& region, PolyFace& out) {
    std::unordered_map<uint64_t, std::pair<int, int>> owned;
    for (int f : region) {
        const PolyFace& face = mesh.faces[f];
        for (size_t i = 0; i < face.verts.size(); i++)
            owned[DirKey(face.verts[i], face.verts[Next(face, (int)i)])] = std::make_pair(f, (int)i);
    }
    std::map<int, std::pair<int, glm::vec2>> next;
    for (const auto& entry : owned) {
        int a = (int)(entry.first >> 32), b = (int)(uint32_t)entry.first;
        if (owned.count(DirKey(b, a))) continue;
        if (next.count(a)) return false;
        const PolyFace& face = mesh.faces[entry.second.first];
        next[a] = std::make_pair(b, face.uvs[entry.second.second]);
    }
    if (next.size() < 3) return false;
    out = PolyFace();
    int start = next.begin()->first, v = start;
    do {
        auto found = next.find(v);
        if (found == next.end() || out.verts.size() > next.size()) return false;
        out.verts.push_back(v);
        out.uvs.push_back(found->second.second);
        v = found->second.first;
    } while (v != start);
    if (out.verts.size() != next.size()) return false;
    for (int f : region) if (mesh.faces[f].smooth) out.smooth = true;
    return true;
}

static void ReplaceFaces(PolyMesh& mesh, const std::set<int>& removed, const std::vector<PolyFace>& added, PolySelection& out) {
    std::vector<PolyFace> kept;
    for (size_t f = 0; f < mesh.faces.size(); f++)
        if (!removed.count((int)f)) kept.push_back(mesh.faces[f]);
    for (const PolyFace& face : added) {
        out.faces.insert((int)kept.size());
        kept.push_back(face);
    }
    mesh.faces = kept;
}

int PolyJoinFaces(PolyMesh& mesh, const std::set<int>& faces, PolySelection& out) {
    std::map<PolyEdge, std::vector<int>> edgeFaces;
    for (int f : faces) {
        const PolyFace& face = mesh.faces[f];
        for (size_t i = 0; i < face.verts.size(); i++) edgeFaces[MakeEdge(face.verts[i], face.verts[Next(face, (int)i)])].push_back(f);
    }
    std::map<int, int> parent;
    for (int f : faces) parent[f] = f;
    auto root = [&](int f) {
        while (parent[f] != f) {
            parent[f] = parent[parent[f]];
            f = parent[f];
        }
        return f;
    };
    for (const auto& entry : edgeFaces)
        for (size_t i = 1; i < entry.second.size(); i++) parent[root(entry.second[i])] = root(entry.second[0]);

    std::map<int, std::set<int>> regions;
    for (int f : faces) regions[root(f)].insert(f);
    std::set<int> removed;
    std::vector<PolyFace> added;
    int joined = 0;
    for (const auto& entry : regions) {
        if (entry.second.size() < 2) continue;
        PolyFace merged;
        if (!RegionLoop(mesh, entry.second, merged)) continue;
        removed.insert(entry.second.begin(), entry.second.end());
        added.push_back(merged);
        joined++;
    }
    if (!joined) return 0;
    ReplaceFaces(mesh, removed, added, out);
    mesh.RemoveUnused(&out);
    return joined;
}

bool PolyFillFace(PolyMesh& mesh, const std::set<int>& verts, PolySelection& out) {
    if (verts.size() < 3) return false;
    std::unordered_map<uint64_t, int> directed = DirectedEdges(mesh);
    std::map<int, int> next;
    for (const auto& entry : directed) {
        int a = (int)(entry.first >> 32), b = (int)(uint32_t)entry.first;
        if (!verts.count(a) || !verts.count(b) || directed.count(DirKey(b, a))) continue;
        next[a] = b;
    }

    PolyFace face;
    if (next.size() == verts.size()) {
        int start = next.begin()->first, v = start;
        do {
            face.verts.push_back(v);
            v = next[v];
        } while (v != start && face.verts.size() <= verts.size());
        if (v != start || face.verts.size() != verts.size()) face.verts.clear();
        std::reverse(face.verts.begin(), face.verts.end());
    }

    if (face.verts.empty()) {
        glm::vec3 center(0.0f);
        for (int v : verts) center += mesh.positions[v];
        center /= (float)verts.size();
        glm::vec3 u = mesh.positions[*verts.begin()] - center;
        if (glm::length(u) < 1e-20f) return false;
        u = glm::normalize(u);
        glm::vec3 normal(0.0f);
        for (int v : verts) {
            glm::vec3 c = glm::cross(u, mesh.positions[v] - center);
            if (glm::length(c) > glm::length(normal)) normal = c;
        }
        if (glm::length(normal) < 1e-20f) return false;
        normal = glm::normalize(normal);
        glm::vec3 meshCenter(0.0f);
        for (const glm::vec3& p : mesh.positions) meshCenter += p;
        meshCenter /= (float)mesh.positions.size();
        if (glm::dot(normal, center - meshCenter) < 0.0f) normal = -normal;
        glm::vec3 w = glm::cross(normal, u);
        std::vector<std::pair<float, int>> order;
        for (int v : verts) {
            glm::vec3 d = mesh.positions[v] - center;
            order.push_back(std::make_pair(atan2f(glm::dot(d, w), glm::dot(d, u)), v));
        }
        std::sort(order.begin(), order.end());
        for (const auto& entry : order) face.verts.push_back(entry.second);
    }

    for (size_t i = 0; i < face.verts.size(); i++) {
        int a = face.verts[i], b = face.verts[(i + 1) % face.verts.size()];
        if (directed.count(DirKey(a, b))) return false;
    }
    face.uvs.assign(face.verts.size(), glm::vec2(0.0f));
    out.faces.insert((int)mesh.faces.size());
    mesh.faces.push_back(face);
    return true;
}

static std::set<int> Neighbors(const PolyMesh& mesh, int v) {
    std::set<int> around;
    for (const PolyFace& face : mesh.faces) {
        int c = FindCorner(face, v);
        if (c < 0) continue;
        around.insert(face.verts[Prev(face, c)]);
        around.insert(face.verts[Next(face, c)]);
    }
    return around;
}

static void RemoveFromFaces(PolyMesh& mesh, int v) {
    for (PolyFace& face : mesh.faces) {
        int c = FindCorner(face, v);
        if (c < 0) continue;
        face.verts.erase(face.verts.begin() + c);
        face.uvs.erase(face.uvs.begin() + c);
    }
}

int PolyDissolveVertices(PolyMesh& mesh, const std::set<int>& verts, PolySelection& out) {
    int dissolved = 0;
    for (int v : verts) {
        std::set<int> around;
        for (size_t f = 0; f < mesh.faces.size(); f++)
            if (FindCorner(mesh.faces[f], v) >= 0) around.insert((int)f);
        if (around.empty()) continue;
        if (Neighbors(mesh, v).size() == 2 || around.size() == 1) {
            RemoveFromFaces(mesh, v);
            dissolved++;
            continue;
        }
        PolyFace merged;
        if (!RegionLoop(mesh, around, merged)) continue;
        PolySelection ignored;
        ReplaceFaces(mesh, around, { merged }, ignored);
        RemoveFromFaces(mesh, v);
        dissolved++;
    }
    if (!dissolved) return 0;
    out = PolySelection();
    mesh.RemoveUnused(&out);
    return dissolved;
}

int PolyDissolveEdges(PolyMesh& mesh, const std::set<PolyEdge>& edges, PolySelection& out) {
    int dissolved = 0;
    std::set<int> ends;
    for (const PolyEdge& e : edges) {
        int a = e.first, b = e.second;
        std::unordered_map<uint64_t, int> directed = DirectedEdges(mesh);
        auto one = directed.find(DirKey(a, b)), two = directed.find(DirKey(b, a));
        if (one == directed.end() || two == directed.end() || one->second == two->second) continue;
        int faceOne = one->second, faceTwo = two->second;
        const PolyFace& f1 = mesh.faces[faceOne];
        const PolyFace& f2 = mesh.faces[faceTwo];
        PolyFace merged;
        merged.smooth = f1.smooth || f2.smooth;
        int s1 = FindCorner(f1, b), s2 = FindCorner(f2, a);
        for (size_t k = 0; k < f1.verts.size(); k++) {
            int i = (s1 + (int)k) % (int)f1.verts.size();
            merged.verts.push_back(f1.verts[i]);
            merged.uvs.push_back(f1.uvs[i]);
        }
        for (size_t k = 1; k + 1 < f2.verts.size(); k++) {
            int i = (s2 + (int)k) % (int)f2.verts.size();
            merged.verts.push_back(f2.verts[i]);
            merged.uvs.push_back(f2.uvs[i]);
        }
        PolySelection ignored;
        ReplaceFaces(mesh, { faceOne, faceTwo }, { merged }, ignored);
        ends.insert(a);
        ends.insert(b);
        dissolved++;
    }
    if (!dissolved) return 0;

    // Straighten bends
    std::unordered_map<uint64_t, int> directed = DirectedEdges(mesh);
    for (int v : ends) {
        std::set<int> around = Neighbors(mesh, v);
        if (around.size() != 2) continue;
        bool inside = true;
        for (int n : around)
            if (!directed.count(DirKey(v, n)) || !directed.count(DirKey(n, v))) inside = false;
        if (inside) RemoveFromFaces(mesh, v);
    }
    out = PolySelection();
    mesh.RemoveUnused(&out);
    return dissolved;
}
