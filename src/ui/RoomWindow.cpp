#include "ui/RoomWindow.h"
#include "ui/DockLayout.h"
#include "imgui/imgui.h"
#include "runbun_net.h"
#include "ui/RoomSync.h"
#include "ui/EditMode.h"
#include "ui/Console.h"
#include "render/SceneRenderer.h"
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

enum RoomState {
    Room_None,
    Room_Connecting,
    Room_Hosting,
    Room_Joined
};

struct RoomMember {
    std::string name;
    const char* tag;
};

enum LineKind {
    Line_System,
    Line_Chat,
    Line_Mine
};

struct MessageLine {
    LineKind kind;
    std::string who;
    std::string text;
};

static RbRoom* room = nullptr;
static RoomState state = Room_None;
static std::string roomKey;
static std::string hostName;
static std::vector<RoomMember> members;
static std::vector<MessageLine> lines;
static bool showWindow = false;
static bool openJoinPopup = false;
static bool scrollMessages = false;
static char nameBuffer[33] = "";
static char keyBuffer[64] = "";
static char chatBuffer[501] = "";
static std::string joinError;
static std::string lastReason;
static SceneRenderer* roomScene = nullptr;
static bool openCreatePopup = false;
static bool openSettingsPopup = false;
static char roomNameBuffer[49] = "";
static RoomSettings draft;
static bool noLimit = false;
static bool startEmpty = false;
static bool copyKeyOnOpen = true;
static int portSetting = 47300;
static RoomSettings startSettings;
static bool clearOnOpen = false;
static bool copyOnOpen = false;
static double openedAt = 0.0;

static void Log(LineKind kind, const std::string& who, const std::string& text) {
    if (kind == Line_System) LogRoom(text);
    else LogChat(who + ": " + text);
    lines.push_back({ kind, who, text });
    if (lines.size() > 500) lines.erase(lines.begin());
    scrollMessages = true;
}

static void Note(const std::string& text) { Log(Line_System, "", text); }

static std::vector<std::string> SplitLines(const char* text) {
    std::vector<std::string> parts;
    std::string current;
    for (const char* c = text; *c; c++) {
        if (*c == '\n') { parts.push_back(current); current.clear(); }
        else current += *c;
    }
    parts.push_back(current);
    return parts;
}

static void EnsureName() {
    if (nameBuffer[0]) return;
    const char* computer = std::getenv("COMPUTERNAME");
    std::strncpy(nameBuffer, computer ? computer : "Guest", sizeof(nameBuffer) - 1);
}

static void ResetRoom() {
    RoomSyncEnd();
    if (room) rb_room_close(room);
    room = nullptr;
    state = Room_None;
    roomKey.clear();
    hostName.clear();
    members.clear();
}

static std::string Trim(const std::string& text) {
    size_t first = text.find_first_not_of(' '), last = text.find_last_not_of(' ');
    return first == std::string::npos ? std::string() : text.substr(first, last - first + 1);
}

void OpenRoomCreate() {
    if (room) { showWindow = true; return; }
    EnsureName();
    if (!roomNameBuffer[0]) std::snprintf(roomNameBuffer, sizeof(roomNameBuffer), "%s's room", Trim(nameBuffer).c_str());
    draft = RoomSettings();
    noLimit = false;
    startEmpty = false;
    openCreatePopup = true;
}

static void SendLimits(const RoomSettings& s) {
    if (room) rb_room_set_limits(room, noLimit ? 0u : (uint32_t)s.maxPeople, s.open ? 1 : 0);
}

static void StartHost() {
    lines.clear();
    lastReason.clear();
    startSettings = draft;
    startSettings.name = Trim(roomNameBuffer);
    if (startSettings.name.empty()) startSettings.name = Trim(nameBuffer) + "'s room";
    startSettings.open = true;
    clearOnOpen = startEmpty;
    copyOnOpen = copyKeyOnOpen;
    room = rb_room_host(nameBuffer, (uint16_t)portSetting);
    SendLimits(startSettings);
    state = Room_Connecting;
    showWindow = true;
    Note("Opening " + startSettings.name + "...");
    ImGui::CloseCurrentPopup();
}

void OpenRoomJoin() {
    if (room) { showWindow = true; return; }
    EnsureName();
    joinError.clear();
    openJoinPopup = true;
}

static void StartJoin() {
    char error[128] = "";
    RbRoom* joined = rb_room_join(nameBuffer, keyBuffer, error, sizeof(error));
    if (!joined) {
        joinError = error;
        LogError("Could not join: " + joinError);
        return;
    }
    lines.clear();
    lastReason.clear();
    room = joined;
    state = Room_Connecting;
    showWindow = true;
    Note("Connecting...");
    ImGui::CloseCurrentPopup();
}

void ShowRoomWindow() { showWindow = true; }
bool IsRoomWindowVisible() { return showWindow; }
bool IsInRoom() { return room != nullptr; }
bool IsHostingRoom() { return state == Room_Hosting; }

void LeaveRoom() {
    if (!room) return;
    lastReason = state == Room_Hosting ? "You closed the room" : "You left the room";
    Note(lastReason);
    ResetRoom();
}

void ShutdownRoom() { ResetRoom(); }

static std::string TrimmedName() {
    std::string name = nameBuffer;
    size_t first = name.find_first_not_of(' '), last = name.find_last_not_of(' ');
    return first == std::string::npos ? std::string("Host") : name.substr(first, last - first + 1);
}

void UpdateRoom(SceneRenderer& scene) {
    roomScene = &scene;
    RoomSyncSetSender([](const std::vector<uint8_t>& data, const char* to, const char* skip, const char* from) {
        if (room) rb_room_send_scene(room, data.data(), data.size(), to, skip, from);
    });
    char buffer[2048];
    while (room) {
        int kind = rb_room_poll(room, buffer, sizeof(buffer));
        if (kind == RB_EVENT_NONE) break;

        switch (kind) {
        case RB_EVENT_HOSTING:
            state = Room_Hosting;
            roomKey = buffer;
            members.clear();
            openedAt = ImGui::GetTime();
            if (clearOnOpen) {
                ExitEditMode();
                scene.ClearScene();
                scene.SetSelection({}, -1);
                Note("Started with an empty scene. Ctrl+Z brings your old one back");
            }
            Note(startSettings.name + " is open. Share the room key so others can join");
            if (copyOnOpen) {
                ImGui::SetClipboardText(roomKey.c_str());
                Note("Room key copied so you can paste it to friends");
            }
            RoomSyncBegin(scene, true, TrimmedName(), startSettings);
            break;
        case RB_EVENT_JOINED: {
            std::vector<std::string> parts = SplitLines(buffer);
            state = Room_Joined;
            hostName = parts.size() > 1 ? parts[1] : "host";
            members.clear();
            members.push_back({ hostName, "host" });
            for (size_t i = 2; i < parts.size(); i++) members.push_back({ parts[i], "" });
            members.push_back({ parts[0], "you" });
            Note("You joined " + hostName + "'s room as " + parts[0]);
            openedAt = ImGui::GetTime();
            RoomSyncBegin(scene, false, parts[0], RoomSettings());
            break;
        }
        case RB_EVENT_PEER_JOINED:
            members.insert(members.end() - (state == Room_Joined ? 1 : 0), { buffer, "" });
            Note(std::string(buffer) + " joined");
            RoomSyncPeerJoined(buffer);
            break;
        case RB_EVENT_PEER_LEFT: {
            std::string who = buffer;
            members.erase(std::remove_if(members.begin(), members.end(),
                [&](const RoomMember& m) { return m.name == who && m.tag[0] == 0; }), members.end());
            Note(who + " left");
            RoomSyncPeerLeft(who);
            break;
        }
        case RB_EVENT_CHAT: {
            std::vector<std::string> parts = SplitLines(buffer);
            Log(Line_Chat, parts[0], parts.size() > 1 ? parts[1] : "");
            break;
        }
        case RB_EVENT_SCENE: {
            std::vector<uint8_t> data(rb_room_scene_data(room, nullptr, 0));
            if (!data.empty()) rb_room_scene_data(room, data.data(), data.size());
            RoomSyncReceive(buffer, data);
            break;
        }
        case RB_EVENT_PROGRESS: {
            std::vector<std::string> parts = SplitLines(buffer);
            if (parts.size() >= 3) RoomSyncProgress(std::strtoull(parts[1].c_str(), nullptr, 10), std::strtoull(parts[2].c_str(), nullptr, 10));
            break;
        }
        case RB_EVENT_INFO:
            Note(buffer);
            break;
        case RB_EVENT_CLOSED:
            lastReason = buffer;
            Note(buffer);
            ResetRoom();
            break;
        }
    }
}

static void DrawJoinPopup() {
    if (openJoinPopup) {
        ImGui::OpenPopup("Join a Room");
        openJoinPopup = false;
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Join a Room", nullptr, ImGuiWindowFlags_NoResize)) return;

    ImGui::TextWrapped("Ask the person hosting for their room key, then paste it here.");
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.86f, 0.35f, 1.0f));
    ImGui::TextWrapped("Joining swaps your scene for the room's scene. Save yours first if you want to keep it.");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Text("Room key");
    ImGui::SetNextItemWidth(-90.0f);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    bool submit = ImGui::InputTextWithHint("##RoomKey", "RB-XXXX-XXXX-XXXX-XXXX", keyBuffer, sizeof(keyBuffer),
                                           ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsUppercase);
    ImGui::SameLine();
    if (ImGui::Button("Paste", ImVec2(-1.0f, 0.0f))) {
        if (const char* clip = ImGui::GetClipboardText()) std::strncpy(keyBuffer, clip, sizeof(keyBuffer) - 1);
    }
    ImGui::Text("Your name");
    ImGui::SetNextItemWidth(-1.0f);
    submit |= ImGui::InputText("##RoomName", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::TextDisabled("This is what others in the room will see");

    if (!joinError.empty()) {
        ImGui::Spacing();
        ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", joinError.c_str());
    }

    ImGui::Spacing();
    float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button("Join", ImVec2(half, 0.0f)) || submit) StartJoin();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(half, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

static void Heading(const char* text) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab));
    ImGui::TextUnformatted(text);
    ImGui::PopStyleColor();
    ImGui::Separator();
}

static void GuestsAndSize(bool creating) {
    Heading("Guests");
    int canEdit = draft.guestsCanEdit ? 1 : 0;
    ImGui::RadioButton("Can edit", &canEdit, 1);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Everyone can add, move and change shapes");
    ImGui::SameLine();
    ImGui::RadioButton("Can only look", &canEdit, 0);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Guests watch and chat but cannot change anything");
    draft.guestsCanEdit = canEdit != 0;

    ImGui::Text("Most people");
    ImGui::BeginDisabled(noLimit);
    ImGui::SetNextItemWidth(-110.0f);
    ImGui::SliderInt("##MaxPeople", &draft.maxPeople, 2, 16, "%d with you");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::Checkbox("No limit", &noLimit);
    if (!creating) {
        int here = (int)members.size() + 1;
        if (!noLimit && here > draft.maxPeople)
            ImGui::TextDisabled("%d are here now. Nobody gets removed but no one new can join", here);
        ImGui::Checkbox("Let new people join", &draft.open);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Off locks the room. People already here stay");
    }
}

static void DrawCreatePopup() {
    if (openCreatePopup) {
        ImGui::OpenPopup("Create a Room");
        openCreatePopup = false;
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Create a Room", nullptr, ImGuiWindowFlags_NoResize)) return;

    ImGui::TextWrapped("Build together with people on your Wi-Fi. You get a room key to send them.");

    Heading("Room");
    ImGui::Text("Room name");
    ImGui::SetNextItemWidth(-1.0f);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    bool submit = ImGui::InputText("##RoomTitle", roomNameBuffer, sizeof(roomNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
    ImGui::TextDisabled("Guests see this at the top of their Room panel");
    ImGui::Text("Your name");
    ImGui::SetNextItemWidth(-1.0f);
    submit |= ImGui::InputText("##HostName", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

    Heading("Scene");
    int shapes = roomScene ? (int)roomScene->GetObjects().size() : 0;
    int empty = startEmpty ? 1 : 0;
    char shareLabel[64];
    std::snprintf(shareLabel, sizeof(shareLabel), "Share my scene (%d shape%s)", shapes, shapes == 1 ? "" : "s");
    ImGui::RadioButton(shareLabel, &empty, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Start empty", &empty, 1);
    startEmpty = empty != 0;
    if (startEmpty && shapes > 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.86f, 0.35f, 1.0f));
        ImGui::TextWrapped("Your shapes get cleared for the room. Ctrl+Z brings them back, or save first.");
        ImGui::PopStyleColor();
    }

    GuestsAndSize(true);

    Heading("When it opens");
    ImGui::Checkbox("Copy the room key for me", &copyKeyOnOpen);
    if (ImGui::TreeNode("Advanced")) {
        ImGui::Text("Port");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::InputInt("##Port", &portSetting, 0)) portSetting = std::clamp(portSetting, 1024, 65535);
        ImGui::SameLine();
        if (ImGui::SmallButton("Reset")) portSetting = 47300;
        ImGui::TextDisabled("If this port is busy any free one is used");
        ImGui::TreePop();
    }

    ImGui::Spacing();
    float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button("Create Room", ImVec2(half, 0.0f)) || submit) StartHost();
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(half, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

static void DrawSettingsPopup() {
    if (openSettingsPopup) {
        ImGui::OpenPopup("Room Settings");
        openSettingsPopup = false;
        draft = RoomSyncSettings();
        std::snprintf(roomNameBuffer, sizeof(roomNameBuffer), "%s", draft.name.c_str());
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(440.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Room Settings", nullptr, ImGuiWindowFlags_NoResize)) return;
    if (state != Room_Hosting) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }

    Heading("Room");
    ImGui::Text("Room name");
    ImGui::SetNextItemWidth(-1.0f);
    bool submit = ImGui::InputText("##RoomTitle", roomNameBuffer, sizeof(roomNameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
    GuestsAndSize(false);

    ImGui::Spacing();
    float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (ImGui::Button("Save", ImVec2(half, 0.0f)) || submit) {
        draft.name = Trim(roomNameBuffer);
        if (draft.name.empty()) draft.name = RoomSyncSettings().name;
        RoomSyncSetSettings(draft);
        SendLimits(draft);
        Note("Room settings saved");
        ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel", ImVec2(half, 0.0f)) || ImGui::IsKeyPressed(ImGuiKey_Escape)) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
}

static std::string OpenFor() {
    int minutes = (int)((ImGui::GetTime() - openedAt) / 60.0);
    if (minutes < 1) return "just now";
    if (minutes < 60) return std::to_string(minutes) + " min";
    return std::to_string(minutes / 60) + " h " + std::to_string(minutes % 60) + " min";
}

static void RoomSummary() {
    const RoomSettings& s = RoomSyncSettings();
    int here = (int)members.size() + (state == Room_Hosting ? 1 : 0);
    if (noLimit && state == Room_Hosting) ImGui::TextDisabled("%d here", here);
    else ImGui::TextDisabled("%d of %d here", here, s.maxPeople);
    ImGui::SameLine();
    ImGui::TextDisabled("  open %s", OpenFor().c_str());
    if (!s.open) {
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(1.0f, 0.86f, 0.35f, 1.0f), "  locked");
    }
}

static void ColorDot(const std::string& name) {
    ImVec2 p = ImGui::GetCursorScreenPos();
    float h = ImGui::GetTextLineHeight();
    ImGui::GetWindowDrawList()->AddCircleFilled(ImVec2(p.x + h * 0.4f, p.y + h * 0.5f), h * 0.3f, RoomSyncColorFor(name));
    ImGui::Dummy(ImVec2(h * 0.9f, h));
    ImGui::SameLine();
}

static void Doing(const std::string& name) {
    for (const PeerPresence& p : RoomSyncPeers()) {
        if (p.name != name) continue;
        if (p.editing) {
            ImGui::SameLine();
            ImGui::TextDisabled("editing a shape");
        } else if (!p.selected.empty()) {
            ImGui::SameLine();
            ImGui::TextDisabled("%d picked", (int)p.selected.size());
        }
    }
}

static void DrawSyncStatus() {
    float progress = RoomSyncProgressFraction();
    if (!RoomSyncReady() && progress >= 0.0f) {
        ImGui::TextDisabled("Getting the room scene");
        ImGui::ProgressBar(progress, ImVec2(-1.0f, 0.0f));
    } else if (RoomSyncReady()) {
        ImGui::TextDisabled(RoomSyncGuestsCanEdit() || RoomSyncIsHost() ? "Scene is shared live" : "View only: the host has turned off guest editing");
    }
}

static void DrawMembers() {
    ImGui::TextDisabled("People in this room");
    if (state == Room_Hosting) {
        ColorDot(TrimmedName());
        ImGui::TextUnformatted(nameBuffer);
        ImGui::SameLine();
        ImGui::TextDisabled("host, you");
    }
    for (size_t i = 0; i < members.size(); i++) {
        const RoomMember& member = members[i];
        ImGui::PushID((int)i);
        ColorDot(member.name);
        ImGui::TextUnformatted(member.name.c_str());
        Doing(member.name);
        if (member.tag[0]) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", member.tag);
        }
        if (state == Room_Hosting) {
            float width = ImGui::CalcTextSize("Remove").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            ImGui::SameLine(ImGui::GetContentRegionMax().x - width);
            if (ImGui::SmallButton("Remove")) rb_room_kick(room, member.name.c_str());
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Remove %s from the room", member.name.c_str());
        }
        ImGui::PopID();
    }
    if (state == Room_Hosting && members.empty()) ImGui::TextDisabled("Nobody else yet");
}

static void DrawRoomWindow() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                            ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(360.0f, 360.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Room", &showWindow, PanelFlags())) {
        ImGui::End();
        return;
    }

    ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);
    float full = ImGui::GetContentRegionAvail().x;
    float half = (full - ImGui::GetStyle().ItemSpacing.x) * 0.5f;

    switch (state) {
    case Room_None:
        ImGui::TextUnformatted("You are not in a room");
        ImGui::TextWrapped("Create a room to build with others on your Wi-Fi, or join one with a room key.");
        if (!lastReason.empty()) ImGui::TextDisabled("%s", lastReason.c_str());
        ImGui::Spacing();
        if (ImGui::Button("Create Room", ImVec2(half, 0.0f))) OpenRoomCreate();
        ImGui::SameLine();
        if (ImGui::Button("Join Room", ImVec2(half, 0.0f))) OpenRoomJoin();
        break;
    case Room_Connecting:
        ImGui::TextColored(accent, "Connecting...");
        ImGui::Spacing();
        if (ImGui::Button("Cancel", ImVec2(full, 0.0f))) LeaveRoom();
        break;
    case Room_Hosting:
        ImGui::SetWindowFontScale(1.2f);
        ImGui::TextColored(accent, "%s", RoomSyncSettings().name.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextDisabled("You are hosting");
        RoomSummary();
        ImGui::Spacing();
        ImGui::TextDisabled("Room key");
        ImGui::SetWindowFontScale(1.3f);
        ImGui::TextUnformatted(roomKey.c_str());
        ImGui::SetWindowFontScale(1.0f);
        if (ImGui::Button("Copy Room Key", ImVec2(full, 0.0f))) {
            ImGui::SetClipboardText(roomKey.c_str());
            Note("Room key copied");
        }
        ImGui::TextDisabled("Send this key to people you want to join");
        ImGui::Spacing();
        {
            bool allowed = RoomSyncGuestsCanEdit();
            if (ImGui::Checkbox("Guests can edit", &allowed)) RoomSyncSetGuestsCanEdit(allowed);
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Off makes the room view only for guests");
            RoomSettings s = RoomSyncSettings();
            bool open = s.open;
            ImGui::SameLine();
            if (ImGui::Checkbox("Let people join", &open)) {
                s.open = open;
                RoomSyncSetSettings(s);
                SendLimits(s);
                Note(open ? "The room is open again" : "The room is locked. People here stay");
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Off locks the room");
            if (ImGui::Button("Room Settings...", ImVec2(full, 0.0f))) openSettingsPopup = true;
        }
        DrawSyncStatus();
        ImGui::Separator();
        DrawMembers();
        ImGui::Spacing();
        if (ImGui::Button("Close Room", ImVec2(full, 0.0f))) LeaveRoom();
        break;
    case Room_Joined:
        ImGui::SetWindowFontScale(1.2f);
        if (RoomSyncSettings().name.empty()) ImGui::TextColored(accent, "In %s's room", hostName.c_str());
        else ImGui::TextColored(accent, "%s", RoomSyncSettings().name.c_str());
        ImGui::SetWindowFontScale(1.0f);
        ImGui::TextDisabled("Hosted by %s", hostName.c_str());
        RoomSummary();
        ImGui::Spacing();
        if (RoomSyncSettings().open && ImGui::Button("Copy Room Key to Invite", ImVec2(full, 0.0f))) {
            ImGui::SetClipboardText(keyBuffer);
            Note("Room key copied");
        }
        DrawSyncStatus();
        ImGui::Separator();
        DrawMembers();
        ImGui::Spacing();
        if (ImGui::Button("Leave Room", ImVec2(full, 0.0f))) LeaveRoom();
        break;
    }
    ImGui::End();
}

static void DrawMessageLines() {
    ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);
    for (const MessageLine& line : lines) {
        if (line.kind == Line_System) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", line.text.c_str());
            ImGui::PopStyleColor();
            continue;
        }
        ImGui::TextColored(line.kind == Line_Mine ? accent : ImVec4(0.55f, 0.80f, 1.0f, 1.0f), "%s", line.who.c_str());
        ImGui::SameLine();
        ImGui::TextWrapped("%s", line.text.c_str());
    }
}

void DrawMessagesWindow(bool* open) {
    if (!ImGui::Begin("Messages", open, PanelFlags())) {
        ImGui::End();
        return;
    }

    bool live = room && state != Room_Connecting;
    float inputHeight = ImGui::GetFrameHeightWithSpacing();

    if (!live && !lines.empty())
        ImGui::TextDisabled("%s", room ? "Connecting to the room..." : "Not in a room. Join or create one to chat again");
    if (ImGui::BeginChild("MessageLines", ImVec2(0.0f, -inputHeight), ImGuiChildFlags_AlwaysUseWindowPadding)) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, live ? 1.0f : 0.55f);
        if (!live && lines.empty()) {
            // Explains the greyed state
            ImVec2 avail = ImGui::GetContentRegionAvail();
            const char* title = room ? "Connecting to the room..." : "Messages are off";
            const char* reason = room ? "Messages turn on once you are in." : "Join or create a room to chat with others. Use Room > Create or Room > Join.";
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + avail.y * 0.30f);
            ImGui::TextDisabled("%s", title);
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", reason);
            ImGui::PopStyleColor();
        } else {
            DrawMessageLines();
        }
        if (scrollMessages) {
            ImGui::SetScrollHereY(1.0f);
            scrollMessages = false;
        }
        ImGui::PopStyleVar();
    }
    ImGui::EndChild();

    ImGui::BeginDisabled(!live);
    ImGui::SetNextItemWidth(-1.0f);
    const char* hint = live ? "Type a message and press Enter" : "Not in a room";
    if (ImGui::InputTextWithHint("##MessageInput", hint, chatBuffer, sizeof(chatBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (chatBuffer[0]) {
            rb_room_chat(room, chatBuffer);
            Log(Line_Mine, "You", chatBuffer);
            chatBuffer[0] = 0;
        }
        ImGui::SetKeyboardFocusHere(-1);
    }
    ImGui::EndDisabled();
    if (!live && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("Join or create a room first (Room menu)");

    ImGui::End();
}

void DrawRoomWindows() {
    DrawJoinPopup();
    DrawCreatePopup();
    DrawSettingsPopup();
    if (showWindow && ActiveWorkspace() == Workspace_Layout) DrawRoomWindow();
}
