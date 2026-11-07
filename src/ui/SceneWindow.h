#pragma once
#include "imgui/imgui.h"
#include "core/Camera.h"

ImVec2 DrawSceneWindow(unsigned int textureID, Camera& camera, bool* open);
bool IsSceneHovered();