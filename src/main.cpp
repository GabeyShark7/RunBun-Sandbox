#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "MenuBar.h"
#include "SceneWindow.h"
#include "ObjectsWindow.h"
#include "PropertiesWindow.h"
#include "Console.h"
#include "Shader.h"
#include "Input.h"
#include "SceneRenderer.h"

using namespace std;

int main() {

    if (!glfwInit()) {
        std::cout << "GLFW failed to start.\n";
        return -1;
    }

    GLFWwindow* sandbox_window = glfwCreateWindow(800, 600, "RunBun Sandbox", NULL, NULL);
    glfwMaximizeWindow(sandbox_window);

    if (!sandbox_window) {
        std::cout << "RunBun Sandbox window failed to start\n";
        glfwTerminate();
        return -1;
    }

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

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding = 8.0f;
    style.FrameRounding = 6.0f;
    style.PopupRounding = 8.0f;
    style.ScrollbarRounding = 8.0f;

    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Header] = ImVec4(0.4f, 0.05f, 0.15f, 1.0f);
    colors[ImGuiCol_HeaderHovered] = ImVec4(0.5f, 0.08f, 0.2f, 1.0f);
    colors[ImGuiCol_HeaderActive] = ImVec4(0.6f, 0.1f, 0.25f, 1.0f);

    colors[ImGuiCol_FrameBg] = ImVec4(0.25f, 0.04f, 0.1f, 1.0f);
    colors[ImGuiCol_FrameBgHovered] = ImVec4(0.35f, 0.06f, 0.15f, 1.0f);
    colors[ImGuiCol_FrameBgActive] = ImVec4(0.45f, 0.08f, 0.2f, 1.0f);

    colors[ImGuiCol_Button] = ImVec4(0.35f, 0.05f, 0.13f, 1.0f);
    colors[ImGuiCol_ButtonHovered] = ImVec4(0.45f, 0.08f, 0.18f, 1.0f);
    colors[ImGuiCol_ButtonActive] = ImVec4(0.55f, 0.1f, 0.22f, 1.0f);

    colors[ImGuiCol_Tab] = ImVec4(0.25f, 0.04f, 0.1f, 1.0f);
    colors[ImGuiCol_TabHovered] = ImVec4(0.45f, 0.08f, 0.18f, 1.0f);
    colors[ImGuiCol_TabActive] = ImVec4(0.55f, 0.1f, 0.22f, 1.0f);
    colors[ImGuiCol_TabUnfocused] = ImVec4(0.2f, 0.03f, 0.08f, 1.0f);
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.35f, 0.06f, 0.13f, 1.0f);

    colors[ImGuiCol_CheckMark] = ImVec4(0.8f, 0.15f, 0.35f, 1.0f);
    colors[ImGuiCol_SliderGrab] = ImVec4(0.6f, 0.1f, 0.25f, 1.0f);
    colors[ImGuiCol_SliderGrabActive] = ImVec4(0.7f, 0.12f, 0.3f, 1.0f);

    colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.4f, 0.06f, 0.15f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.5f, 0.08f, 0.2f, 1.0f);
    colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.6f, 0.1f, 0.25f, 1.0f);

    colors[ImGuiCol_ResizeGrip] = ImVec4(0.4f, 0.06f, 0.15f, 1.0f);
    colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.5f, 0.08f, 0.2f, 1.0f);
    colors[ImGuiCol_ResizeGripActive] = ImVec4(0.6f, 0.1f, 0.25f, 1.0f);

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

        DrawMenuBar(sandbox_window, sceneRenderer);

        ImVec2 scenePanelSize = DrawSceneWindow(sceneRenderer.GetTexture());
        sceneRenderer.Render(scenePanelSize);

        DrawObjectsWindow(sceneRenderer);
        DrawPropertiesWindow(sceneRenderer);
        DrawConsoleWindow();

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