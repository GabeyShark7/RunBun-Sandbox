#pragma once
class SceneRenderer;

bool WorkspaceAllowed(int workspace);
void SetWorkspace(SceneRenderer& sceneRenderer, int workspace);
void ToggleEditWorkspace(SceneRenderer& sceneRenderer);
void DrawWorkspaceTabs(SceneRenderer& sceneRenderer);
void UpdateWorkspaceKeys(SceneRenderer& sceneRenderer);
