#include "ui/SceneWindow.h"
#include "ui/PanelHeader.h"
#include <cstdint>

static bool sceneHovered = false;
static PanelCollapseState collapseState = { false, 600.0f };

ImVec2 DrawSceneWindow(unsigned int textureID, Camera& camera, bool* open) {
    if (!ImGui::Begin("Scene", open, ImGuiWindowFlags_NoCollapse)) {
        sceneHovered = false;
        ImGui::End();
        return ImVec2(0, 0);
    }

    bool expanded = DrawPanelHeader(collapseState);

    sceneHovered = ImGui::IsWindowHovered();

    ImVec2 panelSize = ImVec2(0, 0);

    if (expanded) {
        panelSize = ImGui::GetContentRegionAvail();
        ImVec2 imageMin = ImGui::GetCursorScreenPos();
        ImVec2 imageMax = ImVec2(imageMin.x + panelSize.x, imageMin.y + panelSize.y);

        ImGui::Dummy(panelSize);
        float rounding = ImGui::GetStyle().WindowRounding * 0.75f;
        ImGui::GetWindowDrawList()->AddImageRounded((ImTextureID)(intptr_t)textureID, imageMin, imageMax, ImVec2(0, 1), ImVec2(1, 0), IM_COL32_WHITE, rounding);

        // Reset camera button sits on top of the scene image, top right corner
        const char* resetLabel = "Reset Camera";
        ImVec2 padding = ImGui::GetStyle().FramePadding;
        float buttonWidth = ImGui::CalcTextSize(resetLabel).x + padding.x * 2.0f;

        ImGui::SetCursorScreenPos(ImVec2(imageMax.x - buttonWidth - 10.0f, imageMin.y + 10.0f));
        if (ImGui::Button(resetLabel)) {
            camera.Reset();
        }
    }

    ImGui::End();

    return panelSize;
}

bool IsSceneHovered() {
    return sceneHovered;
}
