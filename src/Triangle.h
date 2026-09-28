#pragma once
#include <glm/glm.hpp>

class Triangle {
public:
    //Sets up the VAO/VBO/shader once at startup and draws the triangle every frame
    void Init();
    void Draw(glm::mat4 view, glm::mat4 projection);

private:
    //The buffer object IDs and compiled shader prgram ID
    unsigned int VAO, VBO;
    unsigned int shaderProgram;
};