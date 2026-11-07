#pragma once
#include "render/Mesh.h"
#include <glm/glm.hpp>
#include <vector>

struct EditFace {
    std::vector<int> triangles;
    std::vector<int> vertices;
    std::vector<int> edges;
};

struct EditMesh {
    std::vector<glm::vec3> positions;
    std::vector<int> renderToUnique;
    std::vector<int> normalGroup;
    std::vector<glm::ivec3> triangles;
    std::vector<int> triangleFace;
    std::vector<glm::ivec2> edges;
    std::vector<EditFace> faces;

    void Build(const Mesh& mesh);
    void Apply(Mesh& mesh) const;
};
