#define GLM_ENABLE_EXPERIMENTAL
#include "ui/SceneGizmo.h"
#include "render/SceneRenderer.h"
#include "core/UISettings.h"
#include "ui/EditMode.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <cfloat>
#include <cmath>
#include <cstdio>

enum TransformMode {
    Transform_None,
    Transform_Grab,
    Transform_Scale,
    Transform_Rotate
};

struct ArrowDrag {
    int axis = -1;
    ImVec2 startMouse;
    glm::vec3 startPosition;
    ImVec2 screenDir;
    float worldPerPixel = 0.0f;
};

struct ModalTransform {
    TransformMode mode = Transform_None;
    int axis = -1;
    int objectIndex = -1;
    bool editTarget = false;
    glm::vec3 centerWorld;
    glm::vec3 startPosition;
    glm::vec3 startRotation;
    glm::vec3 startScale;
    ImVec2 startMouse;
    ImVec2 center;
    glm::vec3 startHit;
    float lastAngle = 0.0f;
    float totalAngle = 0.0f;
};

static ArrowDrag arrowDrag;
static ModalTransform modal;

static const glm::vec3 axisVectors[3] = { glm::vec3(1, 0, 0), glm::vec3(0, 1, 0), glm::vec3(0, 0, 1) };
static const ImU32 axisColors[3] = { IM_COL32(232, 72, 85, 255), IM_COL32(110, 200, 90, 255), IM_COL32(70, 140, 240, 255) };
static const ImU32 axisHotColors[3] = { IM_COL32(255, 150, 160, 255), IM_COL32(180, 245, 160, 255), IM_COL32(150, 200, 255, 255) };
static const char* axisLabels[3] = { "X", "Y", "Z" };

bool IsSceneTransformActive() {
    return modal.mode != Transform_None || arrowDrag.axis >= 0;
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

static void RestoreModalStart(SceneObject& obj) {
    obj.position = modal.startPosition;
    obj.rotation = modal.startRotation;
    obj.scale = modal.startScale;
}

static glm::mat4 RotationMatrix(const glm::vec3& degrees) {
    glm::mat4 matrix = glm::rotate(glm::mat4(1.0f), glm::radians(degrees.x), glm::vec3(1, 0, 0));
    matrix = glm::rotate(matrix, glm::radians(degrees.y), glm::vec3(0, 1, 0));
    return glm::rotate(matrix, glm::radians(degrees.z), glm::vec3(0, 0, 1));
}

static void BeginModal(TransformMode mode, SceneObject& obj, int index, bool editTarget, const glm::vec3& centerWorld, const glm::mat4& viewProj, const Camera& camera, ImVec2 viewMin, ImVec2 viewSize, ImVec2 mouse) {
    modal.mode = mode;
    modal.axis = -1;
    modal.objectIndex = index;
    modal.editTarget = editTarget;
    modal.centerWorld = centerWorld;
    modal.startPosition = obj.position;
    modal.startRotation = obj.rotation;
    modal.startScale = obj.scale;
    modal.startMouse = mouse;
    modal.lastAngle = 0.0f;
    modal.totalAngle = 0.0f;
    if (!ProjectToScreen(viewProj, centerWorld, viewMin, viewSize, modal.center))
        modal.center = ImVec2(viewMin.x + viewSize.x * 0.5f, viewMin.y + viewSize.y * 0.5f);

    glm::vec3 origin, dir;
    ScreenRay(viewProj, mouse, viewMin, viewSize, origin, dir);
    if (!RayPlane(origin, dir, centerWorld, camera.front, modal.startHit)) modal.startHit = centerWorld;

    ImVec2 offset = ImVec2(mouse.x - modal.center.x, mouse.y - modal.center.y);
    modal.lastAngle = atan2f(offset.y, offset.x);
}

static void UpdateModal(SceneRenderer& sceneRenderer, SceneObject& obj, const UISettings& settings, const glm::mat4& viewProj, const Camera& camera, ImVec2 viewMin, ImVec2 viewSize, ImVec2 mouse, char* readout, size_t readoutSize) {
    ImGuiIO& io = ImGui::GetIO();
    bool snapping = settings.snapEnabled || io.KeyCtrl;
    glm::vec3 center = modal.centerWorld;

    if (modal.mode == Transform_Grab) {
        glm::vec3 origin, dir, hit;
        ScreenRay(viewProj, mouse, viewMin, viewSize, origin, dir);
        if (!RayPlane(origin, dir, center, camera.front, hit)) return;

        glm::vec3 delta = hit - modal.startHit;
        if (modal.axis >= 0) delta = axisVectors[modal.axis] * glm::dot(delta, axisVectors[modal.axis]);
        if (snapping) {
            float step = settings.snapUnitMm / 1000.0f;
            delta = glm::vec3(SnapValue(delta.x, step), SnapValue(delta.y, step), SnapValue(delta.z, step));
        }
        if (modal.editTarget) EditApplyTransform(sceneRenderer, [&](const glm::vec3& world) { return world + delta; });
        else obj.position = modal.startPosition + delta;
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
        else obj.scale = modal.startScale * multiplier;
        snprintf(readout, readoutSize, "Scale   x%.3f", factor);
    } else if (modal.mode == Transform_Rotate) {
        ImVec2 offset = ImVec2(mouse.x - modal.center.x, mouse.y - modal.center.y);
        if (Length(offset) > 2.0f) {
            float angle = atan2f(offset.y, offset.x);
            modal.totalAngle += WrapDegrees(glm::degrees(angle - modal.lastAngle));
            modal.lastAngle = angle;
        }

        float degrees = io.KeyCtrl ? SnapValue(modal.totalAngle, 5.0f) : modal.totalAngle;
        glm::vec3 rotationAxis = modal.axis >= 0 ? axisVectors[modal.axis] : camera.front;
        float sign = glm::dot(rotationAxis, camera.front) < 0.0f ? -1.0f : 1.0f;

        glm::mat4 delta = glm::rotate(glm::mat4(1.0f), glm::radians(degrees * sign), rotationAxis);
        if (modal.editTarget) {
            glm::mat3 turn = glm::mat3(delta);
            EditApplyTransform(sceneRenderer, [&](const glm::vec3& world) { return center + turn * (world - center); });
        } else {
            glm::mat4 rotation = delta * RotationMatrix(modal.startRotation);
            float x, y, z;
            glm::extractEulerAngleXYZ(rotation, x, y, z);
            obj.rotation = glm::degrees(glm::vec3(x, y, z));
        }
        snprintf(readout, readoutSize, "Rotate   %.1f deg", degrees);
    }
}

void DrawSceneGizmo(SceneRenderer& sceneRenderer, UISettings& settings, ImVec2 viewMin, ImVec2 viewSize, bool viewHovered, bool viewActivated, bool viewActive) {
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

    if (modal.mode != Transform_None && (!hasSelection || modal.objectIndex != selected || modal.editTarget != editing)) {
        if (modal.editTarget && editing) EditCancelTransform(sceneRenderer);
        modal.mode = Transform_None;
    }

    bool keysAllowed = viewHovered && arrowDrag.axis < 0 && !io.WantTextInput && !io.KeyCtrl && !ImGui::IsMouseDown(ImGuiMouseButton_Right);
    if (modal.mode == Transform_None && keysAllowed) {
        if (editAllowed && hasSelection && ImGui::IsKeyPressed(ImGuiKey_E, false)) {
            if (editing) ExitEditMode();
            else EnterEditMode(sceneRenderer, selected);
            editing = IsEditModeActive();
        }

        if (editing) {
            if (ImGui::IsKeyPressed(ImGuiKey_1, false) || ImGui::IsKeyPressed(ImGuiKey_Keypad1, false)) SetEditSelectMode(EditSelect_Vertex);
            if (ImGui::IsKeyPressed(ImGuiKey_2, false) || ImGui::IsKeyPressed(ImGuiKey_Keypad2, false)) SetEditSelectMode(EditSelect_Edge);
            if (ImGui::IsKeyPressed(ImGuiKey_3, false) || ImGui::IsKeyPressed(ImGuiKey_Keypad3, false)) SetEditSelectMode(EditSelect_Face);
            if (ImGui::IsKeyPressed(ImGuiKey_A, false)) EditToggleSelectAll();
        }

        TransformMode requested = Transform_None;
        if (ImGui::IsKeyPressed(ImGuiKey_G, false)) requested = Transform_Grab;
        else if (ImGui::IsKeyPressed(ImGuiKey_S, false)) requested = Transform_Scale;
        else if (ImGui::IsKeyPressed(ImGuiKey_R, false)) requested = Transform_Rotate;

        if (requested != Transform_None && hasSelection) {
            if (editing) {
                glm::vec3 center;
                if (EditBeginTransform(sceneRenderer, center))
                    BeginModal(requested, objects[selected], selected, true, center, viewProj, camera, viewMin, viewSize, mouse);
            } else {
                BeginModal(requested, objects[selected], selected, false, objects[selected].position, viewProj, camera, viewMin, viewSize, mouse);
            }
        }
    }

    char readout[160] = "";
    if (modal.mode != Transform_None) {
        SceneObject& obj = objects[selected];
        for (int i = 0; i < 3; i++) {
            if (ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_X + i), false)) modal.axis = (modal.axis == i) ? -1 : i;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            if (modal.editTarget) EditCancelTransform(sceneRenderer);
            else RestoreModalStart(obj);
            modal.mode = Transform_None;
            clickConsumed = true;
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
            UpdateModal(sceneRenderer, obj, settings, viewProj, camera, viewMin, viewSize, mouse, readout, sizeof(readout));
            modal.mode = Transform_None;
            clickConsumed = true;
        } else {
            UpdateModal(sceneRenderer, obj, settings, viewProj, camera, viewMin, viewSize, mouse, readout, sizeof(readout));
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
        glm::vec3 position = objects[selected].position;
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

    if (viewActivated && !clickConsumed && modal.mode == Transform_None && editing) {
        glm::vec3 rayOrigin, rayDir;
        ScreenRay(viewProj, mouse, viewMin, viewSize, rayOrigin, rayDir);
        EditPick(sceneRenderer, viewProj, viewMin, viewSize, mouse, rayOrigin, rayDir, io.KeyShift);
    } else if (viewActivated && !clickConsumed && modal.mode == Transform_None) {
        if (hoveredAxis >= 0) {
            ImVec2 dir = ImVec2(tips[hoveredAxis].x - origin.x, tips[hoveredAxis].y - origin.y);
            float screenLength = Length(dir);
            arrowDrag.axis = hoveredAxis;
            arrowDrag.startMouse = mouse;
            arrowDrag.startPosition = objects[selected].position;
            arrowDrag.screenDir = ImVec2(dir.x / screenLength, dir.y / screenLength);
            arrowDrag.worldPerPixel = axisLength / screenLength;
        } else {
            glm::vec3 rayOrigin, rayDir;
            ScreenRay(viewProj, mouse, viewMin, viewSize, rayOrigin, rayDir);
            sceneRenderer.selectedIndex = sceneRenderer.PickObject(rayOrigin, rayDir);
        }
    }

    if (arrowDrag.axis >= 0) {
        if (viewActive && hasSelection) {
            float along = (mouse.x - arrowDrag.startMouse.x) * arrowDrag.screenDir.x + (mouse.y - arrowDrag.startMouse.y) * arrowDrag.screenDir.y;
            float distance = along * arrowDrag.worldPerPixel;
            if (settings.snapEnabled || io.KeyCtrl) distance = SnapValue(distance, settings.snapUnitMm / 1000.0f);
            objects[selected].position = arrowDrag.startPosition + axisVectors[arrowDrag.axis] * distance;
            snprintf(readout, sizeof(readout), "Move   %s %.1f mm", axisLabels[arrowDrag.axis], distance * 1000.0f);
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll);
        } else {
            arrowDrag.axis = -1;
        }
    } else if (hoveredAxis >= 0) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }

    drawList->PushClipRect(viewMin, viewMax, true);

    if (editing) {
        DrawEditOverlay(sceneRenderer, drawList, viewProj, viewMin, viewSize);

        char label[96];
        snprintf(label, sizeof(label), "Edit Mode  -  %s  (1 / 2 / 3)", GetEditSelectModeName(GetEditSelectMode()));
        ImVec2 labelSize = ImGui::CalcTextSize(label);
        ImVec2 labelMin = ImVec2(viewMin.x + 12.0f, viewMin.y + 12.0f);
        drawList->AddRectFilled(labelMin, ImVec2(labelMin.x + labelSize.x + 20.0f, labelMin.y + labelSize.y + 12.0f), ImGui::GetColorU32(ImGuiCol_SliderGrab), 8.0f);
        drawList->AddText(ImVec2(labelMin.x + 10.0f, labelMin.y + 6.0f), IM_COL32(255, 255, 255, 255), label);
    }

    if (modal.mode != Transform_None && hasSelection) {
        if (modal.axis >= 0) {
            glm::vec3 position = modal.centerWorld;
            float reach = glm::length(position - camera.position) * 4.0f;
            ImVec2 a, b;
            if (ProjectToScreen(viewProj, position - axisVectors[modal.axis] * reach, viewMin, viewSize, a) &&
                ProjectToScreen(viewProj, position + axisVectors[modal.axis] * reach, viewMin, viewSize, b))
                drawList->AddLine(a, b, axisColors[modal.axis], 1.5f);
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
            ? "X / Y / Z lock axis   Ctrl snap   Click or Enter confirm   Esc or Right-click cancel"
            : "Ctrl snap";
        char axisNote[32] = "";
        if (modal.mode != Transform_None && modal.axis >= 0) snprintf(axisNote, sizeof(axisNote), "   [%s axis]", axisLabels[modal.axis]);

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
