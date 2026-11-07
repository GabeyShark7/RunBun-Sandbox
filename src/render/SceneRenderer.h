#pragma once
#include "imgui/imgui.h"
#include "render/Framebuffer.h"
#include "core/Camera.h"
#include "render/Triangle.h"
#include "core/SceneObject.h"
#include "render/Primitives.h"
#include <vector>
#include <string>

class SceneRenderer {
public:
    void Init();
    void Render(ImVec2 panelSize);
    unsigned int GetTexture();
    Camera& GetCamera();
    void LoadModel(const std::string& path);
    void CreatePrimitive(PrimitiveType type);
    int PickObject(const glm::vec3& rayOrigin, const glm::vec3& rayDir);
    void LoadTextureForSelected(const std::string& path);
    void RemoveObject(int index);
    std::vector<SceneObject>& GetObjects();

    int selectedIndex = -1;
    bool showGrid = true;

private:
    Framebuffer framebuffer;
    Camera camera;
    Triangle triangle;
    std::vector<SceneObject> objects;
    unsigned int shaderProgram = 0; // shared by every model and the triangle

    unsigned int gridVAO = 0;
    unsigned int gridVBO = 0;
    unsigned int gridProgram = 0;

    void InitGrid();
    void DrawGrid(const glm::mat4& view, const glm::mat4& projection);
    std::string GenerateUniqueName(const std::string& baseName);
};