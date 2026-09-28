#include "MenuBar.h"
#include "imgui/imgui.h"
#include "FileDialog.h"
#include <iostream>

// not the best, i know. Will fix later. (probably not)

void DrawMenuBar(GLFWwindow* sandbox_window, SceneRenderer& sceneRenderer) {
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 10.0f));
    ImGui::PushStyleColor(ImGuiCol_MenuBarBg, ImVec4(0.3f, 0.3f, 0.3f, 0.3f));
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("Import")) {
                std::string path = OpenFileDialog();
                if (!path.empty()) {
                    sceneRenderer.LoadModel(path);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("About")) {
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleColor(1);
    ImGui::PopStyleVar();
}