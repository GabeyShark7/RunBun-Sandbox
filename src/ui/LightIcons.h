#pragma once
#include "imgui/imgui.h"
#include <glm/glm.hpp>

class SceneRenderer;

void DrawLightIcons(SceneRenderer& scene, ImDrawList* drawList, const glm::mat4& viewProj, ImVec2 viewMin, ImVec2 viewSize);
