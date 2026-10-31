#pragma once
#include "imgui/imgui.h"
#include "Camera.h"

ImVec2 DrawSceneWindow(unsigned int textureID, Camera& camera);
bool IsSceneHovered();