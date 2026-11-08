#include "render/UVUnwrap.h"
#include <algorithm>
#include <cmath>
#include <map>

static const float uvTolerance = 1e-4f;

static bool SameUV(const glm::vec2& a, const glm::vec2& b) {
    return std::fabs(a.x - b.x) < uvTolerance && std::fabs(a.y - b.y) < uvTolerance;
}

static int CornerOf(const PolyFace& face, int v) {
    for (size_t i = 0; i < face.verts.size(); i++)
        if (face.verts[i] == v) return (int)i;
    return -1;
}

std::set<PolyEdge> PolyUVSeams(const PolyMesh& mesh) {
    std::map<PolyEdge, std::vector<int>> edgeFaces;
    for (size_t f = 0; f < mesh.faces.size(); f++) {
        const PolyFace& face = mesh.faces[f];
        for (size_t i = 0; i < face.verts.size(); i++)
            edgeFaces[MakeEdge(face.verts[i], face.verts[(i + 1) % face.verts.size()])].push_back((int)f);
    }
    std::set<PolyEdge> seams;
    for (const auto& entry : edgeFaces) {
        if (entry.second.size() != 2) continue;
        const PolyFace& f1 = mesh.faces[entry.second[0]];
        const PolyFace& f2 = mesh.faces[entry.second[1]];
        int a = entry.first.first, b = entry.first.second;
        if (!SameUV(f1.uvs[CornerOf(f1, a)], f2.uvs[CornerOf(f2, a)]) || !SameUV(f1.uvs[CornerOf(f1, b)], f2.uvs[CornerOf(f2, b)]))
            seams.insert(entry.first);
    }
    return seams;
}

static int Root(std::vector<int>& parent, int i) {
    while (parent[i] != i) {
        parent[i] = parent[parent[i]];
        i = parent[i];
    }
    return i;
}

std::vector<int> PolyIslands(const PolyMesh& mesh, const std::set<int>& faces, const std::set<PolyEdge>& seams, int& islandCount) {
    size_t count = mesh.faces.size();
    std::vector<int> parent(count);
    for (size_t f = 0; f < count; f++) parent[f] = (int)f;
    std::map<PolyEdge, int> firstFace;
    for (int f : faces) {
        const PolyFace& face = mesh.faces[f];
        for (size_t i = 0; i < face.verts.size(); i++) {
            PolyEdge e = MakeEdge(face.verts[i], face.verts[(i + 1) % face.verts.size()]);
            if (seams.count(e)) continue;
            auto found = firstFace.find(e);
            if (found == firstFace.end()) firstFace[e] = f;
            else parent[Root(parent, f)] = Root(parent, found->second);
        }
    }
    std::vector<int> island(count, -1);
    std::map<int, int> ids;
    for (int f : faces) {
        int r = Root(parent, f);
        auto found = ids.find(r);
        if (found == ids.end()) { int id = (int)ids.size(); ids[r] = id; island[f] = id; }
        else island[f] = found->second;
    }
    islandCount = (int)ids.size();
    return island;
}

std::vector<int> PolyUVIslands(const PolyMesh& mesh, int& islandCount) {
    std::set<int> all;
    for (size_t f = 0; f < mesh.faces.size(); f++) all.insert((int)f);
    return PolyIslands(mesh, all, PolyUVSeams(mesh), islandCount);
}

static std::set<int> AllFacesIfEmpty(const PolyMesh& mesh, const std::set<int>& faces) {
    if (!faces.empty()) return faces;
    std::set<int> all;
    for (size_t f = 0; f < mesh.faces.size(); f++) all.insert((int)f);
    return all;
}

// Flatten one island
static void FlattenIsland(PolyMesh& mesh, const std::vector<int>& islandFaces, const std::set<PolyEdge>& seams) {
    // Seams split points apart
    std::map<int, int> firstCorner;
    int cornerCount = 0;
    for (int f : islandFaces) { firstCorner[f] = cornerCount; cornerCount += (int)mesh.faces[f].verts.size(); }
    std::vector<int> parent(cornerCount);
    for (int i = 0; i < cornerCount; i++) parent[i] = i;
    std::map<PolyEdge, std::vector<std::pair<int, int>>> edgeCorners;
    for (int f : islandFaces) {
        const PolyFace& face = mesh.faces[f];
        for (size_t i = 0; i < face.verts.size(); i++) {
            size_t j = (i + 1) % face.verts.size();
            PolyEdge e = MakeEdge(face.verts[i], face.verts[j]);
            if (seams.count(e)) continue;
            int ci = firstCorner[f] + (int)i, cj = firstCorner[f] + (int)j;
            edgeCorners[e].push_back(face.verts[i] == e.first ? std::make_pair(ci, cj) : std::make_pair(cj, ci));
        }
    }
    for (const auto& entry : edgeCorners) {
        const auto& list = entry.second;
        for (size_t k = 1; k < list.size(); k++) {
            parent[Root(parent, list[k].first)] = Root(parent, list[0].first);
            parent[Root(parent, list[k].second)] = Root(parent, list[0].second);
        }
    }
    std::map<int, int> rootLocal;
    std::vector<int> verts;
    std::vector<int> cornerLocal(cornerCount);
    for (int f : islandFaces) {
        const PolyFace& face = mesh.faces[f];
        for (size_t i = 0; i < face.verts.size(); i++) {
            int c = firstCorner[f] + (int)i, r = Root(parent, c);
            auto found = rootLocal.find(r);
            if (found == rootLocal.end()) { rootLocal[r] = (int)verts.size(); cornerLocal[c] = (int)verts.size(); verts.push_back(face.verts[i]); }
            else cornerLocal[c] = found->second;
        }
    }
    auto local = [&](int f, size_t i) { return cornerLocal[firstCorner[f] + (int)i]; };
    int n = (int)verts.size();

    glm::vec3 normal(0.0f), centroid(0.0f);
    for (int f : islandFaces) normal += mesh.FaceNormal(f);
    for (int v : verts) centroid += mesh.positions[v];
    centroid /= (float)n;
    if (glm::length(normal) < 1e-20f) normal = glm::vec3(0.0f, 0.0f, 1.0f);
    normal = glm::normalize(normal);

    int pinA = 0;
    float best = -1.0f;
    for (int i = 0; i < n; i++) {
        float d = glm::length(mesh.positions[verts[i]] - centroid);
        if (d > best) { best = d; pinA = i; }
    }
    int pinB = pinA == 0 ? (n > 1 ? 1 : 0) : 0;
    best = -1.0f;
    for (int i = 0; i < n; i++) {
        float d = glm::length(mesh.positions[verts[i]] - mesh.positions[verts[pinA]]);
        if (d > best) { best = d; pinB = i; }
    }

    glm::vec3 origin = mesh.positions[verts[pinA]];
    glm::vec3 axisU = mesh.positions[verts[pinB]] - origin;
    axisU -= normal * glm::dot(axisU, normal);
    if (glm::length(axisU) < 1e-20f) axisU = std::fabs(normal.x) < 0.9f ? glm::cross(normal, glm::vec3(1, 0, 0)) : glm::cross(normal, glm::vec3(0, 1, 0));
    axisU = glm::normalize(axisU);
    glm::vec3 axisV = glm::cross(normal, axisU);

    std::vector<double> x(2 * n);
    for (int i = 0; i < n; i++) {
        glm::vec3 d = mesh.positions[verts[i]] - origin;
        x[i] = glm::dot(d, axisU);
        x[n + i] = glm::dot(d, axisV);
    }

    struct Row { int index[3]; double re[3], im[3]; };
    std::vector<Row> rows;
    for (int f : islandFaces) {
        const PolyFace& face = mesh.faces[f];
        for (size_t k = 1; k + 1 < face.verts.size(); k++) {
            int ids[3] = { face.verts[0], face.verts[k], face.verts[k + 1] };
            int slots[3] = { local(f, 0), local(f, k), local(f, k + 1) };
            glm::vec3 p0 = mesh.positions[ids[0]], p1 = mesh.positions[ids[1]], p2 = mesh.positions[ids[2]];
            glm::vec3 e1 = p1 - p0, e2 = p2 - p0;
            glm::vec3 cross = glm::cross(e1, e2);
            double doubleArea = glm::length(cross);
            if (doubleArea < 1e-14 || glm::length(e1) < 1e-12f) continue;
            glm::vec3 fx = glm::normalize(e1);
            glm::vec3 fy = glm::cross(cross / (float)doubleArea, fx);
            double qx[3] = { 0.0, glm::length(e1), glm::dot(e2, fx) };
            double qy[3] = { 0.0, 0.0, glm::dot(e2, fy) };
            double scale = 1.0 / std::sqrt(doubleArea);
            Row row;
            for (int j = 0; j < 3; j++) {
                int k1 = (j + 1) % 3, k2 = (j + 2) % 3;
                row.index[j] = slots[j];
                row.re[j] = (qx[k2] - qx[k1]) * scale;
                row.im[j] = (qy[k2] - qy[k1]) * scale;
            }
            rows.push_back(row);
        }
    }

    if (n >= 3 && !rows.empty()) {
        std::vector<char> pinned(2 * n, 0);
        pinned[pinA] = pinned[n + pinA] = pinned[pinB] = pinned[n + pinB] = 1;
        auto apply = [&](const std::vector<double>& in, std::vector<double>& out) {
            std::fill(out.begin(), out.end(), 0.0);
            for (const Row& row : rows) {
                double real = 0.0, imag = 0.0;
                for (int j = 0; j < 3; j++) {
                    double u = in[row.index[j]], v = in[n + row.index[j]];
                    real += row.re[j] * u - row.im[j] * v;
                    imag += row.im[j] * u + row.re[j] * v;
                }
                for (int j = 0; j < 3; j++) {
                    out[row.index[j]] += row.re[j] * real + row.im[j] * imag;
                    out[n + row.index[j]] += -row.im[j] * real + row.re[j] * imag;
                }
            }
        };

        std::vector<double> fixed(2 * n, 0.0), b(2 * n), r(2 * n), p(2 * n), ap(2 * n);
        for (int i = 0; i < 2 * n; i++) if (pinned[i]) fixed[i] = x[i];
        apply(fixed, b);
        for (int i = 0; i < 2 * n; i++) b[i] = pinned[i] ? 0.0 : -b[i];

        std::vector<double> freeX(2 * n, 0.0);
        for (int i = 0; i < 2 * n; i++) if (!pinned[i]) freeX[i] = x[i];
        apply(freeX, ap);
        double bNorm = 0.0, rr = 0.0;
        for (int i = 0; i < 2 * n; i++) {
            r[i] = pinned[i] ? 0.0 : b[i] - ap[i];
            p[i] = r[i];
            rr += r[i] * r[i];
            bNorm += b[i] * b[i];
        }
        int limit = std::min(6000, std::max(300, 6 * n));
        for (int it = 0; it < limit && rr > 1e-24 * std::max(bNorm, 1e-30); it++) {
            apply(p, ap);
            double pap = 0.0;
            for (int i = 0; i < 2 * n; i++) if (!pinned[i]) pap += p[i] * ap[i];
            if (std::fabs(pap) < 1e-300) break;
            double alpha = rr / pap, next = 0.0;
            for (int i = 0; i < 2 * n; i++) {
                if (pinned[i]) continue;
                freeX[i] += alpha * p[i];
                r[i] -= alpha * ap[i];
                next += r[i] * r[i];
            }
            double beta = next / rr;
            rr = next;
            for (int i = 0; i < 2 * n; i++) p[i] = pinned[i] ? 0.0 : r[i] + beta * p[i];
        }
        for (int i = 0; i < 2 * n; i++) if (!pinned[i]) x[i] = freeX[i];
    }

    double area3D = 0.0, areaUV = 0.0;
    for (int f : islandFaces) {
        const PolyFace& face = mesh.faces[f];
        for (size_t k = 1; k + 1 < face.verts.size(); k++) {
            int a = local(f, 0), b = local(f, k), c = local(f, k + 1);
            area3D += 0.5 * glm::length(glm::cross(mesh.positions[face.verts[k]] - mesh.positions[face.verts[0]], mesh.positions[face.verts[k + 1]] - mesh.positions[face.verts[0]]));
            areaUV += 0.5 * ((x[b] - x[a]) * (x[n + c] - x[n + a]) - (x[c] - x[a]) * (x[n + b] - x[n + a]));
        }
    }
    // Image v runs down
    double flip = areaUV < 0.0 ? 1.0 : -1.0;
    double scale = std::fabs(areaUV) > 1e-20 ? std::sqrt(area3D / std::fabs(areaUV)) : 1.0;
    for (int f : islandFaces) {
        PolyFace& face = mesh.faces[f];
        for (size_t i = 0; i < face.verts.size(); i++) {
            int l = local(f, i);
            face.uvs[i] = glm::vec2((float)(x[l] * scale), (float)(x[n + l] * scale * flip));
        }
    }
}

bool PolyUnwrap(PolyMesh& mesh, const std::set<int>& faces, const std::set<PolyEdge>& seams) {
    std::set<int> chosen = AllFacesIfEmpty(mesh, faces);
    if (chosen.empty()) return false;
    int islandCount = 0;
    std::vector<int> island = PolyIslands(mesh, chosen, seams, islandCount);
    std::vector<std::vector<int>> groups(islandCount);
    for (int f : chosen) groups[island[f]].push_back(f);
    for (const std::vector<int>& group : groups) FlattenIsland(mesh, group, seams);
    return PolyPackIslands(mesh, chosen);
}

bool PolyPackIslands(PolyMesh& mesh, const std::set<int>& faces) {
    std::set<int> chosen = AllFacesIfEmpty(mesh, faces);
    if (chosen.empty()) return false;
    int islandCount = 0;
    std::vector<int> island = PolyIslands(mesh, chosen, PolyUVSeams(mesh), islandCount);

    struct Island { std::vector<UVCorner> corners; glm::vec2 min, max; glm::vec2 place; };
    std::vector<Island> islands(islandCount);
    for (int f : chosen)
        for (size_t c = 0; c < mesh.faces[f].verts.size(); c++) islands[island[f]].corners.push_back(UVCorner(f, (int)c));

    double totalArea = 0.0;
    for (Island& is : islands) {
        glm::vec2 center(0.0f);
        for (const UVCorner& c : is.corners) center += mesh.faces[c.first].uvs[c.second];
        center /= (float)is.corners.size();
        float bestAngle = 0.0f, bestArea = 1e30f;
        for (int step = 0; step < 30; step++) {
            float angle = step * 3.0f * 3.14159265f / 180.0f;
            float cs = cosf(angle), sn = sinf(angle);
            glm::vec2 lo(1e30f), hi(-1e30f);
            for (const UVCorner& c : is.corners) {
                glm::vec2 d = mesh.faces[c.first].uvs[c.second] - center;
                glm::vec2 r(d.x * cs - d.y * sn, d.x * sn + d.y * cs);
                lo = glm::min(lo, r);
                hi = glm::max(hi, r);
            }
            float area = (hi.x - lo.x) * (hi.y - lo.y);
            if (area < bestArea - 1e-9f) { bestArea = area; bestAngle = angle; }
        }
        float cs = cosf(bestAngle), sn = sinf(bestAngle);
        is.min = glm::vec2(1e30f);
        is.max = glm::vec2(-1e30f);
        for (const UVCorner& c : is.corners) {
            glm::vec2& uv = mesh.faces[c.first].uvs[c.second];
            glm::vec2 d = uv - center;
            uv = glm::vec2(d.x * cs - d.y * sn, d.x * sn + d.y * cs);
            is.min = glm::min(is.min, uv);
            is.max = glm::max(is.max, uv);
        }
        if (is.max.y - is.min.y > (is.max.x - is.min.x) * 1.0001f) {
            for (const UVCorner& c : is.corners) {
                glm::vec2& uv = mesh.faces[c.first].uvs[c.second];
                uv = glm::vec2(-uv.y, uv.x);
            }
            glm::vec2 lo = is.min, hi = is.max;
            is.min = glm::vec2(-hi.y, lo.x);
            is.max = glm::vec2(-lo.y, hi.x);
        }
        totalArea += (double)(is.max.x - is.min.x) * (is.max.y - is.min.y);
    }

    std::vector<int> order(islandCount);
    for (int i = 0; i < islandCount; i++) order[i] = i;
    std::sort(order.begin(), order.end(), [&](int a, int b) { return islands[a].max.y - islands[a].min.y > islands[b].max.y - islands[b].min.y; });

    float margin = (float)std::sqrt(std::max(totalArea, 1e-20) / std::max(islandCount, 1)) * 0.04f;
    float widest = 0.0f;
    for (const Island& is : islands) widest = std::max(widest, is.max.x - is.min.x);
    float rowWidth = std::max(widest, (float)std::sqrt(totalArea) * 1.15f);
    float cursorX = 0.0f, cursorY = 0.0f, shelf = 0.0f, usedW = 0.0f;
    for (int index : order) {
        Island& is = islands[index];
        float w = is.max.x - is.min.x, h = is.max.y - is.min.y;
        if (cursorX > 0.0f && cursorX + w > rowWidth) {
            cursorX = 0.0f;
            cursorY += shelf + margin;
            shelf = 0.0f;
        }
        is.place = glm::vec2(cursorX, cursorY);
        cursorX += w + margin;
        shelf = std::max(shelf, h);
        usedW = std::max(usedW, cursorX - margin);
    }
    float usedH = cursorY + shelf;
    float size = std::max(std::max(usedW, usedH), 1e-20f);
    float scale = 0.98f / size;
    glm::vec2 shift = glm::vec2(0.01f) + (glm::vec2(size) - glm::vec2(usedW, usedH)) * 0.5f * scale;
    for (Island& is : islands)
        for (const UVCorner& c : is.corners) {
            glm::vec2& uv = mesh.faces[c.first].uvs[c.second];
            uv = (uv - is.min + is.place) * scale + shift;
        }
    return true;
}

static void FixWrap(PolyMesh& mesh, int f) {
    PolyFace& face = mesh.faces[f];
    float lo = 1e30f, hi = -1e30f;
    for (const glm::vec2& uv : face.uvs) { lo = std::min(lo, uv.x); hi = std::max(hi, uv.x); }
    if (hi - lo <= 0.5f) return;
    for (glm::vec2& uv : face.uvs) if (uv.x < 0.5f) uv.x += 1.0f;
}

bool PolyProject(PolyMesh& mesh, const std::set<int>& faces, UVProjection mode, const glm::mat4* modelViewProj) {
    std::set<int> chosen = AllFacesIfEmpty(mesh, faces);
    if (chosen.empty()) return false;
    glm::vec3 lo(1e30f), hi(-1e30f);
    for (int f : chosen)
        for (int v : mesh.faces[f].verts) { lo = glm::min(lo, mesh.positions[v]); hi = glm::max(hi, mesh.positions[v]); }
    glm::vec3 center = (lo + hi) * 0.5f, extent = hi - lo;
    float size = std::max(std::max(extent.x, extent.y), std::max(extent.z, 1e-20f));

    std::vector<UVCorner> corners;
    for (int f : chosen) {
        PolyFace& face = mesh.faces[f];
        glm::vec3 n = mesh.FaceNormal(f);
        int axis = std::fabs(n.x) > std::fabs(n.y) ? (std::fabs(n.x) > std::fabs(n.z) ? 0 : 2) : (std::fabs(n.y) > std::fabs(n.z) ? 1 : 2);
        for (size_t i = 0; i < face.verts.size(); i++) {
            glm::vec3 p = mesh.positions[face.verts[i]];
            glm::vec3 d = p - center;
            glm::vec2 uv(0.0f);
            if (mode == UVProject_Box) {
                int a = axis == 0 ? 1 : 0, b = axis == 2 ? 1 : 2;
                float u = (p[a] - lo[a]) / size, v = 1.0f - (p[b] - lo[b]) / size;
                bool mirror = (axis == 0 && n.x < 0.0f) || (axis == 1 && n.y > 0.0f) || (axis == 2 && n.z < 0.0f);
                uv = glm::vec2(mirror ? 1.0f - u : u, v);
            } else if (mode == UVProject_Cylinder) {
                float height = std::max(extent.z, 1e-20f);
                uv = glm::vec2(atan2f(d.y, d.x) / 6.2831853f + 0.5f, 1.0f - (p.z - lo.z) / height);
            } else if (mode == UVProject_Sphere) {
                float r = glm::length(d);
                float z = r > 1e-20f ? glm::clamp(d.z / r, -1.0f, 1.0f) : 0.0f;
                uv = glm::vec2(atan2f(d.y, d.x) / 6.2831853f + 0.5f, acosf(z) / 3.14159265f);
            } else if (modelViewProj) {
                glm::vec4 clip = *modelViewProj * glm::vec4(p, 1.0f);
                float w = std::fabs(clip.w) > 1e-20f ? clip.w : 1e-20f;
                uv = glm::vec2((clip.x / w + 1.0f) * 0.5f, (1.0f - clip.y / w) * 0.5f);
            }
            face.uvs[i] = uv;
            corners.push_back(UVCorner(f, (int)i));
        }
        if (mode == UVProject_Cylinder || mode == UVProject_Sphere) FixWrap(mesh, f);
    }

    if (mode == UVProject_View) {
        glm::vec2 a, b;
        if (PolyCornerBounds(mesh, corners, a, b)) {
            float span = std::max(std::max(b.x - a.x, b.y - a.y), 1e-20f);
            glm::vec2 mid = (a + b) * 0.5f;
            PolyTransformCorners(mesh, corners, [&](const glm::vec2& uv) { return (uv - mid) / span * 0.98f + glm::vec2(0.5f); });
        }
    }
    return true;
}

void PolyTransformCorners(PolyMesh& mesh, const std::vector<UVCorner>& corners, const std::function<glm::vec2(const glm::vec2&)>& transform) {
    for (const UVCorner& c : corners) {
        if (c.first < 0 || c.first >= (int)mesh.faces.size()) continue;
        PolyFace& face = mesh.faces[c.first];
        if (c.second < 0 || c.second >= (int)face.uvs.size()) continue;
        face.uvs[c.second] = transform(face.uvs[c.second]);
    }
}

bool PolyCornerBounds(const PolyMesh& mesh, const std::vector<UVCorner>& corners, glm::vec2& outMin, glm::vec2& outMax) {
    outMin = glm::vec2(1e30f);
    outMax = glm::vec2(-1e30f);
    bool any = false;
    for (const UVCorner& c : corners) {
        if (c.first < 0 || c.first >= (int)mesh.faces.size()) continue;
        const PolyFace& face = mesh.faces[c.first];
        if (c.second < 0 || c.second >= (int)face.uvs.size()) continue;
        outMin = glm::min(outMin, face.uvs[c.second]);
        outMax = glm::max(outMax, face.uvs[c.second]);
        any = true;
    }
    return any;
}
