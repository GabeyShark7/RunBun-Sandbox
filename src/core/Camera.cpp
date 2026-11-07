#include "core/Camera.h"
#include <glm/gtc/matrix_transform.hpp>

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

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    glm::vec3 newFront;
    newFront.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    newFront.y = sin(glm::radians(pitch));
    newFront.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(newFront);
}

void Camera::Reset() {
    position = glm::vec3(0.0f, 1.5f, 4.0f);
    front = glm::vec3(0.0f, -0.342f, -0.940f);
    up = glm::vec3(0.0f, 1.0f, 0.0f);
    yaw = -90.0f;
    pitch = -20.0f;
    firstMouse = true;
}