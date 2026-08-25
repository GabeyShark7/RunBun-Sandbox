#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>

struct Vertex {
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
};

class Mesh {
public:
    void Init(std::vector<Vertex> vertices, std::vector<unsigned int> indices);
    void Draw();

private:
    unsigned int VAO, VBO, EBO;
    int indexCount;
};