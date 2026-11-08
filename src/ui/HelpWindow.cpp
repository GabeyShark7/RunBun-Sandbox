#include "ui/HelpWindow.h"
#include "imgui/imgui.h"

static bool showWindow = false;

struct Shortcut {
    const char* keys;
    const char* action;
};

void ShowHelpWindow() { showWindow = true; }

static void ShortcutTable(const char* id, const Shortcut* shortcuts, int count) {
    if (!ImGui::BeginTable(id, 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_PadOuterX)) return;
    ImGui::TableSetupColumn("Keys", ImGuiTableColumnFlags_WidthFixed, 190.0f);
    ImGui::TableSetupColumn("Action", ImGuiTableColumnFlags_WidthStretch);
    ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);
    for (int i = 0; i < count; i++) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextColored(accent, "%s", shortcuts[i].keys);
        ImGui::TableNextColumn();
        ImGui::TextWrapped("%s", shortcuts[i].action);
    }
    ImGui::EndTable();
}

static void Section(const char* title) {
    ImGui::Spacing();
    ImGui::SeparatorText(title);
}

void DrawHelpWindow() {
    if (!showWindow) return;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(620.0f, 640.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Help", &showWindow, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    if (ImGui::BeginTabBar("HelpTabs")) {
        if (ImGui::BeginTabItem("Getting Started")) {
            ImGui::TextWrapped("RunBun Sandbox is a simple 3D building tool. Here is the basic flow:");
            ImGui::Spacing();
            ImGui::BulletText("Add a shape: Ctrl+A or Ctrl+Right-click in the Scene.");
            ImGui::BulletText("Select it: left-click it in the Scene or in Objects.");
            ImGui::BulletText("Move it: drag a colored arrow, or press G.");
            ImGui::BulletText("Fine tune it: type exact numbers in Properties.");
            ImGui::BulletText("Look around: hold Right-click and use W A S D.");
            ImGui::BulletText("Made a mistake? Ctrl+Z undoes it.");
            ImGui::BulletText("Keep your work: File > Save Scene (Ctrl+S).");
            Section("Tabs");
            ImGui::TextWrapped("The tabs at the top swap the whole screen to suit the job:");
            ImGui::BulletText("Layout: every panel at once.");
            ImGui::BulletText("Edit: a big view for reshaping one object.");
            ImGui::BulletText("Texturing: colors, images and how they wrap.");
            Section("Modes");
            ImGui::TextWrapped("Tinker keeps things simple: shapes, moving, scaling and rotating.");
            ImGui::TextWrapped("Mix adds Edit Mode so you can reshape objects by their points, edges and faces.");
            ImGui::TextDisabled("Change modes in View > Preferences > General.");
            Section("Directions");
            ImGui::TextWrapped("Z points up. X (red) and Y (green) lie flat on the ground grid. Sizes are in millimeters.");
            Section("Rooms");
            ImGui::TextWrapped("Room > Create opens a room on your PC and gives you a room key. Others on your Wi-Fi use Room > Join with that key. Chat in the Messages panel.");
            ImGui::BulletText("Everyone sees and builds the same scene live.");
            ImGui::BulletText("Joining swaps your scene for the room's scene.");
            ImGui::BulletText("Colored outlines show what others have picked.");
            ImGui::BulletText("A shape someone is editing is locked for everyone else.");
            ImGui::BulletText("Ctrl+Z only undoes your own changes.");
            ImGui::BulletText("The host can make the room view only.");
            ImGui::BulletText("When the room closes, everyone keeps the scene.");
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Shortcuts")) {
            static const Shortcut camera[] = {
                { "Hold Right-click", "Look around" },
                { "W A S D", "Fly while looking around" },
                { "Shift", "Fly faster" },
                { "F", "Focus on what is selected" },
                { "Shift+Alt+Z", "Look from the top (again for bottom)" },
                { "Shift+Alt+Y", "Look from the front (again for back)" },
                { "Shift+Alt+X", "Look from the side (again for other side)" },
            };
            static const Shortcut objects[] = {
                { "Left-click", "Select an object" },
                { "Shift+Left-click", "Select more than one" },
                { "A", "Select all or none (Scene)" },
                { "Ctrl+D", "Duplicate, then move it" },
                { "J", "Join the picked objects into one" },
                { "Right-click", "Quick menu for what is under the mouse" },
                { "Ctrl+G", "Group (holes cut into shapes)" },
                { "Ctrl+Shift+G", "Ungroup" },
                { "H", "Switch between solid and hole" },
                { "D", "Drop to the ground" },
                { "L / M", "Align / Mirror" },
                { "Arrow keys", "Nudge (Shift x10, Ctrl up or down)" },
                { "F2 or double-click", "Rename (Objects panel)" },
                { "Delete", "Delete the selected objects" },
                { "Ctrl+A", "Add a shape" },
                { "Ctrl+Right-click", "Add a shape (Scene)" },
                { "Drag an arrow", "Move along that direction" },
            };
            static const Shortcut transform[] = {
                { "G", "Move freely" },
                { "S", "Scale" },
                { "R", "Rotate" },
                { "X  Y  Z", "While moving: lock to one direction" },
                { "Hold Ctrl", "Snap (5 degrees when rotating)" },
                { "Left-click or Enter", "Confirm" },
                { "Esc or Right-click", "Cancel" },
            };
            static const Shortcut edit[] = {
                { "E", "Go to the Edit tab and start editing" },
                { "Tab", "Quick edit here, or finish editing" },
                { "1", "Select points (vertices)" },
                { "2", "Select edges" },
                { "3", "Select faces" },
                { "Left-click", "Select one" },
                { "Shift+Left-click", "Add to the selection" },
                { "A", "Select all or none" },
                { "G  S  R", "Move, scale or rotate the selection" },
                { "Drag", "Box pick (Shift adds, Ctrl removes)" },
                { "Alt+Z", "See through: pick hidden points too" },
                { "Shift while moving", "Snap onto nearby points" },
            };
            static const Shortcut tools[] = {
                { "E", "Extrude: pull faces or open edges out" },
                { "I", "Inset: smaller face inside each face" },
                { "Ctrl+B", "Bevel the picked edges" },
                { "Ctrl+R", "Loop cut (scroll for more cuts)" },
                { "M", "Merge picked points into one" },
                { "F", "Join faces (Faces) or fill a hole (Points)" },
                { "X", "Dissolve points or edges (Faces: delete)" },
                { "Delete", "Delete what is picked, leaving a hole" },
            };
            static const Shortcut file[] = {
                { "Ctrl+Z", "Undo" },
                { "Ctrl+Y or Ctrl+Shift+Z", "Redo" },
                { "Ctrl+S", "Save the scene" },
                { "Ctrl+Shift+S", "Save as a new file" },
                { "Ctrl+O", "Open a scene" },
                { "Ctrl+N", "Start a new scene" },
                { "Ctrl+1  2  3", "Layout, Edit or Texturing tab" },
            };
            Section("General");
            ShortcutTable("FileKeys", file, IM_ARRAYSIZE(file));
            Section("Camera");
            ShortcutTable("CameraKeys", camera, IM_ARRAYSIZE(camera));
            Section("Objects");
            ShortcutTable("ObjectKeys", objects, IM_ARRAYSIZE(objects));
            Section("Move, Scale, Rotate");
            ShortcutTable("TransformKeys", transform, IM_ARRAYSIZE(transform));
            Section("Edit Mode");
            ShortcutTable("EditKeys", edit, IM_ARRAYSIZE(edit));
            Section("Shape Tools (Edit Mode)");
            ShortcutTable("ToolKeys", tools, IM_ARRAYSIZE(tools));
            ImGui::EndTabItem();
        }

        if (ImGui::BeginTabItem("Panels")) {
            ImGui::TextWrapped("Every panel has a tab with its name. Drag a tab onto another panel to stack them as tabs, or onto an edge to split the space.");
            ImGui::Spacing();
            ImGui::BulletText("Scene: the 3D view.");
            ImGui::BulletText("Objects: everything in your scene.");
            ImGui::BulletText("Properties: exact position, rotation, size and look.");
            ImGui::BulletText("Edit: reshape the selected object (Mix mode).");
            ImGui::BulletText("Messages: chat with people in your room.");
            ImGui::BulletText("Console: technical log, hidden by default.");
            ImGui::BulletText("Material (Texturing tab): color and images.");
            ImGui::BulletText("UV (Texturing tab): how the image lays on the object. Edit it with G S R.");
            ImGui::BulletText("UV Tools (Texturing tab): seams, unwrap and arranging pieces.");
            Section("Tips");
            ImGui::BulletText("View > Lock Layout stops panels moving by accident.");
            ImGui::BulletText("View > Reset Layout puts everything back.");
            ImGui::BulletText("Closed a panel? Reopen it from the View menu.");
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    ImGui::End();
}
