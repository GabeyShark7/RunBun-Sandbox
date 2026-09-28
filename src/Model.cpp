// THIS IS ASSIMP MAGIC. BEWARE!!!

#include "Model.h"
#include "Shader.h"
#include "Console.h"
#include <iostream>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

glm::mat4 Model::AiMatrixToGlm(const aiMatrix4x4& from) {
    glm::mat4 to;
    to[0][0] = from.a1; to[1][0] = from.a2; to[2][0] = from.a3; to[3][0] = from.a4;
    to[0][1] = from.b1; to[1][1] = from.b2; to[2][1] = from.b3; to[3][1] = from.b4;
    to[0][2] = from.c1; to[1][2] = from.c2; to[2][2] = from.c3; to[3][2] = from.c4;
    to[0][3] = from.d1; to[1][3] = from.d2; to[2][3] = from.d3; to[3][3] = from.d4;
    return to;
}

void Model::Load(const std::string& path) {
    Assimp::Importer importer;

    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_FlipUVs);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        std::cout << "Assimp error: " << importer.GetErrorString() << "\n";
        return;
    }

    if (!shaderLoaded) {
        shaderProgram = LoadShader("shaders/basic.vert", "shaders/basic.frag");
        shaderLoaded = true;
    }

    ProcessNode(scene->mRootNode, scene, glm::mat4(1.0f));
}

void Model::ProcessNode(aiNode* node, const aiScene* scene, glm::mat4 parentTransform) {
    glm::mat4 nodeTransform = AiMatrixToGlm(node->mTransformation);
    glm::mat4 globalTransform = parentTransform * nodeTransform;

    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        meshes.push_back(ProcessMesh(mesh, scene, globalTransform));
    }

    for (unsigned int i = 0; i < node->mNumChildren; i++) {
        ProcessNode(node->mChildren[i], scene, globalTransform);
    }
}

Mesh Model::ProcessMesh(aiMesh* mesh, const aiScene* scene, glm::mat4 transform) {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(transform)));

    for (unsigned int i = 0; i < mesh->mNumVertices; i++) {
        Vertex vertex;

        glm::vec4 pos = transform * glm::vec4(mesh->mVertices[i].x, mesh->mVertices[i].y, mesh->mVertices[i].z, 1.0f);
        vertex.position = glm::vec3(pos);

        if (mesh->HasNormals()) {
            glm::vec3 normal = glm::vec3(mesh->mNormals[i].x, mesh->mNormals[i].y, mesh->mNormals[i].z);
            vertex.normal = glm::normalize(normalMatrix * normal);
        } else {
            vertex.normal = glm::vec3(0.0f, 1.0f, 0.0f);
        }

        if (mesh->mTextureCoords[0])
            vertex.texCoords = glm::vec2(mesh->mTextureCoords[0][i].x, mesh->mTextureCoords[0][i].y);
        else
            vertex.texCoords = glm::vec2(0.0f, 0.0f);

        vertices.push_back(vertex);
    }

    for (unsigned int i = 0; i < mesh->mNumFaces; i++) {
        aiFace face = mesh->mFaces[i];
        for (unsigned int j = 0; j < face.mNumIndices; j++)
            indices.push_back(face.mIndices[j]);
    }

    Mesh result;
    result.Init(vertices, indices);
    return result;
}

void Model::AddTexture(unsigned int id, const std::string& name) {
    bool enabledByDefault = textures.empty();
    textures.push_back({ id, name, enabledByDefault });
}

void Model::RemoveTexture(int index) {
    if (index < 0 || index >= (int)textures.size()) return;
    textures.erase(textures.begin() + index);
}

void Model::ToggleTexture(int index, bool enabled) {
    if (index < 0 || index >= (int)textures.size()) return;
    textures[index].enabled = enabled;
}

std::vector<TextureInfo>& Model::GetTextures() {
    return textures;
}

void Model::SetName(const std::string& n) {
    modelName = n;
}

void Model::Draw(glm::mat4 view, glm::mat4 projection, glm::mat4 modelMatrix) {
    glUseProgram(shaderProgram);

    int enabledCount = 0;
    int activeIndex = -1;
    for (int i = 0; i < (int)textures.size(); i++) {
        if (textures[i].enabled) {
            enabledCount++;
            if (activeIndex == -1) activeIndex = i;
        }
    }

    bool conflict = enabledCount > 1;

    if (conflict && !conflictLogged) {
        LogMessage("Texture conflict on '" + modelName + "': more than one texture is enabled. Disable extras to fix.");
        conflictLogged = true;
    } else if (!conflict) {
        conflictLogged = false;
    }

    int viewLoc = glGetUniformLocation(shaderProgram, "view");
    int projLoc = glGetUniformLocation(shaderProgram, "projection");
    int modelLoc = glGetUniformLocation(shaderProgram, "model");
    int hasTexLoc = glGetUniformLocation(shaderProgram, "hasTexture");
    int texLoc = glGetUniformLocation(shaderProgram, "texture1");
    int conflictLoc = glGetUniformLocation(shaderProgram, "conflictMode");

    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));
    glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(modelMatrix));
    glUniform1i(conflictLoc, conflict ? 1 : 0);

    if (!conflict && enabledCount == 1) {
        glUniform1i(hasTexLoc, 1);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textures[activeIndex].id);
        glUniform1i(texLoc, 0);
    } else {
        glUniform1i(hasTexLoc, 0);
    }

    for (auto& mesh : meshes) {
        mesh.Draw();
    }
}