#pragma once

enum LightType {
    Light_None = -1,
    Light_Sun = 0,
    Light_Point = 1,
    Light_Spot = 2,
    Light_Count = 3
};

struct ObjectLook {
    float roughness = 0.5f;
    float metallic = 0.0f;
    float glow = 0.0f;
    int light = Light_None;
    float power = 1.0f;
    float spotAngle = 40.0f;
    bool castShadows = true;

    bool operator==(const ObjectLook& o) const {
        return roughness == o.roughness && metallic == o.metallic && glow == o.glow && light == o.light &&
               power == o.power && spotAngle == o.spotAngle && castShadows == o.castShadows;
    }
    bool operator!=(const ObjectLook& o) const { return !(*this == o); }
};

struct LookPreset {
    const char* name;
    float r, g, b, a;
    float roughness;
    float metallic;
    float glow;
};

inline const LookPreset* LookPresets(int& count) {
    static const LookPreset presets[] = {
        { "Plastic", 0.80f, 0.80f, 0.80f, 1.0f, 0.45f, 0.0f, 0.0f },
        { "Metal", 0.78f, 0.78f, 0.80f, 1.0f, 0.25f, 1.0f, 0.0f },
        { "Gold", 1.00f, 0.77f, 0.34f, 1.0f, 0.20f, 1.0f, 0.0f },
        { "Copper", 0.95f, 0.64f, 0.54f, 1.0f, 0.30f, 1.0f, 0.0f },
        { "Glass", 0.85f, 0.92f, 1.00f, 0.25f, 0.05f, 0.0f, 0.0f },
        { "Rubber", 0.12f, 0.12f, 0.13f, 1.0f, 0.90f, 0.0f, 0.0f },
        { "Wood", 0.55f, 0.36f, 0.20f, 1.0f, 0.70f, 0.0f, 0.0f },
        { "Clay", 0.76f, 0.52f, 0.40f, 1.0f, 0.95f, 0.0f, 0.0f },
        { "Glowing", 1.00f, 0.55f, 0.20f, 1.0f, 0.50f, 0.0f, 1.0f },
    };
    count = (int)(sizeof(presets) / sizeof(presets[0]));
    return presets;
}
