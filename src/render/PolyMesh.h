#pragma once
#include "render/Mesh.h"
#include "render/EditMesh.h"
#include <glm/glm.hpp>
#include <set>
#include <utility>
#include <vector>

struct PolyFace {
    std::vector<int> verts;
    std::vector<glm::vec2> uvs;
    bool smooth = false;
    int source = -1;
};

typedef std::pair<int, int> PolyEdge;

inline PolyEdge MakeEdge(int a, int b) { return a < b ? PolyEdge(a, b) : PolyEdge(b, a); }

struct PolySelection {
    std::set<int> verts;
    std::set<int> faces;
    std::set<PolyEdge> edges;
};

struct EdgeRing {
    std::vector<int> sideA;
    std::vector<int> sideB;
    std::vector<int> quads;
    bool closed = false;
};

struct PolyMesh {
    std::vector<glm::vec3> positions;
    std::vector<PolyFace> faces;

    void FromEdit(const EditMesh& edit, const Mesh& mesh);
    void FromEdit(const EditMesh& edit, const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices);
    void ToRender(std::vector<Vertex>& vertices, std::vector<unsigned int>& indices, EditMesh& edit) const;

    glm::vec3 FaceNormal(int face) const;
    glm::vec3 FaceCenter(int face) const;
    void RemoveUnused(PolySelection* keep, std::vector<int>* vertexMap = nullptr);
    bool IsClosed() const;
};

bool PolyExtrudeFaces(PolyMesh& mesh, const std::set<int>& faces, PolySelection& out, glm::vec3& outNormal);
bool PolyExtrudeEdges(PolyMesh& mesh, const std::set<PolyEdge>& edges, PolySelection& out);
bool PolyInsetFaces(PolyMesh& mesh, const std::set<int>& faces, float amount, PolySelection& out);
bool PolyBevelEdges(PolyMesh& mesh, const std::set<PolyEdge>& edges, float amount, PolySelection& out);
bool PolyFindRing(const PolyMesh& mesh, int a, int b, EdgeRing& ring);
bool PolyLoopCut(PolyMesh& mesh, const EdgeRing& ring, int cuts, float slide, PolySelection& out);
bool PolySubdivideFaces(PolyMesh& mesh, const std::set<int>& faces, PolySelection& out);
bool PolySubdivideEdges(PolyMesh& mesh, const std::set<PolyEdge>& edges, PolySelection& out);
bool PolyMergeVertices(PolyMesh& mesh, const std::set<int>& verts, PolySelection& out);
int PolyMergeByDistance(PolyMesh& mesh, float distance, PolySelection& out);
bool PolyDeleteFaces(PolyMesh& mesh, const std::set<int>& faces, PolySelection& out);
bool PolyMirrorCopy(PolyMesh& mesh, const std::set<int>& faces, int axis, PolySelection& out);
bool PolyFlip(PolyMesh& mesh, const std::set<int>& verts, int axis, PolySelection& out);
int PolyJoinFaces(PolyMesh& mesh, const std::set<int>& faces, PolySelection& out);
bool PolyFillFace(PolyMesh& mesh, const std::set<int>& verts, PolySelection& out);
int PolyDissolveVertices(PolyMesh& mesh, const std::set<int>& verts, PolySelection& out);
int PolyDissolveEdges(PolyMesh& mesh, const std::set<PolyEdge>& edges, PolySelection& out);
