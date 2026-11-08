#pragma once
#include "imgui/imgui.h"
#include <glm/glm.hpp>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

class SceneRenderer;

struct PeerPresence {
    std::string name;
    ImU32 color = 0;
    std::vector<unsigned int> selected;
    unsigned int editing = 0;
    uint32_t version = 0;
    std::vector<std::pair<int, int>> locks;
};

struct RoomSettings {
    std::string name;
    bool guestsCanEdit = true;
    int maxPeople = 8;
    bool open = true;
};

typedef std::function<void(const std::vector<uint8_t>& data, const char* to, const char* skip, const char* from)> RoomSender;

void RoomSyncSetSender(const RoomSender& sender);
void RoomSyncBegin(SceneRenderer& scene, bool host, const std::string& myName, const RoomSettings& settings);
void RoomSyncEnd();
void RoomSyncPeerJoined(const std::string& name);
void RoomSyncPeerLeft(const std::string& name);
void RoomSyncReceive(const std::string& from, const std::vector<uint8_t>& data);
void RoomSyncProgress(uint64_t received, uint64_t total);
void RoomSyncUpdate(SceneRenderer& scene);

bool RoomSyncActive();
bool RoomSyncReady();
float RoomSyncProgressFraction();
bool RoomSyncIsHost();
bool RoomSyncGuestsCanEdit();
void RoomSyncSetGuestsCanEdit(bool allowed);
const RoomSettings& RoomSyncSettings();
void RoomSyncSetSettings(const RoomSettings& settings);
const std::vector<PeerPresence>& RoomSyncPeers();
ImU32 RoomSyncColorFor(const std::string& name);
bool RoomSyncLockedBy(unsigned int id, std::string& who);
void DrawRoomPresence(SceneRenderer& scene, ImDrawList* drawList, const glm::mat4& viewProj, ImVec2 viewMin, ImVec2 viewSize);
