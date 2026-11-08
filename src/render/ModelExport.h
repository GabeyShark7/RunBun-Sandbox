#pragma once
#include "render/Mesh.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>

struct ExportMaterial {
    glm::vec4 color = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
    float roughness = 0.5f;
    float metallic = 0.0f;
    float glow = 0.0f;
    std::string texturePath;
    bool clampTexture = false;
};

struct ExportItem {
    std::string name;
    glm::mat4 transform = glm::mat4(1.0f);
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    ExportMaterial material;
};

struct ExportFormat {
    const char* id;
    const char* extension;
    const char* label;
    bool textures;
    bool embeds;
    bool yUpDefault;
    int unitsDefault;
};

enum ExportUnits { Units_Meters, Units_Centimeters, Units_Millimeters, Units_Inches };

struct ExportOptions {
    int format = 0;
    bool embedTextures = true;
    bool yUp = true;
    int units = Units_Meters;
};

struct ExportResult {
    bool ok = false;
    int triangles = 0;
    int images = 0;
    std::string error;
};

const std::vector<ExportFormat>& ExportFormats();
float ExportUnitScale(int units);
const char* ExportUnitName(int units);
ExportResult ExportModels(const std::vector<ExportItem>& items, const ExportOptions& options, const std::string& path);
// Png bytes from pixels
std::vector<uint8_t> EncodePng(const uint8_t* pixels, int width, int height, int channels, bool flipRows);
