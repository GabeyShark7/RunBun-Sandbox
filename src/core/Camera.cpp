#include "core/Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

glm::mat4 Camera::GetViewMatrix() {
    return glm::lookAt(position, position + front, up);
}

glm::mat4 Camera::GetProjectionMatrix(int width, int height) {
    float aspect = height > 0 ? (float)width / (float)height : 1.0f;
    float nearClip = nearPlane > 0.0001f ? nearPlane : 0.0001f;
    float farClip = farPlane > nearClip * 1.01f ? farPlane : nearClip * 1.01f;
    return glm::perspective(glm::radians(fov), aspect, nearClip, farClip);
}

void Camera::ProcessKeyboard(GLFWwindow* window, float deltaTime) {
    if (animTime >= 0.0f) return;
    float speed = 2.5f * deltaTime;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        speed = speed * 1.5f;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        position += front * speed;

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        position -= front * speed;

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        position -= glm::normalize(glm::cross(front, up)) * speed;

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        position += glm::normalize(glm::cross(front, up)) * speed;
}

void Camera::ProcessMouse(float xpos, float ypos) {
    if (firstMouse) {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    float xoffset = xpos - lastX;
    float yoffset = lastY - ypos;
    lastX = xpos;
    lastY = ypos;

    float sensitivity = 0.1f;
    xoffset *= sensitivity;
    yoffset *= sensitivity;

    animTime = -1.0f;
    SetAngles(yaw - xoffset, pitch + yoffset);
}

void Camera::SetAngles(float newYaw, float newPitch) {
    yaw = newYaw;
    pitch = newPitch;
    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 newFront;
    // Z is up
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.z = sin(glm::radians(pitch));
    front = glm::normalize(newFront);
}

void Camera::AnimateTo(const glm::vec3& targetPosition, float targetYaw, float targetPitch) {
    animFromPos = position;
    animToPos = targetPosition;
    animFromYaw = yaw;
    // Turn the short way
    float delta = fmodf(targetYaw - yaw, 360.0f);
    if (delta > 180.0f) delta -= 360.0f;
    if (delta < -180.0f) delta += 360.0f;
    animToYaw = yaw + delta;
    animFromPitch = pitch;
    animToPitch = targetPitch;
    animTime = 0.0f;
}

void Camera::ViewFrom(const glm::vec3& pivot, float targetYaw, float targetPitch) {
    float distance = glm::length(pivot - position);
    if (distance < 0.5f) distance = 5.0f;
    float clampedPitch = targetPitch > 89.0f ? 89.0f : (targetPitch < -89.0f ? -89.0f : targetPitch);
    glm::vec3 dir;
    dir.x = cos(glm::radians(targetYaw)) * cos(glm::radians(clampedPitch));
    dir.y = sin(glm::radians(targetYaw)) * cos(glm::radians(clampedPitch));
    dir.z = sin(glm::radians(clampedPitch));
    AnimateTo(pivot - glm::normalize(dir) * distance, targetYaw, clampedPitch);
}

void Camera::Update(float deltaTime) {
    if (animTime < 0.0f) return;
    const float duration = 0.3f;
    animTime += deltaTime;
    float t = animTime >= duration ? 1.0f : animTime / duration;
    float eased = t * t * (3.0f - 2.0f * t);
    position = animFromPos + (animToPos - animFromPos) * eased;
    SetAngles(animFromYaw + (animToYaw - animFromYaw) * eased, animFromPitch + (animToPitch - animFromPitch) * eased);
    if (t >= 1.0f) animTime = -1.0f;
}

void Camera::Reset() {
    position = glm::vec3(0.0f, -4.0f, 1.5f);
    front = glm::vec3(0.0f, 0.940f, -0.342f);
    up = glm::vec3(0.0f, 0.0f, 1.0f);
    yaw = 90.0f;
    pitch = -20.0f;
    firstMouse = true;
    animTime = -1.0f;
}