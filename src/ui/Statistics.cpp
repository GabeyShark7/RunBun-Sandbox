#include "core/Profiler.h"
#include "ui/Statistics.h"
#include "ui/EditMode.h"
#include "render/SceneRenderer.h"
#include "render/EditMesh.h"
#include <cstdio>
#include <chrono>
#include <future>
#include <unordered_map>

struct MeshCounts {
    int points = 0, edges = 0, faces = 0, triangles = 0;
};

static std::unordered_map<unsigned int, MeshCounts> cache;
static std::unordered_map<unsigned int, std::shared_future<MeshCounts>> pending;

static MeshCounts Count(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
    EditMesh edit;
    edit.Build(vertices, indices);
    MeshCounts counts;
    counts.points = (int)edit.positions.size();
    counts.edges = (int)edit.edges.size();
    counts.faces = (int)edit.faces.size();
    counts.triangles = (int)(indices.size() / 3);
    return counts;
}

// Big ones count later
static MeshCounts CountMesh(const Mesh& mesh) {
    auto found = cache.find(mesh.revision);
    if (found != cache.end()) return found->second;
    auto waiting = pending.find(mesh.revision);
    if (waiting != pending.end()) {
        if (waiting->second.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            MeshCounts unknown;
            unknown.points = unknown.edges = unknown.faces = -1;
            unknown.triangles = (int)(mesh.GetIndices().size() / 3);
            return unknown;
        }
        MeshCounts counts = waiting->second.get();
        pending.erase(waiting);
        cache[mesh.revision] = counts;
        return counts;
    }
    if (cache.size() > 1024) cache.clear();
    if (mesh.GetIndices().size() < 6000) {
        MeshCounts counts = Count(mesh.GetVertices(), mesh.GetIndices());
        cache[mesh.revision] = counts;
        return counts;
    }
    std::vector<Vertex> vertices = mesh.GetVertices();
    std::vector<unsigned int> indices = mesh.GetIndices();
    pending[mesh.revision] = std::async(std::launch::async, [vertices = std::move(vertices), indices = std::move(indices)]() { return Count(vertices, indices); }).share();
    return CountMesh(mesh);
}

static void Add(MeshCounts& into, const MeshCounts& more) {
    auto add = [](int& a, int b) { a = (a < 0 || b < 0) ? -1 : a + b; };
    add(into.points, more.points);
    add(into.edges, more.edges);
    add(into.faces, more.faces);
    into.triangles += more.triangles;
}

static void Row(char* out, size_t size, const char* label, int picked, int total) {
    if (picked < 0 || total < 0) snprintf(out, size, "%-10s counting...", label);
    else snprintf(out, size, "%-10s %8d / %d", label, picked, total);
}

void DrawSceneStatistics(SceneRenderer& sceneRenderer, ImDrawList* drawList, ImVec2 viewMin, ImVec2 viewMax) {
    PROFILE("DrawSceneStatistics");
    auto& objects = sceneRenderer.GetObjects();
    char lines[6][96];
    int count = 0;

    if (IsEditModeActive()) {
        int total[3], picked[3];
        EditGetStats(total, picked);
        snprintf(lines[count++], 96, "Editing %s", objects[GetEditObjectIndex()].name.c_str());
        Row(lines[count++], 96, "Points", picked[0], total[0]);
        Row(lines[count++], 96, "Edges", picked[1], total[1]);
        Row(lines[count++], 96, "Faces", picked[2], total[2]);
        int triangles = 0;
        for (Mesh& mesh : objects[GetEditObjectIndex()].model.GetMeshes()) triangles += (int)(mesh.GetIndices().size() / 3);
        snprintf(lines[count++], 96, "%-10s %8d", "Triangles", triangles);
    } else {
        static std::unordered_map<unsigned int, MeshCounts> lastKnown;
        MeshCounts all, picked;
        for (int i = 0; i < (int)objects.size(); i++) {
            MeshCounts object;
            for (Mesh& mesh : objects[i].DisplayMeshes()) Add(object, CountMesh(mesh));
            // Keep old numbers
            auto known = lastKnown.find(objects[i].id);
            if (object.points < 0 && known != lastKnown.end()) object = known->second;
            else if (object.points >= 0) lastKnown[objects[i].id] = object;
            Add(all, object);
            if (sceneRenderer.IsSelected(i)) Add(picked, object);
        }
        Row(lines[count++], 96, "Objects", (int)sceneRenderer.GetSelection().size(), (int)objects.size());
        Row(lines[count++], 96, "Points", picked.points, all.points);
        Row(lines[count++], 96, "Edges", picked.edges, all.edges);
        Row(lines[count++], 96, "Faces", picked.faces, all.faces);
        Row(lines[count++], 96, "Triangles", picked.triangles, all.triangles);
    }

    float width = 0.0f, lineHeight = ImGui::GetTextLineHeight();
    for (int i = 0; i < count; i++) {
        float w = ImGui::CalcTextSize(lines[i]).x;
        if (w > width) width = w;
    }
    const char* note = "picked / total";
    ImVec2 size = ImVec2(width + 24.0f, lineHeight * (count + 1) + 16.0f);
    ImVec2 min = ImVec2(viewMax.x - size.x - 12.0f, viewMax.y - size.y - 12.0f);
    if (min.x < viewMin.x || min.y < viewMin.y) return;
    drawList->AddRectFilled(min, ImVec2(min.x + size.x, min.y + size.y), IM_COL32(15, 15, 20, 200), 10.0f);
    for (int i = 0; i < count; i++)
        drawList->AddText(ImVec2(min.x + 12.0f, min.y + 8.0f + lineHeight * i), IM_COL32(240, 240, 245, 255), lines[i]);
    drawList->AddText(ImVec2(min.x + 12.0f, min.y + 8.0f + lineHeight * count), IM_COL32(160, 160, 170, 255), note);
}
