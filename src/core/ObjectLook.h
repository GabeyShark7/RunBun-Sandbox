#pragma once

enum LightType {
    Light_None = -1,
    Light_Sun = 0,
    Light_Point = 1,
    Light_Spot = 2,
    Light_Count = 3
};

enum NoiseType {
    Noise_None,
    Noise_Perlin,
    Noise_Clouds,
    Noise_Marble,
    Noise_Wood,
    Noise_Cells,
    Noise_Stripes,
    Noise_Count
};

inline const char* NoiseName(int type) {
    static const char* names[Noise_Count] = { "None", "Perlin", "Clouds", "Marble", "Wood", "Cells", "Stripes" };
    return type >= 0 && type < Noise_Count ? names[type] : "None";
}

struct ObjectLook {
    float roughness = 0.5f;
    float metallic = 0.0f;
    float glow = 0.0f;
    int light = Light_None;
    float power = 1.0f;
    float spotAngle = 40.0f;
    bool castShadows = true;
    int noise = Noise_None;
    float noiseScale = 4.0f;
    float noiseDetail = 4.0f;
    float noiseContrast = 0.3f;
    float noiseBump = 0.0f;
    float noiseRough = 0.0f;
    float noiseSeed = 0.0f;
    float noiseColor[3] = { 0.15f, 0.15f, 0.16f };

    bool operator==(const ObjectLook& o) const {
        return roughness == o.roughness && metallic == o.metallic && glow == o.glow && light == o.light &&
               power == o.power && spotAngle == o.spotAngle && castShadows == o.castShadows && noise == o.noise &&
               noiseScale == o.noiseScale && noiseDetail == o.noiseDetail && noiseContrast == o.noiseContrast &&
               noiseBump == o.noiseBump && noiseRough == o.noiseRough && noiseSeed == o.noiseSeed &&
               noiseColor[0] == o.noiseColor[0] && noiseColor[1] == o.noiseColor[1] && noiseColor[2] == o.noiseColor[2];
    }
    bool operator!=(const ObjectLook& o) const { return !(*this == o); }
};

struct LookPreset {
    const char* name;
    float r, g, b, a;
    float roughness;
    float metallic;
    float glow;
    int noise;
    float noiseScale;
    float nr, ng, nb;
    float bump;
};

inline const LookPreset* LookPresets(int& count) {
    static const LookPreset presets[] = {
        { "Plastic", 0.80f, 0.80f, 0.80f, 1.0f, 0.45f, 0.0f, 0.0f },
        { "Metal", 0.78f, 0.78f, 0.80f, 1.0f, 0.25f, 1.0f, 0.0f },
        { "Gold", 1.00f, 0.77f, 0.34f, 1.0f, 0.20f, 1.0f, 0.0f },
        { "Copper", 0.95f, 0.64f, 0.54f, 1.0f, 0.30f, 1.0f, 0.0f },
        { "Glass", 0.85f, 0.92f, 1.00f, 0.25f, 0.05f, 0.0f, 0.0f },
        { "Rubber", 0.12f, 0.12f, 0.13f, 1.0f, 0.90f, 0.0f, 0.0f },
        { "Wood", 0.62f, 0.42f, 0.24f, 1.0f, 0.65f, 0.0f, 0.0f, Noise_Wood, 3.0f, 0.36f, 0.22f, 0.11f, 0.15f },
        { "Marble", 0.92f, 0.92f, 0.90f, 1.0f, 0.15f, 0.0f, 0.0f, Noise_Marble, 2.5f, 0.35f, 0.36f, 0.40f, 0.0f },
        { "Stone", 0.55f, 0.54f, 0.52f, 1.0f, 0.85f, 0.0f, 0.0f, Noise_Clouds, 8.0f, 0.30f, 0.29f, 0.28f, 0.6f },
        { "Clay", 0.76f, 0.52f, 0.40f, 1.0f, 0.95f, 0.0f, 0.0f },
        { "Glowing", 1.00f, 0.55f, 0.20f, 1.0f, 0.50f, 0.0f, 1.0f },
    };
    count = (int)(sizeof(presets) / sizeof(presets[0]));
    return presets;
}
