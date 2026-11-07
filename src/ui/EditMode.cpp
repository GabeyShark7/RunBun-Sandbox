#include "ui/EditMode.h"
#include "render/SceneRenderer.h"
#include "render/EditMesh.h"
#include <cfloat>
#include <cmath>
#include <set>
#include <vector>

static bool active = false;
static int objectIndex = -1;
static EditSelectMode selectMode = EditSelect_Vertex;
static std::vector<EditMesh> editMeshes;
static std::vector<std::set<int>> selection;
static std::vector<std::vector<int>> affected;
static std::vector<std::vector<glm::vec3>> startPositions;

static const ImU32 edgeColor = IM_COL32(20, 20, 26, 170);
static const ImU32 vertexColor = IM_COL32(15, 15, 20, 230);
static const ImU32 selectedColor = IM_COL32(255, 170, 60, 255);
static const ImU32 selectedFill = IM_COL32(255, 170, 60, 70);

static SceneObject* EditObject(SceneRenderer& sceneRenderer) {
    auto& objects = sceneRenderer.GetObjects();
    if (!active || objectIndex < 0 || objectIndex >= (int)objects.size()) return nullptr;
    return &objects[objectIndex];
}

static bool Project(const glm::mat4& mvp, const glm::vec3& point, ImVec2 viewMin, ImVec2 viewSize, ImVec2& out) {
    glm::vec4 clip = mvp * glm::vec4(point, 1.0f);
    if (clip.w <= 0.0001f) return false;
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    out = ImVec2(viewMin.x + (ndc.x * 0.5f + 0.5f) * viewSize.x, viewMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewSize.y);
    return true;
}

static float DistanceToSegment(ImVec2 p, ImVec2 a, ImVec2 b) {
    ImVec2 ab = ImVec2(b.x - a.x, b.y - a.y);
    float lengthSq = ab.x * ab.x + ab.y * ab.y;
    float t = lengthSq > 0.0f ? ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / lengthSq : 0.0f;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    float dx = a.x + ab.x * t - p.x, dy = a.y + ab.y * t - p.y;
    return sqrtf(dx * dx + dy * dy);
}

static std::set<int> SelectedVertices(int meshIndex) {
    std::set<int> vertices;
    const EditMesh& mesh = editMeshes[meshIndex];
    for (int id : selection[meshIndex]) {
        if (selectMode == EditSelect_Vertex) {
            vertices.insert(id);
        } else if (selectMode == EditSelect_Edge) {
            vertices.insert(mesh.edges[id].x);
            vertices.insert(mesh.edges[id].y);
        } else {
            vertices.insert(mesh.faces[id].vertices.begin(), mesh.faces[id].vertices.end());
        }
    }
    return vertices;
}

bool IsEditModeActive() {
    return active;
}

int GetEditObjectIndex() {
    return active ? objectIndex : -1;
}

EditSelectMode GetEditSelectMode() {
    return selectMode;
}

const char* GetEditSelectModeName(EditSelectMode mode) {
    switch (mode) {
        case EditSelect_Vertex: return "Vertex";
        case EditSelect_Edge: return "Edge";
        default: return "Face";
    }
}

void SetEditSelectMode(EditSelectMode mode) {
    if (!active || mode == selectMode) {
        selectMode = mode;
        return;
    }

    for (size_t m = 0; m < editMeshes.size(); m++) {
        std::set<int> vertices = SelectedVertices((int)m);
        std::set<int> next;
        const EditMesh& mesh = editMeshes[m];
        if (mode == EditSelect_Vertex) {
            next = vertices;
        } else if (mode == EditSelect_Edge) {
            for (size_t e = 0; e < mesh.edges.size(); e++) {
                if (vertices.count(mesh.edges[e].x) && vertices.count(mesh.edges[e].y)) next.insert((int)e);
            }
        } else {
            for (size_t f = 0; f < mesh.faces.size(); f++) {
                bool all = !mesh.faces[f].vertices.empty();
                for (int v : mesh.faces[f].vertices) {
                    if (!vertices.count(v)) { all = false; break; }
                }
                if (all) next.insert((int)f);
            }
        }
        selection[m] = next;
    }
    selectMode = mode;
}

bool EnterEditMode(SceneRenderer& sceneRenderer, int index) {
    auto& objects = sceneRenderer.GetObjects();
    if (index < 0 || index >= (int)objects.size()) return false;

    std::vector<EditMesh> built;
    size_t totalVertices = 0;
    for (Mesh& mesh : objects[index].model.GetMeshes()) {
        EditMesh editMesh;
        editMesh.Build(mesh);
        totalVertices += editMesh.positions.size();
        built.push_back(std::move(editMesh));
    }
    if (totalVertices == 0) return false;

    editMeshes = std::move(built);
    selection.assign(editMeshes.size(), std::set<int>());
    affected.clear();
    startPositions.clear();
    objectIndex = index;
    active = true;
    return true;
}

void ExitEditMode() {
    active = false;
    objectIndex = -1;
    editMeshes.clear();
    selection.clear();
    affected.clear();
    startPositions.clear();
}

void ValidateEditMode(SceneRenderer& sceneRenderer, bool allowed) {
    if (!active) return;
    auto& objects = sceneRenderer.GetObjects();
    bool valid = allowed && objectIndex >= 0 && objectIndex < (int)objects.size() && sceneRenderer.selectedIndex == objectIndex
        && objects[objectIndex].model.GetMeshes().size() == editMeshes.size();
    if (!valid) ExitEditMode();
}

int GetEditSelectionCount() {
    int count = 0;
    for (const auto& set : selection) count += (int)set.size();
    return count;
}

void EditSelectAll(bool select) {
    for (size_t m = 0; m < editMeshes.size(); m++) {
        selection[m].clear();
        if (!select) continue;
        const EditMesh& mesh = editMeshes[m];
        int count = selectMode == EditSelect_Vertex ? (int)mesh.positions.size()
                  : selectMode == EditSelect_Edge ? (int)mesh.edges.size() : (int)mesh.faces.size();
        for (int i = 0; i < count; i++) selection[m].insert(i);
    }
}

void EditToggleSelectAll() {
    EditSelectAll(GetEditSelectionCount() == 0);
}

void EditPick(SceneRenderer& sceneRenderer, const glm::mat4& viewProj, ImVec2 viewMin, ImVec2 viewSize, ImVec2 mouse,
              const glm::vec3& rayOrigin, const glm::vec3& rayDir, bool additive) {
    SceneObject* obj = EditObject(sceneRenderer);
    if (!obj) return;

    glm::mat4 model = obj->GetModelMatrix();
    glm::mat4 mvp = viewProj * model;
    int bestMesh = -1, bestElement = -1;
    float best = FLT_MAX;

    if (selectMode == EditSelect_Vertex) {
        best = 12.0f;
        for (size_t m = 0; m < editMeshes.size(); m++) {
            for (size_t v = 0; v < editMeshes[m].positions.size(); v++) {
                ImVec2 p;
                if (!Project(mvp, editMeshes[m].positions[v], viewMin, viewSize, p)) continue;
                float d = sqrtf((p.x - mouse.x) * (p.x - mouse.x) + (p.y - mouse.y) * (p.y - mouse.y));
                if (d < best) { best = d; bestMesh = (int)m; bestElement = (int)v; }
            }
        }
    } else if (selectMode == EditSelect_Edge) {
        best = 8.0f;
        for (size_t m = 0; m < editMeshes.size(); m++) {
            const EditMesh& mesh = editMeshes[m];
            for (size_t e = 0; e < mesh.edges.size(); e++) {
                ImVec2 a, b;
                if (!Project(mvp, mesh.positions[mesh.edges[e].x], viewMin, viewSize, a)) continue;
                if (!Project(mvp, mesh.positions[mesh.edges[e].y], viewMin, viewSize, b)) continue;
                float d = DistanceToSegment(mouse, a, b);
                if (d < best) { best = d; bestMesh = (int)m; bestElement = (int)e; }
            }
        }
    } else {
        glm::mat4 inverseModel = glm::inverse(model);
        glm::vec3 origin = glm::vec3(inverseModel * glm::vec4(rayOrigin, 1.0f));
        glm::vec3 dir = glm::vec3(inverseModel * glm::vec4(rayDir, 0.0f));
        for (size_t m = 0; m < editMeshes.size(); m++) {
            const EditMesh& mesh = editMeshes[m];
            for (size_t t = 0; t < mesh.triangles.size(); t++) {
                if (mesh.triangleFace[t] < 0) continue;
                glm::vec3 p0 = mesh.positions[mesh.triangles[t].x];
                glm::vec3 e1 = mesh.positions[mesh.triangles[t].y] - p0;
                glm::vec3 e2 = mesh.positions[mesh.triangles[t].z] - p0;
                glm::vec3 pv = glm::cross(dir, e2);
                float det = glm::dot(e1, pv);
                if (std::fabs(det) < 1e-12f) continue;
                float inv = 1.0f / det;
                glm::vec3 tv = origin - p0;
                float u = glm::dot(tv, pv) * inv;
                if (u < 0.0f || u > 1.0f) continue;
                glm::vec3 qv = glm::cross(tv, e1);
                float v = glm::dot(dir, qv) * inv;
                if (v < 0.0f || u + v > 1.0f) continue;
                float hit = glm::dot(e2, qv) * inv;
                if (hit > 0.0f && hit < best) { best = hit; bestMesh = (int)m; bestElement = mesh.triangleFace[t]; }
            }
        }
    }

    if (additive) {
        if (bestMesh < 0) return;
        auto& set = selection[bestMesh];
        if (set.count(bestElement)) set.erase(bestElement);
        else set.insert(bestElement);
        return;
    }

    for (auto& set : selection) set.clear();
    if (bestMesh >= 0) selection[bestMesh].insert(bestElement);
}

void DrawEditOverlay(SceneRenderer& sceneRenderer, ImDrawList* drawList, const glm::mat4& viewProj, ImVec2 viewMin, ImVec2 viewSize) {
    SceneObject* obj = EditObject(sceneRenderer);
    if (!obj) return;

    glm::mat4 mvp = viewProj * obj->GetModelMatrix();
    for (size_t m = 0; m < editMeshes.size(); m++) {
        const EditMesh& mesh = editMeshes[m];
        std::vector<ImVec2> points(mesh.positions.size());
        std::vector<char> visible(mesh.positions.size(), 0);
        for (size_t v = 0; v < mesh.positions.size(); v++) visible[v] = Project(mvp, mesh.positions[v], viewMin, viewSize, points[v]) ? 1 : 0;

        std::set<int> selectedVertices = SelectedVertices((int)m);

        if (selectMode == EditSelect_Face) {
            for (int f : selection[m]) {
                for (int t : mesh.faces[f].triangles) {
                    const glm::ivec3& tri = mesh.triangles[t];
                    if (visible[tri.x] && visible[tri.y] && visible[tri.z])
                        drawList->AddTriangleFilled(points[tri.x], points[tri.y], points[tri.z], selectedFill);
                }
            }
        }

        for (int pass = 0; pass < 2; pass++) {
            for (size_t e = 0; e < mesh.edges.size(); e++) {
                int a = mesh.edges[e].x, b = mesh.edges[e].y;
                if (!visible[a] || !visible[b]) continue;
                bool isSelected = selectMode == EditSelect_Edge ? selection[m].count((int)e) > 0
                                : (selectedVertices.count(a) && selectedVertices.count(b));
                if ((pass == 1) != isSelected) continue;
                drawList->AddLine(points[a], points[b], isSelected ? selectedColor : edgeColor, isSelected ? 2.0f : 1.0f);
            }
        }

        if (selectMode == EditSelect_Vertex) {
            for (size_t v = 0; v < mesh.positions.size(); v++) {
                if (!visible[v] || selectedVertices.count((int)v)) continue;
                drawList->AddCircleFilled(points[v], 2.5f, vertexColor);
            }
            for (int v : selectedVertices) {
                if (visible[v]) drawList->AddCircleFilled(points[v], 4.0f, selectedColor);
            }
        } else if (selectMode == EditSelect_Face) {
            for (size_t f = 0; f < mesh.faces.size(); f++) {
                glm::vec3 center(0.0f);
                for (int v : mesh.faces[f].vertices) center += mesh.positions[v];
                center /= (float)mesh.faces[f].vertices.size();
                ImVec2 p;
                if (!Project(mvp, center, viewMin, viewSize, p)) continue;
                bool isSelected = selection[m].count((int)f) > 0;
                float h = isSelected ? 3.0f : 2.0f;
                drawList->AddRectFilled(ImVec2(p.x - h, p.y - h), ImVec2(p.x + h, p.y + h), isSelected ? selectedColor : vertexColor);
            }
        }
    }
}

bool EditGetSelectionCenter(SceneRenderer& sceneRenderer, glm::vec3& outWorld) {
    SceneObject* obj = EditObject(sceneRenderer);
    if (!obj) return false;
    glm::mat4 model = obj->GetModelMatrix();
    glm::vec3 sum(0.0f);
    int count = 0;
    for (size_t m = 0; m < editMeshes.size(); m++) {
        for (int v : SelectedVertices((int)m)) {
            sum += glm::vec3(model * glm::vec4(editMeshes[m].positions[v], 1.0f));
            count++;
        }
    }
    if (count == 0) return false;
    outWorld = sum / (float)count;
    return true;
}

bool EditBeginTransform(SceneRenderer& sceneRenderer, glm::vec3& outCenterWorld) {
    if (!EditGetSelectionCenter(sceneRenderer, outCenterWorld)) return false;
    affected.assign(editMeshes.size(), std::vector<int>());
    startPositions.assign(editMeshes.size(), std::vector<glm::vec3>());
    for (size_t m = 0; m < editMeshes.size(); m++) {
        for (int v : SelectedVertices((int)m)) {
            affected[m].push_back(v);
            startPositions[m].push_back(editMeshes[m].positions[v]);
        }
    }
    return true;
}

void EditApplyTransform(SceneRenderer& sceneRenderer, const std::function<glm::vec3(const glm::vec3&)>& worldTransform) {
    SceneObject* obj = EditObject(sceneRenderer);
    if (!obj || affected.size() != editMeshes.size()) return;

    glm::mat4 model = obj->GetModelMatrix();
    glm::mat4 inverseModel = glm::inverse(model);
    auto& meshes = obj->model.GetMeshes();
    for (size_t m = 0; m < editMeshes.size(); m++) {
        if (affected[m].empty()) continue;
        for (size_t k = 0; k < affected[m].size(); k++) {
            glm::vec3 world = glm::vec3(model * glm::vec4(startPositions[m][k], 1.0f));
            editMeshes[m].positions[affected[m][k]] = glm::vec3(inverseModel * glm::vec4(worldTransform(world), 1.0f));
        }
        editMeshes[m].Apply(meshes[m]);
    }
}

void EditCancelTransform(SceneRenderer& sceneRenderer) {
    EditApplyTransform(sceneRenderer, [](const glm::vec3& world) { return world; });
}

void EditMoveSelection(SceneRenderer& sceneRenderer, const glm::vec3& worldDelta) {
    glm::vec3 center;
    if (!EditBeginTransform(sceneRenderer, center)) return;
    EditApplyTransform(sceneRenderer, [&](const glm::vec3& world) { return world + worldDelta; });
}
