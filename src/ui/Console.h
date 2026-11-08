#pragma once
#include <string>

enum LogKind {
    Log_Info,
    Log_Success,
    Log_Warning,
    Log_Error,
    Log_Chat,
    Log_Room,
    Log_KindCount
};

void Log(LogKind kind, const std::string& message);
void LogMessage(const std::string& message);
void LogSuccess(const std::string& message);
void LogWarning(const std::string& message);
void LogError(const std::string& message);
void LogChat(const std::string& message);
void LogRoom(const std::string& message);
unsigned int LogSuccessCount();
void DrawConsoleWindow(bool* open);
