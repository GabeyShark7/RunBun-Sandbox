#include "SceneRenderer.h"
#include "TextureLoader.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>

void SceneRenderer::Init() {
    glEnable(GL_DEPTH_TEST);
    framebuffer.Init(800, 600);
    triangle.Init();
}

void SceneRenderer::Render(ImVec2 panelSize) {
    int width = (int)panelSize.x;
    int height = (int)panelSize.y;

    if (width > 0 && height > 0) {
        framebuffer.Resize(width, height);
    }

    framebuffer.Bind();
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 projection = camera.GetProjectionMatrix(width, height);

    if (objects.empty()) {
        triangle.Draw(view, projection);
    } else {
        for (auto& obj : objects) {
            glm::mat4 modelMatrix = glm::mat4(1.0f);
            modelMatrix = glm::translate(modelMatrix, obj.position);
            modelMatrix = glm::rotate(modelMatrix, glm::radians(obj.rotation.x), glm::vec3(1, 0, 0));
            modelMatrix = glm::rotate(modelMatrix, glm::radians(obj.rotation.y), glm::vec3(0, 1, 0));
            modelMatrix = glm::rotate(modelMatrix, glm::radians(obj.rotation.z), glm::vec3(0, 0, 1));
            modelMatrix = glm::scale(modelMatrix, obj.scale);

            obj.model.Draw(view, projection, modelMatrix);
        }
    }

    framebuffer.Unbind();
}

unsigned int SceneRenderer::GetTexture() {
    return framebuffer.GetTexture();
}

Camera& SceneRenderer::GetCamera() {
    return camera;
}

std::string SceneRenderer::GenerateUniqueName(const std::string& baseName) {
    int count = 0;
    for (auto& obj : objects) {
        if (obj.name == baseName) count++;
    }

    if (count == 0) return baseName;

    size_t dotPos = baseName.find_last_of('.');
    std::string namePart = (dotPos == std::string::npos) ? baseName : baseName.substr(0, dotPos);
    std::string extPart = (dotPos == std::string::npos) ? "" : baseName.substr(dotPos);

    std::string candidate;
    int suffix = count;
    do {
        candidate = namePart + "_D" + std::to_string(suffix) + extPart;
        suffix++;
    } while ([&]() {
        for (auto& obj : objects) if (obj.name == candidate) return true;
        return false;
    }());

    return candidate;
}

void SceneRenderer::LoadModel(const std::string& path) {
    SceneObject obj;
    obj.model.Load(path);

    size_t lastSlash = path.find_last_of("/\\");
    std::string baseName = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);

    obj.name = GenerateUniqueName(baseName);
    obj.model.SetName(obj.name);

    objects.push_back(std::move(obj));
    selectedIndex = (int)objects.size() - 1;
}

void SceneRenderer::LoadTextureForSelected(const std::string& path) {
    if (selectedIndex < 0 || selectedIndex >= (int)objects.size()) return;

    unsigned int texID = LoadTexture(path);

    size_t lastSlash = path.find_last_of("/\\");
    std::string texName = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);

    objects[selectedIndex].model.AddTexture(texID, texName);
}

void SceneRenderer::RemoveObject(int index) {
    if (index < 0 || index >= (int)objects.size()) return;
    objects.erase(objects.begin() + index);

    if (selectedIndex == index) selectedIndex = -1;
    else if (selectedIndex > index) selectedIndex--;
}

std::vector<SceneObject>& SceneRenderer::GetObjects() {
    return objects;
}