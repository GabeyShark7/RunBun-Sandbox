#pragma once
#include "imgui/imgui.h"

enum LayoutMode {
    LayoutMode_Floating = 0,
    LayoutMode_Docked = 1
};

struct UISettings {
    bool showScene = true;
    bool showObjects = true;
    bool showProperties = true;
    bool showConsole = true;
    bool showPreferences = false;
    bool resetLayout = false;

    int layoutMode = LayoutMode_Floating;
    int layoutVersion = 0;

    int themePreset = 0;
    float glassOpacity = 0.72f;
    float roundness = 14.0f;
    float uiScale = 1.0f;
    ImVec4 accent = ImVec4(0.55f, 0.45f, 1.0f, 1.0f);
};

UISettings& GetUISettings();
void ResetAppearance(UISettings& settings);
void RegisterUISettingsHandler();
