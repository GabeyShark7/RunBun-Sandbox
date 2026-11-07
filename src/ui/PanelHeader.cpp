#include "ui/PanelHeader.h"
#include "ui/IconManager.h"
#include "imgui/imgui.h"
#include <cstdint>

bool DrawPanelHeader(PanelCollapseState& state) {
    unsigned int icon = GetDropdownIcon();
    ImVec2 uv0 = state.collapsed ? ImVec2(0, 1) : ImVec2(0, 0);
    ImVec2 uv1 = state.collapsed ? ImVec2(1, 0) : ImVec2(1, 1);

    bool toggled = ImGui::ImageButton("collapseToggle", (ImTextureID)(intptr_t)icon, ImVec2(16, 16), uv0, uv1);
    ImGui::Separator();

    if (toggled) {
        state.collapsed = !state.collapsed;

        if (!ImGui::IsWindowDocked()) {
            ImVec2 size = ImGui::GetWindowSize();
            if (state.collapsed) {
                state.expandedHeight = size.y;
                ImGui::SetWindowSize(ImVec2(size.x, ImGui::GetCursorPosY() + ImGui::GetStyle().WindowPadding.y));
            } else {
                ImGui::SetWindowSize(ImVec2(size.x, state.expandedHeight));
            }
        }
    }

    return !state.collapsed;
}
