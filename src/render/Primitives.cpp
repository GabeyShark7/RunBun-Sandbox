#include "render/Primitives.h"
#include <algorithm>
#include <cmath>

static const float PI = 3.14159265358979f;

struct MeshBuilder {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    // Built Y up then made Z up
    static glm::vec3 ZUp(glm::vec3 v) { return glm::vec3(v.x, -v.z, v.y); }

    unsigned int Add(glm::vec3 position, glm::vec3 normal, glm::vec2 uv) {
        vertices.push_back({ ZUp(position), ZUp(normal), uv });
        return (unsigned int)vertices.size() - 1;
    }

    void Triangle(unsigned int a, unsigned int b, unsigned int c) {
        indices.insert(indices.end(), { a, b, c });
    }

    void Quad(unsigned int a, unsigned int b, unsigned int c, unsigned int d) {
        indices.insert(indices.end(), { a, b, c, a, c, d });
    }

    // Flat face facing out
    void Flat(std::vector<glm::vec3> points, glm::vec3 inside) {
        glm::vec3 center(0.0f);
        for (const glm::vec3& p : points) center += p;
        center /= (float)points.size();
        glm::vec3 normal = glm::normalize(glm::cross(points[1] - points[0], points[2] - points[0]));
        if (glm::dot(normal, center - inside) < 0.0f) {
            std::reverse(points.begin(), points.end());
            normal = -normal;
        }
        glm::vec3 axisU = glm::normalize(points[1] - points[0]);
        glm::vec3 axisV = glm::cross(normal, axisU);
        float lo[2] = { 1e30f, 1e30f }, hi[2] = { -1e30f, -1e30f };
        for (const glm::vec3& p : points) {
            float u = glm::dot(p, axisU), v = glm::dot(p, axisV);
            lo[0] = std::min(lo[0], u); hi[0] = std::max(hi[0], u);
            lo[1] = std::min(lo[1], v); hi[1] = std::max(hi[1], v);
        }
        unsigned int first = (unsigned int)vertices.size();
        for (const glm::vec3& p : points) {
            float u = (glm::dot(p, axisU) - lo[0]) / std::max(hi[0] - lo[0], 1e-6f);
            float v = (glm::dot(p, axisV) - lo[1]) / std::max(hi[1] - lo[1], 1e-6f);
            Add(p, normal, glm::vec2(u, 1.0f - v));
        }
        for (size_t i = 1; i + 1 < points.size(); i++) Triangle(first, first + (unsigned int)i, first + (unsigned int)i + 1);
    }

    Mesh Build() {
        Mesh mesh;
        mesh.Init(vertices, indices);
        return mesh;
    }
};

struct ShapeData {
    float radius = 0.5f;
    float inner = 0.375f;
    float minor = 0.125f;
    float yTop = 0.5f;
    float yBottom = -0.5f;
};

static void AddGrid(MeshBuilder& builder, int columns, int rows, glm::vec3 (*position)(float u, float v, const void* data),
                    glm::vec3 (*normal)(float u, float v, const void* data), const void* data, bool flipped = false) {
    unsigned int start = (unsigned int)builder.vertices.size();
    for (int r = 0; r <= rows; r++) {
        for (int c = 0; c <= columns; c++) {
            float u = (float)c / columns;
            float v = (float)r / rows;
            glm::vec3 n = normal(u, v, data);
            builder.Add(position(u, v, data), flipped ? -n : n, glm::vec2(u, v));
        }
    }
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < columns; c++) {
            unsigned int a = start + r * (columns + 1) + c;
            unsigned int b = a + 1;
            unsigned int d = a + (columns + 1);
            unsigned int e = d + 1;
            if (flipped) builder.Quad(a, b, e, d);
            else builder.Quad(a, d, e, b);
        }
    }
}

static void AddDisc(MeshBuilder& builder, float y, float radius, int segments, bool facingUp) {
    glm::vec3 normal = glm::vec3(0, facingUp ? 1.0f : -1.0f, 0);
    unsigned int center = builder.Add(glm::vec3(0, y, 0), normal, glm::vec2(0.5f, 0.5f));
    unsigned int first = (unsigned int)builder.vertices.size();
    for (int i = 0; i <= segments; i++) {
        float a = 2.0f * PI * i / segments;
        float x = cosf(a), z = sinf(a);
        builder.Add(glm::vec3(x * radius, y, z * radius), normal, glm::vec2(0.5f + x * 0.5f, 0.5f + z * 0.5f));
    }
    for (int i = 0; i < segments; i++) {
        if (facingUp) builder.Triangle(center, first + i + 1, first + i);
        else builder.Triangle(center, first + i, first + i + 1);
    }
}

static void AddRing(MeshBuilder& builder, float y, float outer, float inner, int segments, bool facingUp) {
    glm::vec3 normal = glm::vec3(0, facingUp ? 1.0f : -1.0f, 0);
    unsigned int first = (unsigned int)builder.vertices.size();
    for (int i = 0; i <= segments; i++) {
        float a = 2.0f * PI * i / segments;
        float x = cosf(a), z = sinf(a);
        builder.Add(glm::vec3(x * outer, y, z * outer), normal, glm::vec2(0.5f + x * 0.5f, 0.5f + z * 0.5f));
        builder.Add(glm::vec3(x * inner, y, z * inner), normal, glm::vec2(0.5f + x * inner, 0.5f + z * inner));
    }
    for (int i = 0; i < segments; i++) {
        unsigned int o0 = first + i * 2, i0 = o0 + 1, o1 = o0 + 2, i1 = o0 + 3;
        if (facingUp) builder.Quad(o0, i0, i1, o1);
        else builder.Quad(o0, o1, i1, i0);
    }
}

static Mesh CreateCube() {
    MeshBuilder builder;
    struct Face { glm::vec3 normal, right, up; };
    const Face faces[] = {
        { glm::vec3( 0,  0,  1), glm::vec3( 1, 0,  0), glm::vec3(0, 1,  0) },
        { glm::vec3( 0,  0, -1), glm::vec3(-1, 0,  0), glm::vec3(0, 1,  0) },
        { glm::vec3( 1,  0,  0), glm::vec3( 0, 0, -1), glm::vec3(0, 1,  0) },
        { glm::vec3(-1,  0,  0), glm::vec3( 0, 0,  1), glm::vec3(0, 1,  0) },
        { glm::vec3( 0,  1,  0), glm::vec3( 1, 0,  0), glm::vec3(0, 0, -1) },
        { glm::vec3( 0, -1,  0), glm::vec3( 1, 0,  0), glm::vec3(0, 0,  1) },
    };
    for (const Face& face : faces) {
        glm::vec3 center = face.normal * 0.5f;
        unsigned int a = builder.Add(center - face.right * 0.5f - face.up * 0.5f, face.normal, glm::vec2(0, 1));
        unsigned int b = builder.Add(center + face.right * 0.5f - face.up * 0.5f, face.normal, glm::vec2(1, 1));
        unsigned int c = builder.Add(center + face.right * 0.5f + face.up * 0.5f, face.normal, glm::vec2(1, 0));
        unsigned int d = builder.Add(center - face.right * 0.5f + face.up * 0.5f, face.normal, glm::vec2(0, 0));
        builder.Quad(a, b, c, d);
    }
    return builder.Build();
}

static glm::vec3 SpherePosition(float u, float v, const void* data) {
    const ShapeData* shape = (const ShapeData*)data;
    float theta = u * 2.0f * PI, phi = v * PI * (shape ? shape->yTop : 1.0f);
    glm::vec3 p = glm::vec3(-cosf(theta) * sinf(phi), cosf(phi), sinf(theta) * sinf(phi)) * 0.5f;
    if (shape) p.y += shape->yBottom;
    return p;
}

static glm::vec3 SphereNormal(float u, float v, const void* data) {
    const ShapeData* shape = (const ShapeData*)data;
    float theta = u * 2.0f * PI, phi = v * PI * (shape ? shape->yTop : 1.0f);
    return glm::vec3(-cosf(theta) * sinf(phi), cosf(phi), sinf(theta) * sinf(phi));
}

static Mesh CreateSphere(int sides) {
    MeshBuilder builder;
    AddGrid(builder, sides, std::max(3, sides / 2), SpherePosition, SphereNormal, nullptr);
    return builder.Build();
}

static Mesh CreateHalfSphere(int sides) {
    MeshBuilder builder;
    ShapeData dome;
    dome.yTop = 0.5f;
    dome.yBottom = -0.25f;
    AddGrid(builder, sides, std::max(2, sides / 4), SpherePosition, SphereNormal, &dome);
    AddDisc(builder, -0.25f, 0.5f, sides, false);
    return builder.Build();
}

static glm::vec3 TubePosition(float u, float v, const void* data) {
    float radius = data ? ((const ShapeData*)data)->radius : 0.5f;
    float theta = u * 2.0f * PI;
    return glm::vec3(-cosf(theta) * radius, 0.5f - v, sinf(theta) * radius);
}

static glm::vec3 TubeNormal(float u, float, const void*) {
    float theta = u * 2.0f * PI;
    return glm::vec3(-cosf(theta), 0.0f, sinf(theta));
}

static Mesh CreateCylinder(int sides) {
    MeshBuilder builder;
    AddGrid(builder, sides, 1, TubePosition, TubeNormal, nullptr);
    AddDisc(builder, 0.5f, 0.5f, sides, true);
    AddDisc(builder, -0.5f, 0.5f, sides, false);
    return builder.Build();
}

static Mesh CreateTube(int sides, float thickness) {
    MeshBuilder builder;
    ShapeData outer, inner;
    inner.radius = 0.5f * (1.0f - std::min(std::max(thickness, 0.02f), 0.98f));
    AddGrid(builder, sides, 1, TubePosition, TubeNormal, &outer);
    AddGrid(builder, sides, 1, TubePosition, TubeNormal, &inner, true);
    AddRing(builder, 0.5f, 0.5f, inner.radius, sides, true);
    AddRing(builder, -0.5f, 0.5f, inner.radius, sides, false);
    return builder.Build();
}

static glm::vec3 ConePosition(float u, float v, const void*) {
    float theta = u * 2.0f * PI;
    float radius = 0.5f * v;
    return glm::vec3(-cosf(theta) * radius, 0.5f - v, sinf(theta) * radius);
}

static glm::vec3 ConeNormal(float u, float, const void*) {
    float theta = u * 2.0f * PI;
    return glm::normalize(glm::vec3(-cosf(theta), 0.5f, sinf(theta)));
}

static Mesh CreateCone(int sides) {
    MeshBuilder builder;
    AddGrid(builder, sides, 8, ConePosition, ConeNormal, nullptr);
    AddDisc(builder, -0.5f, 0.5f, sides, false);
    return builder.Build();
}

static glm::vec3 CapsuleDirection(float u, float v) {
    float theta = u * 2.0f * PI;
    float phi = v * PI;
    return glm::vec3(-cosf(theta) * sinf(phi), cosf(phi), sinf(theta) * sinf(phi));
}

static Mesh CreateCapsule(int sides) {
    MeshBuilder builder;
    const int columns = sides;
    const int half = std::max(2, sides / 4);
    const int rows = half * 2 + 1;

    for (int r = 0; r <= rows; r++) {
        bool top = r <= half;
        float phi = top ? (float)r / half * PI * 0.5f : PI * 0.5f + (float)(r - half - 1) / half * PI * 0.5f;
        float offset = top ? 0.25f : -0.25f;
        for (int c = 0; c <= columns; c++) {
            float u = (float)c / columns;
            glm::vec3 dir = CapsuleDirection(u, phi / PI);
            builder.Add(dir * 0.25f + glm::vec3(0, offset, 0), dir, glm::vec2(u, (float)r / rows));
        }
    }
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < columns; c++) {
            unsigned int a = r * (columns + 1) + c;
            unsigned int d = a + columns + 1;
            builder.Quad(a, d, d + 1, a + 1);
        }
    }
    return builder.Build();
}

static glm::vec3 TorusPosition(float u, float v, const void* data) {
    const ShapeData* shape = (const ShapeData*)data;
    float theta = u * 2.0f * PI, phi = v * 2.0f * PI;
    float ring = (0.5f - shape->minor) + shape->minor * cosf(phi);
    return glm::vec3(-cosf(theta) * ring, shape->minor * sinf(phi), sinf(theta) * ring);
}

static glm::vec3 TorusNormal(float u, float v, const void*) {
    float theta = u * 2.0f * PI, phi = v * 2.0f * PI;
    return glm::vec3(-cosf(theta) * cosf(phi), sinf(phi), sinf(theta) * cosf(phi));
}

static glm::vec3 TorusNormalInward(float u, float v, const void* data) {
    return -TorusNormal(u, v, data);
}

static Mesh CreateTorus(int sides, float thickness) {
    MeshBuilder builder;
    ShapeData shape;
    shape.minor = 0.5f * std::min(std::max(thickness, 0.04f), 0.96f);
    AddGrid(builder, sides, std::max(6, sides / 2), TorusPosition, TorusNormalInward, &shape, true);
    return builder.Build();
}

static Mesh CreatePyramid() {
    MeshBuilder builder;
    glm::vec3 apex = glm::vec3(0, 0.5f, 0);
    glm::vec3 corners[4] = { { -0.5f, -0.5f, 0.5f }, { 0.5f, -0.5f, 0.5f }, { 0.5f, -0.5f, -0.5f }, { -0.5f, -0.5f, -0.5f } };
    for (int i = 0; i < 4; i++) {
        glm::vec3 a = corners[i], b = corners[(i + 1) % 4];
        glm::vec3 normal = glm::normalize(glm::cross(b - a, apex - a));
        unsigned int ia = builder.Add(a, normal, glm::vec2(0, 1));
        unsigned int ib = builder.Add(b, normal, glm::vec2(1, 1));
        unsigned int ic = builder.Add(apex, normal, glm::vec2(0.5f, 0));
        builder.Triangle(ia, ib, ic);
    }
    glm::vec3 down = glm::vec3(0, -1, 0);
    unsigned int a = builder.Add(corners[0], down, glm::vec2(0, 0));
    unsigned int b = builder.Add(corners[1], down, glm::vec2(1, 0));
    unsigned int c = builder.Add(corners[2], down, glm::vec2(1, 1));
    unsigned int d = builder.Add(corners[3], down, glm::vec2(0, 1));
    builder.Quad(a, d, c, b);
    return builder.Build();
}

static Mesh CreateWedge() {
    MeshBuilder builder;
    glm::vec3 inside = glm::vec3(0.1f, -0.2f, -0.1f);
    glm::vec3 b0(-0.5f, -0.5f, 0.5f), b1(0.5f, -0.5f, 0.5f), b2(0.5f, -0.5f, -0.5f), b3(-0.5f, -0.5f, -0.5f);
    glm::vec3 t2(0.5f, 0.5f, -0.5f), t3(-0.5f, 0.5f, -0.5f);
    builder.Flat({ b0, b1, b2, b3 }, inside);
    builder.Flat({ b3, b2, t2, t3 }, inside);
    builder.Flat({ b0, b1, t2, t3 }, inside);
    builder.Flat({ b0, b3, t3 }, inside);
    builder.Flat({ b1, b2, t2 }, inside);
    return builder.Build();
}

static Mesh CreateRoof() {
    MeshBuilder builder;
    glm::vec3 inside = glm::vec3(0.0f, -0.2f, 0.0f);
    glm::vec3 b0(-0.5f, -0.5f, 0.5f), b1(0.5f, -0.5f, 0.5f), b2(0.5f, -0.5f, -0.5f), b3(-0.5f, -0.5f, -0.5f);
    glm::vec3 r0(-0.5f, 0.5f, 0.0f), r1(0.5f, 0.5f, 0.0f);
    builder.Flat({ b0, b1, b2, b3 }, inside);
    builder.Flat({ b0, b1, r1, r0 }, inside);
    builder.Flat({ b3, b2, r1, r0 }, inside);
    builder.Flat({ b0, b3, r0 }, inside);
    builder.Flat({ b1, b2, r1 }, inside);
    return builder.Build();
}

static Mesh CreatePlane() {
    MeshBuilder builder;
    glm::vec3 up = glm::vec3(0, 1, 0);
    unsigned int a = builder.Add(glm::vec3(-0.5f, 0, 0.5f), up, glm::vec2(0, 1));
    unsigned int b = builder.Add(glm::vec3(0.5f, 0, 0.5f), up, glm::vec2(1, 1));
    unsigned int c = builder.Add(glm::vec3(0.5f, 0, -0.5f), up, glm::vec2(1, 0));
    unsigned int d = builder.Add(glm::vec3(-0.5f, 0, -0.5f), up, glm::vec2(0, 0));
    builder.Quad(a, b, c, d);
    return builder.Build();
}

const char* GetPrimitiveName(PrimitiveType type) {
    switch (type) {
        case Primitive_Cube: return "Cube";
        case Primitive_Sphere: return "Sphere";
        case Primitive_Cylinder: return "Cylinder";
        case Primitive_Cone: return "Cone";
        case Primitive_Capsule: return "Capsule";
        case Primitive_Torus: return "Torus";
        case Primitive_Pyramid: return "Pyramid";
        case Primitive_Plane: return "Plane";
        case Primitive_Wedge: return "Wedge";
        case Primitive_Roof: return "Roof";
        case Primitive_HalfSphere: return "Half Sphere";
        case Primitive_Tube: return "Tube";
        default: return "Shape";
    }
}

bool PrimitiveHasSides(PrimitiveType type) {
    return type == Primitive_Sphere || type == Primitive_Cylinder || type == Primitive_Cone || type == Primitive_Capsule ||
           type == Primitive_Torus || type == Primitive_HalfSphere || type == Primitive_Tube;
}

bool PrimitiveHasThickness(PrimitiveType type) {
    return type == Primitive_Torus || type == Primitive_Tube;
}

Mesh CreatePrimitiveMesh(PrimitiveType type, int sides, float thickness) {
    if (sides < 3) sides = 40;
    if (sides > 128) sides = 128;
    switch (type) {
        case Primitive_Sphere: return CreateSphere(sides);
        case Primitive_Cylinder: return CreateCylinder(sides);
        case Primitive_Cone: return CreateCone(sides);
        case Primitive_Capsule: return CreateCapsule(sides);
        case Primitive_Torus: return CreateTorus(sides, thickness);
        case Primitive_Pyramid: return CreatePyramid();
        case Primitive_Plane: return CreatePlane();
        case Primitive_Wedge: return CreateWedge();
        case Primitive_Roof: return CreateRoof();
        case Primitive_HalfSphere: return CreateHalfSphere(sides);
        case Primitive_Tube: return CreateTube(sides, thickness);
        default: return CreateCube();
    }
}
