#pragma once
#include "render/Mesh.h"
#include <vector>

// Fast ray hits on a mesh
struct RayBVH {
    struct Node {
        glm::vec3 lo, hi;
        int left = -1, right = -1;
        int start = 0, count = 0;
    };
    std::vector<Node> nodes;
    std::vector<glm::vec3> a, b, c;

    void Build(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    bool Hit(const glm::vec3& origin, const glm::vec3& dir, float& outDistance) const;
};
