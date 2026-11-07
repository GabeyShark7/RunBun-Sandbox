#include "ui/MenuBar.h"
#include "imgui/imgui.h"
#include "ui/FileDialog.h"
#include "core/UISettings.h"
#include "imgui/imgui_internal.h"
#include <iostream>

// not the best, i know. Will fix later. (probably not)

void DrawMenuBar(GLFWwindow* sandbox_window, SceneRenderer& sceneRenderer) {
    UISettings& settings = GetUISettings();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 10.0f));
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
        if (ImGui::BeginMenu("View")) {
            bool changed = false;
            changed |= ImGui::MenuItem("Scene", nullptr, &settings.showScene);
            changed |= ImGui::MenuItem("Objects", nullptr, &settings.showObjects);
            changed |= ImGui::MenuItem("Properties", nullptr, &settings.showProperties);
            changed |= ImGui::MenuItem("Console", nullptr, &settings.showConsole);
            ImGui::Separator();
            if (ImGui::MenuItem("Float All Panels")) {
                settings.showScene = settings.showObjects = settings.showProperties = settings.showConsole = true;
                settings.layoutMode = LayoutMode_Floating;
                settings.resetLayout = true;
                changed = true;
            }
            if (ImGui::MenuItem("Dock All Panels")) {
                settings.showScene = settings.showObjects = settings.showProperties = settings.showConsole = true;
                settings.layoutMode = LayoutMode_Docked;
                settings.resetLayout = true;
                changed = true;
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Preferences...")) {
                settings.showPreferences = true;
            }
            if (changed) ImGui::MarkIniSettingsDirty();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("About")) {
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleVar();
}
