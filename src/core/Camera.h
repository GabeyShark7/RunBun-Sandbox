#pragma once
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

class Camera {
public:
    glm::vec3 position = glm::vec3(0.0f, 1.5f, 4.0f);
    glm::vec3 front = glm::vec3(0.0f, -0.342f, -0.940f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);

    float yaw = -90.0f;
    float pitch = -20.0f;

    float fov = 55.0f;
    float nearPlane = 0.05f;
    float farPlane = 1000.0f;

    float lastX = 400.0f;
    float lastY = 300.0f;
    bool firstMouse = true;

    glm::mat4 GetViewMatrix();
    glm::mat4 GetProjectionMatrix(int width, int height);

    void ProcessKeyboard(GLFWwindow* window, float deltaTime);
    void ProcessMouse(float xpos, float ypos);
    void Reset(); // back to the starting position and direction
};