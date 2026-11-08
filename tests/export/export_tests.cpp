#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.h"
#include "render/ModelExport.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>

static void Walk(const aiScene* scene, const aiNode* node, aiMatrix4x4 parent, glm::vec3& lo, glm::vec3& hi) {
    aiMatrix4x4 matrix = parent * node->mTransformation;
    for (unsigned int i = 0; i < node->mNumMeshes; i++) {
        const aiMesh* mesh = scene->mMeshes[node->mMeshes[i]];
        for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
            aiVector3D p = matrix * mesh->mVertices[v];
            lo = glm::min(lo, glm::vec3(p.x, p.y, p.z));
            hi = glm::max(hi, glm::vec3(p.x, p.y, p.z));
        }
    }
    for (unsigned int c = 0; c < node->mNumChildren; c++) Walk(scene, node->mChildren[c], matrix, lo, hi);
}

static int checks = 0, failed = 0;
static void Check(bool ok, const std::string& what) {
    checks++;
    if (!ok) { failed++; printf("FAIL %s\n", what.c_str()); }
}

static ExportItem Cube(const std::string& name, const std::string& texture) {
    ExportItem item;
    item.name = name;
    item.transform = glm::translate(glm::mat4(1.0f), glm::vec3(1, 2, 3));
    const glm::vec3 n[6] = { {1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1} };
    for (int f = 0; f < 6; f++) {
        glm::vec3 u = std::abs(n[f].z) > 0.5f ? glm::vec3(1, 0, 0) : glm::vec3(0, 0, 1);
        glm::vec3 v = glm::cross(n[f], u);
        unsigned int base = (unsigned int)item.vertices.size();
        for (int k = 0; k < 4; k++) {
            float a = (k == 1 || k == 2) ? 1.0f : -1.0f, b = k >= 2 ? 1.0f : -1.0f;
            item.vertices.push_back({ (n[f] + u * a + v * b) * 0.5f, n[f], glm::vec2(a * 0.5f + 0.5f, b * 0.5f + 0.5f) });
        }
        item.indices.insert(item.indices.end(), { base, base + 1, base + 2, base, base + 2, base + 3 });
    }
    item.material.color = glm::vec4(0.2f, 0.4f, 0.8f, 1.0f);
    item.material.roughness = 0.3f;
    item.material.metallic = 1.0f;
    item.material.texturePath = texture;
    return item;
}

int main() {
    namespace fs = std::filesystem;
    fs::path dir = fs::temp_directory_path() / "runbun_export_test";
    fs::remove_all(dir);
    fs::create_directories(dir);

    // Tiny checker image
    uint8_t pixels[4 * 4 * 3];
    for (int i = 0; i < 16; i++) { uint8_t c = ((i % 4) + (i / 4)) % 2 ? 255 : 30; pixels[i * 3] = c; pixels[i * 3 + 1] = 90; pixels[i * 3 + 2] = c; }
    std::vector<uint8_t> png = EncodePng(pixels, 4, 4, 3, false);
    int w = 0, h = 0, n = 0;
    uint8_t* back = stbi_load_from_memory(png.data(), (int)png.size(), &w, &h, &n, 0);
    Check(back && w == 4 && h == 4 && n == 3 && memcmp(back, pixels, sizeof(pixels)) == 0, "png encodes and decodes the same pixels");
    stbi_image_free(back);
    fs::path texture = dir / "checker.png";
    { std::ofstream f(texture, std::ios::binary); f.write((const char*)png.data(), png.size()); }

    std::vector<ExportItem> items = { Cube("Box One", texture.string()), Cube("Box \"Two\"", texture.string()) };
    const auto& formats = ExportFormats();
    for (int i = 0; i < (int)formats.size(); i++) {
        const ExportFormat& format = formats[i];
        for (int embed = 0; embed < 2; embed++) {
            if (embed && !format.embeds) continue;
            ExportOptions options;
            options.format = i;
            options.embedTextures = embed != 0;
            options.yUp = format.yUpDefault;
            options.units = format.unitsDefault;
            fs::path out = dir / (std::string("model_") + format.id + (embed ? "_embed." : ".") + format.extension);
            ExportResult result = ExportModels(items, options, out.string());
            std::string label = std::string(format.label) + (embed ? " embedded" : "");
            Check(result.ok, label + " exports: " + result.error);
            Check(result.triangles == 24, label + " counts 24 triangles");
            if (!result.ok) continue;
            Assimp::Importer importer;
            if (std::string(format.id) == "x3d") {
                std::ifstream f(out);
                std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
                Check(text.find("<IndexedTriangleSet") != std::string::npos || text.find("<IndexedFaceSet") != std::string::npos, label + " has the shapes");
                Check(text.find("model_x3d_checker.png") != std::string::npos, label + " links the image");
                continue;
            }
            const aiScene* scene = importer.ReadFile(out.string(), 0);
            if (std::string(format.id) == "stp") {
                Check(fs::file_size(out) > 100, label + " wrote a file");
                continue;
            }
            Check(scene != nullptr, label + " reads back: " + importer.GetErrorString());
            if (!scene) continue;
            unsigned int faces = 0;
            for (unsigned int m = 0; m < scene->mNumMeshes; m++) faces += scene->mMeshes[m]->mNumFaces;
            Check(faces == 24, label + " has 24 faces after reading back, got " + std::to_string(faces));
            glm::vec3 lo(1e30f), hi(-1e30f);
            Walk(scene, scene->mRootNode, aiMatrix4x4(), lo, hi);
            float scale = ExportUnitScale(options.units);
            glm::vec3 center = glm::vec3(1, 2, 3) * scale;
            if (options.yUp) center = glm::vec3(center.x, center.z, -center.y);
            // Reader turns 3ds to y up
            if (std::string(format.id) == "3ds") center = glm::vec3(center.x, center.z, -center.y);
            glm::vec3 got = (lo + hi) * 0.5f;
            float size = hi.x - lo.x;
            Check(glm::length(got - center) < 0.01f * scale && std::abs(size - scale) < 0.01f * scale,
                  label + " keeps place and size, center " + std::to_string(got.x) + " " + std::to_string(got.y) + " " + std::to_string(got.z) + " size " + std::to_string(size));
            if (format.textures) {
                bool found = false;
                for (unsigned int m = 0; m < scene->mNumMaterials; m++) {
                    aiString path;
                    if (scene->mMaterials[m]->GetTexture(aiTextureType_DIFFUSE, 0, &path) == AI_SUCCESS ||
                        scene->mMaterials[m]->GetTexture(aiTextureType_BASE_COLOR, 0, &path) == AI_SUCCESS) found = true;
                }
                Check(found, label + " keeps the image on the material");
                if (embed) Check(scene->mNumTextures > 0, label + " carries the image inside the file");
                else if (std::string(format.id) != "3ds") {
                    bool beside = false;
                    for (auto& entry : fs::directory_iterator(dir))
                        if (entry.path().filename().string().rfind(out.stem().string() + "_checker", 0) == 0) beside = true;
                    Check(beside, label + " writes the image next to the file");
                }
            }
        }
    }
    printf("\n%d checks, %d failed\n", checks, failed);
    return failed ? 1 : 0;
}
