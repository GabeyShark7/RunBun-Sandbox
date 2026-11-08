#pragma once

struct GLFWwindow;
class SceneRenderer;

void InitSceneCommands(GLFWwindow* window, SceneRenderer& scene);
void UpdateSceneCommands(SceneRenderer& scene);
void DrawSceneCommandPopups(SceneRenderer& scene);

void CommandNew();
void CommandOpen();
bool CommandSave(SceneRenderer& scene);
bool CommandSaveAs(SceneRenderer& scene);
void CommandUndo(SceneRenderer& scene);
void CommandRedo(SceneRenderer& scene);
void CommandDelete(SceneRenderer& scene);
void CommandDuplicate(SceneRenderer& scene, bool startMove);
void CommandSelectAll(SceneRenderer& scene);
void CommandJoin(SceneRenderer& scene);
void CommandGroup(SceneRenderer& scene);
void CommandUngroup(SceneRenderer& scene);
void CommandToggleHole(SceneRenderer& scene);
void CommandDrop(SceneRenderer& scene);
void CommandAlign();
void CommandMirror();
void CommandExportSTL(SceneRenderer& scene);
void CommandFitAll(SceneRenderer& scene);
void CommandRename(SceneRenderer& scene);
int TakeRenameRequest();
void CommandFocus(SceneRenderer& scene);
void CommandView(SceneRenderer& scene, int preset, bool allowFlip = true);

bool CanUndo();
bool CanRedo();
bool HasObjectSelection(SceneRenderer& scene);

enum ViewPreset {
    View_Front,
    View_Back,
    View_Right,
    View_Left,
    View_Top,
    View_Bottom
};
