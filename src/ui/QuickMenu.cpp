#include "ui/RenderExport.h"
#include "ui/QuickMenu.h"
#include "ui/SceneCommands.h"
#include "ui/SceneGizmo.h"
#include "ui/EditMode.h"
#include "ui/Workspaces.h"
#include "ui/CreateMenu.h"
#include "core/UISettings.h"
#include "render/SceneRenderer.h"
#include <cmath>

struct RightClick {
    bool down = false;
    bool spoiled = false;
    ImVec2 at;
    double time = 0.0;
    glm::vec3 cameraPosition;
    float yaw = 0.0f, pitch = 0.0f;
};
static RightClick click;
static bool openMenu = false;

static void PickUnderMouse(SceneRenderer& sceneRenderer, ImVec2 mouse, ImVec2 viewMin, ImVec2 viewSize) {
    Camera& camera = sceneRenderer.GetCamera();
    glm::mat4 viewProj = camera.GetProjectionMatrix((int)viewSize.x, (int)viewSize.y) * camera.GetViewMatrix();
    glm::mat4 inverse = glm::inverse(viewProj);
    float ndcX = (mouse.x - viewMin.x) / viewSize.x * 2.0f - 1.0f;
    float ndcY = 1.0f - (mouse.y - viewMin.y) / viewSize.y * 2.0f;
    glm::vec4 a = inverse * glm::vec4(ndcX, ndcY, -1.0f, 1.0f);
    glm::vec4 b = inverse * glm::vec4(ndcX, ndcY, 1.0f, 1.0f);
    glm::vec3 origin = glm::vec3(a) / a.w;
    glm::vec3 dir = glm::normalize(glm::vec3(b) / b.w - origin);
    int picked = sceneRenderer.PickObject(origin, dir);
    if (picked >= 0 && !sceneRenderer.IsSelected(picked)) sceneRenderer.Select(picked, false);
    if (picked < 0) sceneRenderer.Select(-1, false);
}

void UpdateQuickMenu(SceneRenderer& sceneRenderer, ImVec2 viewMin, ImVec2 viewSize, bool viewHovered) {
    ImGuiIO& io = ImGui::GetIO();
    Camera& camera = sceneRenderer.GetCamera();
    if (viewHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        click.down = true;
        click.spoiled = io.KeyCtrl || IsSceneTransformActive();
        click.at = io.MousePos;
        click.time = ImGui::GetTime();
        click.cameraPosition = camera.position;
        click.yaw = camera.yaw;
        click.pitch = camera.pitch;
    }
    if (click.down && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
        click.down = false;
        bool still = fabsf(camera.yaw - click.yaw) < 0.05f && fabsf(camera.pitch - click.pitch) < 0.05f && glm::length(camera.position - click.cameraPosition) < 1e-4f;
        bool quick = ImGui::GetTime() - click.time < 0.45;
        float moved = sqrtf((io.MousePos.x - click.at.x) * (io.MousePos.x - click.at.x) + (io.MousePos.y - click.at.y) * (io.MousePos.y - click.at.y));
        if (!click.spoiled && still && quick && moved < 6.0f) {
            if (!IsEditModeActive()) PickUnderMouse(sceneRenderer, click.at, viewMin, viewSize);
            openMenu = true;
        }
    }
}

static void ObjectMenu(SceneRenderer& sceneRenderer) {
    UISettings& settings = GetUISettings();
    auto& objects = sceneRenderer.GetObjects();
    int selected = sceneRenderer.selectedIndex;
    bool mix = settings.workMode != WorkMode_Tinker;
    size_t picked = sceneRenderer.GetSelection().size();

    if (selected < 0) {
        ImGui::TextDisabled("Scene");
        ImGui::Separator();
        if (ImGui::BeginMenu("Add Shape")) {
            DrawCreateMenuItems(sceneRenderer);
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Fit Everything")) CommandFitAll(sceneRenderer);
        if (ImGui::MenuItem("Home View")) sceneRenderer.GetCamera().Reset();
        if (ImGui::MenuItem("Select All", "A", false, !objects.empty())) CommandSelectAll(sceneRenderer);
        return;
    }

    SceneObject& obj = objects[selected];
    if (picked > 1) ImGui::TextDisabled("%s and %d more", obj.name.c_str(), (int)picked - 1);
    else ImGui::TextDisabled("%s", obj.name.c_str());
    ImGui::Separator();
    if (ImGui::MenuItem("Duplicate", "Ctrl+D")) CommandDuplicate(sceneRenderer, false);
    if (ImGui::MenuItem("Delete", "Del")) CommandDelete(sceneRenderer);
    if (ImGui::MenuItem("Rename", "F2")) CommandRename(sceneRenderer);
    if (ImGui::MenuItem(obj.hole ? "Make Solid" : "Make Hole", "H")) CommandToggleHole(sceneRenderer);
    ImGui::Separator();
    if (ImGui::MenuItem("Group", "Ctrl+G", false, picked > 1)) CommandGroup(sceneRenderer);
    if (ImGui::MenuItem("Ungroup", "Ctrl+Shift+G", false, (bool)obj.parts)) CommandUngroup(sceneRenderer);
    if (ImGui::MenuItem("Join Into One", "J", false, picked > 1)) CommandJoin(sceneRenderer);
    ImGui::Separator();
    if (ImGui::MenuItem("Drop to Ground", "D")) CommandDrop(sceneRenderer);
    if (ImGui::MenuItem("Align...", "L", false, picked > 1)) CommandAlign();
    if (ImGui::BeginMenu("Mirror")) {
        const char* axes[3] = { "Left to right (X)", "Front to back (Y)", "Upside down (Z)" };
        for (int axis = 0; axis < 3; axis++)
            if (ImGui::MenuItem(axes[axis])) sceneRenderer.MirrorSelected(axis);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Color")) {
        if (ImGui::ColorPicker3("##QuickColor", &obj.color.x, ImGuiColorEditFlags_PickerHueWheel | ImGuiColorEditFlags_NoSidePreview)) {
            for (int index : sceneRenderer.GetSelection()) objects[index].color = obj.color;
        }
        ImGui::EndMenu();
    }
    if (ImGui::MenuItem("Focus", "F")) CommandFocus(sceneRenderer);
    if (mix) {
        ImGui::Separator();
        if (ImGui::BeginMenu("Add Mod")) {
            for (int t = 0; t < Mod_Count; t++) {
                if (ImGui::MenuItem(GetModifierName(t))) {
                    Modifier m = MakeModifier(t);
                    if (t == Mod_Boolean)
                        for (int index : sceneRenderer.GetSelection())
                            if (index != selected) { m.target = objects[index].id; break; }
                    obj.modifiers.push_back(m);
                    settings.showMods = true;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", GetModifierHelp(t));
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem("Apply All Mods", nullptr, false, !obj.modifiers.empty())) sceneRenderer.ApplyModifiers(selected, obj.modifiers.size());
        if (ImGui::MenuItem("Edit Shape Here", "Tab")) EnterEditMode(sceneRenderer, selected);
        if (ImGui::MenuItem("Open Edit Tab", "E")) ToggleEditWorkspace(sceneRenderer);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Export...", "Ctrl+E")) OpenExportModels();
}

static void EditMenu(SceneRenderer& sceneRenderer) {
    ImVec2 mouse = ImGui::GetIO().MousePos;
    bool any = GetEditSelectionCount() > 0;
    EditSelectMode mode = GetEditSelectMode();
    ImGui::TextDisabled("Edit Mode");
    ImGui::Separator();
    if (ImGui::BeginMenu("Pick")) {
        if (ImGui::MenuItem("Points", "1", mode == EditSelect_Vertex)) SetEditSelectMode(EditSelect_Vertex);
        if (ImGui::MenuItem("Edges", "2", mode == EditSelect_Edge)) SetEditSelectMode(EditSelect_Edge);
        if (ImGui::MenuItem("Faces", "3", mode == EditSelect_Face)) SetEditSelectMode(EditSelect_Face);
        ImGui::Separator();
        if (ImGui::MenuItem("All or None", "A")) EditToggleSelectAll();
        if (ImGui::MenuItem("Whole Pieces")) EditSelectIslands();
        ImGui::EndMenu();
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Extrude", "E", false, any)) RequestSceneExtrude();
    if (ImGui::MenuItem("Inset", "I", false, any)) EditBeginTool(sceneRenderer, EditTool_Inset, mouse);
    if (ImGui::MenuItem("Bevel", "Ctrl+B", false, any)) EditBeginTool(sceneRenderer, EditTool_Bevel, mouse);
    if (ImGui::MenuItem("Loop Cut", "Ctrl+R")) EditBeginTool(sceneRenderer, EditTool_LoopCut, mouse);
    if (ImGui::MenuItem("Subdivide", nullptr, false, any)) EditSubdivide(sceneRenderer);
    ImGui::Separator();
    if (ImGui::MenuItem(mode == EditSelect_Face ? "Join Faces" : "Fill Face", "F", false, any)) EditJoinOrFill(sceneRenderer);
    if (ImGui::MenuItem("Merge Points", "M", false, any)) EditMerge(sceneRenderer);
    if (ImGui::MenuItem("Dissolve", "X", false, any && mode != EditSelect_Face)) EditDissolve(sceneRenderer);
    if (ImGui::MenuItem("Delete", "Del", false, any)) EditDelete(sceneRenderer);
    if (ImGui::MenuItem("Weld Doubles")) EditWeldDoubles(sceneRenderer);
    ImGui::Separator();
    if (ImGui::MenuItem("Move", "G", false, any)) RequestSceneTransform('G');
    if (ImGui::MenuItem("Scale", "S", false, any)) RequestSceneTransform('S');
    if (ImGui::MenuItem("Rotate", "R", false, any)) RequestSceneTransform('R');
    if (GetUISettings().workMode != WorkMode_Tinker) {
        ImGui::Separator();
        if (ImGui::MenuItem("Mark Seam", nullptr, false, any)) EditMarkSeams(sceneRenderer, true);
        if (ImGui::MenuItem("Clear Seam", nullptr, false, any)) EditMarkSeams(sceneRenderer, false);
        if (ImGui::MenuItem("Unwrap")) EditUnwrapOrProject(sceneRenderer, -1);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Finish Editing", "Tab")) ExitEditMode();
}

void DrawQuickMenu(SceneRenderer& sceneRenderer) {
    if (openMenu) {
        ImGui::OpenPopup("QuickMenu");
        openMenu = false;
    }
    if (!ImGui::BeginPopup("QuickMenu")) return;
    if (IsEditModeActive()) EditMenu(sceneRenderer);
    else ObjectMenu(sceneRenderer);
    ImGui::EndPopup();
}
