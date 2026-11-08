#include "ui/DockLayout.h"
#include "imgui/imgui_internal.h"
#include <cstring>
#include <string>

static const int currentLayoutVersion = 3;
static bool layoutLocked = false;
static bool floatingPlacementPending = false;
static int activeWorkspace = Workspace_Layout;
static bool workspaceStale[Workspace_Count] = {};
static bool workspaceReset[Workspace_Count] = {};

struct FloatingRect {
    const char* name;
    float x, y, w, h;
};

static const FloatingRect layoutRects[] = {
    { "Objects",    0.015f, 0.03f, 0.180f, 0.30f },
    { "Scene",      0.210f, 0.03f, 0.570f, 0.66f },
    { "Properties", 0.795f, 0.03f, 0.190f, 0.62f },
    { "Console",    0.210f, 0.72f, 0.570f, 0.25f },
    { "Edit",       0.015f, 0.36f, 0.180f, 0.61f },
    { "Messages",   0.210f, 0.72f, 0.570f, 0.25f },
    { "Room",       0.795f, 0.67f, 0.190f, 0.30f },
    { "Shapes",     0.015f, 0.36f, 0.180f, 0.61f },
    { "Object Mods", 0.795f, 0.03f, 0.190f, 0.62f },
};

static const FloatingRect editRects[] = {
    { "Edit",  0.015f, 0.03f, 0.220f, 0.94f },
    { "Scene", 0.250f, 0.03f, 0.735f, 0.94f },
};

static const FloatingRect texturingRects[] = {
    { "Objects",  0.015f, 0.03f, 0.160f, 0.30f },
    { "UV Tools", 0.015f, 0.36f, 0.160f, 0.61f },
    { "Scene",    0.190f, 0.03f, 0.500f, 0.94f },
    { "Material", 0.705f, 0.03f, 0.280f, 0.40f },
    { "UV",       0.705f, 0.45f, 0.280f, 0.52f },
};

struct WorkspaceInfo {
    const char* title;
    const char* dockName;
    const char* suffix;
    const FloatingRect* rects;
    int rectCount;
};

static const WorkspaceInfo workspaces[Workspace_Count] = {
    { "Layout",    "RunBunDockSpace",          "",             layoutRects,    IM_ARRAYSIZE(layoutRects) },
    { "Edit",      "RunBunDockSpaceEdit",      "##Edit",       editRects,      IM_ARRAYSIZE(editRects) },
    { "Texturing", "RunBunDockSpaceTexturing2", "##Texturing",  texturingRects, IM_ARRAYSIZE(texturingRects) },
};

int ActiveWorkspace() {
    return activeWorkspace;
}

const char* WorkspaceTitle(int workspace) {
    return (workspace >= 0 && workspace < Workspace_Count) ? workspaces[workspace].title : "";
}

const char* PanelId(const char* name) {
    if (activeWorkspace == Workspace_Layout) return name;
    static std::string ids[Workspace_Count][8];
    static const char* names[Workspace_Count][8] = {};
    for (int i = 0; i < 8; i++) {
        if (names[activeWorkspace][i] && strcmp(names[activeWorkspace][i], name) == 0) return ids[activeWorkspace][i].c_str();
        if (!names[activeWorkspace][i]) {
            names[activeWorkspace][i] = name;
            ids[activeWorkspace][i] = std::string(name) + workspaces[activeWorkspace].suffix;
            return ids[activeWorkspace][i].c_str();
        }
    }
    return name;
}

static ImGuiID DockIdFor(int workspace) {
    return ImGui::GetID(workspaces[workspace].dockName);
}

static void ClearDockSpace(ImGuiID dockId) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::DockBuilderRemoveNodeDockedWindows(dockId, true);
    ImGui::DockBuilderRemoveNode(dockId);
    ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockId, viewport->WorkSize);
}

static void BuildLayoutWorkspace(ImGuiID dockId) {
    ImGuiID center = dockId;
    ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.19f, nullptr, &center);
    ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.24f, nullptr, &center);
    ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.24f, nullptr, &center);
    ImGuiID leftBottom = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.58f, nullptr, &left);

    // Same spot means tabs
    ImGui::DockBuilderDockWindow("Scene", center);
    ImGui::DockBuilderDockWindow("Objects", left);
    ImGui::DockBuilderDockWindow("Shapes", leftBottom);
    ImGui::DockBuilderDockWindow("Edit", leftBottom);
    ImGui::DockBuilderDockWindow("Properties", right);
    ImGui::DockBuilderDockWindow("Room", right);
    ImGui::DockBuilderDockWindow("Object Mods", right);
    ImGui::DockBuilderDockWindow("Messages", bottom);
    ImGui::DockBuilderDockWindow("Console", bottom);
}

static void BuildEditWorkspace(ImGuiID dockId) {
    ImGuiID center = dockId;
    ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.22f, nullptr, &center);
    ImGui::DockBuilderDockWindow("Scene##Edit", center);
    ImGui::DockBuilderDockWindow("Edit##Edit", left);
}

static void BuildTexturingWorkspace(ImGuiID dockId) {
    ImGuiID center = dockId;
    ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.18f, nullptr, &center);
    ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.40f, nullptr, &center);
    ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.66f, nullptr, &right);
    ImGuiID leftBottom = ImGui::DockBuilderSplitNode(left, ImGuiDir_Down, 0.70f, nullptr, &left);
    ImGui::DockBuilderDockWindow("Scene##Texturing", center);
    ImGui::DockBuilderDockWindow("Objects##Texturing", left);
    ImGui::DockBuilderDockWindow("UV Tools##Texturing", leftBottom);
    ImGui::DockBuilderDockWindow("Material##Texturing", right);
    ImGui::DockBuilderDockWindow("UV##Texturing", rightBottom);
}

static void BuildWorkspace(int workspace, ImGuiID dockId, bool docked) {
    ClearDockSpace(dockId);
    if (docked) {
        if (workspace == Workspace_Edit) BuildEditWorkspace(dockId);
        else if (workspace == Workspace_Texturing) BuildTexturingWorkspace(dockId);
        else BuildLayoutWorkspace(dockId);
    } else {
        floatingPlacementPending = true;
    }
    ImGui::DockBuilderFinish(dockId);
    workspaceStale[workspace] = false;
    workspaceReset[workspace] = false;
}

void DrawMainDockSpace(UISettings& settings) {
    if (settings.workspace < 0 || settings.workspace >= Workspace_Count) settings.workspace = Workspace_Layout;
    activeWorkspace = settings.workspace;

    bool outdated = settings.layoutVersion < currentLayoutVersion;
    if (outdated && settings.layoutVersion < 3) {
        settings.layoutMode = LayoutMode_Docked;
        settings.showConsole = false;
        settings.showMessages = true;
    }
    layoutLocked = settings.lockLayout;
    if (settings.resetLayout || outdated) {
        for (int i = 0; i < Workspace_Count; i++) workspaceStale[i] = workspaceReset[i] = true;
        settings.resetLayout = false;
        settings.layoutVersion = currentLayoutVersion;
        ImGui::MarkIniSettingsDirty();
    }

    // Hashed outside the host
    ImGuiID dockIds[Workspace_Count];
    for (int i = 0; i < Workspace_Count; i++) dockIds[i] = DockIdFor(i);

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags hostFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoBackground;

    float gap = ImGui::GetStyle().DockingSeparatorSize;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(gap, gap));
    ImGui::Begin("RunBunDockHost", nullptr, hostFlags);
    ImGui::PopStyleVar(3);

    for (int i = 0; i < Workspace_Count; i++) {
        ImGuiID dockId = dockIds[i];
        bool missing = ImGui::DockBuilderGetNode(dockId) == nullptr;
        if (i == activeWorkspace) {
            // New tabs start docked
            bool docked = settings.layoutMode == LayoutMode_Docked || !workspaceReset[i];
            if (workspaceStale[i] || missing) BuildWorkspace(i, dockId, docked);
        } else {
            if (missing) workspaceStale[i] = true;
            // Hidden tabs stay docked
            ImGui::DockSpace(dockId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_KeepAliveOnly);
        }
    }
    ImGui::DockSpace(dockIds[activeWorkspace], ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_PassthruCentralNode);
    ImGui::End();
}

void PreparePanel(const char* name) {
    if (!floatingPlacementPending) return;

    const WorkspaceInfo& info = workspaces[activeWorkspace];
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    for (int i = 0; i < info.rectCount; i++) {
        const FloatingRect& rect = info.rects[i];
        if (strcmp(rect.name, name) != 0) continue;

        ImVec2 pos = ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * rect.x, viewport->WorkPos.y + viewport->WorkSize.y * rect.y);
        ImVec2 size = ImVec2(viewport->WorkSize.x * rect.w, viewport->WorkSize.y * rect.h);
        ImGui::SetNextWindowDockID(0, ImGuiCond_Always);
        ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
        ImGui::SetNextWindowSize(size, ImGuiCond_Always);
        return;
    }
}

// First time joins buddy
void PreparePanelNear(const char* name, const char* buddy) {
    PreparePanel(name);
    if (floatingPlacementPending) return;
    const char* id = PanelId(name);
    if (ImGui::FindWindowByName(id) || ImGui::FindWindowSettingsByID(ImHashStr(id))) return;
    ImGuiWindowSettings* buddySettings = ImGui::FindWindowSettingsByID(ImHashStr(PanelId(buddy)));
    ImGuiWindow* buddyWindow = ImGui::FindWindowByName(PanelId(buddy));
    ImGuiID dock = buddyWindow && buddyWindow->DockId ? buddyWindow->DockId : (buddySettings ? buddySettings->DockId : 0);
    if (dock) ImGui::SetNextWindowDockID(dock, ImGuiCond_Once);
}

ImGuiWindowFlags PanelFlags() {
    return ImGuiWindowFlags_NoCollapse | (layoutLocked ? ImGuiWindowFlags_NoMove : 0);
}

// Keep title bars reachable
static void KeepOnScreen(const char* name) {
    ImGuiWindow* window = ImGui::FindWindowByName(name);
    if (!window || !window->WasActive) return;
    ImGuiWindow* root = window->DockIsActive ? window->RootWindowDockTree : window;
    if (!root || (root->DockNodeAsHost && root->DockNodeAsHost->IsDockSpace())) return;
    if (root->Flags & ImGuiWindowFlags_ChildWindow) return;

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 min = viewport->WorkPos;
    ImVec2 max = ImVec2(viewport->WorkPos.x + viewport->WorkSize.x, viewport->WorkPos.y + viewport->WorkSize.y);
    // Fully inside when it fits
    ImVec2 pos = root->Pos;
    pos.x = ImClamp(pos.x, min.x, ImMax(min.x, max.x - root->Size.x));
    pos.y = ImClamp(pos.y, min.y, ImMax(min.y, max.y - root->Size.y));
    if (pos.x != root->Pos.x || pos.y != root->Pos.y) ImGui::SetWindowPos(root, pos);
}

void FinishPanelLayout() {
    floatingPlacementPending = false;
    const WorkspaceInfo& info = workspaces[activeWorkspace];
    for (int i = 0; i < info.rectCount; i++) KeepOnScreen(PanelId(info.rects[i].name));
}
