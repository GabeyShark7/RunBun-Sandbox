#include "core/Profiler.h"
#include "core/UISettings.h"
#include "ui/ObjectsWindow.h"
#include "imgui/imgui.h"
#include "ui/DockLayout.h"
#include "ui/PanelHeader.h"
#include "ui/CreateMenu.h"
#include "ui/SceneCommands.h"
#include <cstdio>

static PanelCollapseState collapseState;
static int renamingIndex = -1;
static unsigned int renamingId = 0;
static bool renameFocus = false;
static char renameBuffer[128] = "";

static void StartRename(SceneRenderer& sceneRenderer, int index) {
    auto& objects = sceneRenderer.GetObjects();
    if (index < 0 || index >= (int)objects.size()) return;
    renamingIndex = index;
    renamingId = objects[index].id;
    renameFocus = true;
    snprintf(renameBuffer, sizeof(renameBuffer), "%s", objects[index].name.c_str());
}

void DrawObjectsWindow(SceneRenderer& sceneRenderer, bool* open) {
    PROFILE("DrawObjectsWindow");
    if (!ImGui::Begin(PanelId("Objects"), open, PanelFlags())) {
        ImGui::End();
        return;
    }

    if (DrawPanelHeader(collapseState)) {
        auto& objects = sceneRenderer.GetObjects();
        sceneRenderer.listHoveredIndex = -1;
        int request = TakeRenameRequest();
        if (request >= 0) StartRename(sceneRenderer, request);
        if (renamingIndex >= (int)objects.size() || (renamingIndex >= 0 && objects[renamingIndex].id != renamingId)) renamingIndex = -1;

        if (objects.empty()) {
            ImGui::TextWrapped("No objects yet. Right-click here or press Ctrl+A in the Scene to add a shape.");
        } else {
            int deleteIndex = -1;
            size_t picked = sceneRenderer.GetSelection().size();
            if (picked > 1) ImGui::TextDisabled("%d selected", (int)picked);

            for (int i = 0; i < (int)objects.size(); i++) {
                ImGui::PushID(i);

                bool isSelected = sceneRenderer.IsSelected(i);
                if (i == renamingIndex) {
                    if (renameFocus) ImGui::SetKeyboardFocusHere();
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 30);
                    bool done = ImGui::InputText("##Rename", renameBuffer, sizeof(renameBuffer), ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
                    bool cancelled = ImGui::IsItemActive() && ImGui::IsKeyPressed(ImGuiKey_Escape, false);
                    bool left = !renameFocus && !ImGui::IsItemActive();
                    renameFocus = false;
                    if (cancelled) renamingIndex = -1;
                    else if (done || left) {
                        sceneRenderer.RenameObject(i, renameBuffer);
                        renamingIndex = -1;
                    }
                } else {
                    if (ImGui::Selectable(objects[i].name.c_str(), isSelected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(ImGui::GetContentRegionAvail().x - 30, 0))) {
                        ImGuiIO& io = ImGui::GetIO();
                        if (io.KeyShift) sceneRenderer.SelectRange(sceneRenderer.selectedIndex, i);
                        else sceneRenderer.Select(i, io.KeyCtrl);
                        if (ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) StartRename(sceneRenderer, i);
                    }
                    if (ImGui::IsItemHovered()) {
                        sceneRenderer.listHoveredIndex = i;
                        ImGui::SetTooltip("Double-click or F2 to rename");
                    }
                }

                if (ImGui::BeginPopupContextItem("ObjectMenu")) {
                    if (!sceneRenderer.IsSelected(i)) sceneRenderer.Select(i, false);
                    if (ImGui::MenuItem("Focus", "F")) CommandFocus(sceneRenderer);
                    if (ImGui::MenuItem("Rename", "F2")) StartRename(sceneRenderer, i);
                    if (ImGui::MenuItem("Duplicate", "Ctrl+D")) CommandDuplicate(sceneRenderer, false);
                    if (ImGui::MenuItem("Join Objects", "J", false, sceneRenderer.GetSelection().size() > 1)) CommandJoin(sceneRenderer);
                    if (ImGui::MenuItem("Delete", "Del")) CommandDelete(sceneRenderer);
                    ImGui::EndPopup();
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("X")) {
                    deleteIndex = i;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Delete (Ctrl+Z brings it back)");

                ImGui::PopID();
            }

            if (deleteIndex != -1) {
                sceneRenderer.RemoveObject(deleteIndex);
            }
            ImGui::Spacing();
            if (ShowHelpText()) ImGui::TextDisabled("Ctrl+click picks more. Shift+click picks a range.");
        }
    }

    if (ImGui::BeginPopupContextWindow("ObjectsCreateMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        DrawCreateMenuItems(sceneRenderer);
        ImGui::EndPopup();
    }

    ImGui::End();
}
