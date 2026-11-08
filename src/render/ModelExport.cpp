#include "render/ModelExport.h"
#include "stb/stb_image.h"
#include <assimp/Exporter.hpp>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <zlib.h>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

namespace fs = std::filesystem;

const std::vector<ExportFormat>& ExportFormats() {
    static const std::vector<ExportFormat> formats = {
        { "glb2", "glb", "GLB (glTF binary)", true, true, true, Units_Meters },
        { "gltf2", "gltf", "glTF", true, true, true, Units_Meters },
        { "fbx", "fbx", "FBX", true, true, true, Units_Centimeters },
        { "obj", "obj", "OBJ", true, false, true, Units_Meters },
        { "collada", "dae", "Collada DAE", true, false, true, Units_Meters },
        { "3ds", "3ds", "3DS", true, false, false, Units_Meters },
        { "x3d", "x3d", "X3D", true, false, true, Units_Meters },
        { "x", "x", "DirectX X", true, false, true, Units_Meters },
        { "plyb", "ply", "PLY", false, false, false, Units_Millimeters },
        { "stlb", "stl", "STL (3D printing)", false, false, false, Units_Millimeters },
        { "3mf", "3mf", "3MF (3D printing)", false, false, false, Units_Millimeters },
        { "stp", "stp", "STEP", false, false, false, Units_Millimeters },
    };
    return formats;
}

float ExportUnitScale(int units) {
    switch (units) {
    case Units_Centimeters: return 100.0f;
    case Units_Millimeters: return 1000.0f;
    case Units_Inches: return 39.3700787f;
    default: return 1.0f;
    }
}

const char* ExportUnitName(int units) {
    switch (units) {
    case Units_Centimeters: return "Centimeters";
    case Units_Millimeters: return "Millimeters";
    case Units_Inches: return "Inches";
    default: return "Meters";
    }
}

static void Put32(std::vector<uint8_t>& out, uint32_t v) {
    out.push_back((uint8_t)(v >> 24));
    out.push_back((uint8_t)(v >> 16));
    out.push_back((uint8_t)(v >> 8));
    out.push_back((uint8_t)v);
}

static void Chunk(std::vector<uint8_t>& out, const char* type, const std::vector<uint8_t>& data) {
    Put32(out, (uint32_t)data.size());
    size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    uLong crc = crc32(0L, out.data() + start, (uInt)(out.size() - start));
    Put32(out, (uint32_t)crc);
}

std::vector<uint8_t> EncodePng(const uint8_t* pixels, int width, int height, int channels, bool flipRows) {
    if (!pixels || width <= 0 || height <= 0 || channels < 1 || channels > 4) return {};
    static const int colorTypes[5] = { 0, 0, 4, 2, 6 };
    size_t stride = (size_t)width * channels;
    std::vector<uint8_t> raw;
    raw.reserve((stride + 1) * height);
    for (int y = 0; y < height; y++) {
        int row = flipRows ? height - 1 - y : y;
        raw.push_back(0);
        raw.insert(raw.end(), pixels + row * stride, pixels + (row + 1) * stride);
    }
    uLongf size = compressBound((uLong)raw.size());
    std::vector<uint8_t> packed(size);
    if (compress2(packed.data(), &size, raw.data(), (uLong)raw.size(), 6) != Z_OK) return {};
    packed.resize(size);

    std::vector<uint8_t> out = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n' };
    std::vector<uint8_t> header;
    Put32(header, (uint32_t)width);
    Put32(header, (uint32_t)height);
    header.insert(header.end(), { 8, (uint8_t)colorTypes[channels], 0, 0, 0 });
    Chunk(out, "IHDR", header);
    Chunk(out, "IDAT", packed);
    Chunk(out, "IEND", {});
    return out;
}

static std::string Base64(const std::vector<uint8_t>& data) {
    static const char* table = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve((data.size() + 2) / 3 * 4);
    for (size_t i = 0; i < data.size(); i += 3) {
        uint32_t n = (uint32_t)data[i] << 16;
        if (i + 1 < data.size()) n |= (uint32_t)data[i + 1] << 8;
        if (i + 2 < data.size()) n |= data[i + 2];
        out += table[(n >> 18) & 63];
        out += table[(n >> 12) & 63];
        out += i + 1 < data.size() ? table[(n >> 6) & 63] : '=';
        out += i + 2 < data.size() ? table[n & 63] : '=';
    }
    return out;
}

static std::string Escape(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if ((unsigned char)c < 0x20) out += ' ';
        else out += c;
    }
    return out;
}

static std::string Num(float v) {
    if (!std::isfinite(v)) v = 0.0f;
    char text[32];
    snprintf(text, sizeof(text), "%.7g", v);
    return text;
}

static std::string Lower(std::string text) {
    for (char& c : text) c = (char)std::tolower((unsigned char)c);
    return text;
}

struct ImageFile {
    std::vector<uint8_t> bytes;
    std::string mime;
    std::string extension;
};

// Keeps png and jpg as is
static bool LoadImageFile(const std::string& path, ImageFile& out) {
    std::ifstream file(fs::u8path(path), std::ios::binary);
    if (!file) return false;
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (bytes.empty()) return false;
    std::string ext = Lower(fs::u8path(path).extension().string());
    if (ext == ".png") { out = { bytes, "image/png", "png" }; return true; }
    if (ext == ".jpg" || ext == ".jpeg") { out = { bytes, "image/jpeg", "jpg" }; return true; }
    int w, h, n;
    uint8_t* pixels = stbi_load_from_memory(bytes.data(), (int)bytes.size(), &w, &h, &n, 0);
    if (!pixels) return false;
    out = { EncodePng(pixels, w, h, n, false), "image/png", "png" };
    stbi_image_free(pixels);
    return !out.bytes.empty();
}

struct GltfBuild {
    std::string json;
    std::vector<uint8_t> bin;
    std::vector<std::pair<std::string, ImageFile>> externalImages;
    int triangles = 0;
    int images = 0;
};

static size_t AddBin(std::vector<uint8_t>& bin, const void* data, size_t size) {
    while (bin.size() % 4) bin.push_back(0);
    size_t offset = bin.size();
    bin.insert(bin.end(), (const uint8_t*)data, (const uint8_t*)data + size);
    return offset;
}

static glm::mat4 RootMatrix(const ExportOptions& options) {
    glm::mat4 root = glm::scale(glm::mat4(1.0f), glm::vec3(ExportUnitScale(options.units)));
    if (options.yUp) root = glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1, 0, 0)) * root;
    return root;
}

// Points in final place
static void Bake(ExportItem& item, const glm::mat4& root) {
    glm::mat4 matrix = root * item.transform;
    glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(matrix)));
    for (Vertex& v : item.vertices) {
        v.position = glm::vec3(matrix * glm::vec4(v.position, 1.0f));
        v.normal = normalMatrix * v.normal;
        if (glm::length(v.normal) > 1e-8f) v.normal = glm::normalize(v.normal);
    }
    if (glm::determinant(glm::mat3(matrix)) < 0.0f)
        for (size_t k = 0; k + 2 < item.indices.size(); k += 3) std::swap(item.indices[k + 1], item.indices[k + 2]);
    item.transform = glm::mat4(1.0f);
}

static bool WritePly(const std::vector<ExportItem>& items, const ExportOptions& options, const fs::path& target) {
    glm::mat4 root = RootMatrix(options);
    std::vector<ExportItem> baked = items;
    size_t vertexCount = 0, faceCount = 0;
    for (ExportItem& item : baked) {
        Bake(item, root);
        vertexCount += item.vertices.size();
        faceCount += item.indices.size() / 3;
    }
    std::ofstream file(target, std::ios::binary);
    if (!file) return false;
    file << "ply\nformat binary_little_endian 1.0\ncomment RunBun Sandbox in " << ExportUnitName(options.units) << "\n";
    file << "element vertex " << vertexCount << "\nproperty float x\nproperty float y\nproperty float z\n";
    file << "property float nx\nproperty float ny\nproperty float nz\nproperty float s\nproperty float t\n";
    file << "element face " << faceCount << "\nproperty list uchar uint vertex_indices\nend_header\n";
    for (const ExportItem& item : baked)
        for (const Vertex& v : item.vertices) {
            float values[8] = { v.position.x, v.position.y, v.position.z, v.normal.x, v.normal.y, v.normal.z, v.texCoords.x, v.texCoords.y };
            file.write((const char*)values, sizeof(values));
        }
    uint32_t base = 0;
    for (const ExportItem& item : baked) {
        for (size_t k = 0; k + 2 < item.indices.size(); k += 3) {
            uint8_t three = 3;
            uint32_t face[3] = { item.indices[k] + base, item.indices[k + 1] + base, item.indices[k + 2] + base };
            file.write((const char*)&three, 1);
            file.write((const char*)face, sizeof(face));
        }
        base += (uint32_t)item.vertices.size();
    }
    return file.good();
}

// One gltf for everything
static GltfBuild BuildGltf(const std::vector<ExportItem>& items, const ExportOptions& options, bool embedImages, bool dataUris, const std::string& baseName, bool bake) {
    GltfBuild build;
    glm::mat4 root = RootMatrix(options);

    std::vector<std::string> views, accessors, meshes, nodes, materials, textures, images, samplers;
    std::map<std::string, int> imageIndex;
    std::set<std::string> usedNames;
    samplers.push_back("{\"magFilter\":9729,\"minFilter\":9987,\"wrapS\":10497,\"wrapT\":10497}");
    samplers.push_back("{\"magFilter\":9729,\"minFilter\":9987,\"wrapS\":33071,\"wrapT\":33071}");

    auto view = [&](size_t offset, size_t length, int target) {
        std::string v = "{\"buffer\":0,\"byteOffset\":" + std::to_string(offset) + ",\"byteLength\":" + std::to_string(length);
        if (target) v += ",\"target\":" + std::to_string(target);
        views.push_back(v + "}");
        return (int)views.size() - 1;
    };

    for (const ExportItem& source : items) {
        if (source.vertices.empty() || source.indices.size() < 3) continue;
        ExportItem item = source;
        if (bake) Bake(item, root);
        std::vector<float> positions, normals, uvs;
        glm::vec3 lo(1e30f), hi(-1e30f);
        for (const Vertex& v : item.vertices) {
            positions.insert(positions.end(), { v.position.x, v.position.y, v.position.z });
            lo = glm::min(lo, v.position);
            hi = glm::max(hi, v.position);
            glm::vec3 n = glm::length(v.normal) > 1e-8f ? glm::normalize(v.normal) : glm::vec3(0, 0, 1);
            normals.insert(normals.end(), { n.x, n.y, n.z });
            uvs.insert(uvs.end(), { v.texCoords.x, v.texCoords.y });
        }
        size_t count = item.vertices.size();
        int pv = view(AddBin(build.bin, positions.data(), positions.size() * 4), positions.size() * 4, 34962);
        int nv = view(AddBin(build.bin, normals.data(), normals.size() * 4), normals.size() * 4, 34962);
        int tv = view(AddBin(build.bin, uvs.data(), uvs.size() * 4), uvs.size() * 4, 34962);
        int iv = view(AddBin(build.bin, item.indices.data(), item.indices.size() * 4), item.indices.size() * 4, 34963);
        int pa = (int)accessors.size();
        accessors.push_back("{\"bufferView\":" + std::to_string(pv) + ",\"componentType\":5126,\"count\":" + std::to_string(count) +
                            ",\"type\":\"VEC3\",\"min\":[" + Num(lo.x) + "," + Num(lo.y) + "," + Num(lo.z) + "],\"max\":[" + Num(hi.x) + "," + Num(hi.y) + "," + Num(hi.z) + "]}");
        accessors.push_back("{\"bufferView\":" + std::to_string(nv) + ",\"componentType\":5126,\"count\":" + std::to_string(count) + ",\"type\":\"VEC3\"}");
        accessors.push_back("{\"bufferView\":" + std::to_string(tv) + ",\"componentType\":5126,\"count\":" + std::to_string(count) + ",\"type\":\"VEC2\"}");
        accessors.push_back("{\"bufferView\":" + std::to_string(iv) + ",\"componentType\":5125,\"count\":" + std::to_string(item.indices.size()) + ",\"type\":\"SCALAR\"}");
        build.triangles += (int)(item.indices.size() / 3);

        const ExportMaterial& m = item.material;
        std::string pbr = "\"baseColorFactor\":[" + Num(m.color.r) + "," + Num(m.color.g) + "," + Num(m.color.b) + "," + Num(m.color.a) + "]," +
                          "\"metallicFactor\":" + Num(m.metallic) + ",\"roughnessFactor\":" + Num(m.roughness);
        if (!m.texturePath.empty()) {
            int image = -1;
            auto found = imageIndex.find(m.texturePath);
            if (found != imageIndex.end()) image = found->second;
            else {
                ImageFile file;
                if (LoadImageFile(m.texturePath, file)) {
                    std::string entry;
                    if (embedImages && dataUris) entry = "{\"uri\":\"data:" + file.mime + ";base64," + Base64(file.bytes) + "\"}";
                    else if (embedImages) {
                        int bv = view(AddBin(build.bin, file.bytes.data(), file.bytes.size()), file.bytes.size(), 0);
                        entry = "{\"bufferView\":" + std::to_string(bv) + ",\"mimeType\":\"" + file.mime + "\"}";
                    } else {
                        auto stemText = fs::u8path(m.texturePath).stem().u8string();
            std::string stem(stemText.begin(), stemText.end());
                        std::string name = baseName + "_" + stem + "." + file.extension;
                        for (int k = 2; usedNames.count(name); k++) name = baseName + "_" + stem + "_" + std::to_string(k) + "." + file.extension;
                        usedNames.insert(name);
                        entry = "{\"uri\":\"" + Escape(name) + "\"}";
                        build.externalImages.push_back({ name, file });
                    }
                    images.push_back(entry);
                    image = (int)images.size() - 1;
                    build.images++;
                }
                imageIndex[m.texturePath] = image;
            }
            if (image >= 0) {
                textures.push_back("{\"sampler\":" + std::to_string(m.clampTexture ? 1 : 0) + ",\"source\":" + std::to_string(image) + "}");
                pbr += ",\"baseColorTexture\":{\"index\":" + std::to_string(textures.size() - 1) + "}";
            }
        }
        std::string material = "{\"name\":\"" + Escape(item.name) + "\",\"doubleSided\":true,\"pbrMetallicRoughness\":{" + pbr + "}";
        if (m.glow > 0.0f) {
            glm::vec3 e = glm::vec3(m.color) * std::min(m.glow, 1.0f);
            material += ",\"emissiveFactor\":[" + Num(e.r) + "," + Num(e.g) + "," + Num(e.b) + "]";
        }
        if (m.color.a < 0.999f) material += ",\"alphaMode\":\"BLEND\"";
        materials.push_back(material + "}");

        meshes.push_back("{\"name\":\"" + Escape(item.name) + "\",\"primitives\":[{\"attributes\":{\"POSITION\":" + std::to_string(pa) + ",\"NORMAL\":" + std::to_string(pa + 1) +
                         ",\"TEXCOORD_0\":" + std::to_string(pa + 2) + "},\"indices\":" + std::to_string(pa + 3) + ",\"material\":" + std::to_string(materials.size() - 1) + "}]}");
        glm::mat4 matrix = bake ? glm::mat4(1.0f) : root * item.transform;
        std::string values;
        const float* p = glm::value_ptr(matrix);
        for (int k = 0; k < 16; k++) values += (k ? "," : "") + Num(p[k]);
        nodes.push_back("{\"name\":\"" + Escape(item.name) + "\",\"mesh\":" + std::to_string(meshes.size() - 1) + ",\"matrix\":[" + values + "]}");
    }

    auto list = [](const std::vector<std::string>& parts) {
        std::string out = "[";
        for (size_t i = 0; i < parts.size(); i++) out += (i ? "," : "") + parts[i];
        return out + "]";
    };
    std::string sceneNodes;
    for (size_t i = 0; i < nodes.size(); i++) sceneNodes += (i ? "," : "") + std::to_string(i);
    std::string json = "{\"asset\":{\"version\":\"2.0\",\"generator\":\"RunBun Sandbox\"},\"scene\":0,\"scenes\":[{\"nodes\":[" + sceneNodes + "]}]";
    json += ",\"nodes\":" + list(nodes) + ",\"meshes\":" + list(meshes) + ",\"materials\":" + list(materials);
    if (!textures.empty()) json += ",\"textures\":" + list(textures) + ",\"samplers\":" + list(samplers);
    if (!images.empty()) json += ",\"images\":" + list(images);
    json += ",\"accessors\":" + list(accessors) + ",\"bufferViews\":" + list(views);
    while (build.bin.size() % 4) build.bin.push_back(0);
    std::string buffer = "{\"byteLength\":" + std::to_string(build.bin.size());
    if (dataUris) buffer += ",\"uri\":\"data:application/octet-stream;base64," + Base64(build.bin) + "\"";
    json += ",\"buffers\":[" + buffer + "}]}";
    build.json = json;
    return build;
}

static std::vector<uint8_t> PackGlb(const GltfBuild& build) {
    std::string json = build.json;
    while (json.size() % 4) json += ' ';
    std::vector<uint8_t> out;
    auto u32 = [&](uint32_t v) { out.insert(out.end(), (uint8_t*)&v, (uint8_t*)&v + 4); };
    uint32_t total = 12 + 8 + (uint32_t)json.size() + 8 + (uint32_t)build.bin.size();
    u32(0x46546C67);
    u32(2);
    u32(total);
    u32((uint32_t)json.size());
    u32(0x4E4F534A);
    out.insert(out.end(), json.begin(), json.end());
    u32((uint32_t)build.bin.size());
    u32(0x004E4942);
    out.insert(out.end(), build.bin.begin(), build.bin.end());
    return out;
}

static bool WriteFile(const fs::path& path, const void* data, size_t size) {
    std::ofstream file(path, std::ios::binary);
    file.write((const char*)data, (std::streamsize)size);
    return file.good();
}

ExportResult ExportModels(const std::vector<ExportItem>& items, const ExportOptions& options, const std::string& path) {
    ExportResult result;
    const auto& formats = ExportFormats();
    if (options.format < 0 || options.format >= (int)formats.size()) { result.error = "Unknown file type."; return result; }
    const ExportFormat& format = formats[options.format];
    if (items.empty()) { result.error = "Nothing to export."; return result; }

    fs::path target = fs::u8path(path);
    fs::path folder = target.parent_path();
    auto baseText = target.stem().u8string();
    std::string baseName(baseText.begin(), baseText.end());
    bool embed = format.embeds && options.embedTextures;
    std::vector<ExportItem> work = items;
    if (!format.textures) for (ExportItem& item : work) item.material.texturePath.clear();

    std::string id = format.id;
    bool own = id == "glb2" || id == "gltf2";
    bool bake = id == "stlb" || id == "plyb" || id == "3mf" || id == "stp" || id == "3ds";
    if (!own) {
        for (ExportItem& item : work) {
            std::string clean;
            for (char c : item.name) if (c != '"' && c != '\'' && c != '<' && c != '>' && c != '&') clean += c;
            item.name = clean.empty() ? "Object" : clean;
        }
    }
    GltfBuild build = BuildGltf(work, options, embed || !own, id == "gltf2", baseName, bake);
    result.triangles = build.triangles;
    if (result.triangles == 0) { result.error = "The picked objects have no faces to export."; return result; }

    if (!own && format.textures && !embed) {
        // Images beside the file
        build = BuildGltf(work, options, false, false, baseName, bake);
    }
    for (const auto& image : build.externalImages) {
        if (!WriteFile(folder / fs::u8path(image.first), image.second.bytes.data(), image.second.bytes.size())) {
            result.error = "Could not write the image '" + image.first + "'.";
            return result;
        }
    }
    result.images = build.images;

    if (id == "plyb") {
        if (!WritePly(work, options, target)) { result.error = "Could not write the file."; return result; }
        result.ok = true;
        return result;
    }
    if (id == "gltf2") {
        if (!WriteFile(target, build.json.data(), build.json.size())) { result.error = "Could not write the file."; return result; }
        result.ok = true;
        return result;
    }
    std::vector<uint8_t> glb = PackGlb(build);
    if (id == "glb2") {
        if (!WriteFile(target, glb.data(), glb.size())) { result.error = "Could not write the file."; return result; }
        result.ok = true;
        return result;
    }

    // Others through assimp
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFileFromMemory(glb.data(), glb.size(), 0, "glb");
    if (!scene) {
        result.error = std::string("Could not build the model: ") + importer.GetErrorString();
        return result;
    }
    Assimp::Exporter exporter;
    if (exporter.Export(scene, format.id, path) != AI_SUCCESS) {
        result.error = std::string("Saving failed: ") + exporter.GetErrorString();
        return result;
    }
    result.ok = true;
    return result;
}
