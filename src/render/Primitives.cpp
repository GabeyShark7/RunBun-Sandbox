#include "render/Primitives.h"
#include <cmath>

static const float PI = 3.14159265358979f;

struct MeshBuilder {
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;

    unsigned int Add(glm::vec3 position, glm::vec3 normal, glm::vec2 uv) {
        vertices.push_back({ position, normal, uv });
        return (unsigned int)vertices.size() - 1;
    }

    void Triangle(unsigned int a, unsigned int b, unsigned int c) {
        indices.insert(indices.end(), { a, b, c });
    }

    void Quad(unsigned int a, unsigned int b, unsigned int c, unsigned int d) {
        indices.insert(indices.end(), { a, b, c, a, c, d });
    }

    Mesh Build() {
        Mesh mesh;
        mesh.Init(vertices, indices);
        return mesh;
    }
};

static void AddGrid(MeshBuilder& builder, int columns, int rows, glm::vec3 (*position)(float u, float v, const void* data),
                    glm::vec3 (*normal)(float u, float v, const void* data), const void* data) {
    unsigned int start = (unsigned int)builder.vertices.size();
    for (int r = 0; r <= rows; r++) {
        for (int c = 0; c <= columns; c++) {
            float u = (float)c / columns;
            float v = (float)r / rows;
            builder.Add(position(u, v, data), normal(u, v, data), glm::vec2(u, v));
        }
    }
    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < columns; c++) {
            unsigned int a = start + r * (columns + 1) + c;
            unsigned int b = a + 1;
            unsigned int d = a + (columns + 1);
            unsigned int e = d + 1;
            builder.Quad(a, d, e, b);
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

static glm::vec3 SpherePosition(float u, float v, const void*) {
    float theta = u * 2.0f * PI, phi = v * PI;
    return glm::vec3(-cosf(theta) * sinf(phi), cosf(phi), sinf(theta) * sinf(phi)) * 0.5f;
}

static glm::vec3 SphereNormal(float u, float v, const void* data) {
    return glm::normalize(SpherePosition(u, v, data));
}

static Mesh CreateSphere() {
    MeshBuilder builder;
    AddGrid(builder, 40, 20, SpherePosition, SphereNormal, nullptr);
    return builder.Build();
}

static glm::vec3 TubePosition(float u, float v, const void*) {
    float theta = u * 2.0f * PI;
    return glm::vec3(-cosf(theta) * 0.5f, 0.5f - v, sinf(theta) * 0.5f);
}

static glm::vec3 TubeNormal(float u, float, const void*) {
    float theta = u * 2.0f * PI;
    return glm::vec3(-cosf(theta), 0.0f, sinf(theta));
}

static Mesh CreateCylinder() {
    MeshBuilder builder;
    AddGrid(builder, 40, 1, TubePosition, TubeNormal, nullptr);
    AddDisc(builder, 0.5f, 0.5f, 40, true);
    AddDisc(builder, -0.5f, 0.5f, 40, false);
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

static Mesh CreateCone() {
    MeshBuilder builder;
    AddGrid(builder, 40, 8, ConePosition, ConeNormal, nullptr);
    AddDisc(builder, -0.5f, 0.5f, 40, false);
    return builder.Build();
}

static glm::vec3 CapsuleDirection(float u, float v) {
    float theta = u * 2.0f * PI;
    float phi = v * PI;
    return glm::vec3(-cosf(theta) * sinf(phi), cosf(phi), sinf(theta) * sinf(phi));
}

static Mesh CreateCapsule() {
    MeshBuilder builder;
    const int columns = 40;
    const int half = 10;
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

static glm::vec3 TorusPosition(float u, float v, const void*) {
    float theta = u * 2.0f * PI, phi = v * 2.0f * PI;
    float ring = 0.375f + 0.125f * cosf(phi);
    return glm::vec3(-cosf(theta) * ring, 0.125f * sinf(phi), sinf(theta) * ring);
}

static glm::vec3 TorusNormal(float u, float v, const void*) {
    float theta = u * 2.0f * PI, phi = v * 2.0f * PI;
    return glm::vec3(-cosf(theta) * cosf(phi), sinf(phi), sinf(theta) * cosf(phi));
}

static Mesh CreateTorus() {
    MeshBuilder builder;
    AddGrid(builder, 48, 24, TorusPosition, TorusNormal, nullptr);
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
        default: return "Shape";
    }
}

Mesh CreatePrimitiveMesh(PrimitiveType type) {
    switch (type) {
        case Primitive_Sphere: return CreateSphere();
        case Primitive_Cylinder: return CreateCylinder();
        case Primitive_Cone: return CreateCone();
        case Primitive_Capsule: return CreateCapsule();
        case Primitive_Torus: return CreateTorus();
        case Primitive_Pyramid: return CreatePyramid();
        case Primitive_Plane: return CreatePlane();
        default: return CreateCube();
    }
}
