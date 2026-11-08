#include "core/UISettings.h"
#include "ui/MaterialWindow.h"
#include "ui/DockLayout.h"
#include "ui/FileDialog.h"
#include "ui/PanelHeader.h"
#include "render/SceneRenderer.h"
#include "imgui/imgui.h"
#include <cmath>
#include <cstdint>

static PanelCollapseState materialCollapse;

static void Hint(const char* text) {
    if (!ShowHelpText()) return;
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

static SceneObject* SelectedObject(SceneRenderer& sceneRenderer) {
    auto& objects = sceneRenderer.GetObjects();
    int selected = sceneRenderer.selectedIndex;
    return (selected >= 0 && selected < (int)objects.size()) ? &objects[selected] : nullptr;
}

static const TextureInfo* ShownTexture(SceneObject& obj) {
    for (const TextureInfo& tex : obj.model.GetTextures()) {
        if (tex.enabled) return &tex;
    }
    return nullptr;
}

void DrawMaterialControls(SceneRenderer& sceneRenderer) {
    SceneObject* picked = SelectedObject(sceneRenderer);
    if (!picked) {
        Hint("Nothing selected. Click an object to change how it looks.");
        return;
    }
    SceneObject& obj = *picked;
    float full = ImGui::GetContentRegionAvail().x;

    if (obj.look.light >= 0) {
        static const char* kinds[Light_Count] = { "Sun", "Point", "Spot" };
        ImGui::SeparatorText("Light");
        int kind = obj.look.light;
        for (int k = 0; k < Light_Count; k++) {
            if (k) ImGui::SameLine();
            if (ImGui::RadioButton(kinds[k], &kind, k)) obj.look.light = kind;
        }
        ImGui::ColorEdit3("##LightColor", &obj.color.x, ImGuiColorEditFlags_PickerHueWheel);
        ImGui::SameLine();
        ImGui::TextUnformatted("Color");
        ImGui::SetNextItemWidth(full * 0.6f);
        float most = obj.look.light == Light_Sun ? 10.0f : 200.0f;
        ImGui::SliderFloat("Strength", &obj.look.power, 0.0f, most, "%.2f", ImGuiSliderFlags_Logarithmic);
        if (obj.look.light == Light_Spot) {
            ImGui::SetNextItemWidth(full * 0.6f);
            ImGui::SliderFloat("Cone", &obj.look.spotAngle, 5.0f, 170.0f, "%.0f deg");
        }
        if (obj.look.light == Light_Sun) Hint("Turn the sun with Rotation to change where light comes from. The first sun makes shadows.");
        else if (obj.look.light == Light_Spot) Hint("The spot shines along its bottom side. Turn it with Rotation.");
        else Hint("Move it closer to make things brighter.");
        Hint("Lights show in the Material view (Shift+Z) and in rendered images.");
        return;
    }

    ImGui::SeparatorText("Color");
    ImGui::ColorEdit4("##ObjectColor", &obj.color.x, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_AlphaBar);
    if (ShownTexture(obj)) Hint("An image is turned on, so it shows instead of this color.");

    ImGui::SeparatorText("Material");
    int presetCount = 0;
    const LookPreset* presets = LookPresets(presetCount);
    float third = (full - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;
    for (int p = 0; p < presetCount; p++) {
        if (p % 3) ImGui::SameLine();
        if (ImGui::Button(presets[p].name, ImVec2(third, 0.0f))) {
            obj.color = glm::vec4(presets[p].r, presets[p].g, presets[p].b, presets[p].a);
            obj.look.roughness = presets[p].roughness;
            obj.look.metallic = presets[p].metallic;
            obj.look.glow = presets[p].glow;
        }
    }
    float field = full * 0.6f;
    ImGui::SetNextItemWidth(field);
    ImGui::SliderFloat("Rough", &obj.look.roughness, 0.0f, 1.0f, "%.2f");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Low is shiny like a mirror, high is dull like chalk");
    ImGui::SetNextItemWidth(field);
    ImGui::SliderFloat("Metal", &obj.look.metallic, 0.0f, 1.0f, "%.2f");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("How much it reflects like metal");
    ImGui::SetNextItemWidth(field);
    ImGui::SliderFloat("Glow", &obj.look.glow, 0.0f, 3.0f, "%.2f");
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Makes it give off its own light color");
    float opacity = obj.color.a;
    ImGui::SetNextItemWidth(field);
    if (ImGui::SliderFloat("Solid", &opacity, 0.05f, 1.0f, "%.2f")) obj.color.a = opacity;
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Lower lets you see through it, like glass");
    ImGui::Checkbox("Casts shadows", &obj.look.castShadows);
    Hint("Materials show in the Material view (Shift+Z) and in rendered images.");

    ImGui::SeparatorText("Image");
    if (ImGui::Button("Load Image...", ImVec2(full, 0.0f))) {
        std::string path = OpenImageFileDialog();
        if (!path.empty()) sceneRenderer.LoadTextureForSelected(path);
    }

    auto& textures = obj.model.GetTextures();
    if (textures.empty()) {
        Hint("No images yet. Load one to paint it onto the object.");
    } else {
        int deleteIndex = -1;
        for (int i = 0; i < (int)textures.size(); i++) {
            ImGui::PushID(i);
            bool enabled = textures[i].enabled;
            if (ImGui::Checkbox("##enabled", &enabled)) obj.model.ToggleTexture(i, enabled);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show this image on the object");
            ImGui::SameLine();
            ImGui::TextUnformatted(textures[i].name.c_str());
            ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::GetFrameHeight());
            if (ImGui::SmallButton("X")) deleteIndex = i;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove this image");
            ImGui::PopID();
        }
        if (deleteIndex != -1) obj.model.RemoveTexture(deleteIndex);

        for (TextureInfo& tex : textures) {
            if (!tex.enabled) continue;
            ImGui::Spacing();
            ImGui::TextDisabled("How %s sits on the object", tex.name.c_str());
            float field = ImGui::GetContentRegionAvail().x * 0.6f;
            ImGui::SetNextItemWidth(field);
            ImGui::DragFloat2("Repeat", &tex.tiling.x, 0.01f, 0.01f, 100.0f, "%.2fx");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("How many times the image repeats across and down");
            ImGui::SetNextItemWidth(field);
            ImGui::DragFloat2("Offset", &tex.offset.x, 0.005f, -10.0f, 10.0f, "%.3f");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Slide the image across and down");
            ImGui::SetNextItemWidth(field);
            ImGui::DragFloat("Turn", &tex.rotation, 0.5f, -360.0f, 360.0f, "%.1f deg");
            int wrap = tex.clamp ? 1 : 0;
            ImGui::RadioButton("Repeat edges", &wrap, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Stretch edges", &wrap, 1);
            tex.clamp = wrap == 1;
            if (ImGui::SmallButton("Reset image placement")) {
                tex.tiling = glm::vec2(1.0f);
                tex.offset = glm::vec2(0.0f);
                tex.rotation = 0.0f;
                tex.clamp = false;
            }
            break;
        }
    }

    const std::vector<int>& selection = sceneRenderer.GetSelection();
    if (selection.size() > 1) {
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::TextDisabled("%d objects selected", (int)selection.size());
        if (ImGui::Button("Copy This Look to All Selected", ImVec2(full, 0.0f))) {
            auto& objects = sceneRenderer.GetObjects();
            for (int index : selection) {
                if (index == sceneRenderer.selectedIndex || index < 0 || index >= (int)objects.size()) continue;
                Model& target = objects[index].model;
                objects[index].color = obj.color;
                if (objects[index].look.light < 0 && obj.look.light < 0) objects[index].look = obj.look;
                while (!target.GetTextures().empty()) target.RemoveTexture(0);
                for (const TextureInfo& tex : obj.model.GetTextures()) target.GetTextures().push_back(tex);
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Gives every selected object this color and these images");
    }
}

void DrawMaterialWindow(SceneRenderer& sceneRenderer, bool* open) {
    if (!ImGui::Begin(PanelId("Material"), open, PanelFlags())) {
        ImGui::End();
        return;
    }
    if (DrawPanelHeader(materialCollapse)) {
        if (SceneObject* obj = SelectedObject(sceneRenderer)) ImGui::TextUnformatted(obj->name.c_str());
        DrawMaterialControls(sceneRenderer);
    }
    ImGui::End();
}
