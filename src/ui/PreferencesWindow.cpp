#include "core/UISettings.h"
#include "ui/PreferencesWindow.h"
#include "ui/GlassTheme.h"
#include "ui/EditMode.h"
#include "ui/UpdateWindow.h"
#include "imgui/imgui_internal.h"

static void Hint(const char* text) {
    if (!ShowHelpText()) return;
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

static void Heading(const char* text) {
    ImGui::Spacing();
    ImGui::SeparatorText(text);
}

// Big clickable choice card
static bool ModeCard(const char* title, const char* description, bool selected, float width) {
    ImGuiStyle& style = ImGui::GetStyle();
    float wrap = width - style.FramePadding.x * 2.0f;
    ImVec2 descriptionSize = ImGui::CalcTextSize(description, nullptr, false, wrap);
    float height = ImGui::GetTextLineHeight() + descriptionSize.y + style.FramePadding.y * 3.0f;

    ImVec2 min = ImGui::GetCursorScreenPos();
    bool clicked = ImGui::InvisibleButton(title, ImVec2(width, height));
    bool hovered = ImGui::IsItemHovered();
    ImVec2 max = ImGui::GetItemRectMax();

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImU32 fill = ImGui::GetColorU32(selected ? ImGuiCol_Header : (hovered ? ImGuiCol_ButtonHovered : ImGuiCol_Button));
    drawList->AddRectFilled(min, max, fill, style.FrameRounding);
    if (selected) drawList->AddRect(min, max, ImGui::GetColorU32(ImGuiCol_SliderGrab), style.FrameRounding, 0, 2.0f);

    ImVec2 textPos = ImVec2(min.x + style.FramePadding.x, min.y + style.FramePadding.y);
    drawList->AddText(textPos, ImGui::GetColorU32(ImGuiCol_Text), title);
    textPos.y += ImGui::GetTextLineHeight() + style.FramePadding.y * 0.5f;
    drawList->AddText(nullptr, 0.0f, textPos, ImGui::GetColorU32(ImGuiCol_TextDisabled), description, nullptr, wrap);
    return clicked;
}

static bool DrawGeneralTab(UISettings& settings) {
    bool changed = false;
    float width = ImGui::GetContentRegionAvail().x;

    Heading("Mode");
    Hint("Pick how much RunBun shows you. You can switch any time.");
    ImGui::Spacing();
    if (ModeCard("Tinker", "Build with shapes and holes. Drag shapes in, pull their handles to size them, and Group to cut holes.", settings.workMode == WorkMode_Tinker, width)) {
        if (settings.workMode != WorkMode_Tinker) settings.cameraOrbit = true;
        settings.workMode = WorkMode_Tinker;
        ExitEditMode();
        changed = true;
    }
    ImGui::Spacing();
    if (ModeCard("Mix", "Everything in Tinker plus Edit Mode, the Edit and Texturing tabs, and Object Mods.", settings.workMode == WorkMode_Mix, width)) {
        if (settings.workMode != WorkMode_Mix) settings.cameraOrbit = false;
        settings.workMode = WorkMode_Mix;
        changed = true;
    }

    Heading("Help text");
    int help = settings.helpMinimal ? 1 : 0;
    if (ImGui::RadioButton("Full", &help, 0)) { settings.helpMinimal = false; changed = true; }
    ImGui::SameLine();
    if (ImGui::RadioButton("Minimal", &help, 1)) { settings.helpMinimal = true; changed = true; }
    Hint("Minimal hides most of the explaining text. Hover tips stay.");

    Heading("Camera");
    int orbit = settings.cameraOrbit ? 0 : 1;
    if (ImGui::RadioButton("Orbit", &orbit, 0)) { settings.cameraOrbit = true; changed = true; }
    ImGui::SameLine();
    if (ImGui::RadioButton("Fly", &orbit, 1)) { settings.cameraOrbit = false; changed = true; }
    Hint(settings.cameraOrbit ? "Right-drag to orbit, middle-drag or Shift+right-drag to pan, scroll to zoom."
                              : "Hold right-click and use W A S D to fly. Middle-drag orbits, scroll zooms.");
    changed |= ImGui::Checkbox("Flip orbit left and right", &settings.orbitFlipX);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Flip orbit up and down", &settings.orbitFlipY);
    Hint("Use these if turning the view feels backwards.");

    Heading("Scene");
    changed |= ImGui::Checkbox("Show the ground grid", &settings.showGrid);
    changed |= ImGui::Checkbox("Show move arrows on the selected object", &settings.showTransformGizmo);
    changed |= ImGui::Checkbox("Show size handles on the selected object", &settings.showHandles);
    changed |= ImGui::Checkbox("Snap when moving", &settings.snapEnabled);
    ImGui::BeginDisabled(!settings.snapEnabled);
    ImGui::SetNextItemWidth(160.0f);
    ImGui::Indent();
    changed |= ImGui::DragFloat("Snap step", &settings.snapUnitMm, 1.0f, 0.1f, 100000.0f, "%.1f mm", ImGuiSliderFlags_AlwaysClamp);
    ImGui::Unindent();
    ImGui::EndDisabled();
    Hint("Holding Ctrl while moving also snaps.");

    Heading("Updates");
    if (ImGui::Button("Check for Updates")) ShowUpdateWindow();
    return changed;
}

static bool DrawLayoutTab(UISettings& settings) {
    bool changed = false;

    Heading("Panels");
    Hint("Turn panels on or off. You can also close a panel with the x on its tab.");
    changed |= ImGui::Checkbox("Scene", &settings.showScene);
    changed |= ImGui::Checkbox("Objects", &settings.showObjects);
    changed |= ImGui::Checkbox("Properties", &settings.showProperties);
    ImGui::BeginDisabled(settings.workMode == WorkMode_Tinker);
    changed |= ImGui::Checkbox("Edit (Mix mode)", &settings.showEdit);
    ImGui::EndDisabled();
    changed |= ImGui::Checkbox("Messages", &settings.showMessages);
    changed |= ImGui::Checkbox("Console (technical log)", &settings.showConsole);

    Heading("Arrangement");
    if (ImGui::RadioButton("Docked", &settings.layoutMode, LayoutMode_Docked)) { settings.resetLayout = true; changed = true; }
    ImGui::SameLine();
    Hint("Panels fill the window side by side");
    if (ImGui::RadioButton("Floating", &settings.layoutMode, LayoutMode_Floating)) { settings.resetLayout = true; changed = true; }
    ImGui::SameLine();
    Hint("Panels are separate windows");
    ImGui::Spacing();
    changed |= ImGui::Checkbox("Lock layout", &settings.lockLayout);
    ImGui::SameLine();
    Hint("Stops panels moving by accident");
    ImGui::Spacing();
    Hint("To rearrange, drag a panel by its tab. Drop it on another panel to stack them as tabs, or near an edge to split the space.");
    ImGui::Spacing();
    if (ImGui::Button("Reset Layout")) {
        settings.showScene = settings.showObjects = settings.showProperties = settings.showEdit = settings.showMessages = true;
        settings.resetLayout = true;
        changed = true;
    }
    return changed;
}

static bool DrawAppearanceTab(UISettings& settings) {
    bool changed = false;

    Heading("Theme");
    ImGui::SetNextItemWidth(220.0f);
    if (ImGui::BeginCombo("Color theme", GetThemeName(settings.themePreset))) {
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
    changed |= ImGui::ColorEdit3("Highlight color", &settings.accent.x, ImGuiColorEditFlags_NoInputs);
    Hint("Used for selected items, buttons and tabs.");

    Heading("Size and shape");
    ImGui::SetNextItemWidth(220.0f);
    changed |= ImGui::SliderFloat("Text and UI size", &settings.uiScale, 0.8f, 1.5f, "%.2fx");
    ImGui::SetNextItemWidth(220.0f);
    changed |= ImGui::SliderFloat("Corner roundness", &settings.roundness, 4.0f, 24.0f, "%.0f px");
    ImGui::SetNextItemWidth(220.0f);
    changed |= ImGui::SliderFloat("Panel opacity", &settings.glassOpacity, 0.30f, 1.0f, "%.2f");
    Hint("Below 1 the panels become see-through.");

    if (ImGui::CollapsingHeader("Custom colors")) {
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
    }

    ImGui::Spacing();
    if (ImGui::Button("Restore Default Look")) {
        ResetAppearance(settings);
        changed = true;
    }
    return changed;
}

void DrawPreferencesWindow(UISettings& settings) {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(520, 560), ImGuiCond_FirstUseEver);

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse;
    if (!ImGui::Begin("Preferences", &settings.showPreferences, flags)) {
        ImGui::End();
        return;
    }

    bool changed = false;

    if (ImGui::BeginTabBar("PreferencesTabs")) {
        if (ImGui::BeginTabItem("General")) {
            changed |= DrawGeneralTab(settings);
            ImGui::EndTabItem();
        }
        if (ImGui::BeginTabItem("Layout")) {
            changed |= DrawLayoutTab(settings);
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
