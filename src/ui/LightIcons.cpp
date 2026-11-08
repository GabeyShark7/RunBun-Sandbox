#include "ui/LightIcons.h"
#include "core/UISettings.h"
#include "render/SceneRenderer.h"
#include <cmath>

static bool Project(const glm::mat4& viewProj, const glm::vec3& p, ImVec2 viewMin, ImVec2 viewSize, ImVec2& out) {
    glm::vec4 clip = viewProj * glm::vec4(p, 1.0f);
    if (clip.w <= 0.0001f) return false;
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    out = ImVec2(viewMin.x + (ndc.x * 0.5f + 0.5f) * viewSize.x, viewMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewSize.y);
    return true;
}

static ImU32 Color(const glm::vec4& c, float alpha) {
    return ImGui::GetColorU32(ImVec4(std::min(c.r, 1.0f), std::min(c.g, 1.0f), std::min(c.b, 1.0f), alpha));
}

void DrawLightIcons(SceneRenderer& scene, ImDrawList* drawList, const glm::mat4& viewProj, ImVec2 viewMin, ImVec2 viewSize) {
    if (!scene.showLights) return;
    auto& objects = scene.GetObjects();
    ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);
    ImU32 outline = IM_COL32(15, 15, 18, 220);
    drawList->PushClipRect(viewMin, ImVec2(viewMin.x + viewSize.x, viewMin.y + viewSize.y), true);
    for (int i = 0; i < (int)objects.size(); i++) {
        const SceneObject& obj = objects[i];
        if (obj.look.light < 0) continue;
        ImVec2 p;
        if (!Project(viewProj, obj.position, viewMin, viewSize, p)) continue;
        bool picked = scene.IsSelected(i);
        bool hovered = i == scene.hoveredIndex;
        ImU32 fill = Color(obj.color, 1.0f);
        ImU32 ring = picked ? ImGui::GetColorU32(accent) : (hovered ? IM_COL32(255, 255, 255, 230) : outline);

        // Aim line for sun and spot
        if (obj.look.light != Light_Point) {
            glm::vec3 down = glm::mat3(obj.GetModelMatrix()) * glm::vec3(0.0f, 0.0f, -1.0f);
            if (glm::length(down) > 1e-6f) {
                ImVec2 tip;
                glm::vec3 end = obj.position + glm::normalize(down) * (obj.look.light == Light_Sun ? 1.2f : 0.9f);
                if (Project(viewProj, end, viewMin, viewSize, tip)) {
                    ImU32 lineColor = picked ? ImGui::GetColorU32(accent) : Color(obj.color, 0.75f);
                    drawList->AddLine(p, tip, IM_COL32(0, 0, 0, 120), 3.5f);
                    drawList->AddLine(p, tip, lineColor, 1.6f);
                    drawList->AddCircleFilled(tip, 3.0f, lineColor);
                }
            }
        }

        if (obj.look.light == Light_Sun) {
            for (int k = 0; k < 8; k++) {
                float a = k * 3.14159265f / 4.0f;
                ImVec2 d = ImVec2(std::cos(a), std::sin(a));
                drawList->AddLine(ImVec2(p.x + d.x * 8.5f, p.y + d.y * 8.5f), ImVec2(p.x + d.x * 13.0f, p.y + d.y * 13.0f), outline, 4.0f);
                drawList->AddLine(ImVec2(p.x + d.x * 8.5f, p.y + d.y * 8.5f), ImVec2(p.x + d.x * 12.5f, p.y + d.y * 12.5f), fill, 2.0f);
            }
            drawList->AddCircleFilled(p, 7.0f, ring);
            drawList->AddCircleFilled(p, 5.5f, fill);
        } else if (obj.look.light == Light_Point) {
            drawList->AddCircleFilled(p, 9.0f, ring);
            drawList->AddCircleFilled(p, 7.5f, IM_COL32(30, 30, 34, 235));
            drawList->AddCircleFilled(p, 4.5f, fill);
            drawList->AddRectFilled(ImVec2(p.x - 3.0f, p.y + 8.0f), ImVec2(p.x + 3.0f, p.y + 12.0f), ring, 1.5f);
        } else {
            ImVec2 pts[4] = { ImVec2(p.x - 4.0f, p.y - 7.0f), ImVec2(p.x + 4.0f, p.y - 7.0f), ImVec2(p.x + 9.0f, p.y + 6.0f), ImVec2(p.x - 9.0f, p.y + 6.0f) };
            drawList->AddConvexPolyFilled(pts, 4, fill);
            drawList->AddPolyline(pts, 4, ring, ImDrawFlags_Closed, 2.0f);
            drawList->AddLine(ImVec2(p.x - 6.0f, p.y + 9.0f), ImVec2(p.x + 6.0f, p.y + 9.0f), fill, 2.0f);
        }
        if (picked || hovered) {
            ImVec2 size = ImGui::CalcTextSize(obj.name.c_str());
            ImVec2 at = ImVec2(p.x - size.x * 0.5f, p.y + 16.0f);
            drawList->AddRectFilled(ImVec2(at.x - 4.0f, at.y - 1.0f), ImVec2(at.x + size.x + 4.0f, at.y + size.y + 1.0f), IM_COL32(0, 0, 0, 160), 4.0f);
            drawList->AddText(at, IM_COL32(255, 255, 255, 235), obj.name.c_str());
        }
    }
    drawList->PopClipRect();
}
