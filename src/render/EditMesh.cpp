#include "render/EditMesh.h"
#include <algorithm>
#include <cmath>
#include <map>
#include <tuple>

static int FindRoot(std::vector<int>& parent, int i) {
    while (parent[i] != i) {
        parent[i] = parent[parent[i]];
        i = parent[i];
    }
    return i;
}

static std::pair<int, int> EdgeKey(int a, int b) {
    return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
}

void EditMesh::Build(const Mesh& mesh) {
    const std::vector<Vertex>& vertices = mesh.GetVertices();
    const std::vector<unsigned int>& indices = mesh.GetIndices();

    positions.clear();
    triangles.clear();
    triangleFace.clear();
    edges.clear();
    faces.clear();
    renderToUnique.assign(vertices.size(), 0);
    normalGroup.assign(vertices.size(), 0);

    std::map<std::tuple<long long, long long, long long>, int> lookup;
    std::map<std::tuple<int, long long, long long, long long>, int> groups;
    for (size_t i = 0; i < vertices.size(); i++) {
        const glm::vec3& p = vertices[i].position;
        auto key = std::make_tuple(llround(p.x * 1e5), llround(p.y * 1e5), llround(p.z * 1e5));
        auto found = lookup.find(key);
        int unique;
        if (found == lookup.end()) {
            unique = (int)positions.size();
            lookup[key] = unique;
            positions.push_back(p);
        } else {
            unique = found->second;
        }
        renderToUnique[i] = unique;

        const glm::vec3& n = vertices[i].normal;
        auto groupKey = std::make_tuple(unique, llround(n.x * 100.0), llround(n.y * 100.0), llround(n.z * 100.0));
        auto groupFound = groups.find(groupKey);
        if (groupFound == groups.end()) {
            int id = (int)groups.size();
            groups[groupKey] = id;
            normalGroup[i] = id;
        } else {
            normalGroup[i] = groupFound->second;
        }
    }

    glm::vec3 boundsMin = positions.empty() ? glm::vec3(0.0f) : positions[0];
    glm::vec3 boundsMax = boundsMin;
    for (const glm::vec3& p : positions) {
        boundsMin = glm::min(boundsMin, p);
        boundsMax = glm::max(boundsMax, p);
    }
    float planeTolerance = std::max(glm::length(boundsMax - boundsMin) * 1e-4f, 1e-6f);

    size_t triangleCount = indices.size() / 3;
    std::vector<glm::vec3> normals(triangleCount, glm::vec3(0.0f));
    std::vector<bool> valid(triangleCount, false);
    std::map<std::pair<int, int>, std::vector<int>> edgeTriangles;

    for (size_t t = 0; t < triangleCount; t++) {
        glm::ivec3 tri(renderToUnique[indices[t * 3]], renderToUnique[indices[t * 3 + 1]], renderToUnique[indices[t * 3 + 2]]);
        triangles.push_back(tri);
        if (tri.x == tri.y || tri.y == tri.z || tri.x == tri.z) continue;

        glm::vec3 cross = glm::cross(positions[tri.y] - positions[tri.x], positions[tri.z] - positions[tri.x]);
        float area = glm::length(cross);
        if (area < 1e-12f) continue;

        normals[t] = cross / area;
        valid[t] = true;
        edgeTriangles[EdgeKey(tri.x, tri.y)].push_back((int)t);
        edgeTriangles[EdgeKey(tri.y, tri.z)].push_back((int)t);
        edgeTriangles[EdgeKey(tri.z, tri.x)].push_back((int)t);
    }

    std::vector<int> parent(triangleCount);
    for (size_t t = 0; t < triangleCount; t++) parent[t] = (int)t;

    for (const auto& entry : edgeTriangles) {
        const std::vector<int>& shared = entry.second;
        for (size_t i = 0; i < shared.size(); i++) {
            for (size_t j = i + 1; j < shared.size(); j++) {
                int a = shared[i], b = shared[j];
                if (glm::dot(normals[a], normals[b]) < 0.9999f) continue;
                float distance = std::fabs(glm::dot(normals[a], positions[triangles[b].x] - positions[triangles[a].x]));
                if (distance > planeTolerance) continue;
                parent[FindRoot(parent, a)] = FindRoot(parent, b);
            }
        }
    }

    std::map<int, int> rootToFace;
    triangleFace.assign(triangleCount, -1);
    for (size_t t = 0; t < triangleCount; t++) {
        if (!valid[t]) continue;
        int root = FindRoot(parent, (int)t);
        auto found = rootToFace.find(root);
        int face;
        if (found == rootToFace.end()) {
            face = (int)faces.size();
            rootToFace[root] = face;
            faces.push_back(EditFace());
        } else {
            face = found->second;
        }
        faces[face].triangles.push_back((int)t);
        triangleFace[t] = face;
    }

    std::map<std::pair<int, int>, int> edgeIndex;
    for (EditFace& face : faces) {
        std::map<std::pair<int, int>, int> counts;
        std::vector<int> faceVertices;
        for (int t : face.triangles) {
            const glm::ivec3& tri = triangles[t];
            counts[EdgeKey(tri.x, tri.y)]++;
            counts[EdgeKey(tri.y, tri.z)]++;
            counts[EdgeKey(tri.z, tri.x)]++;
            faceVertices.insert(faceVertices.end(), { tri.x, tri.y, tri.z });
        }
        std::sort(faceVertices.begin(), faceVertices.end());
        faceVertices.erase(std::unique(faceVertices.begin(), faceVertices.end()), faceVertices.end());
        face.vertices = faceVertices;

        for (const auto& count : counts) {
            if (count.second != 1) continue;
            auto found = edgeIndex.find(count.first);
            int index;
            if (found == edgeIndex.end()) {
                index = (int)edges.size();
                edgeIndex[count.first] = index;
                edges.push_back(glm::ivec2(count.first.first, count.first.second));
            } else {
                index = found->second;
            }
            face.edges.push_back(index);
        }
    }
}

void EditMesh::Apply(Mesh& mesh) const {
    std::vector<Vertex> vertices = mesh.GetVertices();
    const std::vector<unsigned int>& indices = mesh.GetIndices();
    if (vertices.size() != renderToUnique.size()) return;

    for (size_t i = 0; i < vertices.size(); i++) {
        vertices[i].position = positions[renderToUnique[i]];
    }

    int groupCount = 0;
    for (int g : normalGroup) groupCount = std::max(groupCount, g + 1);
    std::vector<glm::vec3> groupNormals(groupCount, glm::vec3(0.0f));

    for (size_t t = 0; t + 2 < indices.size(); t += 3) {
        unsigned int a = indices[t], b = indices[t + 1], c = indices[t + 2];
        glm::vec3 faceNormal = glm::cross(vertices[b].position - vertices[a].position, vertices[c].position - vertices[a].position);
        if (glm::dot(faceNormal, vertices[a].normal + vertices[b].normal + vertices[c].normal) < 0.0f) faceNormal = -faceNormal;
        groupNormals[normalGroup[a]] += faceNormal;
        groupNormals[normalGroup[b]] += faceNormal;
        groupNormals[normalGroup[c]] += faceNormal;
    }

    for (size_t i = 0; i < vertices.size(); i++) {
        glm::vec3 n = groupNormals[normalGroup[i]];
        float length = glm::length(n);
        if (length > 1e-12f) {
            glm::vec3 normal = n / length;
            if (glm::dot(normal, vertices[i].normal) < 0.0f && glm::length(vertices[i].normal) > 0.5f) normal = -normal;
            vertices[i].normal = normal;
        }
    }

    mesh.UpdateVertices(vertices);
}
