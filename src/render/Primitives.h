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
    Primitive_Wedge,
    Primitive_Roof,
    Primitive_HalfSphere,
    Primitive_Tube,
    Primitive_Count
};

const char* GetPrimitiveName(PrimitiveType type);
bool PrimitiveHasSides(PrimitiveType type);
bool PrimitiveHasThickness(PrimitiveType type);
Mesh CreatePrimitiveMesh(PrimitiveType type, int sides = 40, float thickness = 0.25f);
