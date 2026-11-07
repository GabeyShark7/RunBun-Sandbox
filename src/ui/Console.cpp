#include "ui/Console.h"
#include "imgui/imgui.h"
#include "ui/PanelHeader.h"
#include <vector>

static std::vector<std::string> consoleMessages;
static PanelCollapseState collapseState = { false, 200.0f };

void LogMessage(const std::string& message) {
    consoleMessages.push_back(message);
}

void DrawConsoleWindow(bool* open) {
    if (!ImGui::Begin("Console", open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    if (DrawPanelHeader(collapseState)) {
        for (auto& msg : consoleMessages) {
            ImGui::TextWrapped("%s", msg.c_str());
        }

        if (ImGui::Button("Clear")) {
            consoleMessages.clear();
        }
    }

    ImGui::End();
}