#include "ui/CreateMenu.h"
#include "imgui/imgui.h"

void DrawCreateMenuItems(SceneRenderer& sceneRenderer) {
    ImGui::TextDisabled("Create");
    ImGui::Separator();
    for (int i = 0; i < Primitive_Count; i++) {
        PrimitiveType type = (PrimitiveType)i;
        if (ImGui::MenuItem(GetPrimitiveName(type))) {
            sceneRenderer.CreatePrimitive(type);
        }
    }
}
