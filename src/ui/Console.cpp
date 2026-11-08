#include <filesystem>
#include "ui/Console.h"
#include "imgui/imgui.h"
#include "ui/DockLayout.h"
#include "ui/PanelHeader.h"
#include "ui/FileDialog.h"
#include <cctype>
#include <ctime>
#include <deque>
#include <fstream>
#include <vector>

struct ConsoleLine {
    LogKind kind;
    std::string time;
    std::string text;
    int repeats = 1;
    double at = 0.0;
};

static std::deque<ConsoleLine> consoleMessages;
static PanelCollapseState collapseState = { false, 200.0f };
static bool showKind[Log_KindCount] = { true, true, true, true, true, true };
static int kindCount[Log_KindCount] = {};
static char searchBuffer[128] = "";
static bool autoScroll = true;
static bool scrollToEnd = false;
static unsigned int successCount = 0;
static const size_t maxLines = 5000;

static const char* kindNames[Log_KindCount] = { "Info", "Done", "Warning", "Error", "Chat", "Room" };

static ImVec4 KindColor(LogKind kind) {
    switch (kind) {
    case Log_Success: return ImVec4(0.45f, 0.85f, 0.45f, 1.0f);
    case Log_Warning: return ImVec4(1.0f, 0.80f, 0.30f, 1.0f);
    case Log_Error: return ImVec4(1.0f, 0.40f, 0.40f, 1.0f);
    case Log_Chat: return ImVec4(0.55f, 0.80f, 1.0f, 1.0f);
    case Log_Room: return ImVec4(0.80f, 0.60f, 1.0f, 1.0f);
    default: return ImVec4(0.75f, 0.75f, 0.78f, 1.0f);
    }
}

static std::string Clock() {
    std::time_t now = std::time(nullptr);
    std::tm local{};
    localtime_s(&local, &now);
    char text[16];
    std::strftime(text, sizeof(text), "%H:%M:%S", &local);
    return text;
}

void Log(LogKind kind, const std::string& message) {
    if (message.empty()) return;
    if (kind == Log_Success) successCount++;
    double now = ImGui::GetCurrentContext() ? ImGui::GetTime() : 0.0;
    // Fold quick repeats
    if (!consoleMessages.empty()) {
        ConsoleLine& last = consoleMessages.back();
        if (last.kind == kind && last.text == message && now - last.at < 3.0) {
            last.repeats++;
            last.at = now;
            last.time = Clock();
            scrollToEnd = autoScroll;
            return;
        }
    }
    consoleMessages.push_back({ kind, Clock(), message, 1, now });
    kindCount[kind]++;
    if (consoleMessages.size() > maxLines) {
        kindCount[consoleMessages.front().kind]--;
        consoleMessages.pop_front();
    }
    scrollToEnd = autoScroll;
}

void LogMessage(const std::string& message) { Log(Log_Info, message); }
void LogSuccess(const std::string& message) { Log(Log_Success, message); }
void LogWarning(const std::string& message) { Log(Log_Warning, message); }
void LogError(const std::string& message) { Log(Log_Error, message); }
void LogChat(const std::string& message) { Log(Log_Chat, message); }
void LogRoom(const std::string& message) { Log(Log_Room, message); }
unsigned int LogSuccessCount() { return successCount; }

static std::string Lower(std::string text) {
    for (char& c : text) c = (char)std::tolower((unsigned char)c);
    return text;
}

static std::string LineText(const ConsoleLine& line) {
    std::string text = "[" + line.time + "] " + kindNames[line.kind] + ": " + line.text;
    if (line.repeats > 1) text += " (x" + std::to_string(line.repeats) + ")";
    return text;
}

static std::string AllText(const std::vector<const ConsoleLine*>& shown) {
    std::string text;
    for (const ConsoleLine* line : shown) text += LineText(*line) + "\n";
    return text;
}

void DrawConsoleWindow(bool* open) {
    if (!ImGui::Begin("Console", open, PanelFlags())) {
        ImGui::End();
        return;
    }

    if (DrawPanelHeader(collapseState)) {
        for (int k = 0; k < Log_KindCount; k++) {
            if (k > 0) ImGui::SameLine();
            ImGui::PushID(k);
            ImVec4 color = KindColor((LogKind)k);
            ImGui::PushStyleColor(ImGuiCol_Text, showKind[k] ? color : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            char label[48];
            std::snprintf(label, sizeof(label), "%s %d", kindNames[k], kindCount[k]);
            if (ImGui::SmallButton(label)) showKind[k] = !showKind[k];
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(showKind[k] ? "Click to hide these" : "Click to show these");
            ImGui::PopID();
        }

        ImGui::SetNextItemWidth(-260.0f);
        ImGui::InputTextWithHint("##ConsoleSearch", "Search the log", searchBuffer, sizeof(searchBuffer));
        ImGui::SameLine();
        ImGui::Checkbox("Follow", &autoScroll);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Keep showing the newest lines");

        std::string needle = Lower(searchBuffer);
        std::vector<const ConsoleLine*> shown;
        shown.reserve(consoleMessages.size());
        for (const ConsoleLine& line : consoleMessages) {
            if (!showKind[line.kind]) continue;
            if (!needle.empty() && Lower(line.text).find(needle) == std::string::npos) continue;
            shown.push_back(&line);
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("Copy")) ImGui::SetClipboardText(AllText(shown).c_str());
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Copy the lines shown");
        ImGui::SameLine();
        if (ImGui::SmallButton("Save")) {
            std::string path = SaveLogFileDialog("runbun-log.txt");
            if (!path.empty()) {
                std::ofstream file(std::filesystem::u8path(path));
                file << AllText(shown);
                if (file.good()) LogSuccess("Saved the log to '" + path + "'.");
                else LogError("Could not save the log to '" + path + "'.");
            }
        }
        ImGui::SameLine();
        if (ImGui::SmallButton("Clear")) {
            consoleMessages.clear();
            for (int& count : kindCount) count = 0;
            shown.clear();
        }

        if (ImGui::BeginChild("ConsoleLines", ImVec2(0.0f, 0.0f), ImGuiChildFlags_Borders, ImGuiWindowFlags_HorizontalScrollbar)) {
            if (shown.empty()) ImGui::TextDisabled(consoleMessages.empty() ? "Nothing has happened yet" : "No lines match");
            ImGuiListClipper clipper;
            clipper.Begin((int)shown.size());
            while (clipper.Step()) {
                for (int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++) {
                    const ConsoleLine& line = *shown[i];
                    ImGui::TextDisabled("%s", line.time.c_str());
                    ImGui::SameLine();
                    ImGui::TextColored(KindColor(line.kind), "%-7s", kindNames[line.kind]);
                    ImGui::SameLine();
                    if (line.kind == Log_Error || line.kind == Log_Warning) ImGui::PushStyleColor(ImGuiCol_Text, KindColor(line.kind));
                    if (line.repeats > 1) ImGui::Text("%s  x%d", line.text.c_str(), line.repeats);
                    else ImGui::TextUnformatted(line.text.c_str());
                    if (line.kind == Log_Error || line.kind == Log_Warning) ImGui::PopStyleColor();
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) ImGui::SetClipboardText(line.text.c_str());
                }
            }
            if (scrollToEnd) {
                ImGui::SetScrollHereY(1.0f);
                scrollToEnd = false;
            }
        }
        ImGui::EndChild();
    }

    ImGui::End();
}
