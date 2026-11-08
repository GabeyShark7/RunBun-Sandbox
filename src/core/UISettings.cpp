#include "core/UISettings.h"
#include "imgui/imgui_internal.h"
#include <cstdio>
#include <cstring>

static UISettings uiSettings;

UISettings& GetUISettings() {
    return uiSettings;
}

bool ShowHelpText() {
    return !uiSettings.helpMinimal;
}

void ResetAppearance(UISettings& settings) {
    UISettings defaults;
    settings.themePreset = defaults.themePreset;
    settings.glassOpacity = defaults.glassOpacity;
    settings.roundness = defaults.roundness;
    settings.uiScale = defaults.uiScale;
    settings.accent = defaults.accent;
    settings.useCustomColors = defaults.useCustomColors;
}

static void* SettingsReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name) {
    return strcmp(name, "Preferences") == 0 ? (void*)&uiSettings : nullptr;
}

static void SettingsReadLine(ImGuiContext*, ImGuiSettingsHandler*, void*, const char* line) {
    int i;
    float f, r, g, b, a;
    if (sscanf(line, "ShowScene=%d", &i) == 1) uiSettings.showScene = i != 0;
    else if (sscanf(line, "ShowObjects=%d", &i) == 1) uiSettings.showObjects = i != 0;
    else if (sscanf(line, "ShowProperties=%d", &i) == 1) uiSettings.showProperties = i != 0;
    else if (sscanf(line, "ShowConsole=%d", &i) == 1) uiSettings.showConsole = i != 0;
    else if (sscanf(line, "ShowMessages=%d", &i) == 1) uiSettings.showMessages = i != 0;
    else if (sscanf(line, "LockLayout=%d", &i) == 1) uiSettings.lockLayout = i != 0;
    else if (sscanf(line, "ShowGrid=%d", &i) == 1) uiSettings.showGrid = i != 0;
    else if (sscanf(line, "ShowTransformGizmo=%d", &i) == 1) uiSettings.showTransformGizmo = i != 0;
    else if (sscanf(line, "ShowDimensions=%d", &i) == 1) uiSettings.showDimensions = i != 0;
    else if (sscanf(line, "ShowStatistics=%d", &i) == 1) uiSettings.showStatistics = i != 0;
    else if (sscanf(line, "HelpMinimal=%d", &i) == 1) uiSettings.helpMinimal = i != 0;
    else if (sscanf(line, "CameraOrbit=%d", &i) == 1) uiSettings.cameraOrbit = i != 0;
    else if (sscanf(line, "OrbitFlipX=%d", &i) == 1) uiSettings.orbitFlipX = i != 0;
    else if (sscanf(line, "OrbitFlipY=%d", &i) == 1) uiSettings.orbitFlipY = i != 0;
    else if (sscanf(line, "EditShowHidden=%d", &i) == 1) uiSettings.editShowHidden = i != 0;
    else if (sscanf(line, "ShowHandles=%d", &i) == 1) uiSettings.showHandles = i != 0;
    else if (sscanf(line, "ShowShapes=%d", &i) == 1) uiSettings.showShapes = i != 0;
    else if (sscanf(line, "ShowMods=%d", &i) == 1) uiSettings.showMods = i != 0;
    else if (sscanf(line, "SceneGrey=%f,%f,%f", &r, &g, &b) == 3) uiSettings.sceneBackground = ImVec4(r, g, b, 1.0f);
    else if (sscanf(line, "SceneBottom=%f,%f,%f", &r, &g, &b) == 3) uiSettings.sceneBackgroundBottom = ImVec4(r, g, b, 1.0f);
    else if (sscanf(line, "BackgroundMode=%d", &i) == 1) uiSettings.backgroundMode = (i >= 0 && i <= 2) ? i : 0;
    else if (strncmp(line, "BackgroundImage=", 16) == 0) snprintf(uiSettings.backgroundImage, sizeof(uiSettings.backgroundImage), "%s", line + 16);
    else if (sscanf(line, "ViewStyle=%d", &i) == 1) uiSettings.viewStyle = (i >= 0 && i <= 3) ? i : 0;
    else if (sscanf(line, "Shadows=%d", &i) == 1) uiSettings.shadows = i != 0;
    else if (sscanf(line, "ShowLights=%d", &i) == 1) uiSettings.showLights = i != 0;
    else if (sscanf(line, "SnapEnabled=%d", &i) == 1) uiSettings.snapEnabled = i != 0;
    else if (sscanf(line, "SnapToPoints=%d", &i) == 1) uiSettings.snapToPoints = i != 0;
    else if (sscanf(line, "SnapUnitMm=%f", &f) == 1) uiSettings.snapUnitMm = f > 0.0f ? f : 100.0f;
    else if (sscanf(line, "ViewFov=%f", &f) == 1) uiSettings.viewFov = f;
    else if (sscanf(line, "ShowEdit=%d", &i) == 1) uiSettings.showEdit = i != 0;
    else if (sscanf(line, "WorkMode=%d", &i) == 1) uiSettings.workMode = (i == WorkMode_Tinker) ? i : WorkMode_Mix;
    else if (sscanf(line, "Workspace=%d", &i) == 1) uiSettings.workspace = (i >= 0 && i < Workspace_Count) ? i : Workspace_Layout;
    else if (sscanf(line, "NearClipMm=%f", &f) == 1) uiSettings.nearClipMm = f;
    else if (sscanf(line, "FarClipMm=%f", &f) == 1) uiSettings.farClipMm = f;
    else if (sscanf(line, "LayoutMode=%d", &i) == 1) uiSettings.layoutMode = i;
    else if (sscanf(line, "LayoutVersion=%d", &i) == 1) uiSettings.layoutVersion = i;
    else if (sscanf(line, "Theme=%d", &i) == 1) uiSettings.themePreset = i;
    else if (sscanf(line, "GlassOpacity=%f", &f) == 1) uiSettings.glassOpacity = f;
    else if (sscanf(line, "Roundness=%f", &f) == 1) uiSettings.roundness = f;
    else if (sscanf(line, "UIScale=%f", &f) == 1) uiSettings.uiScale = f;
    else if (sscanf(line, "Accent=%f,%f,%f", &r, &g, &b) == 3) uiSettings.accent = ImVec4(r, g, b, 1.0f);
    else if (sscanf(line, "UseCustomColors=%d", &i) == 1) uiSettings.useCustomColors = i != 0;
    else if (sscanf(line, "ColorBackdrop=%f,%f,%f,%f", &r, &g, &b, &a) == 4) uiSettings.customBackdrop = ImVec4(r, g, b, a);
    else if (sscanf(line, "ColorPanel=%f,%f,%f,%f", &r, &g, &b, &a) == 4) uiSettings.customPanel = ImVec4(r, g, b, a);
    else if (sscanf(line, "ColorText=%f,%f,%f,%f", &r, &g, &b, &a) == 4) uiSettings.customText = ImVec4(r, g, b, a);
    else if (sscanf(line, "ColorBorder=%f,%f,%f,%f", &r, &g, &b, &a) == 4) uiSettings.customBorder = ImVec4(r, g, b, a);
}

static void SettingsWriteAll(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf) {
    buf->appendf("[%s][Preferences]\n", handler->TypeName);
    buf->appendf("ShowScene=%d\n", uiSettings.showScene ? 1 : 0);
    buf->appendf("ShowObjects=%d\n", uiSettings.showObjects ? 1 : 0);
    buf->appendf("ShowProperties=%d\n", uiSettings.showProperties ? 1 : 0);
    buf->appendf("ShowConsole=%d\n", uiSettings.showConsole ? 1 : 0);
    buf->appendf("ShowMessages=%d\n", uiSettings.showMessages ? 1 : 0);
    buf->appendf("LockLayout=%d\n", uiSettings.lockLayout ? 1 : 0);
    buf->appendf("ShowGrid=%d\n", uiSettings.showGrid ? 1 : 0);
    buf->appendf("ShowTransformGizmo=%d\n", uiSettings.showTransformGizmo ? 1 : 0);
    buf->appendf("ShowDimensions=%d\n", uiSettings.showDimensions ? 1 : 0);
    buf->appendf("ShowStatistics=%d\n", uiSettings.showStatistics ? 1 : 0);
    buf->appendf("HelpMinimal=%d\n", uiSettings.helpMinimal ? 1 : 0);
    buf->appendf("CameraOrbit=%d\n", uiSettings.cameraOrbit ? 1 : 0);
    buf->appendf("OrbitFlipX=%d\n", uiSettings.orbitFlipX ? 1 : 0);
    buf->appendf("OrbitFlipY=%d\n", uiSettings.orbitFlipY ? 1 : 0);
    buf->appendf("EditShowHidden=%d\n", uiSettings.editShowHidden ? 1 : 0);
    buf->appendf("ShowHandles=%d\n", uiSettings.showHandles ? 1 : 0);
    buf->appendf("ShowShapes=%d\n", uiSettings.showShapes ? 1 : 0);
    buf->appendf("ShowMods=%d\n", uiSettings.showMods ? 1 : 0);
    buf->appendf("SceneGrey=%.3f,%.3f,%.3f\n", uiSettings.sceneBackground.x, uiSettings.sceneBackground.y, uiSettings.sceneBackground.z);
    buf->appendf("SceneBottom=%.3f,%.3f,%.3f\n", uiSettings.sceneBackgroundBottom.x, uiSettings.sceneBackgroundBottom.y, uiSettings.sceneBackgroundBottom.z);
    buf->appendf("BackgroundMode=%d\n", uiSettings.backgroundMode);
    buf->appendf("BackgroundImage=%s\n", uiSettings.backgroundImage);
    buf->appendf("ViewStyle=%d\n", uiSettings.viewStyle);
    buf->appendf("Shadows=%d\n", uiSettings.shadows ? 1 : 0);
    buf->appendf("ShowLights=%d\n", uiSettings.showLights ? 1 : 0);
    buf->appendf("SnapEnabled=%d\n", uiSettings.snapEnabled ? 1 : 0);
    buf->appendf("SnapToPoints=%d\n", uiSettings.snapToPoints ? 1 : 0);
    buf->appendf("SnapUnitMm=%.3f\n", uiSettings.snapUnitMm);
    buf->appendf("ViewFov=%.2f\n", uiSettings.viewFov);
    buf->appendf("ShowEdit=%d\n", uiSettings.showEdit ? 1 : 0);
    buf->appendf("WorkMode=%d\n", uiSettings.workMode);
    buf->appendf("NearClipMm=%.3f\n", uiSettings.nearClipMm);
    buf->appendf("FarClipMm=%.3f\n", uiSettings.farClipMm);
    buf->appendf("LayoutMode=%d\n", uiSettings.layoutMode);
    buf->appendf("LayoutVersion=%d\n", uiSettings.layoutVersion);
    buf->appendf("Theme=%d\n", uiSettings.themePreset);
    buf->appendf("GlassOpacity=%.3f\n", uiSettings.glassOpacity);
    buf->appendf("Roundness=%.1f\n", uiSettings.roundness);
    buf->appendf("UIScale=%.2f\n", uiSettings.uiScale);
    buf->appendf("Accent=%.3f,%.3f,%.3f\n", uiSettings.accent.x, uiSettings.accent.y, uiSettings.accent.z);
    buf->appendf("UseCustomColors=%d\n", uiSettings.useCustomColors ? 1 : 0);
    buf->appendf("ColorBackdrop=%.3f,%.3f,%.3f,%.3f\n", uiSettings.customBackdrop.x, uiSettings.customBackdrop.y, uiSettings.customBackdrop.z, uiSettings.customBackdrop.w);
    buf->appendf("ColorPanel=%.3f,%.3f,%.3f,%.3f\n", uiSettings.customPanel.x, uiSettings.customPanel.y, uiSettings.customPanel.z, uiSettings.customPanel.w);
    buf->appendf("ColorText=%.3f,%.3f,%.3f,%.3f\n", uiSettings.customText.x, uiSettings.customText.y, uiSettings.customText.z, uiSettings.customText.w);
    buf->appendf("ColorBorder=%.3f,%.3f,%.3f,%.3f\n", uiSettings.customBorder.x, uiSettings.customBorder.y, uiSettings.customBorder.z, uiSettings.customBorder.w);
    buf->append("\n");
}

void RegisterUISettingsHandler() {
    ImGuiSettingsHandler handler;
    handler.TypeName = "RunBun";
    handler.TypeHash = ImHashStr("RunBun");
    handler.ReadOpenFn = SettingsReadOpen;
    handler.ReadLineFn = SettingsReadLine;
    handler.WriteAllFn = SettingsWriteAll;
    ImGui::AddSettingsHandler(&handler);
}
