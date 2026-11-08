#pragma once
#include "core/UISettings.h"

void DrawMainDockSpace(UISettings& settings);
void PreparePanel(const char* name);
void PreparePanelNear(const char* name, const char* buddy);
void FinishPanelLayout();
ImGuiWindowFlags PanelFlags();
int ActiveWorkspace();
const char* WorkspaceTitle(int workspace);
const char* PanelId(const char* name);
