#include "ui/SceneWindow.h"
#include "ui/PanelHeader.h"
#include "ui/CreateMenu.h"
#include "render/SceneRenderer.h"
#include "ui/SceneGizmo.h"
#include "core/UISettings.h"
#include "imgui/imgui_internal.h"
#include <cstdint>

static bool sceneHovered = false;
static PanelCollapseState collapseState = { false, 600.0f };

ImVec2 DrawSceneWindow(SceneRenderer& sceneRenderer, UISettings& settings, bool* open) {
    unsigned int textureID = sceneRenderer.GetTexture();
    Camera& camera = sceneRenderer.GetCamera();

    if (!ImGui::Begin("Scene", open, ImGuiWindowFlags_NoCollapse)) {
        sceneHovered = false;
        ImGui::End();
        return ImVec2(0, 0);
    }

    bool expanded = DrawPanelHeader(collapseState);

    sceneHovered = ImGui::IsWindowHovered();

    ImVec2 panelSize = ImVec2(0, 0);

    if (expanded) {
        panelSize = ImGui::GetContentRegionAvail();
        ImVec2 imageMin = ImGui::GetCursorScreenPos();
        ImVec2 imageMax = ImVec2(imageMin.x + panelSize.x, imageMin.y + panelSize.y);

        if (panelSize.x < 1.0f || panelSize.y < 1.0f) {
            ImGui::End();
            return ImVec2(0, 0);
        }

        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton("SceneViewport", panelSize);
        bool viewHovered = ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenOverlappedByItem);
        bool viewActivated = ImGui::IsItemActivated();
        bool viewActive = ImGui::IsItemActive();

        float rounding = ImGui::GetStyle().WindowRounding * 0.75f;
        ImGui::GetWindowDrawList()->AddImageRounded((ImTextureID)(intptr_t)textureID, imageMin, imageMax, ImVec2(0, 1), ImVec2(1, 0), IM_COL32_WHITE, rounding);
        DrawSceneGizmo(sceneRenderer, settings, imageMin, panelSize, viewHovered, viewActivated, viewActive);

        // Reset camera button sits on top of the scene image, top right corner
        const char* resetLabel = "Reset Camera";
        ImVec2 padding = ImGui::GetStyle().FramePadding;
        float buttonWidth = ImGui::CalcTextSize(resetLabel).x + padding.x * 2.0f;

        ImGui::SetCursorScreenPos(ImVec2(imageMax.x - buttonWidth - 10.0f, imageMin.y + 10.0f));
        if (ImGui::Button(resetLabel)) {
            camera.Reset();
        }

        const char* gizmosLabel = "Gizmos";
        float arrowSpace = ImGui::GetFontSize() * 1.2f;
        float gizmosWidth = ImGui::CalcTextSize(gizmosLabel).x + padding.x * 2.0f + arrowSpace;
        ImGui::SetCursorScreenPos(ImVec2(imageMax.x - buttonWidth - gizmosWidth - 10.0f - ImGui::GetStyle().ItemSpacing.x, imageMin.y + 10.0f));
        bool gizmosOpen = ImGui::IsPopupOpen("SceneGizmosMenu");
        if (gizmosOpen) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
        bool gizmosClicked = ImGui::Button("Gizmos##SceneGizmosButton", ImVec2(gizmosWidth, 0.0f));
        ImGui::PopStyleVar();
        if (gizmosClicked) {
            ImGui::OpenPopup("SceneGizmosMenu");
        }
        if (gizmosOpen) ImGui::PopStyleColor();
        ImVec2 buttonMin = ImGui::GetItemRectMin();
        ImVec2 buttonMax = ImGui::GetItemRectMax();
        {
            float cx = buttonMax.x - padding.x - arrowSpace * 0.35f;
            float cy = (buttonMin.y + buttonMax.y) * 0.5f + 1.0f;
            float h = arrowSpace * 0.22f;
            ImGui::GetWindowDrawList()->AddTriangleFilled(ImVec2(cx - h * 1.4f, cy - h), ImVec2(cx + h * 1.4f, cy - h), ImVec2(cx, cy + h), ImGui::GetColorU32(ImGuiCol_Text));
        }
        ImGui::SetNextWindowPos(ImVec2(buttonMax.x, buttonMax.y + 6.0f), ImGuiCond_Appearing, ImVec2(1.0f, 0.0f));
        if (ImGui::BeginPopup("SceneGizmosMenu")) {
            bool changed = false;
            changed |= ImGui::Checkbox("Grid", &settings.showGrid);
            changed |= ImGui::Checkbox("Transform Arrows", &settings.showTransformGizmo);
            changed |= ImGui::Checkbox("Snapping", &settings.snapEnabled);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(ImGui::CalcTextSize("00000.0 mm").x + padding.x * 2.0f);
            ImGui::BeginDisabled(!settings.snapEnabled);
            changed |= ImGui::DragFloat("##SnapUnit", &settings.snapUnitMm, 1.0f, 0.1f, 100000.0f, "%.1f mm", ImGuiSliderFlags_AlwaysClamp);
            ImGui::EndDisabled();
            if (changed) ImGui::MarkIniSettingsDirty();
            ImGui::EndPopup();
        }

        const char* viewLabel = "View";
        float viewWidth = ImGui::CalcTextSize(viewLabel).x + padding.x * 2.0f + arrowSpace;
        ImVec2 viewButtonPos = ImVec2(buttonMin.x - viewWidth - ImGui::GetStyle().ItemSpacing.x, buttonMin.y);
        ImGui::SetCursorScreenPos(viewButtonPos);
        bool viewOpen = ImGui::IsPopupOpen("SceneViewMenu");
        if (viewOpen) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
        bool viewClicked = ImGui::Button("View##SceneViewButton", ImVec2(viewWidth, 0.0f));
        ImGui::PopStyleVar();
        if (viewOpen) ImGui::PopStyleColor();
        if (viewClicked) ImGui::OpenPopup("SceneViewMenu");
        ImVec2 viewMin = ImGui::GetItemRectMin();
        ImVec2 viewMax = ImGui::GetItemRectMax();
        {
            float cx = viewMax.x - padding.x - arrowSpace * 0.35f;
            float cy = (viewMin.y + viewMax.y) * 0.5f + 1.0f;
            float h = arrowSpace * 0.22f;
            ImGui::GetWindowDrawList()->AddTriangleFilled(ImVec2(cx - h * 1.4f, cy - h), ImVec2(cx + h * 1.4f, cy - h), ImVec2(cx, cy + h), ImGui::GetColorU32(ImGuiCol_Text));
        }
        ImGui::SetNextWindowPos(ImVec2(viewMax.x, viewMax.y + 6.0f), ImGuiCond_Appearing, ImVec2(1.0f, 0.0f));
        if (ImGui::BeginPopup("SceneViewMenu")) {
            bool changed = false;
            float fieldWidth = ImGui::CalcTextSize("0000000.0 mm").x + padding.x * 2.0f;
            ImGui::TextDisabled("Camera");
            ImGui::SetNextItemWidth(fieldWidth);
            changed |= ImGui::SliderFloat("Field of View", &settings.viewFov, 20.0f, 120.0f, "%.0f\u00b0");
            ImGui::Spacing();
            ImGui::TextDisabled("Clipping");
            ImGui::SetNextItemWidth(fieldWidth);
            changed |= ImGui::DragFloat("Near", &settings.nearClipMm, 1.0f, 1.0f, settings.farClipMm * 0.5f, "%.1f mm", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic);
            ImGui::SetNextItemWidth(fieldWidth);
            changed |= ImGui::DragFloat("Far", &settings.farClipMm, 1000.0f, settings.nearClipMm * 2.0f, 10000000.0f, "%.0f mm", ImGuiSliderFlags_AlwaysClamp | ImGuiSliderFlags_Logarithmic);
            ImGui::Spacing();
            if (ImGui::Button("Reset View Settings")) {
                UISettings defaults;
                settings.viewFov = defaults.viewFov;
                settings.nearClipMm = defaults.nearClipMm;
                settings.farClipMm = defaults.farClipMm;
                changed = true;
            }
            if (changed) ImGui::MarkIniSettingsDirty();
            ImGui::EndPopup();
        }

        ImGuiIO& io = ImGui::GetIO();
        bool createShortcut = io.KeyCtrl && !io.WantTextInput && !IsSceneTransformActive() &&
            (ImGui::IsKeyPressed(ImGuiKey_A, false) || ImGui::IsMouseClicked(ImGuiMouseButton_Right));
        if (sceneHovered && createShortcut) {
            ImGui::OpenPopup("SceneCreateMenu");
        }
        if (ImGui::BeginPopup("SceneCreateMenu")) {
            DrawCreateMenuItems(sceneRenderer);
            ImGui::EndPopup();
        }
    }

    ImGui::End();

    return panelSize;
}

bool IsSceneHovered() {
    return sceneHovered;
}
