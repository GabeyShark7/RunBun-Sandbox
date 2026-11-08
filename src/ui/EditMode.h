#pragma once
#include "imgui/imgui.h"
#include <glm/glm.hpp>
#include <functional>
#include <vector>
#include <set>
#include <string>
#include "render/Mesh.h"

struct PolyMesh;

class SceneRenderer;

enum EditSelectMode {
    EditSelect_Vertex = 0,
    EditSelect_Edge = 1,
    EditSelect_Face = 2
};

enum EditTool {
    EditTool_None = 0,
    EditTool_Inset,
    EditTool_Bevel,
    EditTool_LoopCut
};

enum EditBoxMode {
    EditBox_Replace = 0,
    EditBox_Add,
    EditBox_Remove
};

struct EditView {
    glm::mat4 viewProj;
    ImVec2 min;
    ImVec2 size;
    glm::vec3 eye;
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
void EditPick(SceneRenderer& sceneRenderer, const EditView& view, ImVec2 mouse, bool additive);
void EditBoxSelect(SceneRenderer& sceneRenderer, const EditView& view, ImVec2 a, ImVec2 b, EditBoxMode mode);
void DrawEditOverlay(SceneRenderer& sceneRenderer, ImDrawList* drawList, const EditView& view, const ImVec2* hoverMouse);

bool GetEditSelectThrough();
void SetEditSelectThrough(bool through);
void SetEditShowHidden(bool show);
const char* GetEditMessage();
void EditGetStats(int total[3], int picked[3]);

struct EditOverlayData {
    std::vector<float> points, hotPoints, lines, hotLines, fill;
    bool seeThrough = false;
    bool vertexMode = false;
    bool faceMode = false;
};
unsigned int EditOverlayStamp();

struct ForeignLock {
    std::string name;
    ImU32 color = 0;
    int mesh = 0;
    std::vector<int> render;
};
void EditSetForeignLocks(const std::vector<ForeignLock>& locks);
int EditDropLockedPicks();
void EditSay(const std::string& text);
void EditMyLockedRender(std::vector<std::pair<int, int>>& out);
void EditExternalVertices(int mesh, const std::vector<std::pair<unsigned int, Vertex>>& patch);
void EditFillOverlay(EditOverlayData& out);

bool EditGetSelectionCenter(SceneRenderer& sceneRenderer, glm::vec3& outWorld);
bool EditBeginTransform(SceneRenderer& sceneRenderer, glm::vec3& outCenterWorld);
void EditApplyTransform(SceneRenderer& sceneRenderer, const std::function<glm::vec3(const glm::vec3&)>& worldTransform);
void EditCancelTransform(SceneRenderer& sceneRenderer);
void EditMoveSelection(SceneRenderer& sceneRenderer, const glm::vec3& worldDelta);
void EditCollectFixedPoints(SceneRenderer& sceneRenderer, std::vector<glm::vec3>& outWorld);

bool EditExtrude(SceneRenderer& sceneRenderer, glm::vec3& outWorldDirection, bool& outAlongNormal);
bool EditSubdivide(SceneRenderer& sceneRenderer);
bool EditMerge(SceneRenderer& sceneRenderer);
int EditWeldDoubles(SceneRenderer& sceneRenderer);
bool EditDelete(SceneRenderer& sceneRenderer);
bool EditJoinOrFill(SceneRenderer& sceneRenderer);
bool EditDissolve(SceneRenderer& sceneRenderer);
bool EditMirrorCopy(SceneRenderer& sceneRenderer, int axis);
bool EditFlip(SceneRenderer& sceneRenderer, int axis);

bool EditBeginTool(SceneRenderer& sceneRenderer, EditTool tool, ImVec2 mouse);
EditTool GetEditTool();
bool EditUpdateTool(SceneRenderer& sceneRenderer, ImDrawList* drawList, const EditView& view, ImVec2 mouse, bool viewHovered, char* readout, size_t readoutSize);
void EditCancelTool(SceneRenderer& sceneRenderer);

void EditSetLastView(const EditView& view);
int EditMeshCount();
const PolyMesh* EditPoly(int m);
bool EditIsSeam(int m, int a, int b);
bool EditPolyVertexSelected(int m, int v);
bool EditPolyFaceSelected(int m, int f);
void EditSelectPolyElement(int m, int f, int corner, int cornerB, bool additive);
void EditSelectPolyBox(const std::function<bool(int, int, int)>& cornerInside, EditBoxMode mode);
bool EditUVSessionActive();
bool EditUVBegin(glm::vec2& outCenter);
void EditUVUpdate(SceneRenderer& sceneRenderer, const std::function<glm::vec2(const glm::vec2&)>& transform);
void EditUVEnd(SceneRenderer& sceneRenderer, bool cancel);
bool EditMarkSeams(SceneRenderer& sceneRenderer, bool mark);
bool EditUnwrapOrProject(SceneRenderer& sceneRenderer, int how);
bool EditUVArrange(SceneRenderer& sceneRenderer, int action);
bool EditSelectIslands();
