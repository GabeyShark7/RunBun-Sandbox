#include <filesystem>
#include "core/Profiler.h"
#define GLM_ENABLE_EXPERIMENTAL
#include "render/SceneRenderer.h"
#include "render/Primitives.h"
#include "core/SceneState.h"
#include "ui/Console.h"
#include <glm/gtx/euler_angles.hpp>
#include <glm/gtx/matrix_decompose.hpp>
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <fstream>

static uint64_t Mix(uint64_t hash, const void* data, size_t size) {
    const unsigned char* p = (const unsigned char*)data;
    for (size_t i = 0; i < size; i++) {
        hash ^= p[i];
        hash *= 1099511628211ull;
    }
    return hash;
}

bool SceneRenderer::GetObjectRaw(int index, RawMesh& out, bool display) {
    if (index < 0 || index >= (int)objects.size()) return false;
    SceneObject& obj = objects[index];
    out = RawMesh();
    std::vector<Mesh>& meshes = display ? obj.DisplayMeshes() : obj.model.GetMeshes();
    for (Mesh& mesh : meshes) {
        RawMesh part;
        part.vertices = mesh.GetVertices();
        part.indices = mesh.GetIndices();
        AppendRawMesh(out, part, glm::mat4(1.0f));
    }
    return !out.indices.empty();
}

ModifierTargetFetch SceneRenderer::PartnerFetch(int index) {
    glm::mat4 toLocal = glm::inverse(objects[index].GetModelMatrix());
    return [this, index, toLocal](unsigned int id, RawMesh& outLocal) {
        for (int j = 0; j < (int)objects.size(); j++) {
            if (objects[j].id != id || j == index) continue;
            RawMesh raw;
            if (!GetObjectRaw(j, raw, false)) return false;
            outLocal = RawMesh();
            AppendRawMesh(outLocal, raw, toLocal * objects[j].GetModelMatrix());
            return true;
        }
        return false;
    };
}

bool SceneRenderer::ApplyModifiers(int index, size_t count) {
    if (index < 0 || index >= (int)objects.size()) return false;
    SceneObject& obj = objects[index];
    count = std::min(count, obj.modifiers.size());
    if (count == 0) return false;
    RawMesh base;
    GetObjectRaw(index, base, false);
    RawMesh result = EvaluateModifiers(base, obj.modifiers, count, PartnerFetch(index));
    for (Mesh& mesh : obj.model.GetMeshes()) mesh.Destroy();
    obj.model.GetMeshes().clear();
    Mesh mesh;
    mesh.Init(result.vertices, result.indices);
    obj.model.AddMesh(mesh);
    obj.modifiers.erase(obj.modifiers.begin(), obj.modifiers.begin() + count);
    obj.evaluatedKey = 0;
    LogSuccess("Applied " + std::to_string(count) + (count == 1 ? " mod to '" : " mods to '") + obj.name + "'.");
    return true;
}

void SceneRenderer::UpdateDisplayMeshes(int editIndex, bool interacting) {
    PROFILE("UpdateDisplayMeshes");
    std::vector<unsigned int> hidden;
    for (SceneObject& obj : objects)
        for (const Modifier& m : obj.modifiers)
            if (m.enabled && m.type == Mod_Boolean && m.hideTarget && m.target != 0) hidden.push_back(m.target);

    for (int i = 0; i < (int)objects.size(); i++) {
        SceneObject& obj = objects[i];
        obj.showBase = i == editIndex;
        obj.ghost = std::find(hidden.begin(), hidden.end(), obj.id) != hidden.end();

        bool any = false;
        for (const Modifier& m : obj.modifiers) any |= m.enabled;
        if (!any) {
            if (obj.evaluatedActive) {
                obj.evaluated.Destroy();
                obj.evaluatedActive = false;
                obj.evaluatedKey = 0;
            }
            continue;
        }

        uint64_t key = 1469598103934665603ull;
        for (Mesh& mesh : obj.model.GetMeshes()) key = Mix(key, &mesh.revision, sizeof(mesh.revision));
        for (const Modifier& m : obj.modifiers) {
            key = Mix(key, &m.type, sizeof(m.type));
            key = Mix(key, &m.enabled, sizeof(m.enabled));
            key = Mix(key, &m.axis, sizeof(m.axis));
            key = Mix(key, &m.count, sizeof(m.count));
            key = Mix(key, &m.levels, sizeof(m.levels));
            key = Mix(key, &m.operation, sizeof(m.operation));
            key = Mix(key, &m.target, sizeof(m.target));
            key = Mix(key, &m.amount, sizeof(m.amount));
            key = Mix(key, &m.angle, sizeof(m.angle));
            if (m.type != Mod_Boolean || !m.enabled) continue;
            for (SceneObject& other : objects) {
                if (other.id != m.target) continue;
                glm::mat4 relative = glm::inverse(obj.GetModelMatrix()) * other.GetModelMatrix();
                key = Mix(key, &relative, sizeof(relative));
                for (Mesh& mesh : other.model.GetMeshes()) key = Mix(key, &mesh.revision, sizeof(mesh.revision));
            }
        }
        auto install = [&](const RawMesh& result, uint64_t resultKey) {
            obj.evaluated.Destroy();
            Mesh mesh;
            mesh.Init(result.vertices, result.indices);
            obj.evaluated.AddMesh(mesh);
            obj.evaluatedActive = true;
            obj.evaluatedKey = resultKey;
        };
        if (obj.job && obj.job->result.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            uint64_t doneKey = obj.job->key;
            auto started = std::chrono::steady_clock::now();
            install(obj.job->result.get(), doneKey);
            obj.job.reset();
            obj.evaluateMs = std::max(obj.evaluateMs, std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - started).count());
        }
        if (obj.evaluatedActive && key == obj.evaluatedKey) continue;
        // One job at a time
        if (obj.job) continue;

        RawMesh base;
        GetObjectRaw(i, base, false);
        std::vector<std::pair<unsigned int, RawMesh>> partners;
        ModifierTargetFetch live = PartnerFetch(i);
        for (const Modifier& m : obj.modifiers) {
            RawMesh partner;
            if (m.enabled && m.type == Mod_Boolean && m.target != 0 && live(m.target, partner)) partners.push_back({ m.target, partner });
        }
        auto evaluate = [base, partners, modifiers = obj.modifiers]() {
            ModifierTargetFetch fetch = [&partners](unsigned int id, RawMesh& out) {
                for (const auto& p : partners) if (p.first == id) { out = p.second; return true; }
                return false;
            };
            auto started = std::chrono::steady_clock::now();
            RawMesh result = EvaluateModifiers(base, modifiers, modifiers.size(), fetch);
            (void)started;
            return result;
        };
        if (!obj.evaluatedActive || obj.evaluateMs < 2.0f) {
            auto started = std::chrono::steady_clock::now();
            RawMesh result = evaluate();
            obj.evaluateMs = std::chrono::duration<float, std::milli>(std::chrono::steady_clock::now() - started).count();
            install(result, key);
            continue;
        }
        (void)interacting;
        auto job = std::make_shared<SceneObject::EvalJob>();
        job->key = key;
        job->result = std::async(std::launch::async, evaluate);
        obj.job = job;
    }
}

static glm::vec3 EulerFromMatrix(const glm::mat3& rotation) {
    float x, y, z;
    glm::extractEulerAngleXYZ(glm::mat4(rotation), x, y, z);
    return glm::degrees(glm::vec3(x, y, z));
}

static void SetFromMatrix(SceneObject& obj, const glm::mat4& matrix) {
    glm::vec3 scale, translation, skew;
    glm::quat orientation;
    glm::vec4 perspective;
    glm::decompose(matrix, scale, orientation, translation, skew, perspective);
    obj.position = translation;
    obj.scale = scale;
    obj.rotation = EulerFromMatrix(glm::mat3_cast(orientation));
}

bool SceneRenderer::GroupSelected() {
    DropLightsFromSelection();
    if (selection.size() < 2) return false;
    glm::vec3 lo, hi;
    if (!GetSelectionBounds(lo, hi)) return false;
    glm::vec3 origin = glm::vec3((lo.x + hi.x) * 0.5f, (lo.y + hi.y) * 0.5f, lo.z);
    glm::mat4 toGroup = glm::translate(glm::mat4(1.0f), -origin);

    std::vector<int> order = selection;
    RawMesh solids, holes;
    bool anySolid = false;
    glm::vec4 color = objects[order.front()].color;
    auto parts = std::make_shared<std::vector<ObjectState>>();
    for (int index : order) {
        SceneObject& obj = objects[index];
        RawMesh raw, placed;
        GetObjectRaw(index, raw, true);
        AppendRawMesh(placed, raw, toGroup * obj.GetModelMatrix());
        if (obj.hole) {
            holes = holes.indices.empty() ? placed : CSGCombine(holes, placed, CSG_Union);
        } else {
            if (!anySolid) color = obj.color;
            solids = anySolid ? CSGCombine(solids, placed, CSG_Union) : placed;
            anySolid = true;
        }
        ObjectState part = CaptureObject(obj, nullptr);
        part.position -= origin;
        parts->push_back(part);
    }
    RawMesh result = anySolid ? (holes.indices.empty() ? solids : CSGCombine(solids, holes, CSG_Subtract)) : holes;

    SceneObject group;
    Mesh mesh;
    mesh.Init(result.vertices, result.indices);
    group.model.AddMesh(mesh);
    group.name = GenerateUniqueName("Group");
    group.model.SetName(group.name);
    group.position = origin;
    group.rotation = glm::vec3(0.0f);
    group.scale = glm::vec3(1.0f);
    group.color = color;
    group.hole = !anySolid;
    group.parts = parts;

    std::vector<int> doomed = order;
    std::sort(doomed.begin(), doomed.end());
    for (int k = (int)doomed.size() - 1; k >= 0; k--) RemoveObject(doomed[k]);
    int added = AddObject(std::move(group));
    SetSelection({ added }, added);
    LogSuccess("Grouped " + std::to_string(order.size()) + " shapes.");
    return true;
}

bool SceneRenderer::UngroupSelected() {
    std::vector<int> groups;
    for (int index : selection)
        if (objects[index].parts && !objects[index].parts->empty()) groups.push_back(index);
    if (groups.empty()) return false;

    std::vector<unsigned int> newIds;
    std::vector<int> doomed;
    for (int index : groups) {
        glm::mat4 groupMatrix = objects[index].GetModelMatrix();
        std::shared_ptr<const std::vector<ObjectState>> parts = objects[index].parts;
        for (const ObjectState& part : *parts) {
            SceneObject obj;
            ApplyObjectState(obj, part, true);
            obj.id = NewObjectId();
            obj.name = GenerateUniqueName(part.name);
            obj.model.SetName(obj.name);
            SetFromMatrix(obj, groupMatrix * obj.GetModelMatrix());
            if (!part.shapeEdited && part.shape >= 0 && !obj.model.GetMeshes().empty()) obj.shapeRevision = obj.model.GetMeshes()[0].revision;
            newIds.push_back(obj.id);
            objects.push_back(std::move(obj));
        }
        doomed.push_back(index);
    }
    std::sort(doomed.begin(), doomed.end());
    for (int k = (int)doomed.size() - 1; k >= 0; k--) RemoveObject(doomed[k]);
    std::vector<int> picked;
    for (int i = 0; i < (int)objects.size(); i++)
        if (std::find(newIds.begin(), newIds.end(), objects[i].id) != newIds.end()) picked.push_back(i);
    SetSelection(picked, picked.empty() ? -1 : picked.back());
    LogSuccess("Ungrouped into " + std::to_string(picked.size()) + " shapes.");
    return true;
}

bool SceneRenderer::SetShapeSettings(int index, int sides, float thickness) {
    if (index < 0 || index >= (int)objects.size()) return false;
    SceneObject& obj = objects[index];
    if (!obj.ShapeEditable()) return false;
    obj.shapeSides = sides;
    obj.shapeThickness = thickness;
    for (Mesh& mesh : obj.model.GetMeshes()) mesh.Destroy();
    obj.model.GetMeshes().clear();
    obj.model.AddMesh(CreatePrimitiveMesh((PrimitiveType)obj.shape, sides, thickness));
    obj.shapeRevision = obj.model.GetMeshes()[0].revision;
    return true;
}

void SceneRenderer::DropSelectedToGround() {
    for (int index : selection) {
        glm::vec3 lo, hi;
        if (objects[index].look.light >= 0) continue;
        if (objects[index].GetWorldBounds(lo, hi)) objects[index].position.z -= lo.z;
    }
}

void SceneRenderer::NudgeSelected(const glm::vec3& worldDelta) {
    for (int index : selection) objects[index].position += worldDelta;
}

void SceneRenderer::AlignSelected(int axis, int where) {
    glm::vec3 lo, hi;
    if (selection.size() < 2 || !GetSelectionBounds(lo, hi)) return;
    float target = where == 0 ? lo[axis] : (where == 2 ? hi[axis] : (lo[axis] + hi[axis]) * 0.5f);
    for (int index : selection) {
        glm::vec3 a, b;
        if (!objects[index].GetWorldBounds(a, b)) continue;
        float current = where == 0 ? a[axis] : (where == 2 ? b[axis] : (a[axis] + b[axis]) * 0.5f);
        objects[index].position[axis] += target - current;
    }
}

void SceneRenderer::MirrorSelected(int axis) {
    for (int index : selection) {
        SceneObject& obj = objects[index];
        glm::vec3 a, b;
        bool had = obj.GetWorldBounds(a, b);
        glm::vec3 before = (a + b) * 0.5f;
        glm::mat4 flip(1.0f);
        flip[axis][axis] = -1.0f;
        glm::mat4 world = obj.GetModelMatrix();
        glm::vec3 pivot = glm::vec3(world[3]);
        glm::mat4 mirrored = glm::translate(glm::mat4(1.0f), pivot) * flip * glm::translate(glm::mat4(1.0f), -pivot) * world;
        SetFromMatrix(obj, mirrored);
        if (had && obj.GetWorldBounds(a, b)) obj.position += before - (a + b) * 0.5f;
    }
}

void SceneRenderer::FocusAll() {
    std::vector<int> kept = selection;
    int active = selectedIndex;
    selection.clear();
    FocusSelection();
    selection = kept;
    selectedIndex = active;
}

int SceneRenderer::ExportSTL(const std::string& path) {
    std::vector<int> chosen = selection;
    if (chosen.empty()) for (int i = 0; i < (int)objects.size(); i++) chosen.push_back(i);
    std::vector<float> data;
    uint32_t triangles = 0;
    for (int index : chosen) {
        SceneObject& obj = objects[index];
        if (obj.hole || obj.ghost) continue;
        RawMesh raw;
        if (!GetObjectRaw(index, raw, true)) continue;
        glm::mat4 matrix = obj.GetModelMatrix();
        bool mirrored = glm::determinant(glm::mat3(matrix)) < 0.0f;
        for (size_t k = 0; k + 2 < raw.indices.size(); k += 3) {
            glm::vec3 p[3];
            for (int j = 0; j < 3; j++) p[j] = glm::vec3(matrix * glm::vec4(raw.vertices[raw.indices[k + j]].position, 1.0f)) * 1000.0f;
            if (mirrored) std::swap(p[1], p[2]);
            glm::vec3 n = glm::cross(p[1] - p[0], p[2] - p[0]);
            n = glm::length(n) > 1e-20f ? glm::normalize(n) : glm::vec3(0.0f);
            data.insert(data.end(), { n.x, n.y, n.z, p[0].x, p[0].y, p[0].z, p[1].x, p[1].y, p[1].z, p[2].x, p[2].y, p[2].z });
            triangles++;
        }
    }
    std::ofstream file(std::filesystem::u8path(path), std::ios::binary);
    if (!file) return -1;
    char header[80] = {};
    snprintf(header, sizeof(header), "RunBun Sandbox export in millimeters");
    file.write(header, 80);
    file.write((const char*)&triangles, 4);
    uint16_t attribute = 0;
    for (uint32_t t = 0; t < triangles; t++) {
        file.write((const char*)&data[t * 12], 48);
        file.write((const char*)&attribute, 2);
    }
    return file.good() ? (int)triangles : -1;
}
