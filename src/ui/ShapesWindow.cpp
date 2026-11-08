#include "core/Profiler.h"
#include "ui/ShapesWindow.h"
#include "ui/DockLayout.h"
#include "ui/PanelHeader.h"
#include "core/UISettings.h"
#include "render/SceneRenderer.h"
#include "imgui/imgui.h"
#include <cmath>

static PanelCollapseState collapseState;

struct ShapeTile {
    PrimitiveType type;
    bool hole;
    const char* label;
};

static const ShapeTile tiles[] = {
    { Primitive_Cube, false, "Box" },
    { Primitive_Cylinder, false, "Cylinder" },
    { Primitive_Sphere, false, "Sphere" },
    { Primitive_Cone, false, "Cone" },
    { Primitive_Pyramid, false, "Pyramid" },
    { Primitive_Roof, false, "Roof" },
    { Primitive_Wedge, false, "Wedge" },
    { Primitive_HalfSphere, false, "Half Sphere" },
    { Primitive_Tube, false, "Tube" },
    { Primitive_Torus, false, "Torus" },
    { Primitive_Capsule, false, "Capsule" },
    { Primitive_Plane, false, "Plane" },
    { Primitive_Cube, true, "Box Hole" },
    { Primitive_Cylinder, true, "Round Hole" },
};

static void DrawIcon(ImDrawList* d, PrimitiveType type, ImVec2 c, float s, ImU32 color, bool hole) {
    float t = hole ? 1.5f : 2.0f;
    ImU32 fill = hole ? IM_COL32(0, 0, 0, 0) : (color & 0x00FFFFFF) | 0x50000000;
    switch (type) {
    case Primitive_Cube:
        d->AddRectFilled(ImVec2(c.x - s, c.y - s * 0.6f), ImVec2(c.x + s * 0.6f, c.y + s), fill);
        d->AddRect(ImVec2(c.x - s, c.y - s * 0.6f), ImVec2(c.x + s * 0.6f, c.y + s), color, 0.0f, 0, t);
        d->AddLine(ImVec2(c.x - s, c.y - s * 0.6f), ImVec2(c.x - s * 0.6f, c.y - s), color, t);
        d->AddLine(ImVec2(c.x + s * 0.6f, c.y - s * 0.6f), ImVec2(c.x + s, c.y - s), color, t);
        d->AddLine(ImVec2(c.x - s * 0.6f, c.y - s), ImVec2(c.x + s, c.y - s), color, t);
        d->AddLine(ImVec2(c.x + s, c.y - s), ImVec2(c.x + s, c.y + s * 0.6f), color, t);
        d->AddLine(ImVec2(c.x + s * 0.6f, c.y + s), ImVec2(c.x + s, c.y + s * 0.6f), color, t);
        break;
    case Primitive_Cylinder:
    case Primitive_Tube:
        d->AddRectFilled(ImVec2(c.x - s * 0.8f, c.y - s * 0.7f), ImVec2(c.x + s * 0.8f, c.y + s * 0.7f), fill);
        d->AddEllipse(ImVec2(c.x, c.y - s * 0.7f), ImVec2(s * 0.8f, s * 0.3f), color, 0.0f, 0, t);
        d->AddLine(ImVec2(c.x - s * 0.8f, c.y - s * 0.7f), ImVec2(c.x - s * 0.8f, c.y + s * 0.7f), color, t);
        d->AddLine(ImVec2(c.x + s * 0.8f, c.y - s * 0.7f), ImVec2(c.x + s * 0.8f, c.y + s * 0.7f), color, t);
        d->PathArcTo(ImVec2(c.x, c.y + s * 0.7f), s * 0.8f, 0.0f, 3.14159f, 16);
        d->PathStroke(color, 0, t);
        if (type == Primitive_Tube) d->AddEllipse(ImVec2(c.x, c.y - s * 0.7f), ImVec2(s * 0.45f, s * 0.15f), color, 0.0f, 0, t);
        break;
    case Primitive_Sphere:
        d->AddCircleFilled(c, s * 0.9f, fill);
        d->AddCircle(c, s * 0.9f, color, 0, t);
        d->AddEllipse(c, ImVec2(s * 0.9f, s * 0.3f), color, 0.0f, 0, 1.0f);
        break;
    case Primitive_HalfSphere:
        d->PathArcTo(ImVec2(c.x, c.y + s * 0.4f), s * 0.9f, 3.14159f, 6.28318f, 20);
        d->PathFillConvex(fill);
        d->PathArcTo(ImVec2(c.x, c.y + s * 0.4f), s * 0.9f, 3.14159f, 6.28318f, 20);
        d->PathStroke(color, ImDrawFlags_Closed, t);
        break;
    case Primitive_Cone:
    case Primitive_Pyramid:
        d->AddTriangleFilled(ImVec2(c.x, c.y - s), ImVec2(c.x - s * 0.9f, c.y + s * 0.8f), ImVec2(c.x + s * 0.9f, c.y + s * 0.8f), fill);
        d->AddTriangle(ImVec2(c.x, c.y - s), ImVec2(c.x - s * 0.9f, c.y + s * 0.8f), ImVec2(c.x + s * 0.9f, c.y + s * 0.8f), color, t);
        if (type == Primitive_Pyramid) d->AddLine(ImVec2(c.x, c.y - s), ImVec2(c.x + s * 0.2f, c.y + s * 0.8f), color, 1.0f);
        break;
    case Primitive_Roof: {
        ImVec2 p[5] = { ImVec2(c.x - s, c.y + s * 0.8f), ImVec2(c.x + s, c.y + s * 0.8f), ImVec2(c.x + s, c.y), ImVec2(c.x, c.y - s), ImVec2(c.x - s, c.y) };
        d->AddConvexPolyFilled(p, 5, fill);
        d->AddPolyline(p, 5, color, ImDrawFlags_Closed, t);
        break;
    }
    case Primitive_Wedge:
        d->AddTriangleFilled(ImVec2(c.x - s, c.y + s * 0.8f), ImVec2(c.x + s, c.y + s * 0.8f), ImVec2(c.x + s, c.y - s), fill);
        d->AddTriangle(ImVec2(c.x - s, c.y + s * 0.8f), ImVec2(c.x + s, c.y + s * 0.8f), ImVec2(c.x + s, c.y - s), color, t);
        break;
    case Primitive_Torus:
        d->AddCircle(c, s * 0.9f, color, 0, t);
        d->AddCircle(c, s * 0.4f, color, 0, t);
        break;
    case Primitive_Capsule:
        d->AddRectFilled(ImVec2(c.x - s * 0.45f, c.y - s), ImVec2(c.x + s * 0.45f, c.y + s), fill, s * 0.45f);
        d->AddRect(ImVec2(c.x - s * 0.45f, c.y - s), ImVec2(c.x + s * 0.45f, c.y + s), color, s * 0.45f, 0, t);
        break;
    default: {
        ImVec2 p[4] = { ImVec2(c.x - s, c.y + s * 0.4f), ImVec2(c.x + s * 0.4f, c.y + s * 0.4f), ImVec2(c.x + s, c.y - s * 0.4f), ImVec2(c.x - s * 0.4f, c.y - s * 0.4f) };
        d->AddConvexPolyFilled(p, 4, fill);
        d->AddPolyline(p, 4, color, ImDrawFlags_Closed, t);
        break;
    }
    }
}

static glm::vec3 ViewCenterOnGround(SceneRenderer& sceneRenderer) {
    Camera& camera = sceneRenderer.GetCamera();
    if (camera.front.z < -0.05f) {
        float t = -camera.position.z / camera.front.z;
        if (t > 0.0f && t < 200.0f) return camera.position + camera.front * t;
    }
    glm::vec3 ahead = camera.position + camera.front * 5.0f;
    ahead.z = 0.0f;
    return ahead;
}

static glm::vec3 SnapPoint(glm::vec3 p) {
    UISettings& settings = GetUISettings();
    if (!settings.snapEnabled) return p;
    float step = settings.snapUnitMm / 1000.0f;
    if (step <= 0.0f) return p;
    p.x = roundf(p.x / step) * step;
    p.y = roundf(p.y / step) * step;
    return p;
}

void AddShapeAt(SceneRenderer& sceneRenderer, int payload, const glm::vec3* point) {
    PrimitiveType type = (PrimitiveType)(payload & 0xFF);
    bool hole = (payload >> 8) != 0;
    glm::vec3 at = SnapPoint(point ? *point : ViewCenterOnGround(sceneRenderer));
    sceneRenderer.CreatePrimitive(type, &at, hole);
}

void DrawShapesWindow(SceneRenderer& sceneRenderer, bool* open) {
    PROFILE("DrawShapesWindow");
    if (!ImGui::Begin(PanelId("Shapes"), open, PanelFlags())) {
        ImGui::End();
        return;
    }
    if (DrawPanelHeader(collapseState)) {
        if (ShowHelpText()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("Click a shape to add it, or drag it into the Scene. Holes cut into shapes when you Group them (Ctrl+G).");
            ImGui::PopStyleColor();
        }
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float avail = ImGui::GetContentRegionAvail().x;
        int columns = avail > 300.0f ? 4 : (avail > 200.0f ? 3 : 2);
        float tile = fmaxf((avail - spacing * (columns - 1)) / columns, 24.0f);
        ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);
        for (int i = 0; i < IM_ARRAYSIZE(tiles); i++) {
            const ShapeTile& shape = tiles[i];
            if (i % columns != 0) ImGui::SameLine();
            ImGui::PushID(i);
            ImVec2 min = ImGui::GetCursorScreenPos();
            float height = tile * 0.82f;
            bool clicked = ImGui::Button("##tile", ImVec2(tile, height));
            int payload = shape.type | (shape.hole ? 0x100 : 0);
            if (ImGui::BeginDragDropSource()) {
                ImGui::SetDragDropPayload("RUNBUN_SHAPE", &payload, sizeof(payload));
                ImGui::Text("%s", shape.label);
                ImGui::EndDragDropSource();
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(shape.hole ? "%s: cuts into shapes when grouped" : "%s", shape.label);
            ImDrawList* d = ImGui::GetWindowDrawList();
            ImU32 color = shape.hole ? IM_COL32(170, 175, 185, 255) : ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 1.0f));
            float iconSize = fminf(tile, height) * 0.22f;
            DrawIcon(d, shape.type, ImVec2(min.x + tile * 0.5f, min.y + height * 0.40f), iconSize, color, shape.hole);
            ImVec2 textSize = ImGui::CalcTextSize(shape.label);
            float scale = textSize.x > tile - 6.0f ? (tile - 6.0f) / textSize.x : 1.0f;
            if (ImGui::GetFontSize() * scale >= 4.0f) d->AddText(ImGui::GetFont(), ImGui::GetFontSize() * scale, ImVec2(min.x + (tile - textSize.x * scale) * 0.5f, min.y + height - ImGui::GetFontSize() * scale - 6.0f),
                       ImGui::GetColorU32(ImGuiCol_Text), shape.label);
            if (clicked) AddShapeAt(sceneRenderer, payload, nullptr);
            ImGui::PopID();
        }
    }
    ImGui::End();
}
