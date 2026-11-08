#include "core/Profiler.h"
#include "ui/EditWindow.h"
#include "ui/EditMode.h"
#include "ui/PanelHeader.h"
#include "ui/DockLayout.h"
#include "ui/SceneGizmo.h"
#include "ui/Workspaces.h"
#include "core/UISettings.h"
#include "imgui/imgui_internal.h"

static PanelCollapseState collapseState;

static void Hint(const char* text) {
    if (!ShowHelpText()) return;
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

static bool ToggleButton(const char* label, bool selected, float width) {
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    }
    bool clicked = ImGui::Button(label, ImVec2(width, 0.0f));
    if (selected) ImGui::PopStyleColor(3);
    return clicked;
}

static void Step(int number, const char* title) {
    ImGui::Spacing();
    ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab), "%d", number);
    ImGui::SameLine();
    ImGui::TextUnformatted(title);
}

void DrawEditWindow(SceneRenderer& sceneRenderer, bool* open) {
    PROFILE("DrawEditWindow");
    if (!ImGui::Begin(PanelId("Edit"), open, PanelFlags())) {
        ImGui::End();
        return;
    }

    if (DrawPanelHeader(collapseState)) {
        auto& objects = sceneRenderer.GetObjects();
        int selected = sceneRenderer.selectedIndex;
        bool hasSelection = selected >= 0 && selected < (int)objects.size();
        float full = ImGui::GetContentRegionAvail().x;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        bool editTab = ActiveWorkspace() == Workspace_Edit;

        if (!IsEditModeActive()) {
            ImGui::TextUnformatted("Reshape an object");
            Hint("Edit Mode lets you move the points, edges and faces of an object to change its shape.");
            ImGui::Spacing();
            if (!hasSelection) {
                Hint(editTab ? "Pick the object to reshape, here or in the Scene." : "First click an object in the Scene to select it.");
            } else {
                ImGui::Text("Selected: %s", objects[selected].name.c_str());
                ImGui::Spacing();
                if (editTab) {
                    if (ImGui::Button("Start Editing   (E)", ImVec2(full, 0.0f))) EnterEditMode(sceneRenderer, selected);
                } else {
                    if (ImGui::Button("Edit Here   (Tab)", ImVec2(full, 0.0f))) EnterEditMode(sceneRenderer, selected);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Quick edit without leaving this tab");
                    if (ImGui::Button("Open Edit Tab   (E)", ImVec2(full, 0.0f))) ToggleEditWorkspace(sceneRenderer);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("A roomier screen made just for editing");
                }
            }
            if (editTab && !objects.empty()) {
                ImGui::Spacing();
                ImGui::SeparatorText("Objects");
                for (int i = 0; i < (int)objects.size(); i++) {
                    ImGui::PushID(i);
                    if (ImGui::Selectable(objects[i].name.c_str(), i == selected)) {
                        sceneRenderer.Select(i, false);
                        EnterEditMode(sceneRenderer, i);
                    }
                    ImGui::PopID();
                }
            }
        } else {
            ImGui::Text("Editing %s", objects[GetEditObjectIndex()].name.c_str());

            Step(1, "Choose what to pick");
            float third = (full - spacing * 2.0f) / 3.0f;
            EditSelectMode mode = GetEditSelectMode();
            if (ToggleButton("Points", mode == EditSelect_Vertex, third)) SetEditSelectMode(EditSelect_Vertex);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Corners of the shape  (key 1)");
            ImGui::SameLine();
            if (ToggleButton("Edges", mode == EditSelect_Edge, third)) SetEditSelectMode(EditSelect_Edge);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lines between corners  (key 2)");
            ImGui::SameLine();
            if (ToggleButton("Faces", mode == EditSelect_Face, third)) SetEditSelectMode(EditSelect_Face);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Flat sides  (key 3)");

            Step(2, "Click them on the object");
            int count = GetEditSelectionCount();
            const char* singular[3] = { "point", "edge", "face" };
            const char* plural[3] = { "points", "edges", "faces" };
            if (count == 0) Hint("Nothing picked yet. Shift+click picks more than one.");
            else ImGui::Text("%d %s picked", count, count == 1 ? singular[mode] : plural[mode]);
            float half = (full - spacing) * 0.5f;
            if (ImGui::Button("Pick All  (A)", ImVec2(half, 0.0f))) EditSelectAll(true);
            ImGui::SameLine();
            ImGui::BeginDisabled(count == 0);
            if (ImGui::Button("Clear", ImVec2(half, 0.0f))) EditSelectAll(false);
            ImGui::EndDisabled();

            Step(3, "Change them");
            ImGui::BeginDisabled(count == 0);
            if (ImGui::Button("Move  (G)", ImVec2(third, 0.0f))) RequestSceneTransform('G');
            ImGui::SameLine();
            if (ImGui::Button("Scale  (S)", ImVec2(third, 0.0f))) RequestSceneTransform('S');
            ImGui::SameLine();
            if (ImGui::Button("Rotate  (R)", ImVec2(third, 0.0f))) RequestSceneTransform('R');
            ImGui::EndDisabled();
            Hint(count == 0 ? "Pick something first." : "Move the mouse, then click to finish. Esc cancels. Press X, Y or Z to lock a direction.");

            glm::vec3 center;
            if (count > 0 && EditGetSelectionCenter(sceneRenderer, center)) {
                if (ImGui::CollapsingHeader("Exact position")) {
                    glm::vec3 centerMm = center * 1000.0f;
                    glm::vec3 edited = centerMm;
                    bool changed = false;
                    changed |= ImGui::DragFloat("X##EditCenter", &edited.x, 5.0f, 0.0f, 0.0f, "%.1f mm");
                    changed |= ImGui::DragFloat("Y##EditCenter", &edited.y, 5.0f, 0.0f, 0.0f, "%.1f mm");
                    changed |= ImGui::DragFloat("Z (up)##EditCenter", &edited.z, 5.0f, 0.0f, 0.0f, "%.1f mm");
                    if (changed) EditMoveSelection(sceneRenderer, (edited - centerMm) / 1000.0f);
                }
            }

            Step(4, "Shape tools");
            bool busy = IsSceneTransformActive();
            ImGui::BeginDisabled(busy);
            ImGui::BeginDisabled(count == 0);
            if (ImGui::Button("Extrude  (E)", ImVec2(half, 0.0f))) RequestSceneExtrude();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Pull faces out to grow new sides. Also works on open edges.");
            ImGui::SameLine();
            if (ImGui::Button("Inset  (I)", ImVec2(half, 0.0f))) EditBeginTool(sceneRenderer, EditTool_Inset, ImGui::GetIO().MousePos);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Make a smaller face inside each picked face");
            if (ImGui::Button("Bevel  (Ctrl+B)", ImVec2(half, 0.0f))) EditBeginTool(sceneRenderer, EditTool_Bevel, ImGui::GetIO().MousePos);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Round off sharp edges with a slanted strip");
            ImGui::SameLine();
            ImGui::EndDisabled();
            if (ImGui::Button("Loop Cut  (Ctrl+R)", ImVec2(half, 0.0f))) EditBeginTool(sceneRenderer, EditTool_LoopCut, ImGui::GetIO().MousePos);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Slice a ring of new edges around the shape");
            ImGui::BeginDisabled(count == 0);
            if (ImGui::Button("Subdivide", ImVec2(half, 0.0f))) EditSubdivide(sceneRenderer);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Split each picked face into smaller faces");
            ImGui::SameLine();
            if (ImGui::Button("Merge  (M)", ImVec2(half, 0.0f))) EditMerge(sceneRenderer);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Join the picked points into one point in their middle");
            bool faceMode = mode == EditSelect_Face;
            if (ImGui::Button(faceMode ? "Join Faces  (F)" : "Fill Face  (F)", ImVec2(half, 0.0f))) EditJoinOrFill(sceneRenderer);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip(faceMode ? "Join touching faces into one face" : "Make a new face from the picked points, like filling a hole");
            ImGui::SameLine();
            ImGui::BeginDisabled(faceMode);
            if (ImGui::Button("Dissolve  (X)", ImVec2(half, 0.0f))) EditDissolve(sceneRenderer);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip(faceMode ? "Use Points or Edges to dissolve. X deletes faces." : "Remove the picked points or edges but keep the surface closed");
            ImGui::EndDisabled();
            if (ImGui::Button("Delete  (Del)", ImVec2(half, 0.0f))) EditDelete(sceneRenderer);
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Remove what is picked and the faces touching it, leaving a hole");
            ImGui::SameLine();
            ImGui::EndDisabled();
            if (ImGui::Button("Weld Doubles", ImVec2(half, 0.0f))) EditWeldDoubles(sceneRenderer);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Join points sitting on top of each other");

            ImGui::Spacing();
            ImGui::TextDisabled("Mirror across the object middle");
            const char* axes[3] = { "X", "Y", "Z" };
            float quarter = (full - spacing * 3.0f) / 4.0f;
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Copy");
            ImGui::SameLine(quarter + spacing);
            for (int axis = 0; axis < 3; axis++) {
                ImGui::PushID(axis);
                if (ImGui::Button(axes[axis], ImVec2(quarter, 0.0f))) EditMirrorCopy(sceneRenderer, axis);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Add a mirrored copy of the picked faces (or all faces) joined at the middle");
                ImGui::PopID();
                if (axis < 2) ImGui::SameLine();
            }
            ImGui::BeginDisabled(count == 0);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted("Flip");
            ImGui::SameLine(quarter + spacing);
            for (int axis = 0; axis < 3; axis++) {
                ImGui::PushID(axis + 10);
                if (ImGui::Button(axes[axis], ImVec2(quarter, 0.0f))) EditFlip(sceneRenderer, axis);
                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Turn the picked part over in place");
                ImGui::PopID();
                if (axis < 2) ImGui::SameLine();
            }
            ImGui::EndDisabled();
            ImGui::EndDisabled();

            if (const char* note = GetEditMessage()) {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.86f, 0.35f, 1.0f));
                ImGui::TextWrapped("%s", note);
                ImGui::PopStyleColor();
            }

            ImGui::Spacing();
            ImGui::SeparatorText("Options");
            bool through = GetEditSelectThrough();
            if (ImGui::Checkbox("Pick through  (Alt+Z)", &through)) SetEditSelectThrough(through);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("On: pick points and edges behind the object too. Off: only what you can see.");
            UISettings& settings = GetUISettings();
            bool showHidden = settings.editShowHidden;
            if (ImGui::Checkbox("Show hidden parts  (Alt+H)", &showHidden)) SetEditShowHidden(showHidden);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("On: points, edges and faces behind the shape or other shapes show faded. Off: they are hidden.");
            if (ImGui::Checkbox("Snap to points", &settings.snapToPoints)) ImGui::MarkIniSettingsDirty();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("While moving, jump onto nearby points. Holding Shift does this too.");
            Hint("Drag on empty space to box pick. Shift adds, Ctrl removes.");

            ImGui::Spacing();
            ImGui::Separator();
            if (editTab) {
                if (ImGui::Button("Done Editing   (Tab)", ImVec2(full, 0.0f))) ToggleEditWorkspace(sceneRenderer);
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("Go back to the tab you came from");
            } else {
                if (ImGui::Button("Done Editing   (Tab)", ImVec2(full, 0.0f))) ExitEditMode();
            }
        }
    }

    ImGui::End();
}
