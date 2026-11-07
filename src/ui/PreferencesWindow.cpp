#include "ui/PreferencesWindow.h"
#include "ui/GlassTheme.h"
#include "imgui/imgui_internal.h"

static bool DrawPanelsTab(UISettings& settings) {
    bool changed = false;

    ImGui::TextDisabled("Visible panels");
    changed |= ImGui::Checkbox("Scene", &settings.showScene);
    changed |= ImGui::Checkbox("Objects", &settings.showObjects);
    changed |= ImGui::Checkbox("Properties", &settings.showProperties);
    changed |= ImGui::Checkbox("Console", &settings.showConsole);
    changed |= ImGui::Checkbox("Edit", &settings.showEdit);

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::TextDisabled("Layout style");
    changed |= ImGui::RadioButton("Floating", &settings.layoutMode, LayoutMode_Floating);
    ImGui::SameLine();
    changed |= ImGui::RadioButton("Docked", &settings.layoutMode, LayoutMode_Docked);
    ImGui::TextDisabled("Drag a panel by its title onto another panel or a window edge to dock it.");

    ImGui::Spacing();

    if (ImGui::Button("Show All")) {
        settings.showScene = settings.showObjects = settings.showProperties = settings.showConsole = settings.showEdit = true;
        changed = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset Layout")) {
        settings.showScene = settings.showObjects = settings.showProperties = settings.showConsole = settings.showEdit = true;
        settings.resetLayout = true;
        changed = true;
    }

    return changed;
}

static bool DrawAppearanceTab(UISettings& settings) {
    bool changed = false;

    ImGui::TextDisabled("Theme");
    if (ImGui::BeginCombo("##Theme", GetThemeName(settings.themePreset))) {
        for (int i = 0; i < GetThemeCount(); i++) {
            bool selected = settings.themePreset == i;
            if (ImGui::Selectable(GetThemeName(i), selected)) {
                settings.themePreset = i;
                settings.accent = GetThemeAccent(i);
                CopyThemeColorsToCustom(settings);
                changed = true;
            }
            if (selected) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Spacing();
    ImGui::TextDisabled("Glass");
    changed |= ImGui::SliderFloat("Opacity", &settings.glassOpacity, 0.30f, 1.0f, "%.2f");
    changed |= ImGui::SliderFloat("Roundness", &settings.roundness, 4.0f, 24.0f, "%.0f px");

    ImGui::Spacing();
    ImGui::TextDisabled("Interface");
    changed |= ImGui::SliderFloat("UI Scale", &settings.uiScale, 0.8f, 1.5f, "%.2fx");
    changed |= ImGui::ColorEdit3("Accent", &settings.accent.x, ImGuiColorEditFlags_NoInputs);

    ImGui::Spacing();
    ImGui::TextDisabled("Colors");
    if (ImGui::Checkbox("Use my own colors", &settings.useCustomColors)) {
        if (settings.useCustomColors) CopyThemeColorsToCustom(settings);
        changed = true;
    }
    ImGui::BeginDisabled(!settings.useCustomColors);
    ImGuiColorEditFlags colorFlags = ImGuiColorEditFlags_NoInputs | ImGuiColorEditFlags_AlphaBar | ImGuiColorEditFlags_PickerHueWheel;
    changed |= ImGui::ColorEdit3("Background", &settings.customBackdrop.x, colorFlags);
    changed |= ImGui::ColorEdit3("Panels", &settings.customPanel.x, colorFlags);
    changed |= ImGui::ColorEdit3("Text", &settings.customText.x, colorFlags);
    changed |= ImGui::ColorEdit4("Borders", &settings.customBorder.x, colorFlags);
    if (ImGui::Button("Start From Current Theme")) {
        CopyThemeColorsToCustom(settings);
        changed = true;
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    if (ImGui::Button("Restore Defaults")) {
        ResetAppearance(settings);
        changed = true;
    }

    return changed;
}

void DrawPreferencesWindow(UISettings& settings) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(470, 400), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse;
    if (!ImGui::Begin("Preferences", &settings.showPreferences, flags)) {
        ImGui::End();
        return;
    }

    bool changed = false;

    if (ImGui::BeginTabBar("PreferencesTabs")) {
        if (ImGui::BeginTabItem("Panels")) {
            changed |= DrawPanelsTab(settings);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Appearance")) {
            changed |= DrawAppearanceTab(settings);
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    if (changed) ImGui::MarkIniSettingsDirty();

    ImGui::End();
}
