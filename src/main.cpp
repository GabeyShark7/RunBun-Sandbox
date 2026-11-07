#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "ui/MenuBar.h"
#include "ui/SceneWindow.h"
#include "ui/ObjectsWindow.h"
#include "ui/PropertiesWindow.h"
#include "ui/Console.h"
#include "render/Shader.h"
#include "core/Input.h"
#include "render/SceneRenderer.h"
#include "core/UISettings.h"
#include "ui/GlassTheme.h"
#include "ui/DockLayout.h"
#include "ui/PreferencesWindow.h"
#include <filesystem>
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

using namespace std;

int main() {

    // Run from the exe's folder so shaders/, fonts/ and icons_made/ are always found
    wchar_t exePath[MAX_PATH];
    GetModuleFileNameW(NULL, exePath, MAX_PATH);
    std::filesystem::current_path(std::filesystem::path(exePath).parent_path());

    if (!glfwInit()) {
        std::cout << "GLFW failed to start.\n";
        return -1;
    }

    GLFWwindow* sandbox_window = glfwCreateWindow(800, 600, "RunBun Sandbox", NULL, NULL);

    if (!sandbox_window) {
        std::cout << "RunBun Sandbox window failed to start\n";
        glfwTerminate();
        return -1;
    }

    glfwMaximizeWindow(sandbox_window);

    glfwMakeContextCurrent(sandbox_window);

    if (!gladLoadGL()) {
        std::cout << "Failed to initialize GLAD\n";
        return -1;
    }

    SceneRenderer sceneRenderer;
    sceneRenderer.Init();

    InitInput(sandbox_window, &sceneRenderer.GetCamera());

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->AddFontFromFileTTF("fonts/NunitoSans.ttf", 18.0f);

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    RegisterUISettingsHandler();

    ImGui_ImplGlfw_InitForOpenGL(sandbox_window, true);
    ImGui_ImplOpenGL3_Init("#version 130");

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(sandbox_window)) {
        glfwPollEvents();
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        UpdateInput(sandbox_window, deltaTime);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        UISettings& uiSettings = GetUISettings();
        ApplyGlassStyle(uiSettings);
        DrawThemeBackdrop(uiSettings);

        DrawMenuBar(sandbox_window, sceneRenderer);
        DrawMainDockSpace(uiSettings);

        ImVec2 scenePanelSize = ImVec2(0, 0);
        if (uiSettings.showScene) {
            PreparePanel("Scene");
            scenePanelSize = DrawSceneWindow(sceneRenderer.GetTexture(), sceneRenderer.GetCamera(), &uiSettings.showScene);
        }
        sceneRenderer.Render(scenePanelSize);

        if (uiSettings.showObjects) {
            PreparePanel("Objects");
            DrawObjectsWindow(sceneRenderer, &uiSettings.showObjects);
        }
        if (uiSettings.showProperties) {
            PreparePanel("Properties");
            DrawPropertiesWindow(sceneRenderer, &uiSettings.showProperties);
        }
        if (uiSettings.showConsole) {
            PreparePanel("Console");
            DrawConsoleWindow(&uiSettings.showConsole);
        }
        FinishPanelLayout();

        if (uiSettings.showPreferences) DrawPreferencesWindow(uiSettings);

        int windowWidth, windowHeight;
        glfwGetFramebufferSize(sandbox_window, &windowWidth, &windowHeight);
        glViewport(0, 0, windowWidth, windowHeight);

        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(sandbox_window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}