#include "FileDialog.h"
#include "tinyfiledialogs/tinyfiledialogs.h"

std::string OpenFileDialog() {
    // Start with 3 for now
    // Later will add more support 

    const char* filterPatterns[3] = { "*.fbx", "*.gltf", "*.glb" };

    const char* result = tinyfd_openFileDialog(
        "Import 3D Model",
        "",
        3,
        filterPatterns,
        "3D Models", //Might change this to be more detailed later because now its confusing for user
        0
    );

    if (result) return std::string(result);
    return "";
}

std::string OpenImageFileDialog() {

    //Don't know anyone who would use other file types. Probably never going to add anymore support.
    // I'll maybe try gif or idfk ill have to look it up
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