#include "ui/MenuBar.h"
#include "ui/RenderExport.h"
#include "imgui/imgui.h"
#include "ui/FileDialog.h"
#include "core/UISettings.h"
#include "imgui/imgui_internal.h"
#include "ui/RoomWindow.h"
#include "ui/UpdateWindow.h"
#include "ui/HelpWindow.h"
#include "ui/SceneCommands.h"
#include "ui/Workspaces.h"
#include "ui/DockLayout.h"
#include "render/SceneRenderer.h"
#include <cstdio>
#include <iostream>

// not the best, i know. Will fix later. (probably not)

static void ShowAllPanels(UISettings& settings) {
    settings.showScene = settings.showObjects = settings.showProperties = settings.showEdit = settings.showMessages = true;
    settings.showShapes = settings.showMods = true;
}

void DrawMenuBar(GLFWwindow* sandbox_window, SceneRenderer& sceneRenderer) {
    UISettings& settings = GetUISettings();

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10.0f, 10.0f));
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Scene", "Ctrl+N")) CommandNew();
            if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) CommandOpen();
            ImGui::Separator();
            if (ImGui::MenuItem("Save Scene", "Ctrl+S")) CommandSave(sceneRenderer);
            if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) CommandSaveAs(sceneRenderer);
            ImGui::Separator();
            if (ImGui::MenuItem("Export Models...", "Ctrl+E")) OpenExportModels();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save the picked objects (or everything) as GLB, FBX, OBJ, STL and more, with their images");
            if (ImGui::MenuItem("Render Image...", "F12")) OpenRenderImage();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save a picture of the Scene with materials, lights and shadows");
            if (ImGui::MenuItem("Import Model...")) {
                std::string path = OpenFileDialog();
                if (!path.empty()) {
                    sceneRenderer.LoadModel(path);
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Edit")) {
            bool objectsPicked = HasObjectSelection(sceneRenderer);
            if (ImGui::MenuItem("Undo", "Ctrl+Z", false, CanUndo())) CommandUndo(sceneRenderer);
            if (ImGui::MenuItem("Redo", "Ctrl+Y", false, CanRedo())) CommandRedo(sceneRenderer);
            ImGui::Separator();
            if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, objectsPicked)) CommandDuplicate(sceneRenderer, false);
            if (ImGui::MenuItem("Delete", "Del", false, objectsPicked)) CommandDelete(sceneRenderer);
            if (ImGui::MenuItem("Join Objects", "J", false, objectsPicked && sceneRenderer.GetSelection().size() > 1)) CommandJoin(sceneRenderer);
            if (ImGui::MenuItem("Rename", "F2", false, objectsPicked)) CommandRename(sceneRenderer);
            ImGui::Separator();
            if (ImGui::MenuItem("Group", "Ctrl+G", false, objectsPicked && sceneRenderer.GetSelection().size() > 1)) CommandGroup(sceneRenderer);
            if (ImGui::MenuItem("Ungroup", "Ctrl+Shift+G", false, objectsPicked)) CommandUngroup(sceneRenderer);
            if (ImGui::MenuItem("Solid or Hole", "H", false, objectsPicked)) CommandToggleHole(sceneRenderer);
            if (ImGui::MenuItem("Drop to Ground", "D", false, objectsPicked)) CommandDrop(sceneRenderer);
            if (ImGui::MenuItem("Align...", "L", false, objectsPicked && sceneRenderer.GetSelection().size() > 1)) CommandAlign();
            if (ImGui::MenuItem("Mirror...", "M", false, objectsPicked)) CommandMirror();
            ImGui::Separator();
            if (ImGui::MenuItem("Select All", "A", false, !sceneRenderer.GetObjects().empty())) CommandSelectAll(sceneRenderer);
            if (ImGui::MenuItem("Focus on Selected", "F")) CommandFocus(sceneRenderer);
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            bool changed = false;
            ImGui::TextDisabled("Tabs");
            for (int i = 0; i < Workspace_Count; i++) {
                char keys[16];
                snprintf(keys, sizeof(keys), "Ctrl+%d", i + 1);
                if (ImGui::MenuItem(WorkspaceTitle(i), keys, settings.workspace == i, WorkspaceAllowed(i))) SetWorkspace(sceneRenderer, i);
            }
            ImGui::Separator();
            bool layoutTab = settings.workspace == Workspace_Layout;
            ImGui::TextDisabled(layoutTab ? "Panels" : "Panels (Layout tab)");
            ImGui::BeginDisabled(!layoutTab);
            changed |= ImGui::MenuItem("Scene", nullptr, &settings.showScene);
            changed |= ImGui::MenuItem("Objects", nullptr, &settings.showObjects);
            changed |= ImGui::MenuItem("Properties", nullptr, &settings.showProperties);
            changed |= ImGui::MenuItem("Shapes", nullptr, &settings.showShapes);
            changed |= ImGui::MenuItem("Object Mods", settings.workMode == WorkMode_Tinker ? "Mix mode" : nullptr, &settings.showMods, settings.workMode != WorkMode_Tinker);
            changed |= ImGui::MenuItem("Edit##EditPanel", settings.workMode == WorkMode_Tinker ? "Mix mode" : nullptr, &settings.showEdit, settings.workMode != WorkMode_Tinker);
            changed |= ImGui::MenuItem("Messages", nullptr, &settings.showMessages);
            changed |= ImGui::MenuItem("Console", nullptr, &settings.showConsole);
            ImGui::EndDisabled();
            ImGui::Separator();
            changed |= ImGui::MenuItem("Grid", nullptr, &settings.showGrid);
            changed |= ImGui::MenuItem("Statistics", nullptr, &settings.showStatistics);
            ImGui::Separator();
            ImGui::TextDisabled("Layout");
            changed |= ImGui::MenuItem("Lock Layout", nullptr, &settings.lockLayout);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stops panels from being dragged around by accident");
            if (ImGui::MenuItem("Reset Layout")) {
                ShowAllPanels(settings);
                settings.layoutMode = LayoutMode_Docked;
                settings.resetLayout = true;
                changed = true;
            }
            if (ImGui::MenuItem("Float All Panels")) {
                ShowAllPanels(settings);
                settings.layoutMode = LayoutMode_Floating;
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
            if (ImGui::MenuItem("Create Room", nullptr, false, !inRoom)) OpenRoomCreate();
            if (ImGui::MenuItem("Join Room...", nullptr, false, !inRoom)) OpenRoomJoin();
            ImGui::Separator();
            if (ImGui::MenuItem("Show Room Panel")) {
                SetWorkspace(sceneRenderer, Workspace_Layout);
                ShowRoomWindow();
            }
            if (ImGui::MenuItem("Show Messages", nullptr, false, !settings.showMessages || settings.workspace != Workspace_Layout)) {
                SetWorkspace(sceneRenderer, Workspace_Layout);
                settings.showMessages = true;
                ImGui::MarkIniSettingsDirty();
            }
            if (ImGui::MenuItem(IsHostingRoom() ? "Close Room" : "Leave Room", nullptr, false, inRoom)) LeaveRoom();
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("About")) {
            if (ImGui::MenuItem("Help and Shortcuts", "F1")) ShowHelpWindow();
            if (ImGui::MenuItem("Updates...")) ShowUpdateWindow();
            ImGui::EndMenu();
        }

        ImGui::Separator();
        DrawWorkspaceTabs(sceneRenderer);
        ImGui::Separator();

        ImGui::TextDisabled("%s mode", settings.workMode == WorkMode_Tinker ? "Tinker" : "Mix");
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Change this in View > Preferences > General");

        DrawUpdateBadge(ImGui::GetWindowWidth() - 16.0f);
        ImGui::EndMainMenuBar();
    }
    ImGui::PopStyleVar();

    if (ImGui::IsKeyPressed(ImGuiKey_F1, false)) ShowHelpWindow();
}
