#pragma once
#include "imgui/imgui.h"
class SceneRenderer;
struct UISettings;

ImVec2 DrawSceneWindow(SceneRenderer& sceneRenderer, UISettings& settings, bool* open);
bool IsSceneHovered();