#pragma once
#include "render/Model.h"
#include "render/Modifiers.h"
#include "core/ObjectLook.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdint>
#include <future>
#include <memory>
#include <string>
#include <vector>

struct ObjectState;

struct SceneObject {
    Model model;
    std::string name;
    unsigned int id = 0;
    glm::vec4 color = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    glm::vec3 position = glm::vec3(0.0f);
    glm::vec3 rotation = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(0.02f);
    ObjectLook look;

    bool hole = false;
    int shape = -1;
    int shapeSides = 40;
    float shapeThickness = 0.25f;
    unsigned int shapeRevision = 0;
    std::vector<Modifier> modifiers;
    std::shared_ptr<const std::vector<ObjectState>> parts;

    // Built from modifiers
    Model evaluated;
    bool evaluatedActive = false;
    uint64_t evaluatedKey = 0;
    bool showBase = false;
    bool ghost = false;
    float evaluateMs = 0.0f;
    struct EvalJob {
        std::future<RawMesh> result;
        uint64_t key = 0;
    };
    std::shared_ptr<EvalJob> job;

    bool UsesEvaluated() const { return evaluatedActive && !showBase; }
    std::vector<Mesh>& DisplayMeshes() { return UsesEvaluated() ? evaluated.GetMeshes() : model.GetMeshes(); }
    bool ShapeEditable() const {
        return shape >= 0 && model.GetMeshesConst().size() == 1 && model.GetMeshesConst()[0].revision == shapeRevision;
    }

    glm::mat4 GetModelMatrix() const {
        glm::mat4 matrix = glm::translate(glm::mat4(1.0f), position);
        matrix = glm::rotate(matrix, glm::radians(rotation.x), glm::vec3(1, 0, 0));
        matrix = glm::rotate(matrix, glm::radians(rotation.y), glm::vec3(0, 1, 0));
        matrix = glm::rotate(matrix, glm::radians(rotation.z), glm::vec3(0, 0, 1));
        return glm::scale(matrix, scale);
    }

    bool GetLocalBounds(glm::vec3& outMin, glm::vec3& outMax) const {
        if (look.light >= 0) {
            // Lights are only an icon
            glm::vec3 half = glm::vec3(0.1f) / glm::max(glm::abs(scale), glm::vec3(1e-4f));
            outMin = -half;
            outMax = half;
            return true;
        }
        return UsesEvaluated() ? evaluated.GetBounds(outMin, outMax) : model.GetBounds(outMin, outMax);
    }

    // World space box
    bool GetWorldBounds(glm::vec3& outMin, glm::vec3& outMax) const {
        glm::vec3 localMin, localMax;
        if (!GetLocalBounds(localMin, localMax)) return false;
        glm::mat4 matrix = GetModelMatrix();
        outMin = glm::vec3(1e30f);
        outMax = glm::vec3(-1e30f);
        for (int corner = 0; corner < 8; corner++) {
            glm::vec3 local((corner & 1) ? localMax.x : localMin.x, (corner & 2) ? localMax.y : localMin.y, (corner & 4) ? localMax.z : localMin.z);
            glm::vec3 world = glm::vec3(matrix * glm::vec4(local, 1.0f));
            outMin = glm::min(outMin, world);
            outMax = glm::max(outMax, world);
        }
        return true;
    }
};
