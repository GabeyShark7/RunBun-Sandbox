#pragma once

class SceneRenderer;

void OpenExportModels();
void OpenRenderImage();
#include "imgui/imgui.h"
void DrawRenderExportWindows(SceneRenderer& scene, ImVec2 sceneSize);
