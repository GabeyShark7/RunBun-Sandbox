#pragma once
#include <string>

std::string OpenFileDialog();
std::string OpenImageFileDialog();
std::string OpenSceneFileDialog();
std::string SaveSceneFileDialog(const std::string& suggested);
std::string SaveSTLFileDialog(const std::string& suggested);
std::string SaveLogFileDialog(const std::string& suggested);
std::string SaveFileDialogFor(const std::string& title, const std::string& suggested, const std::string& extension, const std::string& description);
