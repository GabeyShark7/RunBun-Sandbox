#include "ui/RenderExport.h"
#include "ui/Console.h"
#include "ui/FileDialog.h"
#include "core/UISettings.h"
#include "render/SceneRenderer.h"
#include "render/ModelExport.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>

static bool openExport = false;
static bool openRender = false;
static ExportOptions exportOptions;
static bool exportPickedOnly = true;
static bool exportWithMods = true;
static int renderSize = 2;
static int customWidth = 1920;
static int customHeight = 1080;
static bool renderTransparent = false;

void OpenExportModels() { openExport = true; }
void OpenRenderImage() { openRender = true; }

static void Hint(const char* text) {
    if (!ShowHelpText()) return;
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

static std::string FileName(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

// Same math as the shader
static glm::vec2 PlaceUv(const TextureInfo& tex, glm::vec2 uv) {
    float c = std::cos(glm::radians(tex.rotation)), s = std::sin(glm::radians(tex.rotation));
    glm::vec2 p = (uv - glm::vec2(0.5f)) * tex.tiling;
    p = glm::vec2(c * p.x - s * p.y, s * p.x + c * p.y);
    return p + glm::vec2(0.5f) + tex.offset;
}

static std::vector<int> Chosen(SceneRenderer& scene, bool pickedOnly) {
    std::vector<int> chosen;
    auto& objects = scene.GetObjects();
    if (pickedOnly) chosen = scene.GetSelection();
    if (chosen.empty()) for (int i = 0; i < (int)objects.size(); i++) chosen.push_back(i);
    chosen.erase(std::remove_if(chosen.begin(), chosen.end(), [&](int i) {
        return i < 0 || i >= (int)objects.size() || objects[i].look.light >= 0 || objects[i].hole || objects[i].ghost;
    }), chosen.end());
    return chosen;
}

static std::vector<ExportItem> BuildItems(SceneRenderer& scene, const std::vector<int>& chosen) {
    std::vector<ExportItem> items;
    auto& objects = scene.GetObjects();
    for (int index : chosen) {
        SceneObject& obj = objects[index];
        RawMesh raw;
        if (!scene.GetObjectRaw(index, raw, exportWithMods)) continue;
        ExportItem item;
        item.name = obj.name;
        item.transform = obj.GetModelMatrix();
        item.vertices = std::move(raw.vertices);
        item.indices = std::move(raw.indices);
        item.material.color = obj.color;
        item.material.roughness = obj.look.roughness;
        item.material.metallic = obj.look.metallic;
        item.material.glow = obj.look.glow;
        for (const TextureInfo& tex : obj.model.GetTextures()) {
            if (!tex.enabled) continue;
            item.material.texturePath = tex.path;
            item.material.clampTexture = tex.clamp;
            for (Vertex& v : item.vertices) v.texCoords = PlaceUv(tex, v.texCoords);
            break;
        }
        items.push_back(std::move(item));
    }
    return items;
}

static void DrawExportWindow(SceneRenderer& scene) {
    const auto& formats = ExportFormats();
    if (openExport) {
        ImGui::OpenPopup("Export Models");
        openExport = false;
        exportPickedOnly = !scene.GetSelection().empty();
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Export Models", nullptr, ImGuiWindowFlags_NoResize)) return;

    Hint("Save shapes to use in other 3D apps, games or printers. Colors, shine and images come along.");
    ImGui::SeparatorText("File type");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##ExportFormat", formats[exportOptions.format].label)) {
        for (int i = 0; i < (int)formats.size(); i++) {
            char label[96];
            snprintf(label, sizeof(label), "%s   .%s", formats[i].label, formats[i].extension);
            if (ImGui::Selectable(label, i == exportOptions.format)) {
                exportOptions.format = i;
                exportOptions.yUp = formats[i].yUpDefault;
                exportOptions.units = formats[i].unitsDefault;
            }
        }
        ImGui::EndCombo();
    }
    const ExportFormat& format = formats[exportOptions.format];
    if (!format.textures) Hint("This type only keeps the shape. No colors or images.");

    ImGui::SeparatorText("What to save");
    int picked = (int)Chosen(scene, true).size();
    int all = (int)Chosen(scene, false).size();
    int which = exportPickedOnly ? 0 : 1;
    ImGui::BeginDisabled(scene.GetSelection().empty());
    char pickedLabel[48];
    snprintf(pickedLabel, sizeof(pickedLabel), "Picked (%d)", scene.GetSelection().empty() ? 0 : picked);
    ImGui::RadioButton(pickedLabel, &which, 0);
    ImGui::EndDisabled();
    ImGui::SameLine();
    char allLabel[48];
    snprintf(allLabel, sizeof(allLabel), "Everything (%d)", all);
    ImGui::RadioButton(allLabel, &which, 1);
    exportPickedOnly = which == 0 && !scene.GetSelection().empty();
    ImGui::Checkbox("Apply mods", &exportWithMods);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Save shapes the way they look with their mods. Off saves the shape underneath.");
    ImGui::TextDisabled("Lights and holes are left out");

    ImGui::SeparatorText("Options");
    if (format.textures) {
        ImGui::BeginDisabled(!format.embeds);
        bool embed = format.embeds && exportOptions.embedTextures;
        if (ImGui::Checkbox("Put images inside the file", &embed)) exportOptions.embedTextures = embed;
        ImGui::EndDisabled();
        if (!format.embeds) Hint("This type cannot hold images, so they are saved next to the file.");
        else if (!exportOptions.embedTextures) Hint("Images are saved next to the file.");
    }
    ImGui::Checkbox("Y is up", &exportOptions.yUp);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Most apps and game engines want Y up. 3D printers want Z up.");
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::BeginCombo("Units", ExportUnitName(exportOptions.units))) {
        for (int u = Units_Meters; u <= Units_Inches; u++)
            if (ImGui::Selectable(ExportUnitName(u), u == exportOptions.units)) exportOptions.units = u;
        ImGui::EndCombo();
    }

    ImGui::Spacing();
    float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    std::vector<int> chosen = Chosen(scene, exportPickedOnly);
    ImGui::BeginDisabled(chosen.empty());
    if (ImGui::Button("Export...", ImVec2(half, 0.0f))) {
        std::string suggested = std::string(chosen.size() == 1 ? scene.GetObjects()[chosen[0]].name : "RunBun Models") + "." + format.extension;
        std::string path = SaveFileDialogFor("Export Models", suggested, format.extension, format.label);
        if (!path.empty()) {
            std::vector<ExportItem> items = BuildItems(scene, chosen);
            ExportResult result = ExportModels(items, exportOptions, path);
            if (result.ok) {
                std::string text = "Exported " + std::to_string(items.size()) + (items.size() == 1 ? " object" : " objects") + " (" +
                                   std::to_string(result.triangles) + " triangles) to '" + FileName(path) + "'";
                if (result.images) text += std::string(" with ") + std::to_string(result.images) + (result.images == 1 ? " image" : " images");
                LogSuccess(text + ".");
                ImGui::CloseCurrentPopup();
            } else {
                LogError("Export to '" + FileName(path) + "' failed. " + result.error);
            }
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(half, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

struct SizePreset {
    const char* label;
    int width;
    int height;
};

static const SizePreset sizes[] = {
    { "Scene view size", 0, 0 },
    { "HD 1280 x 720", 1280, 720 },
    { "Full HD 1920 x 1080", 1920, 1080 },
    { "2K 2560 x 1440", 2560, 1440 },
    { "4K 3840 x 2160", 3840, 2160 },
    { "Square 1080 x 1080", 1080, 1080 },
    { "Phone 1080 x 1920", 1080, 1920 },
    { "Custom", -1, -1 },
};

static void DrawRenderWindow(SceneRenderer& scene, int viewWidth, int viewHeight) {
    if (openRender) {
        ImGui::OpenPopup("Render Image");
        openRender = false;
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Render Image", nullptr, ImGuiWindowFlags_NoResize)) return;

    Hint("Save a picture of what the Scene camera sees, with materials, lights and shadows.");
    ImGui::SeparatorText("Size");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::BeginCombo("##RenderSize", sizes[renderSize].label)) {
        for (int i = 0; i < (int)(sizeof(sizes) / sizeof(sizes[0])); i++)
            if (ImGui::Selectable(sizes[i].label, i == renderSize)) renderSize = i;
        ImGui::EndCombo();
    }
    int width = sizes[renderSize].width, height = sizes[renderSize].height;
    if (width == 0) { width = std::max(viewWidth, 16); height = std::max(viewHeight, 16); }
    if (width < 0) {
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("Wide", &customWidth, 0);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputInt("Tall", &customHeight, 0);
        customWidth = std::clamp(customWidth, 16, 16384);
        customHeight = std::clamp(customHeight, 16, 16384);
        width = customWidth;
        height = customHeight;
    }
    ImGui::TextDisabled("%d x %d pixels", width, height);

    ImGui::SeparatorText("Options");
    ImGui::Checkbox("See-through background", &renderTransparent);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Leaves out the background so the picture can sit on top of anything");
    UISettings& settings = GetUISettings();
    if (ImGui::Checkbox("Shadows", &settings.shadows)) ImGui::MarkIniSettingsDirty();
    Hint("The grid, lights and selection outlines are left out.");

    ImGui::Spacing();
    float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button("Render...", ImVec2(half, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
        std::string path = SaveFileDialogFor("Render Image", "RunBun Render.png", "png", "PNG image");
        if (!path.empty()) {
            std::vector<uint8_t> rgba;
            std::string error;
            scene.shadows = settings.shadows;
            if (scene.RenderImage(width, height, renderTransparent, rgba, error)) {
                std::vector<uint8_t> png = EncodePng(rgba.data(), width, height, 4, true);
                std::ofstream file(std::filesystem::u8path(path), std::ios::binary);
                file.write((const char*)png.data(), (std::streamsize)png.size());
                if (file.good() && !png.empty()) {
                    LogSuccess("Rendered " + std::to_string(width) + " x " + std::to_string(height) + " image to '" + FileName(path) + "'.");
                    ImGui::CloseCurrentPopup();
                } else {
                    LogError("Could not write '" + FileName(path) + "'.");
                }
            } else {
                LogError("Render failed. " + error);
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(half, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

static int lastViewWidth = 1280, lastViewHeight = 720;

void DrawRenderExportWindows(SceneRenderer& scene, ImVec2 sceneSize) {
    if (sceneSize.x > 16 && sceneSize.y > 16) { lastViewWidth = (int)sceneSize.x; lastViewHeight = (int)sceneSize.y; }
    DrawExportWindow(scene);
    DrawRenderWindow(scene, lastViewWidth, lastViewHeight);
}
