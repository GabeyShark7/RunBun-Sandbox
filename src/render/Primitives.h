#pragma once
#include "render/Mesh.h"

enum PrimitiveType {
    Primitive_Cube,
    Primitive_Sphere,
    Primitive_Cylinder,
    Primitive_Cone,
    Primitive_Capsule,
    Primitive_Torus,
    Primitive_Pyramid,
    Primitive_Plane,
    Primitive_Count
};

const char* GetPrimitiveName(PrimitiveType type);
Mesh CreatePrimitiveMesh(PrimitiveType type);
