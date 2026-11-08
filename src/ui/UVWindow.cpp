#include "core/Profiler.h"
#include "core/UISettings.h"
#include "ui/MaterialWindow.h"
#include "ui/DockLayout.h"
#include "ui/PanelHeader.h"
#include "ui/EditMode.h"
#include "ui/SceneGizmo.h"
#include "render/SceneRenderer.h"
#include "render/PolyMesh.h"
#include "render/UVUnwrap.h"
#include "imgui/imgui.h"
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstdio>

static PanelCollapseState uvCollapse;
static PanelCollapseState toolsCollapse;
static float zoom = 1.0f;
static glm::vec2 viewCenter = glm::vec2(0.5f);

enum UVModalMode { UVModal_None, UVModal_Move, UVModal_Scale, UVModal_Rotate };
struct UVModal {
    UVModalMode mode = UVModal_None;
    ImVec2 startMouse;
    glm::vec2 center;
    int axis = -1;
};
static UVModal uvModal;

struct UVBox {
    bool pending = false;
    bool dragging = false;
    ImVec2 start;
};
static UVBox uvBox;

static void Hint(const char* text) {
    if (!ShowHelpText()) return;
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::TextWrapped("%s", text);
    ImGui::PopStyleColor();
}

static SceneObject* SelectedObject(SceneRenderer& sceneRenderer) {
    auto& objects = sceneRenderer.GetObjects();
    int selected = sceneRenderer.selectedIndex;
    return (selected >= 0 && selected < (int)objects.size()) ? &objects[selected] : nullptr;
}

static const TextureInfo* ShownTexture(SceneObject& obj) {
    for (const TextureInfo& tex : obj.model.GetTextures())
        if (tex.enabled) return &tex;
    return nullptr;
}

static void DrawChecker(ImDrawList* drawList, ImVec2 min, ImVec2 max) {
    int cells = 8;
    float stepX = (max.x - min.x) / cells, stepY = (max.y - min.y) / cells;
    ImU32 dark = ImGui::GetColorU32(ImGuiCol_FrameBg);
    ImU32 light = ImGui::GetColorU32(ImGuiCol_FrameBgHovered);
    for (int y = 0; y < cells; y++)
        for (int x = 0; x < cells; x++) {
            ImVec2 a = ImVec2(min.x + x * stepX, min.y + y * stepY);
            drawList->AddRectFilled(a, ImVec2(a.x + stepX, a.y + stepY), ((x + y) & 1) ? light : dark);
        }
}

static float Length(ImVec2 v) { return sqrtf(v.x * v.x + v.y * v.y); }

static float SegmentDistance(ImVec2 p, ImVec2 a, ImVec2 b) {
    ImVec2 ab = ImVec2(b.x - a.x, b.y - a.y);
    float lengthSq = ab.x * ab.x + ab.y * ab.y;
    float t = lengthSq > 0.0f ? ((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / lengthSq : 0.0f;
    t = t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
    return Length(ImVec2(a.x + ab.x * t - p.x, a.y + ab.y * t - p.y));
}

static bool InsidePolygon(ImVec2 p, const std::vector<ImVec2>& poly) {
    bool inside = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        if ((poly[i].y > p.y) != (poly[j].y > p.y) &&
            p.x < (poly[j].x - poly[i].x) * (p.y - poly[i].y) / (poly[j].y - poly[i].y) + poly[i].x) inside = !inside;
    }
    return inside;
}

void DrawUVWindow(SceneRenderer& sceneRenderer, bool* open) {
    PROFILE("DrawUVWindow");
    if (!ImGui::Begin(PanelId("UV"), open, PanelFlags())) {
        ImGui::End();
        return;
    }
    if (!DrawPanelHeader(uvCollapse)) {
        ImGui::End();
        return;
    }

    SceneObject* obj = SelectedObject(sceneRenderer);
    if (!obj) {
        Hint("Select an object to see how its image wraps around it.");
        ImGui::End();
        return;
    }
    bool editing = IsEditModeActive() && GetEditObjectIndex() == sceneRenderer.selectedIndex;
    const TextureInfo* shown = ShownTexture(*obj);
    ImGui::TextDisabled("%s", shown ? shown->name.c_str() : "No image turned on");
    ImGui::SameLine(ImGui::GetContentRegionMax().x - ImGui::CalcTextSize("Frame").x - ImGui::GetStyle().FramePadding.x * 2.0f);
    if (ImGui::SmallButton("Frame")) { zoom = 1.0f; viewCenter = glm::vec2(0.5f); }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Show the whole image again");

    ImVec2 avail = ImGui::GetContentRegionAvail();
    float hintHeight = ImGui::GetTextLineHeightWithSpacing() * 2.2f;
    ImVec2 size = ImVec2(avail.x, avail.y - hintHeight);
    if (size.x < 40.0f || size.y < 40.0f) {
        ImGui::End();
        return;
    }
    ImVec2 min = ImGui::GetCursorScreenPos();
    ImVec2 max = ImVec2(min.x + size.x, min.y + size.y);
    ImGui::InvisibleButton("UVCanvas", size, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
    bool hovered = ImGui::IsItemHovered();
    bool pressed = ImGui::IsItemActivated() && ImGui::IsMouseClicked(ImGuiMouseButton_Left);

    ImGuiIO& io = ImGui::GetIO();
    ImVec2 mouse = io.MousePos;
    ImVec2 middle = ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f);
    float scale = fminf(size.x, size.y) * 0.92f * zoom;
    auto toScreen = [&](const glm::vec2& uv) { return ImVec2(middle.x + (uv.x - viewCenter.x) * scale, middle.y + (uv.y - viewCenter.y) * scale); };
    auto toUV = [&](ImVec2 p) { return glm::vec2((p.x - middle.x) / scale + viewCenter.x, (p.y - middle.y) / scale + viewCenter.y); };

    if (hovered && io.MouseWheel != 0.0f) {
        glm::vec2 under = toUV(mouse);
        zoom = fminf(fmaxf(zoom * powf(1.15f, io.MouseWheel), 0.1f), 40.0f);
        scale = fminf(size.x, size.y) * 0.92f * zoom;
        viewCenter = under - glm::vec2((mouse.x - middle.x) / scale, (mouse.y - middle.y) / scale);
    }
    if (ImGui::IsItemActive() && (ImGui::IsMouseDragging(ImGuiMouseButton_Middle, 0.0f) || (ImGui::IsMouseDragging(ImGuiMouseButton_Right, 0.0f) && uvModal.mode == UVModal_None))) {
        viewCenter -= glm::vec2(io.MouseDelta.x, io.MouseDelta.y) / scale;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    drawList->PushClipRect(min, max, true);
    drawList->AddRectFilled(min, max, ImGui::GetColorU32(ImGuiCol_WindowBg));
    ImVec2 imageMin = toScreen(glm::vec2(0.0f)), imageMax = toScreen(glm::vec2(1.0f));
    if (shown) drawList->AddImage((ImTextureID)(intptr_t)shown->id, imageMin, imageMax);
    else DrawChecker(drawList, imageMin, imageMax);
    drawList->AddRect(imageMin, imageMax, ImGui::GetColorU32(ImGuiCol_Border), 0.0f, 0, 1.5f);

    ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);
    ImU32 lineColor = ImGui::GetColorU32(ImVec4(accent.x, accent.y, accent.z, 0.9f));
    ImU32 shadow = IM_COL32(0, 0, 0, 140);
    ImU32 picked = IM_COL32(255, 170, 60, 255);
    ImU32 pickedFill = IM_COL32(255, 170, 60, 60);
    ImU32 seam = IM_COL32(235, 60, 80, 255);
    char readout[128] = "";

    if (!editing) {
        for (Mesh& mesh : obj->model.GetMeshes()) {
            const auto& vertices = mesh.GetVertices();
            const auto& indices = mesh.GetIndices();
            size_t limit = indices.size() < 60000 ? indices.size() : 60000;
            for (size_t t = 0; t + 2 < limit; t += 3) {
                ImVec2 a = toScreen(vertices[indices[t]].texCoords), b = toScreen(vertices[indices[t + 1]].texCoords), c = toScreen(vertices[indices[t + 2]].texCoords);
                drawList->AddTriangle(a, b, c, shadow, 3.0f);
                drawList->AddTriangle(a, b, c, lineColor, 1.2f);
            }
        }
    } else {
        EditSelectMode mode = GetEditSelectMode();
        for (int m = 0; m < EditMeshCount(); m++) {
            const PolyMesh* poly = EditPoly(m);
            if (!poly) continue;
            bool dense = poly->faces.size() > 30000;
            for (size_t f = 0; f < poly->faces.size(); f++) {
                const PolyFace& face = poly->faces[f];
                bool faceOn = EditPolyFaceSelected(m, (int)f);
                if (dense && !faceOn) continue;
                std::vector<ImVec2> points;
                for (const glm::vec2& uv : face.uvs) points.push_back(toScreen(uv));
                if (faceOn && points.size() >= 3) {
                    ImDrawListFlags flags = drawList->Flags;
                    drawList->Flags &= ~ImDrawListFlags_AntiAliasedFill;
                    for (size_t k = 1; k + 1 < points.size(); k++) drawList->AddTriangleFilled(points[0], points[k], points[k + 1], pickedFill);
                    drawList->Flags = flags;
                }
                for (size_t c = 0; c < points.size(); c++) {
                    size_t d = (c + 1) % points.size();
                    bool on = EditPolyVertexSelected(m, face.verts[c]) && EditPolyVertexSelected(m, face.verts[d]);
                    bool isSeam = EditIsSeam(m, face.verts[c], face.verts[d]);
                    drawList->AddLine(points[c], points[d], shadow, 3.0f);
                    drawList->AddLine(points[c], points[d], on ? picked : (isSeam ? seam : lineColor), on || isSeam ? 2.0f : 1.2f);
                }
                if (mode == EditSelect_Vertex)
                    for (size_t c = 0; c < points.size(); c++)
                        if (EditPolyVertexSelected(m, face.verts[c])) drawList->AddCircleFilled(points[c], 3.5f, picked);
            }
        }

        bool idle = uvModal.mode == UVModal_None;
        bool keys = hovered && idle && !io.WantTextInput && !io.KeyCtrl && !io.KeyAlt && !uvBox.pending;
        UVModalMode wanted = UVModal_None;
        if (keys && ImGui::IsKeyPressed(ImGuiKey_G, false)) wanted = UVModal_Move;
        if (keys && ImGui::IsKeyPressed(ImGuiKey_S, false)) wanted = UVModal_Scale;
        if (keys && ImGui::IsKeyPressed(ImGuiKey_R, false)) wanted = UVModal_Rotate;
        if (keys && ImGui::IsKeyPressed(ImGuiKey_A, false)) EditToggleSelectAll();
        if (keys && ImGui::IsKeyPressed(ImGuiKey_1, false)) SetEditSelectMode(EditSelect_Vertex);
        if (keys && ImGui::IsKeyPressed(ImGuiKey_2, false)) SetEditSelectMode(EditSelect_Edge);
        if (keys && ImGui::IsKeyPressed(ImGuiKey_3, false)) SetEditSelectMode(EditSelect_Face);
        if (wanted != UVModal_None && !IsSceneTransformActive()) {
            glm::vec2 center;
            if (EditUVBegin(center)) {
                uvModal.mode = wanted;
                uvModal.startMouse = mouse;
                uvModal.center = center;
                uvModal.axis = -1;
            }
        }

        bool clickUsed = false;
        if (uvModal.mode != UVModal_None) {
            if (ImGui::IsKeyPressed(ImGuiKey_X, false)) uvModal.axis = uvModal.axis == 0 ? -1 : 0;
            if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) uvModal.axis = uvModal.axis == 1 ? -1 : 1;
            glm::vec2 center = uvModal.center;
            ImVec2 centerScreen = toScreen(center);
            if (uvModal.mode == UVModal_Move) {
                glm::vec2 delta = glm::vec2(mouse.x - uvModal.startMouse.x, mouse.y - uvModal.startMouse.y) / scale;
                if (io.KeyCtrl) delta = glm::round(delta * 64.0f) / 64.0f;
                if (uvModal.axis == 0) delta.y = 0.0f;
                if (uvModal.axis == 1) delta.x = 0.0f;
                EditUVUpdate(sceneRenderer, [&](const glm::vec2& uv) { return uv + delta; });
                snprintf(readout, sizeof(readout), "Move   %.3f across   %.3f down", delta.x, delta.y);
            } else if (uvModal.mode == UVModal_Scale) {
                float startDistance = fmaxf(Length(ImVec2(uvModal.startMouse.x - centerScreen.x, uvModal.startMouse.y - centerScreen.y)), 4.0f);
                float factor = Length(ImVec2(mouse.x - centerScreen.x, mouse.y - centerScreen.y)) / startDistance;
                if (io.KeyCtrl) factor = roundf(factor * 10.0f) / 10.0f;
                glm::vec2 k(factor);
                if (uvModal.axis == 0) k.y = 1.0f;
                if (uvModal.axis == 1) k.x = 1.0f;
                EditUVUpdate(sceneRenderer, [&](const glm::vec2& uv) { return center + (uv - center) * k; });
                snprintf(readout, sizeof(readout), "Scale   x%.3f", factor);
            } else {
                float a0 = atan2f(uvModal.startMouse.y - centerScreen.y, uvModal.startMouse.x - centerScreen.x);
                float a1 = atan2f(mouse.y - centerScreen.y, mouse.x - centerScreen.x);
                float degrees = (a1 - a0) * 57.2957795f;
                if (io.KeyCtrl) degrees = roundf(degrees / 5.0f) * 5.0f;
                float cs = cosf(degrees / 57.2957795f), sn = sinf(degrees / 57.2957795f);
                EditUVUpdate(sceneRenderer, [&](const glm::vec2& uv) { glm::vec2 d = uv - center; return center + glm::vec2(d.x * cs - d.y * sn, d.x * sn + d.y * cs); });
                snprintf(readout, sizeof(readout), "Rotate   %.1f deg", degrees);
            }
            drawList->AddCircleFilled(centerScreen, 3.5f, IM_COL32(245, 245, 245, 255));
            if (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
                EditUVEnd(sceneRenderer, true);
                uvModal = UVModal();
                clickUsed = true;
            } else if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) || ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) {
                EditUVEnd(sceneRenderer, false);
                uvModal = UVModal();
                clickUsed = true;
            }
        }

        if (pressed && !clickUsed && uvModal.mode == UVModal_None) {
            uvBox.pending = true;
            uvBox.dragging = false;
            uvBox.start = mouse;
        }
        if (uvBox.pending) {
            if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
                if (Length(ImVec2(mouse.x - uvBox.start.x, mouse.y - uvBox.start.y)) > 5.0f) uvBox.dragging = true;
                if (uvBox.dragging) {
                    ImVec2 lo = ImVec2(fminf(uvBox.start.x, mouse.x), fminf(uvBox.start.y, mouse.y));
                    ImVec2 hi = ImVec2(fmaxf(uvBox.start.x, mouse.x), fmaxf(uvBox.start.y, mouse.y));
                    drawList->AddRectFilled(lo, hi, IM_COL32(255, 255, 255, 25));
                    drawList->AddRect(lo, hi, IM_COL32(255, 255, 255, 200), 0.0f, 0, 1.5f);
                }
            } else {
                if (uvBox.dragging) {
                    ImVec2 lo = ImVec2(fminf(uvBox.start.x, mouse.x), fminf(uvBox.start.y, mouse.y));
                    ImVec2 hi = ImVec2(fmaxf(uvBox.start.x, mouse.x), fmaxf(uvBox.start.y, mouse.y));
                    EditSelectPolyBox([&](int m, int f, int c) {
                        ImVec2 p = toScreen(EditPoly(m)->faces[f].uvs[c]);
                        return p.x >= lo.x && p.x <= hi.x && p.y >= lo.y && p.y <= hi.y;
                    }, io.KeyCtrl ? EditBox_Remove : (io.KeyShift ? EditBox_Add : EditBox_Replace));
                } else {
                    int bestMesh = -1, bestFace = -1, bestCorner = 0, bestNext = 0;
                    float best = mode == EditSelect_Vertex ? 10.0f : (mode == EditSelect_Edge ? 8.0f : FLT_MAX);
                    for (int m = 0; m < EditMeshCount(); m++) {
                        const PolyMesh* poly = EditPoly(m);
                        for (size_t f = 0; poly && f < poly->faces.size(); f++) {
                            const PolyFace& face = poly->faces[f];
                            std::vector<ImVec2> points;
                            for (const glm::vec2& uv : face.uvs) points.push_back(toScreen(uv));
                            if (mode == EditSelect_Vertex) {
                                for (size_t c = 0; c < points.size(); c++) {
                                    float d = Length(ImVec2(points[c].x - uvBox.start.x, points[c].y - uvBox.start.y));
                                    if (d < best) { best = d; bestMesh = m; bestFace = (int)f; bestCorner = (int)c; }
                                }
                            } else if (mode == EditSelect_Edge) {
                                for (size_t c = 0; c < points.size(); c++) {
                                    size_t n = (c + 1) % points.size();
                                    float d = SegmentDistance(uvBox.start, points[c], points[n]);
                                    if (d < best) { best = d; bestMesh = m; bestFace = (int)f; bestCorner = (int)c; bestNext = (int)n; }
                                }
                            } else if (points.size() >= 3 && InsidePolygon(uvBox.start, points)) {
                                float area = 0.0f;
                                for (size_t c = 0; c < points.size(); c++) {
                                    size_t n = (c + 1) % points.size();
                                    area += points[c].x * points[n].y - points[n].x * points[c].y;
                                }
                                area = fabsf(area);
                                if (area < best) { best = area; bestMesh = m; bestFace = (int)f; }
                            }
                        }
                    }
                    EditSelectPolyElement(bestMesh >= 0 ? bestMesh : 0, bestFace, bestCorner, bestNext, io.KeyShift);
                }
                uvBox = UVBox();
            }
        }
    }

    if (readout[0]) {
        ImVec2 textSize = ImGui::CalcTextSize(readout);
        ImVec2 boxMin = ImVec2(min.x + 8.0f, max.y - textSize.y - 20.0f);
        drawList->AddRectFilled(boxMin, ImVec2(boxMin.x + textSize.x + 16.0f, max.y - 8.0f), IM_COL32(15, 15, 20, 210), 6.0f);
        drawList->AddText(ImVec2(boxMin.x + 8.0f, boxMin.y + 6.0f), IM_COL32(245, 245, 245, 255), readout);
    }
    drawList->PopClipRect();

    Hint(editing ? "Click or drag to pick. G move  S scale  R rotate  (X or Y locks a direction). Scroll zooms, right-drag pans."
                 : "Each outline is a piece of the object laid flat on the image. Press Tab in the Scene to edit them.");
    ImGui::End();
}

static bool ToggleButton(const char* label, bool selected, float width) {
    if (selected) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    }
    bool clicked = ImGui::Button(label, ImVec2(width, 0.0f));
    if (selected) ImGui::PopStyleColor(3);
    return clicked;
}

static bool ToolButton(const char* label, const char* tip, float width) {
    bool clicked = ImGui::Button(label, ImVec2(width, 0.0f));
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("%s", tip);
    return clicked;
}

void DrawUVToolsWindow(SceneRenderer& sceneRenderer, bool* open) {
    if (!ImGui::Begin(PanelId("UV Tools"), open, PanelFlags())) {
        ImGui::End();
        return;
    }
    if (DrawPanelHeader(toolsCollapse)) {
        auto& objects = sceneRenderer.GetObjects();
        int selected = sceneRenderer.selectedIndex;
        float full = ImGui::GetContentRegionAvail().x;
        float spacing = ImGui::GetStyle().ItemSpacing.x;
        float half = (full - spacing) * 0.5f;
        float third = (full - spacing * 2.0f) / 3.0f;
        bool editing = IsEditModeActive() && GetEditObjectIndex() == selected;

        if (!editing) {
            ImGui::TextUnformatted("Lay images onto shapes");
            Hint("Mark seams where the image may split, unwrap the shape flat, then arrange the pieces over the image.");
            ImGui::Spacing();
            if (selected < 0 || selected >= (int)objects.size()) Hint("First click an object.");
            else if (ImGui::Button("Edit UVs   (Tab)", ImVec2(full, 0.0f))) EnterEditMode(sceneRenderer, selected);
        } else {
            ImGui::Text("Editing %s", objects[selected].name.c_str());
            EditSelectMode mode = GetEditSelectMode();
            if (ToggleButton("Points", mode == EditSelect_Vertex, third)) SetEditSelectMode(EditSelect_Vertex);
            ImGui::SameLine();
            if (ToggleButton("Edges", mode == EditSelect_Edge, third)) SetEditSelectMode(EditSelect_Edge);
            ImGui::SameLine();
            if (ToggleButton("Faces", mode == EditSelect_Face, third)) SetEditSelectMode(EditSelect_Face);

            bool busy = IsSceneTransformActive();
            ImGui::BeginDisabled(busy);
            ImGui::SeparatorText("1  Seams");
            Hint("Seams are where the image may split, like the seams on clothes. They show red.");
            if (ToolButton("Mark Seam", "Mark the picked edges (or the border of picked faces) as seams", half)) EditMarkSeams(sceneRenderer, true);
            ImGui::SameLine();
            if (ToolButton("Clear Seam", "Remove seams from the picked edges", half)) EditMarkSeams(sceneRenderer, false);

            ImGui::SeparatorText("2  Lay flat");
            if (ToolButton("Unwrap", "Cut along the seams and lay each piece flat with as little stretching as possible. Uses the picked faces, or all faces if none are picked.", full))
                EditUnwrapOrProject(sceneRenderer, -1);
            float quarter = (full - spacing * 3.0f) / 4.0f;
            if (ToolButton("Box", "Project from six sides, good for boxy shapes", quarter)) EditUnwrapOrProject(sceneRenderer, UVProject_Box);
            ImGui::SameLine();
            if (ToolButton("Tube", "Wrap around like a label on a can", quarter)) EditUnwrapOrProject(sceneRenderer, UVProject_Cylinder);
            ImGui::SameLine();
            if (ToolButton("Ball", "Wrap around like a globe", quarter)) EditUnwrapOrProject(sceneRenderer, UVProject_Sphere);
            ImGui::SameLine();
            if (ToolButton("View", "Project straight from the Scene camera", quarter)) EditUnwrapOrProject(sceneRenderer, UVProject_View);

            ImGui::SeparatorText("3  Arrange on the image");
            if (ToolButton("Fit", "Scale the picked pieces to fill the image", third)) EditUVArrange(sceneRenderer, 0);
            ImGui::SameLine();
            if (ToolButton("Center", "Move the picked pieces to the middle of the image", third)) EditUVArrange(sceneRenderer, 1);
            ImGui::SameLine();
            if (ToolButton("Pack", "Lay the pieces side by side without overlapping", third)) EditUVArrange(sceneRenderer, 2);
            if (ToolButton("Turn 90", "Turn the picked pieces a quarter turn", third)) EditUVArrange(sceneRenderer, 3);
            ImGui::SameLine();
            if (ToolButton("Flip Across", "Mirror left to right", third)) EditUVArrange(sceneRenderer, 4);
            ImGui::SameLine();
            if (ToolButton("Flip Down", "Mirror top to bottom", third)) EditUVArrange(sceneRenderer, 5);
            if (ToolButton("Pick Whole Pieces", "Grow the pick to the whole pieces it touches", full)) EditSelectIslands();
            ImGui::EndDisabled();

            if (const char* note = GetEditMessage()) {
                ImGui::Spacing();
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.86f, 0.35f, 1.0f));
                ImGui::TextWrapped("%s", note);
                ImGui::PopStyleColor();
            }
            ImGui::Spacing();
            Hint("Move pieces in the UV panel with G, S and R. Image repeat, offset and turn are in the Material panel.");
        }
    }
    ImGui::End();
}
