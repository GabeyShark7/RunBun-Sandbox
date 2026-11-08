#pragma once
#include "render/PolyMesh.h"
#include <functional>
#include <set>
#include <utility>
#include <vector>

enum UVProjection {
    UVProject_Box = 0,
    UVProject_Cylinder,
    UVProject_Sphere,
    UVProject_View
};

typedef std::pair<int, int> UVCorner;

std::set<PolyEdge> PolyUVSeams(const PolyMesh& mesh);
std::vector<int> PolyIslands(const PolyMesh& mesh, const std::set<int>& faces, const std::set<PolyEdge>& seams, int& islandCount);
std::vector<int> PolyUVIslands(const PolyMesh& mesh, int& islandCount);

bool PolyUnwrap(PolyMesh& mesh, const std::set<int>& faces, const std::set<PolyEdge>& seams);
bool PolyProject(PolyMesh& mesh, const std::set<int>& faces, UVProjection mode, const glm::mat4* modelViewProj);
bool PolyPackIslands(PolyMesh& mesh, const std::set<int>& faces);

void PolyTransformCorners(PolyMesh& mesh, const std::vector<UVCorner>& corners, const std::function<glm::vec2(const glm::vec2&)>& transform);
bool PolyCornerBounds(const PolyMesh& mesh, const std::vector<UVCorner>& corners, glm::vec2& outMin, glm::vec2& outMax);
