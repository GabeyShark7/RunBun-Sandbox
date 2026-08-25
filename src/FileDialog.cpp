#include "FileDialog.h"
#include "tinyfiledialogs/tinyfiledialogs.h"

std::string OpenFileDialog() {
    const char* filterPatterns[3] = { "*.fbx", "*.gltf", "*.glb" };

    const char* result = tinyfd_openFileDialog(
        "Import 3D Model",
        "",
        3,
        filterPatterns,
        "3D Models",
        0
    );

    if (result) return std::string(result);
    return "";
}

std::string OpenImageFileDialog() {
    const char* filterPatterns[4] = { "*.png", "*.jpg", "*.jpeg", "*.bmp" };

    const char* result = tinyfd_openFileDialog(
        "Import Texture",
        "",
        4,
        filterPatterns,
        "Image Files",
        0
    );

    if (result) return std::string(result);
    return "";
}