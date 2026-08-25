#include "Console.h"
#include "imgui/imgui.h"
#include "IconManager.h"
#include <vector>

static std::vector<std::string> consoleMessages;
static bool collapsed = false;
static float expandedHeight = 200.0f;

void LogMessage(const std::string& message) {
    consoleMessages.push_back(message);
}

void DrawConsoleWindow() {
    ImGui::Begin("Console", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

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
    ImGui::Text("Console");
    ImGui::Separator();

    if (!collapsed) {
        for (auto& msg : consoleMessages) {
            ImGui::TextWrapped("%s", msg.c_str());
        }

        if (ImGui::Button("Clear")) {
            consoleMessages.clear();
        }
    }

    ImGui::End();
}