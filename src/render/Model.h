#pragma once
#include "render/Mesh.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <string>
#include <vector>
#include <glm/glm.hpp>

struct TextureInfo {
    unsigned int id;
    std::string name;
    bool enabled;
    std::string path;
    glm::vec2 tiling = glm::vec2(1.0f);
    glm::vec2 offset = glm::vec2(0.0f);
    float rotation = 0.0f;
    bool clamp = false;
};

class Model {
public:
    bool Load(const std::string& path);
    void Draw(unsigned int shaderProgram, glm::mat4 view, glm::mat4 projection, glm::mat4 modelMatrix, std::vector<Mesh>* meshesOverride = nullptr);

    void AddTexture(unsigned int id, const std::string& name, const std::string& path, bool enabled);
    void RemoveTexture(int index);
    void ToggleTexture(int index, bool enabled);
    std::vector<TextureInfo>& GetTextures();

    void SetName(const std::string& n);
    void AddMesh(const Mesh& mesh);
    bool GetBounds(glm::vec3& outMin, glm::vec3& outMax) const;
    std::vector<Mesh>& GetMeshes() { return meshes; }
    const std::vector<Mesh>& GetMeshesConst() const { return meshes; }
    void Destroy(); // frees all meshes and textures on the GPU

private:
    std::vector<Mesh> meshes;

    std::vector<TextureInfo> textures;
    bool conflictLogged = false;
    std::string modelName;

    void ProcessNode(aiNode* node, const aiScene* scene, glm::mat4 parentTransform);
    Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene, glm::mat4 transform);
    glm::mat4 AiMatrixToGlm(const aiMatrix4x4& from);
};