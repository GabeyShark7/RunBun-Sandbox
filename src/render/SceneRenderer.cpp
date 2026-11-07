#include "render/SceneRenderer.h"
#include "render/TextureLoader.h"
#include "render/Shader.h"
#include "render/Primitives.h"
#include "ui/Console.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cfloat>
#include <cmath>

void SceneRenderer::Init() {
    glEnable(GL_DEPTH_TEST);
    framebuffer.Init(800, 600);
    shaderProgram = LoadShader("shaders/basic.vert", "shaders/basic.frag");
    triangle.Init(shaderProgram);
    InitGrid();
}

void SceneRenderer::InitGrid() {
    gridProgram = LoadShader("shaders/grid.vert", "shaders/grid.frag");
    const float quad[] = { -1, 0, -1,   1, 0, -1,   1, 0, 1,   -1, 0, -1,   1, 0, 1,   -1, 0, 1 };

    glGenVertexArrays(1, &gridVAO);
    glGenBuffers(1, &gridVBO);
    glBindVertexArray(gridVAO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void SceneRenderer::DrawGrid(const glm::mat4& view, const glm::mat4& projection) {
    glUseProgram(gridProgram);
    glUniformMatrix4fv(glGetUniformLocation(gridProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(gridProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(gridProgram, "cameraPos"), 1, glm::value_ptr(camera.position));
    glUniform1f(glGetUniformLocation(gridProgram, "extent"), camera.farPlane * 0.9f);

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glBindVertexArray(gridVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void SceneRenderer::Render(ImVec2 panelSize) {
    int width = (int)panelSize.x;
    int height = (int)panelSize.y;

    // Panel is collapsed/hidden, nothing to draw (and a 0 size would break the projection)
    if (width <= 0 || height <= 0) return;

    framebuffer.Resize(width, height);

    framebuffer.Bind();
    glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glm::mat4 view = camera.GetViewMatrix();
    glm::mat4 projection = camera.GetProjectionMatrix(width, height);

    if (objects.empty()) {
        triangle.Draw(view, projection);
    } else {
        for (auto& obj : objects) {
            obj.model.Draw(shaderProgram, view, projection, obj.GetModelMatrix());
        }
    }

    if (showGrid) DrawGrid(view, projection);

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
    size_t lastSlash = path.find_last_of("/\\");
    std::string baseName = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);

    SceneObject obj;
    if (!obj.model.Load(path)) {
        LogMessage("Failed to import '" + baseName + "'.");
        return;
    }

    obj.name = GenerateUniqueName(baseName);
    obj.model.SetName(obj.name);

    objects.push_back(std::move(obj));
    selectedIndex = (int)objects.size() - 1;
}

void SceneRenderer::CreatePrimitive(PrimitiveType type) {
    SceneObject obj;
    obj.model.AddMesh(CreatePrimitiveMesh(type));
    obj.name = GenerateUniqueName(GetPrimitiveName(type));
    obj.model.SetName(obj.name);
    obj.scale = glm::vec3(1.0f);

    glm::vec3 boundsMin, boundsMax;
    obj.model.GetBounds(boundsMin, boundsMax);
    obj.position = camera.position + camera.front * 5.0f;
    obj.position.y = -boundsMin.y;

    objects.push_back(std::move(obj));
    selectedIndex = (int)objects.size() - 1;
    LogMessage("Created '" + objects.back().name + "'.");
}

int SceneRenderer::PickObject(const glm::vec3& rayOrigin, const glm::vec3& rayDir) {
    int best = -1;
    float bestT = FLT_MAX;

    for (int i = 0; i < (int)objects.size(); i++) {
        glm::vec3 boundsMin, boundsMax;
        if (!objects[i].model.GetBounds(boundsMin, boundsMax)) continue;

        glm::vec3 pad = glm::max((boundsMax - boundsMin) * 0.01f, glm::vec3(0.001f));
        boundsMin -= pad;
        boundsMax += pad;

        glm::mat4 inverseModel = glm::inverse(objects[i].GetModelMatrix());
        glm::vec3 origin = glm::vec3(inverseModel * glm::vec4(rayOrigin, 1.0f));
        glm::vec3 dir = glm::vec3(inverseModel * glm::vec4(rayDir, 0.0f));

        float tMin = -FLT_MAX, tMax = FLT_MAX;
        bool hit = true;
        for (int axis = 0; axis < 3 && hit; axis++) {
            if (std::fabs(dir[axis]) < 1e-8f) {
                if (origin[axis] < boundsMin[axis] || origin[axis] > boundsMax[axis]) hit = false;
                continue;
            }
            float t1 = (boundsMin[axis] - origin[axis]) / dir[axis];
            float t2 = (boundsMax[axis] - origin[axis]) / dir[axis];
            if (t1 > t2) std::swap(t1, t2);
            tMin = std::max(tMin, t1);
            tMax = std::min(tMax, t2);
            if (tMin > tMax) hit = false;
        }
        if (!hit || tMax < 0.0f) continue;

        float t = tMin > 0.0f ? tMin : tMax;
        if (t < bestT) {
            bestT = t;
            best = i;
        }
    }

    return best;
}

void SceneRenderer::LoadTextureForSelected(const std::string& path) {
    if (selectedIndex < 0 || selectedIndex >= (int)objects.size()) return;

    size_t lastSlash = path.find_last_of("/\\");
    std::string texName = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);

    unsigned int texID = LoadTexture(path);
    if (texID == 0) {
        LogMessage("Failed to load texture '" + texName + "'.");
        return;
    }

    objects[selectedIndex].model.AddTexture(texID, texName);
}

void SceneRenderer::RemoveObject(int index) {
    if (index < 0 || index >= (int)objects.size()) return;
    objects[index].model.Destroy();
    objects.erase(objects.begin() + index);

    if (selectedIndex == index) selectedIndex = -1;
    else if (selectedIndex > index) selectedIndex--;
}

std::vector<SceneObject>& SceneRenderer::GetObjects() {
    return objects;
}