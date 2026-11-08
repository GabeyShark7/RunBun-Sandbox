#include "core/Profiler.h"
#include "ui/ModsWindow.h"
#include "ui/DockLayout.h"
#include "ui/PanelHeader.h"
#include "ui/EditMode.h"
#include "core/UISettings.h"
#include "render/SceneRenderer.h"
#include "imgui/imgui.h"
#include <cstdio>

static PanelCollapseState collapseState;

static void Hint(const char* text) {
    if (!ShowHelpText()) return;
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

static bool AxisPicker(const char* id, int& axis) {
    bool changed = false;
    const char* names[3] = { "X", "Y", "Z" };
    ImGui::PushID(id);
    for (int k = 0; k < 3; k++) {
        if (k) ImGui::SameLine();
        changed |= ImGui::RadioButton(names[k], &axis, k);
    }
    ImGui::PopID();
    return changed;
}

static bool TargetsMe(const SceneObject& other, unsigned int me) {
    for (const Modifier& m : other.modifiers)
        if (m.type == Mod_Boolean && m.target == me) return true;
    return false;
}

void DrawModsWindow(SceneRenderer& sceneRenderer, bool* open) {
    PROFILE("DrawModsWindow");
    if (!ImGui::Begin(PanelId("Object Mods"), open, PanelFlags())) {
        ImGui::End();
        return;
    }
    if (!DrawPanelHeader(collapseState)) {
        ImGui::End();
        return;
    }

    auto& objects = sceneRenderer.GetObjects();
    int selected = sceneRenderer.selectedIndex;
    if (selected < 0 || selected >= (int)objects.size()) {
        Hint("Select an object to add mods to it.");
        ImGui::TextDisabled("Nothing selected");
        ImGui::End();
        return;
    }
    SceneObject& obj = objects[selected];
    float full = ImGui::GetContentRegionAvail().x;
    ImGui::Text("%s", obj.name.c_str());
    if (obj.look.light >= 0) {
        ImGui::TextDisabled("Lights have no shape to change.");
        ImGui::End();
        return;
    }
    Hint("Mods change the shape without touching the original, so you can tweak or remove them any time. Apply makes them permanent.");
    if (IsEditModeActive() && GetEditObjectIndex() == selected) Hint("Mods are hidden while you edit the shape underneath.");

    ImGui::SetNextItemWidth(full);
    if (ImGui::BeginCombo("##AddMod", "Add a mod...")) {
        for (int t = 0; t < Mod_Count; t++) {
            if (ImGui::Selectable(GetModifierName(t))) {
                Modifier m = MakeModifier(t);
                if (t == Mod_Boolean) {
                    for (int index : sceneRenderer.GetSelection())
                        if (index != selected) { m.target = objects[index].id; break; }
                }
                obj.modifiers.push_back(m);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", GetModifierHelp(t));
        }
        ImGui::EndCombo();
    }
    if (obj.modifiers.empty()) Hint("No mods yet.");

    int removeIndex = -1, moveUp = -1, moveDown = -1, applyIndex = -1;
    for (int k = 0; k < (int)obj.modifiers.size(); k++) {
        Modifier& m = obj.modifiers[k];
        ImGui::PushID(k);
        ImGui::Separator();
        ImGui::Checkbox("##on", &m.enabled);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Turn this mod on or off");
        ImGui::SameLine();
        bool expanded = ImGui::TreeNodeEx("##mod", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_SpanAvailWidth, "%s", GetModifierName(m.type));
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", GetModifierHelp(m.type));
        if (expanded) {
            ImGui::Indent();
            ImGui::BeginDisabled(k == 0);
            if (ImGui::ArrowButton("##up", ImGuiDir_Up)) moveUp = k;
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Run this mod earlier");
            ImGui::SameLine();
            ImGui::BeginDisabled(k + 1 == (int)obj.modifiers.size());
            if (ImGui::ArrowButton("##down", ImGuiDir_Down)) moveDown = k;
            ImGui::EndDisabled();
            if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Run this mod later");
            ImGui::SameLine();
            if (ImGui::SmallButton("Apply")) applyIndex = k;
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Make this mod (and the ones above it) part of the shape");
            ImGui::SameLine();
            if (ImGui::SmallButton("Remove")) removeIndex = k;
            ImGui::Unindent();
        }

        if (expanded) {
            float field = ImGui::GetContentRegionAvail().x * 0.6f;
            switch (m.type) {
            case Mod_Boolean: {
                const char* ops[3] = { "Join with", "Cut away", "Keep overlap with" };
                ImGui::SetNextItemWidth(field);
                ImGui::Combo("How", &m.operation, ops, 3);
                const char* current = "Pick an object";
                for (const SceneObject& other : objects) if (other.id == m.target) current = other.name.c_str();
                ImGui::SetNextItemWidth(field);
                if (ImGui::BeginCombo("Partner", current)) {
                    for (const SceneObject& other : objects) {
                        if (other.id == obj.id || TargetsMe(other, obj.id)) continue;
                        if (ImGui::Selectable(other.name.c_str(), other.id == m.target)) m.target = other.id;
                    }
                    ImGui::EndCombo();
                }
                ImGui::Checkbox("Show partner see-through", &m.hideTarget);
                if (m.target == 0) Hint("Pick the object to cut with. Tip: select this object, then Shift+click the partner before adding the mod.");
                break;
            }
            case Mod_Mirror:
                AxisPicker("axis", m.axis);
                Hint("Mirrors across the object middle. Works best on half a shape.");
                break;
            case Mod_Array:
                AxisPicker("axis", m.axis);
                ImGui::SetNextItemWidth(field);
                ImGui::SliderInt("Copies", &m.count, 2, 50);
                ImGui::SetNextItemWidth(field);
                ImGui::DragFloat("Spacing", &m.amount, 0.01f, 0.0f, 10.0f, "%.2fx size");
                break;
            case Mod_Bevel: {
                float percent = m.amount * 100.0f;
                ImGui::SetNextItemWidth(field);
                if (ImGui::SliderFloat("Width", &percent, 1.0f, 95.0f, "%.0f%%")) m.amount = percent / 100.0f;
                ImGui::SetNextItemWidth(field);
                ImGui::SliderFloat("Sharper than", &m.angle, 1.0f, 89.0f, "%.0f deg");
                break;
            }
            case Mod_Subdivide:
                ImGui::SetNextItemWidth(field);
                ImGui::SliderInt("Levels", &m.levels, 1, 3);
                if (m.levels >= 3) Hint("Level 3 can get slow on detailed shapes.");
                break;
            case Mod_Solidify: {
                float mm = m.amount * 1000.0f;
                ImGui::SetNextItemWidth(field);
                if (ImGui::DragFloat("Wall", &mm, 1.0f, -2000.0f, 2000.0f, "%.1f mm")) m.amount = mm / 1000.0f;
                Hint("On a closed shape this hollows it out, handy for 3D printing.");
                break;
            }
            default:
                break;
            }
            ImGui::TreePop();
        }
        ImGui::PopID();
    }

    if (moveUp > 0) std::swap(obj.modifiers[moveUp], obj.modifiers[moveUp - 1]);
    if (moveDown >= 0 && moveDown + 1 < (int)obj.modifiers.size()) std::swap(obj.modifiers[moveDown], obj.modifiers[moveDown + 1]);
    if (removeIndex >= 0) obj.modifiers.erase(obj.modifiers.begin() + removeIndex);
    if (applyIndex >= 0) {
        if (IsEditModeActive() && GetEditObjectIndex() == selected) ExitEditMode();
        sceneRenderer.ApplyModifiers(selected, (size_t)applyIndex + 1);
    }

    if (!obj.modifiers.empty()) {
        ImGui::Separator();
        float half = (full - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
        if (ImGui::Button("Apply All", ImVec2(half, 0.0f))) {
            if (IsEditModeActive() && GetEditObjectIndex() == selected) ExitEditMode();
            sceneRenderer.ApplyModifiers(selected, obj.modifiers.size());
        }
        ImGui::SameLine();
        if (ImGui::Button("Remove All", ImVec2(half, 0.0f))) obj.modifiers.clear();
    }
    ImGui::End();
}
