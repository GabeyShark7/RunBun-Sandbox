#pragma once
#include "render/CSG.h"
#include <functional>
#include <vector>

enum ModifierType {
    Mod_Boolean = 0,
    Mod_Mirror,
    Mod_Array,
    Mod_Bevel,
    Mod_Subdivide,
    Mod_Solidify,
    Mod_Smooth,
    Mod_Count
};

struct Modifier {
    int type = Mod_Mirror;
    bool enabled = true;
    int axis = 0;
    int count = 3;
    int levels = 1;
    int operation = CSG_Subtract;
    unsigned int target = 0;
    bool hideTarget = true;
    float amount = 0.2f;
    float angle = 30.0f;

    bool operator==(const Modifier& o) const {
        return type == o.type && enabled == o.enabled && axis == o.axis && count == o.count && levels == o.levels &&
               operation == o.operation && target == o.target && hideTarget == o.hideTarget && amount == o.amount && angle == o.angle;
    }
};

const char* GetModifierName(int type);
const char* GetModifierHelp(int type);
Modifier MakeModifier(int type);

// Partner in local space
typedef std::function<bool(unsigned int id, RawMesh& outLocal)> ModifierTargetFetch;

RawMesh EvaluateModifiers(const RawMesh& base, const std::vector<Modifier>& modifiers, size_t count, const ModifierTargetFetch& fetch);
