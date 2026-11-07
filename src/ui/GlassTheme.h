#pragma once
#include "core/UISettings.h"

void ApplyGlassStyle(const UISettings& settings);
void DrawThemeBackdrop(const UISettings& settings);
const char* GetThemeName(int index);
int GetThemeCount();
ImVec4 GetThemeAccent(int index);
