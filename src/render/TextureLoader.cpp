#include "render/TextureLoader.h"
#include "ui/Console.h"
#include <glad/glad.h>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#include "stb/stb_image.h"
#include <iostream>
#include <map>
#include <set>

unsigned int LoadTexture(const std::string& path) {
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    // Always four channels
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &nrComponents, 4);

    if (data) {
        GLenum format = GL_RGBA;
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    } else {
        std::cout << "Failed to load texture: " << path << "\n";
        const char* why = stbi_failure_reason();
        LogError("Could not load the image '" + std::string(path) + "'" + (why ? std::string(" (") + why + ")" : std::string()) + ".");
        glDeleteTextures(1, &textureID);
        return 0; // 0 = no texture, callers check for this
    }

    return textureID;
}
static std::set<std::string> failedPaths;

unsigned int AcquireTexture(const std::string& path, bool retry) {
    static std::map<std::string, unsigned int> cache;
    auto found = cache.find(path);
    if (found != cache.end()) return found->second;
    // Bad files are tried once
    if (!retry && failedPaths.count(path)) return 0;
    unsigned int id = LoadTexture(path);
    if (id != 0) cache[path] = id;
    else failedPaths.insert(path);
    return id;
}
