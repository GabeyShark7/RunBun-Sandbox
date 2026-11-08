#pragma once
#include <glm/glm.hpp>

class SceneRenderer;

void DrawShapesWindow(SceneRenderer& sceneRenderer, bool* open);
void AddShapeAt(SceneRenderer& sceneRenderer, int payload, const glm::vec3* point);
