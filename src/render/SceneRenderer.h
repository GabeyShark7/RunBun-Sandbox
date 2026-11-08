#pragma once
#include "imgui/imgui.h"
#include "render/Framebuffer.h"
#include "core/Camera.h"
#include "render/Triangle.h"
#include "core/SceneObject.h"
#include "render/Primitives.h"
#include <vector>
#include <algorithm>
#include "render/CSG.h"
#include <string>

class SceneRenderer {
public:
    void Init();
    void Render(ImVec2 panelSize);
    unsigned int GetTexture();
    Camera& GetCamera();
    void LoadModel(const std::string& path);
    void CreatePrimitive(PrimitiveType type, const glm::vec3* groundPoint = nullptr, bool hole = false);
    int PickObject(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float* outDistance = nullptr);
    void LoadTextureForSelected(const std::string& path);
    void RemoveObject(int index);
    std::vector<SceneObject>& GetObjects();

    int AddObject(SceneObject&& obj);
    void ClearScene();
    unsigned int NewObjectId() { return nextObjectId++; }
    // One id block each
    void NoteObjectId(unsigned int id) { if ((id >> 24) == (idBase >> 24) && id >= nextObjectId) nextObjectId = id + 1; }
    void SetIdBase(unsigned int base) {
        idBase = base;
        unsigned int highest = base;
        for (const SceneObject& obj : objects) if ((obj.id >> 24) == (base >> 24) && obj.id >= highest) highest = obj.id + 1;
        nextObjectId = std::max(highest, base + 1);
    }

    // Active is selectedIndex
    bool IsSelected(int index) const;
    void Select(int index, bool additive);
    void SelectRange(int from, int to);
    void SelectAll(bool select);
    void SetSelection(const std::vector<int>& indices, int active);
    const std::vector<int>& GetSelection() const { return selection; }
    void ValidateSelection();
    bool GetSelectionBounds(glm::vec3& outMin, glm::vec3& outMax) const;
    glm::vec3 GetSelectionCenter() const;

    void DeleteSelected();
    void DuplicateSelected();
    bool JoinSelected();
    bool GroupSelected();
    bool UngroupSelected();
    bool SetShapeSettings(int index, int sides, float thickness);
    void DropSelectedToGround();
    void NudgeSelected(const glm::vec3& worldDelta);
    void AlignSelected(int axis, int where);
    void MirrorSelected(int axis);
    void FocusAll();
    int ExportSTL(const std::string& path);
    int CreateLight(int type);
    void DropLightsFromSelection();
    // Rgba rows bottom first
    bool RenderImage(int width, int height, bool transparent, std::vector<uint8_t>& rgba, std::string& error);
    void UpdateDisplayMeshes(int editIndex, bool interacting = false);
    bool GetObjectRaw(int index, RawMesh& out, bool display);
    bool ApplyModifiers(int index, size_t count);
    ModifierTargetFetch PartnerFetch(int index);
    void RenameObject(int index, const std::string& name);
    void FocusSelection();

    int selectedIndex = -1;
    int hoveredIndex = -1;
    int listHoveredIndex = -1;
    bool showGrid = true;
    glm::vec3 backgroundColor = glm::vec3(0.239f);
    glm::vec3 highlightColor = glm::vec3(0.26f, 0.56f, 0.96f);
    int viewStyle = 0;
    int backgroundMode = 0;
    glm::vec3 backgroundBottom = glm::vec3(0.12f);
    std::string backgroundImage;
    bool shadows = true;
    bool showLights = true;
    int lastViewHeight = 600;

private:
    Framebuffer framebuffer;
    Camera camera;
    Triangle triangle;
    std::vector<SceneObject> objects;
    std::vector<int> selection;
    unsigned int nextObjectId = 1;
    unsigned int idBase = 0;
    unsigned int shaderProgram = 0; // shared by every model and the triangle

    unsigned int gridVAO = 0;
    unsigned int gridVBO = 0;
    unsigned int gridProgram = 0;

    void InitGrid();

    unsigned int shadowProgram = 0;
    unsigned int shadowFbo = 0;
    unsigned int shadowTexture = 0;
    int shadowSize = 2048;
    unsigned int backgroundProgram = 0;
    unsigned int backgroundVao = 0;
    struct SceneLight {
        int type;
        glm::vec3 position;
        glm::vec3 direction;
        glm::vec3 color;
        float power;
        float cone;
    };
    std::vector<SceneLight> GatherLights(bool& defaultLight);
    bool DrawShadowMap(const std::vector<SceneLight>& lights, int& shadowLight, glm::mat4& lightSpace);
    void DrawBackground(int width, int height, bool transparent);
    void DrawWorld(const glm::mat4& view, const glm::mat4& projection, int width, int height, bool forImage, bool transparent);

    struct OverlayBuffer { unsigned int vao = 0, vbo = 0; int count = 0; };
    OverlayBuffer overlayPoints, overlayHotPoints, overlayLines, overlayHotLines, overlayFill;
    unsigned int overlayProgram = 0;
    unsigned int overlayStamp = 0;
    int overlayObject = -1;
    bool overlaySeeThrough = false;
    void DrawEditWireframe(const glm::mat4& view, const glm::mat4& projection);
    void DrawGrid(const glm::mat4& view, const glm::mat4& projection, bool lines, bool groundShadow, const glm::mat4& lightSpace);
    std::string GenerateUniqueName(const std::string& baseName);
};
