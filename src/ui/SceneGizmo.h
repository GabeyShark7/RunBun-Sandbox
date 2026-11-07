#pragma once
#include "imgui/imgui.h"

class SceneRenderer;
struct UISettings;

void DrawSceneGizmo(SceneRenderer& sceneRenderer, UISettings& settings, ImVec2 viewMin, ImVec2 viewSize, bool viewHovered, bool viewActivated, bool viewActive);
bool IsSceneTransformActive();
