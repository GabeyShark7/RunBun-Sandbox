#include "ui/EditWindow.h"
#include "ui/EditMode.h"
#include "ui/PanelHeader.h"
#include "imgui/imgui.h"

static PanelCollapseState collapseState;

static bool ModeButton(const char* label, bool selected, float width) {
    if (selected) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
    bool clicked = ImGui::Button(label, ImVec2(width, 0.0f));
    if (selected) ImGui::PopStyleColor();
    return clicked;
}

void DrawEditWindow(SceneRenderer& sceneRenderer, bool* open) {
    if (!ImGui::Begin("Edit", open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    if (DrawPanelHeader(collapseState)) {
        auto& objects = sceneRenderer.GetObjects();
        int selected = sceneRenderer.selectedIndex;
        bool hasSelection = selected >= 0 && selected < (int)objects.size();

        if (!IsEditModeActive()) {
            if (!hasSelection) {
                ImGui::TextDisabled("Select an object, then press E");
                ImGui::TextDisabled("to edit its vertices, edges and faces.");
            } else {
                ImGui::Text("%s", objects[selected].name.c_str());
                ImGui::TextDisabled("Object Mode");
                ImGui::Spacing();
                if (ImGui::Button("Enter Edit Mode  (E)", ImVec2(-1.0f, 0.0f))) {
                    EnterEditMode(sceneRenderer, selected);
                }
            }
        } else {
            ImGui::Text("Editing %s", objects[GetEditObjectIndex()].name.c_str());
            ImGui::Spacing();

            ImGui::TextDisabled("Select");
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float width = (ImGui::GetContentRegionAvail().x - spacing * 2.0f) / 3.0f;
            EditSelectMode mode = GetEditSelectMode();
            if (ModeButton("Vertex  1", mode == EditSelect_Vertex, width)) SetEditSelectMode(EditSelect_Vertex);
            ImGui::SameLine();
            if (ModeButton("Edge  2", mode == EditSelect_Edge, width)) SetEditSelectMode(EditSelect_Edge);
            ImGui::SameLine();
            if (ModeButton("Face  3", mode == EditSelect_Face, width)) SetEditSelectMode(EditSelect_Face);

            ImGui::Spacing();
            int count = GetEditSelectionCount();
            const char* singular[3] = { "vertex", "edge", "face" };
            const char* plural[3] = { "vertices", "edges", "faces" };
            ImGui::Text("%d %s selected", count, count == 1 ? singular[mode] : plural[mode]);
            float half = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;
            if (ImGui::Button("Select All  (A)", ImVec2(half, 0.0f))) EditSelectAll(true);
            ImGui::SameLine();
            if (ImGui::Button("Deselect", ImVec2(half, 0.0f))) EditSelectAll(false);

            glm::vec3 center;
            if (EditGetSelectionCenter(sceneRenderer, center)) {
                ImGui::Spacing();
                ImGui::TextDisabled("Selection center");
                glm::vec3 centerMm = center * 1000.0f;
                glm::vec3 edited = centerMm;
                bool changed = false;
                changed |= ImGui::DragFloat("X##EditCenter", &edited.x, 5.0f, 0.0f, 0.0f, "%.1f mm");
                changed |= ImGui::DragFloat("Y##EditCenter", &edited.y, 5.0f, 0.0f, 0.0f, "%.1f mm");
                changed |= ImGui::DragFloat("Z##EditCenter", &edited.z, 5.0f, 0.0f, 0.0f, "%.1f mm");
                if (changed) EditMoveSelection(sceneRenderer, (edited - centerMm) / 1000.0f);
            }

            ImGui::Spacing();
            ImGui::Separator();
            ImGui::TextDisabled("Click to select, Shift+click to add");
            ImGui::TextDisabled("G move   S scale   R rotate");
            ImGui::Spacing();
            if (ImGui::Button("Exit Edit Mode  (E)", ImVec2(-1.0f, 0.0f))) {
                ExitEditMode();
            }
        }
    }

    ImGui::End();
}
