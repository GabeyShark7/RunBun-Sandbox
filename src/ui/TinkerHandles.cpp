#include "core/Profiler.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "ui/TinkerHandles.h"
#include "ui/EditMode.h"
#include "ui/SceneGizmo.h"
#include "core/UISettings.h"
#include "render/SceneRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>

enum HandleKind { Handle_None, Handle_Resize, Handle_Height, Handle_Lift, Handle_Rotate };

struct Handle {
    HandleKind kind = Handle_None;
    glm::vec3 local;
    glm::vec3 anchor;
    int mask = 0;
    int axis = 0;
    ImVec2 screen;
};

struct HandleDrag {
    HandleKind kind = Handle_None;
    Handle handle;
    int object = -1;
    unsigned int id = 0;
    ImVec2 startMouse;
    bool moved = false;
    glm::mat4 startMatrix;
    glm::vec3 startScale, startPosition, startRotation;
    glm::vec3 lineOrigin, lineDir;
    float startParam = 0.0f;
    glm::vec3 center;
    ImVec2 centerScreen;
    float startAngle = 0.0f;
};

static HandleDrag drag;
static bool openSizePopup = false;
static unsigned int popupObject = 0;

bool TinkerHandleActive() {
    return drag.kind != Handle_None;
}

static bool Project(const glm::mat4& viewProj, const glm::vec3& p, ImVec2 viewMin, ImVec2 viewSize, ImVec2& out) {
    glm::vec4 clip = viewProj * glm::vec4(p, 1.0f);
    if (clip.w <= 0.0001f) return false;
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    out = ImVec2(viewMin.x + (ndc.x * 0.5f + 0.5f) * viewSize.x, viewMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewSize.y);
    return true;
}

static void Ray(const glm::mat4& viewProj, ImVec2 p, ImVec2 viewMin, ImVec2 viewSize, glm::vec3& origin, glm::vec3& dir) {
    float ndcX = (p.x - viewMin.x) / viewSize.x * 2.0f - 1.0f;
    float ndcY = 1.0f - (p.y - viewMin.y) / viewSize.y * 2.0f;
    glm::mat4 inverse = glm::inverse(viewProj);
    glm::vec4 a = inverse * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 b = inverse * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    origin = glm::vec3(a) / a.w;
    dir = glm::normalize(glm::vec3(b) / b.w - origin);
}

// Nearest spot on a line
static float LineParam(const glm::vec3& lineOrigin, const glm::vec3& lineDir, const glm::vec3& rayOrigin, const glm::vec3& rayDir) {
    glm::vec3 w = lineOrigin - rayOrigin;
    float a = glm::dot(lineDir, lineDir), b = glm::dot(lineDir, rayDir), c = glm::dot(rayDir, rayDir);
    float d = glm::dot(lineDir, w), e = glm::dot(rayDir, w);
    float denom = a * c - b * b;
    if (std::fabs(denom) < 1e-9f) return 0.0f;
    return (b * e - c * d) / denom;
}

static float Snap(float value, bool active) {
    UISettings& settings = GetUISettings();
    float step = settings.snapUnitMm / 1000.0f;
    if (!active || step <= 0.0f) return value;
    return roundf(value / step) * step;
}

static float Length(ImVec2 v) { return sqrtf(v.x * v.x + v.y * v.y); }

static glm::vec3 BoxPoint(const glm::vec3& lo, const glm::vec3& hi, const glm::vec3& t) {
    return lo + (hi - lo) * t;
}

static void KeepAnchor(SceneObject& obj, const glm::vec3& anchorLocal, const glm::vec3& anchorWorld) {
    glm::vec3 now = glm::vec3(obj.GetModelMatrix() * glm::vec4(anchorLocal, 1.0f));
    obj.position += anchorWorld - now;
}

static void DrawSizePopup(SceneRenderer& sceneRenderer) {
    if (openSizePopup) {
        ImGui::OpenPopup("Exact Size##Handles");
        openSizePopup = false;
    }
    if (!ImGui::BeginPopup("Exact Size##Handles")) return;
    auto& objects = sceneRenderer.GetObjects();
    int index = -1;
    for (int i = 0; i < (int)objects.size(); i++) if (objects[i].id == popupObject) index = i;
    glm::vec3 lo, hi;
    if (index < 0 || !objects[index].GetLocalBounds(lo, hi)) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    SceneObject& obj = objects[index];
    ImGui::TextDisabled("Type exact sizes in millimeters");
    glm::vec3 extent = (hi - lo) * glm::abs(obj.scale);
    glm::vec3 mm = extent * 1000.0f;
    const char* labels[3] = { "Width (X)", "Depth (Y)", "Height (Z)" };
    glm::vec3 bottomLocal = glm::vec3((lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, lo.z);
    glm::vec3 bottomWorld = glm::vec3(obj.GetModelMatrix() * glm::vec4(bottomLocal, 1.0f));
    for (int k = 0; k < 3; k++) {
        ImGui::SetNextItemWidth(130.0f);
        if (ImGui::InputFloat(labels[k], &mm[k], 1.0f, 10.0f, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue) && mm[k] > 0.01f && hi[k] - lo[k] > 1e-9f) {
            float sign = obj.scale[k] < 0.0f ? -1.0f : 1.0f;
            obj.scale[k] = sign * (mm[k] / 1000.0f) / (hi[k] - lo[k]);
            KeepAnchor(obj, bottomLocal, bottomWorld);
        }
    }
    glm::vec3 a, b;
    if (obj.GetWorldBounds(a, b)) {
        float lift = a.z * 1000.0f;
        ImGui::SetNextItemWidth(130.0f);
        if (ImGui::InputFloat("Above ground", &lift, 1.0f, 10.0f, "%.1f", ImGuiInputTextFlags_EnterReturnsTrue)) obj.position.z += lift / 1000.0f - a.z;
    }
    ImGui::EndPopup();
}

bool DrawTinkerHandles(SceneRenderer& sceneRenderer, UISettings& settings, ImVec2 viewMin, ImVec2 viewSize, bool viewHovered, bool viewActivated) {
    PROFILE("DrawTinkerHandles");
    DrawSizePopup(sceneRenderer);
    auto& objects = sceneRenderer.GetObjects();
    int selected = sceneRenderer.selectedIndex;
    bool usable = settings.showHandles && !IsEditModeActive() && sceneRenderer.GetSelection().size() == 1 && selected >= 0 &&
                  selected < (int)objects.size() && (!IsSceneTransformActive() || drag.kind != Handle_None);
    if (!usable) {
        drag = HandleDrag();
        return false;
    }
    SceneObject& obj = objects[selected];
    if (obj.look.light >= 0) {
        drag = HandleDrag();
        return false;
    }
    glm::vec3 lo, hi;
    if (!obj.GetLocalBounds(lo, hi)) return false;

    ImGuiIO& io = ImGui::GetIO();
    Camera& camera = sceneRenderer.GetCamera();
    glm::mat4 viewProj = camera.GetProjectionMatrix((int)viewSize.x, (int)viewSize.y) * camera.GetViewMatrix();
    glm::mat4 matrix = drag.kind != Handle_None ? glm::mat4(1.0f) : obj.GetModelMatrix();
    if (drag.kind != Handle_None) matrix = obj.GetModelMatrix();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 viewMax = ImVec2(viewMin.x + viewSize.x, viewMin.y + viewSize.y);
    drawList->PushClipRect(viewMin, viewMax, true);

    // Box outline
    ImVec2 corners[8];
    bool cornerOk[8];
    for (int c = 0; c < 8; c++) {
        glm::vec3 t((c & 1) ? 1.0f : 0.0f, (c & 2) ? 1.0f : 0.0f, (c & 4) ? 1.0f : 0.0f);
        cornerOk[c] = Project(viewProj, glm::vec3(matrix * glm::vec4(BoxPoint(lo, hi, t), 1.0f)), viewMin, viewSize, corners[c]);
    }
    for (int c = 0; c < 8; c++)
        for (int bit = 1; bit <= 4; bit <<= 1)
            if (!(c & bit) && cornerOk[c] && cornerOk[c | bit]) drawList->AddLine(corners[c], corners[c | bit], IM_COL32(255, 255, 255, 70), 1.0f);

    std::vector<Handle> handles;
    auto addHandle = [&](HandleKind kind, glm::vec3 t, glm::vec3 anchorT, int mask, int axis) {
        Handle h;
        h.kind = kind;
        h.local = BoxPoint(lo, hi, t);
        h.anchor = BoxPoint(lo, hi, anchorT);
        h.mask = mask;
        h.axis = axis;
        if (Project(viewProj, glm::vec3(matrix * glm::vec4(h.local, 1.0f)), viewMin, viewSize, h.screen)) handles.push_back(h);
    };
    for (int c = 0; c < 4; c++) {
        float tx = (c & 1) ? 1.0f : 0.0f, ty = (c & 2) ? 1.0f : 0.0f;
        addHandle(Handle_Resize, glm::vec3(tx, ty, 0.0f), glm::vec3(1.0f - tx, 1.0f - ty, 0.0f), 3, 0);
    }
    addHandle(Handle_Resize, glm::vec3(0.5f, 0.0f, 0.0f), glm::vec3(0.5f, 1.0f, 0.0f), 2, 0);
    addHandle(Handle_Resize, glm::vec3(0.5f, 1.0f, 0.0f), glm::vec3(0.5f, 0.0f, 0.0f), 2, 0);
    addHandle(Handle_Resize, glm::vec3(0.0f, 0.5f, 0.0f), glm::vec3(1.0f, 0.5f, 0.0f), 1, 0);
    addHandle(Handle_Resize, glm::vec3(1.0f, 0.5f, 0.0f), glm::vec3(0.0f, 0.5f, 0.0f), 1, 0);
    addHandle(Handle_Height, glm::vec3(0.5f, 0.5f, 1.0f), glm::vec3(0.5f, 0.5f, 0.0f), 4, 2);

    ImVec2 top, topAbove;
    glm::vec3 topWorld = glm::vec3(matrix * glm::vec4(BoxPoint(lo, hi, glm::vec3(0.5f, 0.5f, 1.0f)), 1.0f));
    float reach = glm::length(topWorld - camera.position) * 0.06f;
    if (Project(viewProj, topWorld, viewMin, viewSize, top) && Project(viewProj, topWorld + glm::vec3(0, 0, reach), viewMin, viewSize, topAbove)) {
        Handle lift;
        lift.kind = Handle_Lift;
        lift.screen = topAbove;
        handles.push_back(lift);
    }
    glm::vec3 centerWorld = glm::vec3(matrix * glm::vec4((lo + hi) * 0.5f, 1.0f));
    ImVec2 centerScreen;
    bool centerOk = Project(viewProj, centerWorld, viewMin, viewSize, centerScreen);
    const glm::vec3 spots[3] = { glm::vec3(1.0f, 0.5f, 0.5f), glm::vec3(0.5f, 0.0f, 0.5f), glm::vec3(1.0f, 0.0f, 0.0f) };
    for (int axis = 0; axis < 3 && centerOk; axis++) {
        ImVec2 spot;
        if (!Project(viewProj, glm::vec3(matrix * glm::vec4(BoxPoint(lo, hi, spots[axis]), 1.0f)), viewMin, viewSize, spot)) continue;
        ImVec2 out = ImVec2(spot.x - centerScreen.x, spot.y - centerScreen.y);
        float length = std::max(Length(out), 1.0f);
        Handle turn;
        turn.kind = Handle_Rotate;
        turn.axis = axis;
        turn.screen = ImVec2(spot.x + out.x / length * 28.0f, spot.y + out.y / length * 28.0f);
        handles.push_back(turn);
    }

    int hovered = -1;
    if (drag.kind == Handle_None && viewHovered && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        float best = 10.0f;
        for (int h = 0; h < (int)handles.size(); h++) {
            float d = Length(ImVec2(handles[h].screen.x - io.MousePos.x, handles[h].screen.y - io.MousePos.y));
            if (d < best) { best = d; hovered = h; }
        }
    }

    bool taken = false;
    if (hovered >= 0 && viewActivated) {
        drag = HandleDrag();
        drag.kind = handles[hovered].kind;
        drag.handle = handles[hovered];
        drag.object = selected;
        drag.id = obj.id;
        drag.startMouse = io.MousePos;
        drag.startMatrix = matrix;
        drag.startScale = obj.scale;
        drag.startPosition = obj.position;
        drag.startRotation = obj.rotation;
        drag.center = centerWorld;
        drag.centerScreen = centerScreen;
        drag.startAngle = atan2f(io.MousePos.y - centerScreen.y, io.MousePos.x - centerScreen.x);
        glm::vec3 origin, dir;
        Ray(viewProj, io.MousePos, viewMin, viewSize, origin, dir);
        if (drag.kind == Handle_Height) {
            drag.lineOrigin = glm::vec3(matrix * glm::vec4(drag.handle.anchor, 1.0f));
            drag.lineDir = glm::normalize(glm::vec3(matrix[2]));
            drag.startParam = LineParam(drag.lineOrigin, drag.lineDir, origin, dir);
        } else if (drag.kind == Handle_Lift) {
            drag.lineOrigin = topWorld;
            drag.lineDir = glm::vec3(0.0f, 0.0f, 1.0f);
            drag.startParam = LineParam(drag.lineOrigin, drag.lineDir, origin, dir);
        }
        taken = true;
    }

    char label[96] = "";
    if (drag.kind != Handle_None) {
        taken = true;
        if (drag.id != obj.id) {
            drag = HandleDrag();
        } else if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            if (!drag.moved && drag.kind != Handle_Rotate) {
                openSizePopup = true;
                popupObject = obj.id;
            }
            drag = HandleDrag();
        } else if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            obj.scale = drag.startScale;
            obj.position = drag.startPosition;
            obj.rotation = drag.startRotation;
            drag = HandleDrag();
        } else {
            if (Length(ImVec2(io.MousePos.x - drag.startMouse.x, io.MousePos.y - drag.startMouse.y)) > 3.0f) drag.moved = true;
            bool snapping = settings.snapEnabled || io.KeyCtrl;
            glm::vec3 origin, dir;
            Ray(viewProj, io.MousePos, viewMin, viewSize, origin, dir);
            if (drag.moved && drag.kind == Handle_Resize) {
                glm::mat4 inverse = glm::inverse(drag.startMatrix);
                glm::vec3 o = glm::vec3(inverse * glm::vec4(origin, 1.0f));
                glm::vec3 d = glm::vec3(inverse * glm::vec4(dir, 0.0f));
                if (std::fabs(d.z) > 1e-9f) {
                    float t = (drag.handle.local.z - o.z) / d.z;
                    glm::vec3 p = o + d * t;
                    glm::vec3 ratio(1.0f);
                    for (int k = 0; k < 2; k++) {
                        if (!(drag.handle.mask & (1 << k))) continue;
                        float original = std::fabs(drag.handle.local[k] - drag.handle.anchor[k]);
                        if (original < 1e-9f) continue;
                        float worldSize = std::fabs(p[k] - drag.handle.anchor[k]) * std::fabs(drag.startScale[k]);
                        worldSize = std::max(Snap(worldSize, snapping), 0.001f);
                        ratio[k] = worldSize / (original * std::fabs(drag.startScale[k]));
                    }
                    if (io.KeyShift) {
                        float uniform = (drag.handle.mask == 3) ? std::max(ratio.x, ratio.y) : (drag.handle.mask == 1 ? ratio.x : ratio.y);
                        ratio = glm::vec3(uniform);
                    }
                    glm::vec3 anchorWorld = glm::vec3(drag.startMatrix * glm::vec4(drag.handle.anchor, 1.0f));
                    obj.scale = drag.startScale * ratio;
                    obj.position = drag.startPosition;
                    KeepAnchor(obj, drag.handle.anchor, anchorWorld);
                }
            } else if (drag.moved && drag.kind == Handle_Height) {
                float param = LineParam(drag.lineOrigin, drag.lineDir, origin, dir);
                float original = drag.handle.local.z - drag.handle.anchor.z;
                float startHeight = original * std::fabs(drag.startScale.z);
                float height = std::max(Snap(startHeight + (param - drag.startParam), snapping), 0.001f);
                float ratio = original > 1e-9f ? height / startHeight : 1.0f;
                glm::vec3 anchorWorld = glm::vec3(drag.startMatrix * glm::vec4(drag.handle.anchor, 1.0f));
                obj.scale = drag.startScale * (io.KeyShift ? glm::vec3(ratio) : glm::vec3(1.0f, 1.0f, ratio));
                obj.position = drag.startPosition;
                KeepAnchor(obj, drag.handle.anchor, anchorWorld);
            } else if (drag.moved && drag.kind == Handle_Lift) {
                float param = LineParam(drag.lineOrigin, drag.lineDir, origin, dir);
                obj.position = drag.startPosition;
                obj.position.z += Snap(param - drag.startParam, snapping);
                snprintf(label, sizeof(label), "Lift %.1f mm", (obj.position.z - drag.startPosition.z) * 1000.0f);
            } else if (drag.kind == Handle_Rotate) {
                float angle = atan2f(io.MousePos.y - drag.centerScreen.y, io.MousePos.x - drag.centerScreen.x);
                float degrees = glm::degrees(angle - drag.startAngle);
                while (degrees > 180.0f) degrees -= 360.0f;
                while (degrees < -180.0f) degrees += 360.0f;
                glm::vec3 axisWorld(0.0f);
                axisWorld[drag.handle.axis] = 1.0f;
                glm::vec3 toEye = camera.position - drag.center;
                if (glm::dot(axisWorld, toEye) < 0.0f) degrees = -degrees;
                degrees = io.KeyShift ? roundf(degrees) : roundf(degrees / 22.5f) * 22.5f;
                glm::mat4 turn = glm::rotate(glm::mat4(1.0f), glm::radians(-degrees), axisWorld);
                glm::mat3 rotation = glm::mat3(turn) * glm::mat3(glm::eulerAngleXYZ(glm::radians(drag.startRotation.x), glm::radians(drag.startRotation.y), glm::radians(drag.startRotation.z)));
                float x, y, z;
                glm::extractEulerAngleXYZ(glm::mat4(rotation), x, y, z);
                obj.rotation = glm::degrees(glm::vec3(x, y, z));
                obj.position = drag.center + glm::mat3(turn) * (drag.startPosition - drag.center);
                snprintf(label, sizeof(label), "Turn %.1f deg", degrees);
                drawList->AddCircle(drag.centerScreen, Length(ImVec2(drag.startMouse.x - drag.centerScreen.x, drag.startMouse.y - drag.centerScreen.y)), IM_COL32(255, 220, 90, 160), 64, 1.5f);
            }
            if (!label[0] && drag.moved) {
                glm::vec3 size = (hi - lo) * glm::abs(obj.scale) * 1000.0f;
                snprintf(label, sizeof(label), "%.1f x %.1f x %.1f mm", size.x, size.y, size.z);
            }
        }
    }

    ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);
    ImU32 hot = ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 1.0f));
    for (int h = 0; h < (int)handles.size(); h++) {
        const Handle& handle = handles[h];
        bool lit = h == hovered || (drag.kind != Handle_None && drag.handle.kind == handle.kind && drag.handle.screen.x == handle.screen.x && drag.handle.screen.y == handle.screen.y);
        ImVec2 p = handle.screen;
        if (handle.kind == Handle_Resize || handle.kind == Handle_Height) {
            float s = handle.kind == Handle_Height ? 5.5f : 5.0f;
            ImU32 fill = lit ? hot : (handle.kind == Handle_Height ? IM_COL32(20, 20, 24, 255) : IM_COL32(250, 250, 252, 255));
            drawList->AddRectFilled(ImVec2(p.x - s - 1, p.y - s - 1), ImVec2(p.x + s + 1, p.y + s + 1), IM_COL32(0, 0, 0, 200), 2.0f);
            drawList->AddRectFilled(ImVec2(p.x - s, p.y - s), ImVec2(p.x + s, p.y + s), fill, 2.0f);
            if (handle.kind == Handle_Height) drawList->AddRect(ImVec2(p.x - s, p.y - s), ImVec2(p.x + s, p.y + s), IM_COL32(250, 250, 252, 255), 2.0f);
        } else if (handle.kind == Handle_Lift) {
            drawList->AddLine(top, p, IM_COL32(0, 0, 0, 140), 3.0f);
            drawList->AddLine(top, p, IM_COL32(20, 20, 24, 255), 1.5f);
            drawList->AddTriangleFilled(ImVec2(p.x, p.y - 9.0f), ImVec2(p.x - 7.0f, p.y + 5.0f), ImVec2(p.x + 7.0f, p.y + 5.0f), lit ? hot : IM_COL32(20, 20, 24, 255));
            drawList->AddTriangle(ImVec2(p.x, p.y - 9.0f), ImVec2(p.x - 7.0f, p.y + 5.0f), ImVec2(p.x + 7.0f, p.y + 5.0f), IM_COL32(250, 250, 252, 255), 1.2f);
        } else if (handle.kind == Handle_Rotate) {
            static const ImU32 axisColors[3] = { IM_COL32(232, 72, 85, 255), IM_COL32(110, 200, 90, 255), IM_COL32(70, 140, 240, 255) };
            ImU32 color = lit ? hot : axisColors[handle.axis];
            drawList->PathArcTo(p, 8.0f, 0.4f, 4.6f, 16);
            drawList->PathStroke(IM_COL32(0, 0, 0, 160), 0, 4.5f);
            drawList->PathArcTo(p, 8.0f, 0.4f, 4.6f, 16);
            drawList->PathStroke(color, 0, 2.5f);
            ImVec2 tip = ImVec2(p.x + cosf(4.6f) * 8.0f, p.y + sinf(4.6f) * 8.0f);
            drawList->AddTriangleFilled(ImVec2(tip.x - 4.0f, tip.y - 2.0f), ImVec2(tip.x + 4.0f, tip.y - 4.0f), ImVec2(tip.x + 1.0f, tip.y + 4.0f), color);
        }
    }
    if (hovered >= 0) {
        const char* tips[5] = { "", "Drag to resize (Shift keeps the shape). Click to type a size.", "Drag to change the height. Click to type a size.",
                                "Drag to lift off the ground. Click to type the height.", "Drag to turn (Shift for 1 degree steps)" };
        ImGui::SetTooltip("%s", tips[handles[hovered].kind]);
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    if (label[0]) {
        ImVec2 size = ImGui::CalcTextSize(label);
        ImVec2 at = ImVec2(io.MousePos.x + 18.0f, io.MousePos.y + 12.0f);
        drawList->AddRectFilled(ImVec2(at.x - 6.0f, at.y - 3.0f), ImVec2(at.x + size.x + 6.0f, at.y + size.y + 3.0f), IM_COL32(15, 15, 20, 220), 5.0f);
        drawList->AddText(at, IM_COL32(245, 245, 245, 255), label);
    }
    drawList->PopClipRect();
    return taken || hovered >= 0 && viewActivated;
}
