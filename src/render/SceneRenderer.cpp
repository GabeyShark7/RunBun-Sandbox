#include "core/Profiler.h"
#include "render/SceneRenderer.h"
#include "render/TextureLoader.h"
#include "render/Shader.h"
#include "render/Primitives.h"
#include "ui/Console.h"
#include "ui/EditMode.h"
#include "render/RayBVH.h"
#include <chrono>
#include <future>
#include <memory>
#include <unordered_map>
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <string>

void SceneRenderer::Init() {
    glEnable(GL_DEPTH_TEST);
    framebuffer.Init(800, 600);
    shaderProgram = LoadShader("shaders/basic.vert", "shaders/basic.frag");
    triangle.Init(shaderProgram);
    InitGrid();
    overlayProgram = LoadShader("shaders/overlay.vert", "shaders/overlay.frag");
    shadowProgram = LoadShader("shaders/shadow.vert", "shaders/shadow.frag");
    backgroundProgram = LoadShader("shaders/background.vert", "shaders/background.frag");
    glGenVertexArrays(1, &backgroundVao);
    glGenTextures(1, &shadowTexture);
    glBindTexture(GL_TEXTURE_2D, shadowTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, shadowSize, shadowSize, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenFramebuffers(1, &shadowFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, shadowTexture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    for (OverlayBuffer* b : { &overlayPoints, &overlayHotPoints, &overlayLines, &overlayHotLines, &overlayFill }) {
        glGenVertexArrays(1, &b->vao);
        glGenBuffers(1, &b->vbo);
        glBindVertexArray(b->vao);
        glBindBuffer(GL_ARRAY_BUFFER, b->vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
    }
    glBindVertexArray(0);
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

void SceneRenderer::DrawGrid(const glm::mat4& view, const glm::mat4& projection, bool lines, bool groundShadow, const glm::mat4& lightSpace) {
    glUseProgram(gridProgram);
    glUniformMatrix4fv(glGetUniformLocation(gridProgram, "view"), 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(gridProgram, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
    glUniform3fv(glGetUniformLocation(gridProgram, "cameraPos"), 1, glm::value_ptr(camera.position));
    glUniform1f(glGetUniformLocation(gridProgram, "extent"), camera.farPlane * 0.9f);
    glUniform1i(glGetUniformLocation(gridProgram, "gridLines"), lines ? 1 : 0);
    glUniform1i(glGetUniformLocation(gridProgram, "useShadows"), groundShadow ? 1 : 0);
    glUniformMatrix4fv(glGetUniformLocation(gridProgram, "lightSpace"), 1, GL_FALSE, glm::value_ptr(lightSpace));
    glUniform1i(glGetUniformLocation(gridProgram, "shadowMap"), 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowTexture);
    glActiveTexture(GL_TEXTURE0);

    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glBindVertexArray(gridVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

std::vector<SceneRenderer::SceneLight> SceneRenderer::GatherLights(bool& defaultLight) {
    std::vector<SceneLight> lights;
    for (const SceneObject& obj : objects) {
        if (obj.look.light < 0 || lights.size() >= 8) continue;
        glm::mat4 matrix = obj.GetModelMatrix();
        glm::vec3 down = glm::mat3(matrix) * glm::vec3(0.0f, 0.0f, -1.0f);
        SceneLight light;
        light.type = obj.look.light;
        light.position = glm::vec3(matrix[3]);
        light.direction = glm::length(down) > 1e-8f ? glm::normalize(down) : glm::vec3(0.0f, 0.0f, -1.0f);
        light.color = glm::vec3(obj.color);
        light.power = obj.look.power;
        light.cone = std::cos(glm::radians(std::clamp(obj.look.spotAngle, 1.0f, 179.0f) * 0.5f));
        lights.push_back(light);
    }
    defaultLight = lights.empty();
    if (defaultLight) lights.push_back({ Light_Sun, glm::vec3(0.0f), -glm::normalize(glm::vec3(0.5f, -0.7f, 1.0f)), glm::vec3(1.0f), 1.0f, 0.0f });
    return lights;
}

// Depth from the first sun
bool SceneRenderer::DrawShadowMap(const std::vector<SceneLight>& lights, int& shadowLight, glm::mat4& lightSpace) {
    shadowLight = -1;
    for (int i = 0; i < (int)lights.size(); i++) if (lights[i].type == Light_Sun) { shadowLight = i; break; }
    if (shadowLight < 0 || !shadowProgram) return false;
    glm::vec3 lo(1e30f), hi(-1e30f);
    bool any = false;
    for (const SceneObject& obj : objects) {
        if (obj.look.light >= 0 || obj.hole || obj.ghost || !obj.look.castShadows) continue;
        glm::vec3 a, b;
        if (!obj.GetWorldBounds(a, b)) continue;
        lo = glm::min(lo, a);
        hi = glm::max(hi, b);
        any = true;
    }
    if (!any) return false;
    lo.z = std::min(lo.z, 0.0f);
    glm::vec3 center = (lo + hi) * 0.5f;
    float radius = glm::length(hi - lo) * 0.5f + 0.05f;
    // Room for floor shadows
    radius *= 1.6f;
    glm::vec3 dir = lights[shadowLight].direction;
    glm::vec3 up = std::fabs(dir.z) > 0.99f ? glm::vec3(0.0f, 1.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
    glm::mat4 lightView = glm::lookAt(center - dir * radius * 2.0f, center, up);
    glm::mat4 lightProj = glm::ortho(-radius, radius, -radius, radius, 0.01f, radius * 4.0f);
    lightSpace = lightProj * lightView;

    glBindFramebuffer(GL_FRAMEBUFFER, shadowFbo);
    glViewport(0, 0, shadowSize, shadowSize);
    glClear(GL_DEPTH_BUFFER_BIT);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(2.0f, 4.0f);
    glUseProgram(shadowProgram);
    glUniformMatrix4fv(glGetUniformLocation(shadowProgram, "lightSpace"), 1, GL_FALSE, glm::value_ptr(lightSpace));
    int modelLoc = glGetUniformLocation(shadowProgram, "model");
    for (SceneObject& obj : objects) {
        if (obj.look.light >= 0 || obj.hole || obj.ghost || !obj.look.castShadows || obj.color.a < 0.3f) continue;
        glm::mat4 matrix = obj.GetModelMatrix();
        glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(matrix));
        for (Mesh& mesh : obj.DisplayMeshes()) mesh.Draw();
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    return true;
}

void SceneRenderer::DrawBackground(int width, int height, bool transparent) {
    if (transparent || backgroundMode == 0 || !backgroundProgram) return;
    unsigned int image = 0;
    float imageAspect = 1.0f;
    if (backgroundMode == 2) {
        if (backgroundImage.empty()) return;
        image = AcquireTexture(backgroundImage);
        if (!image) return;
        int w = 1, h = 1;
        glBindTexture(GL_TEXTURE_2D, image);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &w);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &h);
        imageAspect = h > 0 ? (float)w / (float)h : 1.0f;
    }
    glUseProgram(backgroundProgram);
    glUniform1i(glGetUniformLocation(backgroundProgram, "mode"), backgroundMode);
    glUniform3fv(glGetUniformLocation(backgroundProgram, "topColor"), 1, glm::value_ptr(backgroundColor));
    glUniform3fv(glGetUniformLocation(backgroundProgram, "bottomColor"), 1, glm::value_ptr(backgroundBottom));
    glUniform1f(glGetUniformLocation(backgroundProgram, "viewAspect"), (float)width / (float)std::max(height, 1));
    glUniform1f(glGetUniformLocation(backgroundProgram, "imageAspect"), imageAspect);
    glUniform1i(glGetUniformLocation(backgroundProgram, "image"), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, image);
    glDepthMask(GL_FALSE);
    glDisable(GL_DEPTH_TEST);
    glBindVertexArray(backgroundVao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
}

void SceneRenderer::DrawWorld(const glm::mat4& view, const glm::mat4& projection, int width, int height, bool forImage, bool transparent) {
    int style = forImage ? 1 : viewStyle;
    GLint target = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &target);

    bool defaultLight = false;
    std::vector<SceneLight> lights = GatherLights(defaultLight);
    int shadowLight = -1;
    glm::mat4 lightSpace(1.0f);
    bool useShadows = style == 1 && shadows && DrawShadowMap(lights, shadowLight, lightSpace);

    glBindFramebuffer(GL_FRAMEBUFFER, (unsigned int)target);
    glViewport(0, 0, width, height);
    if (transparent) glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    else glClearColor(backgroundColor.r, backgroundColor.g, backgroundColor.b, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    DrawBackground(width, height, transparent);

    glUseProgram(shaderProgram);
    auto loc = [&](const char* name) { return glGetUniformLocation(shaderProgram, name); };
    glUniform3fv(loc("viewPos"), 1, glm::value_ptr(camera.position));
    glUniform3fv(loc("highlightColor"), 1, glm::value_ptr(highlightColor));
    glUniform1i(loc("shadeMode"), style == 1 ? 1 : (style == 2 ? 2 : 0));
    glUniform1f(loc("alphaScale"), 1.0f);
    glUniform1i(loc("unlit"), 0);
    int count = (int)lights.size();
    glUniform1i(loc("lightCount"), count);
    for (int i = 0; i < count; i++) {
        std::string at = "[" + std::to_string(i) + "]";
        glUniform1i(loc(("lightType" + at).c_str()), lights[i].type);
        glUniform3fv(loc(("lightPos" + at).c_str()), 1, glm::value_ptr(lights[i].position));
        glUniform3fv(loc(("lightDir" + at).c_str()), 1, glm::value_ptr(lights[i].direction));
        glUniform3fv(loc(("lightColor" + at).c_str()), 1, glm::value_ptr(lights[i].color));
        glUniform1f(loc(("lightPower" + at).c_str()), lights[i].power);
        glUniform1f(loc(("lightCone" + at).c_str()), lights[i].cone);
    }
    glm::vec3 sky = defaultLight ? glm::vec3(0.62f, 0.64f, 0.68f) : glm::vec3(0.50f, 0.52f, 0.57f);
    glm::vec3 ground = defaultLight ? glm::vec3(0.32f, 0.31f, 0.30f) : glm::vec3(0.26f, 0.25f, 0.24f);
    glUniform3fv(loc("skyColor"), 1, glm::value_ptr(sky));
    glUniform3fv(loc("groundColor"), 1, glm::value_ptr(ground));
    glUniform1i(loc("useShadows"), useShadows ? 1 : 0);
    glUniform1i(loc("shadowLight"), shadowLight);
    glUniformMatrix4fv(loc("lightSpace"), 1, GL_FALSE, glm::value_ptr(lightSpace));
    glUniform1i(loc("shadowMap"), 1);
    glUniform1i(loc("texture1"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, shadowTexture);
    glActiveTexture(GL_TEXTURE0);
    int colorLoc = loc("baseColor");
    int highlightLoc = loc("highlight");

    if (objects.empty()) {
        if (!forImage) {
            glUniform4f(colorLoc, 0.8f, 0.8f, 0.8f, 1.0f);
            glUniform1f(highlightLoc, 0.0f);
            glUniform1i(loc("shadeMode"), 0);
            triangle.Draw(view, projection);
        }
    } else if (style == 2) {
        glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        for (int i = 0; i < (int)objects.size(); i++) {
            SceneObject& obj = objects[i];
            if (obj.look.light >= 0 && !showLights) continue;
            bool picked = IsSelected(i);
            glm::vec4 color = picked ? glm::vec4(highlightColor, 1.0f) : glm::vec4(glm::mix(glm::vec3(obj.color), glm::vec3(0.85f), 0.5f), 1.0f);
            if (obj.hole || obj.ghost) color = glm::vec4(0.55f, 0.58f, 0.66f, 1.0f);
            glUseProgram(shaderProgram);
            glUniform4fv(colorLoc, 1, glm::value_ptr(color));
            glUniform1f(highlightLoc, 0.0f);
            obj.model.Draw(shaderProgram, view, projection, obj.GetModelMatrix(), &obj.DisplayMeshes());
        }
        glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    } else {
        bool xray = style == 3;
        // See through ones last
        for (int pass = 0; pass < 2; pass++) {
            if (pass == 1) {
                glEnable(GL_BLEND);
                glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                glDepthMask(GL_FALSE);
            }
            for (int i = 0; i < (int)objects.size(); i++) {
                SceneObject& obj = objects[i];
                bool isLight = obj.look.light >= 0;
                if (isLight && (forImage || !showLights)) continue;
                bool seeThrough = (obj.hole || obj.ghost) && !obj.showBase;
                bool clear = seeThrough || xray || (style == 1 && obj.color.a < 0.999f && !isLight);
                if (isLight) clear = false;
                if (clear != (pass == 1)) continue;
                float highlight = forImage ? 0.0f : (i == selectedIndex ? 1.0f : (IsSelected(i) ? 0.7f : ((i == hoveredIndex || i == listHoveredIndex) ? 0.3f : 0.0f)));
                glm::vec4 color = obj.color;
                if (style != 1) color.a = 1.0f;
                if (seeThrough) color = obj.hole ? glm::vec4(0.62f, 0.64f, 0.70f, 0.38f) : glm::vec4(obj.color.r, obj.color.g, obj.color.b, 0.18f);
                glUseProgram(shaderProgram);
                glUniform4fv(colorLoc, 1, glm::value_ptr(color));
                glUniform1f(highlightLoc, highlight);
                glUniform1f(loc("alphaScale"), xray && !seeThrough ? 0.35f : 1.0f);
                glUniform1i(loc("unlit"), isLight ? 1 : 0);
                glUniform1f(loc("roughness"), obj.look.roughness);
                glUniform1f(loc("metallic"), obj.look.metallic);
                glUniform1f(loc("glow"), obj.look.glow);
                obj.model.Draw(shaderProgram, view, projection, obj.GetModelMatrix(), &obj.DisplayMeshes());
            }
        }
        glUniform1f(loc("alphaScale"), 1.0f);
        glUniform1i(loc("unlit"), 0);
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }

    if (!forImage) DrawEditWireframe(view, projection);
    bool lines = !forImage && showGrid;
    if (lines || useShadows) DrawGrid(view, projection, lines, useShadows, lightSpace);
}

void SceneRenderer::Render(ImVec2 panelSize) {
    PROFILE("Render");
    int width = (int)panelSize.x;
    int height = (int)panelSize.y;

    // Panel is collapsed/hidden, nothing to draw (and a 0 size would break the projection)
    if (width <= 0 || height <= 0) return;

    framebuffer.Resize(width, height);

    framebuffer.Bind();
    DrawWorld(camera.GetViewMatrix(), camera.GetProjectionMatrix(width, height), width, height, false, false);
    framebuffer.Unbind();
}

bool SceneRenderer::RenderImage(int width, int height, bool transparent, std::vector<uint8_t>& rgba, std::string& error) {
    GLint maxSize = 0, dims[2] = { 0, 0 };
    glGetIntegerv(GL_MAX_RENDERBUFFER_SIZE, &maxSize);
    glGetIntegerv(GL_MAX_VIEWPORT_DIMS, dims);
    int limit = std::min((int)maxSize, std::min((int)dims[0], (int)dims[1]));
    if (width < 16 || height < 16 || width > limit || height > limit) {
        error = "Pick a size between 16 and " + std::to_string(limit) + " pixels.";
        return false;
    }
    GLint samples = 0;
    glGetIntegerv(GL_MAX_SAMPLES, &samples);
    samples = std::min(samples, 8);

    unsigned int msFbo = 0, msColor = 0, msDepth = 0, outFbo = 0, outColor = 0;
    glGenFramebuffers(1, &msFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, msFbo);
    glGenRenderbuffers(1, &msColor);
    glBindRenderbuffer(GL_RENDERBUFFER, msColor);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_RGBA8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, msColor);
    glGenRenderbuffers(1, &msDepth);
    glBindRenderbuffer(GL_RENDERBUFFER, msDepth);
    glRenderbufferStorageMultisample(GL_RENDERBUFFER, samples, GL_DEPTH24_STENCIL8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, msDepth);
    bool ok = glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    glGenFramebuffers(1, &outFbo);
    glBindFramebuffer(GL_FRAMEBUFFER, outFbo);
    glGenRenderbuffers(1, &outColor);
    glBindRenderbuffer(GL_RENDERBUFFER, outColor);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA8, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, outColor);
    ok = ok && glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;

    if (ok) {
        glBindFramebuffer(GL_FRAMEBUFFER, msFbo);
        DrawWorld(camera.GetViewMatrix(), camera.GetProjectionMatrix(width, height), width, height, true, transparent);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, msFbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, outFbo);
        glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, outFbo);
        rgba.resize((size_t)width * height * 4);
        glPixelStorei(GL_PACK_ALIGNMENT, 1);
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        if (!transparent) for (size_t k = 3; k < rgba.size(); k += 4) rgba[k] = 255;
    } else {
        error = "This graphics card could not make an image that big.";
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glDeleteRenderbuffers(1, &msColor);
    glDeleteRenderbuffers(1, &msDepth);
    glDeleteRenderbuffers(1, &outColor);
    glDeleteFramebuffers(1, &msFbo);
    glDeleteFramebuffers(1, &outFbo);
    return ok;
}

int SceneRenderer::CreateLight(int type) {
    static const char* names[Light_Count] = { "Sun Light", "Point Light", "Spot Light" };
    if (type < 0 || type >= Light_Count) return -1;
    SceneObject obj;
    obj.model.AddMesh(CreatePrimitiveMesh(type == Light_Spot ? Primitive_Cone : Primitive_Sphere, 16, 0.25f));
    glm::vec3 lo, hi;
    obj.model.GetBounds(lo, hi);
    float size = std::max(std::max(hi.x - lo.x, hi.y - lo.y), hi.z - lo.z);
    obj.scale = glm::vec3((type == Light_Sun ? 0.3f : 0.18f) / std::max(size, 1e-4f));
    obj.name = GenerateUniqueName(names[type]);
    obj.model.SetName(obj.name);
    obj.color = type == Light_Sun ? glm::vec4(1.0f, 0.97f, 0.92f, 1.0f) : glm::vec4(1.0f, 0.9f, 0.75f, 1.0f);
    obj.look.light = type;
    obj.look.power = type == Light_Sun ? 1.0f : (type == Light_Point ? 6.0f : 12.0f);
    obj.look.castShadows = false;
    glm::vec3 focus = camera.position + camera.front * 5.0f;
    if (camera.front.z < -0.05f) {
        float t = -camera.position.z / camera.front.z;
        if (t > 0.0f && t < 500.0f) focus = camera.position + camera.front * t;
    }
    focus.z = 0.0f;
    obj.position = focus + glm::vec3(0.0f, 0.0f, type == Light_Sun ? 4.0f : (type == Light_Point ? 1.5f : 3.0f));
    if (type == Light_Sun) {
        // Shadow falls in view
        glm::vec3 flatFront = glm::normalize(glm::vec3(camera.front.x, camera.front.y, 0.0f) + glm::vec3(1e-4f, 0.0f, 0.0f));
        glm::vec3 right = glm::vec3(flatFront.y, -flatFront.x, 0.0f);
        glm::vec3 travel = glm::normalize(right * 0.7f - flatFront * 0.5f);
        glm::vec3 d = glm::normalize(travel * 0.64f + glm::vec3(0.0f, 0.0f, -0.77f));
        obj.rotation = glm::vec3(glm::degrees(std::atan2(d.y, -d.z)), glm::degrees(std::asin(std::clamp(-d.x, -1.0f, 1.0f))), 0.0f);
    }
    int index = AddObject(std::move(obj));
    SetSelection({ index }, index);
    LogSuccess("Added '" + objects.back().name + "'.");
    return index;
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
    // Count up not stack up
    size_t mark = namePart.rfind("_D");
    if (mark != std::string::npos && mark + 2 < namePart.size() && namePart.find_first_not_of("0123456789", mark + 2) == std::string::npos)
        namePart = namePart.substr(0, mark);

    std::string candidate;
    int suffix = 1;
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
        LogError("Failed to import '" + baseName + "'.");
        return;
    }

    obj.name = GenerateUniqueName(baseName);
    obj.model.SetName(obj.name);

    int index = AddObject(std::move(obj));
    SetSelection({ index }, index);
}

void SceneRenderer::CreatePrimitive(PrimitiveType type, const glm::vec3* groundPoint, bool hole) {
    SceneObject obj;
    obj.model.AddMesh(CreatePrimitiveMesh(type, obj.shapeSides, obj.shapeThickness));
    obj.shape = type;
    obj.shapeRevision = obj.model.GetMeshes()[0].revision;
    obj.hole = hole;
    obj.name = GenerateUniqueName(hole ? std::string("Hole ") + GetPrimitiveName(type) : std::string(GetPrimitiveName(type)));
    obj.model.SetName(obj.name);
    obj.scale = glm::vec3(1.0f);

    glm::vec3 boundsMin, boundsMax;
    obj.model.GetBounds(boundsMin, boundsMax);
    obj.position = groundPoint ? *groundPoint : camera.position + camera.front * 5.0f;
    obj.position.z = (groundPoint ? groundPoint->z : 0.0f) - boundsMin.z;

    int index = AddObject(std::move(obj));
    SetSelection({ index }, index);
    LogSuccess("Created '" + objects.back().name + "'.");
}

// Trees cached per mesh
static const RayBVH* MeshTree(const Mesh& mesh) {
    static std::unordered_map<unsigned int, std::shared_ptr<RayBVH>> ready;
    static std::unordered_map<unsigned int, std::shared_future<std::shared_ptr<RayBVH>>> building;
    auto found = ready.find(mesh.revision);
    if (found != ready.end()) return found->second.get();
    auto waiting = building.find(mesh.revision);
    if (waiting != building.end()) {
        if (waiting->second.wait_for(std::chrono::seconds(0)) != std::future_status::ready) return nullptr;
        ready[mesh.revision] = waiting->second.get();
        building.erase(waiting);
        return ready[mesh.revision].get();
    }
    if (ready.size() > 64) ready.clear();
    if (mesh.GetIndices().size() < 60000) {
        auto tree = std::make_shared<RayBVH>();
        tree->Build(mesh.GetVertices(), mesh.GetIndices());
        ready[mesh.revision] = tree;
        return tree.get();
    }
    std::vector<Vertex> vertices = mesh.GetVertices();
    std::vector<unsigned int> indices = mesh.GetIndices();
    building[mesh.revision] = std::async(std::launch::async, [vertices = std::move(vertices), indices = std::move(indices)]() {
        auto tree = std::make_shared<RayBVH>();
        tree->Build(vertices, indices);
        return tree;
    }).share();
    return nullptr;
}

int SceneRenderer::PickObject(const glm::vec3& rayOrigin, const glm::vec3& rayDir, float* outDistance) {
    PROFILE("PickObject");
    int best = -1;
    float bestT = FLT_MAX;

    for (int i = 0; i < (int)objects.size(); i++) {
        glm::vec3 boundsMin, boundsMax;
        if (!objects[i].GetLocalBounds(boundsMin, boundsMax)) continue;

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
        float boxT = tMin > 0.0f ? tMin : tMax;
        if (boxT >= bestT) continue;

        // Real surface not the box
        float t = FLT_MAX;
        for (Mesh& mesh : objects[i].DisplayMeshes()) {
            float hit;
            const RayBVH* tree = MeshTree(mesh);
            if (!tree) { t = std::min(t, boxT); continue; }
            if (tree->Hit(origin, dir, hit) && hit < t) t = hit;
        }
        if (t < bestT) {
            bestT = t;
            best = i;
        }
    }

    if (outDistance) *outDistance = bestT;
    return best;
}

void SceneRenderer::LoadTextureForSelected(const std::string& path) {
    if (selectedIndex < 0 || selectedIndex >= (int)objects.size()) return;

    size_t lastSlash = path.find_last_of("/\\");
    std::string texName = (lastSlash == std::string::npos) ? path : path.substr(lastSlash + 1);

    unsigned int texID = AcquireTexture(path);
    if (texID == 0) {
        LogError("Failed to load texture '" + texName + "'.");
        return;
    }

    Model& model = objects[selectedIndex].model;
    model.AddTexture(texID, texName, path, model.GetTextures().empty());
}

void SceneRenderer::RemoveObject(int index) {
    if (index < 0 || index >= (int)objects.size()) return;
    objects[index].model.Destroy();
    objects[index].evaluated.Destroy();
    objects.erase(objects.begin() + index);

    std::vector<int> kept;
    for (int i : selection) {
        if (i == index) continue;
        kept.push_back(i > index ? i - 1 : i);
    }
    selection = kept;
    if (selectedIndex == index) selectedIndex = selection.empty() ? -1 : selection.back();
    else if (selectedIndex > index) selectedIndex--;
    if (hoveredIndex == index) hoveredIndex = -1;
    else if (hoveredIndex > index) hoveredIndex--;
}

std::vector<SceneObject>& SceneRenderer::GetObjects() {
    return objects;
}
int SceneRenderer::AddObject(SceneObject&& obj) {
    if (obj.id == 0) obj.id = NewObjectId();
    else NoteObjectId(obj.id);
    objects.push_back(std::move(obj));
    return (int)objects.size() - 1;
}

void SceneRenderer::ClearScene() {
    for (auto& obj : objects) {
        obj.model.Destroy();
        obj.evaluated.Destroy();
    }
    objects.clear();
    selection.clear();
    selectedIndex = -1;
    hoveredIndex = -1;
}

bool SceneRenderer::IsSelected(int index) const {
    return std::find(selection.begin(), selection.end(), index) != selection.end();
}

void SceneRenderer::Select(int index, bool additive) {
    bool valid = index >= 0 && index < (int)objects.size();
    if (!additive) {
        selection.clear();
        if (valid) selection.push_back(index);
        selectedIndex = valid ? index : -1;
        return;
    }
    if (!valid) return;
    auto found = std::find(selection.begin(), selection.end(), index);
    if (found != selection.end() && selectedIndex == index) {
        selection.erase(found);
        selectedIndex = selection.empty() ? -1 : selection.back();
    } else {
        if (found == selection.end()) selection.push_back(index);
        selectedIndex = index;
    }
}

void SceneRenderer::SelectRange(int from, int to) {
    if (from < 0 || from >= (int)objects.size()) { Select(to, false); return; }
    int low = std::min(from, to), high = std::max(from, to);
    for (int i = low; i <= high && i < (int)objects.size(); i++)
        if (!IsSelected(i)) selection.push_back(i);
    selectedIndex = to;
}

void SceneRenderer::SelectAll(bool select) {
    int active = selectedIndex;
    selection.clear();
    if (select) for (int i = 0; i < (int)objects.size(); i++) selection.push_back(i);
    selectedIndex = selection.empty() ? -1 : (IsSelected(active) ? active : selection.back());
}

void SceneRenderer::SetSelection(const std::vector<int>& indices, int active) {
    selection.clear();
    for (int i : indices)
        if (i >= 0 && i < (int)objects.size() && !IsSelected(i)) selection.push_back(i);
    selectedIndex = IsSelected(active) ? active : (selection.empty() ? -1 : selection.back());
}

void SceneRenderer::ValidateSelection() {
    std::vector<int> kept;
    for (int i : selection)
        if (i >= 0 && i < (int)objects.size() && std::find(kept.begin(), kept.end(), i) == kept.end()) kept.push_back(i);
    selection = kept;
    if (selectedIndex >= (int)objects.size()) selectedIndex = -1;
    // Old code sets it directly
    if (selectedIndex >= 0 && !IsSelected(selectedIndex)) selection = { selectedIndex };
    if (selectedIndex < 0) selection.clear();
    if (hoveredIndex >= (int)objects.size()) hoveredIndex = -1;
}

bool SceneRenderer::GetSelectionBounds(glm::vec3& outMin, glm::vec3& outMax) const {
    bool any = false;
    for (int i : selection) {
        glm::vec3 a, b;
        if (!objects[i].GetWorldBounds(a, b)) continue;
        outMin = any ? glm::min(outMin, a) : a;
        outMax = any ? glm::max(outMax, b) : b;
        any = true;
    }
    return any;
}

glm::vec3 SceneRenderer::GetSelectionCenter() const {
    if (selection.empty()) return glm::vec3(0.0f);
    glm::vec3 sum(0.0f);
    for (int i : selection) sum += objects[i].position;
    return sum / (float)selection.size();
}

void SceneRenderer::DeleteSelected() {
    std::vector<int> doomed = selection;
    std::sort(doomed.begin(), doomed.end());
    for (int k = (int)doomed.size() - 1; k >= 0; k--) RemoveObject(doomed[k]);
    selection.clear();
    selectedIndex = -1;
    if (!doomed.empty()) LogSuccess("Deleted " + std::to_string(doomed.size()) + (doomed.size() == 1 ? " object." : " objects."));
}

void SceneRenderer::DuplicateSelected() {
    std::vector<int> copies;
    int activeCopy = -1;
    std::vector<int> source = selection;
    for (int index : source) {
        SceneObject copy;
        copy.name = GenerateUniqueName(objects[index].name);
        copy.model.SetName(copy.name);
        copy.position = objects[index].position;
        copy.rotation = objects[index].rotation;
        copy.scale = objects[index].scale;
        copy.color = objects[index].color;
        copy.hole = objects[index].hole;
        copy.shape = objects[index].shape;
        copy.shapeSides = objects[index].shapeSides;
        copy.shapeThickness = objects[index].shapeThickness;
        copy.modifiers = objects[index].modifiers;
        copy.look = objects[index].look;
        copy.parts = objects[index].parts;
        bool unedited = objects[index].ShapeEditable();
        if (objects[index].evaluatedActive) {
            for (Mesh& mesh : objects[index].evaluated.GetMeshes()) {
                Mesh shown;
                shown.Init(mesh.GetVertices(), mesh.GetIndices());
                copy.evaluated.AddMesh(shown);
            }
            copy.evaluatedActive = true;
            copy.evaluateMs = std::max(objects[index].evaluateMs, 2.5f);
        }
        // New GPU buffers
        for (const Mesh& mesh : objects[index].model.GetMeshes()) {
            Mesh fresh;
            fresh.Init(mesh.GetVertices(), mesh.GetIndices());
            copy.model.AddMesh(fresh);
        }
        for (const TextureInfo& tex : objects[index].model.GetTextures()) copy.model.GetTextures().push_back(tex);
        if (unedited) copy.shapeRevision = copy.model.GetMeshes()[0].revision;
        int added = AddObject(std::move(copy));
        copies.push_back(added);
        if (index == selectedIndex) activeCopy = added;
    }
    SetSelection(copies, activeCopy);
}

bool SceneRenderer::JoinSelected() {
    if (selection.size() < 2) return false;
    int target = selection.front();
    bool anyHole = false, anySolid = false;
    for (int index : selection) {
        if (objects[index].hole) anyHole = true;
        else anySolid = true;
    }
    for (int index : selection)
        if (anySolid && !objects[index].hole) { target = index; break; }
    if (anyHole) {
        // Holes cut not add
        SceneObject& base = objects[target];
        glm::mat4 toBase = glm::inverse(base.GetModelMatrix());
        RawMesh solids, holes;
        bool haveSolid = false;
        for (int index : selection) {
            RawMesh raw, placed;
            GetObjectRaw(index, raw, true);
            AppendRawMesh(placed, raw, index == target ? glm::mat4(1.0f) : toBase * objects[index].GetModelMatrix());
            if (objects[index].hole) holes = holes.indices.empty() ? placed : CSGCombine(holes, placed, CSG_Union);
            else {
                solids = haveSolid ? CSGCombine(solids, placed, CSG_Union) : placed;
                haveSolid = true;
            }
        }
        RawMesh result = haveSolid ? CSGCombine(solids, holes, CSG_Subtract) : holes;
        for (Mesh& mesh : base.model.GetMeshes()) mesh.Destroy();
        base.model.GetMeshes().clear();
        Mesh joined;
        joined.Init(result.vertices, result.indices);
        base.model.AddMesh(joined);
        base.modifiers.clear();
        base.parts.reset();
        base.shape = -1;
        base.hole = !haveSolid;
        unsigned int keepId = base.id;
        std::string keepName = base.name;
        int count = (int)selection.size();
        std::vector<int> doomed;
        for (int index : selection) if (index != target) doomed.push_back(index);
        std::sort(doomed.begin(), doomed.end());
        for (int k = (int)doomed.size() - 1; k >= 0; k--) RemoveObject(doomed[k]);
        for (int i = 0; i < (int)objects.size(); i++)
            if (objects[i].id == keepId) { SetSelection({ i }, i); break; }
        LogSuccess("Joined " + std::to_string(count) + " objects into '" + keepName + "', with holes cut out.");
        return true;
    }
    SceneObject& base = objects[target];
    glm::mat4 toBase = glm::inverse(base.GetModelMatrix());

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    for (int index : selection) {
        SceneObject& obj = objects[index];
        glm::mat4 transform = index == target ? glm::mat4(1.0f) : toBase * obj.GetModelMatrix();
        glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(transform)));
        bool mirrored = glm::determinant(glm::mat3(transform)) < 0.0f;
        for (Mesh& mesh : obj.DisplayMeshes()) {
            unsigned int offset = (unsigned int)vertices.size();
            for (Vertex v : mesh.GetVertices()) {
                v.position = glm::vec3(transform * glm::vec4(v.position, 1.0f));
                glm::vec3 n = normalMatrix * v.normal;
                v.normal = glm::length(n) > 1e-20f ? glm::normalize(n) : v.normal;
                vertices.push_back(v);
            }
            const std::vector<unsigned int>& source = mesh.GetIndices();
            for (size_t k = 0; k + 2 < source.size(); k += 3) {
                indices.push_back(offset + source[k]);
                indices.push_back(offset + (mirrored ? source[k + 2] : source[k + 1]));
                indices.push_back(offset + (mirrored ? source[k + 1] : source[k + 2]));
            }
        }
    }

    for (Mesh& mesh : base.model.GetMeshes()) mesh.Destroy();
    base.model.GetMeshes().clear();
    Mesh joined;
    joined.Init(vertices, indices);
    base.model.AddMesh(joined);
    base.modifiers.clear();
    base.parts.reset();
    base.shape = -1;
    base.hole = false;

    unsigned int keepId = base.id;
    std::string keepName = base.name;
    int count = (int)selection.size();
    std::vector<int> doomed(selection.begin() + 1, selection.end());
    std::sort(doomed.begin(), doomed.end());
    for (int k = (int)doomed.size() - 1; k >= 0; k--) RemoveObject(doomed[k]);
    for (int i = 0; i < (int)objects.size(); i++) {
        if (objects[i].id == keepId) { SetSelection({ i }, i); break; }
    }
    LogSuccess("Joined " + std::to_string(count) + " objects into '" + keepName + "'.");
    return true;
}

void SceneRenderer::RenameObject(int index, const std::string& name) {
    if (index < 0 || index >= (int)objects.size()) return;
    size_t first = name.find_first_not_of(" 	");
    size_t last = name.find_last_not_of(" 	");
    if (first == std::string::npos) return;
    std::string wanted = name.substr(first, last - first + 1);
    if (wanted == objects[index].name) return;
    std::string old = objects[index].name;
    objects[index].name = "";
    objects[index].name = GenerateUniqueName(wanted);
    objects[index].model.SetName(objects[index].name);
    LogSuccess("Renamed '" + old + "' to '" + objects[index].name + "'.");
}

void SceneRenderer::FocusSelection() {
    glm::vec3 boundsMin(0.0f), boundsMax(0.0f);
    bool any = GetSelectionBounds(boundsMin, boundsMax);
    if (!any) {
        for (const SceneObject& obj : objects) {
            glm::vec3 a, b;
            if (!obj.GetWorldBounds(a, b)) continue;
            boundsMin = any ? glm::min(boundsMin, a) : a;
            boundsMax = any ? glm::max(boundsMax, b) : b;
            any = true;
        }
    }
    if (!any) {
        boundsMin = glm::vec3(-1.0f);
        boundsMax = glm::vec3(1.0f);
    }
    glm::vec3 center = (boundsMin + boundsMax) * 0.5f;
    float radius = std::max(glm::length(boundsMax - boundsMin) * 0.5f, 0.05f);
    float distance = radius / std::sin(glm::radians(camera.fov) * 0.5f) * 1.15f;
    camera.AnimateTo(center - camera.front * distance, camera.yaw, camera.pitch);
}

static void Upload(unsigned int vbo, const std::vector<float>& data, int& count) {
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, data.size() * sizeof(float), data.empty() ? nullptr : data.data(), GL_DYNAMIC_DRAW);
    count = (int)(data.size() / 4);
}

// Wireframe on the GPU
void SceneRenderer::DrawEditWireframe(const glm::mat4& view, const glm::mat4& projection) {
    int index = GetEditObjectIndex();
    if (index < 0 || index >= (int)objects.size() || !overlayProgram) return;
    unsigned int stamp = EditOverlayStamp();
    if (stamp != overlayStamp || index != overlayObject) {
        EditOverlayData data;
        EditFillOverlay(data);
        Upload(overlayPoints.vbo, data.points, overlayPoints.count);
        Upload(overlayHotPoints.vbo, data.hotPoints, overlayHotPoints.count);
        Upload(overlayLines.vbo, data.lines, overlayLines.count);
        Upload(overlayHotLines.vbo, data.hotLines, overlayHotLines.count);
        Upload(overlayFill.vbo, data.fill, overlayFill.count);
        overlaySeeThrough = data.seeThrough;
        overlayStamp = stamp;
        overlayObject = index;
    }

    glm::mat4 mvp = projection * view * objects[index].GetModelMatrix();
    glUseProgram(overlayProgram);
    glUniformMatrix4fv(glGetUniformLocation(overlayProgram, "mvp"), 1, GL_FALSE, glm::value_ptr(mvp));
    int colorsLoc = glGetUniformLocation(overlayProgram, "colors");
    int biasLoc = glGetUniformLocation(overlayProgram, "bias");
    int sizeLoc = glGetUniformLocation(overlayProgram, "pointSize");
    int roundLoc = glGetUniformLocation(overlayProgram, "roundPoints");
    const float colors[16] = {
        0.86f, 0.88f, 0.92f, 0.80f,
        1.00f, 0.67f, 0.24f, 1.00f,
        0.92f, 0.24f, 0.31f, 1.00f,
        1.00f, 0.67f, 0.24f, 0.28f,
    };
    glUniform4fv(colorsLoc, 4, colors);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_PROGRAM_POINT_SIZE);
    // Needed for round points
    glEnable(0x8861);
    glGetError();
    glDepthMask(GL_FALSE);
    int alphaLoc = glGetUniformLocation(overlayProgram, "alphaScale");
    // Faded hidden parts first
    int passes = overlaySeeThrough ? 2 : 1;
    for (int pass = 0; pass < passes; pass++) {
        bool hidden = overlaySeeThrough && pass == 0;
        glDepthFunc(hidden ? GL_GREATER : GL_LESS);
        glUniform1f(alphaLoc, hidden ? 0.3f : 1.0f);

        glUniform1f(biasLoc, 0.0004f);
        glUniform1i(roundLoc, 0);
        glBindVertexArray(overlayFill.vao);
        glDrawArrays(GL_TRIANGLES, 0, overlayFill.count);

        glUniform1f(biasLoc, 0.0008f);
        glLineWidth(1.4f);
        glBindVertexArray(overlayLines.vao);
        glDrawArrays(GL_LINES, 0, overlayLines.count);
        glLineWidth(2.6f);
        glBindVertexArray(overlayHotLines.vao);
        glDrawArrays(GL_LINES, 0, overlayHotLines.count);
        glLineWidth(1.0f);

        glUniform1f(biasLoc, 0.0012f);
        glUniform1i(roundLoc, 1);
        glUniform1f(sizeLoc, overlayPoints.count > 40000 ? 4.0f : 7.0f);
        glBindVertexArray(overlayPoints.vao);
        glDrawArrays(GL_POINTS, 0, overlayPoints.count);
        glUniform1f(sizeLoc, 8.0f);
        glBindVertexArray(overlayHotPoints.vao);
        glDrawArrays(GL_POINTS, 0, overlayHotPoints.count);
    }
    glDepthFunc(GL_LESS);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}
