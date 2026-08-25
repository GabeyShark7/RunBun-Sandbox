#pragma once
#include "Model.h"
#include <glm/glm.hpp>
#include <string>

struct SceneObject {
    Model model;
    std::string name;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(0.02f);
};