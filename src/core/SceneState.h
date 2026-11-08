#pragma once
#include "render/Mesh.h"
#include "render/Modifiers.h"
#include "core/ObjectLook.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

class SceneRenderer;

struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
};

struct TextureState {
    std::string name;
    std::string path;
    bool enabled = false;
    glm::vec2 tiling = glm::vec2(1.0f);
    glm::vec2 offset = glm::vec2(0.0f);
    float rotation = 0.0f;
    bool clamp = false;
};

struct ObjectState {
    unsigned int id = 0;
    std::string name;
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    glm::vec4 color = glm::vec4(1.0f);
    std::vector<TextureState> textures;
    std::vector<std::shared_ptr<const MeshData>> meshes;
    std::vector<unsigned int> revisions;
    bool hole = false;
    int shape = -1;
    int shapeSides = 40;
    float shapeThickness = 0.25f;
    bool shapeEdited = false;
    std::vector<Modifier> modifiers;
    ObjectLook look;
    std::shared_ptr<const std::vector<ObjectState>> parts;
    bool keepMeshes = false;
    bool keepParts = false;
};

struct SceneState {
    std::vector<ObjectState> objects;
    unsigned int serial = 0;
    std::vector<unsigned int> selectedIds;
    unsigned int activeId = 0;
    std::string label;
};

struct SceneObject;
ObjectState CaptureObject(SceneObject& obj, const ObjectState* before);
void ApplyObjectState(SceneObject& obj, const ObjectState& o, bool rebuildMeshes);

// Reuses unchanged meshes
SceneState CaptureScene(SceneRenderer& scene, const SceneState* previous);
bool SceneMatches(SceneRenderer& scene, const SceneState& state);
void RestoreScene(SceneRenderer& scene, const SceneState& state);

std::vector<uint8_t> SerializeScene(const SceneState& state);
std::vector<uint8_t> EncodeObjectState(const ObjectState& o, bool withMeshes, bool withParts);
bool DecodeObjectState(const uint8_t* data, size_t size, ObjectState& out);
bool DeserializeScene(const uint8_t* data, size_t size, SceneState& out);

void HistoryReset(SceneRenderer& scene);
// Only when not dragging
void HistoryTrack(SceneRenderer& scene);
// Remote edits skip undo
void HistoryPatchObject(unsigned int id, const ObjectState* now, const ObjectState* before);
bool HistoryUndo(SceneRenderer& scene);
bool HistoryRedo(SceneRenderer& scene);
bool HistoryCanUndo();
bool HistoryCanRedo();
bool HistoryIsDirty();
void HistoryMarkSaved();
