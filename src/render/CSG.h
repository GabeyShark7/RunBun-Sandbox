#pragma once
#include "render/Mesh.h"
#include <vector>

enum CSGOperation {
    CSG_Union = 0,
    CSG_Subtract = 1,
    CSG_Intersect = 2
};

struct RawMesh {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
};

RawMesh CSGCombine(const RawMesh& a, const RawMesh& b, CSGOperation operation);
void WeldRawMesh(RawMesh& mesh);
void AppendRawMesh(RawMesh& into, const RawMesh& more, const glm::mat4& transform);
bool RawMeshBounds(const RawMesh& mesh, glm::vec3& outMin, glm::vec3& outMax);
