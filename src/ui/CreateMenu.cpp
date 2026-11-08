#include "ui/CreateMenu.h"
#include "imgui/imgui.h"
#include "render/SceneRenderer.h"

void DrawCreateMenuItems(SceneRenderer& sceneRenderer) {
    ImGui::TextDisabled("Create");
    ImGui::Separator();
    for (int i = 0; i < Primitive_Count; i++) {
        PrimitiveType type = (PrimitiveType)i;
        if (ImGui::MenuItem(GetPrimitiveName(type))) {
            sceneRenderer.CreatePrimitive(type);
        }
    }
    ImGui::Spacing();
    ImGui::TextDisabled("Lights");
    ImGui::Separator();
    if (ImGui::MenuItem("Sun")) sceneRenderer.CreateLight(Light_Sun);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Far away light from one direction, like daylight. Casts shadows.");
    if (ImGui::MenuItem("Point")) sceneRenderer.CreateLight(Light_Point);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Shines all around from one spot, like a bulb");
    if (ImGui::MenuItem("Spot")) sceneRenderer.CreateLight(Light_Spot);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("A cone of light, like a flashlight");
}
