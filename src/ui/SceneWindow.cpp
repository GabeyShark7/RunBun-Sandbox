#include <cstring>
#include "ui/FileDialog.h"
#include "core/Profiler.h"
#include "ui/SceneWindow.h"
#include "ui/PanelHeader.h"
#include "ui/DockLayout.h"
#include "ui/CreateMenu.h"
#include "render/SceneRenderer.h"
#include "ui/SceneGizmo.h"
#include "ui/SceneCommands.h"
#include "ui/Statistics.h"
#include "ui/SceneNavigation.h"
#include "ui/QuickMenu.h"
#include "ui/RoomSync.h"
#include <cmath>
#include <cstdio>
#include "ui/EditMode.h"
#include "ui/ShapesWindow.h"
#include "ui/TinkerHandles.h"
#include "core/UISettings.h"
#include "imgui/imgui_internal.h"
#include <cstdint>

static bool sceneHovered = false;
static PanelCollapseState collapseState = { false, 600.0f };

ImVec2 DrawSceneWindow(SceneRenderer& sceneRenderer, UISettings& settings, bool* open) {
    PROFILE("DrawSceneWindow");
    unsigned int textureID = sceneRenderer.GetTexture();
    Camera& camera = sceneRenderer.GetCamera();

    if (!ImGui::Begin(PanelId("Scene"), open, PanelFlags())) {
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

        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("RUNBUN_SHAPE")) {
                int shapeCode = *(const int*)payload->Data;
                glm::mat4 viewProj = camera.GetProjectionMatrix((int)panelSize.x, (int)panelSize.y) * camera.GetViewMatrix();
                glm::mat4 inverse = glm::inverse(viewProj);
                ImVec2 mouse = ImGui::GetIO().MousePos;
                float ndcX = (mouse.x - imageMin.x) / panelSize.x * 2.0f - 1.0f;
                float ndcY = 1.0f - (mouse.y - imageMin.y) / panelSize.y * 2.0f;
                glm::vec4 nearPoint = inverse * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
                glm::vec4 farPoint = inverse * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
                glm::vec3 origin = glm::vec3(nearPoint) / nearPoint.w;
                glm::vec3 dir = glm::normalize(glm::vec3(farPoint) / farPoint.w - origin);
                float hit = 0.0f;
                glm::vec3 point;
                if (sceneRenderer.PickObject(origin, dir, &hit) >= 0) point = origin + dir * hit;
                else if (dir.z < -1e-4f) point = origin + dir * (-origin.z / dir.z);
                else point = origin + dir * 5.0f;
                AddShapeAt(sceneRenderer, shapeCode, &point);
            }
            ImGui::EndDragDropTarget();
        }
        HandleSceneNavigation(sceneRenderer, imageMin, panelSize, viewHovered);
        UpdateQuickMenu(sceneRenderer, imageMin, panelSize, viewHovered);

        float rounding = ImGui::GetStyle().WindowRounding * 0.75f;
        ImGui::GetWindowDrawList()->AddImageRounded((ImTextureID)(intptr_t)textureID, imageMin, imageMax, ImVec2(0, 1), ImVec2(1, 0), IM_COL32_WHITE, rounding);
        bool handleTaken = DrawTinkerHandles(sceneRenderer, settings, imageMin, panelSize, viewHovered, viewActivated);
        DrawSceneGizmo(sceneRenderer, settings, imageMin, panelSize, viewHovered, viewActivated && !handleTaken, viewActive && !handleTaken);
        if (settings.showStatistics) DrawSceneStatistics(sceneRenderer, ImGui::GetWindowDrawList(), imageMin, imageMax);
        {
            glm::mat4 viewProj = camera.GetProjectionMatrix((int)panelSize.x, (int)panelSize.y) * camera.GetViewMatrix();
            DrawRoomPresence(sceneRenderer, ImGui::GetWindowDrawList(), viewProj, imageMin, panelSize);
            if (RoomSyncActive() && !RoomSyncReady()) {
                char text[96];
                snprintf(text, sizeof(text), "Getting the room scene  %.0f%%", RoomSyncProgressFraction() * 100.0f);
                ImVec2 size = ImGui::CalcTextSize(text);
                ImVec2 at = ImVec2((imageMin.x + imageMax.x - size.x) * 0.5f, (imageMin.y + imageMax.y) * 0.5f);
                ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(at.x - 14, at.y - 10), ImVec2(at.x + size.x + 14, at.y + size.y + 10), IM_COL32(15, 15, 20, 230), 8.0f);
                ImGui::GetWindowDrawList()->AddText(at, IM_COL32(245, 245, 245, 255), text);
            }
        }

        // Reset camera button sits on top of the scene image, top right corner
        const char* resetLabel = "Home";
        ImVec2 padding = ImGui::GetStyle().FramePadding;
        float buttonWidth = ImGui::CalcTextSize(resetLabel).x + padding.x * 2.0f;

        ImGui::SetCursorScreenPos(ImVec2(imageMax.x - buttonWidth - 10.0f, imageMin.y + 10.0f));
        if (ImGui::Button(resetLabel)) {
            camera.Reset();
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Back to the starting view");

        if (!IsEditModeActive()) {
            char gridLabel[48];
            snprintf(gridLabel, sizeof(gridLabel), settings.snapEnabled ? "Snap %g mm##SnapGrid" : "Snap off##SnapGrid", settings.snapUnitMm);
            ImGui::SetCursorScreenPos(ImVec2(imageMin.x + 10.0f, imageMin.y + 10.0f));
            if (ImGui::Button(gridLabel)) ImGui::OpenPopup("SnapGridMenu");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("How far shapes jump when you move or size them");
            if (ImGui::BeginPopup("SnapGridMenu")) {
                const float steps[] = { 0.1f, 0.5f, 1.0f, 2.0f, 5.0f, 10.0f, 50.0f, 100.0f, 250.0f, 500.0f };
                if (ImGui::MenuItem("Off", nullptr, !settings.snapEnabled)) { settings.snapEnabled = false; ImGui::MarkIniSettingsDirty(); }
                for (float step : steps) {
                    char item[32];
                    snprintf(item, sizeof(item), "%g mm", step);
                    if (ImGui::MenuItem(item, nullptr, settings.snapEnabled && fabsf(settings.snapUnitMm - step) < 1e-4f)) {
                        settings.snapEnabled = true;
                        settings.snapUnitMm = step;
                        ImGui::MarkIniSettingsDirty();
                    }
                }
                ImGui::EndPopup();
            }
        }

        float cubeSize = 46.0f;
        ImVec2 cubeCenter = ImVec2(imageMax.x - 16.0f - cubeSize * 1.1f, imageMin.y + 56.0f + cubeSize * 1.1f);
        ImGui::SetCursorScreenPos(ImVec2(cubeCenter.x - cubeSize * 1.1f, cubeCenter.y - cubeSize * 1.1f));
        bool cubeClicked = ImGui::InvisibleButton("ViewCube", ImVec2(cubeSize * 2.2f, cubeSize * 2.2f));
        DrawViewCube(sceneRenderer, ImGui::GetWindowDrawList(), cubeCenter, cubeSize, cubeClicked);
        const char* fitLabel = "Fit";
        float fitWidth = ImGui::CalcTextSize(fitLabel).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetCursorScreenPos(ImVec2(cubeCenter.x - fitWidth * 0.5f, cubeCenter.y + cubeSize * 1.15f));
        if (ImGui::Button(fitLabel)) CommandFitAll(sceneRenderer);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Zoom to show everything");

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
            changed |= ImGui::Checkbox("Sizes on Hover", &settings.showDimensions);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Shows width, depth and height of the object under the mouse");
            changed |= ImGui::Checkbox("Statistics", &settings.showStatistics);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Shows how many points, edges, faces and triangles there are");
            changed |= ImGui::Checkbox("Snap to Points", &settings.snapToPoints);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("While moving, jump onto nearby points (or hold Shift)");
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
        {
            // Look of the whole scene
            static const char* styles[4] = { "Solid", "Material", "Wire", "X-ray" };
            static const char* tips[4] = { "Plain grey look for shaping  (Shift+Z)", "Real materials, lights and shadows  (Shift+Z)",
                                           "Only the edges  (Shift+Z)", "See through everything  (Shift+Z)" };
            float total = 0.0f;
            for (const char* style : styles) total += ImGui::CalcTextSize(style).x + padding.x * 2.0f;
            float x = viewMin.x - ImGui::GetStyle().ItemSpacing.x * 2.0f - total;
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
            for (int k = 0; k < 4; k++) {
                ImGui::SetCursorScreenPos(ImVec2(x, viewMin.y));
                bool on = settings.viewStyle == k;
                if (on) ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab));
                if (ImGui::Button(styles[k])) {
                    settings.viewStyle = k;
                    ImGui::MarkIniSettingsDirty();
                }
                if (on) ImGui::PopStyleColor();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", tips[k]);
                x += ImGui::GetItemRectSize().x;
            }
            ImGui::PopStyleVar();
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
            ImGui::TextDisabled("Look from");
            float third = (fieldWidth * 2.0f - ImGui::GetStyle().ItemSpacing.x * 2.0f) / 3.0f;
            if (ImGui::Button("Top", ImVec2(third, 0.0f))) CommandView(sceneRenderer, View_Top);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Shift+Alt+Z   (again for bottom)");
            ImGui::SameLine();
            if (ImGui::Button("Front", ImVec2(third, 0.0f))) CommandView(sceneRenderer, View_Front);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Shift+Alt+Y   (again for back)");
            ImGui::SameLine();
            if (ImGui::Button("Side", ImVec2(third, 0.0f))) CommandView(sceneRenderer, View_Right);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Shift+Alt+X   (again for the other side)");
            if (ImGui::Button("Focus on Selected   (F)", ImVec2(fieldWidth * 2.0f, 0.0f))) CommandFocus(sceneRenderer);
            ImGui::Spacing();
            ImGui::TextDisabled("Background");
            changed |= ImGui::RadioButton("Color", &settings.backgroundMode, 0);
            ImGui::SameLine();
            changed |= ImGui::RadioButton("Fade", &settings.backgroundMode, 1);
            ImGui::SameLine();
            changed |= ImGui::RadioButton("Image", &settings.backgroundMode, 2);
            changed |= ImGui::ColorEdit3("##SceneBackground", &settings.sceneBackground.x, ImGuiColorEditFlags_NoInputs);
            ImGui::SameLine();
            ImGui::TextUnformatted(settings.backgroundMode == 1 ? "Top" : "Scene color");
            if (settings.backgroundMode == 1) {
                ImGui::SameLine();
                changed |= ImGui::ColorEdit3("##SceneBackgroundBottom", &settings.sceneBackgroundBottom.x, ImGuiColorEditFlags_NoInputs);
                ImGui::SameLine();
                ImGui::TextUnformatted("Bottom");
            }
            if (settings.backgroundMode == 2) {
                if (ImGui::Button("Pick Image...")) {
                    std::string path = OpenImageFileDialog();
                    if (!path.empty()) {
                        snprintf(settings.backgroundImage, sizeof(settings.backgroundImage), "%s", path.c_str());
                        changed = true;
                    }
                }
                ImGui::SameLine();
                const char* name = strrchr(settings.backgroundImage, '\\');
                if (!name) name = strrchr(settings.backgroundImage, '/');
                ImGui::TextDisabled("%s", settings.backgroundImage[0] ? (name ? name + 1 : settings.backgroundImage) : "No image yet");
            }
            ImGui::Spacing();
            ImGui::TextDisabled("Material view");
            changed |= ImGui::Checkbox("Shadows", &settings.shadows);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("The first sun light casts shadows on shapes and the ground");
            ImGui::SameLine();
            changed |= ImGui::Checkbox("Show lights", &settings.showLights);
            ImGui::Spacing();
            if (ImGui::Button("Reset View Settings")) {
                UISettings defaults;
                settings.viewFov = defaults.viewFov;
                settings.nearClipMm = defaults.nearClipMm;
                settings.farClipMm = defaults.farClipMm;
                settings.sceneBackground = defaults.sceneBackground;
                settings.sceneBackgroundBottom = defaults.sceneBackgroundBottom;
                settings.backgroundMode = defaults.backgroundMode;
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
        DrawQuickMenu(sceneRenderer);
    }

    ImGui::End();

    return panelSize;
}

bool IsSceneHovered() {
    return sceneHovered;
}
