#include "core/Profiler.h"
#include <cctype>
#include <set>
#include "ui/Console.h"
#include "core/SceneState.h"
#include "render/SceneRenderer.h"
#include "render/TextureLoader.h"
#include <algorithm>
#include <cstring>
#include <map>

static bool SameVec(const glm::vec3& a, const glm::vec3& b) { return a.x == b.x && a.y == b.y && a.z == b.z; }
static bool SameVec(const glm::vec4& a, const glm::vec4& b) { return a.x == b.x && a.y == b.y && a.z == b.z && a.w == b.w; }

ObjectState CaptureObject(SceneObject& obj, const ObjectState* before) {
    ObjectState o;
    o.id = obj.id;
    o.name = obj.name;
    o.position = obj.position;
    o.rotation = obj.rotation;
    o.scale = obj.scale;
    o.color = obj.color;
    o.hole = obj.hole;
    o.shape = obj.shape;
    o.shapeSides = obj.shapeSides;
    o.shapeThickness = obj.shapeThickness;
    o.shapeEdited = obj.shape >= 0 && !obj.ShapeEditable();
    o.modifiers = obj.modifiers;
    o.look = obj.look;
    o.parts = obj.parts;
    for (const TextureInfo& tex : obj.model.GetTextures()) {
        TextureState state;
        state.name = tex.name;
        state.path = tex.path;
        state.enabled = tex.enabled;
        state.tiling = tex.tiling;
        state.offset = tex.offset;
        state.rotation = tex.rotation;
        state.clamp = tex.clamp;
        o.textures.push_back(state);
    }
    auto& meshes = obj.model.GetMeshes();
    for (size_t m = 0; m < meshes.size(); m++) {
        o.revisions.push_back(meshes[m].revision);
        if (before && m < before->meshes.size() && before->revisions[m] == meshes[m].revision) {
            o.meshes.push_back(before->meshes[m]);
            continue;
        }
        auto data = std::make_shared<MeshData>();
        data->vertices = meshes[m].GetVertices();
        data->indices = meshes[m].GetIndices();
        o.meshes.push_back(data);
    }
    return o;
}

SceneState CaptureScene(SceneRenderer& scene, const SceneState* previous) {
    std::map<unsigned int, const ObjectState*> old;
    if (previous) for (const ObjectState& o : previous->objects) old[o.id] = &o;

    SceneState state;
    for (SceneObject& obj : scene.GetObjects()) {
        auto found = old.find(obj.id);
        state.objects.push_back(CaptureObject(obj, found != old.end() ? found->second : nullptr));
    }
    auto& objects = scene.GetObjects();
    for (int i : scene.GetSelection()) state.selectedIds.push_back(objects[i].id);
    if (scene.selectedIndex >= 0) state.activeId = objects[scene.selectedIndex].id;
    return state;
}

bool SceneMatches(SceneRenderer& scene, const SceneState& state) {
    auto& objects = scene.GetObjects();
    if (objects.size() != state.objects.size()) return false;
    for (size_t i = 0; i < objects.size(); i++) {
        SceneObject& obj = objects[i];
        const ObjectState& o = state.objects[i];
        if (obj.id != o.id || obj.name != o.name) return false;
        if (!SameVec(obj.position, o.position) || !SameVec(obj.rotation, o.rotation) || !SameVec(obj.scale, o.scale)) return false;
        if (!SameVec(obj.color, o.color)) return false;
        if (obj.hole != o.hole || obj.shape != o.shape || obj.shapeSides != o.shapeSides || obj.shapeThickness != o.shapeThickness) return false;
        if (!(obj.modifiers == o.modifiers) || obj.parts != o.parts || obj.look != o.look) return false;

        auto& textures = obj.model.GetTextures();
        if (textures.size() != o.textures.size()) return false;
        for (size_t t = 0; t < textures.size(); t++) {
            const TextureInfo& a = textures[t];
            const TextureState& b = o.textures[t];
            if (a.path != b.path || a.enabled != b.enabled || a.clamp != b.clamp || a.rotation != b.rotation) return false;
            if (a.tiling != b.tiling || a.offset != b.offset) return false;
        }

        auto& meshes = obj.model.GetMeshes();
        if (meshes.size() != o.revisions.size()) return false;
        for (size_t m = 0; m < meshes.size(); m++)
            if (meshes[m].revision != o.revisions[m]) return false;
    }
    return true;
}

static void BuildMeshes(SceneObject& obj, const ObjectState& o) {
    if (o.look.light >= 0) return;
    for (size_t m = 0; m < o.meshes.size(); m++) {
        Mesh mesh;
        mesh.Init(o.meshes[m]->vertices, o.meshes[m]->indices);
        // Keep snapshot ids
        if (m < o.revisions.size() && o.revisions[m] != 0) mesh.revision = o.revisions[m];
        obj.model.AddMesh(mesh);
    }
}

void ApplyObjectState(SceneObject& obj, const ObjectState& o, bool rebuildMeshes) {
    if (rebuildMeshes) {
        for (Mesh& mesh : obj.model.GetMeshes()) mesh.Destroy();
        obj.model.GetMeshes().clear();
        BuildMeshes(obj, o);
    }
    obj.id = o.id;
    obj.name = o.name;
    obj.model.SetName(o.name);
    obj.position = o.position;
    obj.rotation = o.rotation;
    obj.scale = o.scale;
    obj.color = o.color;
    obj.hole = o.hole;
    obj.shape = o.shape;
    obj.shapeSides = o.shapeSides;
    obj.shapeThickness = o.shapeThickness;
    obj.shapeRevision = (o.shape >= 0 && !o.shapeEdited && !obj.model.GetMeshes().empty()) ? obj.model.GetMeshes()[0].revision : 0;
    obj.modifiers = o.modifiers;
    obj.look = o.look;
    obj.parts = o.parts;
    obj.model.GetTextures().clear();
    for (const TextureState& tex : o.textures) {
        unsigned int id = tex.path.empty() ? 0 : AcquireTexture(tex.path);
        if (id == 0) continue;
        obj.model.AddTexture(id, tex.name, tex.path, tex.enabled);
        TextureInfo& added = obj.model.GetTextures().back();
        added.tiling = tex.tiling;
        added.offset = tex.offset;
        added.rotation = tex.rotation;
        added.clamp = tex.clamp;
    }
}

void RestoreScene(SceneRenderer& scene, const SceneState& state) {
    auto& objects = scene.GetObjects();
    const std::vector<unsigned int>& selectedIds = state.selectedIds;
    unsigned int activeId = state.activeId;

    std::vector<SceneObject> live = std::move(objects);
    objects.clear();
    std::vector<bool> used(live.size(), false);

    for (const ObjectState& o : state.objects) {
        int match = -1;
        for (size_t i = 0; i < live.size(); i++)
            if (!used[i] && live[i].id == o.id) { match = (int)i; break; }

        SceneObject obj;
        bool sameMeshes = false;
        if (match >= 0) {
            used[match] = true;
            obj = std::move(live[match]);
            auto& meshes = obj.model.GetMeshes();
            sameMeshes = meshes.size() == o.revisions.size();
            for (size_t m = 0; sameMeshes && m < meshes.size(); m++) sameMeshes = meshes[m].revision == o.revisions[m];
        }
        ApplyObjectState(obj, o, !sameMeshes);
        scene.NoteObjectId(o.id);
        objects.push_back(std::move(obj));
    }

    for (size_t i = 0; i < live.size(); i++)
        if (!used[i]) {
            live[i].model.Destroy();
            live[i].evaluated.Destroy();
        }

    std::vector<int> selection;
    int active = -1;
    for (int i = 0; i < (int)objects.size(); i++) {
        if (std::find(selectedIds.begin(), selectedIds.end(), objects[i].id) != selectedIds.end()) selection.push_back(i);
        if (objects[i].id == activeId) active = i;
    }
    scene.SetSelection(selection, active);
    scene.hoveredIndex = -1;
}

struct Writer {
    std::vector<uint8_t> bytes;
    void Raw(const void* data, size_t size) {
        const uint8_t* p = (const uint8_t*)data;
        bytes.insert(bytes.end(), p, p + size);
    }
    void U32(uint32_t v) { Raw(&v, 4); }
    void F32(float v) { Raw(&v, 4); }
    void Str(const std::string& s) { U32((uint32_t)s.size()); Raw(s.data(), s.size()); }
    void Vec(const float* v, int n) { for (int i = 0; i < n; i++) F32(v[i]); }
};

struct Reader {
    const uint8_t* data;
    size_t size;
    size_t at = 0;
    bool ok = true;
    bool Raw(void* out, size_t n) {
        if (!ok || n > size - at) { ok = false; return false; }
        memcpy(out, data + at, n);
        at += n;
        return true;
    }
    uint32_t U32() { uint32_t v = 0; Raw(&v, 4); return v; }
    float F32() { float v = 0; Raw(&v, 4); return v; }
    std::string Str() {
        uint32_t n = U32();
        if (!ok || n > size - at) { ok = false; return ""; }
        std::string s((const char*)data + at, n);
        at += n;
        return s;
    }
    void Vec(float* v, int n) { for (int i = 0; i < n; i++) v[i] = F32(); }
};

static const char sceneMagic[4] = { 'R', 'B', 'S', 'N' };
static const uint32_t sceneVersion = 5;

static void WriteObject(Writer& w, const ObjectState& o, bool withMeshes = true, bool withParts = true) {
    w.U32(o.id);
    w.Str(o.name);
    w.Vec(&o.position.x, 3);
    w.Vec(&o.rotation.x, 3);
    w.Vec(&o.scale.x, 3);
    w.Vec(&o.color.x, 4);
    w.U32((uint32_t)o.textures.size());
    for (const TextureState& tex : o.textures) {
        w.Str(tex.name);
        w.Str(tex.path);
        w.U32(tex.enabled ? 1 : 0);
        w.Vec(&tex.tiling.x, 2);
        w.Vec(&tex.offset.x, 2);
        w.Vec(&tex.rotation, 1);
        w.U32(tex.clamp ? 1 : 0);
    }
    // All ones means unchanged
    w.U32(withMeshes ? (uint32_t)o.meshes.size() : 0xFFFFFFFFu);
    for (const auto& mesh : withMeshes ? o.meshes : std::vector<std::shared_ptr<const MeshData>>()) {
        w.U32((uint32_t)mesh->vertices.size());
        for (const Vertex& v : mesh->vertices) {
            w.Vec(&v.position.x, 3);
            w.Vec(&v.normal.x, 3);
            w.Vec(&v.texCoords.x, 2);
        }
        w.U32((uint32_t)mesh->indices.size());
        w.Raw(mesh->indices.data(), mesh->indices.size() * 4);
    }
    w.U32(o.hole ? 1 : 0);
    w.U32((uint32_t)(o.shape + 1));
    w.U32((uint32_t)o.shapeSides);
    w.F32(o.shapeThickness);
    w.U32(o.shapeEdited ? 1 : 0);
    w.U32((uint32_t)o.modifiers.size());
    for (const Modifier& m : o.modifiers) {
        w.U32((uint32_t)m.type);
        w.U32(m.enabled ? 1 : 0);
        w.U32((uint32_t)m.axis);
        w.U32((uint32_t)m.count);
        w.U32((uint32_t)m.levels);
        w.U32((uint32_t)m.operation);
        w.U32(m.target);
        w.U32(m.hideTarget ? 1 : 0);
        w.F32(m.amount);
        w.F32(m.angle);
    }
    w.F32(o.look.roughness);
    w.F32(o.look.metallic);
    w.F32(o.look.glow);
    w.U32((uint32_t)(o.look.light + 1));
    w.F32(o.look.power);
    w.F32(o.look.spotAngle);
    w.U32(o.look.castShadows ? 1 : 0);
    w.U32((uint32_t)o.look.noise);
    w.F32(o.look.noiseScale);
    w.F32(o.look.noiseDetail);
    w.F32(o.look.noiseContrast);
    w.F32(o.look.noiseBump);
    w.F32(o.look.noiseRough);
    w.F32(o.look.noiseSeed);
    w.Vec(o.look.noiseColor, 3);
    uint32_t partCount = o.parts ? (uint32_t)o.parts->size() : 0;
    if (!withParts) {
        w.U32(0xFFFFFFFFu);
        return;
    }
    w.U32(partCount);
    for (uint32_t p = 0; p < partCount; p++) WriteObject(w, (*o.parts)[p]);
}

std::vector<uint8_t> SerializeScene(const SceneState& state) {
    Writer w;
    w.Raw(sceneMagic, 4);
    w.U32(sceneVersion);
    w.U32((uint32_t)state.objects.size());
    for (const ObjectState& o : state.objects) WriteObject(w, o);
    return w.bytes;
}

static bool ReadObject(Reader& r, uint32_t version, ObjectState& o, int depth) {
    if (depth > 32) return false;
    o.id = r.U32();
    o.name = r.Str();
    r.Vec(&o.position.x, 3);
    r.Vec(&o.rotation.x, 3);
    r.Vec(&o.scale.x, 3);
    r.Vec(&o.color.x, 4);
    uint32_t textureCount = r.U32();
    for (uint32_t t = 0; t < textureCount && r.ok; t++) {
        TextureState tex;
        tex.name = r.Str();
        tex.path = r.Str();
        tex.enabled = r.U32() != 0;
        if (version >= 2) {
            r.Vec(&tex.tiling.x, 2);
            r.Vec(&tex.offset.x, 2);
            r.Vec(&tex.rotation, 1);
            tex.clamp = r.U32() != 0;
        }
        o.textures.push_back(tex);
    }
    uint32_t meshCount = r.U32();
    o.keepMeshes = meshCount == 0xFFFFFFFFu;
    if (o.keepMeshes) meshCount = 0;
    for (uint32_t m = 0; m < meshCount && r.ok; m++) {
        auto mesh = std::make_shared<MeshData>();
        uint32_t vertexCount = r.U32();
        // Each vertex is 32 bytes
        if (!r.ok || (size_t)vertexCount > (r.size - r.at) / 32) return false;
        mesh->vertices.resize(vertexCount);
        for (Vertex& v : mesh->vertices) {
            r.Vec(&v.position.x, 3);
            r.Vec(&v.normal.x, 3);
            r.Vec(&v.texCoords.x, 2);
        }
        uint32_t indexCount = r.U32();
        if (!r.ok || (size_t)indexCount > (r.size - r.at) / 4) return false;
        mesh->indices.resize(indexCount);
        r.Raw(mesh->indices.data(), (size_t)indexCount * 4);
        for (unsigned int index : mesh->indices)
            if (index >= vertexCount) return false;
        o.meshes.push_back(mesh);
        o.revisions.push_back(0);
    }
    if (version >= 3) {
        o.hole = r.U32() != 0;
        o.shape = (int)r.U32() - 1;
        o.shapeSides = (int)r.U32();
        o.shapeThickness = r.F32();
        o.shapeEdited = r.U32() != 0;
        uint32_t modifierCount = r.U32();
        if (modifierCount > 256) return false;
        for (uint32_t k = 0; k < modifierCount && r.ok; k++) {
            Modifier m;
            m.type = (int)r.U32();
            m.enabled = r.U32() != 0;
            m.axis = (int)r.U32();
            m.count = (int)r.U32();
            m.levels = (int)r.U32();
            m.operation = (int)r.U32();
            m.target = r.U32();
            m.hideTarget = r.U32() != 0;
            m.amount = r.F32();
            m.angle = r.F32();
            if (m.type < 0 || m.type >= Mod_Count) return false;
            o.modifiers.push_back(m);
        }
        if (version >= 4) {
            o.look.roughness = r.F32();
            o.look.metallic = r.F32();
            o.look.glow = r.F32();
            o.look.light = (int)r.U32() - 1;
            o.look.power = r.F32();
            o.look.spotAngle = r.F32();
            o.look.castShadows = r.U32() != 0;
            if (o.look.light < Light_None || o.look.light >= Light_Count) return false;
        }
        if (version >= 5) {
            o.look.noise = (int)r.U32();
            o.look.noiseScale = r.F32();
            o.look.noiseDetail = r.F32();
            o.look.noiseContrast = r.F32();
            o.look.noiseBump = r.F32();
            o.look.noiseRough = r.F32();
            o.look.noiseSeed = r.F32();
            r.Vec(o.look.noiseColor, 3);
            if (o.look.noise < 0 || o.look.noise >= Noise_Count) return false;
        }
        uint32_t partCount = r.U32();
        o.keepParts = partCount == 0xFFFFFFFFu;
        if (o.keepParts) partCount = 0;
        if (partCount > 100000) return false;
        if (partCount > 0) {
            auto parts = std::make_shared<std::vector<ObjectState>>(partCount);
            for (uint32_t p = 0; p < partCount && r.ok; p++)
                if (!ReadObject(r, version, (*parts)[p], depth + 1)) return false;
            o.parts = parts;
        }
    } else {
        o.shapeEdited = true;
    }
    return r.ok;
}

std::vector<uint8_t> EncodeObjectState(const ObjectState& o, bool withMeshes, bool withParts) {
    Writer w;
    WriteObject(w, o, withMeshes, withParts);
    return w.bytes;
}

bool DecodeObjectState(const uint8_t* data, size_t size, ObjectState& out) {
    Reader r{ data, size };
    return ReadObject(r, sceneVersion, out, 0);
}

bool DeserializeScene(const uint8_t* data, size_t size, SceneState& out) {
    Reader r{ data, size };
    char magic[4];
    if (!r.Raw(magic, 4) || memcmp(magic, sceneMagic, 4) != 0) return false;
    uint32_t version = r.U32();
    if (version < 1 || version > sceneVersion) return false;

    SceneState state;
    uint32_t objectCount = r.U32();
    for (uint32_t i = 0; i < objectCount && r.ok; i++) {
        ObjectState o;
        if (!ReadObject(r, version, o, 0)) return false;
        state.objects.push_back(std::move(o));
    }
    if (!r.ok) return false;
    out = std::move(state);
    return true;
}

static SceneState committed;
static std::vector<SceneState> undoStack;
static std::vector<SceneState> redoStack;
static unsigned int serialCounter = 0;
static unsigned int savedSerial = 0;
static const size_t maxUndo = 200;

void HistoryReset(SceneRenderer& scene) {
    committed = CaptureScene(scene, nullptr);
    committed.serial = ++serialCounter;
    savedSerial = committed.serial;
    undoStack.clear();
    redoStack.clear();
}

static unsigned int loggedAt = 0;

static std::string Quoted(const std::string& name) { return "'" + name + "'"; }

static std::string Many(int count, const std::string& one) {
    return count == 1 ? one : std::to_string(count) + " objects";
}

// Plain words for a step
static std::string DescribeStep(const SceneState& before, const SceneState& after) {
    std::map<unsigned int, const ObjectState*> old;
    for (const ObjectState& o : before.objects) old[o.id] = &o;
    std::vector<std::string> added, removed;
    std::map<std::string, std::vector<std::string>> changes;
    std::vector<std::string> order;
    auto note = [&](const std::string& what, const std::string& name) {
        if (!changes.count(what)) order.push_back(what);
        changes[what].push_back(name);
    };
    std::set<unsigned int> seen;
    for (const ObjectState& o : after.objects) {
        seen.insert(o.id);
        auto it = old.find(o.id);
        if (it == old.end()) { added.push_back(o.name); continue; }
        const ObjectState& b = *it->second;
        if (b.name != o.name) note("Renamed", b.name);
        if (b.position != o.position) note("Moved", o.name);
        if (b.rotation != o.rotation) note("Rotated", o.name);
        if (b.scale != o.scale) note("Resized", o.name);
        if (b.color != o.color) note("Recolored", o.name);
        if (b.hole != o.hole) note(o.hole ? "Made a hole of" : "Made solid", o.name);
        if (b.revisions != o.revisions && !(b.modifiers != o.modifiers)) note("Edited the shape of", o.name);
        if (b.modifiers.size() < o.modifiers.size()) note("Added a mod to", o.name);
        else if (b.modifiers.size() > o.modifiers.size()) note("Removed a mod from", o.name);
        else if (b.modifiers != o.modifiers) note("Changed mods on", o.name);
        if (b.shapeSides != o.shapeSides || b.shapeThickness != o.shapeThickness) note("Changed the settings of", o.name);
        if (b.look != o.look) note(o.look.light >= 0 ? "Changed the light" : "Changed the look of", o.name);
        if (b.textures.size() != o.textures.size()) note("Changed the images on", o.name);
        else {
            for (size_t t = 0; t < o.textures.size(); t++) {
                const TextureState& x = b.textures[t];
                const TextureState& y = o.textures[t];
                if (x.path != y.path || x.enabled != y.enabled || x.tiling != y.tiling || x.offset != y.offset || x.rotation != y.rotation || x.clamp != y.clamp) {
                    note("Changed the images on", o.name);
                    break;
                }
            }
        }
    }
    for (const ObjectState& o : before.objects) if (!seen.count(o.id)) removed.push_back(o.name);

    std::vector<std::string> parts;
    if (!added.empty()) parts.push_back("Added " + (added.size() == 1 ? Quoted(added[0]) : Many((int)added.size(), "")));
    if (!removed.empty()) parts.push_back("Deleted " + (removed.size() == 1 ? Quoted(removed[0]) : Many((int)removed.size(), "")));
    for (const std::string& what : order) {
        const std::vector<std::string>& names = changes[what];
        parts.push_back(what + " " + (names.size() == 1 ? Quoted(names[0]) : Many((int)names.size(), "")));
    }
    if (parts.empty()) return "";
    std::string text = parts[0];
    for (size_t i = 1; i < parts.size() && i < 3; i++) {
        std::string next = parts[i];
        next[0] = (char)std::tolower((unsigned char)next[0]);
        text += i + 1 == parts.size() || i == 2 ? " and " + next : ", " + next;
    }
    if (parts.size() > 3) text += " and more";
    return text + ".";
}

void HistoryTrack(SceneRenderer& scene) {
    PROFILE("HistoryTrack");
    if (SceneMatches(scene, committed)) {
        // Undo brings this back
        auto& objects = scene.GetObjects();
        committed.selectedIds.clear();
        for (int i : scene.GetSelection()) committed.selectedIds.push_back(objects[i].id);
        committed.activeId = scene.selectedIndex >= 0 ? objects[scene.selectedIndex].id : 0;
        loggedAt = LogSuccessCount();
        return;
    }
    SceneState next = CaptureScene(scene, &committed);
    next.serial = ++serialCounter;
    next.label = DescribeStep(committed, next);
    // Skip if already told
    if (LogSuccessCount() == loggedAt && !next.label.empty()) LogSuccess(next.label);
    loggedAt = LogSuccessCount();
    undoStack.push_back(std::move(committed));
    if (undoStack.size() > maxUndo) undoStack.erase(undoStack.begin());
    committed = std::move(next);
    redoStack.clear();
}

bool HistoryUndo(SceneRenderer& scene) {
    HistoryTrack(scene);
    if (undoStack.empty()) {
        LogWarning("Nothing to undo.");
        return false;
    }
    LogMessage("Undid: " + (committed.label.empty() ? std::string("a change") : committed.label));
    redoStack.push_back(std::move(committed));
    committed = std::move(undoStack.back());
    undoStack.pop_back();
    RestoreScene(scene, committed);
    return true;
}

bool HistoryRedo(SceneRenderer& scene) {
    HistoryTrack(scene);
    if (redoStack.empty()) {
        LogWarning("Nothing to redo.");
        return false;
    }
    LogMessage("Redid: " + (redoStack.back().label.empty() ? std::string("a change") : redoStack.back().label));
    undoStack.push_back(std::move(committed));
    committed = std::move(redoStack.back());
    redoStack.pop_back();
    RestoreScene(scene, committed);
    return true;
}

bool HistoryCanUndo() { return !undoStack.empty(); }
bool HistoryCanRedo() { return !redoStack.empty(); }
bool HistoryIsDirty() { return committed.serial != savedSerial; }
void HistoryMarkSaved() { savedSerial = committed.serial; }

// Only what they changed
static void PatchFields(ObjectState& into, const ObjectState& before, const ObjectState& now) {
    if (before.name != now.name) into.name = now.name;
    if (before.position != now.position) into.position = now.position;
    if (before.rotation != now.rotation) into.rotation = now.rotation;
    if (before.scale != now.scale) into.scale = now.scale;
    if (before.color != now.color) into.color = now.color;
    if (before.hole != now.hole) into.hole = now.hole;
    if (before.shape != now.shape || before.shapeSides != now.shapeSides || before.shapeThickness != now.shapeThickness || before.shapeEdited != now.shapeEdited) {
        into.shape = now.shape;
        into.shapeSides = now.shapeSides;
        into.shapeThickness = now.shapeThickness;
        into.shapeEdited = now.shapeEdited;
    }
    if (!(before.modifiers == now.modifiers)) into.modifiers = now.modifiers;
    if (before.look != now.look) into.look = now.look;
    if (before.parts != now.parts) into.parts = now.parts;
    if (before.revisions != now.revisions) {
        into.meshes = now.meshes;
        into.revisions = now.revisions;
    }
    bool texturesChanged = before.textures.size() != now.textures.size();
    for (size_t t = 0; !texturesChanged && t < now.textures.size(); t++) {
        const TextureState& a = before.textures[t];
        const TextureState& b = now.textures[t];
        texturesChanged = a.path != b.path || a.enabled != b.enabled || a.tiling != b.tiling || a.offset != b.offset || a.rotation != b.rotation || a.clamp != b.clamp;
    }
    if (texturesChanged) into.textures = now.textures;
}

static void PatchState(SceneState& state, unsigned int id, const ObjectState* now, const ObjectState* before) {
    for (size_t i = 0; i < state.objects.size(); i++) {
        if (state.objects[i].id != id) continue;
        if (!now) state.objects.erase(state.objects.begin() + i);
        else if (before) PatchFields(state.objects[i], *before, *now);
        else state.objects[i] = *now;
        return;
    }
    if (now) state.objects.push_back(*now);
}

void HistoryPatchObject(unsigned int id, const ObjectState* now, const ObjectState* before) {
    PatchState(committed, id, now, before);
    for (SceneState& state : undoStack) PatchState(state, id, now, before);
    for (SceneState& state : redoStack) PatchState(state, id, now, before);
}
