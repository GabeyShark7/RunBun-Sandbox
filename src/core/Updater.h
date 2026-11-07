#pragma once
#include <string>

enum UpdateState {
    Update_Idle,
    Update_Checking,
    Update_UpToDate,
    Update_Available,
    Update_Downloading,
    Update_Ready,
    Update_Failed
};

int GetCurrentBuild();
int GetLatestBuild();
UpdateState GetUpdateState();
std::string GetUpdateMessage();

bool HasUpdateToken();
std::string GetUpdateToken();
void SetUpdateToken(const std::string& token);

void CheckForUpdates();
void DownloadUpdate();
void RestartIntoUpdate();
bool UpdateWantsExit();
