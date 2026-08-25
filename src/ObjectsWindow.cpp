#include "ObjectsWindow.h"
#include "imgui/imgui.h"
#include "IconManager.h"

static bool collapsed = false;
static float expandedHeight = 300.0f;

void DrawObjectsWindow(SceneRenderer& sceneRenderer) {
    ImGui::Begin("Objects", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

    unsigned int icon = GetDropdownIcon();
    if (ImGui::ImageButton("collapseToggle", (ImTextureID)(intptr_t)icon, ImVec2(16, 16))) {
        ImVec2 currentSize = ImGui::GetWindowSize();
        if (!collapsed) {
            expandedHeight = currentSize.y;
            collapsed = true;
            ImGui::SetWindowSize(ImVec2(currentSize.x, 40.0f));
        } else {
            collapsed = false;
            ImGui::SetWindowSize(ImVec2(currentSize.x, expandedHeight));
        }
    }
    ImGui::SameLine();
    ImGui::Text("Objects");
    ImGui::Separator();

    if (!collapsed) {
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