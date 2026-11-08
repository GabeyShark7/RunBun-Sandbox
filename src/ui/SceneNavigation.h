#pragma once
#include "imgui/imgui.h"

class SceneRenderer;

void HandleSceneNavigation(SceneRenderer& sceneRenderer, ImVec2 viewMin, ImVec2 viewSize, bool viewHovered);
void DrawViewCube(SceneRenderer& sceneRenderer, ImDrawList* drawList, ImVec2 center, float size, bool clicked);
bool ViewCubeHovered(ImVec2 center, float size);
