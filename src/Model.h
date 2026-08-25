#pragma once
#include "Mesh.h"
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
};

class Model {
public:
    void Load(const std::string& path);
    void Draw(glm::mat4 view, glm::mat4 projection, glm::mat4 modelMatrix);

    void AddTexture(unsigned int id, const std::string& name);
    void RemoveTexture(int index);
    void ToggleTexture(int index, bool enabled);
    std::vector<TextureInfo>& GetTextures();

    void SetName(const std::string& n);

private:
    std::vector<Mesh> meshes;
    unsigned int shaderProgram;
    bool shaderLoaded = false;

    std::vector<TextureInfo> textures;
    bool conflictLogged = false;
    std::string modelName;

    void ProcessNode(aiNode* node, const aiScene* scene, glm::mat4 parentTransform);
    Mesh ProcessMesh(aiMesh* mesh, const aiScene* scene, glm::mat4 transform);
    glm::mat4 AiMatrixToGlm(const aiMatrix4x4& from);
};