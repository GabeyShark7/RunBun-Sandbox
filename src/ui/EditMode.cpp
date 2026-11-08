#include "core/Profiler.h"
#include "ui/EditMode.h"
#include "core/UISettings.h"
#include "imgui/imgui_internal.h"
#include "ui/Console.h"
#include "render/SceneRenderer.h"
#include "render/EditMesh.h"
#include "render/PolyMesh.h"
#include "render/UVUnwrap.h"
#include "ui/RoomSync.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

static bool active = false;
static int objectIndex = -1;
static EditSelectMode selectMode = EditSelect_Vertex;
static std::vector<EditMesh> editMeshes;
static std::vector<PolyMesh> polyMeshes;
static std::vector<std::set<int>> selection;
static std::vector<std::vector<int>> affected;
static std::vector<std::vector<glm::vec3>> startPositions;
static unsigned int editRevision = 1;
static unsigned int overlayStamp = 1;
static void Touch() { overlayStamp++; }

// Points others are holding
struct ForeignOwner {
    std::string name;
    ImU32 color;
};
static std::vector<ForeignOwner> foreignOwners;
static std::vector<std::vector<int>> foreignVertex;
static std::vector<std::set<PolyEdge>> seams;
static EditView lastView;
static bool haveView = false;

struct UVSession {
    bool active = false;
    std::vector<std::vector<UVCorner>> corners;
    std::vector<std::vector<glm::vec2>> start;
};
static UVSession uvSession;
static bool selectThrough = false;
static std::string message;
static double messageTime = -100.0;

struct Visibility {
    std::vector<char> verts, edges, faces;
};
static std::vector<Visibility> visibility;
static unsigned int visibilityRevision = 0;
static glm::mat4 visibilityMvp(0.0f);
static bool visibilityThrough = false;

struct ToolState {
    EditTool tool = EditTool_None;
    int stage = 0;
    ImVec2 startMouse;
    float amount = -1.0f;
    std::vector<PolyMesh> base;
    std::vector<PolySelection> original;
    std::vector<std::set<int>> faces;
    std::vector<std::set<PolyEdge>> edges;
    int ringMesh = -1;
    int ringA = -1, ringB = -1;
    EdgeRing ring;
    int cuts = 1;
    ImVec2 slideDir;
    float slideLengthSq = 1.0f;
};
static ToolState toolState;

static const ImU32 edgeShadow = IM_COL32(0, 0, 0, 120);
static const ImU32 edgeColor = IM_COL32(215, 220, 230, 210);
static const ImU32 vertexFill = IM_COL32(25, 25, 32, 255);
static const ImU32 vertexRing = IM_COL32(230, 232, 240, 230);
static const ImU32 selectedColor = IM_COL32(255, 170, 60, 255);
static const ImU32 selectedFill = IM_COL32(255, 170, 60, 70);
static const ImU32 hoverColor = IM_COL32(255, 255, 255, 255);
static const ImU32 hoverFill = IM_COL32(255, 255, 255, 40);
static const ImU32 seamColor = IM_COL32(235, 60, 80, 255);

static void Say(const char* text) {
    message = text;
    messageTime = ImGui::GetTime();
    LogWarning(text);
}

static std::set<int> ConvertVertices(int m, const std::set<int>& vertices, EditSelectMode mode);
static std::set<int> SelectedVertices(int meshIndex);

unsigned int EditOverlayStamp();

void EditGetStats(int total[3], int picked[3]) {
    static unsigned int cachedStamp = 0;
    static int cachedTotal[3], cachedPicked[3];
    if (active && cachedStamp == EditOverlayStamp()) {
        for (int k = 0; k < 3; k++) { total[k] = cachedTotal[k]; picked[k] = cachedPicked[k]; }
        return;
    }
    cachedStamp = EditOverlayStamp();
    for (int k = 0; k < 3; k++) total[k] = picked[k] = 0;
    for (size_t m = 0; m < editMeshes.size(); m++) {
        const EditMesh& mesh = editMeshes[m];
        total[0] += (int)mesh.positions.size();
        total[1] += (int)mesh.edges.size();
        total[2] += (int)mesh.faces.size();
        std::set<int> verts = SelectedVertices((int)m);
        picked[0] += (int)verts.size();
        picked[1] += selectMode == EditSelect_Edge ? (int)selection[m].size() : (int)ConvertVertices((int)m, verts, EditSelect_Edge).size();
        picked[2] += selectMode == EditSelect_Face ? (int)selection[m].size() : (int)ConvertVertices((int)m, verts, EditSelect_Face).size();
    }
    for (int k = 0; k < 3; k++) { cachedTotal[k] = total[k]; cachedPicked[k] = picked[k]; }
}

const char* GetEditMessage() {
    return (!message.empty() && ImGui::GetTime() - messageTime < 5.0) ? message.c_str() : nullptr;
}

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

static void ScreenRay(const EditView& view, ImVec2 point, glm::vec3& origin, glm::vec3& dir) {
    float ndcX = (point.x - view.min.x) / view.size.x * 2.0f - 1.0f;
    float ndcY = 1.0f - (point.y - view.min.y) / view.size.y * 2.0f;
    glm::mat4 inverse = glm::inverse(view.viewProj);
    glm::vec4 nearPoint = inverse * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farPoint = inverse * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    origin = glm::vec3(nearPoint) / nearPoint.w;
    dir = glm::normalize(glm::vec3(farPoint) / farPoint.w - origin);
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

static bool VertexLocked(int m, int v) {
    return m < (int)foreignVertex.size() && v >= 0 && v < (int)foreignVertex[m].size() && foreignVertex[m][v] >= 0;
}

static bool ElementLocked(int m, int element, EditSelectMode mode) {
    if (m >= (int)foreignVertex.size() || foreignVertex[m].empty()) return false;
    const EditMesh& mesh = editMeshes[m];
    if (mode == EditSelect_Vertex) return VertexLocked(m, element);
    if (mode == EditSelect_Edge) return VertexLocked(m, mesh.edges[element].x) || VertexLocked(m, mesh.edges[element].y);
    for (int v : mesh.faces[element].vertices) if (VertexLocked(m, v)) return true;
    return false;
}

static void SayLocked(int m, int element, EditSelectMode mode) {
    const EditMesh& mesh = editMeshes[m];
    std::vector<int> verts;
    if (mode == EditSelect_Vertex) verts = { element };
    else if (mode == EditSelect_Edge) verts = { mesh.edges[element].x, mesh.edges[element].y };
    else verts = mesh.faces[element].vertices;
    for (int v : verts) {
        if (!VertexLocked(m, v)) continue;
        std::string text = foreignOwners[foreignVertex[m][v]].name + " is working on that part.";
        message = text;
        messageTime = ImGui::GetTime();
        LogRoom(text);
        return;
    }
}

static void DropLocked(std::set<int>& set, int m, EditSelectMode mode) {
    for (auto it = set.begin(); it != set.end();) {
        if (ElementLocked(m, *it, mode)) it = set.erase(it);
        else ++it;
    }
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
        case EditSelect_Vertex: return "Points";
        case EditSelect_Edge: return "Edges";
        default: return "Faces";
    }
}

bool GetEditSelectThrough() {
    return selectThrough;
}

void SetEditSelectThrough(bool through) {
    Touch();
    selectThrough = through;
}

void SetEditShowHidden(bool show) {
    Touch();
    GetUISettings().editShowHidden = show;
    ImGui::MarkIniSettingsDirty();
}

static std::set<int> ConvertVertices(int m, const std::set<int>& vertices, EditSelectMode mode) {
    std::set<int> next;
    const EditMesh& mesh = editMeshes[m];
    if (mode == EditSelect_Vertex) {
        next = vertices;
    } else if (mode == EditSelect_Edge) {
        for (size_t e = 0; e < mesh.edges.size(); e++)
            if (vertices.count(mesh.edges[e].x) && vertices.count(mesh.edges[e].y)) next.insert((int)e);
    } else {
        for (size_t f = 0; f < mesh.faces.size(); f++) {
            bool all = !mesh.faces[f].vertices.empty();
            for (int v : mesh.faces[f].vertices) {
                if (!vertices.count(v)) { all = false; break; }
            }
            if (all) next.insert((int)f);
        }
    }
    return next;
}

void SetEditSelectMode(EditSelectMode mode) {
    Touch();
    if (!active || mode == selectMode) {
        selectMode = mode;
        return;
    }
    for (size_t m = 0; m < editMeshes.size(); m++) selection[m] = ConvertVertices((int)m, SelectedVertices((int)m), mode);
    selectMode = mode;
}

bool EnterEditMode(SceneRenderer& sceneRenderer, int index) {
    Touch();
    auto& objects = sceneRenderer.GetObjects();
    if (index < 0 || index >= (int)objects.size()) return false;

    std::vector<EditMesh> built;
    std::vector<PolyMesh> polys;
    size_t totalVertices = 0;
    for (Mesh& mesh : objects[index].model.GetMeshes()) {
        EditMesh editMesh;
        editMesh.Build(mesh);
        PolyMesh poly;
        poly.FromEdit(editMesh, mesh);
        totalVertices += editMesh.positions.size();
        built.push_back(std::move(editMesh));
        polys.push_back(std::move(poly));
    }
    if (totalVertices == 0) return false;

    editMeshes = std::move(built);
    polyMeshes = std::move(polys);
    seams.clear();
    for (const PolyMesh& poly : polyMeshes) seams.push_back(PolyUVSeams(poly));
    uvSession = UVSession();
    selection.assign(editMeshes.size(), std::set<int>());
    affected.clear();
    startPositions.clear();
    toolState = ToolState();
    objectIndex = index;
    active = true;
    editRevision++;
    return true;
}

void ExitEditMode() {
    active = false;
    objectIndex = -1;
    editMeshes.clear();
    polyMeshes.clear();
    seams.clear();
    uvSession = UVSession();
    selection.clear();
    affected.clear();
    startPositions.clear();
    visibility.clear();
    toolState = ToolState();
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
    Touch();
    for (size_t m = 0; m < editMeshes.size(); m++) {
        selection[m].clear();
        if (!select) continue;
        const EditMesh& mesh = editMeshes[m];
        int count = selectMode == EditSelect_Vertex ? (int)mesh.positions.size()
                  : selectMode == EditSelect_Edge ? (int)mesh.edges.size() : (int)mesh.faces.size();
        for (int i = 0; i < count; i++) selection[m].insert(i);
        DropLocked(selection[m], (int)m, selectMode);
    }
}

void EditToggleSelectAll() {
    EditSelectAll(GetEditSelectionCount() == 0);
}

// Is the segment blocked
static bool Blocked(const EditMesh& mesh, const glm::vec3& eye, const glm::vec3& target, int ignoreA, int ignoreB, int ignoreFace) {
    glm::vec3 dir = target - eye;
    for (size_t t = 0; t < mesh.triangles.size(); t++) {
        const glm::ivec3& tri = mesh.triangles[t];
        if (mesh.triangleFace[t] < 0 || mesh.triangleFace[t] == ignoreFace) continue;
        if (tri.x == ignoreA || tri.y == ignoreA || tri.z == ignoreA || tri.x == ignoreB || tri.y == ignoreB || tri.z == ignoreB) continue;
        glm::vec3 p0 = mesh.positions[tri.x];
        glm::vec3 e1 = mesh.positions[tri.y] - p0;
        glm::vec3 e2 = mesh.positions[tri.z] - p0;
        glm::vec3 pv = glm::cross(dir, e2);
        float det = glm::dot(e1, pv);
        if (std::fabs(det) < 1e-14f) continue;
        float inv = 1.0f / det;
        glm::vec3 tv = eye - p0;
        float u = glm::dot(tv, pv) * inv;
        if (u < 0.0f || u > 1.0f) continue;
        glm::vec3 qv = glm::cross(tv, e1);
        float v = glm::dot(dir, qv) * inv;
        if (v < 0.0f || u + v > 1.0f) continue;
        float hit = glm::dot(e2, qv) * inv;
        if (hit > 1e-4f && hit < 0.999f) return true;
    }
    return false;
}

static glm::vec3 FaceCenter(const EditMesh& mesh, int f) {
    glm::vec3 center(0.0f);
    for (int v : mesh.faces[f].vertices) center += mesh.positions[v];
    return mesh.faces[f].vertices.empty() ? center : center / (float)mesh.faces[f].vertices.size();
}

static void UpdateVisibility(SceneRenderer& sceneRenderer, const EditView& view) {
    PROFILE("UpdateVisibility");
    SceneObject* obj = EditObject(sceneRenderer);
    if (!obj) return;
    glm::mat4 model = obj->GetModelMatrix();
    glm::mat4 mvp = view.viewProj * model;
    bool same = visibility.size() == editMeshes.size() && visibilityRevision == editRevision && visibilityThrough == selectThrough &&
        memcmp(&mvp, &visibilityMvp, sizeof(mvp)) == 0;
    if (same) return;
    visibilityRevision = editRevision;
    visibilityMvp = mvp;
    visibilityThrough = selectThrough;
    visibility.assign(editMeshes.size(), Visibility());

    glm::vec3 eye = glm::vec3(glm::inverse(model) * glm::vec4(view.eye, 1.0f));
    for (size_t m = 0; m < editMeshes.size(); m++) {
        const EditMesh& mesh = editMeshes[m];
        Visibility& vis = visibility[m];
        vis.verts.assign(mesh.positions.size(), 1);
        vis.edges.assign(mesh.edges.size(), 1);
        vis.faces.assign(mesh.faces.size(), 1);
        double work = (double)(mesh.positions.size() + mesh.edges.size() + mesh.faces.size()) * (double)mesh.triangles.size();
        if (selectThrough || work > 6e6) continue;
        for (size_t v = 0; v < mesh.positions.size(); v++)
            vis.verts[v] = Blocked(mesh, eye, mesh.positions[v], (int)v, -1, -1) ? 0 : 1;
        for (size_t e = 0; e < mesh.edges.size(); e++) {
            glm::vec3 mid = (mesh.positions[mesh.edges[e].x] + mesh.positions[mesh.edges[e].y]) * 0.5f;
            vis.edges[e] = Blocked(mesh, eye, mid, mesh.edges[e].x, mesh.edges[e].y, -1) ? 0 : 1;
        }
        for (size_t f = 0; f < mesh.faces.size(); f++)
            vis.faces[f] = Blocked(mesh, eye, FaceCenter(mesh, (int)f), -1, -1, (int)f) ? 0 : 1;
    }
}

static bool FindElement(SceneRenderer& sceneRenderer, const EditView& view, ImVec2 mouse, EditSelectMode mode, int& outMesh, int& outElement) {
    PROFILE("FindElement");
    SceneObject* obj = EditObject(sceneRenderer);
    if (!obj) return false;
    UpdateVisibility(sceneRenderer, view);

    glm::mat4 model = obj->GetModelMatrix();
    glm::mat4 mvp = view.viewProj * model;
    outMesh = -1;
    outElement = -1;
    float best = FLT_MAX;

    if (mode == EditSelect_Vertex) {
        best = 12.0f;
        for (size_t m = 0; m < editMeshes.size(); m++) {
            for (size_t v = 0; v < editMeshes[m].positions.size(); v++) {
                if (!visibility[m].verts[v]) continue;
                ImVec2 p;
                if (!Project(mvp, editMeshes[m].positions[v], view.min, view.size, p)) continue;
                float d = sqrtf((p.x - mouse.x) * (p.x - mouse.x) + (p.y - mouse.y) * (p.y - mouse.y));
                if (d < best) { best = d; outMesh = (int)m; outElement = (int)v; }
            }
        }
    } else if (mode == EditSelect_Edge) {
        best = 8.0f;
        for (size_t m = 0; m < editMeshes.size(); m++) {
            const EditMesh& mesh = editMeshes[m];
            for (size_t e = 0; e < mesh.edges.size(); e++) {
                if (!visibility[m].edges[e]) continue;
                ImVec2 a, b;
                if (!Project(mvp, mesh.positions[mesh.edges[e].x], view.min, view.size, a)) continue;
                if (!Project(mvp, mesh.positions[mesh.edges[e].y], view.min, view.size, b)) continue;
                float d = DistanceToSegment(mouse, a, b);
                if (d < best) { best = d; outMesh = (int)m; outElement = (int)e; }
            }
        }
    } else {
        glm::vec3 rayOrigin, rayDir;
        ScreenRay(view, mouse, rayOrigin, rayDir);
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
                if (hit > 0.0f && hit < best) { best = hit; outMesh = (int)m; outElement = mesh.triangleFace[t]; }
            }
        }
    }
    return outMesh >= 0;
}

void EditPick(SceneRenderer& sceneRenderer, const EditView& view, ImVec2 mouse, bool additive) {
    Touch();
    int bestMesh, bestElement;
    bool found = FindElement(sceneRenderer, view, mouse, selectMode, bestMesh, bestElement);
    if (found && ElementLocked(bestMesh, bestElement, selectMode)) {
        SayLocked(bestMesh, bestElement, selectMode);
        return;
    }
    if (additive) {
        if (!found) return;
        auto& set = selection[bestMesh];
        if (set.count(bestElement)) set.erase(bestElement);
        else set.insert(bestElement);
        return;
    }
    for (auto& set : selection) set.clear();
    if (found) selection[bestMesh].insert(bestElement);
}

void EditBoxSelect(SceneRenderer& sceneRenderer, const EditView& view, ImVec2 a, ImVec2 b, EditBoxMode boxMode) {
    Touch();
    SceneObject* obj = EditObject(sceneRenderer);
    if (!obj) return;
    UpdateVisibility(sceneRenderer, view);
    ImVec2 lo = ImVec2(fminf(a.x, b.x), fminf(a.y, b.y)), hi = ImVec2(fmaxf(a.x, b.x), fmaxf(a.y, b.y));
    glm::mat4 mvp = view.viewProj * obj->GetModelMatrix();
    auto inside = [&](const glm::vec3& p) {
        ImVec2 s;
        return Project(mvp, p, view.min, view.size, s) && s.x >= lo.x && s.x <= hi.x && s.y >= lo.y && s.y <= hi.y;
    };

    for (size_t m = 0; m < editMeshes.size(); m++) {
        const EditMesh& mesh = editMeshes[m];
        std::set<int> hits;
        if (selectMode == EditSelect_Vertex) {
            for (size_t v = 0; v < mesh.positions.size(); v++)
                if (visibility[m].verts[v] && inside(mesh.positions[v])) hits.insert((int)v);
        } else if (selectMode == EditSelect_Edge) {
            for (size_t e = 0; e < mesh.edges.size(); e++)
                if (visibility[m].edges[e] && inside(mesh.positions[mesh.edges[e].x]) && inside(mesh.positions[mesh.edges[e].y])) hits.insert((int)e);
        } else {
            for (size_t f = 0; f < mesh.faces.size(); f++)
                if (visibility[m].faces[f] && inside(FaceCenter(mesh, (int)f))) hits.insert((int)f);
        }
        DropLocked(hits, (int)m, selectMode);
        if (boxMode == EditBox_Replace) selection[m] = hits;
        else if (boxMode == EditBox_Add) selection[m].insert(hits.begin(), hits.end());
        else for (int h : hits) selection[m].erase(h);
    }
}

void DrawEditOverlay(SceneRenderer& sceneRenderer, ImDrawList* drawList, const EditView& view, const ImVec2* hoverMouse) {
    PROFILE("DrawEditOverlay");
    SceneObject* obj = EditObject(sceneRenderer);
    if (!obj) return;
    {
        glm::mat4 mvp = view.viewProj * obj->GetModelMatrix();
        std::vector<bool> labeled(foreignOwners.size(), false);
        int drawn = 0;
        for (size_t m = 0; m < foreignVertex.size() && m < editMeshes.size(); m++) {
            for (size_t v = 0; v < foreignVertex[m].size() && drawn < 6000; v++) {
                int owner = foreignVertex[m][v];
                if (owner < 0) continue;
                ImVec2 p;
                if (!Project(mvp, editMeshes[m].positions[v], view.min, view.size, p)) continue;
                drawList->AddCircleFilled(p, 5.0f, IM_COL32(0, 0, 0, 170));
                drawList->AddCircleFilled(p, 3.8f, foreignOwners[owner].color);
                drawn++;
                if (!labeled[owner]) {
                    labeled[owner] = true;
                    const char* name = foreignOwners[owner].name.c_str();
                    ImVec2 size = ImGui::CalcTextSize(name);
                    drawList->AddRectFilled(ImVec2(p.x + 8, p.y - size.y - 6), ImVec2(p.x + size.x + 16, p.y - 2), foreignOwners[owner].color, 4.0f);
                    drawList->AddText(ImVec2(p.x + 12, p.y - size.y - 4), IM_COL32(15, 15, 20, 255), name);
                }
            }
        }
    }
    if (!hoverMouse || toolState.tool != EditTool_None) return;
    int m = -1, element = -1;
    if (!FindElement(sceneRenderer, view, *hoverMouse, selectMode, m, element)) return;
    glm::mat4 mvp = view.viewProj * obj->GetModelMatrix();
    const EditMesh& mesh = editMeshes[m];
    if (selectMode == EditSelect_Vertex) {
        ImVec2 p;
        if (Project(mvp, mesh.positions[element], view.min, view.size, p)) drawList->AddCircle(p, 6.5f, hoverColor, 0, 2.0f);
    } else if (selectMode == EditSelect_Edge) {
        ImVec2 a, b;
        if (Project(mvp, mesh.positions[mesh.edges[element].x], view.min, view.size, a) && Project(mvp, mesh.positions[mesh.edges[element].y], view.min, view.size, b))
            drawList->AddLine(a, b, hoverColor, 2.6f);
    } else {
        ImDrawListFlags flags = drawList->Flags;
        drawList->Flags &= ~ImDrawListFlags_AntiAliasedFill;
        for (int t : mesh.faces[element].triangles) {
            const glm::ivec3& tri = mesh.triangles[t];
            ImVec2 a, b, c;
            if (Project(mvp, mesh.positions[tri.x], view.min, view.size, a) && Project(mvp, mesh.positions[tri.y], view.min, view.size, b) &&
                Project(mvp, mesh.positions[tri.z], view.min, view.size, c))
                drawList->AddTriangleFilled(a, b, c, hoverFill);
        }
        drawList->Flags = flags;
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
    editRevision++;
}

void EditCancelTransform(SceneRenderer& sceneRenderer) {
    EditApplyTransform(sceneRenderer, [](const glm::vec3& world) { return world; });
}

void EditMoveSelection(SceneRenderer& sceneRenderer, const glm::vec3& worldDelta) {
    glm::vec3 center;
    if (!EditBeginTransform(sceneRenderer, center)) return;
    EditApplyTransform(sceneRenderer, [&](const glm::vec3& world) { return world + worldDelta; });
}

void EditCollectFixedPoints(SceneRenderer& sceneRenderer, std::vector<glm::vec3>& outWorld) {
    SceneObject* obj = EditObject(sceneRenderer);
    if (!obj) return;
    glm::mat4 model = obj->GetModelMatrix();
    for (size_t m = 0; m < editMeshes.size(); m++) {
        std::set<int> moving = SelectedVertices((int)m);
        for (size_t v = 0; v < editMeshes[m].positions.size(); v++)
            if (!moving.count((int)v)) outWorld.push_back(glm::vec3(model * glm::vec4(editMeshes[m].positions[v], 1.0f)));
    }
}

// Topology changes go here
static PolySelection CurrentPolySelection(int m) {
    PolySelection out;
    const EditMesh& mesh = editMeshes[m];
    out.verts = SelectedVertices(m);
    if (selectMode == EditSelect_Edge) {
        for (int e : selection[m]) out.edges.insert(MakeEdge(mesh.edges[e].x, mesh.edges[e].y));
    } else {
        for (size_t e = 0; e < mesh.edges.size(); e++)
            if (out.verts.count(mesh.edges[e].x) && out.verts.count(mesh.edges[e].y)) out.edges.insert(MakeEdge(mesh.edges[e].x, mesh.edges[e].y));
    }
    std::set<int> editFaces;
    if (selectMode == EditSelect_Face) editFaces = selection[m];
    else editFaces = ConvertVertices(m, out.verts, EditSelect_Face);
    const PolyMesh& poly = polyMeshes[m];
    for (size_t f = 0; f < poly.faces.size(); f++)
        if (editFaces.count(poly.faces[f].source)) out.faces.insert((int)f);
    return out;
}

static void SyncPositions(int m) {
    polyMeshes[m].positions = editMeshes[m].positions;
}

static void ApplyPolySelection(int m, const PolySelection& chosen) {
    Touch();
    const PolyMesh& poly = polyMeshes[m];
    const EditMesh& mesh = editMeshes[m];
    std::set<int> verts = chosen.verts;
    for (const PolyEdge& e : chosen.edges) { verts.insert(e.first); verts.insert(e.second); }
    for (int f : chosen.faces) if (f < (int)poly.faces.size()) verts.insert(poly.faces[f].verts.begin(), poly.faces[f].verts.end());

    std::set<int> next;
    if (selectMode == EditSelect_Vertex) {
        for (int v : verts) if (v < (int)mesh.positions.size()) next.insert(v);
    } else if (selectMode == EditSelect_Edge) {
        std::set<PolyEdge> wanted = chosen.edges;
        for (int f : chosen.faces) {
            if (f >= (int)poly.faces.size()) continue;
            const std::vector<int>& loop = poly.faces[f].verts;
            for (size_t i = 0; i < loop.size(); i++) wanted.insert(MakeEdge(loop[i], loop[(i + 1) % loop.size()]));
        }
        if (wanted.empty()) next = ConvertVertices(m, verts, EditSelect_Edge);
        for (size_t e = 0; e < mesh.edges.size(); e++)
            if (wanted.count(MakeEdge(mesh.edges[e].x, mesh.edges[e].y))) next.insert((int)e);
    } else {
        for (int f : chosen.faces) if (f < (int)mesh.faces.size()) next.insert(f);
        if (chosen.faces.empty()) next = ConvertVertices(m, verts, EditSelect_Face);
    }
    selection[m] = next;
}

static void Rebuild(SceneObject& obj, int m, const PolySelection& picked, bool keepSeams = false) {
    PolyMesh& poly = polyMeshes[m];
    PolySelection chosen = picked;
    std::vector<int> vertexMap;
    poly.RemoveUnused(&chosen, &vertexMap);
    if (keepSeams && m < seams.size()) {
        std::set<PolyEdge> moved;
        for (const PolyEdge& e : seams[m]) {
            if (e.first >= (int)vertexMap.size() || e.second >= (int)vertexMap.size()) continue;
            if (vertexMap[e.first] >= 0 && vertexMap[e.second] >= 0) moved.insert(MakeEdge(vertexMap[e.first], vertexMap[e.second]));
        }
        seams[m] = moved;
    } else if (m < seams.size()) {
        seams[m] = PolyUVSeams(poly);
    }
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    poly.ToRender(vertices, indices, editMeshes[m]);
    for (size_t f = 0; f < poly.faces.size(); f++) poly.faces[f].source = (int)f;
    obj.model.GetMeshes()[m].Rebuild(vertices, indices);
    ApplyPolySelection(m, chosen);
    affected.clear();
    startPositions.clear();
    editRevision++;
}

bool EditExtrude(SceneRenderer& sceneRenderer, glm::vec3& outWorldDirection, bool& outAlongNormal) {
    SceneObject* obj = EditObject(sceneRenderer);
    if (!obj) return false;
    bool any = false, alongNormal = true;
    glm::vec3 normalSum(0.0f);
    for (size_t m = 0; m < editMeshes.size(); m++) {
        if (selection[m].empty()) continue;
        SyncPositions((int)m);
        PolySelection current = CurrentPolySelection((int)m);
        PolySelection out;
        glm::vec3 normal;
        bool done = false;
        for (int attempt = 0; attempt < 2 && !done; attempt++) {
            bool edgesFirst = selectMode == EditSelect_Edge;
            if ((attempt == 0) != edgesFirst) {
                if (current.faces.empty()) continue;
                done = PolyExtrudeFaces(polyMeshes[m], current.faces, out, normal);
                if (done) normalSum += normal;
            } else {
                if (current.edges.empty()) continue;
                done = PolyExtrudeEdges(polyMeshes[m], current.edges, out);
                if (done) alongNormal = false;
            }
        }
        if (!done) continue;
        Rebuild(*obj, (int)m, out);
        any = true;
    }
    if (!any) {
        Say(GetEditSelectionCount() == 0 ? "Pick some faces to extrude first."
            : "Extrude works on faces, or on open edges at the border of a shape. Picking every face of a closed shape cannot be extruded.");
        return false;
    }
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(obj->GetModelMatrix())));
    glm::vec3 world = normalMatrix * normalSum;
    float length = glm::length(world);
    outAlongNormal = alongNormal && length > 1e-12f;
    outWorldDirection = outAlongNormal ? world / length : glm::vec3(0.0f);
    return true;
}

template <typename Op>
static bool RunOnMeshes(SceneRenderer& sceneRenderer, Op op) {
    SceneObject* obj = EditObject(sceneRenderer);
    if (!obj) return false;
    bool any = false;
    for (size_t m = 0; m < editMeshes.size(); m++) {
        SyncPositions((int)m);
        PolySelection current = CurrentPolySelection((int)m);
        PolySelection out;
        if (!op(polyMeshes[m], current, out)) continue;
        Rebuild(*obj, (int)m, out);
        any = true;
    }
    return any;
}

bool EditSubdivide(SceneRenderer& sceneRenderer) {
    bool done = RunOnMeshes(sceneRenderer, [](PolyMesh& poly, const PolySelection& current, PolySelection& out) {
        if (selectMode == EditSelect_Edge) return PolySubdivideEdges(poly, current.edges, out);
        if (!current.faces.empty()) return PolySubdivideFaces(poly, current.faces, out);
        return PolySubdivideEdges(poly, current.edges, out);
    });
    if (!done) Say("Pick faces or edges to subdivide first.");
    return done;
}

bool EditMerge(SceneRenderer& sceneRenderer) {
    bool done = RunOnMeshes(sceneRenderer, [](PolyMesh& poly, const PolySelection& current, PolySelection& out) {
        return PolyMergeVertices(poly, current.verts, out);
    });
    if (!done) Say("Pick two or more points to merge.");
    return done;
}

int EditWeldDoubles(SceneRenderer& sceneRenderer) {
    int total = 0;
    RunOnMeshes(sceneRenderer, [&](PolyMesh& poly, const PolySelection& current, PolySelection& out) {
        glm::vec3 lo(1e30f), hi(-1e30f);
        for (const glm::vec3& p : poly.positions) { lo = glm::min(lo, p); hi = glm::max(hi, p); }
        out.verts = current.verts;
        int merged = PolyMergeByDistance(poly, std::max(glm::length(hi - lo) * 1e-4f, 1e-7f), out);
        total += merged;
        return merged > 0;
    });
    char text[96];
    snprintf(text, sizeof(text), total ? "Welded %d doubled points." : "No doubled points found.", total);
    Say(text);
    return total;
}

bool EditDelete(SceneRenderer& sceneRenderer) {
    if (GetEditSelectionCount() == 0) {
        Say("Pick something to delete first.");
        return false;
    }
    bool refused = false;
    bool done = RunOnMeshes(sceneRenderer, [&](PolyMesh& poly, const PolySelection& current, PolySelection& out) {
        std::set<int> doomed;
        for (size_t f = 0; f < poly.faces.size(); f++) {
            const std::vector<int>& loop = poly.faces[f].verts;
            bool hit = false;
            for (size_t i = 0; i < loop.size() && !hit; i++) {
                if (selectMode == EditSelect_Vertex) hit = current.verts.count(loop[i]) > 0;
                else if (selectMode == EditSelect_Edge) hit = current.edges.count(MakeEdge(loop[i], loop[(i + 1) % loop.size()])) > 0;
            }
            if (selectMode == EditSelect_Face) hit = current.faces.count((int)f) > 0;
            if (hit) doomed.insert((int)f);
        }
        if (doomed.empty()) return false;
        if (doomed.size() >= poly.faces.size()) { refused = true; return false; }
        return PolyDeleteFaces(poly, doomed, out);
    });
    if (refused) Say("That would delete the whole shape. Delete the object instead.");
    return done;
}

bool EditJoinOrFill(SceneRenderer& sceneRenderer) {
    bool faceMode = selectMode == EditSelect_Face;
    bool done = RunOnMeshes(sceneRenderer, [&](PolyMesh& poly, const PolySelection& current, PolySelection& out) {
        if (faceMode) return PolyJoinFaces(poly, current.faces, out) > 0;
        return PolyFillFace(poly, current.verts, out);
    });
    if (!done) Say(faceMode ? "Pick two or more faces that touch to join them into one."
                            : "Pick 3 or more points around a hole (or in a ring) to fill a face.");
    return done;
}

bool EditDissolve(SceneRenderer& sceneRenderer) {
    if (selectMode == EditSelect_Face) return EditDelete(sceneRenderer);
    bool done = RunOnMeshes(sceneRenderer, [&](PolyMesh& poly, const PolySelection& current, PolySelection& out) {
        if (selectMode == EditSelect_Edge) return PolyDissolveEdges(poly, current.edges, out) > 0;
        return PolyDissolveVertices(poly, current.verts, out) > 0;
    });
    if (!done) Say(selectMode == EditSelect_Edge ? "Pick edges between two faces to dissolve them." : "Pick points to dissolve.");
    return done;
}

bool EditMirrorCopy(SceneRenderer& sceneRenderer, int axis) {
    bool done = RunOnMeshes(sceneRenderer, [&](PolyMesh& poly, const PolySelection& current, PolySelection& out) {
        std::set<int> faces = current.faces;
        if (faces.empty()) for (size_t f = 0; f < poly.faces.size(); f++) faces.insert((int)f);
        return PolyMirrorCopy(poly, faces, axis, out);
    });
    if (!done) Say("Nothing to mirror. Faces lying on the middle line are skipped.");
    return done;
}

bool EditFlip(SceneRenderer& sceneRenderer, int axis) {
    bool done = RunOnMeshes(sceneRenderer, [&](PolyMesh& poly, const PolySelection& current, PolySelection& out) {
        return PolyFlip(poly, current.verts, axis, out);
    });
    if (!done) Say("Pick something to flip first.");
    return done;
}

EditTool GetEditTool() {
    return toolState.tool;
}

bool EditBeginTool(SceneRenderer& sceneRenderer, EditTool tool, ImVec2 mouse) {
    if (!EditObject(sceneRenderer) || toolState.tool != EditTool_None) return false;
    ToolState next;
    next.tool = tool;
    next.startMouse = mouse;
    next.cuts = toolState.cuts > 0 ? toolState.cuts : 1;
    bool any = false;
    for (size_t m = 0; m < editMeshes.size(); m++) {
        SyncPositions((int)m);
        PolySelection current = CurrentPolySelection((int)m);
        next.base.push_back(polyMeshes[m]);
        next.original.push_back(current);
        next.faces.push_back(current.faces);
        next.edges.push_back(current.edges);
        if (tool == EditTool_Inset && !current.faces.empty()) any = true;
        if (tool == EditTool_Bevel && !current.edges.empty()) any = true;
    }
    if (tool == EditTool_LoopCut) any = true;
    if (!any) {
        Say(tool == EditTool_Inset ? "Pick some faces to inset first." : "Pick some edges to bevel first.");
        return false;
    }
    toolState = next;
    return true;
}

static void RestoreBase(SceneObject& obj, bool keepOriginalSelection) {
    for (size_t m = 0; m < toolState.base.size() && m < polyMeshes.size(); m++) {
        polyMeshes[m] = toolState.base[m];
        Rebuild(obj, (int)m, keepOriginalSelection ? toolState.original[m] : PolySelection());
    }
}

void EditCancelTool(SceneRenderer& sceneRenderer) {
    SceneObject* obj = EditObject(sceneRenderer);
    bool changed = toolState.tool != EditTool_None && (toolState.tool != EditTool_LoopCut || toolState.stage == 1);
    if (obj && changed) RestoreBase(*obj, true);
    int cuts = toolState.cuts;
    toolState = ToolState();
    toolState.cuts = cuts;
}

static void FinishTool() {
    int cuts = toolState.cuts;
    toolState = ToolState();
    toolState.cuts = cuts;
}

static void ApplyAmount(SceneObject& obj, float amount) {
    for (size_t m = 0; m < toolState.base.size(); m++) {
        polyMeshes[m] = toolState.base[m];
        PolySelection out;
        bool done = toolState.tool == EditTool_Inset ? PolyInsetFaces(polyMeshes[m], toolState.faces[m], amount, out)
                                                    : PolyBevelEdges(polyMeshes[m], toolState.edges[m], amount, out);
        Rebuild(obj, (int)m, done ? out : toolState.original[m]);
    }
}

static void ApplyLoopCut(SceneObject& obj, float slide) {
    int m = toolState.ringMesh;
    polyMeshes[m] = toolState.base[m];
    PolySelection out;
    PolyLoopCut(polyMeshes[m], toolState.ring, toolState.cuts, slide, out);
    Rebuild(obj, m, out);
}

bool EditUpdateTool(SceneRenderer& sceneRenderer, ImDrawList* drawList, const EditView& view, ImVec2 mouse, bool viewHovered, char* readout, size_t readoutSize) {
    SceneObject* obj = EditObject(sceneRenderer);
    if (!obj || toolState.tool == EditTool_None) return false;
    ImGuiIO& io = ImGui::GetIO();
    bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsMouseClicked(ImGuiMouseButton_Right);
    bool confirm = ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false);
    glm::mat4 mvp = view.viewProj * obj->GetModelMatrix();

    if (cancel) {
        EditCancelTool(sceneRenderer);
        return true;
    }

    if (toolState.tool == EditTool_Inset || toolState.tool == EditTool_Bevel) {
        float amount = 0.2f + (mouse.x - toolState.startMouse.x) / 500.0f;
        if (io.KeyCtrl) amount = roundf(amount * 20.0f) / 20.0f;
        amount = amount < 0.01f ? 0.01f : (amount > 0.95f ? 0.95f : amount);
        if (amount != toolState.amount) {
            toolState.amount = amount;
            ApplyAmount(*obj, amount);
        }
        snprintf(readout, readoutSize, "%s   %.0f%%   Move the mouse left or right", toolState.tool == EditTool_Inset ? "Inset" : "Bevel", amount * 100.0f);
        if (confirm) FinishTool();
        return confirm;
    }

    if (toolState.stage == 0) {
        if (ImGui::IsKeyPressed(ImGuiKey_Equal, true) || ImGui::IsKeyPressed(ImGuiKey_KeypadAdd, true) || (viewHovered && io.MouseWheel > 0.0f))
            toolState.cuts = std::min(toolState.cuts + 1, 32);
        if (ImGui::IsKeyPressed(ImGuiKey_Minus, true) || ImGui::IsKeyPressed(ImGuiKey_KeypadSubtract, true) || (viewHovered && io.MouseWheel < 0.0f))
            toolState.cuts = std::max(toolState.cuts - 1, 1);

        int hoverMesh = -1, hoverEdge = -1;
        if (viewHovered) FindElement(sceneRenderer, view, mouse, EditSelect_Edge, hoverMesh, hoverEdge);
        bool haveRing = false;
        if (hoverMesh >= 0) {
            int a = editMeshes[hoverMesh].edges[hoverEdge].x, b = editMeshes[hoverMesh].edges[hoverEdge].y;
            if (hoverMesh != toolState.ringMesh || a != toolState.ringA || b != toolState.ringB) {
                SyncPositions(hoverMesh);
                toolState.ringMesh = hoverMesh;
                toolState.ringA = a;
                toolState.ringB = b;
                if (!PolyFindRing(polyMeshes[hoverMesh], a, b, toolState.ring)) toolState.ring = EdgeRing();
            }
            haveRing = !toolState.ring.quads.empty();
        }

        if (haveRing) {
            const PolyMesh& poly = polyMeshes[toolState.ringMesh];
            const EdgeRing& ring = toolState.ring;
            size_t count = ring.sideA.size();
            for (int k = 1; k <= toolState.cuts; k++) {
                float t = k / (float)(toolState.cuts + 1);
                for (size_t q = 0; q < ring.quads.size(); q++) {
                    size_t i = q, j = (q + 1) % count;
                    ImVec2 p0, p1;
                    if (!Project(mvp, glm::mix(poly.positions[ring.sideA[i]], poly.positions[ring.sideB[i]], t), view.min, view.size, p0)) continue;
                    if (!Project(mvp, glm::mix(poly.positions[ring.sideA[j]], poly.positions[ring.sideB[j]], t), view.min, view.size, p1)) continue;
                    drawList->AddLine(p0, p1, IM_COL32(0, 0, 0, 140), 4.0f);
                    drawList->AddLine(p0, p1, IM_COL32(255, 220, 90, 255), 2.0f);
                }
            }
        }
        snprintf(readout, readoutSize, "Loop Cut   %d cut%s   Point at an edge, then click   Scroll or + - for more cuts", toolState.cuts, toolState.cuts == 1 ? "" : "s");

        if (confirm && haveRing) {
            for (size_t m = 0; m < editMeshes.size(); m++) SyncPositions((int)m);
            toolState.base.assign(polyMeshes.begin(), polyMeshes.end());
            toolState.original.clear();
            for (size_t m = 0; m < editMeshes.size(); m++) toolState.original.push_back(CurrentPolySelection((int)m));
            if (toolState.cuts > 1) {
                ApplyLoopCut(*obj, 0.5f);
                FinishTool();
                return true;
            }
            const PolyMesh& poly = polyMeshes[toolState.ringMesh];
            ImVec2 a, b;
            if (Project(mvp, poly.positions[toolState.ring.sideA[0]], view.min, view.size, a) &&
                Project(mvp, poly.positions[toolState.ring.sideB[0]], view.min, view.size, b)) {
                toolState.slideDir = ImVec2(b.x - a.x, b.y - a.y);
                toolState.slideLengthSq = std::max(toolState.slideDir.x * toolState.slideDir.x + toolState.slideDir.y * toolState.slideDir.y, 1.0f);
            }
            toolState.startMouse = mouse;
            toolState.stage = 1;
            toolState.amount = -1.0f;
        }
        return confirm;
    }

    float along = ((mouse.x - toolState.startMouse.x) * toolState.slideDir.x + (mouse.y - toolState.startMouse.y) * toolState.slideDir.y) / toolState.slideLengthSq;
    float slide = 0.5f + along;
    if (io.KeyCtrl) slide = roundf(slide * 20.0f) / 20.0f;
    slide = slide < 0.02f ? 0.02f : (slide > 0.98f ? 0.98f : slide);
    if (slide != toolState.amount) {
        toolState.amount = slide;
        ApplyLoopCut(*obj, slide);
    }
    snprintf(readout, readoutSize, "Slide the cut   %.0f%%", slide * 100.0f);
    if (confirm) FinishTool();
    return confirm;
}

void EditSetLastView(const EditView& view) {
    lastView = view;
    haveView = true;
}

int EditMeshCount() {
    return active ? (int)polyMeshes.size() : 0;
}

const PolyMesh* EditPoly(int m) {
    if (m < 0 || m >= (int)polyMeshes.size()) return nullptr;
    SyncPositions(m);
    return &polyMeshes[m];
}

bool EditIsSeam(int m, int a, int b) {
    return m >= 0 && m < (int)seams.size() && seams[m].count(MakeEdge(a, b)) > 0;
}

bool EditPolyVertexSelected(int m, int v) {
    if (m < 0 || m >= (int)selection.size()) return false;
    if (selectMode == EditSelect_Vertex) return selection[m].count(v) > 0;
    return SelectedVertices(m).count(v) > 0;
}

bool EditPolyFaceSelected(int m, int f) {
    if (m < 0 || m >= (int)polyMeshes.size() || f < 0 || f >= (int)polyMeshes[m].faces.size()) return false;
    const PolyFace& face = polyMeshes[m].faces[f];
    if (selectMode == EditSelect_Face) return selection[m].count(face.source) > 0;
    std::set<int> verts = SelectedVertices(m);
    for (int v : face.verts) if (!verts.count(v)) return false;
    return !face.verts.empty();
}

static int EditEdgeIndex(int m, int a, int b) {
    const EditMesh& mesh = editMeshes[m];
    for (size_t e = 0; e < mesh.edges.size(); e++)
        if (MakeEdge(mesh.edges[e].x, mesh.edges[e].y) == MakeEdge(a, b)) return (int)e;
    return -1;
}

static void Toggle(std::set<int>& set, int id, bool additive) {
    if (!additive) {
        for (auto& other : selection) other.clear();
        set.insert(id);
    } else if (set.count(id)) {
        set.erase(id);
    } else {
        set.insert(id);
    }
}

void EditSelectPolyElement(int m, int f, int corner, int cornerB, bool additive) {
    Touch();
    if (m < 0 || m >= (int)polyMeshes.size()) return;
    if (f < 0) {
        if (!additive) for (auto& set : selection) set.clear();
        return;
    }
    const PolyFace& face = polyMeshes[m].faces[f];
    if (selectMode == EditSelect_Vertex && VertexLocked(m, face.verts[corner])) {
        SayLocked(m, face.verts[corner], EditSelect_Vertex);
        return;
    }
    if (selectMode == EditSelect_Vertex) {
        Toggle(selection[m], face.verts[corner], additive);
    } else if (selectMode == EditSelect_Edge) {
        int e = EditEdgeIndex(m, face.verts[corner], face.verts[cornerB]);
        if (e >= 0) Toggle(selection[m], e, additive);
    } else if (face.source >= 0) {
        Toggle(selection[m], face.source, additive);
    }
}

void EditSelectPolyBox(const std::function<bool(int, int, int)>& cornerInside, EditBoxMode mode) {
    Touch();
    for (size_t m = 0; m < polyMeshes.size(); m++) {
        const PolyMesh& poly = polyMeshes[m];
        std::set<int> hits;
        std::map<int, int> vertexInside;
        for (size_t f = 0; f < poly.faces.size(); f++) {
            const PolyFace& face = poly.faces[f];
            bool all = true;
            for (size_t c = 0; c < face.verts.size(); c++) {
                bool in = cornerInside((int)m, (int)f, (int)c);
                if (in) vertexInside[face.verts[c]] = 1;
                else all = false;
            }
            if (selectMode == EditSelect_Face && all && face.source >= 0) hits.insert(face.source);
            if (selectMode == EditSelect_Edge) {
                for (size_t c = 0; c < face.verts.size(); c++) {
                    size_t d = (c + 1) % face.verts.size();
                    if (cornerInside((int)m, (int)f, (int)c) && cornerInside((int)m, (int)f, (int)d)) {
                        int e = EditEdgeIndex((int)m, face.verts[c], face.verts[d]);
                        if (e >= 0) hits.insert(e);
                    }
                }
            }
        }
        if (selectMode == EditSelect_Vertex) for (const auto& entry : vertexInside) hits.insert(entry.first);
        DropLocked(hits, (int)m, selectMode);
        if (mode == EditBox_Replace) selection[m] = hits;
        else if (mode == EditBox_Add) selection[m].insert(hits.begin(), hits.end());
        else for (int h : hits) selection[m].erase(h);
    }
}

static std::vector<UVCorner> UVCornersOf(int m) {
    std::vector<UVCorner> out;
    const PolyMesh& poly = polyMeshes[m];
    if (selectMode == EditSelect_Face) {
        for (size_t f = 0; f < poly.faces.size(); f++)
            if (selection[m].count(poly.faces[f].source))
                for (size_t c = 0; c < poly.faces[f].verts.size(); c++) out.push_back(UVCorner((int)f, (int)c));
        return out;
    }
    std::set<int> verts = SelectedVertices(m);
    for (size_t f = 0; f < poly.faces.size(); f++)
        for (size_t c = 0; c < poly.faces[f].verts.size(); c++)
            if (verts.count(poly.faces[f].verts[c])) out.push_back(UVCorner((int)f, (int)c));
    return out;
}

static void RebuildKeeping(SceneObject& obj, int m) {
    Rebuild(obj, m, CurrentPolySelection(m), true);
}

bool EditUVSessionActive() {
    return uvSession.active;
}

bool EditUVBegin(glm::vec2& outCenter) {
    SceneObject* obj = nullptr;
    if (!active) return false;
    for (size_t m = 0; m < polyMeshes.size(); m++) SyncPositions((int)m);
    uvSession = UVSession();
    glm::vec2 lo(1e30f), hi(-1e30f);
    bool any = false;
    for (size_t m = 0; m < polyMeshes.size(); m++) {
        std::vector<UVCorner> corners = UVCornersOf((int)m);
        std::vector<glm::vec2> start;
        for (const UVCorner& c : corners) {
            glm::vec2 uv = polyMeshes[m].faces[c.first].uvs[c.second];
            start.push_back(uv);
            lo = glm::min(lo, uv);
            hi = glm::max(hi, uv);
            any = true;
        }
        uvSession.corners.push_back(corners);
        uvSession.start.push_back(start);
    }
    (void)obj;
    if (!any) return false;
    outCenter = (lo + hi) * 0.5f;
    uvSession.active = true;
    return true;
}

void EditUVUpdate(SceneRenderer& sceneRenderer, const std::function<glm::vec2(const glm::vec2&)>& transform) {
    SceneObject* obj = EditObject(sceneRenderer);
    if (!obj || !uvSession.active) return;
    for (size_t m = 0; m < polyMeshes.size() && m < uvSession.corners.size(); m++) {
        if (uvSession.corners[m].empty()) continue;
        for (size_t k = 0; k < uvSession.corners[m].size(); k++) {
            const UVCorner& c = uvSession.corners[m][k];
            polyMeshes[m].faces[c.first].uvs[c.second] = transform(uvSession.start[m][k]);
        }
        RebuildKeeping(*obj, (int)m);
    }
}

void EditUVEnd(SceneRenderer& sceneRenderer, bool cancel) {
    if (cancel) EditUVUpdate(sceneRenderer, [](const glm::vec2& uv) { return uv; });
    uvSession = UVSession();
}

bool EditMarkSeams(SceneRenderer& sceneRenderer, bool mark) {
    Touch();
    if (!active) return false;
    bool any = false;
    for (size_t m = 0; m < polyMeshes.size(); m++) {
        SyncPositions((int)m);
        PolySelection current = CurrentPolySelection((int)m);
        std::set<PolyEdge> edges = current.edges;
        if (selectMode == EditSelect_Face) {
            std::map<PolyEdge, int> count;
            for (int f : current.faces) {
                const std::vector<int>& loop = polyMeshes[m].faces[f].verts;
                for (size_t i = 0; i < loop.size(); i++) count[MakeEdge(loop[i], loop[(i + 1) % loop.size()])]++;
            }
            edges.clear();
            for (const auto& entry : count) if (entry.second == 1) edges.insert(entry.first);
        }
        for (const PolyEdge& e : edges) {
            if (mark) seams[m].insert(e);
            else seams[m].erase(e);
            any = true;
        }
    }
    if (!any) Say("Pick edges first, then mark them as seams.");
    else Say(mark ? "Seams marked. Press Unwrap to lay the shape flat along them." : "Seams cleared. Press Unwrap to apply.");
    (void)sceneRenderer;
    return any;
}

bool EditUnwrapOrProject(SceneRenderer& sceneRenderer, int how) {
    SceneObject* obj = EditObject(sceneRenderer);
    if (!obj) return false;
    bool any = false;
    for (size_t m = 0; m < polyMeshes.size(); m++) {
        SyncPositions((int)m);
        PolySelection current = CurrentPolySelection((int)m);
        bool done;
        if (how < 0) {
            done = PolyUnwrap(polyMeshes[m], current.faces, seams[m]);
        } else if (how == UVProject_View) {
            if (!haveView) continue;
            glm::mat4 mvp = lastView.viewProj * obj->GetModelMatrix();
            done = PolyProject(polyMeshes[m], current.faces, UVProject_View, &mvp);
        } else {
            done = PolyProject(polyMeshes[m], current.faces, (UVProjection)how, nullptr);
        }
        if (!done) continue;
        RebuildKeeping(*obj, (int)m);
        any = true;
    }
    if (any && how >= 0) {
        for (size_t m = 0; m < polyMeshes.size(); m++) seams[m] = PolyUVSeams(polyMeshes[m]);
    }
    if (any) Say(GetEditSelectionCount() == 0 ? "Laid out the whole shape. Pick faces first to lay out only part of it." : "Laid out the picked faces.");
    return any;
}

bool EditUVArrange(SceneRenderer& sceneRenderer, int action) {
    SceneObject* obj = EditObject(sceneRenderer);
    if (!obj) return false;
    bool any = false;
    for (size_t m = 0; m < polyMeshes.size(); m++) {
        SyncPositions((int)m);
        PolyMesh& poly = polyMeshes[m];
        std::vector<UVCorner> corners = UVCornersOf((int)m);
        if (action == 2) {
            std::set<int> faces = CurrentPolySelection((int)m).faces;
            if (!PolyPackIslands(poly, faces)) continue;
            RebuildKeeping(*obj, (int)m);
            any = true;
            continue;
        }
        glm::vec2 lo, hi;
        if (!PolyCornerBounds(poly, corners, lo, hi)) continue;
        glm::vec2 mid = (lo + hi) * 0.5f;
        float span = std::max(std::max(hi.x - lo.x, hi.y - lo.y), 1e-20f);
        std::function<glm::vec2(const glm::vec2&)> fn;
        if (action == 0) fn = [&](const glm::vec2& uv) { return (uv - mid) / span * 0.98f + glm::vec2(0.5f); };
        else if (action == 1) fn = [&](const glm::vec2& uv) { return uv - mid + glm::vec2(0.5f); };
        else if (action == 3) fn = [&](const glm::vec2& uv) { glm::vec2 d = uv - mid; return mid + glm::vec2(-d.y, d.x); };
        else if (action == 4) fn = [&](const glm::vec2& uv) { return glm::vec2(2.0f * mid.x - uv.x, uv.y); };
        else fn = [&](const glm::vec2& uv) { return glm::vec2(uv.x, 2.0f * mid.y - uv.y); };
        PolyTransformCorners(poly, corners, fn);
        RebuildKeeping(*obj, (int)m);
        any = true;
    }
    if (!any) Say("Pick something first.");
    return any;
}

bool EditSelectIslands() {
    Touch();
    if (!active) return false;
    bool any = false;
    for (size_t m = 0; m < polyMeshes.size(); m++) {
        SyncPositions((int)m);
        PolySelection current = CurrentPolySelection((int)m);
        std::set<int> seed = current.faces;
        if (seed.empty())
            for (size_t f = 0; f < polyMeshes[m].faces.size(); f++)
                for (int v : polyMeshes[m].faces[f].verts)
                    if (current.verts.count(v)) { seed.insert((int)f); break; }
        if (seed.empty()) continue;
        int count = 0;
        std::vector<int> island = PolyUVIslands(polyMeshes[m], count);
        std::set<int> wanted;
        for (int f : seed) wanted.insert(island[f]);
        PolySelection grown;
        for (size_t f = 0; f < polyMeshes[m].faces.size(); f++) {
            if (!wanted.count(island[f])) continue;
            grown.faces.insert((int)f);
            grown.verts.insert(polyMeshes[m].faces[f].verts.begin(), polyMeshes[m].faces[f].verts.end());
        }
        ApplyPolySelection((int)m, grown);
        any = true;
    }
    if (!any) Say("Pick part of a piece first.");
    return any;
}

unsigned int EditOverlayStamp() {
    return overlayStamp * 2654435761u + editRevision;
}

void EditFillOverlay(EditOverlayData& out) {
    out = EditOverlayData();
    if (!active) return;
    out.seeThrough = GetUISettings().editShowHidden;
    out.vertexMode = selectMode == EditSelect_Vertex;
    out.faceMode = selectMode == EditSelect_Face;
    auto push = [](std::vector<float>& into, const glm::vec3& p, float flag) {
        into.push_back(p.x);
        into.push_back(p.y);
        into.push_back(p.z);
        into.push_back(flag);
    };
    for (size_t m = 0; m < editMeshes.size(); m++) {
        const EditMesh& mesh = editMeshes[m];
        std::vector<char> picked(mesh.positions.size(), 0);
        for (int v : SelectedVertices((int)m)) if (v >= 0 && v < (int)picked.size()) picked[v] = 1;
        if (out.vertexMode)
            for (size_t v = 0; v < mesh.positions.size(); v++) push(picked[v] ? out.hotPoints : out.points, mesh.positions[v], picked[v] ? 1.0f : 0.0f);
        for (size_t e = 0; e < mesh.edges.size(); e++) {
            int a = mesh.edges[e].x, b = mesh.edges[e].y;
            bool on = selectMode == EditSelect_Edge ? selection[m].count((int)e) > 0 : (picked[a] && picked[b]);
            bool seam = m < seams.size() && seams[m].count(MakeEdge(a, b)) > 0;
            float flag = on ? 1.0f : (seam ? 2.0f : 0.0f);
            std::vector<float>& into = (on || seam) ? out.hotLines : out.lines;
            push(into, mesh.positions[a], flag);
            push(into, mesh.positions[b], flag);
        }
        if (out.faceMode) {
            for (size_t f = 0; f < mesh.faces.size(); f++) {
                bool on = selection[m].count((int)f) > 0;
                glm::vec3 center = FaceCenter(mesh, (int)f);
                push(on ? out.hotPoints : out.points, center, on ? 1.0f : 0.0f);
                if (!on) continue;
                for (int t : mesh.faces[f].triangles) {
                    const glm::ivec3& tri = mesh.triangles[t];
                    push(out.fill, mesh.positions[tri.x], 3.0f);
                    push(out.fill, mesh.positions[tri.y], 3.0f);
                    push(out.fill, mesh.positions[tri.z], 3.0f);
                }
            }
        }
    }
}

void EditSetForeignLocks(const std::vector<ForeignLock>& locks) {
    foreignOwners.clear();
    foreignVertex.assign(editMeshes.size(), std::vector<int>());
    if (!active) return;
    for (const ForeignLock& lock : locks) {
        if (lock.mesh < 0 || lock.mesh >= (int)editMeshes.size()) continue;
        const EditMesh& mesh = editMeshes[lock.mesh];
        std::vector<int>& owners = foreignVertex[lock.mesh];
        if (owners.empty()) owners.assign(mesh.positions.size(), -1);
        int owner = -1;
        for (size_t k = 0; k < foreignOwners.size(); k++) if (foreignOwners[k].name == lock.name) owner = (int)k;
        if (owner < 0) {
            owner = (int)foreignOwners.size();
            foreignOwners.push_back({ lock.name, lock.color });
        }
        for (int r : lock.render) {
            if (r < 0 || r >= (int)mesh.renderToUnique.size()) continue;
            int v = mesh.renderToUnique[r];
            if (v >= 0 && v < (int)owners.size()) owners[v] = owner;
        }
    }
}

void EditSay(const std::string& text) {
    Say(text.c_str());
}

int EditDropLockedPicks() {
    int dropped = 0;
    for (size_t m = 0; m < selection.size(); m++) {
        size_t before = selection[m].size();
        DropLocked(selection[m], (int)m, selectMode);
        dropped += (int)(before - selection[m].size());
    }
    if (dropped) Touch();
    return dropped;
}

void EditMyLockedRender(std::vector<std::pair<int, int>>& out) {
    out.clear();
    if (!active) return;
    for (size_t m = 0; m < editMeshes.size(); m++) {
        std::set<int> verts = SelectedVertices((int)m);
        if (verts.empty()) continue;
        const EditMesh& mesh = editMeshes[m];
        std::vector<int> firstRender(mesh.positions.size(), -1);
        for (size_t r = 0; r < mesh.renderToUnique.size(); r++) {
            int v = mesh.renderToUnique[r];
            if (v >= 0 && v < (int)firstRender.size() && firstRender[v] < 0) firstRender[v] = (int)r;
        }
        for (int v : verts)
            if (v < (int)firstRender.size() && firstRender[v] >= 0) out.push_back({ (int)m, firstRender[v] });
    }
}

// Points moved by others
void EditExternalVertices(int m, const std::vector<std::pair<unsigned int, Vertex>>& patch) {
    if (!active || m < 0 || m >= (int)editMeshes.size()) return;
    EditMesh& mesh = editMeshes[m];
    for (const auto& entry : patch) {
        if (entry.first >= mesh.renderToUnique.size()) continue;
        int v = mesh.renderToUnique[entry.first];
        if (v >= 0 && v < (int)mesh.positions.size()) mesh.positions[v] = entry.second.position;
    }
    editRevision++;
}
