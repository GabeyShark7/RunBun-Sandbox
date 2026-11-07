#include "ui/ObjectsWindow.h"
#include "imgui/imgui.h"
#include "ui/PanelHeader.h"

static PanelCollapseState collapseState;

void DrawObjectsWindow(SceneRenderer& sceneRenderer, bool* open) {
    if (!ImGui::Begin("Objects", open, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    if (DrawPanelHeader(collapseState)) {
        auto& objects = sceneRenderer.GetObjects();

        if (objects.empty()) {
            ImGui::Text("No objects imported");
        } else {
            int deleteIndex = -1;

            for (int i = 0; i < (int)objects.size(); i++) {
                ImGui::PushID(i);

                bool isSelected = (sceneRenderer.selectedIndex == i);
                if (ImGui::Selectable(objects[i].name.c_str(), isSelected, 0, ImVec2(ImGui::GetContentRegionAvail().x - 30, 0))) {
                    sceneRenderer.selectedIndex = i;
                }

                ImGui::SameLine();
                if (ImGui::SmallButton("X")) {
                    deleteIndex = i;
                }

                ImGui::PopID();
            }

            if (deleteIndex != -1) {
                sceneRenderer.RemoveObject(deleteIndex);
            }
        }
    }

    ImGui::End();
}
