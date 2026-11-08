#pragma once
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>

class Camera {
public:
    glm::vec3 position = glm::vec3(0.0f, -4.0f, 1.5f);
    glm::vec3 front = glm::vec3(0.0f, 0.940f, -0.342f);
    glm::vec3 up = glm::vec3(0.0f, 0.0f, 1.0f);

    float yaw = 90.0f;
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

    void SetAngles(float newYaw, float newPitch);
    void AnimateTo(const glm::vec3& targetPosition, float targetYaw, float targetPitch);
    // Orbit around a point
    void ViewFrom(const glm::vec3& pivot, float targetYaw, float targetPitch);
    void Update(float deltaTime);
    bool IsAnimating() const { return animTime >= 0.0f; }

private:
    float animTime = -1.0f;
    glm::vec3 animFromPos, animToPos;
    float animFromYaw = 0.0f, animToYaw = 0.0f;
    float animFromPitch = 0.0f, animToPitch = 0.0f;
};