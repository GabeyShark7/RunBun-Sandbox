#include "PropertiesWindow.h"
#include "imgui/imgui.h"
#include "FileDialog.h"
#include "IconManager.h"

static bool collapsed = false;
static float expandedHeight = 300.0f;

void DrawPropertiesWindow(SceneRenderer& sceneRenderer) {
    ImGui::Begin("Properties", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoTitleBar);

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
    ImGui::Text("Properties");
    ImGui::Separator();

    if (!collapsed) {
        auto& objects = sceneRenderer.GetObjects();
        int selected = sceneRenderer.selectedIndex;

        if (selected < 0 || selected >= (int)objects.size()) {
            ImGui::Text("No object selected");
            ImGui::End();
            return;
        }

        SceneObject& obj = objects[selected];
        ImGui::Text("%s", obj.name.c_str());
        ImGui::Separator();

        if (ImGui::BeginTabBar("PropertiesTabs")) {
            if (ImGui::BeginTabItem("Transform")) {
                ImGui::Text("Position");
                ImGui::DragFloat("X##Pos", &obj.position.x, 0.05f);
                ImGui::DragFloat("Y##Pos", &obj.position.y, 0.05f);
                ImGui::DragFloat("Z##Pos", &obj.position.z, 0.05f);

                ImGui::Spacing();
                ImGui::Text("Rotation");
                ImGui::DragFloat("X##Rot", &obj.rotation.x, 1.0f);
                ImGui::DragFloat("Y##Rot", &obj.rotation.y, 1.0f);
                ImGui::DragFloat("Z##Rot", &obj.rotation.z, 1.0f);

                ImGui::Spacing();
                ImGui::Text("Scale");
                ImGui::DragFloat("X##Scale", &obj.scale.x, 0.001f);
                ImGui::DragFloat("Y##Scale", &obj.scale.y, 0.001f);
                ImGui::DragFloat("Z##Scale", &obj.scale.z, 0.001f);

                ImGui::EndTabItem();
            }

            if (ImGui::BeginTabItem("Texture")) {
                if (ImGui::Button("Load Texture")) {
                    std::string path = OpenImageFileDialog();
                    if (!path.empty()) {
                        sceneRenderer.LoadTextureForSelected(path);
                    }
                }

                ImGui::Spacing();

                auto& textures = obj.model.GetTextures();
                if (textures.empty()) {
                    ImGui::Text("No textures loaded");
                } else {
                    int deleteIndex = -1;

                    for (int i = 0; i < (int)textures.size(); i++) {
                        ImGui::PushID(i);

                        bool enabled = textures[i].enabled;
                        if (ImGui::Checkbox("##enabled", &enabled)) {
                            obj.model.ToggleTexture(i, enabled);
                        }

                        ImGui::SameLine();
                        ImGui::Text("%s", textures[i].name.c_str());

                        ImGui::SameLine();
                        if (ImGui::SmallButton("X")) {
                            deleteIndex = i;
                        }

                        ImGui::PopID();
                    }

                    if (deleteIndex != -1) {
                        obj.model.RemoveTexture(deleteIndex);
                    }
                }

                ImGui::EndTabItem();
            }

            ImGui::EndTabBar();
        }
    }

    ImGui::End();
}