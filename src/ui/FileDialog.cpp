#include "ui/FileDialog.h"
#include "tinyfiledialogs/tinyfiledialogs.h"

// Paths come back as utf8
[[maybe_unused]] static const bool utf8Paths = (tinyfd_winUtf8 = 1, true);
#include <cctype>

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
std::string OpenSceneFileDialog() {
    const char* filterPatterns[1] = { "*.rbscene" };
    const char* result = tinyfd_openFileDialog("Open Scene", "", 1, filterPatterns, "RunBun Scenes", 0);
    return result ? std::string(result) : "";
}

std::string SaveSceneFileDialog(const std::string& suggested) {
    const char* filterPatterns[1] = { "*.rbscene" };
    const char* result = tinyfd_saveFileDialog("Save Scene", suggested.c_str(), 1, filterPatterns, "RunBun Scenes");
    if (!result) return "";
    std::string path = result;
    if (path.size() < 8 || path.substr(path.size() - 8) != ".rbscene") path += ".rbscene";
    return path;
}

std::string SaveSTLFileDialog(const std::string& suggested) {
    const char* filterPatterns[1] = { "*.stl" };
    const char* result = tinyfd_saveFileDialog("Export STL", suggested.c_str(), 1, filterPatterns, "STL for 3D printing");
    if (!result) return "";
    std::string path = result;
    if (path.size() < 4 || (path.substr(path.size() - 4) != ".stl" && path.substr(path.size() - 4) != ".STL")) path += ".stl";
    return path;
}

std::string SaveLogFileDialog(const std::string& suggested) {
    const char* filterPatterns[1] = { "*.txt" };
    const char* result = tinyfd_saveFileDialog("Save Console Log", suggested.c_str(), 1, filterPatterns, "Text file");
    if (!result) return "";
    std::string path = result;
    if (path.size() < 4 || (path.substr(path.size() - 4) != ".txt" && path.substr(path.size() - 4) != ".TXT")) path += ".txt";
    return path;
}

std::string SaveFileDialogFor(const std::string& title, const std::string& suggested, const std::string& extension, const std::string& description) {
    std::string pattern = "*." + extension;
    const char* filterPatterns[1] = { pattern.c_str() };
    const char* result = tinyfd_saveFileDialog(title.c_str(), suggested.c_str(), 1, filterPatterns, description.c_str());
    if (!result) return "";
    std::string path = result;
    std::string want = "." + extension;
    auto lower = [](std::string text) { for (char& c : text) c = (char)tolower((unsigned char)c); return text; };
    if (path.size() < want.size() || lower(path.substr(path.size() - want.size())) != want) path += want;
    return path;
}
