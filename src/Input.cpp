#include "Input.h"
#include "SceneWindow.h" // needed for IsSceneHovered()

static Camera* activeCamera = nullptr;
static bool rightMouseHeld = false;

static void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    if (activeCamera && rightMouseHeld) activeCamera->ProcessMouse((float)xpos, (float)ypos);
}

void InitInput(GLFWwindow* window, Camera* camera) {
    activeCamera = camera;
    glfwSetCursorPosCallback(window, mouse_callback);
}

void UpdateInput(GLFWwindow* window, float deltaTime) {
    bool isRightMouseDown = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

    if (isRightMouseDown && !rightMouseHeld && IsSceneHovered()) {
        // only START look-mode if right-click began while hovering the Scene panel
        rightMouseHeld = true;
        activeCamera->firstMouse = true;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    if (!isRightMouseDown && rightMouseHeld) {
        rightMouseHeld = false;
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }

    if (activeCamera && rightMouseHeld) activeCamera->ProcessKeyboard(window, deltaTime);
    // WASD now only processed while right-click look-mode is active
}