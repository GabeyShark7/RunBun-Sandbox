#pragma once

void OpenRoomCreate();
void OpenRoomJoin();
void ShowRoomWindow();
bool IsRoomWindowVisible();
void LeaveRoom();
bool IsInRoom();
bool IsHostingRoom();

class SceneRenderer;
void UpdateRoom(SceneRenderer& scene);
void DrawRoomWindows();
void DrawMessagesWindow(bool* open);
void ShutdownRoom();
