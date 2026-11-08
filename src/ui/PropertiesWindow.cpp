#include "core/Profiler.h"
#include "ui/PropertiesWindow.h"
#include "imgui/imgui.h"
#include "ui/DockLayout.h"
#include "ui/FileDialog.h"
#include "ui/PanelHeader.h"
#include <cfloat>
#include <cstdio>
#include "ui/MaterialWindow.h"
#include "ui/Workspaces.h"
#include "core/UISettings.h"
#include "render/Primitives.h"
#include "core/SceneState.h"

static PanelCollapseState collapseState;

void DrawPropertiesWindow(SceneRenderer& sceneRenderer, bool* open) {
    PROFILE("DrawPropertiesWindow");
    if (!ImGui::Begin(PanelId("Properties"), open, PanelFlags())) {
        ImGui::End();
        return;
    }

    if (DrawPanelHeader(collapseState)) {
        auto& objects = sceneRenderer.GetObjects();
        int selected = sceneRenderer.selectedIndex;

        if (selected < 0 || selected >= (int)objects.size()) {
            ImGui::TextDisabled("Nothing selected. Click an object in the Scene to see its settings here.");
            ImGui::End();
            return;
        }

        SceneObject& obj = objects[selected];
        static char nameBuffer[128] = "";
        static unsigned int nameId = 0;
        static bool nameEditing = false;
        if (!nameEditing || nameId != obj.id) snprintf(nameBuffer, sizeof(nameBuffer), "%s", obj.name.c_str());
        nameId = obj.id;
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::InputText("##ObjectName", nameBuffer, sizeof(nameBuffer));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Click to rename");
        nameEditing = ImGui::IsItemActive();
        if (ImGui::IsItemDeactivatedAfterEdit()) sceneRenderer.RenameObject(selected, nameBuffer);
        int picked = (int)sceneRenderer.GetSelection().size();
        if (picked > 1) ImGui::TextDisabled("%d objects selected. These settings are for %s only.", picked, obj.name.c_str());

        float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        for (int kind = 0; kind < 2; kind++) {
            bool on = (kind == 1) == obj.hole;
            if (on) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab));
            }
            if (kind == 1) ImGui::SameLine();
            if (ImGui::Button(kind == 0 ? "Solid" : "Hole  (H)", ImVec2(half, 0.0f))) {
                for (int index : sceneRenderer.GetSelection()) objects[index].hole = kind == 1;
                obj.hole = kind == 1;
            }
            if (on) ImGui::PopStyleColor(2);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(kind == 0 ? "A normal shape" : "A see-through shape that cuts into solids when you Group them (Ctrl+G)");
        }

        if (obj.parts) {
            ImGui::TextDisabled("Group of %d shapes", (int)obj.parts->size());
            ImGui::SameLine();
            if (ImGui::SmallButton("Ungroup")) sceneRenderer.UngroupSelected();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Get the original shapes back  (Ctrl+Shift+G)");
        }

        if (obj.shape >= 0 && (PrimitiveHasSides((PrimitiveType)obj.shape) || PrimitiveHasThickness((PrimitiveType)obj.shape))) {
            if (obj.ShapeEditable()) {
                int sides = obj.shapeSides;
                float thickness = obj.shapeThickness * 100.0f;
                bool changed = false;
                if (PrimitiveHasSides((PrimitiveType)obj.shape)) {
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
                    changed |= ImGui::SliderInt("Sides", &sides, 3, 96);
                    if (ImGui::IsItemHovered()) ImGui::SetTooltip("More sides make it rounder");
                }
                if (PrimitiveHasThickness((PrimitiveType)obj.shape)) {
                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x * 0.55f);
                    changed |= ImGui::SliderFloat("Thickness", &thickness, 4.0f, 96.0f, "%.0f%%");
                }
                if (changed) sceneRenderer.SetShapeSettings(selected, sides, thickness / 100.0f);
            } else {
                ImGui::TextDisabled("Shape settings are locked after editing its points");
            }
        }
        ImGui::Separator();

        if (ImGui::BeginTabBar("PropertiesTabs")) {
            if (ImGui::BeginTabItem("Transform")) {
                ImGui::Text("Position");
                glm::vec3 positionMm = obj.position * 1000.0f;
                if (ImGui::DragFloat("X##Pos", &positionMm.x, 5.0f, 0.0f, 0.0f, "%.1f mm")) obj.position.x = positionMm.x / 1000.0f;
                if (ImGui::DragFloat("Y##Pos", &positionMm.y, 5.0f, 0.0f, 0.0f, "%.1f mm")) obj.position.y = positionMm.y / 1000.0f;
                if (ImGui::DragFloat("Z (up)##Pos", &positionMm.z, 5.0f, 0.0f, 0.0f, "%.1f mm")) obj.position.z = positionMm.z / 1000.0f;

                ImGui::Spacing();
                ImGui::Text("Rotation");
                ImGui::DragFloat("X##Rot", &obj.rotation.x, 1.0f, 0.0f, 0.0f, "%.1f°");
                ImGui::DragFloat("Y##Rot", &obj.rotation.y, 1.0f, 0.0f, 0.0f, "%.1f°");
                ImGui::DragFloat("Z##Rot", &obj.rotation.z, 1.0f, 0.0f, 0.0f, "%.1f°");

                ImGui::Spacing();
                ImGui::Text("Scale");
                ImGui::DragFloat("X##Scale", &obj.scale.x, 0.001f);
                ImGui::DragFloat("Y##Scale", &obj.scale.y, 0.001f);
                ImGui::DragFloat("Z##Scale", &obj.scale.z, 0.001f);

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Look")) {
                if (WorkspaceAllowed(Workspace_Shading) && ImGui::Button("Open Shading Tab  (Ctrl+4)", ImVec2(-1.0f, 0.0f)))
                    SetWorkspace(sceneRenderer, Workspace_Shading);
                DrawMaterialControls(sceneRenderer);
                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    ImGui::End();
}