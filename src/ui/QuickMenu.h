#pragma once
#include "imgui/imgui.h"

class SceneRenderer;

void UpdateQuickMenu(SceneRenderer& sceneRenderer, ImVec2 viewMin, ImVec2 viewSize, bool viewHovered);
void DrawQuickMenu(SceneRenderer& sceneRenderer);
