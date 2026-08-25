#pragma once
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

class Camera {
public:
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 3.0f);
    glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    float yaw = -90.0f;
    float pitch = 0.0f;

    float lastX = 400.0f;
    float lastY = 300.0f;
    bool firstMouse = true;

    glm::mat4 GetViewMatrix();
    glm::mat4 GetProjectionMatrix(int width, int height);

    void ProcessKeyboard(GLFWwindow* window, float deltaTime);
    void ProcessMouse(float xpos, float ypos);
};