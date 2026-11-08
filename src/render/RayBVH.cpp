#include "render/RayBVH.h"
#include <algorithm>
#include <cfloat>
#include <cmath>
#include <numeric>

void RayBVH::Build(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices) {
    size_t count = indices.size() / 3;
    std::vector<glm::vec3> ta(count), tb(count), tc(count), centers(count);
    for (size_t t = 0; t < count; t++) {
        ta[t] = vertices[indices[t * 3]].position;
        tb[t] = vertices[indices[t * 3 + 1]].position;
        tc[t] = vertices[indices[t * 3 + 2]].position;
        centers[t] = (ta[t] + tb[t] + tc[t]) / 3.0f;
    }
    std::vector<int> order(count);
    std::iota(order.begin(), order.end(), 0);
    nodes.clear();
    nodes.reserve(count / 2 + 1);
    nodes.emplace_back();

    struct Work { int node, start, end; };
    std::vector<Work> stack = { { 0, 0, (int)count } };
    while (!stack.empty()) {
        Work w = stack.back();
        stack.pop_back();
        glm::vec3 lo(FLT_MAX), hi(-FLT_MAX), clo(FLT_MAX), chi(-FLT_MAX);
        for (int i = w.start; i < w.end; i++) {
            int t = order[i];
            lo = glm::min(lo, glm::min(glm::min(ta[t], tb[t]), tc[t]));
            hi = glm::max(hi, glm::max(glm::max(ta[t], tb[t]), tc[t]));
            clo = glm::min(clo, centers[t]);
            chi = glm::max(chi, centers[t]);
        }
        nodes[w.node].lo = lo;
        nodes[w.node].hi = hi;
        if (w.end - w.start <= 8) {
            nodes[w.node].start = w.start;
            nodes[w.node].count = w.end - w.start;
            continue;
        }
        glm::vec3 extent = chi - clo;
        int axis = extent.x > extent.y ? (extent.x > extent.z ? 0 : 2) : (extent.y > extent.z ? 1 : 2);
        int mid = (w.start + w.end) / 2;
        std::nth_element(order.begin() + w.start, order.begin() + mid, order.begin() + w.end,
                         [&](int x, int y) { return centers[x][axis] < centers[y][axis]; });
        int left = (int)nodes.size();
        nodes.emplace_back();
        int right = (int)nodes.size();
        nodes.emplace_back();
        nodes[w.node].left = left;
        nodes[w.node].right = right;
        stack.push_back({ left, w.start, mid });
        stack.push_back({ right, mid, w.end });
    }
    a.resize(count);
    b.resize(count);
    c.resize(count);
    for (size_t i = 0; i < count; i++) {
        a[i] = ta[order[i]];
        b[i] = tb[order[i]];
        c[i] = tc[order[i]];
    }
}

static bool BoxHit(const glm::vec3& lo, const glm::vec3& hi, const glm::vec3& origin, const glm::vec3& inverse, float limit) {
    float tMin = 0.0f, tMax = limit;
    for (int k = 0; k < 3; k++) {
        float t1 = (lo[k] - origin[k]) * inverse[k];
        float t2 = (hi[k] - origin[k]) * inverse[k];
        if (t1 > t2) std::swap(t1, t2);
        tMin = std::max(tMin, t1);
        tMax = std::min(tMax, t2);
        if (tMin > tMax) return false;
    }
    return true;
}

bool RayBVH::Hit(const glm::vec3& origin, const glm::vec3& dir, float& outDistance) const {
    if (nodes.empty()) return false;
    glm::vec3 inverse(1.0f / (std::fabs(dir.x) > 1e-20f ? dir.x : 1e-20f), 1.0f / (std::fabs(dir.y) > 1e-20f ? dir.y : 1e-20f), 1.0f / (std::fabs(dir.z) > 1e-20f ? dir.z : 1e-20f));
    float best = FLT_MAX;
    int stack[128];
    int top = 0;
    stack[top++] = 0;
    while (top > 0) {
        const Node& node = nodes[stack[--top]];
        if (!BoxHit(node.lo, node.hi, origin, inverse, best)) continue;
        if (node.left < 0) {
            for (int i = node.start; i < node.start + node.count; i++) {
                glm::vec3 e1 = b[i] - a[i], e2 = c[i] - a[i];
                glm::vec3 pv = glm::cross(dir, e2);
                float det = glm::dot(e1, pv);
                if (std::fabs(det) < 1e-20f) continue;
                float inv = 1.0f / det;
                glm::vec3 tv = origin - a[i];
                float u = glm::dot(tv, pv) * inv;
                if (u < 0.0f || u > 1.0f) continue;
                glm::vec3 qv = glm::cross(tv, e1);
                float v = glm::dot(dir, qv) * inv;
                if (v < 0.0f || u + v > 1.0f) continue;
                float t = glm::dot(e2, qv) * inv;
                if (t > 0.0f && t < best) best = t;
            }
        } else if (top < 126) {
            stack[top++] = node.left;
            stack[top++] = node.right;
        }
    }
    if (best == FLT_MAX) return false;
    outDistance = best;
    return true;
}
