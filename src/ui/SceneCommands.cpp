#include <filesystem>
#include "ui/RenderExport.h"
#include "imgui/imgui_internal.h"
#include "core/Profiler.h"
#include "ui/SceneCommands.h"
#include "ui/SceneWindow.h"
#include "ui/SceneGizmo.h"
#include "ui/EditMode.h"
#include "ui/FileDialog.h"
#include "ui/Console.h"
#include "core/SceneState.h"
#include "core/UISettings.h"
#include "render/SceneRenderer.h"
#include "imgui/imgui.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <fstream>
#include <iterator>
#include <string>

enum PendingAction {
    Pending_None,
    Pending_New,
    Pending_Open,
    Pending_Quit
};

static GLFWwindow* appWindow = nullptr;
static SceneRenderer* appScene = nullptr;
static std::string currentPath;
static std::string shownTitle;
static PendingAction pending = Pending_None;
static bool askToSave = false;
static bool quitConfirmed = false;

static std::string FileName(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

static bool Idle() {
    return !ImGui::IsAnyItemActive() && !ImGui::IsMouseDown(ImGuiMouseButton_Left) && !IsSceneTransformActive();
}

static void OnCloseRequested(GLFWwindow* window) {
    if (quitConfirmed || !appScene) return;
    HistoryTrack(*appScene);
    if (!HistoryIsDirty()) return;
    // Ask first
    glfwSetWindowShouldClose(window, GLFW_FALSE);
    pending = Pending_Quit;
    askToSave = true;
}

void InitSceneCommands(GLFWwindow* window, SceneRenderer& scene) {
    appWindow = window;
    appScene = &scene;
    glfwSetWindowCloseCallback(window, OnCloseRequested);
    HistoryReset(scene);
}

static void RefreshTitle() {
    std::string title = "RunBun Sandbox";
    bool dirty = HistoryIsDirty();
    if (!currentPath.empty() || dirty) title += " - " + (currentPath.empty() ? std::string("Untitled") : FileName(currentPath));
    if (dirty) title += "*";
    if (title != shownTitle && appWindow) {
        glfwSetWindowTitle(appWindow, title.c_str());
        shownTitle = title;
    }
}

static bool WriteScene(SceneRenderer& scene, const std::string& path) {
    HistoryTrack(scene);
    std::vector<uint8_t> bytes = SerializeScene(CaptureScene(scene, nullptr));
    std::ofstream file(std::filesystem::u8path(path), std::ios::binary);
    if (!file) {
        LogError("Could not save to '" + path + "'.");
        return false;
    }
    file.write((const char*)bytes.data(), (std::streamsize)bytes.size());
    if (!file) {
        LogError("Saving '" + FileName(path) + "' failed.");
        return false;
    }
    currentPath = path;
    HistoryMarkSaved();
    LogSuccess("Saved '" + FileName(path) + "'.");
    return true;
}

static void DoNew(SceneRenderer& scene) {
    ExitEditMode();
    scene.ClearScene();
    currentPath.clear();
    HistoryReset(scene);
}

static void DoOpen(SceneRenderer& scene) {
    std::string path = OpenSceneFileDialog();
    if (path.empty()) return;
    std::ifstream file(std::filesystem::u8path(path), std::ios::binary);
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    SceneState state;
    if (!file.good() && !file.eof()) {
        LogError("Could not open '" + FileName(path) + "'.");
        return;
    }
    if (!DeserializeScene(bytes.data(), bytes.size(), state)) {
        LogError("'" + FileName(path) + "' is not a RunBun scene or is damaged.");
        return;
    }
    ExitEditMode();
    scene.ClearScene();
    RestoreScene(scene, state);
    scene.SetSelection({}, -1);
    currentPath = path;
    HistoryReset(scene);
    LogSuccess("Opened '" + FileName(path) + "'.");
}

static void RunPending(SceneRenderer& scene) {
    PendingAction action = pending;
    pending = Pending_None;
    if (action == Pending_New) DoNew(scene);
    else if (action == Pending_Open) DoOpen(scene);
    else if (action == Pending_Quit) {
        quitConfirmed = true;
        glfwSetWindowShouldClose(appWindow, GLFW_TRUE);
    }
}

static void Request(PendingAction action) {
    if (!appScene) return;
    HistoryTrack(*appScene);
    pending = action;
    if (HistoryIsDirty()) askToSave = true;
    else RunPending(*appScene);
}

void CommandNew() { Request(Pending_New); }
void CommandOpen() { Request(Pending_Open); }

bool CommandSave(SceneRenderer& scene) {
    if (currentPath.empty()) return CommandSaveAs(scene);
    return WriteScene(scene, currentPath);
}

bool CommandSaveAs(SceneRenderer& scene) {
    std::string path = SaveSceneFileDialog(currentPath.empty() ? "Untitled.rbscene" : currentPath);
    if (path.empty()) return false;
    return WriteScene(scene, path);
}

// Rebuild edit data
static void AfterRestore(SceneRenderer& scene, int editIndex, unsigned int editId) {
    if (editIndex < 0) return;
    EditSelectMode mode = GetEditSelectMode();
    ExitEditMode();
    auto& objects = scene.GetObjects();
    for (int i = 0; i < (int)objects.size(); i++) {
        if (objects[i].id != editId) continue;
        scene.SetSelection({ i }, i);
        if (EnterEditMode(scene, i)) SetEditSelectMode(mode);
        return;
    }
}

static void Restore(SceneRenderer& scene, bool redo) {
    if (IsSceneTransformActive() || ImGui::IsAnyItemActive()) return;
    int editIndex = GetEditObjectIndex();
    auto& objects = scene.GetObjects();
    unsigned int editId = editIndex >= 0 && editIndex < (int)objects.size() ? objects[editIndex].id : 0;
    bool changed = redo ? HistoryRedo(scene) : HistoryUndo(scene);
    if (changed) AfterRestore(scene, editIndex, editId);
}

void CommandUndo(SceneRenderer& scene) { Restore(scene, false); }
void CommandRedo(SceneRenderer& scene) { Restore(scene, true); }

bool CanUndo() { return HistoryCanUndo(); }
bool CanRedo() { return HistoryCanRedo(); }

bool HasObjectSelection(SceneRenderer& scene) {
    return !scene.GetSelection().empty() && !IsEditModeActive();
}

void CommandDelete(SceneRenderer& scene) {
    if (IsSceneTransformActive()) return;
    if (IsEditModeActive()) {
        EditDelete(scene);
        return;
    }
    if (!HasObjectSelection(scene)) return;
    scene.DeleteSelected();
}

struct RepeatStep {
    std::vector<unsigned int> sources, copies;
    std::vector<glm::vec3> sourcePositions, sourceRotations, sourceScales;
};
static RepeatStep lastDuplicate;

static std::vector<unsigned int> SelectedIds(SceneRenderer& scene) {
    std::vector<unsigned int> ids;
    for (int i : scene.GetSelection()) ids.push_back(scene.GetObjects()[i].id);
    return ids;
}

static int IndexOf(SceneRenderer& scene, unsigned int id) {
    auto& objects = scene.GetObjects();
    for (int i = 0; i < (int)objects.size(); i++) if (objects[i].id == id) return i;
    return -1;
}

void CommandDuplicate(SceneRenderer& scene, bool startMove) {
    if (!HasObjectSelection(scene) || IsSceneTransformActive()) return;
    auto& objects = scene.GetObjects();
    std::vector<unsigned int> before = SelectedIds(scene);
    // Repeat the last step
    bool repeat = before == lastDuplicate.copies && !before.empty();
    std::vector<glm::vec3> deltaPos, deltaRot, ratioScale;
    if (repeat) {
        for (size_t k = 0; k < before.size(); k++) {
            int now = IndexOf(scene, before[k]);
            if (now < 0) { repeat = false; break; }
            glm::vec3 s = lastDuplicate.sourceScales[k];
            deltaPos.push_back(objects[now].position - lastDuplicate.sourcePositions[k]);
            deltaRot.push_back(objects[now].rotation - lastDuplicate.sourceRotations[k]);
            ratioScale.push_back(glm::vec3(s.x != 0.0f ? objects[now].scale.x / s.x : 1.0f, s.y != 0.0f ? objects[now].scale.y / s.y : 1.0f, s.z != 0.0f ? objects[now].scale.z / s.z : 1.0f));
        }
        bool moved = false;
        for (size_t k = 0; k < deltaPos.size(); k++)
            if (glm::length(deltaPos[k]) > 1e-6f || glm::length(deltaRot[k]) > 1e-4f || glm::length(ratioScale[k] - glm::vec3(1.0f)) > 1e-5f) moved = true;
        repeat = repeat && moved;
    }
    RepeatStep step;
    step.sources = before;
    for (unsigned int id : before) {
        int i = IndexOf(scene, id);
        step.sourcePositions.push_back(objects[i].position);
        step.sourceRotations.push_back(objects[i].rotation);
        step.sourceScales.push_back(objects[i].scale);
    }
    scene.DuplicateSelected();
    step.copies = SelectedIds(scene);
    if (repeat && step.copies.size() == deltaPos.size()) {
        for (size_t k = 0; k < step.copies.size(); k++) {
            int i = IndexOf(scene, step.copies[k]);
            objects[i].position += deltaPos[k];
            objects[i].rotation += deltaRot[k];
            objects[i].scale *= ratioScale[k];
        }
        startMove = false;
    }
    lastDuplicate = step;
    if (startMove) RequestSceneTransform('G');
}

void CommandGroup(SceneRenderer& scene) {
    if (!HasObjectSelection(scene) || IsSceneTransformActive()) return;
    if (scene.GetSelection().size() < 2) {
        LogWarning("Pick two or more shapes to group.");
        return;
    }
    scene.GroupSelected();
}

void CommandUngroup(SceneRenderer& scene) {
    if (!HasObjectSelection(scene) || IsSceneTransformActive()) return;
    if (!scene.UngroupSelected()) LogWarning("Nothing picked is a group.");
}

void CommandToggleHole(SceneRenderer& scene) {
    if (!HasObjectSelection(scene) || IsSceneTransformActive()) return;
    auto& objects = scene.GetObjects();
    bool makeHole = !objects[scene.selectedIndex].hole;
    for (int i : scene.GetSelection()) if (objects[i].look.light < 0) objects[i].hole = makeHole;
}

void CommandDrop(SceneRenderer& scene) {
    if (!HasObjectSelection(scene) || IsSceneTransformActive()) return;
    scene.DropSelectedToGround();
}

static bool openAlign = false, openMirror = false;
void CommandAlign() { openAlign = true; }
void CommandMirror() { openMirror = true; }

void CommandFitAll(SceneRenderer& scene) {
    scene.FocusAll();
}

void CommandExportSTL(SceneRenderer& scene) {
    if (scene.GetObjects().empty()) {
        LogWarning("Nothing to export yet.");
        return;
    }
    std::string base = currentPath.empty() ? std::string("RunBun.stl") : currentPath.substr(0, currentPath.find_last_of('.')) + ".stl";
    std::string path = SaveSTLFileDialog(base);
    if (path.empty()) return;
    int triangles = scene.ExportSTL(path);
    if (triangles < 0) LogError("Could not write '" + FileName(path) + "'.");
    else LogSuccess("Exported " + std::to_string(triangles) + " triangles to '" + FileName(path) + "'.");
}

void CommandJoin(SceneRenderer& scene) {
    if (!HasObjectSelection(scene) || IsSceneTransformActive()) return;
    if (scene.GetSelection().size() < 2) {
        LogWarning("Pick two or more objects to join. Shift+click adds to the pick.");
        return;
    }
    scene.JoinSelected();
}

static int renameRequest = -1;

void CommandRename(SceneRenderer& scene) {
    if (!IsEditModeActive() && scene.selectedIndex >= 0) renameRequest = scene.selectedIndex;
}

int TakeRenameRequest() {
    int request = renameRequest;
    renameRequest = -1;
    return request;
}

void CommandSelectAll(SceneRenderer& scene) {
    bool all = scene.GetSelection().size() == scene.GetObjects().size();
    scene.SelectAll(!all);
}

void CommandFocus(SceneRenderer& scene) {
    scene.FocusSelection();
}

static void PresetAngles(int preset, float& yaw, float& pitch) {
    yaw = 90.0f;
    pitch = 0.0f;
    switch (preset) {
        case View_Front: yaw = 90.0f; break;
        case View_Back: yaw = -90.0f; break;
        case View_Right: yaw = 180.0f; break;
        case View_Left: yaw = 0.0f; break;
        case View_Top: pitch = -89.0f; break;
        case View_Bottom: pitch = 89.0f; break;
    }
}

static int OppositeView(int preset) {
    switch (preset) {
        case View_Front: return View_Back;
        case View_Back: return View_Front;
        case View_Right: return View_Left;
        case View_Left: return View_Right;
        case View_Top: return View_Bottom;
        default: return View_Top;
    }
}

void CommandView(SceneRenderer& scene, int preset, bool allowFlip) {
    Camera& camera = scene.GetCamera();
    float yaw, pitch;
    PresetAngles(preset, yaw, pitch);
    // Same key flips sides
    float yawGap = fmodf(fabsf(camera.yaw - yaw), 360.0f);
    if (yawGap > 180.0f) yawGap = 360.0f - yawGap;
    if (allowFlip && !camera.IsAnimating() && yawGap < 1.0f && fabsf(camera.pitch - pitch) < 1.0f) {
        preset = OppositeView(preset);
        PresetAngles(preset, yaw, pitch);
    }
    glm::vec3 pivot = scene.GetSelection().empty() ? camera.position + camera.front * 5.0f : scene.GetSelectionCenter();
    camera.ViewFrom(pivot, yaw, pitch);
}

static bool Pressed(ImGuiKey key) { return ImGui::IsKeyPressed(key, false); }

void UpdateSceneCommands(SceneRenderer& scene) {
    PROFILE("UpdateSceneCommands");
    scene.ValidateSelection();
    if (Idle()) HistoryTrack(scene);
    RefreshTitle();

    ImGuiIO& io = ImGui::GetIO();
    if (io.WantTextInput || askToSave) return;
    bool ctrl = io.KeyCtrl, shift = io.KeyShift;

    if (ctrl && Pressed(ImGuiKey_Z)) { if (shift) CommandRedo(scene); else CommandUndo(scene); }
    else if (ctrl && Pressed(ImGuiKey_Y)) CommandRedo(scene);
    else if (ctrl && Pressed(ImGuiKey_S)) { if (shift) CommandSaveAs(scene); else CommandSave(scene); }
    else if (ctrl && Pressed(ImGuiKey_O)) CommandOpen();
    else if (ctrl && Pressed(ImGuiKey_N)) CommandNew();
    else if (ctrl && Pressed(ImGuiKey_D)) CommandDuplicate(scene, IsSceneHovered());
    else if (!ctrl && (Pressed(ImGuiKey_Delete) || Pressed(ImGuiKey_Backspace))) CommandDelete(scene);
    else if (!ctrl && !IsSceneTransformActive() && !IsEditModeActive() && Pressed(ImGuiKey_F)) CommandFocus(scene);
    else if (!ctrl && !IsSceneTransformActive() && Pressed(ImGuiKey_KeypadDecimal)) CommandFocus(scene);
    else if (!ctrl && !IsEditModeActive() && Pressed(ImGuiKey_J)) CommandJoin(scene);
    else if (ctrl && !IsEditModeActive() && Pressed(ImGuiKey_G)) { if (shift) CommandUngroup(scene); else CommandGroup(scene); }
    else if (!ctrl && Pressed(ImGuiKey_F2)) CommandRename(scene);
    else if (!ctrl && Pressed(ImGuiKey_F12)) OpenRenderImage();
    else if (ctrl && !shift && Pressed(ImGuiKey_E)) OpenExportModels();
    else if (shift && !ctrl && !io.KeyAlt && IsSceneHovered() && Pressed(ImGuiKey_Z)) {
        UISettings& look = GetUISettings();
        look.viewStyle = (look.viewStyle + 1) % 4;
        ImGui::MarkIniSettingsDirty();
    }

    if (IsSceneTransformActive()) return;
    if (!IsEditModeActive() && HasObjectSelection(scene) && !io.KeyAlt) {
        UISettings& settings = GetUISettings();
        float step = settings.snapUnitMm / 1000.0f * (shift ? 10.0f : 1.0f);
        Camera& camera = scene.GetCamera();
        glm::vec3 forward = glm::vec3(camera.front.x, camera.front.y, 0.0f);
        if (std::fabs(forward.x) > std::fabs(forward.y)) forward = glm::vec3(forward.x > 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
        else forward = glm::vec3(0.0f, forward.y >= 0.0f ? 1.0f : -1.0f, 0.0f);
        glm::vec3 right = glm::vec3(forward.y, -forward.x, 0.0f);
        glm::vec3 delta(0.0f);
        if (ctrl) {
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) delta.z += step;
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) delta.z -= step;
        } else {
            if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) delta += forward * step;
            if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) delta -= forward * step;
            if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) delta += right * step;
            if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) delta -= right * step;
        }
        if (delta != glm::vec3(0.0f)) scene.NudgeSelected(delta);
        if (IsSceneHovered() && !ctrl && !shift && !ImGui::IsMouseDown(ImGuiMouseButton_Right)) {
            if (Pressed(ImGuiKey_D)) CommandDrop(scene);
            if (Pressed(ImGuiKey_H)) CommandToggleHole(scene);
            if (Pressed(ImGuiKey_L)) CommandAlign();
            if (Pressed(ImGuiKey_M)) CommandMirror();
        }
    }
    if (shift && io.KeyAlt && !ctrl) {
        if (Pressed(ImGuiKey_X)) CommandView(scene, View_Right);
        if (Pressed(ImGuiKey_Y)) CommandView(scene, View_Front);
        if (Pressed(ImGuiKey_Z)) CommandView(scene, View_Top);
    }
}

static void DrawToolPopups(SceneRenderer& scene) {
    if (openAlign) ImGui::OpenPopup("Align##Tool");
    if (openMirror) ImGui::OpenPopup("Mirror##Tool");
    openAlign = openMirror = false;
    if (ImGui::BeginPopup("Align##Tool")) {
        ImGui::TextDisabled(scene.GetSelection().size() < 2 ? "Pick two or more shapes to line up" : "Line up the picked shapes");
        const char* axes[3] = { "Across (X)", "Depth (Y)", "Height (Z)" };
        const char* where[3][3] = { { "Left", "Middle", "Right" }, { "Front", "Middle", "Back" }, { "Bottom", "Middle", "Top" } };
        ImGui::BeginDisabled(scene.GetSelection().size() < 2);
        for (int axis = 0; axis < 3; axis++) {
            ImGui::PushID(axis);
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(axes[axis]);
            for (int w = 0; w < 3; w++) {
                ImGui::SameLine(110.0f + w * 80.0f);
                ImGui::PushID(w);
                if (ImGui::Button(where[axis][w], ImVec2(74.0f, 0.0f))) scene.AlignSelected(axis, w);
                ImGui::PopID();
            }
            ImGui::PopID();
        }
        ImGui::EndDisabled();
        ImGui::EndPopup();
    }
    if (ImGui::BeginPopup("Mirror##Tool")) {
        ImGui::TextDisabled("Flip the picked shapes");
        const char* axes[3] = { "Left to right (X)", "Front to back (Y)", "Upside down (Z)" };
        for (int axis = 0; axis < 3; axis++)
            if (ImGui::Button(axes[axis], ImVec2(200.0f, 0.0f))) scene.MirrorSelected(axis);
        ImGui::EndPopup();
    }
}

void DrawSceneCommandPopups(SceneRenderer& scene) {
    DrawToolPopups(scene);
    if (askToSave) {
        ImGui::OpenPopup("Unsaved Changes");
        askToSave = false;
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (!ImGui::BeginPopupModal("Unsaved Changes", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) return;

    std::string name = currentPath.empty() ? std::string("Untitled") : FileName(currentPath);
    ImGui::Text("Save changes to %s?", name.c_str());
    ImGui::TextDisabled("Your changes will be lost if you don't save them.");
    ImGui::Spacing();
    float width = ImGui::CalcTextSize("Don't Save").x + ImGui::GetStyle().FramePadding.x * 4.0f;
    if (ImGui::Button("Save", ImVec2(width, 0.0f))) {
        ImGui::CloseCurrentPopup();
        if (CommandSave(scene)) RunPending(scene);
        else pending = Pending_None;
    }
    ImGui::SameLine();
    if (ImGui::Button("Don't Save", ImVec2(width, 0.0f))) {
        ImGui::CloseCurrentPopup();
        RunPending(scene);
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(width, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape, false)) {
        ImGui::CloseCurrentPopup();
        pending = Pending_None;
    }
    ImGui::EndPopup();
}
