#include "ui/SceneNavigation.h"
#include "ui/SceneCommands.h"
#include "ui/SceneGizmo.h"
#include "ui/EditMode.h"
#include "core/UISettings.h"
#include "render/SceneRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

enum DragKind { Drag_None, Drag_Orbit, Drag_Pan };
static DragKind dragKind = Drag_None;
static ImGuiMouseButton dragButton = ImGuiMouseButton_Right;
static glm::vec3 pivot = glm::vec3(0.0f);

static glm::vec3 GroundUnderCenter(SceneRenderer& sceneRenderer) {
    Camera& camera = sceneRenderer.GetCamera();
    if (camera.front.z < -0.05f) {
        float t = -camera.position.z / camera.front.z;
        if (t > 0.0f && t < 500.0f) return camera.position + camera.front * t;
    }
    return camera.position + camera.front * 5.0f;
}

static void Orbit(Camera& camera, float dx, float dy) {
    float dyaw = -dx * 0.3f;
    float targetPitch = std::max(-89.0f, std::min(89.0f, camera.pitch + dy * 0.3f));
    float dpitch = targetPitch - camera.pitch;
    glm::vec3 offset = camera.position - pivot;
    glm::mat4 yawTurn = glm::rotate(glm::mat4(1.0f), glm::radians(dyaw), glm::vec3(0.0f, 0.0f, 1.0f));
    offset = glm::vec3(yawTurn * glm::vec4(offset, 0.0f));
    camera.SetAngles(camera.yaw + dyaw, camera.pitch);
    glm::vec3 right = glm::normalize(glm::cross(camera.front, glm::vec3(0.0f, 0.0f, 1.0f)));
    glm::mat4 pitchTurn = glm::rotate(glm::mat4(1.0f), glm::radians(dpitch), right);
    offset = glm::vec3(pitchTurn * glm::vec4(offset, 0.0f));
    camera.SetAngles(camera.yaw, targetPitch);
    camera.position = pivot + offset;
}

static void Pan(Camera& camera, float dx, float dy) {
    glm::vec3 right = glm::normalize(glm::cross(camera.front, glm::vec3(0.0f, 0.0f, 1.0f)));
    glm::vec3 up = glm::normalize(glm::cross(right, camera.front));
    float distance = std::max(glm::length(pivot - camera.position), 0.3f);
    float speed = distance * 0.0016f;
    camera.position += (-dx * right + dy * up) * speed;
    pivot += (-dx * right + dy * up) * speed;
}

void HandleSceneNavigation(SceneRenderer& sceneRenderer, ImVec2 viewMin, ImVec2 viewSize, bool viewHovered) {
    UISettings& settings = GetUISettings();
    ImGuiIO& io = ImGui::GetIO();
    Camera& camera = sceneRenderer.GetCamera();
    bool busy = IsSceneTransformActive();

    if (dragKind == Drag_None && viewHovered && !busy && !io.KeyCtrl) {
        DragKind wanted = Drag_None;
        if (settings.cameraOrbit && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
            wanted = io.KeyShift ? Drag_Pan : Drag_Orbit;
            dragButton = ImGuiMouseButton_Right;
        } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) {
            wanted = (settings.cameraOrbit != io.KeyShift) ? Drag_Pan : Drag_Orbit;
            dragButton = ImGuiMouseButton_Middle;
        }
        if (wanted != Drag_None) {
            dragKind = wanted;
            pivot = sceneRenderer.GetSelection().empty() ? GroundUnderCenter(sceneRenderer) : sceneRenderer.GetSelectionCenter();
        }
    }
    if (dragKind != Drag_None) {
        if (!ImGui::IsMouseDown(dragButton) || busy) {
            dragKind = Drag_None;
        } else if (io.MouseDelta.x != 0.0f || io.MouseDelta.y != 0.0f) {
            if (dragKind == Drag_Orbit) {
                // Right drag moves camera
                float dx = dragButton == ImGuiMouseButton_Right ? -io.MouseDelta.x : io.MouseDelta.x;
                float dy = io.MouseDelta.y;
                if (settings.orbitFlipX) dx = -dx;
                if (settings.orbitFlipY) dy = -dy;
                Orbit(camera, dx, dy);
            }
            else Pan(camera, io.MouseDelta.x, io.MouseDelta.y);
        }
    }

    bool toolWantsWheel = GetEditTool() == EditTool_LoopCut;
    if (viewHovered && io.MouseWheel != 0.0f && !toolWantsWheel && !camera.IsAnimating()) {
        glm::mat4 viewProj = camera.GetProjectionMatrix((int)viewSize.x, (int)viewSize.y) * camera.GetViewMatrix();
        glm::mat4 inverse = glm::inverse(viewProj);
        float ndcX = (io.MousePos.x - viewMin.x) / viewSize.x * 2.0f - 1.0f;
        float ndcY = 1.0f - (io.MousePos.y - viewMin.y) / viewSize.y * 2.0f;
        glm::vec4 nearPoint = inverse * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
        glm::vec4 farPoint = inverse * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
        glm::vec3 origin = glm::vec3(nearPoint) / nearPoint.w;
        glm::vec3 dir = glm::normalize(glm::vec3(farPoint) / farPoint.w - origin);
        float hit = 0.0f;
        int picked = sceneRenderer.PickObject(origin, dir, &hit);
        float distance = picked >= 0 ? hit : glm::length(GroundUnderCenter(sceneRenderer) - camera.position);
        distance = std::max(distance, 0.2f);
        float step = io.MouseWheel * distance * 0.15f;
        if (step > distance * 0.8f) step = distance * 0.8f;
        camera.position += dir * step;
    }
}

struct CubeFace {
    glm::vec3 normal;
    const char* label;
    int preset;
};

void DrawViewCube(SceneRenderer& sceneRenderer, ImDrawList* drawList, ImVec2 center, float size, bool clicked) {
    static const CubeFace faces[6] = {
        { glm::vec3(0, 0, 1), "TOP", View_Top },
        { glm::vec3(0, 0, -1), "BOTTOM", View_Bottom },
        { glm::vec3(0, -1, 0), "FRONT", View_Front },
        { glm::vec3(0, 1, 0), "BACK", View_Back },
        { glm::vec3(1, 0, 0), "RIGHT", View_Right },
        { glm::vec3(-1, 0, 0), "LEFT", View_Left },
    };
    Camera& camera = sceneRenderer.GetCamera();
    glm::mat3 view = glm::mat3(camera.GetViewMatrix());
    auto project = [&](const glm::vec3& p) {
        glm::vec3 v = view * p;
        return ImVec2(center.x + v.x * size, center.y - v.y * size);
    };

    struct Drawn { float depth; int face; ImVec2 points[4]; };
    std::vector<Drawn> visible;
    for (int f = 0; f < 6; f++) {
        glm::vec3 n = faces[f].normal;
        glm::vec3 facing = view * n;
        if (facing.z <= 0.02f) continue;
        glm::vec3 u = std::fabs(n.z) > 0.5f ? glm::vec3(1, 0, 0) : glm::vec3(0, 0, 1);
        glm::vec3 w = glm::cross(n, u);
        Drawn d;
        d.depth = facing.z;
        d.face = f;
        glm::vec3 c = n * 0.5f;
        d.points[0] = project(c - u * 0.5f - w * 0.5f);
        d.points[1] = project(c + u * 0.5f - w * 0.5f);
        d.points[2] = project(c + u * 0.5f + w * 0.5f);
        d.points[3] = project(c - u * 0.5f + w * 0.5f);
        visible.push_back(d);
    }
    std::sort(visible.begin(), visible.end(), [](const Drawn& a, const Drawn& b) { return a.depth < b.depth; });

    ImVec2 mouse = ImGui::GetIO().MousePos;
    int hovered = -1;
    for (const Drawn& d : visible) {
        bool inside = true;
        for (int k = 0; k < 4 && inside; k++) {
            ImVec2 a = d.points[k], b = d.points[(k + 1) % 4];
            float cross = (b.x - a.x) * (mouse.y - a.y) - (b.y - a.y) * (mouse.x - a.x);
            float winding = (d.points[1].x - d.points[0].x) * (d.points[2].y - d.points[0].y) - (d.points[1].y - d.points[0].y) * (d.points[2].x - d.points[0].x);
            if (cross * winding < 0.0f) inside = false;
        }
        if (inside) hovered = d.face;
    }

    ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);
    for (const Drawn& d : visible) {
        float shade = 0.35f + 0.45f * d.depth;
        ImU32 fill = d.face == hovered ? ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.95f))
                                       : ImGui::GetColorU32(ImVec4(shade * 0.55f, shade * 0.57f, shade * 0.62f, 0.92f));
        drawList->AddConvexPolyFilled(d.points, 4, fill);
        drawList->AddPolyline(d.points, 4, IM_COL32(0, 0, 0, 160), ImDrawFlags_Closed, 1.2f);
        if (d.depth > 0.35f) {
            ImVec2 mid = ImVec2((d.points[0].x + d.points[2].x) * 0.5f, (d.points[0].y + d.points[2].y) * 0.5f);
            float scale = 0.62f;
            ImVec2 text = ImGui::CalcTextSize(faces[d.face].label);
            drawList->AddText(ImGui::GetFont(), ImGui::GetFontSize() * scale, ImVec2(mid.x - text.x * scale * 0.5f, mid.y - text.y * scale * 0.5f),
                              IM_COL32(255, 255, 255, (int)(255 * std::min(1.0f, d.depth * 1.4f))), faces[d.face].label);
        }
    }
    if (hovered >= 0) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
        ImGui::SetTooltip("Look from the %s", faces[hovered].label);
        if (clicked) CommandView(sceneRenderer, faces[hovered].preset, false);
    }
}

bool ViewCubeHovered(ImVec2 center, float size) {
    ImVec2 mouse = ImGui::GetIO().MousePos;
    float dx = mouse.x - center.x, dy = mouse.y - center.y;
    return dx * dx + dy * dy < size * size * 0.8f;
}
