#pragma once
#include "imgui/imgui.h"

class SceneRenderer;

void DrawSceneStatistics(SceneRenderer& sceneRenderer, ImDrawList* drawList, ImVec2 viewMin, ImVec2 viewMax);
