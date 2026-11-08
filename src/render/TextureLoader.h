#pragma once
#include <string>

unsigned int LoadTexture(const std::string& path);
// Loads once per path
unsigned int AcquireTexture(const std::string& path, bool retry = false);
