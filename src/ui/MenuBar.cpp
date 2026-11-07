#include "ui/MenuBar.h"
#include "imgui/imgui.h"
#include "ui/FileDialog.h"
#include "core/UISettings.h"
#include "imgui/imgui_internal.h"
#include "ui/EditMode.h"
#include "ui/RoomWindow.h"
#include "ui/UpdateWindow.h"
#include <iostream>

// not the best, i know. Will fix later. (probably not)

static void SetWorkMode(UISettings& settings, int mode) {
    if (settings.workMode == mode) return;
    settings.workMode = mode;
    if (mode == WorkMode_Tinker) ExitEditMode();
    ImGui::MarkIniSettingsDirty();
}

static float DrawWorkModeSwitcher(UISettings& settings) {
    const char* names[2] = { "Tinker", "Mix" };
    const char* tips[2] = {
        "Simple shapes and click-and-drag building",
        "Simple tools plus mesh editing and shortcuts",
    };

    ImGuiStyle& style = ImGui::GetStyle();
    ImVec2 padding = ImVec2(12.0f, 4.0f);
    float total = 0.0f;
    for (const char* name : names) total += ImGui::CalcTextSize(name).x + padding.x * 2.0f;
    total += 4.0f;

    float barHeight = ImGui::GetWindowHeight();
    float buttonHeight = ImGui::GetFontSize() + padding.y * 2.0f;
    float left = ImGui::GetWindowWidth() - total - 16.0f;
    ImGui::SetCursorPos(ImVec2(left, (barHeight - buttonHeight) * 0.5f));

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, padding);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, style.ItemSpacing.y));
    for (int i = 0; i < 2; i++) {
        bool active = settings.workMode == i;
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        if (ImGui::Button(names[i])) SetWorkMode(settings, i);
        if (active) ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tips[i]);
        if (i < 1) ImGui::SameLine();
    }
    ImGui::PopStyleVar(2);
    return left;
}

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
            changed |= ImGui::MenuItem("Edit", nullptr, &settings.showEdit, settings.workMode != WorkMode_Tinker);
            ImGui::Separator();
            changed |= ImGui::MenuItem("Grid", nullptr, &settings.showGrid);
            ImGui::Separator();
            if (ImGui::MenuItem("Float All Panels")) {
                settings.showScene = settings.showObjects = settings.showProperties = settings.showConsole = settings.showEdit = true;
                settings.layoutMode = LayoutMode_Floating;
                settings.resetLayout = true;
                changed = true;
            }
            if (ImGui::MenuItem("Dock All Panels")) {
                settings.showScene = settings.showObjects = settings.showProperties = settings.showConsole = settings.showEdit = true;
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
        if (ImGui::BeginMenu("Room")) {
            bool inRoom = IsInRoom();
            if (ImGui::MenuItem("Create", nullptr, false, !inRoom)) OpenRoomCreate();
            if (ImGui::MenuItem("Join", nullptr, false, !inRoom)) OpenRoomJoin();
            ImGui::Separator();
            if (ImGui::MenuItem("Show Room")) ShowRoomWindow();
            if (ImGui::MenuItem(IsHostingRoom() ? "Close Room" : "Leave Room", nullptr, false, inRoom)) LeaveRoom();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("About")) {
            if (ImGui::MenuItem("Updates...")) ShowUpdateWindow();
            ImGui::EndMenu();
        }
        float switcherLeft = DrawWorkModeSwitcher(settings);
        DrawUpdateBadge(switcherLeft - 10.0f);
        ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleVar();
}
