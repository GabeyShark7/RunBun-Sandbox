#include "ui/DockLayout.h"
#include "imgui/imgui_internal.h"
#include <cstring>

static const int currentLayoutVersion = 2;
static bool floatingPlacementPending = false;

struct FloatingRect {
    const char* name;
    float x, y, w, h;
};

static const FloatingRect floatingRects[] = {
    { "Objects",    0.015f, 0.03f, 0.180f, 0.62f },
    { "Scene",      0.210f, 0.03f, 0.570f, 0.66f },
    { "Properties", 0.795f, 0.03f, 0.190f, 0.62f },
    { "Console",    0.210f, 0.72f, 0.570f, 0.25f },
};

static void ClearDockSpace(ImGuiID dockId) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::DockBuilderRemoveNodeDockedWindows(dockId, true);
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockId, viewport->WorkSize);
}

static void BuildDockedLayout(ImGuiID dockId) {
    ClearDockSpace(dockId);

    ImGuiID center = dockId;
    ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.20f, nullptr, &center);
    ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.26f, nullptr, &center);
    ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.25f, nullptr, &center);

    ImGui::DockBuilderDockWindow("Scene", center);
    ImGui::DockBuilderDockWindow("Objects", left);
    ImGui::DockBuilderDockWindow("Properties", right);
    ImGui::DockBuilderDockWindow("Console", bottom);
    ImGui::DockBuilderFinish(dockId);
}

static void BuildFloatingLayout(ImGuiID dockId) {
    ClearDockSpace(dockId);
    ImGui::DockBuilderFinish(dockId);
    floatingPlacementPending = true;
}

void DrawMainDockSpace(UISettings& settings) {
    ImGuiID dockId = ImGui::GetID("RunBunDockSpace");

    bool outdated = settings.layoutVersion < currentLayoutVersion;
    if (settings.resetLayout || outdated || ImGui::DockBuilderGetNode(dockId) == nullptr) {
        if (settings.layoutMode == LayoutMode_Docked)
            BuildDockedLayout(dockId);
        else
            BuildFloatingLayout(dockId);

        settings.resetLayout = false;
        settings.layoutVersion = currentLayoutVersion;
        ImGui::MarkIniSettingsDirty();
    }

    ImGui::DockSpaceOverViewport(dockId, ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
}

void PreparePanel(const char* name) {
    if (!floatingPlacementPending) return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    for (const FloatingRect& rect : floatingRects) {
        if (strcmp(rect.name, name) != 0) continue;

        ImVec2 pos = ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * rect.x, viewport->WorkPos.y + viewport->WorkSize.y * rect.y);
        ImVec2 size = ImVec2(viewport->WorkSize.x * rect.w, viewport->WorkSize.y * rect.h);
        ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        return;
    }
}

void FinishPanelLayout() {
    floatingPlacementPending = false;
}
