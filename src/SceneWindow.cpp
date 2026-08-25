#include "SceneWindow.h"
#include "IconManager.h"

static bool sceneHovered = false;
static bool collapsed = false;
static float expandedHeight = 600.0f;

ImVec2 DrawSceneWindow(unsigned int textureID) {
    ImGui::Begin("Scene", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    unsigned int icon = GetDropdownIcon();
    if (ImGui::ImageButton("collapseToggle", (ImTextureID)(intptr_t)icon, ImVec2(16, 16))) {
        ImVec2 currentSize = ImGui::GetWindowSize();
        if (!collapsed) {
            expandedHeight = currentSize.y;
            collapsed = true;
            ImGui::SetWindowSize(ImVec2(currentSize.x, 40.0f));
        } else {
            collapsed = false;
            ImGui::SetWindowSize(ImVec2(currentSize.x, expandedHeight));
        }
    }
    ImGui::SameLine();
    ImGui::Text("Scene");
    ImGui::Separator();

    sceneHovered = ImGui::IsWindowHovered();

    ImVec2 panelSize = ImVec2(0, 0);

    if (!collapsed) {
        panelSize = ImGui::GetContentRegionAvail();
        ImGui::Image((ImTextureID)(intptr_t)textureID, panelSize, ImVec2(0, 1), ImVec2(1, 0));
    }

    ImGui::End();

    return panelSize;
}

bool IsSceneHovered() {
    return sceneHovered;
}