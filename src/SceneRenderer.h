#pragma once
#include "imgui/imgui.h"
#include "Framebuffer.h"
#include "Camera.h"
#include "Triangle.h"
#include "SceneObject.h"
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

    std::string GenerateUniqueName(const std::string& baseName);
};