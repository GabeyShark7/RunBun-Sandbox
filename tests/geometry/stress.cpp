#include "render/PolyMesh.h"
#include "render/Modifiers.h"
#include "render/Primitives.h"
#include "render/EditMesh.h"
#include <chrono>
#include <cstdio>

static RawMesh Shape(PrimitiveType type, int sides, float scale, glm::vec3 offset) {
    Mesh mesh = CreatePrimitiveMesh(type, sides, 0.25f);
    RawMesh raw;
    raw.vertices = mesh.GetVertices();
    raw.indices = mesh.GetIndices();
    for (Vertex& v : raw.vertices) v.position = v.position * scale + offset;
    return raw;
}

static double Now() {
    return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
}

static void Step(const char* name, const std::function<RawMesh()>& work) {
    double start = Now();
    RawMesh out = work();
    printf("%-40s %8.1f ms  %zu triangles\n", name, (Now() - start) * 1000.0, out.indices.size() / 3);
    fflush(stdout);
}

int main() {
    RawMesh sphere = Shape(Primitive_Sphere, 96, 1.0f, glm::vec3(0.0f));
    RawMesh box = Shape(Primitive_Cube, 0, 1.0f, glm::vec3(0.0f));
    RawMesh hole = Shape(Primitive_Cylinder, 64, 0.5f, glm::vec3(0.2f, 0.0f, 0.0f));
    Modifier smooth = MakeModifier(Mod_Subdivide);
    smooth.levels = 2;
    RawMesh dense = EvaluateModifiers(sphere, { smooth }, 9, nullptr);
    printf("dense sphere has %zu triangles\n", dense.indices.size() / 3);

    Step("box minus round hole", [&] { return CSGCombine(box, hole, CSG_Subtract); });
    Step("sphere minus round hole", [&] { return CSGCombine(sphere, hole, CSG_Subtract); });
    Step("dense sphere minus round hole", [&] { return CSGCombine(dense, hole, CSG_Subtract); });
    Step("dense sphere union box", [&] { return CSGCombine(dense, box, CSG_Union); });
    Step("edit build dense", [&] { EditMesh e; e.Build(dense.vertices, dense.indices); return dense; });
    Step("poly from dense", [&] {
        EditMesh e;
        e.Build(dense.vertices, dense.indices);
        PolyMesh p;
        p.FromEdit(e, dense.vertices, dense.indices);
        return dense;
    });
    RawMesh cut = CSGCombine(sphere, hole, CSG_Subtract);
    Step("smooth a cut sphere", [&] { return EvaluateModifiers(cut, { smooth }, 9, nullptr); });
    printf("done\n");
    return 0;
}
