#pragma once
#include "imgui/imgui.h"

enum WorkMode {
    WorkMode_Tinker = 0,
    WorkMode_Mix = 1
};

enum Workspace {
    Workspace_Layout = 0,
    Workspace_Edit = 1,
    Workspace_Texturing = 2,
    Workspace_Shading = 3,
    Workspace_Count = 4
};

enum LayoutMode {
    LayoutMode_Floating = 0,
    LayoutMode_Docked = 1
};

struct UISettings {
    bool showScene = true;
    bool showObjects = true;
    bool showProperties = true;
    bool showConsole = false;
    bool showMessages = true;
    bool lockLayout = false;
    bool showPreferences = false;
    bool showGrid = true;
    bool showEdit = true;
    int workMode = WorkMode_Mix;
    int workspace = Workspace_Layout;
    bool showTransformGizmo = true;
    bool showDimensions = true;
    bool showStatistics = false;
    bool helpMinimal = false;
    bool cameraOrbit = false;
    bool orbitFlipX = false;
    bool orbitFlipY = false;
    bool editShowHidden = false;
    bool showHandles = true;
    bool showShapes = true;
    bool showMods = true;
    ImVec4 sceneBackground = ImVec4(0.239f, 0.239f, 0.239f, 1.0f);
    ImVec4 sceneBackgroundBottom = ImVec4(0.11f, 0.11f, 0.12f, 1.0f);
    int backgroundMode = 0;
    char backgroundImage[512] = "";
    int viewStyle = 0;
    bool shadows = true;
    bool showLights = true;
    bool snapEnabled = false;
    bool snapToPoints = false;
    float snapUnitMm = 100.0f;

    float viewFov = 55.0f;
    float nearClipMm = 50.0f;
    float farClipMm = 1000000.0f;
    bool resetLayout = false;

    int layoutMode = LayoutMode_Docked;
    int layoutVersion = 0;

    int themePreset = 0;
    float glassOpacity = 1.0f;
    float roundness = 8.0f;
    float uiScale = 1.0f;
    ImVec4 accent = ImVec4(0.26f, 0.56f, 0.96f, 1.0f);

    bool useCustomColors = false;
    ImVec4 customBackdrop = ImVec4(0.075f, 0.075f, 0.075f, 1.0f);
    ImVec4 customPanel = ImVec4(0.135f, 0.135f, 0.135f, 1.0f);
    ImVec4 customText = ImVec4(0.95f, 0.95f, 0.97f, 1.0f);
    ImVec4 customBorder = ImVec4(1.0f, 1.0f, 1.0f, 0.14f);
};

UISettings& GetUISettings();
bool ShowHelpText();
void ResetAppearance(UISettings& settings);
void RegisterUISettingsHandler();
