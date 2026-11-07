#pragma once
#include "imgui/imgui.h"
#include "render/Framebuffer.h"
#include "core/Camera.h"
#include "render/Triangle.h"
#include "core/SceneObject.h"
#include <vector>
#include <string>

class SceneRenderer {
public:
    void Init();
    void Render(ImVec2 panelSize);
    unsigned int GetTexture();
    Camera& GetCamera();
    void LoadModel(const std::string& path);
    void LoadTextureForSelected(const std::string& path);
    void RemoveObject(int index);
    std::vector<SceneObject>& GetObjects();

    int selectedIndex = -1;

private:
    Framebuffer framebuffer;
    Camera camera;
    Triangle triangle;
    std::vector<SceneObject> objects;
    unsigned int shaderProgram = 0; // shared by every model and the triangle

    std::string GenerateUniqueName(const std::string& baseName);
};