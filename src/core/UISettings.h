#pragma once
#include "imgui/imgui.h"

enum WorkMode {
    WorkMode_Tinker = 0,
    WorkMode_Mix = 1
};

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
    bool showGrid = true;
    bool showEdit = true;
    int workMode = WorkMode_Mix;
    bool showTransformGizmo = true;
    bool snapEnabled = false;
    float snapUnitMm = 100.0f;

    float viewFov = 55.0f;
    float nearClipMm = 50.0f;
    float farClipMm = 1000000.0f;
    bool resetLayout = false;

    int layoutMode = LayoutMode_Floating;
    int layoutVersion = 0;

    int themePreset = 0;
    float glassOpacity = 0.72f;
    float roundness = 14.0f;
    float uiScale = 1.0f;
    ImVec4 accent = ImVec4(0.55f, 0.45f, 1.0f, 1.0f);

    bool useCustomColors = false;
    ImVec4 customBackdrop = ImVec4(0.07f, 0.07f, 0.09f, 1.0f);
    ImVec4 customPanel = ImVec4(0.16f, 0.16f, 0.20f, 1.0f);
    ImVec4 customText = ImVec4(0.95f, 0.95f, 0.97f, 1.0f);
    ImVec4 customBorder = ImVec4(1.0f, 1.0f, 1.0f, 0.14f);
};

UISettings& GetUISettings();
void ResetAppearance(UISettings& settings);
void RegisterUISettingsHandler();
