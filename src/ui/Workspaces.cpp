#include "ui/Workspaces.h"
#include "ui/DockLayout.h"
#include "ui/EditMode.h"
#include "ui/SceneGizmo.h"
#include "core/UISettings.h"
#include "render/SceneRenderer.h"
#include "imgui/imgui_internal.h"

static int returnWorkspace = Workspace_Layout;

bool WorkspaceAllowed(int workspace) {
    return workspace == Workspace_Layout || GetUISettings().workMode != WorkMode_Tinker;
}

void SetWorkspace(SceneRenderer& sceneRenderer, int workspace) {
    UISettings& settings = GetUISettings();
    if (workspace < 0 || workspace >= Workspace_Count || !WorkspaceAllowed(workspace)) return;
    if (workspace == settings.workspace || IsSceneTransformActive()) return;

    if (settings.workspace == Workspace_Edit) ExitEditMode();
    else returnWorkspace = settings.workspace;

    if (workspace == Workspace_Edit && !IsEditModeActive() && sceneRenderer.selectedIndex >= 0)
        EnterEditMode(sceneRenderer, sceneRenderer.selectedIndex);

    settings.workspace = workspace;
    if (workspace == Workspace_Shading) settings.viewStyle = 1;
    ImGui::MarkIniSettingsDirty();
}

void ToggleEditWorkspace(SceneRenderer& sceneRenderer) {
    UISettings& settings = GetUISettings();
    if (!WorkspaceAllowed(Workspace_Edit)) return;
    if (settings.workspace != Workspace_Edit) {
        SetWorkspace(sceneRenderer, Workspace_Edit);
    } else if (IsEditModeActive()) {
        SetWorkspace(sceneRenderer, returnWorkspace);
    } else if (sceneRenderer.selectedIndex >= 0) {
        EnterEditMode(sceneRenderer, sceneRenderer.selectedIndex);
    }
}

void DrawWorkspaceTabs(SceneRenderer& sceneRenderer) {
    UISettings& settings = GetUISettings();
    static const char* tips[Workspace_Count] = {
        "Everything at once  (Ctrl+1)",
        "Reshape one object with lots of room  (Ctrl+2, or E with an object picked)",
        "Colors, images and how they wrap  (Ctrl+3)",
        "Materials, noise, lights and the background  (Ctrl+4)",
    };
    ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);
    ImGui::PushStyleVar(ImGuiStyleVar_SelectableTextAlign, ImVec2(0.5f, 0.5f));
    for (int i = 0; i < Workspace_Count; i++) {
        bool current = settings.workspace == i;
        bool allowed = WorkspaceAllowed(i);
        const char* title = WorkspaceTitle(i);
        float width = ImGui::CalcTextSize(title).x + ImGui::GetStyle().ItemSpacing.x * 2.0f;
        ImGui::PushID(i);
        ImGui::BeginDisabled(!allowed);
        if (current) {
            ImGui::PushStyleColor(ImGuiCol_Header, accent);
            ImGui::PushStyleColor(ImGuiCol_HeaderHovered, accent);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
        }
        if (ImGui::Selectable(title, current, 0, ImVec2(width, 0.0f))) SetWorkspace(sceneRenderer, i);
        if (current) ImGui::PopStyleColor(3);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("%s", allowed ? tips[i] : "Needs Mix mode. Change it in View > Preferences > General");
        ImGui::PopID();
    }
    ImGui::PopStyleVar();
}

void UpdateWorkspaceKeys(SceneRenderer& sceneRenderer) {
    UISettings& settings = GetUISettings();
    if (!WorkspaceAllowed(settings.workspace)) {
        ExitEditMode();
        settings.workspace = Workspace_Layout;
    }

    // New pick starts editing
    static int lastSelected = -1;
    int selected = sceneRenderer.selectedIndex;
    if (settings.workspace == Workspace_Edit && selected >= 0 && selected != lastSelected && !IsEditModeActive() && !IsSceneTransformActive())
        EnterEditMode(sceneRenderer, selected);
    lastSelected = selected;

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || !io.KeyCtrl || io.KeyShift || io.KeyAlt) return;
    for (int i = 0; i < Workspace_Count; i++) {
        if (ImGui::IsKeyPressed((ImGuiKey)(ImGuiKey_1 + i), false)) SetWorkspace(sceneRenderer, i);
    }
}
