#define IMGUI_DEFINE_MATH_OPERATORS
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "core/UISettings.h"
#include "ui/DockLayout.h"
#include "ui/GlassTheme.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <functional>
#include <string>
#include <vector>

static UISettings* settings = nullptr;
static int failures = 0;
static int checks = 0;
static std::string currentTest;

static const char* panelNames[] = { "Scene", "Objects", "Edit", "Properties", "Messages", "Console" };
static bool* PanelFlag(const char* name) {
    std::string n = name;
    if (n == "Scene") return &settings->showScene;
    if (n == "Objects") return &settings->showObjects;
    if (n == "Edit") return &settings->showEdit;
    if (n == "Properties") return &settings->showProperties;
    if (n == "Messages") return &settings->showMessages;
    return &settings->showConsole;
}

static bool PanelVisible(const char* name) {
    if (std::string(name) == "Edit" && settings->workMode == WorkMode_Tinker) return false;
    return *PanelFlag(name);
}

static void Check(bool ok, const char* what) {
    checks++;
    if (!ok) {
        failures++;
        printf("  FAIL [%s] %s\n", currentTest.c_str(), what);
    }
}

// One app like frame
static void Frame() {
    ImGuiIO& io = ImGui::GetIO();
    io.DeltaTime = 1.0f / 60.0f;
    ApplyGlassStyle(*settings);
    ImGui::NewFrame();
    DrawMainDockSpace(*settings);
    if (settings->workspace == Workspace_Layout) {
        for (const char* name : panelNames) {
            if (!PanelVisible(name)) continue;
            PreparePanel(name);
            if (ImGui::Begin(name, PanelFlag(name), PanelFlags())) ImGui::Text("%s body", name);
            ImGui::End();
        }
    } else {
        static const char* editPanels[] = { "Scene", "Edit" };
        static const char* texturingPanels[] = { "Objects", "UV Tools", "Scene", "Material", "UV" };
        bool edit = settings->workspace == Workspace_Edit;
        const char** names = edit ? editPanels : texturingPanels;
        int count = edit ? 2 : 5;
        for (int i = 0; i < count; i++) {
            PreparePanel(names[i]);
            if (ImGui::Begin(PanelId(names[i]), nullptr, PanelFlags())) ImGui::Text("%s body", names[i]);
            ImGui::End();
        }
    }
    FinishPanelLayout();
    ImGui::EndFrame();
    ImGui::Render();
}

static void Frames(int count) { for (int i = 0; i < count; i++) Frame(); }

static ImGuiWindow* Win(const char* name) { return ImGui::FindWindowByName(name); }

static ImRect DockArea() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    float gap = ImGui::GetStyle().DockingSeparatorSize;
    return ImRect(viewport->WorkPos + ImVec2(gap, gap), viewport->WorkPos + viewport->WorkSize - ImVec2(gap, gap));
}

// Docked into the main dock space
static bool Docked(const char* name) {
    ImGuiWindow* w = Win(name);
    if (!w || !w->DockIsActive || !w->DockNode) return false;
    return ImGui::DockNodeGetRootNode(w->DockNode)->IsDockSpace();
}

static ImRect RectOf(const char* name) {
    ImGuiWindow* w = Win(name);
    return w ? w->Rect() : ImRect();
}

static ImVec2 GrabPoint(const char* name) {
    ImGuiWindow* w = Win(name);
    if (w->DockIsActive && w->DockNode && w->DockNode->TabBar) {
        ImGuiTabBar* bar = w->DockNode->TabBar;
        if (ImGuiTabItem* tab = ImGui::TabBarFindTabByID(bar, w->TabId))
            return ImVec2(bar->BarRect.Min.x + tab->Offset + ImMin(tab->Width * 0.5f, 30.0f), bar->BarRect.GetCenter().y);
    }
    return ImVec2(w->Pos.x + 30.0f, w->Pos.y + w->TitleBarHeight * 0.5f);
}

static void MouseTo(ImVec2 p) { ImGui::GetIO().AddMousePosEvent(p.x, p.y); Frame(); }

static void Drag(ImVec2 from, ImVec2 to, bool shift = false) {
    ImGuiIO& io = ImGui::GetIO();
    MouseTo(from);
    io.AddMouseButtonEvent(0, true);
    Frame();
    if (shift) { io.AddKeyEvent(ImGuiMod_Shift, true); io.AddKeyEvent(ImGuiKey_LeftShift, true); }
    for (int i = 1; i <= 24; i++) MouseTo(ImLerp(from, to, i / 24.0f));
    Frames(3);
    io.AddMouseButtonEvent(0, false);
    Frames(2);
    if (shift) { io.AddKeyEvent(ImGuiMod_Shift, false); io.AddKeyEvent(ImGuiKey_LeftShift, false); }
    Frames(4);
}

static void DragPanel(const char* name, ImVec2 to, bool shift = false) { Drag(GrabPoint(name), to, shift); }

// Target picked after the panel lifts out
static void DragPanelTo(const char* name, const std::function<ImVec2()>& target) {
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 from = GrabPoint(name);
    MouseTo(from);
    io.AddMouseButtonEvent(0, true);
    Frame();
    ImVec2 lift = from + ImVec2(0.0f, 60.0f);
    for (int i = 1; i <= 8; i++) MouseTo(ImLerp(from, lift, i / 8.0f));
    Frames(2);
    ImVec2 to = target();
    for (int i = 1; i <= 16; i++) MouseTo(ImLerp(lift, to, i / 16.0f));
    Frames(3);
    io.AddMouseButtonEvent(0, false);
    Frames(6);
}

// Point inside a rect at fractions
static ImVec2 At(const ImRect& r, float fx, float fy) { return ImVec2(r.Min.x + r.GetWidth() * fx, r.Min.y + r.GetHeight() * fy); }

static std::vector<const char*> DockedVisible() {
    std::vector<const char*> out;
    for (const char* name : panelNames)
        if (PanelVisible(name) && Docked(name)) out.push_back(name);
    return out;
}

// Docked panels never overlap
static void CheckNoOverlap() {
    std::vector<const char*> docked = DockedVisible();
    for (size_t i = 0; i < docked.size(); i++)
        for (size_t j = i + 1; j < docked.size(); j++) {
            ImGuiWindow* a = Win(docked[i]);
            ImGuiWindow* b = Win(docked[j]);
            if (a->DockNode == b->DockNode) continue;
            ImRect ra = a->Rect(), rb = b->Rect();
            ra.Expand(-1.0f);
            if (ra.Overlaps(rb)) {
                char msg[160];
                snprintf(msg, sizeof(msg), "%s overlaps %s", docked[i], docked[j]);
                Check(false, msg);
            }
        }
}

// No blank holes in dock area
static void CheckCoverage() {
    std::vector<const char*> docked = DockedVisible();
    if (docked.empty()) return;
    ImRect area = DockArea();
    float tolerance = ImGui::GetStyle().DockingSeparatorSize + 2.0f;
    int holes = 0;
    ImVec2 firstHole;
    for (int iy = 1; iy < 40; iy++)
        for (int ix = 1; ix < 60; ix++) {
            ImVec2 p = At(area, ix / 60.0f, iy / 40.0f);
            bool covered = false;
            for (const char* name : docked) {
                ImRect r = Win(name)->Rect();
                r.Expand(tolerance);
                if (r.Contains(p)) { covered = true; break; }
            }
            if (!covered) { if (!holes) firstHole = p; holes++; }
        }
    char msg[160];
    snprintf(msg, sizeof(msg), "%d blank spots in the dock area (first at %.0f, %.0f)", holes, firstHole.x, firstHole.y);
    Check(holes == 0, msg);
}

static void CheckSizes() {
    ImRect screen = ImRect(ImVec2(0, 0), ImGui::GetIO().DisplaySize);
    for (const char* name : panelNames) {
        if (!PanelVisible(name)) continue;
        ImGuiWindow* w = Win(name);
        char msg[160];
        snprintf(msg, sizeof(msg), "%s exists", name);
        Check(w != nullptr, msg);
        if (!w) continue;
        snprintf(msg, sizeof(msg), "%s has a usable size (%.0f x %.0f)", name, w->Size.x, w->Size.y);
        Check(w->Size.x >= 60.0f && w->Size.y >= 40.0f, msg);
        snprintf(msg, sizeof(msg), "%s is on screen", name);
        Check(screen.Overlaps(w->Rect()), msg);
    }
}

static void DumpNode(ImGuiDockNode* node, int depth) {
    if (!node) return;
    printf("    %*snode %08X %s pos %.0f,%.0f size %.0fx%.0f vis %d central %d windows", depth * 2, "", node->ID,
           node->IsSplitNode() ? (node->SplitAxis == ImGuiAxis_X ? "splitX" : "splitY") : "leaf", node->Pos.x, node->Pos.y,
           node->Size.x, node->Size.y, node->IsVisible, node->IsCentralNode());
    for (ImGuiWindow* w : node->Windows) printf(" %s", w->Name);
    printf("\n");
    DumpNode(node->ChildNodes[0], depth + 1);
    DumpNode(node->ChildNodes[1], depth + 1);
}

static void DumpTree() {
    ImGuiWindow* scene = Win("Scene");
    if (scene && scene->DockNode) DumpNode(ImGui::DockNodeGetRootNode(scene->DockNode), 0);
    for (const char* name : panelNames) {
        ImGuiWindow* w = Win(name);
        if (w) printf("    %s docked %d rect %.0f,%.0f %.0fx%.0f\n", name, w->DockIsActive, w->Pos.x, w->Pos.y, w->Size.x, w->Size.y);
    }
}

static void CheckLayout() {
    CheckNoOverlap();
    CheckCoverage();
    CheckSizes();
}

static void ResetDocked() {
    *settings = UISettings();
    settings->layoutVersion = 99;
    settings->layoutMode = LayoutMode_Docked;
    settings->resetLayout = true;
    ImGui::GetIO().AddMousePosEvent(5.0f, 5.0f);
    Frames(4);
    settings->layoutVersion = 99;
}

static void Run(const char* name, const std::function<void()>& body) {
    currentTest = name;
    int before = failures;
    ResetDocked();
    body();
    printf("%s %s\n", failures == before ? "PASS" : "FAIL", name);
}

static bool SameNode(const char* a, const char* b) { return Docked(a) && Docked(b) && Win(a)->DockNode == Win(b)->DockNode; }

// Close button of its node
static bool CloseButtonOf(const char* name, ImVec2& out) {
    ImGuiWindow* w = Win(name);
    if (!w) return false;
    if (w->DockIsActive && w->DockNode) {
        ImGuiDockNode* node = w->DockNode;
        if (!node->HostWindow) return false;
        ImRect bar = node->TabBar ? node->TabBar->BarRect : ImRect();
        float h = ImGui::GetFrameHeight();
        out = ImVec2(node->Pos.x + node->Size.x - h * 0.5f - ImGui::GetStyle().FramePadding.x, node->Pos.y + h * 0.5f);
        if (node->TabBar) out.y = bar.GetCenter().y;
        return true;
    }
    float h = w->TitleBarHeight;
    out = ImVec2(w->Pos.x + w->Size.x - h * 0.5f - ImGui::GetStyle().FramePadding.x, w->Pos.y + h * 0.5f);
    return true;
}

static void Click(ImVec2 p) {
    ImGuiIO& io = ImGui::GetIO();
    MouseTo(p);
    Frame();
    io.AddMouseButtonEvent(0, true);
    Frame();
    io.AddMouseButtonEvent(0, false);
    Frames(3);
}

static const char* HoveredName() {
    ImGuiWindow* w = ImGui::GetCurrentContext()->HoveredWindow;
    return w ? w->Name : "(none)";
}

// Panels close and move
static void CheckInteractive(const char* name) {
    if (!PanelVisible(name)) return;
    char msg[200];
    ImVec2 grab = GrabPoint(name);
    MouseTo(grab);
    Frame();
    ImGuiWindow* hovered = ImGui::GetCurrentContext()->HoveredWindow;
    ImGuiWindow* w = Win(name);
    bool ok = hovered && (hovered == w || hovered->RootWindowDockTree == w->RootWindowDockTree || hovered == w->RootWindowDockTree);
    snprintf(msg, sizeof(msg), "%s tab can be grabbed (mouse over %s)", name, HoveredName());
    Check(ok, msg);

    ImVec2 close;
    if (CloseButtonOf(name, close)) {
        bool* flag = PanelFlag(name);
        bool wasActiveTab = !w->DockIsActive || !w->DockNode->TabBar || w->DockNode->TabBar->SelectedTabId == w->TabId;
        if (!wasActiveTab) return;
        Click(close);
        snprintf(msg, sizeof(msg), "%s closes from its X (mouse over %s)", name, HoveredName());
        Check(!*flag, msg);
        *flag = true;
        Frames(3);
    }
}

static void RunIniRepro(const char* path) {
    currentTest = path;
    RegisterUISettingsHandler();
    ImGui::LoadIniSettingsFromDisk(path);
    settings = &GetUISettings();
    ImGui::GetIO().DisplaySize = ImVec2(1920, 971);
    ImGui::GetIO().AddMousePosEvent(5.0f, 5.0f);
    // Give hidden spots a rect
    bool objects = settings->showObjects;
    settings->showObjects = true;
    Frames(4);
    settings->showObjects = objects;
    Frames(4);
    printf("loaded %s\n", path);
    for (const char* name : panelNames) CheckInteractive(name);
    if (failures) DumpTree();
    for (const char* name : panelNames) {
        if (!PanelVisible(name)) continue;
        ImRect before = RectOf(name);
        DragPanel(name, At(DockArea(), 0.5f, 0.5f), true);
        char msg[160];
        snprintf(msg, sizeof(msg), "%s moves when dragged", name);
        Check(RectOf(name).Min.x != before.Min.x || RectOf(name).Min.y != before.Min.y, msg);
    }
    CheckLayout();
    printf("\n%d checks, %d failed\n", checks, failures);
}

int main(int argc, char** argv) {
    unsigned int seed = argc > 1 ? (unsigned)atoi(argv[1]) : 1234u;
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.DisplaySize = ImVec2(1600, 900);
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    unsigned char* pixels; int w, h;
    io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
    ImGui::GetStyle().DockingSeparatorSize = 10.0f;
    ImGui::GetStyle().WindowMenuButtonPosition = ImGuiDir_None;

    UISettings localSettings;
    settings = &localSettings;
    if (argc > 2 && std::string(argv[1]) == "ini") {
        RunIniRepro(argv[2]);
        return failures == 0 ? 0 : 1;
    }

    Run("default layout fills the window", [] {
        for (const char* name : { "Scene", "Objects", "Edit", "Properties", "Messages" }) {
            char msg[64]; snprintf(msg, sizeof(msg), "%s starts docked", name);
            Check(Docked(name), msg);
        }
        Check(!PanelVisible("Console"), "Console starts hidden");
        Check(RectOf("Objects").Max.x <= RectOf("Scene").Min.x + 1, "Objects is left of Scene");
        Check(RectOf("Properties").Min.x >= RectOf("Scene").Max.x - 1, "Properties is right of Scene");
        Check(RectOf("Messages").Min.y >= RectOf("Scene").Max.y - 1, "Messages is under Scene");
        Check(RectOf("Edit").Min.y >= RectOf("Objects").Max.y - 1, "Edit is under Objects");
        CheckLayout();
    });

    struct SideCase { const char* label; float fx, fy; ImGuiDir dir; };
    const SideCase sides[] = {
        { "left", 0.08f, 0.5f, ImGuiDir_Left }, { "right", 0.92f, 0.5f, ImGuiDir_Right },
        { "top", 0.5f, 0.12f, ImGuiDir_Up }, { "bottom", 0.5f, 0.90f, ImGuiDir_Down },
    };
    for (const SideCase& side : sides) {
        std::string title = std::string("dock Messages to the ") + side.label + " of Scene";
        Run(title.c_str(), [&] {
            DragPanelTo("Messages", [&] { return At(RectOf("Scene"), side.fx, side.fy); });
            Check(Docked("Messages"), "Messages docked");
            Check(!SameNode("Messages", "Scene"), "Messages split instead of tabbed");
            ImRect m = RectOf("Messages"), s = RectOf("Scene");
            if (side.dir == ImGuiDir_Left) Check(m.Max.x <= s.Min.x + 1, "Messages ended up left");
            if (side.dir == ImGuiDir_Right) Check(m.Min.x >= s.Max.x - 1, "Messages ended up right");
            if (side.dir == ImGuiDir_Up) Check(m.Max.y <= s.Min.y + 1, "Messages ended up on top");
            if (side.dir == ImGuiDir_Down) Check(m.Min.y >= s.Max.y - 1, "Messages ended up below");
            CheckLayout();
        });
    }

    Run("drop on the middle of Scene makes a tab", [] {
        DragPanel("Messages", At(RectOf("Scene"), 0.5f, 0.5f));
        Check(SameNode("Messages", "Scene"), "Messages tabbed with Scene");
        CheckLayout();
    });

    Run("drop on a tab bar makes a tab", [] {
        ImGuiWindow* scene = Win("Scene");
        ImVec2 target = ImVec2(scene->DockNode->TabBar->BarRect.Max.x - 60.0f, scene->DockNode->TabBar->BarRect.GetCenter().y);
        DragPanel("Objects", target);
        Check(SameNode("Objects", "Scene"), "Objects tabbed with Scene");
        CheckLayout();
    });

    Run("dock to the far right edge of the window", [] {
        ImRect area = DockArea();
        DragPanel("Objects", ImVec2(area.Max.x - 6.0f, area.GetCenter().y));
        Check(Docked("Objects"), "Objects docked");
        Check(RectOf("Objects").Max.x >= area.Max.x - 2.0f, "Objects is at the right edge");
        Check(RectOf("Objects").GetHeight() >= area.GetHeight() - 4.0f, "Objects spans the full height");
        CheckLayout();
    });

    Run("shift drag floats a panel", [] {
        DragPanel("Messages", At(RectOf("Scene"), 0.5f, 0.5f), true);
        Check(!Docked("Messages"), "Messages is floating");
        CheckLayout();
    });

    Run("floated panel can dock back", [] {
        DragPanel("Properties", At(RectOf("Scene"), 0.5f, 0.4f), true);
        Check(!Docked("Properties"), "Properties floated");
        ImRect area = DockArea();
        DragPanel("Properties", ImVec2(area.Max.x - 6.0f, area.GetCenter().y));
        Check(Docked("Properties"), "Properties docked again");
        CheckLayout();
    });

    Run("close and reopen returns to its spot", [] {
        settings->showEdit = false;
        Frames(4);
        CheckLayout();
        settings->showEdit = true;
        Frames(4);
        Check(Docked("Edit"), "Edit docked after reopening");
        Check(RectOf("Edit").Min.y >= RectOf("Objects").Max.y - 1, "Edit back under Objects");
        CheckLayout();
    });

    Run("Tinker mode hides Edit without leaving a hole", [] {
        settings->workMode = WorkMode_Tinker;
        Frames(4);
        CheckLayout();
        settings->workMode = WorkMode_Mix;
        Frames(4);
        Check(Docked("Edit"), "Edit docked again in Mix");
        CheckLayout();
    });

    Run("reopen Console docks with Messages", [] {
        settings->showConsole = true;
        Frames(4);
        Check(Docked("Console"), "Console docked");
        CheckLayout();
    });

    Run("close every side panel leaves Scene filling", [] {
        settings->showObjects = settings->showEdit = settings->showProperties = settings->showMessages = false;
        Frames(4);
        ImRect area = DockArea();
        Check(RectOf("Scene").GetWidth() >= area.GetWidth() - 4.0f, "Scene fills the width");
        Check(RectOf("Scene").GetHeight() >= area.GetHeight() - 4.0f, "Scene fills the height");
        settings->showObjects = settings->showEdit = settings->showProperties = settings->showMessages = true;
        Frames(4);
        for (const char* name : { "Objects", "Edit", "Properties", "Messages" }) {
            char msg[64]; snprintf(msg, sizeof(msg), "%s docked after reopen", name);
            Check(Docked(name), msg);
        }
        CheckLayout();
    });

    Run("moving Scene out leaves no hole", [] {
        DragPanel("Scene", At(RectOf("Properties"), 0.5f, 0.5f));
        Check(SameNode("Scene", "Properties"), "Scene tabbed with Properties");
        CheckLayout();
    });

    Run("lock layout stops drags", [] {
        settings->lockLayout = true;
        Frames(2);
        ImRect before = RectOf("Objects");
        DragPanel("Objects", At(RectOf("Scene"), 0.5f, 0.5f));
        Check(!SameNode("Objects", "Scene"), "Objects did not move");
        Check(RectOf("Objects").Min.x == before.Min.x && RectOf("Objects").Min.y == before.Min.y, "Objects rect unchanged");
        settings->lockLayout = false;
    });

    Run("splitter drag resizes neighbors", [] {
        ImRect objects = RectOf("Objects"), scene = RectOf("Scene");
        ImVec2 splitter = ImVec2((objects.Max.x + scene.Min.x) * 0.5f, scene.GetCenter().y);
        float before = objects.GetWidth();
        Drag(splitter, splitter + ImVec2(120.0f, 0.0f));
        Check(RectOf("Objects").GetWidth() > before + 60.0f, "Objects got wider");
        CheckLayout();
    });

    Run("floating layout places panels apart", [] {
        settings->layoutMode = LayoutMode_Floating;
        settings->resetLayout = true;
        Frames(4);
        for (const char* name : { "Scene", "Objects", "Properties" }) {
            char msg[64]; snprintf(msg, sizeof(msg), "%s floats", name);
            Check(!Docked(name), msg);
        }
        CheckSizes();
        settings->layoutMode = LayoutMode_Docked;
    });

    Run("pull one tab out of a tab group", [] {
        settings->showConsole = true;
        Frames(4);
        Check(SameNode("Console", "Messages"), "Console and Messages share a spot");
        DragPanelTo("Console", [] { return At(RectOf("Scene"), 0.92f, 0.5f); });
        Check(Docked("Console") && !SameNode("Console", "Messages"), "Console split out on its own");
        Check(Docked("Messages"), "Messages stayed docked");
        CheckLayout();
    });

    Run("split a panel against its own spot", [] {
        settings->showConsole = true;
        Frames(4);
        DragPanelTo("Console", [] { return At(RectOf("Messages"), 0.92f, 0.5f); });
        Check(Docked("Console") && !SameNode("Console", "Messages"), "Console beside Messages");
        Check(RectOf("Console").Min.x >= RectOf("Messages").Max.x - 1, "Console is on the right");
        CheckLayout();
    });

    Run("shrinking the window keeps panels usable", [] {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize = ImVec2(1024, 640);
        Frames(4);
        CheckLayout();
        io.DisplaySize = ImVec2(800, 520);
        Frames(4);
        CheckNoOverlap();
        CheckCoverage();
        io.DisplaySize = ImVec2(1600, 900);
        Frames(4);
        CheckLayout();
    });

    Run("floating group docks back as one", [] {
        DragPanel("Messages", At(RectOf("Scene"), 0.5f, 0.5f), true);
        DragPanel("Properties", At(RectOf("Scene"), 0.3f, 0.3f), true);
        DragPanel("Properties", At(RectOf("Messages"), 0.5f, 0.6f));
        ImGuiWindow* m = Win("Messages");
        ImGuiWindow* p = Win("Properties");
        Check(m->DockNode && p->DockNode && ImGui::DockNodeGetRootNode(m->DockNode) == ImGui::DockNodeGetRootNode(p->DockNode), "Properties joined Messages while floating");
        ImRect area = DockArea();
        DragPanel("Messages", ImVec2(area.Min.x + 6.0f, area.GetCenter().y));
        Check(Docked("Messages"), "Messages docked back");
        if (!Docked("Messages")) { ImGuiWindow* mw = Win("Messages"); printf("    messages dockactive %d node %p pos %.0f,%.0f size %.0fx%.0f grab %.0f,%.0f\n", mw->DockIsActive, (void*)mw->DockNode, mw->Pos.x, mw->Pos.y, mw->Size.x, mw->Size.y, GrabPoint("Messages").x, GrabPoint("Messages").y); DumpNode(mw->DockNode ? ImGui::DockNodeGetRootNode(mw->DockNode) : nullptr, 0); }
        CheckNoOverlap();
        CheckSizes();
    });

    Run("each tab keeps its own layout", [] {
        ImRect layoutScene = RectOf("Scene"), layoutObjects = RectOf("Objects");
        settings->workspace = Workspace_Edit;
        Frames(4);
        Check(!Win("Scene")->Active, "Layout panels hide on the Edit tab");
        Check(Docked("Scene##Edit") && Docked("Edit##Edit"), "Edit tab panels docked");
        Check(RectOf("Edit##Edit").Max.x <= RectOf("Scene##Edit").Min.x + 1, "Edit panel left of Scene");
        Check(RectOf("Scene##Edit").GetWidth() > DockArea().GetWidth() * 0.7f, "Edit tab Scene is big");
        ImRect area = DockArea();
        DragPanel("Edit##Edit", ImVec2(area.Max.x - 6.0f, area.GetCenter().y));
        Check(RectOf("Edit##Edit").Min.x >= RectOf("Scene##Edit").Max.x - 1, "Edit panel moved right");

        settings->workspace = Workspace_Texturing;
        Frames(4);
        Check(!Win("Scene##Edit")->Active, "Edit tab panels hide on Texturing");
        const char* tex[] = { "Objects##Texturing", "UV Tools##Texturing", "Scene##Texturing", "Material##Texturing", "UV##Texturing" };
        for (const char* name : tex) {
            char msg[160];
            snprintf(msg, sizeof(msg), "%s docked", name);
            Check(Docked(name), msg);
            snprintf(msg, sizeof(msg), "%s has room", name);
            Check(RectOf(name).GetWidth() >= 120.0f && RectOf(name).GetHeight() >= 120.0f, msg);
        }
        Check(RectOf("Material##Texturing").Max.y <= RectOf("UV##Texturing").Min.y + 1, "Material above UV");

        settings->workspace = Workspace_Layout;
        Frames(4);
        Check(Docked("Scene") && Docked("Objects"), "Layout panels docked again");
        Check(RectOf("Scene").Min.x == layoutScene.Min.x && RectOf("Scene").GetWidth() == layoutScene.GetWidth(), "Layout Scene unchanged");
        Check(RectOf("Objects").Min.y == layoutObjects.Min.y && RectOf("Objects").GetHeight() == layoutObjects.GetHeight(), "Layout Objects unchanged");
        CheckLayout();

        settings->workspace = Workspace_Edit;
        Frames(4);
        Check(RectOf("Edit##Edit").Min.x >= RectOf("Scene##Edit").Max.x - 1, "Edit tab remembers the move");
        settings->resetLayout = true;
        Frames(4);
        Check(RectOf("Edit##Edit").Max.x <= RectOf("Scene##Edit").Min.x + 1, "Reset puts Edit panel back");
        settings->workspace = Workspace_Layout;
        Frames(4);
        CheckLayout();
    });

    Run("floating works on every tab", [] {
        settings->layoutMode = LayoutMode_Floating;
        settings->resetLayout = true;
        for (int ws : { Workspace_Edit, Workspace_Texturing, Workspace_Layout }) {
            settings->workspace = ws;
            Frames(4);
        }
        CheckSizes();
        for (const char* name : { "Scene", "Objects" }) Check(!Docked(name), "Layout panels float");
        settings->workspace = Workspace_Edit;
        Frames(4);
        Check(!Docked("Scene##Edit") && !Docked("Edit##Edit"), "Edit tab panels float");
        Check(!RectOf("Scene##Edit").Overlaps(RectOf("Edit##Edit")), "Edit tab floats do not overlap");
        settings->workspace = Workspace_Layout;
        settings->layoutMode = LayoutMode_Docked;
        settings->resetLayout = true;
        Frames(4);
    });

    Run("random drags keep the layout sound", [seed] {
        srand(seed);
        const char* movable[] = { "Scene", "Objects", "Edit", "Properties", "Messages" };
        for (int step = 0; step < 150; step++) {
            const char* who = movable[rand() % 5];
            const char* onto = movable[rand() % 5];
            if (!Win(who) || !Win(onto)) continue;
            ImVec2 target = At(RectOf(onto), (rand() % 100) / 100.0f, (rand() % 100) / 100.0f);
            int action = rand() % 10;
            if (action == 0) {
                bool* flag = PanelFlag(who);
                *flag = !*flag;
                Frames(3);
            } else if (PanelVisible(who)) {
                DragPanel(who, target, action == 1);
            }
            int before = failures;
            CheckNoOverlap();
            CheckSizes();
            for (const char* name : panelNames) {
                if (!PanelVisible(name) || !Docked(name)) continue;
                MouseTo(GrabPoint(name));
                ImGuiWindow* hovered = ImGui::GetCurrentContext()->HoveredWindow;
                char msg[160];
                snprintf(msg, sizeof(msg), "%s tab can be grabbed (mouse over %s)", name, HoveredName());
                bool coveredByFloat = hovered && hovered->RootWindowDockTree != Win(name)->RootWindowDockTree &&
                    !(hovered->RootWindowDockTree->DockNodeAsHost && hovered->RootWindowDockTree->DockNodeAsHost->IsDockSpace());
                Check(hovered && (coveredByFloat || hovered->RootWindowDockTree == Win(name)->RootWindowDockTree), msg);
            }
            if (DockedVisible().size() == 5) CheckCoverage();
            if (failures != before) {
                printf("  after step %d: %s onto %s action %d\n", step, who, onto, action);
                DumpTree();
                break;
            }
        }
    });

    printf("\n%d checks, %d failed\n", checks, failures);
    ImGui::DestroyContext();
    return failures == 0 ? 0 : 1;
}
