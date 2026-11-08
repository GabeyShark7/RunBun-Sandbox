#include "core/Profiler.h"
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
#include "ui/EditWindow.h"
#include "ui/RoomWindow.h"
#include "ui/RenderExport.h"
#include "ui/RoomSync.h"
#include "ui/UpdateWindow.h"
#include "ui/HelpWindow.h"
#include "ui/SceneCommands.h"
#include "ui/Workspaces.h"
#include "ui/MaterialWindow.h"
#include "ui/ShapesWindow.h"
#include "ui/ModsWindow.h"
#include "ui/EditMode.h"
#include "ui/SceneGizmo.h"
#include "core/Updater.h"
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
    ImFontConfig fontConfig;
    fontConfig.OversampleH = 3;
    fontConfig.OversampleV = 2;
    fontConfig.PixelSnapH = true;
    io.Fonts->AddFontFromFileTTF("fonts/NunitoSans-Bold.ttf", 18.0f, &fontConfig);
    float contentScaleX = 1.0f, contentScaleY = 1.0f;
    glfwGetWindowContentScale(sandbox_window, &contentScaleX, &contentScaleY);
    ImGui::GetStyle().FontScaleDpi = contentScaleX;

    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    RegisterUISettingsHandler();

    ImGui_ImplGlfw_InitForOpenGL(sandbox_window, true);
    ImGui_ImplOpenGL3_Init("#version 130");
    InitSceneCommands(sandbox_window, sceneRenderer);

    // Dev builds skip this
    if (GetCurrentBuild() > 0 && HasUpdateToken()) CheckForUpdates();

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(sandbox_window)) {
        ProfileDump(glfwGetTime());
        PROFILE("Frame");
        glfwPollEvents();
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;
        UpdateInput(sandbox_window, deltaTime);
        sceneRenderer.GetCamera().Update(deltaTime);

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        TakeSceneTab();
        ImGui::NewFrame();

        UISettings& uiSettings = GetUISettings();
        ApplyGlassStyle(uiSettings);
        DrawThemeBackdrop(uiSettings);

        UpdateSceneCommands(sceneRenderer);
        UpdateWorkspaceKeys(sceneRenderer);
        sceneRenderer.backgroundColor = glm::vec3(uiSettings.sceneBackground.x, uiSettings.sceneBackground.y, uiSettings.sceneBackground.z);
        sceneRenderer.highlightColor = glm::vec3(uiSettings.accent.x, uiSettings.accent.y, uiSettings.accent.z);
        sceneRenderer.backgroundBottom = glm::vec3(uiSettings.sceneBackgroundBottom.x, uiSettings.sceneBackgroundBottom.y, uiSettings.sceneBackgroundBottom.z);
        sceneRenderer.backgroundMode = uiSettings.backgroundMode;
        sceneRenderer.backgroundImage = uiSettings.backgroundImage;
        sceneRenderer.viewStyle = uiSettings.viewStyle;
        sceneRenderer.shadows = uiSettings.shadows;
        sceneRenderer.showLights = uiSettings.showLights;
        DrawMenuBar(sandbox_window, sceneRenderer);
        DrawMainDockSpace(uiSettings);

        int workspace = ActiveWorkspace();
        bool layoutTab = workspace == Workspace_Layout;
        ImVec2 scenePanelSize = ImVec2(0, 0);
        if (uiSettings.showScene || !layoutTab) {
            PreparePanel("Scene");
            scenePanelSize = DrawSceneWindow(sceneRenderer, uiSettings, layoutTab ? &uiSettings.showScene : nullptr);
        }
        sceneRenderer.showGrid = uiSettings.showGrid;
        Camera& viewCamera = sceneRenderer.GetCamera();
        viewCamera.fov = uiSettings.viewFov;
        viewCamera.nearPlane = uiSettings.nearClipMm / 1000.0f;
        viewCamera.farPlane = uiSettings.farClipMm / 1000.0f;
        sceneRenderer.UpdateDisplayMeshes(GetEditObjectIndex(), IsSceneTransformActive() || ImGui::IsAnyItemActive());
        sceneRenderer.Render(scenePanelSize);

        if (layoutTab) {
            if (uiSettings.showObjects) {
                PreparePanel("Objects");
                DrawObjectsWindow(sceneRenderer, &uiSettings.showObjects);
            }
            if (uiSettings.showProperties) {
                PreparePanel("Properties");
                DrawPropertiesWindow(sceneRenderer, &uiSettings.showProperties);
            }
            if (uiSettings.showShapes) {
                PreparePanelNear("Shapes", "Edit");
                DrawShapesWindow(sceneRenderer, &uiSettings.showShapes);
            }
            if (uiSettings.showMods && uiSettings.workMode != WorkMode_Tinker) {
                PreparePanelNear("Object Mods", "Properties");
                DrawModsWindow(sceneRenderer, &uiSettings.showMods);
            }
            if (uiSettings.showConsole) {
                PreparePanel("Console");
                DrawConsoleWindow(&uiSettings.showConsole);
            }
            if (uiSettings.showMessages) {
                PreparePanel("Messages");
                DrawMessagesWindow(&uiSettings.showMessages);
            }
            if (uiSettings.showEdit && uiSettings.workMode != WorkMode_Tinker) {
                ImGuiViewport* mainViewport = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(ImVec2(mainViewport->WorkPos.x + mainViewport->WorkSize.x * 0.015f, mainViewport->WorkPos.y + mainViewport->WorkSize.y * 0.36f), ImGuiCond_FirstUseEver);
                ImGui::SetNextWindowSize(ImVec2(mainViewport->WorkSize.x * 0.18f, mainViewport->WorkSize.y * 0.61f), ImGuiCond_FirstUseEver);
                PreparePanel("Edit");
                DrawEditWindow(sceneRenderer, &uiSettings.showEdit);
            }
        } else if (workspace == Workspace_Edit) {
            PreparePanel("Edit");
            DrawEditWindow(sceneRenderer, nullptr);
        } else if (workspace == Workspace_Texturing) {
            PreparePanel("Objects");
            DrawObjectsWindow(sceneRenderer, nullptr);
            PreparePanel("UV Tools");
            DrawUVToolsWindow(sceneRenderer, nullptr);
            PreparePanel("Material");
            DrawMaterialWindow(sceneRenderer, nullptr);
            PreparePanel("UV");
            DrawUVWindow(sceneRenderer, nullptr);
        }
        FinishPanelLayout();

        if (uiSettings.showPreferences) DrawPreferencesWindow(uiSettings);
        UpdateRoom(sceneRenderer);
        RoomSyncUpdate(sceneRenderer);
        DrawRoomWindows();
        DrawRenderExportWindows(sceneRenderer, scenePanelSize);
        DrawUpdateWindow();
        DrawHelpWindow();
        DrawSceneCommandPopups(sceneRenderer);
        if (UpdateWantsExit()) glfwSetWindowShouldClose(sandbox_window, GLFW_TRUE);

        int windowWidth, windowHeight;
        glfwGetFramebufferSize(sandbox_window, &windowWidth, &windowHeight);
        glViewport(0, 0, windowWidth, windowHeight);

        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        {
            PROFILE("ImGuiRender");
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        }
        {
            PROFILE("Swap");
            glfwSwapBuffers(sandbox_window);
        }
    }

    ShutdownRoom();
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwTerminate();
    return 0;
}