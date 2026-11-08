#include "ui/ShadingWindow.h"
#include "ui/MaterialWindow.h"
#include "ui/DockLayout.h"
#include "ui/PanelHeader.h"
#include "ui/FileDialog.h"
#include "ui/RenderExport.h"
#include "core/UISettings.h"
#include "render/SceneRenderer.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include <cstdio>
#include <cstring>

static PanelCollapseState shadingCollapse;
static PanelCollapseState worldCollapse;

static void Hint(const char* text) {
    if (!ShowHelpText()) return;
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

void DrawShadingWindow(SceneRenderer& scene, bool* open) {
    if (!ImGui::Begin(PanelId("Shading"), open, PanelFlags())) {
        ImGui::End();
        return;
    }
    if (DrawPanelHeader(shadingCollapse)) {
        auto& objects = scene.GetObjects();
        int selected = scene.selectedIndex;
        if (selected >= 0 && selected < (int)objects.size()) {
            ImGui::TextUnformatted(objects[selected].name.c_str());
            if (scene.GetSelection().size() > 1) {
                ImGui::SameLine();
                ImGui::TextDisabled("and %d more", (int)scene.GetSelection().size() - 1);
            }
        }
        DrawMaterialControls(scene);
    }
    ImGui::End();
}

void DrawWorldWindow(SceneRenderer& scene, bool* open) {
    if (!ImGui::Begin(PanelId("World"), open, PanelFlags())) {
        ImGui::End();
        return;
    }
    if (DrawPanelHeader(worldCollapse)) {
        UISettings& settings = GetUISettings();
        bool changed = false;
        float full = ImGui::GetContentRegionAvail().x;
        float spacing = ImGui::GetStyle().ItemSpacing.x;

        ImGui::SeparatorText("View");
        static const char* styles[4] = { "Solid", "Material", "Wire", "X-ray" };
        float quarter = (full - spacing * 3.0f) / 4.0f;
        for (int k = 0; k < 4; k++) {
            if (k) ImGui::SameLine();
            bool on = settings.viewStyle == k;
            if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab));
            if (ImGui::Button(styles[k], ImVec2(quarter, 0.0f))) { settings.viewStyle = k; changed = true; }
            if (on) ImGui::PopStyleColor();
        }

        ImGui::SeparatorText("Lights");
        float third = (full - spacing * 2.0f) / 3.0f;
        if (ImGui::Button("+ Sun", ImVec2(third, 0.0f))) scene.CreateLight(Light_Sun);
        ImGui::SameLine();
        if (ImGui::Button("+ Point", ImVec2(third, 0.0f))) scene.CreateLight(Light_Point);
        ImGui::SameLine();
        if (ImGui::Button("+ Spot", ImVec2(third, 0.0f))) scene.CreateLight(Light_Spot);
        auto& objects = scene.GetObjects();
        int lights = 0;
        for (int i = 0; i < (int)objects.size(); i++) {
            if (objects[i].look.light < 0) continue;
            lights++;
            ImGui::PushID(i);
            ImVec4 c = ImVec4(objects[i].color.r, objects[i].color.g, objects[i].color.b, 1.0f);
            ImGui::ColorButton("##LightSwatch", c, ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, ImVec2(ImGui::GetFrameHeight(), ImGui::GetFrameHeight()));
            ImGui::SameLine();
            if (ImGui::Selectable(objects[i].name.c_str(), scene.IsSelected(i), 0, ImVec2(0.0f, ImGui::GetFrameHeight()))) scene.SetSelection({ i }, i);
            ImGui::PopID();
        }
        if (!lights) Hint("No lights yet, so a soft default light is used. Add a Sun for shadows.");
        changed |= ImGui::Checkbox("Shadows", &settings.shadows);
        ImGui::SameLine();
        changed |= ImGui::Checkbox("Show light icons", &settings.showLights);

        ImGui::SeparatorText("Background");
        changed |= ImGui::RadioButton("Color", &settings.backgroundMode, 0);
        ImGui::SameLine();
        changed |= ImGui::RadioButton("Fade", &settings.backgroundMode, 1);
        ImGui::SameLine();
        changed |= ImGui::RadioButton("Image", &settings.backgroundMode, 2);
        changed |= ImGui::ColorEdit3("##WorldTop", &settings.sceneBackground.x, ImGuiColorEditFlags_NoInputs);
        ImGui::SameLine();
        ImGui::TextUnformatted(settings.backgroundMode == 1 ? "Top" : "Color");
        if (settings.backgroundMode == 1) {
            ImGui::SameLine();
            changed |= ImGui::ColorEdit3("##WorldBottom", &settings.sceneBackgroundBottom.x, ImGuiColorEditFlags_NoInputs);
            ImGui::SameLine();
            ImGui::TextUnformatted("Bottom");
        }
        if (settings.backgroundMode == 2) {
            if (ImGui::Button("Pick Image...")) {
                std::string path = OpenImageFileDialog();
                if (!path.empty()) {
                    snprintf(settings.backgroundImage, sizeof(settings.backgroundImage), "%s", path.c_str());
                    changed = true;
                }
            }
            ImGui::SameLine();
            const char* name = strrchr(settings.backgroundImage, '\\');
            if (!name) name = strrchr(settings.backgroundImage, '/');
            ImGui::TextDisabled("%s", settings.backgroundImage[0] ? (name ? name + 1 : settings.backgroundImage) : "No image yet");
        }

        ImGui::SeparatorText("Save");
        float half = (full - spacing) * 0.5f;
        if (ImGui::Button("Render Image...", ImVec2(half, 0.0f))) OpenRenderImage();
        ImGui::SameLine();
        if (ImGui::Button("Export Models...", ImVec2(half, 0.0f))) OpenExportModels();
        if (changed) ImGui::MarkIniSettingsDirty();
    }
    ImGui::End();
}
