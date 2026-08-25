#pragma once
#include <GLFW/glfw3.h>
#include "Camera.h"

void InitInput(GLFWwindow* window, Camera* camera); 
void UpdateInput(GLFWwindow* window, float deltaTime);