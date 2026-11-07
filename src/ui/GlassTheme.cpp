#include "ui/GlassTheme.h"
#include <cfloat>

struct ThemePreset {
    const char* name;
    ImVec4 backdrop;
    ImVec4 glassTint;
    ImVec4 text;
    ImVec4 overlay;
    ImVec4 border;
    ImVec4 popupBg;
    ImVec4 accent;
    float glassBoost;
};

static const ThemePreset themes[] = {
    { "Dark",
      ImVec4(0.07f, 0.07f, 0.09f, 1.0f), ImVec4(0.16f, 0.16f, 0.20f, 1.0f),
      ImVec4(0.95f, 0.95f, 0.97f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
      ImVec4(1.0f, 1.0f, 1.0f, 0.14f), ImVec4(0.13f, 0.13f, 0.17f, 0.97f),
      ImVec4(0.55f, 0.45f, 1.0f, 1.0f), 0.0f },
    { "Light",
      ImVec4(0.86f, 0.87f, 0.91f, 1.0f), ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
      ImVec4(0.10f, 0.11f, 0.15f, 1.0f), ImVec4(0.0f, 0.0f, 0.0f, 1.0f),
      ImVec4(0.0f, 0.0f, 0.0f, 0.12f), ImVec4(0.98f, 0.98f, 1.0f, 0.98f),
      ImVec4(0.25f, 0.45f, 0.95f, 1.0f), 0.15f },
    { "Midnight",
      ImVec4(0.03f, 0.05f, 0.11f, 1.0f), ImVec4(0.08f, 0.12f, 0.25f, 1.0f),
      ImVec4(0.90f, 0.94f, 1.0f, 1.0f), ImVec4(0.75f, 0.85f, 1.0f, 1.0f),
      ImVec4(0.60f, 0.75f, 1.0f, 0.20f), ImVec4(0.07f, 0.10f, 0.22f, 0.97f),
      ImVec4(0.30f, 0.65f, 1.0f, 1.0f), 0.0f },
    { "Crimson",
      ImVec4(0.08f, 0.03f, 0.05f, 1.0f), ImVec4(0.25f, 0.04f, 0.10f, 1.0f),
      ImVec4(0.98f, 0.94f, 0.95f, 1.0f), ImVec4(1.0f, 0.85f, 0.90f, 1.0f),
      ImVec4(1.0f, 0.60f, 0.70f, 0.18f), ImVec4(0.18f, 0.04f, 0.08f, 0.97f),
      ImVec4(0.80f, 0.15f, 0.35f, 1.0f), 0.0f },
    { "Maroon",
      ImVec4(0.025f, 0.018f, 0.020f, 1.0f), ImVec4(0.105f, 0.035f, 0.045f, 1.0f),
      ImVec4(0.97f, 0.93f, 0.93f, 1.0f), ImVec4(1.0f, 0.80f, 0.80f, 1.0f),
      ImVec4(0.90f, 0.20f, 0.25f, 0.22f), ImVec4(0.08f, 0.025f, 0.032f, 0.98f),
      ImVec4(0.78f, 0.08f, 0.14f, 1.0f), 0.0f },
};

static const int themeCount = sizeof(themes) / sizeof(themes[0]);

static const ThemePreset& GetTheme(int index) {
    if (index < 0 || index >= themeCount) return themes[0];
    return themes[index];
}

const char* GetThemeName(int index) {
    if (index < 0 || index >= themeCount) return "";
    return themes[index].name;
}

int GetThemeCount() {
    return themeCount;
}

ImVec4 GetThemeAccent(int index) {
    return GetTheme(index).accent;
}

void CopyThemeColorsToCustom(UISettings& settings) {
    const ThemePreset& theme = GetTheme(settings.themePreset);
    settings.customBackdrop = theme.backdrop;
    settings.customPanel = theme.glassTint;
    settings.customText = theme.text;
    settings.customBorder = theme.border;
}

static ThemePreset ResolveTheme(const UISettings& settings) {
    ThemePreset theme = GetTheme(settings.themePreset);
    if (!settings.useCustomColors) return theme;

    theme.backdrop = settings.customBackdrop;
    theme.glassTint = settings.customPanel;
    theme.text = settings.customText;
    theme.border = settings.customBorder;
    theme.overlay = ImVec4(settings.customText.x, settings.customText.y, settings.customText.z, 1.0f);
    ImVec4 panel = settings.customPanel;
    theme.popupBg = ImVec4(panel.x * 0.85f, panel.y * 0.85f, panel.z * 0.85f, 0.98f);
    return theme;
}

static ImVec4 WithAlpha(const ImVec4& color, float alpha) {
    return ImVec4(color.x, color.y, color.z, alpha);
}

static ImVec4 Mix(const ImVec4& a, const ImVec4& b, float t) {
    return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t);
}

static float Clamp01(float value) {
    return value < 0.0f ? 0.0f : (value > 1.0f ? 1.0f : value);
}

void ApplyGlassStyle(const UISettings& settings) {
    const ThemePreset theme = ResolveTheme(settings);
    ImGuiStyle& style = ImGui::GetStyle();
    float r = settings.roundness < 4.0f ? 4.0f : settings.roundness;
    float glass = Clamp01(settings.glassOpacity + theme.glassBoost);
    ImVec4 accent = settings.accent;
    ImVec4 accentSoft = Mix(accent, theme.text, 0.25f);
    ImVec4 overlay = theme.overlay;

    style.FontScaleMain = settings.uiScale;

    style.WindowRounding = r;
    style.ChildRounding = r;
    style.PopupRounding = r * 0.75f;
    style.FrameRounding = r;
    style.GrabRounding = r;
    style.ScrollbarRounding = r;
    style.TabRounding = r * 0.6f;
    style.TabBarBorderSize = 0.0f;
    style.TabBarOverlineSize = 0.0f;
    style.TabCloseButtonMinWidthSelected = FLT_MAX;
    style.TabCloseButtonMinWidthUnselected = FLT_MAX;

    style.WindowBorderSize = 1.0f;
    style.ChildBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;
    style.FrameBorderSize = 1.0f;
    style.TabBorderSize = 0.0f;

    style.WindowPadding = ImVec2(14.0f, 12.0f);
    style.FramePadding = ImVec2(12.0f, 6.0f);
    style.ItemSpacing = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing = ImVec2(8.0f, 6.0f);
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 12.0f;
    style.WindowMinSize = ImVec2(160.0f, 90.0f);
    style.WindowBorderHoverPadding = 8.0f;
    style.DockingSeparatorSize = 10.0f;
    style.WindowMenuButtonPosition = ImGuiDir_None;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = theme.text;
    colors[ImGuiCol_TextDisabled] = WithAlpha(theme.text, 0.50f);
    colors[ImGuiCol_WindowBg] = WithAlpha(theme.glassTint, glass);
    colors[ImGuiCol_ChildBg] = WithAlpha(overlay, 0.03f);
    colors[ImGuiCol_PopupBg] = theme.popupBg;
    colors[ImGuiCol_Border] = theme.border;
    colors[ImGuiCol_BorderShadow] = WithAlpha(overlay, 0.0f);

    colors[ImGuiCol_FrameBg] = WithAlpha(overlay, 0.08f);
    colors[ImGuiCol_FrameBgHovered] = WithAlpha(overlay, 0.14f);
    colors[ImGuiCol_FrameBgActive] = WithAlpha(overlay, 0.20f);

    colors[ImGuiCol_TitleBg] = WithAlpha(theme.glassTint, Clamp01(glass + 0.05f));
    colors[ImGuiCol_TitleBgActive] = WithAlpha(Mix(theme.glassTint, accent, 0.18f), Clamp01(glass + 0.12f));
    colors[ImGuiCol_TitleBgCollapsed] = WithAlpha(theme.glassTint, glass * 0.7f);
    colors[ImGuiCol_MenuBarBg] = WithAlpha(theme.glassTint, Clamp01(glass + 0.10f));

    colors[ImGuiCol_ScrollbarBg] = WithAlpha(overlay, 0.0f);
    colors[ImGuiCol_ScrollbarGrab] = WithAlpha(overlay, 0.20f);
    colors[ImGuiCol_ScrollbarGrabHovered] = WithAlpha(overlay, 0.30f);
    colors[ImGuiCol_ScrollbarGrabActive] = WithAlpha(overlay, 0.40f);

    colors[ImGuiCol_CheckMark] = accentSoft;
    colors[ImGuiCol_SliderGrab] = accent;
    colors[ImGuiCol_SliderGrabActive] = accentSoft;

    colors[ImGuiCol_Button] = WithAlpha(overlay, 0.10f);
    colors[ImGuiCol_ButtonHovered] = WithAlpha(overlay, 0.18f);
    colors[ImGuiCol_ButtonActive] = WithAlpha(accent, 0.75f);

    colors[ImGuiCol_Header] = WithAlpha(accent, 0.30f);
    colors[ImGuiCol_HeaderHovered] = WithAlpha(overlay, 0.14f);
    colors[ImGuiCol_HeaderActive] = WithAlpha(accent, 0.55f);

    colors[ImGuiCol_Separator] = WithAlpha(overlay, 0.12f);
    colors[ImGuiCol_SeparatorHovered] = WithAlpha(accent, 0.80f);
    colors[ImGuiCol_SeparatorActive] = accent;

    colors[ImGuiCol_ResizeGrip] = WithAlpha(overlay, 0.10f);
    colors[ImGuiCol_ResizeGripHovered] = WithAlpha(accent, 0.60f);
    colors[ImGuiCol_ResizeGripActive] = accent;

    colors[ImGuiCol_Tab] = WithAlpha(overlay, 0.0f);
    colors[ImGuiCol_TabHovered] = WithAlpha(overlay, 0.10f);
    colors[ImGuiCol_TabSelected] = WithAlpha(overlay, 0.08f);
    colors[ImGuiCol_TabSelectedOverline] = WithAlpha(accent, 0.0f);
    colors[ImGuiCol_TabDimmed] = WithAlpha(overlay, 0.0f);
    colors[ImGuiCol_TabDimmedSelected] = WithAlpha(overlay, 0.05f);
    colors[ImGuiCol_TabDimmedSelectedOverline] = WithAlpha(accent, 0.0f);

    colors[ImGuiCol_DockingPreview] = WithAlpha(accent, 0.55f);
    colors[ImGuiCol_DockingEmptyBg] = WithAlpha(overlay, 0.0f);

    colors[ImGuiCol_TextSelectedBg] = WithAlpha(accent, 0.40f);
    colors[ImGuiCol_DragDropTarget] = accent;
    colors[ImGuiCol_NavCursor] = accent;
    colors[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.35f);
}

void DrawThemeBackdrop(const UISettings& settings) {
    const ThemePreset theme = ResolveTheme(settings);
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImVec2 max = ImVec2(viewport->Pos.x + viewport->Size.x, viewport->Pos.y + viewport->Size.y);
    ImGui::GetBackgroundDrawList(viewport)->AddRectFilled(viewport->Pos, max, ImGui::ColorConvertFloat4ToU32(theme.backdrop));
}
