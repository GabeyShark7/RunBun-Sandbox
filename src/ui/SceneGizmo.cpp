#include "core/Profiler.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "ui/SceneGizmo.h"
#include "ui/Workspaces.h"
#include "ui/TinkerHandles.h"
#include "render/SceneRenderer.h"
#include "core/UISettings.h"
#include "ui/EditMode.h"
#include "ui/SceneWindow.h"
#include "imgui/imgui_internal.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

enum TransformMode {
    Transform_None,
    Transform_Grab,
    Transform_Scale,
    Transform_Rotate
};

// Set by panel buttons
static TransformMode pendingRequest = Transform_None;
static bool pendingExtrude = false;
static bool sceneTab = false;

// Tab belongs to the Scene
void TakeSceneTab() {
    sceneTab = false;
    ImGuiContext& g = *GImGui;
    if (!IsSceneHovered() || g.IO.WantTextInput) return;
    for (int i = 0; i < g.InputEventsQueue.Size;) {
        ImGuiInputEvent& e = g.InputEventsQueue[i];
        if (e.Type == ImGuiInputEventType_Key && e.Key.Key == ImGuiKey_Tab) {
            if (e.Key.Down) sceneTab = true;
            g.InputEventsQueue.erase(g.InputEventsQueue.Data + i);
        } else {
            i++;
        }
    }
}

void RequestSceneExtrude() {
    pendingExtrude = true;
}

void RequestSceneTransform(char key) {
    pendingRequest = key == 'G' ? Transform_Grab : (key == 'S' ? Transform_Scale : (key == 'R' ? Transform_Rotate : Transform_None));
}

struct ArrowDrag {
    int axis = -1;
    ImVec2 startMouse;
    std::vector<int> indices;
    std::vector<glm::vec3> startPositions;
    ImVec2 screenDir;
    float worldPerPixel = 0.0f;
};

struct ModalTransform {
    TransformMode mode = Transform_None;
    int axis = -1;
    std::vector<int> indices;
    int editIndex = -1;
    bool editTarget = false;
    glm::vec3 centerWorld;
    std::vector<glm::vec3> startPositions;
    std::vector<glm::vec3> startRotations;
    std::vector<glm::vec3> startScales;
    ImVec2 startMouse;
    ImVec2 center;
    glm::vec3 startHit;
    float lastAngle = 0.0f;
    float totalAngle = 0.0f;
    glm::vec3 customAxis = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 snapAnchor;
    std::vector<glm::vec3> snapPoints;
    bool snapped = false;
    glm::vec3 snapTarget;
};

struct BoxDrag {
    bool pending = false;
    bool dragging = false;
    ImVec2 start;
};
static BoxDrag box;

static ArrowDrag arrowDrag;
static ModalTransform modal;
static glm::vec3 arrowCenter = glm::vec3(0.0f);

static const glm::vec3 axisVectors[3] = { glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1) };
static const ImU32 axisColors[3] = { IM_COL32(232, 72, 85, 255), IM_COL32(110, 200, 90, 255), IM_COL32(70, 140, 240, 255) };
static const ImU32 axisHotColors[3] = { IM_COL32(255, 150, 160, 255), IM_COL32(180, 245, 160, 255), IM_COL32(150, 200, 255, 255) };
static const char* axisLabels[3] = { "X", "Y", "Z" };

bool IsSceneTransformActive() {
    return modal.mode != Transform_None || arrowDrag.axis >= 0 || GetEditTool() != EditTool_None || EditUVSessionActive() || TinkerHandleActive();
}

static glm::vec3 ModalAxis() {
    return modal.axis == 3 ? modal.customAxis : axisVectors[modal.axis];
}

static bool ProjectToScreen(const glm::mat4& viewProj, const glm::vec3& point, ImVec2 viewMin, ImVec2 viewSize, ImVec2& out) {
    glm::vec4 clip = viewProj * glm::vec4(point, 1.0f);
    if (clip.w <= 0.0001f) return false;
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    out = ImVec2(viewMin.x + (ndc.x * 0.5f + 0.5f) * viewSize.x, viewMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewSize.y);
    return true;
}

static void ScreenRay(const glm::mat4& viewProj, ImVec2 point, ImVec2 viewMin, ImVec2 viewSize, glm::vec3& origin, glm::vec3& dir) {
    float ndcX = (point.x - viewMin.x) / viewSize.x * 2.0f - 1.0f;
    float ndcY = 1.0f - (point.y - viewMin.y) / viewSize.y * 2.0f;
    glm::mat4 inverseViewProj = glm::inverse(viewProj);
    glm::vec4 nearPoint = inverseViewProj * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 farPoint = inverseViewProj * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    origin = glm::vec3(nearPoint) / nearPoint.w;
    dir = glm::normalize(glm::vec3(farPoint) / farPoint.w - origin);
}

static bool RayPlane(const glm::vec3& origin, const glm::vec3& dir, const glm::vec3& planePoint, const glm::vec3& normal, glm::vec3& hit) {
    float denom = glm::dot(dir, normal);
    if (std::fabs(denom) < 1e-6f) return false;
    float t = glm::dot(planePoint - origin, normal) / denom;
    hit = origin + dir * t;
    return true;
}

static float Length(ImVec2 v) {
    return sqrtf(v.x * v.x + v.y * v.y);
}

static float DistanceToSegment(ImVec2 p, ImVec2 a, ImVec2 b) {
    ImVec2 ab = ImVec2(b.x - a.x, b.y - a.y);
    float lengthSq = ab.x * ab.x + ab.y * ab.y;
    float t = lengthSq > 0.0f ? ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / lengthSq : 0.0f;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return Length(ImVec2(a.x + ab.x * t - p.x, a.y + ab.y * t - p.y));
}

static float SnapValue(float value, float step) {
    return step > 0.0f ? roundf(value / step) * step : value;
}

static float WrapDegrees(float degrees) {
    while (degrees > 180.0f) degrees -= 360.0f;
    while (degrees < -180.0f) degrees += 360.0f;
    return degrees;
}

static void DrawArrow(ImDrawList* drawList, ImVec2 from, ImVec2 to, ImU32 color, float thickness) {
    ImVec2 dir = ImVec2(to.x - from.x, to.y - from.y);
    float length = Length(dir);
    if (length < 1.0f) return;
    dir = ImVec2(dir.x / length, dir.y / length);
    ImVec2 side = ImVec2(-dir.y, dir.x);

    float headLength = 14.0f;
    float headWidth = 6.0f;
    ImVec2 headBase = ImVec2(to.x - dir.x * headLength, to.y - dir.y * headLength);

    drawList->AddLine(from, headBase, color, thickness);
    drawList->AddTriangleFilled(to,
        ImVec2(headBase.x + side.x * headWidth, headBase.y + side.y * headWidth),
        ImVec2(headBase.x - side.x * headWidth, headBase.y - side.y * headWidth), color);
}

static void FormatLength(float meters, char* out, size_t size) {
    float mm = meters * 1000.0f;
    if (mm >= 1000.0f) snprintf(out, size, "%.2f m", meters);
    else if (mm >= 10.0f) snprintf(out, size, "%.0f mm", mm);
    else snprintf(out, size, "%.1f mm", mm);
}

static void DrawLabel(ImDrawList* drawList, ImVec2 at, ImU32 color, const char* text) {
    ImVec2 size = ImGui::CalcTextSize(text);
    ImVec2 min = ImVec2(at.x - size.x * 0.5f - 6.0f, at.y - size.y * 0.5f - 3.0f);
    ImVec2 max = ImVec2(at.x + size.x * 0.5f + 6.0f, at.y + size.y * 0.5f + 3.0f);
    drawList->AddRectFilled(min, max, IM_COL32(15, 15, 20, 215), 5.0f);
    drawList->AddText(ImVec2(min.x + 6.0f, min.y + 3.0f), color, text);
}

// Box with W D H labels
static void DrawDimensions(ImDrawList* drawList, const glm::mat4& viewProj, ImVec2 viewMin, ImVec2 viewSize, const glm::vec3& eye, const glm::vec3& boundsMin, const glm::vec3& boundsMax) {
    ImVec2 points[8];
    glm::vec3 corners[8];
    for (int i = 0; i < 8; i++) {
        corners[i] = glm::vec3((i & 1) ? boundsMax.x : boundsMin.x, (i & 2) ? boundsMax.y : boundsMin.y, (i & 4) ? boundsMax.z : boundsMin.z);
        if (!ProjectToScreen(viewProj, corners[i], viewMin, viewSize, points[i])) return;
    }
    for (int i = 0; i < 8; i++)
        for (int bit = 1; bit <= 4; bit <<= 1)
            if (!(i & bit)) drawList->AddLine(points[i], points[i | bit], IM_COL32(235, 235, 240, 90), 1.0f);

    glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    int nearY = eye.y > center.y ? 2 : 0;
    int nearX = eye.x > center.x ? 1 : 0;
    // Edges facing the camera
    int edges[3][2] = { { nearY, nearY | 1 }, { nearX, nearX | 2 }, { nearX | nearY, nearX | nearY | 4 } };
    glm::vec3 size = boundsMax - boundsMin;
    for (int axis = 0; axis < 3; axis++) {
        ImVec2 a = points[edges[axis][0]], b = points[edges[axis][1]];
        drawList->AddLine(a, b, axisColors[axis], 2.0f);
        char text[48], length[32];
        FormatLength(size[axis], length, sizeof(length));
        snprintf(text, sizeof(text), "%s %s", axisLabels[axis], length);
        DrawLabel(drawList, ImVec2((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f), axisHotColors[axis], text);
    }
}

static void DrawDashedLine(ImDrawList* drawList, ImVec2 from, ImVec2 to, ImU32 color) {
    ImVec2 dir = ImVec2(to.x - from.x, to.y - from.y);
    float length = Length(dir);
    if (length < 1.0f) return;
    dir = ImVec2(dir.x / length, dir.y / length);
    for (float t = 0.0f; t < length; t += 10.0f) {
        float end = t + 5.0f < length ? t + 5.0f : length;
        drawList->AddLine(ImVec2(from.x + dir.x * t, from.y + dir.y * t), ImVec2(from.x + dir.x * end, from.y + dir.y * end), color, 1.5f);
    }
}

static void RestoreModalStart(std::vector<SceneObject>& objects) {
    for (size_t k = 0; k < modal.indices.size(); k++) {
        SceneObject& obj = objects[modal.indices[k]];
        obj.position = modal.startPositions[k];
        obj.rotation = modal.startRotations[k];
        obj.scale = modal.startScales[k];
    }
}

static glm::mat4 RotationMatrix(const glm::vec3& degrees) {
    glm::mat4 matrix = glm::rotate(glm::mat4(1.0f), glm::radians(degrees.x), glm::vec3(1, 0, 0));
    matrix = glm::rotate(matrix, glm::radians(degrees.y), glm::vec3(0, 1, 0));
    return glm::rotate(matrix, glm::radians(degrees.z), glm::vec3(0, 0, 1));
}

static void CollectSnapPoints(SceneRenderer& sceneRenderer, bool editTarget, ImVec2 mouse, const glm::mat4& viewProj, ImVec2 viewMin, ImVec2 viewSize) {
    modal.snapPoints.clear();
    modal.snapAnchor = modal.centerWorld;
    auto& objects = sceneRenderer.GetObjects();
    if (editTarget) EditCollectFixedPoints(sceneRenderer, modal.snapPoints);
    float bestAnchor = 30.0f;
    for (int i = 0; i < (int)objects.size(); i++) {
        bool moving = editTarget ? i == sceneRenderer.selectedIndex : sceneRenderer.IsSelected(i);
        if (editTarget && moving) continue;
        glm::mat4 model = objects[i].GetModelMatrix();
        for (Mesh& mesh : objects[i].model.GetMeshes()) {
            for (const Vertex& vertex : mesh.GetVertices()) {
                glm::vec3 world = glm::vec3(model * glm::vec4(vertex.position, 1.0f));
                if (!moving) {
                    if (modal.snapPoints.size() < 400000) modal.snapPoints.push_back(world);
                    continue;
                }
                ImVec2 screen;
                if (!ProjectToScreen(viewProj, world, viewMin, viewSize, screen)) continue;
                float d = Length(ImVec2(screen.x - mouse.x, screen.y - mouse.y));
                if (d < bestAnchor) { bestAnchor = d; modal.snapAnchor = world; }
            }
        }
    }
}

static bool FindSnapPoint(const glm::mat4& viewProj, ImVec2 viewMin, ImVec2 viewSize, ImVec2 mouse, glm::vec3& out) {
    float best = 18.0f;
    bool found = false;
    for (const glm::vec3& p : modal.snapPoints) {
        ImVec2 screen;
        if (!ProjectToScreen(viewProj, p, viewMin, viewSize, screen)) continue;
        float d = Length(ImVec2(screen.x - mouse.x, screen.y - mouse.y));
        if (d < best) { best = d; out = p; found = true; }
    }
    return found;
}

static void BeginModal(TransformMode mode, SceneRenderer& sceneRenderer, bool editTarget, const glm::vec3& centerWorld, const glm::mat4& viewProj, const Camera& camera, ImVec2 viewMin, ImVec2 viewSize, ImVec2 mouse) {
    auto& objects = sceneRenderer.GetObjects();
    modal.mode = mode;
    modal.axis = -1;
    modal.indices = editTarget ? std::vector<int>() : sceneRenderer.GetSelection();
    modal.editIndex = editTarget ? sceneRenderer.selectedIndex : -1;
    modal.editTarget = editTarget;
    modal.centerWorld = centerWorld;
    modal.startPositions.clear();
    modal.startRotations.clear();
    modal.startScales.clear();
    for (int index : modal.indices) {
        modal.startPositions.push_back(objects[index].position);
        modal.startRotations.push_back(objects[index].rotation);
        modal.startScales.push_back(objects[index].scale);
    }
    modal.startMouse = mouse;
    modal.lastAngle = 0.0f;
    modal.totalAngle = 0.0f;
    if (!ProjectToScreen(viewProj, centerWorld, viewMin, viewSize, modal.center))
        modal.center = ImVec2(viewMin.x + viewSize.x * 0.5f, viewMin.y + viewSize.y * 0.5f);

    glm::vec3 origin, dir;
    ScreenRay(viewProj, mouse, viewMin, viewSize, origin, dir);
    if (!RayPlane(origin, dir, centerWorld, camera.front, modal.startHit)) modal.startHit = centerWorld;
    modal.snapped = false;
    if (mode == Transform_Grab) CollectSnapPoints(sceneRenderer, editTarget, mouse, viewProj, viewMin, viewSize);

    ImVec2 offset = ImVec2(mouse.x - modal.center.x, mouse.y - modal.center.y);
    modal.lastAngle = atan2f(offset.y, offset.x);
}

static void UpdateModal(SceneRenderer& sceneRenderer, const UISettings& settings, const glm::mat4& viewProj, const Camera& camera, ImVec2 viewMin, ImVec2 viewSize, ImVec2 mouse, char* readout, size_t readoutSize) {
    ImGuiIO& io = ImGui::GetIO();
    bool snapping = settings.snapEnabled || io.KeyCtrl;
    glm::vec3 center = modal.centerWorld;
    auto& objects = sceneRenderer.GetObjects();

    if (modal.mode == Transform_Grab) {
        glm::vec3 origin, dir, hit;
        ScreenRay(viewProj, mouse, viewMin, viewSize, origin, dir);
        if (!RayPlane(origin, dir, center, camera.front, hit)) return;

        glm::vec3 delta = hit - modal.startHit;
        if (modal.axis >= 0) delta = ModalAxis() * glm::dot(delta, ModalAxis());
        modal.snapped = false;
        if ((settings.snapToPoints || io.KeyShift) && FindSnapPoint(viewProj, viewMin, viewSize, mouse, modal.snapTarget)) {
            delta = modal.snapTarget - modal.snapAnchor;
            if (modal.axis >= 0) delta = ModalAxis() * glm::dot(delta, ModalAxis());
            modal.snapped = true;
            snapping = false;
        }
        if (snapping) {
            float step = settings.snapUnitMm / 1000.0f;
            delta = glm::vec3(SnapValue(delta.x, step), SnapValue(delta.y, step), SnapValue(delta.z, step));
        }
        if (modal.editTarget) EditApplyTransform(sceneRenderer, [&](const glm::vec3& world) { return world + delta; });
        else for (size_t k = 0; k < modal.indices.size(); k++) objects[modal.indices[k]].position = modal.startPositions[k] + delta;
        snprintf(readout, readoutSize, "Move   X %.1f mm   Y %.1f mm   Z %.1f mm", delta.x * 1000.0f, delta.y * 1000.0f, delta.z * 1000.0f);
    } else if (modal.mode == Transform_Scale) {
        float startDistance = Length(ImVec2(modal.startMouse.x - modal.center.x, modal.startMouse.y - modal.center.y));
        if (startDistance < 4.0f) startDistance = 4.0f;
        float factor = Length(ImVec2(mouse.x - modal.center.x, mouse.y - modal.center.y)) / startDistance;
        if (io.KeyCtrl) factor = SnapValue(factor, 0.1f);
        if (factor < 0.001f) factor = 0.001f;

        glm::vec3 multiplier = glm::vec3(factor);
        if (modal.axis >= 0) {
            multiplier = glm::vec3(1.0f);
            multiplier[modal.axis] = factor;
        }
        if (modal.editTarget) EditApplyTransform(sceneRenderer, [&](const glm::vec3& world) { return center + (world - center) * multiplier; });
        else for (size_t k = 0; k < modal.indices.size(); k++) {
            SceneObject& obj = objects[modal.indices[k]];
            obj.scale = modal.startScales[k] * multiplier;
            // Spread around the middle
            obj.position = center + (modal.startPositions[k] - center) * multiplier;
        }
        snprintf(readout, readoutSize, "Scale   x%.3f", factor);
    } else if (modal.mode == Transform_Rotate) {
        ImVec2 offset = ImVec2(mouse.x - modal.center.x, mouse.y - modal.center.y);
        if (Length(offset) > 2.0f) {
            float angle = atan2f(offset.y, offset.x);
            modal.totalAngle += WrapDegrees(glm::degrees(angle - modal.lastAngle));
            modal.lastAngle = angle;
        }

        float degrees = io.KeyCtrl ? SnapValue(modal.totalAngle, 5.0f) : modal.totalAngle;
        glm::vec3 rotationAxis = modal.axis >= 0 ? ModalAxis() : camera.front;
        float sign = glm::dot(rotationAxis, camera.front) < 0.0f ? -1.0f : 1.0f;

        glm::mat4 delta = glm::rotate(glm::mat4(1.0f), glm::radians(degrees * sign), rotationAxis);
        if (modal.editTarget) {
            glm::mat3 turn = glm::mat3(delta);
            EditApplyTransform(sceneRenderer, [&](const glm::vec3& world) { return center + turn * (world - center); });
        } else {
            glm::mat3 turn = glm::mat3(delta);
            for (size_t k = 0; k < modal.indices.size(); k++) {
                SceneObject& obj = objects[modal.indices[k]];
                glm::mat4 rotation = delta * RotationMatrix(modal.startRotations[k]);
                float x, y, z;
                glm::extractEulerAngleXYZ(rotation, x, y, z);
                obj.rotation = glm::degrees(glm::vec3(x, y, z));
                obj.position = center + turn * (modal.startPositions[k] - center);
            }
        }
        snprintf(readout, readoutSize, "Rotate   %.1f deg", degrees);
    }
}

void DrawSceneGizmo(SceneRenderer& sceneRenderer, UISettings& settings, ImVec2 viewMin, ImVec2 viewSize, bool viewHovered, bool viewActivated, bool viewActive) {
    PROFILE("DrawSceneGizmo");
    if (viewSize.x <= 0.0f || viewSize.y <= 0.0f) return;

    ImGuiIO& io = ImGui::GetIO();
    Camera& camera = sceneRenderer.GetCamera();
    glm::mat4 viewProj = camera.GetProjectionMatrix((int)viewSize.x, (int)viewSize.y) * camera.GetViewMatrix();
    auto& objects = sceneRenderer.GetObjects();
    int selected = sceneRenderer.selectedIndex;
    bool hasSelection = selected >= 0 && selected < (int)objects.size();
    ImVec2 mouse = io.MousePos;
    ImVec2 viewMax = ImVec2(viewMin.x + viewSize.x, viewMin.y + viewSize.y);
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    bool clickConsumed = false;

    bool editAllowed = settings.workMode != WorkMode_Tinker;
    ValidateEditMode(sceneRenderer, editAllowed);
    bool editing = IsEditModeActive();

    bool modalStale = modal.editTarget ? (modal.editIndex != selected) : (modal.indices != sceneRenderer.GetSelection());
    if (modal.mode != Transform_None && (!hasSelection || modalStale || modal.editTarget != editing)) {
        if (modal.editTarget && editing) EditCancelTransform(sceneRenderer);
        modal.mode = Transform_None;
    }

    EditView editView = { viewProj, viewMin, viewSize, camera.position };
    EditSetLastView(editView);
    bool toolIdle = GetEditTool() == EditTool_None;
    bool keysAllowed = viewHovered && arrowDrag.axis < 0 && !io.WantTextInput && !ImGui::IsMouseDown(ImGuiMouseButton_Right) && toolIdle && !box.pending;
    bool plainKeys = keysAllowed && !io.KeyCtrl;
    TransformMode requested = modal.mode == Transform_None ? pendingRequest : Transform_None;
    bool extrudeRequested = modal.mode == Transform_None && toolIdle && pendingExtrude;
    pendingRequest = Transform_None;
    pendingExtrude = false;
    if (modal.mode == Transform_None && plainKeys) {
        if (editAllowed && sceneTab) {
            if (editing && settings.workspace == Workspace_Edit) ToggleEditWorkspace(sceneRenderer);
            else if (editing) ExitEditMode();
            else if (hasSelection) EnterEditMode(sceneRenderer, selected);
            editing = IsEditModeActive();
        } else if (editAllowed && ImGui::IsKeyPressed(ImGuiKey_E, false)) {
            if (editing) extrudeRequested = true;
            else ToggleEditWorkspace(sceneRenderer);
            editing = IsEditModeActive();
        }

        if (editing) {
            if (ImGui::IsKeyPressed(ImGuiKey_1, false)) SetEditSelectMode(EditSelect_Vertex);
            if (ImGui::IsKeyPressed(ImGuiKey_2, false)) SetEditSelectMode(EditSelect_Edge);
            if (ImGui::IsKeyPressed(ImGuiKey_3, false)) SetEditSelectMode(EditSelect_Face);
            if (ImGui::IsKeyPressed(ImGuiKey_A, false) && !io.KeyAlt) EditToggleSelectAll();
            if (ImGui::IsKeyPressed(ImGuiKey_I, false)) EditBeginTool(sceneRenderer, EditTool_Inset, mouse);
            if (ImGui::IsKeyPressed(ImGuiKey_M, false)) EditMerge(sceneRenderer);
            if (ImGui::IsKeyPressed(ImGuiKey_X, false) && !io.KeyShift && !io.KeyAlt) EditDissolve(sceneRenderer);
            if (ImGui::IsKeyPressed(ImGuiKey_F, false) && !io.KeyShift && !io.KeyAlt) EditJoinOrFill(sceneRenderer);
            if (ImGui::IsKeyPressed(ImGuiKey_Z, false) && io.KeyAlt && !io.KeyShift) SetEditSelectThrough(!GetEditSelectThrough());
            if (ImGui::IsKeyPressed(ImGuiKey_H, false) && io.KeyAlt && !io.KeyShift) SetEditShowHidden(!GetUISettings().editShowHidden);
        } else if (ImGui::IsKeyPressed(ImGuiKey_A, false) && !io.KeyAlt) {
            bool all = sceneRenderer.GetSelection().size() == objects.size();
            sceneRenderer.SelectAll(!all);
            selected = sceneRenderer.selectedIndex;
            hasSelection = selected >= 0;
        }

        if (!io.KeyAlt) {
            if (ImGui::IsKeyPressed(ImGuiKey_G, false)) requested = Transform_Grab;
            else if (ImGui::IsKeyPressed(ImGuiKey_S, false)) requested = Transform_Scale;
            else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) requested = Transform_Rotate;
        }
    }
    if (modal.mode == Transform_None && keysAllowed && editing && io.KeyCtrl && !io.KeyShift) {
        if (ImGui::IsKeyPressed(ImGuiKey_B, false)) EditBeginTool(sceneRenderer, EditTool_Bevel, mouse);
        if (ImGui::IsKeyPressed(ImGuiKey_R, false)) EditBeginTool(sceneRenderer, EditTool_LoopCut, mouse);
    }

    if (extrudeRequested && editing) {
        glm::vec3 direction;
        bool alongNormal = false;
        glm::vec3 center;
        if (EditExtrude(sceneRenderer, direction, alongNormal) && EditBeginTransform(sceneRenderer, center)) {
            BeginModal(Transform_Grab, sceneRenderer, true, center, viewProj, camera, viewMin, viewSize, mouse);
            if (alongNormal) {
                modal.axis = 3;
                modal.customAxis = direction;
            }
        }
        requested = Transform_None;
    }

    if (requested != Transform_None && hasSelection) {
        if (editing) {
            glm::vec3 center;
            if (EditBeginTransform(sceneRenderer, center))
                BeginModal(requested, sceneRenderer, true, center, viewProj, camera, viewMin, viewSize, mouse);
        } else {
            BeginModal(requested, sceneRenderer, false, sceneRenderer.GetSelectionCenter(), viewProj, camera, viewMin, viewSize, mouse);
        }
    }

    char readout[160] = "";
    if (editing && GetEditTool() != EditTool_None) {
        drawList->PushClipRect(viewMin, viewMax, true);
        if (EditUpdateTool(sceneRenderer, drawList, editView, mouse, viewHovered, readout, sizeof(readout))) clickConsumed = true;
        drawList->PopClipRect();
    }
    if (modal.mode != Transform_None) {
        for (int i = 0; i < 3; i++) {
            if (ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_X + i), false)) modal.axis = (modal.axis == i) ? -1 : i;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            if (modal.editTarget) EditCancelTransform(sceneRenderer);
            else RestoreModalStart(objects);
            modal.mode = Transform_None;
            clickConsumed = true;
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
            UpdateModal(sceneRenderer, settings, viewProj, camera, viewMin, viewSize, mouse, readout, sizeof(readout));
            modal.mode = Transform_None;
            clickConsumed = true;
        } else {
            UpdateModal(sceneRenderer, settings, viewProj, camera, viewMin, viewSize, mouse, readout, sizeof(readout));
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        }
    }

    int hoveredAxis = -1;
    ImVec2 origin;
    ImVec2 tips[3];
    bool tipVisible[3] = { false, false, false };
    float axisLength = 0.0f;
    bool arrowsVisible = false;

    if (hasSelection && settings.showTransformGizmo && modal.mode == Transform_None && !editing) {
        glm::vec3 position = arrowDrag.axis >= 0 ? arrowCenter : sceneRenderer.GetSelectionCenter();
        axisLength = glm::length(position - camera.position) * 0.16f;

        if (ProjectToScreen(viewProj, position, viewMin, viewSize, origin)) {
            arrowsVisible = true;
            float bestDistance = 9.0f;
            for (int i = 0; i < 3; i++) {
                tipVisible[i] = ProjectToScreen(viewProj, position + axisVectors[i] * axisLength, viewMin, viewSize, tips[i]);
                if (!tipVisible[i] || arrowDrag.axis >= 0 || !viewHovered) continue;
                if (Length(ImVec2(tips[i].x - origin.x, tips[i].y - origin.y)) < 6.0f) continue;

                float distance = DistanceToSegment(mouse, origin, tips[i]);
                if (distance < bestDistance) {
                    bestDistance = distance;
                    hoveredAxis = i;
                }
            }
        }
    }

    if (!editing) box = BoxDrag();
    if (viewActivated && !clickConsumed && modal.mode == Transform_None && editing && GetEditTool() == EditTool_None) {
        box.pending = true;
        box.dragging = false;
        box.start = mouse;
    } else if (viewActivated && !clickConsumed && modal.mode == Transform_None) {
        if (hoveredAxis >= 0) {
            ImVec2 dir = ImVec2(tips[hoveredAxis].x - origin.x, tips[hoveredAxis].y - origin.y);
            float screenLength = Length(dir);
            arrowDrag.axis = hoveredAxis;
            arrowDrag.startMouse = mouse;
            arrowDrag.indices = sceneRenderer.GetSelection();
            arrowDrag.startPositions.clear();
            for (int index : arrowDrag.indices) arrowDrag.startPositions.push_back(objects[index].position);
            arrowCenter = sceneRenderer.GetSelectionCenter();
            arrowDrag.screenDir = ImVec2(dir.x / screenLength, dir.y / screenLength);
            arrowDrag.worldPerPixel = axisLength / screenLength;
        } else {
            glm::vec3 rayOrigin, rayDir;
            ScreenRay(viewProj, mouse, viewMin, viewSize, rayOrigin, rayDir);
            int picked = sceneRenderer.PickObject(rayOrigin, rayDir);
            if (io.KeyShift) sceneRenderer.Select(picked, true);
            else sceneRenderer.Select(picked, false);
        }
    }

    if (box.pending && editing) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (Length(ImVec2(mouse.x - box.start.x, mouse.y - box.start.y)) > 5.0f) box.dragging = true;
        } else {
            if (box.dragging) EditBoxSelect(sceneRenderer, editView, box.start, mouse, io.KeyCtrl ? EditBox_Remove : (io.KeyShift ? EditBox_Add : EditBox_Replace));
            else EditPick(sceneRenderer, editView, box.start, io.KeyShift);
            box = BoxDrag();
        }
    }

    if (arrowDrag.axis >= 0) {
        if (viewActive && hasSelection && arrowDrag.indices == sceneRenderer.GetSelection()) {
            float along = (mouse.x - arrowDrag.startMouse.x) * arrowDrag.screenDir.x + (mouse.y - arrowDrag.startMouse.y) * arrowDrag.screenDir.y;
            float distance = along * arrowDrag.worldPerPixel;
            if (settings.snapEnabled || io.KeyCtrl) distance = SnapValue(distance, settings.snapUnitMm / 1000.0f);
            for (size_t k = 0; k < arrowDrag.indices.size(); k++)
                objects[arrowDrag.indices[k]].position = arrowDrag.startPositions[k] + axisVectors[arrowDrag.axis] * distance;
            arrowCenter = sceneRenderer.GetSelectionCenter();
            snprintf(readout, sizeof(readout), "Move   %s %.1f mm", axisLabels[arrowDrag.axis], distance * 1000.0f);
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        } else {
            arrowDrag.axis = -1;
        }
    } else if (hoveredAxis >= 0) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    drawList->PushClipRect(viewMin, viewMax, true);

    bool canHover = viewHovered && !editing && modal.mode == Transform_None && arrowDrag.axis < 0 && hoveredAxis < 0 &&
        !ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsMouseDown(ImGuiMouseButton_Right);
    sceneRenderer.hoveredIndex = -1;
    if (canHover) {
        glm::vec3 rayOrigin, rayDir;
        ScreenRay(viewProj, mouse, viewMin, viewSize, rayOrigin, rayDir);
        sceneRenderer.hoveredIndex = sceneRenderer.PickObject(rayOrigin, rayDir);
        glm::vec3 boundsMin, boundsMax;
        if (settings.showDimensions && sceneRenderer.hoveredIndex >= 0 && objects[sceneRenderer.hoveredIndex].GetWorldBounds(boundsMin, boundsMax))
            DrawDimensions(drawList, viewProj, viewMin, viewSize, camera.position, boundsMin, boundsMax);
    }

    if (!editing) {
        if (const char* note = GetEditMessage()) {
            ImVec2 size = ImGui::CalcTextSize(note);
            ImVec2 at = ImVec2(viewMin.x + (viewSize.x - size.x) * 0.5f, viewMin.y + 14.0f);
            drawList->AddRectFilled(ImVec2(at.x - 10, at.y - 6), ImVec2(at.x + size.x + 10, at.y + size.y + 6), IM_COL32(15, 15, 20, 220), 8.0f);
            drawList->AddText(at, IM_COL32(255, 220, 90, 255), note);
        }
    }
    if (editing) {
        bool idle = viewHovered && modal.mode == Transform_None && !box.dragging && !ImGui::IsMouseDown(ImGuiMouseButton_Right);
        DrawEditOverlay(sceneRenderer, drawList, editView, idle ? &mouse : nullptr);
        if (box.dragging) {
            ImVec2 lo = ImVec2(fminf(box.start.x, mouse.x), fminf(box.start.y, mouse.y));
            ImVec2 hi = ImVec2(fmaxf(box.start.x, mouse.x), fmaxf(box.start.y, mouse.y));
            drawList->AddRectFilled(lo, hi, IM_COL32(255, 255, 255, 25));
            drawList->AddRect(lo, hi, IM_COL32(255, 255, 255, 200), 0.0f, 0, 1.5f);
        }

        char label[128];
        snprintf(label, sizeof(label), "Edit Mode   Picking %s   1 Points  2 Edges  3 Faces   %s   Tab to finish", GetEditSelectModeName(GetEditSelectMode()),
                 GetEditSelectThrough() ? "Pick through" : "Front only");
        ImVec2 labelSize = ImGui::CalcTextSize(label);
        ImVec2 labelMin = ImVec2(viewMin.x + 12.0f, viewMin.y + 12.0f);
        drawList->AddRectFilled(labelMin, ImVec2(labelMin.x + labelSize.x + 20.0f, labelMin.y + labelSize.y + 12.0f), ImGui::GetColorU32(ImGuiCol_SliderGrab), 8.0f);
        drawList->AddText(ImVec2(labelMin.x + 10.0f, labelMin.y + 6.0f), IM_COL32(255, 255, 255, 255), label);
        if (const char* note = GetEditMessage()) {
            ImVec2 noteSize = ImGui::CalcTextSize(note);
            ImVec2 noteMin = ImVec2(labelMin.x, labelMin.y + labelSize.y + 18.0f);
            drawList->AddRectFilled(noteMin, ImVec2(noteMin.x + noteSize.x + 20.0f, noteMin.y + noteSize.y + 12.0f), IM_COL32(15, 15, 20, 220), 8.0f);
            drawList->AddText(ImVec2(noteMin.x + 10.0f, noteMin.y + 6.0f), IM_COL32(255, 220, 90, 255), note);
        }
    }

    if (modal.mode != Transform_None && hasSelection) {
        if (modal.axis >= 0) {
            glm::vec3 position = modal.centerWorld;
            float reach = glm::length(position - camera.position) * 4.0f;
            ImVec2 a, b;
            if (ProjectToScreen(viewProj, position - ModalAxis() * reach, viewMin, viewSize, a) &&
                ProjectToScreen(viewProj, position + ModalAxis() * reach, viewMin, viewSize, b))
                drawList->AddLine(a, b, modal.axis == 3 ? IM_COL32(255, 220, 90, 255) : axisColors[modal.axis], 1.5f);
        }
        if (modal.snapped) {
            ImVec2 target;
            if (ProjectToScreen(viewProj, modal.snapTarget, viewMin, viewSize, target)) {
                drawList->AddCircle(target, 9.0f, IM_COL32(0, 0, 0, 160), 0, 4.0f);
                drawList->AddCircle(target, 9.0f, IM_COL32(255, 220, 90, 255), 0, 2.0f);
            }
        }
        if (modal.mode == Transform_Scale || modal.mode == Transform_Rotate) {
            DrawDashedLine(drawList, modal.center, mouse, IM_COL32(235, 235, 235, 200));
            drawList->AddCircleFilled(modal.center, 3.5f, IM_COL32(245, 245, 245, 255));
        }
    }

    if (arrowsVisible && sceneRenderer.selectedIndex == selected) {
        for (int i = 0; i < 3; i++) {
            if (!tipVisible[i]) continue;
            bool hot = (i == hoveredAxis) || (i == arrowDrag.axis);
            ImU32 color = hot ? axisHotColors[i] : axisColors[i];
            DrawArrow(drawList, origin, tips[i], color, hot ? 4.0f : 3.0f);

            ImVec2 dir = ImVec2(tips[i].x - origin.x, tips[i].y - origin.y);
            float length = Length(dir);
            if (length > 1.0f) {
                ImVec2 labelPos = ImVec2(tips[i].x + dir.x / length * 10.0f - 4.0f, tips[i].y + dir.y / length * 10.0f - 8.0f);
                drawList->AddText(labelPos, color, axisLabels[i]);
            }
        }
        drawList->AddCircleFilled(origin, 4.5f, IM_COL32(245, 245, 245, 255));
    }

    if (readout[0] != '\0') {
        const char* help = modal.mode != Transform_None
            ? "X / Y / Z lock axis   Ctrl grid snap   Shift snap to points   Click or Enter confirm   Esc cancel"
            : (GetEditTool() != EditTool_None ? "Ctrl snap   Click or Enter confirm   Esc or Right-click cancel" : "Ctrl snap");
        char axisNote[32] = "";
        if (modal.mode != Transform_None && modal.axis >= 0) snprintf(axisNote, sizeof(axisNote), "   [%s]", modal.axis == 3 ? "along the face" : axisLabels[modal.axis]);
        if (modal.snapped) strncat(axisNote, "   snapped", sizeof(axisNote) - strlen(axisNote) - 1);

        char line[224];
        snprintf(line, sizeof(line), "%s%s", readout, axisNote);
        ImVec2 lineSize = ImGui::CalcTextSize(line);
        ImVec2 helpSize = ImGui::CalcTextSize(help);
        float width = (lineSize.x > helpSize.x ? lineSize.x : helpSize.x) + 24.0f;
        float height = lineSize.y + helpSize.y + 20.0f;
        ImVec2 boxMin = ImVec2(viewMin.x + 12.0f, viewMax.y - height - 12.0f);
        ImVec2 boxMax = ImVec2(boxMin.x + width, boxMin.y + height);
        drawList->AddRectFilled(boxMin, boxMax, IM_COL32(15, 15, 20, 200), 10.0f);
        drawList->AddText(ImVec2(boxMin.x + 12.0f, boxMin.y + 8.0f), IM_COL32(245, 245, 245, 255), line);
        drawList->AddText(ImVec2(boxMin.x + 12.0f, boxMin.y + 12.0f + lineSize.y), IM_COL32(170, 170, 180, 255), help);
    }

    drawList->PopClipRect();
}
