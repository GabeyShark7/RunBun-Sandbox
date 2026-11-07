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
    void Destroy();
    void UpdateVertices(const std::vector<Vertex>& vertices);
    const std::vector<Vertex>& GetVertices() const { return cpuVertices; }
    const std::vector<unsigned int>& GetIndices() const { return cpuIndices; }

    glm::vec3 boundsMin = glm::vec3(0.0f);
    glm::vec3 boundsMax = glm::vec3(0.0f);

private:
    unsigned int VAO, VBO, EBO;
    int indexCount;
    std::vector<Vertex> cpuVertices;
    std::vector<unsigned int> cpuIndices;

    void UpdateBounds();
};