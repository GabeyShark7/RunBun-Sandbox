#pragma once
#include "imgui/imgui.h"
#include <glm/glm.hpp>
#include <functional>

class SceneRenderer;

enum EditSelectMode {
    EditSelect_Vertex = 0,
    EditSelect_Edge = 1,
    EditSelect_Face = 2
};

bool IsEditModeActive();
int GetEditObjectIndex();
EditSelectMode GetEditSelectMode();
const char* GetEditSelectModeName(EditSelectMode mode);
void SetEditSelectMode(EditSelectMode mode);

bool EnterEditMode(SceneRenderer& sceneRenderer, int objectIndex);
void ExitEditMode();
void ValidateEditMode(SceneRenderer& sceneRenderer, bool allowed);

int GetEditSelectionCount();
void EditSelectAll(bool select);
void EditToggleSelectAll();
void EditPick(SceneRenderer& sceneRenderer, const glm::mat4& viewProj, ImVec2 viewMin, ImVec2 viewSize, ImVec2 mouse,
              const glm::vec3& rayOrigin, const glm::vec3& rayDir, bool additive);
void DrawEditOverlay(SceneRenderer& sceneRenderer, ImDrawList* drawList, const glm::mat4& viewProj, ImVec2 viewMin, ImVec2 viewSize);

bool EditGetSelectionCenter(SceneRenderer& sceneRenderer, glm::vec3& outWorld);
bool EditBeginTransform(SceneRenderer& sceneRenderer, glm::vec3& outCenterWorld);
void EditApplyTransform(SceneRenderer& sceneRenderer, const std::function<glm::vec3(const glm::vec3&)>& worldTransform);
void EditCancelTransform(SceneRenderer& sceneRenderer);
void EditMoveSelection(SceneRenderer& sceneRenderer, const glm::vec3& worldDelta);
