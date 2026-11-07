#include "core/Updater.h"
#include "BuildInfo.h"
#include <atomic>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <thread>
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>

namespace fs = std::filesystem;

static const wchar_t* API_HOST = L"api.github.com";
static const char* REPO_PATH = "/repos/GabeyShark7/RunBun-Test";
static const char* ASSET_NAME = "RunBunSandbox.zip";

static std::atomic<int> state{ Update_Idle };
static std::atomic<int> latestBuild{ 0 };
static std::atomic<bool> wantsExit{ false };
static std::mutex messageMutex;
static std::string message;
static std::string latestAssetId;
static std::string tokenOverride;
static bool tokenLoaded = false;

static fs::path AppDir() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    return fs::path(path).parent_path();
}

static fs::path TokenFile() { return AppDir() / "update_token.txt"; }
static fs::path WorkDir() { return fs::temp_directory_path() / "RunBunUpdate"; }

static void SetState(UpdateState next, const std::string& text) {
    std::lock_guard<std::mutex> lock(messageMutex);
    message = text;
    state = next;
}

int GetCurrentBuild() { return RUNBUN_BUILD; }
int GetLatestBuild() { return latestBuild; }
UpdateState GetUpdateState() { return (UpdateState)state.load(); }
bool UpdateWantsExit() { return wantsExit; }

std::string GetUpdateMessage() {
    std::lock_guard<std::mutex> lock(messageMutex);
    return message;
}

// File beats baked token
std::string GetUpdateToken() {
    if (!tokenLoaded) {
        std::ifstream file(TokenFile());
        std::getline(file, tokenOverride);
        while (!tokenOverride.empty() && isspace((unsigned char)tokenOverride.back())) tokenOverride.pop_back();
        tokenLoaded = true;
    }
    return tokenOverride.empty() ? std::string(RUNBUN_UPDATE_TOKEN) : tokenOverride;
}

bool HasUpdateToken() { return !GetUpdateToken().empty(); }

void SetUpdateToken(const std::string& token) {
    tokenOverride = token;
    tokenLoaded = true;
    std::error_code ec;
    if (token.empty()) fs::remove(TokenFile(), ec);
    else std::ofstream(TokenFile()) << token;
}

static std::wstring Widen(const std::string& text) {
    return std::wstring(text.begin(), text.end());
}

struct HttpResult {
    DWORD status = 0;
    std::string body;
    std::wstring location;
};

static bool HttpGet(const std::wstring& host, const std::wstring& path, const std::wstring& headers, bool followRedirects, HttpResult& out) {
    bool ok = false;
    HINTERNET session = WinHttpOpen(L"RunBun-Updater", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    HINTERNET connect = session ? WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0) : nullptr;
    HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE) : nullptr;

    if (request) {
        if (!followRedirects) {
            DWORD policy = WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
            WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &policy, sizeof(policy));
        }
        if (WinHttpSendRequest(request, headers.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headers.c_str(), (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
            && WinHttpReceiveResponse(request, nullptr)) {
            DWORD size = sizeof(out.status);
            WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &out.status, &size, WINHTTP_NO_HEADER_INDEX);

            wchar_t location[4096];
            DWORD locationSize = sizeof(location);
            if (WinHttpQueryHeaders(request, WINHTTP_QUERY_LOCATION, WINHTTP_HEADER_NAME_BY_INDEX, location, &locationSize, WINHTTP_NO_HEADER_INDEX))
                out.location = location;

            DWORD available = 0;
            while (WinHttpQueryDataAvailable(request, &available) && available > 0) {
                size_t start = out.body.size();
                out.body.resize(start + available);
                DWORD read = 0;
                if (!WinHttpReadData(request, &out.body[start], available, &read)) break;
                out.body.resize(start + read);
            }
            ok = true;
        }
    }
    if (request) WinHttpCloseHandle(request);
    if (connect) WinHttpCloseHandle(connect);
    if (session) WinHttpCloseHandle(session);
    return ok;
}

static std::wstring ApiHeaders(const char* accept) {
    std::wstring headers = L"Accept: " + Widen(accept) + L"\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
    std::string token = GetUpdateToken();
    if (!token.empty()) headers += L"Authorization: Bearer " + Widen(token) + L"\r\n";
    return headers;
}

// Reads a JSON field value
static std::string JsonValue(const std::string& json, const std::string& key, size_t from = 0) {
    size_t at = json.find("\"" + key + "\"", from);
    if (at == std::string::npos) return "";
    at = json.find(':', at);
    if (at == std::string::npos) return "";
    at = json.find_first_not_of(" \t\r\n", at + 1);
    if (at == std::string::npos) return "";
    if (json[at] == '"') {
        size_t end = json.find('"', at + 1);
        return end == std::string::npos ? "" : json.substr(at + 1, end - at - 1);
    }
    size_t end = json.find_first_of(",}] \r\n", at);
    return json.substr(at, end - at);
}

static void CheckWorker() {
    HttpResult result;
    std::wstring path = Widen(std::string(REPO_PATH) + "/releases/latest");
    if (!HttpGet(API_HOST, path, ApiHeaders("application/vnd.github+json"), true, result)) {
        SetState(Update_Failed, "Could not reach GitHub");
        return;
    }
    if (result.status == 401 || result.status == 403 || result.status == 404) {
        SetState(Update_Failed, HasUpdateToken() ? "GitHub refused the update token" : "Add an update token in Preferences");
        return;
    }
    if (result.status != 200) {
        SetState(Update_Failed, "GitHub answered " + std::to_string(result.status));
        return;
    }

    // Tags look like build 12
    std::string tag = JsonValue(result.body, "tag_name");
    int build = tag.rfind("build-", 0) == 0 ? std::atoi(tag.c_str() + 6) : 0;

    size_t nameAt = result.body.find("\"" + std::string(ASSET_NAME) + "\"");
    size_t assetAt = nameAt == std::string::npos ? std::string::npos : result.body.rfind("\"url\"", nameAt);
    std::string assetUrl = assetAt == std::string::npos ? "" : JsonValue(result.body, "url", assetAt);
    size_t slash = assetUrl.rfind('/');
    if (build <= 0 || slash == std::string::npos) {
        SetState(Update_Failed, "Latest release has no build zip");
        return;
    }

    {
        std::lock_guard<std::mutex> lock(messageMutex);
        latestAssetId = assetUrl.substr(slash + 1);
    }
    latestBuild = build;
    if (build > GetCurrentBuild()) SetState(Update_Available, "Build " + std::to_string(build) + " is ready to download");
    else SetState(Update_UpToDate, "You are on the latest build");
}

static bool RunHidden(const std::wstring& command, bool wait) {
    STARTUPINFOW startup = { sizeof(startup) };
    PROCESS_INFORMATION process = {};
    std::wstring line = command;
    if (!CreateProcessW(nullptr, &line[0], nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process)) return false;
    DWORD exitCode = 0;
    if (wait) {
        WaitForSingleObject(process.hProcess, INFINITE);
        GetExitCodeProcess(process.hProcess, &exitCode);
    }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    return exitCode == 0;
}

static void DownloadWorker() {
    std::string assetId;
    {
        std::lock_guard<std::mutex> lock(messageMutex);
        assetId = latestAssetId;
    }

    // GitHub redirects to S3
    HttpResult first;
    std::wstring path = Widen(std::string(REPO_PATH) + "/releases/assets/" + assetId);
    if (!HttpGet(API_HOST, path, ApiHeaders("application/octet-stream"), false, first) || first.location.empty()) {
        SetState(Update_Failed, "Download link failed");
        return;
    }

    URL_COMPONENTS parts = { sizeof(parts) };
    wchar_t host[256], urlPath[4096];
    parts.lpszHostName = host;
    parts.dwHostNameLength = 256;
    parts.lpszUrlPath = urlPath;
    parts.dwUrlPathLength = 4096;
    wchar_t extra[4096];
    parts.lpszExtraInfo = extra;
    parts.dwExtraInfoLength = 4096;
    if (!WinHttpCrackUrl(first.location.c_str(), 0, 0, &parts)) {
        SetState(Update_Failed, "Bad download link");
        return;
    }

    HttpResult zip;
    if (!HttpGet(host, std::wstring(urlPath) + extra, L"", true, zip) || zip.status != 200 || zip.body.size() < 1024) {
        SetState(Update_Failed, "Download failed");
        return;
    }

    std::error_code ec;
    fs::remove_all(WorkDir(), ec);
    fs::create_directories(WorkDir() / "files", ec);
    fs::path zipPath = WorkDir() / ASSET_NAME;
    std::ofstream(zipPath, std::ios::binary).write(zip.body.data(), zip.body.size());

    std::wstring unzip = L"powershell -NoProfile -ExecutionPolicy Bypass -Command \"Expand-Archive -LiteralPath '"
        + zipPath.wstring() + L"' -DestinationPath '" + (WorkDir() / "files").wstring() + L"' -Force\"";
    if (!RunHidden(unzip, true) || !fs::exists(WorkDir() / "files" / "RunBunSandbox.exe")) {
        SetState(Update_Failed, "Could not unpack the update");
        return;
    }
    SetState(Update_Ready, "Build " + std::to_string(latestBuild.load()) + " downloaded");
}

void CheckForUpdates() {
    int current = state;
    if (current == Update_Checking || current == Update_Downloading) return;
    SetState(Update_Checking, "Checking for updates...");
    std::thread(CheckWorker).detach();
}

void DownloadUpdate() {
    if (state != Update_Available) return;
    SetState(Update_Downloading, "Downloading build " + std::to_string(latestBuild.load()) + "...");
    std::thread(DownloadWorker).detach();
}

void RestartIntoUpdate() {
    if (state != Update_Ready) return;

    // Waits for exit to copy
    fs::path script = WorkDir() / "apply.cmd";
    std::string pid = std::to_string(GetCurrentProcessId());
    std::ofstream out(script);
    out << "@echo off\r\n"
        << ":wait\r\n"
        << "tasklist /FI \"PID eq " << pid << "\" | find \"" << pid << "\" >nul && (timeout /t 1 /nobreak >nul & goto wait)\r\n"
        << "robocopy \"" << (WorkDir() / "files").string() << "\" \"" << AppDir().string() << "\" /E /NFL /NDL /NJH /NJS /NP >nul\r\n"
        << "start \"\" \"" << (AppDir() / "RunBunSandbox.exe").string() << "\"\r\n";
    out.close();

    if (RunHidden(L"cmd /c \"" + script.wstring() + L"\"", false)) wantsExit = true;
    else SetState(Update_Failed, "Could not start the updater");
}
