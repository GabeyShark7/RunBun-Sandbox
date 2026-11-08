#include "ui/RoomSync.h"
#include "ui/EditMode.h"
#include "ui/SceneGizmo.h"
#include "ui/Console.h"
#include "core/SceneState.h"
#include "render/SceneRenderer.h"
#include "imgui/imgui.h"
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <zlib.h>

enum SyncType { Sync_Snapshot = 1, Sync_Batch = 2 };
enum OpKind { Op_Upsert = 1, Op_Remove = 2, Op_Presence = 3, Op_Image = 4, Op_Permission = 5, Op_Patch = 6 };

static const uint32_t syncVersion = 5;
static const size_t maxImage = 8 * 1024 * 1024;

static RoomSender sender;
static SceneRenderer* scene = nullptr;
static bool active = false, host = false, ready = false, guestsCanEdit = true;
static RoomSettings settings;
static std::string myName;
static SceneState lastSent;
static double lastSend = 0.0;
static std::map<unsigned int, double> lastMeshSend;
static std::vector<PeerPresence> peers;
static std::vector<uint8_t> lastPresence;
static double lastPresenceTime = 0.0;
static std::map<std::string, std::string> hashToLocal;
static std::map<std::string, std::string> pathToHash;
static std::set<std::string> knownHashes;
static unsigned int nextIndex = 2;
static float progress = -1.0f;
static std::map<unsigned int, uint32_t> versions;
static bool locksDirty = true;
static unsigned int lockStamp = 0;
static int lockObject = -1;

struct Out {
    std::vector<uint8_t> bytes;
    void Raw(const void* data, size_t size) { const uint8_t* p = (const uint8_t*)data; bytes.insert(bytes.end(), p, p + size); }
    void U32(uint32_t v) { Raw(&v, 4); }
    void U8(uint8_t v) { bytes.push_back(v); }
    void Str(const std::string& s) { U32((uint32_t)s.size()); Raw(s.data(), s.size()); }
};

struct In {
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
    uint8_t U8() { uint8_t v = 0; Raw(&v, 1); return v; }
    std::string Str() {
        uint32_t n = U32();
        if (!ok || n > size - at) { ok = false; return ""; }
        std::string s((const char*)data + at, n);
        at += n;
        return s;
    }
};

ImU32 RoomSyncColorFor(const std::string& name) {
    uint32_t hash = 2166136261u;
    for (char c : name) { hash ^= (uint8_t)c; hash *= 16777619u; }
    float h = (hash % 360) / 360.0f, r, g, b;
    ImGui::ColorConvertHSVtoRGB(h, 0.65f, 1.0f, r, g, b);
    return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), 255);
}

void RoomSyncSetSender(const RoomSender& s) { sender = s; }
bool RoomSyncActive() { return active; }
bool RoomSyncReady() { return active && ready; }
float RoomSyncProgressFraction() { return progress; }
bool RoomSyncIsHost() { return active && host; }
bool RoomSyncGuestsCanEdit() { return guestsCanEdit; }
const RoomSettings& RoomSyncSettings() { return settings; }
const std::vector<PeerPresence>& RoomSyncPeers() { return peers; }

static void Header(Out& o, uint32_t type) {
    o.Raw("RBSY", 4);
    o.U32(syncVersion);
    o.U32(type);
}

// Squeeze before sending
static std::vector<uint8_t> Pack(const std::vector<uint8_t>& data) {
    if (data.size() < 256) return data;
    uLongf size = compressBound((uLong)data.size());
    std::vector<uint8_t> out(8 + size);
    memcpy(out.data(), "RBSZ", 4);
    uint32_t raw = (uint32_t)data.size();
    memcpy(out.data() + 4, &raw, 4);
    if (compress2(out.data() + 8, &size, data.data(), (uLong)data.size(), 1) != Z_OK || size + 8 >= data.size()) return data;
    out.resize(8 + size);
    return out;
}

static bool Unpack(const std::vector<uint8_t>& packed, std::vector<uint8_t>& out) {
    if (packed.size() < 8 || memcmp(packed.data(), "RBSZ", 4) != 0) {
        out = packed;
        return true;
    }
    uint32_t raw;
    memcpy(&raw, packed.data() + 4, 4);
    if (raw > 1024u * 1024u * 1024u) return false;
    out.resize(raw);
    uLongf size = raw;
    return uncompress(out.data(), &size, packed.data() + 8, (uLong)(packed.size() - 8)) == Z_OK && size == raw;
}

static void Send(const std::vector<uint8_t>& data, const char* to, const char* skip, const char* from) {
    if (sender && !data.empty()) sender(Pack(data), to, skip, from);
}

static std::string Extension(const std::string& path) {
    size_t dot = path.find_last_of('.');
    size_t slash = path.find_last_of("/\\");
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return "";
    std::string ext = path.substr(dot);
    return ext.size() <= 6 ? ext : "";
}

static bool ReadFile(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream file(std::filesystem::u8path(path), std::ios::binary | std::ios::ate);
    if (!file) return false;
    std::streamsize size = file.tellg();
    if (size <= 0 || (size_t)size > maxImage) return false;
    out.resize((size_t)size);
    file.seekg(0);
    return (bool)file.read((char*)out.data(), size);
}

// Image name by content
static std::string HashOf(const std::string& path) {
    auto found = pathToHash.find(path);
    if (found != pathToHash.end()) return found->second;
    std::vector<uint8_t> bytes;
    if (!ReadFile(path, bytes)) { pathToHash[path] = ""; return ""; }
    uint64_t hash = 1469598103934665603ull;
    for (uint8_t b : bytes) { hash ^= b; hash *= 1099511628211ull; }
    char text[32];
    snprintf(text, sizeof(text), "%016llx", (unsigned long long)hash);
    std::string key = std::string(text) + Extension(path);
    pathToHash[path] = key;
    hashToLocal[key] = path;
    return key;
}

static void AddImageOp(Out& ops, uint32_t& count, const std::string& key) {
    if (knownHashes.count(key)) return;
    auto local = hashToLocal.find(key);
    std::vector<uint8_t> bytes;
    if (local == hashToLocal.end() || !ReadFile(local->second, bytes)) return;
    Out op;
    op.Str(key);
    op.U32((uint32_t)bytes.size());
    op.Raw(bytes.data(), bytes.size());
    ops.U8(Op_Image);
    ops.U32((uint32_t)op.bytes.size());
    ops.Raw(op.bytes.data(), op.bytes.size());
    count++;
    knownHashes.insert(key);
}

// Paths become room names
static ObjectState ForRoom(const ObjectState& o, Out* ops, uint32_t* count) {
    ObjectState copy = o;
    for (TextureState& tex : copy.textures) {
        if (tex.path.rfind("room:", 0) == 0) continue;
        std::string key = HashOf(tex.path);
        if (key.empty()) { tex.path.clear(); continue; }
        if (ops) AddImageOp(*ops, *count, key);
        tex.path = "room:" + key;
    }
    if (copy.parts) {
        auto parts = std::make_shared<std::vector<ObjectState>>();
        for (const ObjectState& part : *copy.parts) parts->push_back(ForRoom(part, ops, count));
        copy.parts = parts;
    }
    return copy;
}

static void FromRoom(ObjectState& o) {
    for (TextureState& tex : o.textures) {
        if (tex.path.rfind("room:", 0) != 0) continue;
        auto found = hashToLocal.find(tex.path.substr(5));
        tex.path = found != hashToLocal.end() ? found->second : "";
    }
    for (unsigned int& revision : o.revisions) revision = 0;
    if (o.parts) {
        auto parts = std::make_shared<std::vector<ObjectState>>(*o.parts);
        for (ObjectState& part : *parts) FromRoom(part);
        o.parts = parts;
    }
}

static bool SameObject(const ObjectState& a, const ObjectState& b) {
    if (a.name != b.name || a.position != b.position || a.rotation != b.rotation || a.scale != b.scale || a.color != b.color) return false;
    if (a.hole != b.hole || a.shape != b.shape || a.shapeSides != b.shapeSides || a.shapeThickness != b.shapeThickness) return false;
    if (!(a.modifiers == b.modifiers) || a.parts != b.parts || a.revisions != b.revisions || a.look != b.look) return false;
    if (a.textures.size() != b.textures.size()) return false;
    for (size_t t = 0; t < a.textures.size(); t++) {
        const TextureState& x = a.textures[t];
        const TextureState& y = b.textures[t];
        if (x.path != y.path || x.enabled != y.enabled || x.tiling != y.tiling || x.offset != y.offset || x.rotation != y.rotation || x.clamp != y.clamp) return false;
    }
    return true;
}

static int FindState(const SceneState& state, unsigned int id) {
    for (int i = 0; i < (int)state.objects.size(); i++) if (state.objects[i].id == id) return i;
    return -1;
}

static int FindObject(unsigned int id) {
    auto& objects = scene->GetObjects();
    for (int i = 0; i < (int)objects.size(); i++) if (objects[i].id == id) return i;
    return -1;
}

// Remote change is not ours
static void Remember(unsigned int id) {
    int index = FindObject(id);
    int known = FindState(lastSent, id);
    if (index < 0) {
        if (known >= 0) lastSent.objects.erase(lastSent.objects.begin() + known);
        HistoryPatchObject(id, nullptr, nullptr);
        return;
    }
    ObjectState now = CaptureObject(scene->GetObjects()[index], known >= 0 ? &lastSent.objects[known] : nullptr);
    ObjectState before = known >= 0 ? lastSent.objects[known] : now;
    if (known >= 0) lastSent.objects[known] = now;
    else lastSent.objects.push_back(now);
    HistoryPatchObject(id, &now, known >= 0 ? &before : nullptr);
}

static void PutOp(Out& ops, uint32_t& count, uint8_t kind, const std::vector<uint8_t>& payload) {
    ops.U8(kind);
    ops.U32((uint32_t)payload.size());
    ops.Raw(payload.data(), payload.size());
    count++;
}

static void UpsertOp(Out& ops, uint32_t& count, const ObjectState& room, bool meshes, bool parts, uint32_t base, uint32_t version, bool force) {
    Out op;
    op.U32(base);
    op.U32(version);
    op.U8(force ? 1 : 0);
    std::vector<uint8_t> bytes = EncodeObjectState(room, meshes, parts);
    op.Raw(bytes.data(), bytes.size());
    PutOp(ops, count, Op_Upsert, op.bytes);
}

// False when out of date
static bool ApplyUpsert(ObjectState o, uint32_t base, uint32_t version, bool force, const std::string& from) {
    FromRoom(o);
    int index = FindObject(o.id);
    auto& objects = scene->GetObjects();
    if (index < 0) {
        if (o.keepMeshes) return true;
        SceneObject obj;
        ApplyObjectState(obj, o, true);
        objects.push_back(std::move(obj));
        scene->NoteObjectId(o.id);
        LogRoom(from + " added '" + o.name + "'.");
        versions[o.id] = version;
        Remember(o.id);
        return true;
    }
    SceneObject& obj = objects[index];
    bool reenter = false;
    if (!o.keepMeshes) {
        if (!force && versions[o.id] != base) return false;
        versions[o.id] = version;
        if (GetEditObjectIndex() == index) {
            ExitEditMode();
            reenter = true;
        }
    }
    auto parts = obj.parts;
    ApplyObjectState(obj, o, !o.keepMeshes);
    if (o.keepParts) obj.parts = parts;
    if (reenter && EnterEditMode(*scene, index)) EditSay(from + " changed the shape so pick again.");
    else if (!o.keepMeshes) LogRoom(from + " changed the shape of '" + obj.name + "'.");
    Remember(o.id);
    locksDirty = true;
    return true;
}

static bool ApplyPatch(In& in) {
    unsigned int id = in.U32();
    uint32_t mesh = in.U32();
    uint32_t version = in.U32();
    uint32_t count = in.U32();
    if (!in.ok || count > (in.size - in.at) / (4 + sizeof(Vertex))) return true;
    int index = FindObject(id);
    if (index < 0) return true;
    if (versions[id] != version) return false;
    auto& meshes = scene->GetObjects()[index].model.GetMeshes();
    if (mesh >= meshes.size()) return false;
    std::vector<Vertex> vertices = meshes[mesh].GetVertices();
    std::vector<std::pair<unsigned int, Vertex>> patch;
    patch.reserve(count);
    for (uint32_t k = 0; k < count; k++) {
        unsigned int at = in.U32();
        Vertex v;
        in.Raw(&v, sizeof(Vertex));
        if (!in.ok || at >= vertices.size()) return false;
        vertices[at] = v;
        patch.push_back({ at, v });
    }
    meshes[mesh].UpdateVertices(vertices);
    if (GetEditObjectIndex() == index) EditExternalVertices((int)mesh, patch);
    Remember(id);
    return true;
}

static void ApplyRemove(unsigned int id, const std::string& from) {
    int index = FindObject(id);
    if (index < 0) return;
    LogRoom(from + " deleted '" + scene->GetObjects()[index].name + "'.");
    if (GetEditObjectIndex() == index) ExitEditMode();
    scene->RemoveObject(index);
    versions.erase(id);
    Remember(id);
}

static void ApplyPresence(In& in) {
    PeerPresence p;
    p.name = in.Str();
    p.editing = in.U32();
    p.version = in.U32();
    uint32_t count = in.U32();
    if (!in.ok || count > 1000000) return;
    for (uint32_t k = 0; k < count && in.ok; k++) p.selected.push_back(in.U32());
    uint32_t locks = in.U32();
    if (!in.ok || locks > 2000000) return;
    for (uint32_t k = 0; k < locks && in.ok; k++) {
        int mesh = (int)in.U32();
        int render = (int)in.U32();
        p.locks.push_back({ mesh, render });
    }
    if (p.name == myName || !in.ok) return;
    p.color = RoomSyncColorFor(p.name);
    locksDirty = true;
    for (PeerPresence& existing : peers)
        if (existing.name == p.name) { existing = p; return; }
    peers.push_back(p);
}

static void StoreImage(In& in) {
    std::string key = in.Str();
    uint32_t size = in.U32();
    if (!in.ok || size > maxImage || size > in.size - in.at) { in.ok = false; return; }
    knownHashes.insert(key);
    if (key.find_first_of("/\\:") != std::string::npos) { in.at += size; return; }
    if (hashToLocal.count(key)) { in.at += size; return; }
    std::filesystem::create_directories("room_images");
    auto wide = (std::filesystem::current_path() / "room_images" / key).u8string();
    std::string path(wide.begin(), wide.end());
    std::ofstream file(std::filesystem::u8path(path), std::ios::binary);
    file.write((const char*)in.data + in.at, size);
    in.at += size;
    if (file.good()) {
        hashToLocal[key] = path;
        pathToHash[path] = key;
    }
}

static void CorrectOp(Out& ops, uint32_t& count, unsigned int id) {
    int index = FindObject(id);
    if (index < 0) return;
    ObjectState now = CaptureObject(scene->GetObjects()[index], nullptr);
    ObjectState room = ForRoom(now, &ops, &count);
    uint32_t version = versions[id];
    UpsertOp(ops, count, room, true, true, version, version, true);
}

static void WriteSettings(Out& out) {
    out.U32(guestsCanEdit ? 1 : 0);
    out.Str(settings.name);
    out.U32((uint32_t)settings.maxPeople);
    out.U32(settings.open ? 1 : 0);
}

static void ReadSettings(In& in) {
    bool canEdit = in.U32() != 0;
    std::string name = in.Str();
    uint32_t maxPeople = in.U32();
    bool open = in.U32() != 0;
    if (!in.ok || host) return;
    if (guestsCanEdit != canEdit) LogRoom(canEdit ? "The host let guests edit." : "The host made the room view only.");
    guestsCanEdit = canEdit;
    settings.guestsCanEdit = canEdit;
    settings.name = name.substr(0, 48);
    settings.maxPeople = (int)std::min<uint32_t>(maxPeople, 64);
    settings.open = open;
}

// Keeps what was accepted
static void ApplyBatch(In& in, const std::string& from, Out* accepted, uint32_t* acceptedCount, Out* fixes, uint32_t* fixCount) {
    uint32_t count = in.U32();
    for (uint32_t k = 0; k < count && in.ok; k++) {
        uint8_t kind = in.U8();
        uint32_t length = in.U32();
        if (!in.ok || length > in.size - in.at) return;
        In op{ in.data + in.at, length };
        in.at += length;
        bool keep = true;
        unsigned int rejectedId = 0;
        if (kind == Op_Upsert) {
            uint32_t base = op.U32(), version = op.U32();
            bool force = op.U8() != 0;
            ObjectState o;
            if (op.ok && DecodeObjectState(op.data + op.at, op.size - op.at, o)) {
                keep = ApplyUpsert(o, base, version, force, from);
                if (!keep) rejectedId = o.id;
            }
        } else if (kind == Op_Patch) {
            In probe = op;
            unsigned int id = probe.U32();
            keep = ApplyPatch(op);
            if (!keep) rejectedId = id;
        } else if (kind == Op_Remove) {
            ApplyRemove(op.U32(), from);
        } else if (kind == Op_Presence) {
            ApplyPresence(op);
        } else if (kind == Op_Image) {
            StoreImage(op);
        } else if (kind == Op_Permission) {
            ReadSettings(op);
        }
        if (keep && accepted) {
            accepted->U8(kind);
            accepted->U32(length);
            accepted->Raw(op.data, length);
            (*acceptedCount)++;
        }
        if (!keep && rejectedId && fixes) CorrectOp(*fixes, *fixCount, rejectedId);
    }
}

static void PresenceOp(Out& ops, uint32_t& count, const PeerPresence& p) {
    Out op;
    op.Str(p.name);
    op.U32(p.editing);
    op.U32(p.version);
    op.U32((uint32_t)p.selected.size());
    for (unsigned int id : p.selected) op.U32(id);
    op.U32((uint32_t)p.locks.size());
    for (const auto& lock : p.locks) {
        op.U32((uint32_t)lock.first);
        op.U32((uint32_t)lock.second);
    }
    PutOp(ops, count, Op_Presence, op.bytes);
}

static PeerPresence MyPresence() {
    PeerPresence p;
    p.name = myName;
    auto& objects = scene->GetObjects();
    for (int i : scene->GetSelection()) p.selected.push_back(objects[i].id);
    int edit = GetEditObjectIndex();
    if (edit >= 0 && edit < (int)objects.size()) {
        p.editing = objects[edit].id;
        p.version = versions[p.editing];
        EditMyLockedRender(p.locks);
    }
    return p;
}

static std::vector<uint8_t> Batch(Out& ops, uint32_t count) {
    Out out;
    Header(out, Sync_Batch);
    out.U32(count);
    out.Raw(ops.bytes.data(), ops.bytes.size());
    return out.bytes;
}

static void SendSnapshot(const std::string& to) {
    Out out;
    Header(out, Sync_Snapshot);
    out.U32(nextIndex++);
    WriteSettings(out);
    Out images;
    uint32_t imageCount = 0;
    SceneState state = CaptureScene(*scene, &lastSent);
    std::set<std::string> sentBefore = knownHashes;
    knownHashes.clear();
    for (ObjectState& o : state.objects) o = ForRoom(o, &images, &imageCount);
    knownHashes.insert(sentBefore.begin(), sentBefore.end());
    out.U32(imageCount);
    out.Raw(images.bytes.data(), images.bytes.size());
    std::vector<uint8_t> sceneBytes = SerializeScene(state);
    out.U32((uint32_t)sceneBytes.size());
    out.Raw(sceneBytes.data(), sceneBytes.size());
    out.U32((uint32_t)versions.size());
    for (const auto& entry : versions) {
        out.U32(entry.first);
        out.U32(entry.second);
    }
    Send(out.bytes, to.c_str(), nullptr, nullptr);

    // Who is doing what
    Out ops;
    uint32_t count = 0;
    PresenceOp(ops, count, MyPresence());
    for (const PeerPresence& p : peers) if (p.name != to) PresenceOp(ops, count, p);
    Send(Batch(ops, count), to.c_str(), nullptr, nullptr);
}

static void ApplySnapshot(In& in) {
    uint32_t index = in.U32();
    ReadSettings(in);
    uint32_t imageCount = in.U32();
    for (uint32_t k = 0; k < imageCount && in.ok; k++) {
        uint8_t kind = in.U8();
        uint32_t length = in.U32();
        if (!in.ok || length > in.size - in.at) return;
        In op{ in.data + in.at, length };
        in.at += length;
        if (kind == Op_Image) StoreImage(op);
    }
    uint32_t sceneSize = in.U32();
    if (!in.ok || sceneSize > in.size - in.at) return;
    SceneState state;
    if (!DeserializeScene(in.data + in.at, sceneSize, state)) {
        LogError("The room scene could not be read.");
        return;
    }
    in.at += sceneSize;
    versions.clear();
    uint32_t versionCount = in.U32();
    for (uint32_t k = 0; k < versionCount && in.ok && k < 10000000; k++) {
        unsigned int id = in.U32();
        uint32_t version = in.U32();
        if (in.ok) versions[id] = version;
    }
    for (ObjectState& o : state.objects) FromRoom(o);
    ExitEditMode();
    scene->ClearScene();
    scene->SetIdBase(index << 24);
    RestoreScene(*scene, state);
    scene->SetSelection({}, -1);
    HistoryReset(*scene);
    lastSent = CaptureScene(*scene, nullptr);
    ready = true;
    progress = -1.0f;
    LogRoom("Got the room scene.");
}

void RoomSyncBegin(SceneRenderer& s, bool isHost, const std::string& name, const RoomSettings& start) {
    scene = &s;
    active = true;
    host = isHost;
    myName = name;
    ready = isHost;
    settings = start;
    guestsCanEdit = start.guestsCanEdit;
    peers.clear();
    lastPresence.clear();
    lastMeshSend.clear();
    knownHashes.clear();
    versions.clear();
    nextIndex = 2;
    progress = isHost ? -1.0f : 0.0f;
    if (isHost) {
        s.SetIdBase(1u << 24);
        lastSent = CaptureScene(s, nullptr);
    }
}

void RoomSyncEnd() {
    active = false;
    ready = false;
    peers.clear();
    progress = -1.0f;
    EditSetForeignLocks({});
}

void RoomSyncPeerJoined(const std::string& name) {
    if (!active || !host || !scene) return;
    SendSnapshot(name);
}

void RoomSyncPeerLeft(const std::string& name) {
    peers.erase(std::remove_if(peers.begin(), peers.end(), [&](const PeerPresence& p) { return p.name == name; }), peers.end());
    locksDirty = true;
}

void RoomSyncProgress(uint64_t received, uint64_t total) {
    if (!ready && total > 0) progress = (float)((double)received / (double)total);
}

void RoomSyncReceive(const std::string& from, const std::vector<uint8_t>& packed) {
    if (!active || !scene) return;
    std::vector<uint8_t> data;
    if (!Unpack(packed, data)) return;
    In in{ data.data(), data.size() };
    char magic[4];
    if (!in.Raw(magic, 4) || memcmp(magic, "RBSY", 4) != 0 || in.U32() != syncVersion) return;
    uint32_t type = in.U32();
    if (type == Sync_Snapshot) {
        if (!host) ApplySnapshot(in);
        return;
    }
    if (type != Sync_Batch || !ready) return;
    if (!host) {
        ApplyBatch(in, from, nullptr, nullptr, nullptr, nullptr);
        return;
    }
    if (!guestsCanEdit) {
        // Only who is doing what
        In probe = in;
        uint32_t count = probe.U32();
        Out ops;
        uint32_t kept = 0;
        for (uint32_t k = 0; k < count && probe.ok; k++) {
            uint8_t kind = probe.U8();
            uint32_t length = probe.U32();
            if (!probe.ok || length > probe.size - probe.at) break;
            if (kind == Op_Presence) {
                ops.U8(kind);
                ops.U32(length);
                ops.Raw(probe.data + probe.at, length);
                kept++;
            }
            probe.at += length;
        }
        std::vector<uint8_t> filtered = Batch(ops, kept);
        In clean{ filtered.data(), filtered.size() };
        clean.at = 12;
        ApplyBatch(clean, from, nullptr, nullptr, nullptr, nullptr);
        Send(filtered, nullptr, from.c_str(), from.c_str());
        return;
    }
    Out accepted, fixes;
    uint32_t acceptedCount = 0, fixCount = 0;
    ApplyBatch(in, from, &accepted, &acceptedCount, &fixes, &fixCount);
    if (acceptedCount) Send(Batch(accepted, acceptedCount), nullptr, from.c_str(), from.c_str());
    if (fixCount) Send(Batch(fixes, fixCount), from.c_str(), nullptr, nullptr);
}

static bool Busy() {
    return ImGui::IsMouseDown(ImGuiMouseButton_Left) || IsSceneTransformActive() || ImGui::IsAnyItemActive();
}

// Only points moved
static bool PatchOps(Out& ops, uint32_t& count, const ObjectState& before, const ObjectState& now) {
    if (before.meshes.size() != now.meshes.size()) return false;
    for (size_t m = 0; m < now.meshes.size(); m++) {
        if (!before.meshes[m] || !now.meshes[m]) return false;
        if (before.meshes[m]->vertices.size() != now.meshes[m]->vertices.size()) return false;
        if (before.meshes[m]->indices != now.meshes[m]->indices) return false;
    }
    uint32_t version = versions[now.id];
    for (size_t m = 0; m < now.meshes.size(); m++) {
        const std::vector<Vertex>& a = before.meshes[m]->vertices;
        const std::vector<Vertex>& b = now.meshes[m]->vertices;
        Out op;
        op.U32(now.id);
        op.U32((uint32_t)m);
        op.U32(version);
        op.U32(0);
        uint32_t changed = 0;
        for (size_t v = 0; v < b.size(); v++) {
            if (memcmp(&a[v], &b[v], sizeof(Vertex)) == 0) continue;
            op.U32((uint32_t)v);
            op.Raw(&b[v], sizeof(Vertex));
            changed++;
        }
        if (!changed) continue;
        memcpy(op.bytes.data() + 12, &changed, 4);
        PutOp(ops, count, Op_Patch, op.bytes);
    }
    return true;
}

static void RefreshLocks() {
    int edit = GetEditObjectIndex();
    unsigned int stamp = EditOverlayStamp();
    if (!locksDirty && stamp == lockStamp && edit == lockObject) return;
    locksDirty = false;
    lockStamp = stamp;
    lockObject = edit;
    if (edit < 0) return;
    unsigned int id = scene->GetObjects()[edit].id;
    std::vector<ForeignLock> all, earlier;
    for (const PeerPresence& p : peers) {
        if (p.editing != id || p.version != versions[id]) continue;
        std::map<int, ForeignLock> perMesh;
        for (const auto& lock : p.locks) {
            ForeignLock& f = perMesh[lock.first];
            f.name = p.name;
            f.color = p.color;
            f.mesh = lock.first;
            f.render.push_back(lock.second);
        }
        for (auto& entry : perMesh) {
            all.push_back(entry.second);
            if (p.name < myName) earlier.push_back(entry.second);
        }
    }
    // Earlier name wins ties
    if (!earlier.empty()) {
        EditSetForeignLocks(earlier);
        if (EditDropLockedPicks() > 0) EditSay(earlier[0].name + " already had some of those picked.");
    }
    EditSetForeignLocks(all);
    lockStamp = EditOverlayStamp();
}

void RoomSyncUpdate(SceneRenderer& s) {
    scene = &s;
    if (!active || !ready) return;
    double now = ImGui::GetTime();
    RefreshLocks();

    if (now - lastSend < 0.05) return;
    lastSend = now;

    if (!host && !guestsCanEdit && !Busy() && !SceneMatches(s, lastSent)) {
        // Keep what they picked
        lastSent.selectedIds.clear();
        for (int i : s.GetSelection()) lastSent.selectedIds.push_back(s.GetObjects()[i].id);
        lastSent.activeId = s.selectedIndex >= 0 ? s.GetObjects()[s.selectedIndex].id : 0;
        ExitEditMode();
        RestoreScene(s, lastSent);
        LogWarning("This room is view only. Ask the host to let guests edit.");
    }

    Out ops;
    uint32_t count = 0;
    SceneState current = CaptureScene(s, &lastSent);
    std::set<unsigned int> seen;
    std::vector<ObjectState> nextKnown;
    bool canSend = host || guestsCanEdit;
    for (ObjectState& o : current.objects) {
        seen.insert(o.id);
        int known = FindState(lastSent, o.id);
        if (known >= 0 && SameObject(lastSent.objects[known], o)) { nextKnown.push_back(o); continue; }
        if (!canSend) { if (known >= 0) nextKnown.push_back(lastSent.objects[known]); continue; }
        bool meshes = known < 0 || lastSent.objects[known].revisions != o.revisions;
        bool parts = known < 0 || lastSent.objects[known].parts != o.parts;
        ObjectState fieldsOnly = o;
        if (known >= 0) {
            fieldsOnly.meshes = lastSent.objects[known].meshes;
            fieldsOnly.revisions = lastSent.objects[known].revisions;
        }
        bool fieldsChanged = known < 0 || !SameObject(lastSent.objects[known], fieldsOnly);
        if (meshes && known >= 0 && PatchOps(ops, count, lastSent.objects[known], o)) {
            if (fieldsChanged) UpsertOp(ops, count, ForRoom(fieldsOnly, &ops, &count), false, parts, versions[o.id], versions[o.id], false);
            nextKnown.push_back(o);
            continue;
        }
        if (meshes && known >= 0 && Busy() && now - lastMeshSend[o.id] < 0.25) {
            // Shape updates wait a bit
            if (fieldsChanged) UpsertOp(ops, count, ForRoom(fieldsOnly, &ops, &count), false, parts, versions[o.id], versions[o.id], false);
            nextKnown.push_back(fieldsOnly);
            continue;
        }
        uint32_t base = versions[o.id];
        uint32_t version = meshes ? base + 1 : base;
        if (meshes) {
            lastMeshSend[o.id] = now;
            versions[o.id] = version;
            locksDirty = true;
        }
        UpsertOp(ops, count, ForRoom(o, &ops, &count), meshes, parts, base, version, false);
        nextKnown.push_back(o);
    }
    for (const ObjectState& o : lastSent.objects) {
        if (seen.count(o.id)) continue;
        if (!canSend) { nextKnown.push_back(o); continue; }
        Out op;
        op.U32(o.id);
        PutOp(ops, count, Op_Remove, op.bytes);
        versions.erase(o.id);
    }
    lastSent.objects = nextKnown;

    Out presence;
    uint32_t presenceCount = 0;
    PresenceOp(presence, presenceCount, MyPresence());
    if (presence.bytes != lastPresence || now - lastPresenceTime > 2.0) {
        lastPresence = presence.bytes;
        lastPresenceTime = now;
        ops.Raw(presence.bytes.data(), presence.bytes.size());
        count++;
    }
    if (count == 0) return;
    Send(Batch(ops, count), nullptr, nullptr, nullptr);
}

void RoomSyncSetSettings(const RoomSettings& next) {
    if (!active || !host) return;
    settings = next;
    guestsCanEdit = next.guestsCanEdit;
    Out ops;
    Out op;
    WriteSettings(op);
    ops.U8(Op_Permission);
    ops.U32((uint32_t)op.bytes.size());
    ops.Raw(op.bytes.data(), op.bytes.size());
    Send(Batch(ops, 1), nullptr, nullptr, nullptr);
}

void RoomSyncSetGuestsCanEdit(bool allowed) {
    RoomSettings next = settings;
    next.guestsCanEdit = allowed;
    RoomSyncSetSettings(next);
}

bool RoomSyncLockedBy(unsigned int id, std::string& who) {
    if (!active) return false;
    for (const PeerPresence& p : peers)
        if (p.editing == id) { who = p.name; return true; }
    return false;
}

void DrawRoomPresence(SceneRenderer& s, ImDrawList* drawList, const glm::mat4& viewProj, ImVec2 viewMin, ImVec2 viewSize) {
    if (!active) return;
    auto& objects = s.GetObjects();
    auto project = [&](const glm::vec3& p, ImVec2& out) {
        glm::vec4 clip = viewProj * glm::vec4(p, 1.0f);
        if (clip.w <= 0.0001f) return false;
        glm::vec3 ndc = glm::vec3(clip) / clip.w;
        out = ImVec2(viewMin.x + (ndc.x * 0.5f + 0.5f) * viewSize.x, viewMin.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewSize.y);
        return true;
    };
    drawList->PushClipRect(viewMin, ImVec2(viewMin.x + viewSize.x, viewMin.y + viewSize.y), true);
    for (const PeerPresence& p : peers) {
        for (unsigned int id : p.selected) {
            int index = -1;
            for (int i = 0; i < (int)objects.size(); i++) if (objects[i].id == id) { index = i; break; }
            glm::vec3 lo, hi;
            if (index < 0 || !objects[index].GetWorldBounds(lo, hi)) continue;
            ImVec2 a(1e9f, 1e9f), b(-1e9f, -1e9f);
            bool any = false;
            for (int c = 0; c < 8; c++) {
                glm::vec3 corner((c & 1) ? hi.x : lo.x, (c & 2) ? hi.y : lo.y, (c & 4) ? hi.z : lo.z);
                ImVec2 sp;
                if (!project(corner, sp)) continue;
                a = ImVec2(std::min(a.x, sp.x), std::min(a.y, sp.y));
                b = ImVec2(std::max(b.x, sp.x), std::max(b.y, sp.y));
                any = true;
            }
            if (!any) continue;
            drawList->AddRect(ImVec2(a.x - 4, a.y - 4), ImVec2(b.x + 4, b.y + 4), p.color, 4.0f, 0, 2.0f);
            std::string label = p.editing == id ? p.name + " is editing" : p.name;
            ImVec2 size = ImGui::CalcTextSize(label.c_str());
            ImVec2 at = ImVec2(a.x - 4, a.y - size.y - 10);
            drawList->AddRectFilled(at, ImVec2(at.x + size.x + 10, at.y + size.y + 4), p.color, 4.0f);
            drawList->AddText(ImVec2(at.x + 5, at.y + 2), IM_COL32(15, 15, 20, 255), label.c_str());
        }
    }
    drawList->PopClipRect();
}
