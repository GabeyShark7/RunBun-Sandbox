#include "ui/UpdateWindow.h"
#include "core/Updater.h"
#include "imgui/imgui.h"
#include <cstdio>
#include <cstring>

static bool showWindow = false;
static char tokenBuffer[256] = "";
static bool tokenEditing = false;

void ShowUpdateWindow() {
    showWindow = true;
    if (GetUpdateState() == Update_Idle) CheckForUpdates();
}

static const char* BuildLabel(int build, char* buffer, size_t size) {
    if (build <= 0) return "Dev build";
    snprintf(buffer, size, "Build %d", build);
    return buffer;
}

// Returns its left edge
float DrawUpdateBadge(float rightEdge) {
    UpdateState state = GetUpdateState();
    const char* label = nullptr;
    if (state == Update_Available) label = "Update";
    else if (state == Update_Downloading) label = "Downloading...";
    else if (state == Update_Ready) label = "Restart to Update";
    if (!label) return rightEdge;

    ImVec2 padding = ImVec2(12.0f, 4.0f);
    float width = ImGui::CalcTextSize(label).x + padding.x * 2.0f;
    float height = ImGui::GetFontSize() + padding.y * 2.0f;
    float left = rightEdge - width;
    ImGui::SetCursorPos(ImVec2(left, (ImGui::GetWindowHeight() - height) * 0.5f));

    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, padding);
    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 1.0f, 1.0f));
    if (ImGui::Button(label)) {
        if (state == Update_Ready) RestartIntoUpdate();
        else showWindow = true;
    }
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar();
    return left;
}

void DrawUpdateWindow() {
    if (!showWindow) return;
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);
    if (!ImGui::Begin("Updates", &showWindow, ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::End();
        return;
    }

    char current[32], latest[32];
    UpdateState state = GetUpdateState();
    ImGui::Text("You have: %s", BuildLabel(GetCurrentBuild(), current, sizeof(current)));
    if (GetLatestBuild() > 0) ImGui::Text("Newest: %s", BuildLabel(GetLatestBuild(), latest, sizeof(latest)));
    ImGui::Spacing();

    ImVec4 color = state == Update_Failed ? ImVec4(1.0f, 0.45f, 0.45f, 1.0f) : ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled);
    if (state == Update_Available || state == Update_Ready) color = ImGui::GetStyleColorVec4(ImGuiCol_SliderGrab);
    ImGui::TextColored(color, "%s", state == Update_Idle ? "Not checked yet" : GetUpdateMessage().c_str());
    ImGui::Spacing();

    float width = 380.0f;
    bool busy = state == Update_Checking || state == Update_Downloading;
    ImGui::BeginDisabled(busy);
    if (state == Update_Available) {
        if (ImGui::Button("Download Update", ImVec2(width, 0.0f))) DownloadUpdate();
    } else if (state == Update_Ready) {
        if (ImGui::Button("Restart to Update", ImVec2(width, 0.0f))) RestartIntoUpdate();
    } else {
        if (ImGui::Button("Check for Updates", ImVec2(width, 0.0f))) CheckForUpdates();
    }
    ImGui::EndDisabled();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::TextDisabled(HasUpdateToken() ? "Access token is set" : "Needs a GitHub access token");
    if (!tokenEditing) {
        if (ImGui::SmallButton(HasUpdateToken() ? "Change Token" : "Add Token")) {
            tokenEditing = true;
            tokenBuffer[0] = 0;
        }
    } else {
        ImGui::SetNextItemWidth(width);
        ImGui::InputTextWithHint("##UpdateToken", "github_pat_...", tokenBuffer, sizeof(tokenBuffer), ImGuiInputTextFlags_Password);
        if (ImGui::Button("Save", ImVec2((width - ImGui::GetStyle().ItemSpacing.x) * 0.5f, 0.0f))) {
            SetUpdateToken(tokenBuffer);
            tokenEditing = false;
            CheckForUpdates();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancel", ImVec2((width - ImGui::GetStyle().ItemSpacing.x) * 0.5f, 0.0f))) tokenEditing = false;
    }
    ImGui::End();
}
