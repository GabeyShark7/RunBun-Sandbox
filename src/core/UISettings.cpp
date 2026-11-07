#include "core/UISettings.h"
#include "imgui/imgui_internal.h"
#include <cstdio>
#include <cstring>

static UISettings uiSettings;

UISettings& GetUISettings() {
    return uiSettings;
}

void ResetAppearance(UISettings& settings) {
    UISettings defaults;
    settings.themePreset = defaults.themePreset;
    settings.glassOpacity = defaults.glassOpacity;
    settings.roundness = defaults.roundness;
    settings.uiScale = defaults.uiScale;
    settings.accent = defaults.accent;
}

static void* SettingsReadOpen(ImGuiContext*, ImGuiSettingsHandler*, const char* name) {
    return strcmp(name, "Preferences") == 0 ? (void*)&uiSettings : nullptr;
}

static void SettingsReadLine(ImGuiContext*, ImGuiSettingsHandler*, void*, const char* line) {
    int i;
    float f, r, g, b;
    if (sscanf(line, "ShowScene=%d", &i) == 1) uiSettings.showScene = i != 0;
    else if (sscanf(line, "ShowObjects=%d", &i) == 1) uiSettings.showObjects = i != 0;
    else if (sscanf(line, "ShowProperties=%d", &i) == 1) uiSettings.showProperties = i != 0;
    else if (sscanf(line, "ShowConsole=%d", &i) == 1) uiSettings.showConsole = i != 0;
    else if (sscanf(line, "LayoutMode=%d", &i) == 1) uiSettings.layoutMode = i;
    else if (sscanf(line, "LayoutVersion=%d", &i) == 1) uiSettings.layoutVersion = i;
    else if (sscanf(line, "Theme=%d", &i) == 1) uiSettings.themePreset = i;
    else if (sscanf(line, "GlassOpacity=%f", &f) == 1) uiSettings.glassOpacity = f;
    else if (sscanf(line, "Roundness=%f", &f) == 1) uiSettings.roundness = f;
    else if (sscanf(line, "UIScale=%f", &f) == 1) uiSettings.uiScale = f;
    else if (sscanf(line, "Accent=%f,%f,%f", &r, &g, &b) == 3) uiSettings.accent = ImVec4(r, g, b, 1.0f);
}

static void SettingsWriteAll(ImGuiContext*, ImGuiSettingsHandler* handler, ImGuiTextBuffer* buf) {
    buf->appendf("[%s][Preferences]\n", handler->TypeName);
    buf->appendf("ShowScene=%d\n", uiSettings.showScene ? 1 : 0);
    buf->appendf("ShowObjects=%d\n", uiSettings.showObjects ? 1 : 0);
    buf->appendf("ShowProperties=%d\n", uiSettings.showProperties ? 1 : 0);
    buf->appendf("ShowConsole=%d\n", uiSettings.showConsole ? 1 : 0);
    buf->appendf("LayoutMode=%d\n", uiSettings.layoutMode);
    buf->appendf("LayoutVersion=%d\n", uiSettings.layoutVersion);
    buf->appendf("Theme=%d\n", uiSettings.themePreset);
    buf->appendf("GlassOpacity=%.3f\n", uiSettings.glassOpacity);
    buf->appendf("Roundness=%.1f\n", uiSettings.roundness);
    buf->appendf("UIScale=%.2f\n", uiSettings.uiScale);
    buf->appendf("Accent=%.3f,%.3f,%.3f\n", uiSettings.accent.x, uiSettings.accent.y, uiSettings.accent.z);
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
