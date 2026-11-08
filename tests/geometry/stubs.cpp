#include "render/Mesh.h"

void Mesh::Init(std::vector<Vertex> vertices, std::vector<unsigned int> indices) {
    cpuVertices = vertices;
    cpuIndices = indices;
    indexCount = (int)indices.size();
}

void Mesh::UpdateVertices(const std::vector<Vertex>& vertices) {
    if (vertices.size() == cpuVertices.size()) cpuVertices = vertices;
}

void Mesh::Rebuild(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
    Init(vertices, indices);
}

void Mesh::Draw() {}
void Mesh::Destroy() {}
void Mesh::UpdateBounds() {}
