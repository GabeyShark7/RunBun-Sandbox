#include "ui/RoomWindow.h"
#include "imgui/imgui.h"
#include "runbun_net.h"
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

static RbRoom* room = nullptr;
static RoomState state = Room_None;
static std::string roomKey;
static std::string hostName;
static std::vector<RoomMember> members;
static std::vector<std::string> activity;
static bool showWindow = false;
static bool openJoinPopup = false;
static bool scrollActivity = false;
static char nameBuffer[33] = "";
static char keyBuffer[64] = "";
static char chatBuffer[501] = "";
static std::string joinError;

static void Log(const std::string& line) {
    activity.push_back(line);
    if (activity.size() > 300) activity.erase(activity.begin());
    scrollActivity = true;
}

static std::vector<std::string> SplitLines(const char* text) {
    std::vector<std::string> lines;
    std::string current;
    for (const char* c = text; *c; c++) {
        if (*c == '\n') { lines.push_back(current); current.clear(); }
        else current += *c;
    }
    lines.push_back(current);
    return lines;
}

static void EnsureName() {
    if (nameBuffer[0]) return;
    const char* computer = std::getenv("COMPUTERNAME");
    std::strncpy(nameBuffer, computer ? computer : "Guest", sizeof(nameBuffer) - 1);
}

static void ResetRoom() {
    if (room) rb_room_close(room);
    room = nullptr;
    state = Room_None;
    roomKey.clear();
    hostName.clear();
    members.clear();
}

void OpenRoomCreate() {
    if (room) { showWindow = true; return; }
    EnsureName();
    activity.clear();
    room = rb_room_host(nameBuffer, 47300);
    state = Room_Connecting;
    showWindow = true;
    Log("Opening a room...");
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
        return;
    }
    activity.clear();
    room = joined;
    state = Room_Connecting;
    showWindow = true;
    ImGui::CloseCurrentPopup();
}

void ShowRoomWindow() { showWindow = true; }
bool IsInRoom() { return room != nullptr; }
bool IsHostingRoom() { return state == Room_Hosting; }

void LeaveRoom() {
    if (!room) return;
    Log(state == Room_Hosting ? "You closed the room" : "You left the room");
    ResetRoom();
}

void ShutdownRoom() { ResetRoom(); }

void UpdateRoom() {
    char buffer[2048];
    while (room) {
        int kind = rb_room_poll(room, buffer, sizeof(buffer));
        if (kind == RB_EVENT_NONE) break;

        switch (kind) {
        case RB_EVENT_HOSTING:
            state = Room_Hosting;
            roomKey = buffer;
            members.clear();
            Log("Room is open. Share the key to let people in");
            break;
        case RB_EVENT_JOINED: {
            std::vector<std::string> lines = SplitLines(buffer);
            state = Room_Joined;
            hostName = lines.size() > 1 ? lines[1] : "host";
            members.clear();
            members.push_back({ hostName, "host" });
            for (size_t i = 2; i < lines.size(); i++) members.push_back({ lines[i], "" });
            members.push_back({ lines[0], "you" });
            Log("Joined " + hostName + "'s room as " + lines[0]);
            break;
        }
        case RB_EVENT_PEER_JOINED:
            members.insert(members.end() - (state == Room_Joined ? 1 : 0), { buffer, "" });
            Log(std::string(buffer) + " joined");
            break;
        case RB_EVENT_PEER_LEFT: {
            std::string who = buffer;
            members.erase(std::remove_if(members.begin(), members.end(),
                [&](const RoomMember& m) { return m.name == who && m.tag[0] == 0; }), members.end());
            Log(who + " left");
            break;
        }
        case RB_EVENT_CHAT: {
            std::vector<std::string> lines = SplitLines(buffer);
            Log(lines[0] + ": " + (lines.size() > 1 ? lines[1] : ""));
            break;
        }
        case RB_EVENT_INFO:
            Log(buffer);
            break;
        case RB_EVENT_CLOSED:
            Log(buffer);
            ResetRoom();
            break;
        }
    }
}

static void DrawJoinPopup() {
    if (openJoinPopup) {
        ImGui::OpenPopup("Join Room");
        openJoinPopup = false;
    }
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::BeginPopupModal("Join Room", nullptr, ImGuiWindowFlags_NoResize)) return;

    ImGui::TextDisabled("Paste the key from the person hosting");
    ImGui::Spacing();
    ImGui::Text("Room Key");
    ImGui::SetNextItemWidth(-90.0f);
    if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
    bool submit = ImGui::InputTextWithHint("##RoomKey", "RB-XXXX-XXXX-XXXX-XXXX", keyBuffer, sizeof(keyBuffer),
                                           ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_CharsUppercase);
    ImGui::SameLine();
    if (ImGui::Button("Paste", ImVec2(-1.0f, 0.0f))) {
        if (const char* clip = ImGui::GetClipboardText()) std::strncpy(keyBuffer, clip, sizeof(keyBuffer) - 1);
    }
    ImGui::Text("Your Name");
    ImGui::SetNextItemWidth(-1.0f);
    submit |= ImGui::InputText("##RoomName", nameBuffer, sizeof(nameBuffer), ImGuiInputTextFlags_EnterReturnsTrue);

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

static void DrawMembers() {
    ImGui::TextDisabled("People");
    if (state == Room_Hosting) {
        ImGui::BulletText("%s", nameBuffer);
        ImGui::SameLine();
        ImGui::TextDisabled("host  you");
    }
    for (size_t i = 0; i < members.size(); i++) {
        const RoomMember& member = members[i];
        ImGui::PushID((int)i);
        ImGui::BulletText("%s", member.name.c_str());
        if (member.tag[0]) {
            ImGui::SameLine();
            ImGui::TextDisabled("%s", member.tag);
        }
        if (state == Room_Hosting) {
            float width = ImGui::CalcTextSize("Kick").x + ImGui::GetStyle().FramePadding.x * 2.0f;
            ImGui::SameLine(ImGui::GetContentRegionMax().x - width);
            if (ImGui::SmallButton("Kick")) rb_room_kick(room, member.name.c_str());
        }
        ImGui::PopID();
    }
    if (state == Room_Hosting && members.empty()) ImGui::TextDisabled("  Waiting for people to join");
}

static void DrawActivity() {
    ImGui::TextDisabled("Activity");
    float inputHeight = room && state != Room_Connecting ? ImGui::GetFrameHeightWithSpacing() : 0.0f;
    float footer = inputHeight + ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
    if (ImGui::BeginChild("RoomActivity", ImVec2(0.0f, -footer), ImGuiChildFlags_Borders)) {
        for (const std::string& line : activity) ImGui::TextWrapped("%s", line.c_str());
        if (scrollActivity) {
            ImGui::SetScrollHereY(1.0f);
            scrollActivity = false;
        }
    }
    ImGui::EndChild();

    if (inputHeight > 0.0f) {
        ImGui::SetNextItemWidth(-1.0f);
        if (ImGui::InputTextWithHint("##RoomChat", "Message the room", chatBuffer, sizeof(chatBuffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
            if (chatBuffer[0]) {
                rb_room_chat(room, chatBuffer);
                Log(std::string("You: ") + chatBuffer);
                chatBuffer[0] = 0;
            }
            ImGui::SetKeyboardFocusHere(-1);
        }
    }
}

static void DrawRoomWindow() {
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x * 0.5f, viewport->WorkPos.y + viewport->WorkSize.y * 0.5f),
                            ImGuiCond_FirstUseEver, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(380.0f, 480.0f), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("Room", &showWindow, ImGuiWindowFlags_NoCollapse)) {
        ImGui::End();
        return;
    }

    ImVec4 accent = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);
    switch (state) {
    case Room_None:
        ImGui::TextDisabled("Not in a room");
        break;
    case Room_Connecting:
        ImGui::TextColored(accent, "Connecting...");
        break;
    case Room_Hosting:
        ImGui::TextColored(accent, "Hosting a room");
        break;
    case Room_Joined:
        ImGui::TextColored(accent, "In %s's room", hostName.c_str());
        break;
    }

    if (state == Room_Hosting) {
        ImGui::Spacing();
        ImGui::TextDisabled("Room Key");
        ImGui::SetWindowFontScale(1.35f);
        ImGui::TextUnformatted(roomKey.c_str());
        ImGui::SetWindowFontScale(1.0f);
        if (ImGui::Button("Copy Key", ImVec2(-1.0f, 0.0f))) {
            ImGui::SetClipboardText(roomKey.c_str());
            Log("Key copied");
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    if (state == Room_Hosting || state == Room_Joined) {
        DrawMembers();
        ImGui::Separator();
    }
    DrawActivity();

    float half = (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    if (room) {
        const char* label = state == Room_Hosting ? "Close Room" : (state == Room_Connecting ? "Cancel" : "Leave Room");
        if (ImGui::Button(label, ImVec2(-1.0f, 0.0f))) LeaveRoom();
    } else {
        if (ImGui::Button("Create", ImVec2(half, 0.0f))) OpenRoomCreate();
        ImGui::SameLine();
        if (ImGui::Button("Join", ImVec2(half, 0.0f))) OpenRoomJoin();
    }
    ImGui::End();
}

void DrawRoomWindows() {
    DrawJoinPopup();
    if (showWindow) DrawRoomWindow();
}
