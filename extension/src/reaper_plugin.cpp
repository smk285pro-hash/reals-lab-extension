// Reals Lab — REAPER extension shell (Windows, WebView2 UI).
// Loads into REAPER, registers the "Reals Lab: Show Window" action, opens a
// Win32 window hosting WebView2 which renders ui-web/ and talks JSON bridge.
#ifdef _WIN32

#include <windows.h>
#include <objbase.h>
#include <ole2.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include <reaper_plugin.h>

#define REAPERAPI_MINIMAL
#define REAPERAPI_WANT_plugin_register
#define REAPERAPI_WANT_GetMainHwnd
#define REAPERAPI_WANT_GetResourcePath
#define REAPERAPI_WANT_InsertMedia
#define REAPERAPI_WANT_Master_GetTempo
#define REAPERAPI_WANT_Undo_BeginBlock
#define REAPERAPI_WANT_Undo_EndBlock
#define REAPERAPI_WANT_Main_OnCommand
#define REAPERAPI_WANT_GetPlayState
#define REAPERAPI_WANT_GetPlayPosition
#define REAPERAPI_WANT_GetPlayPosition2
#define REAPERAPI_WANT_PCM_Source_CreateFromFileEx
#define REAPERAPI_WANT_GetSetMediaItemTakeInfo
#define REAPERAPI_WANT_TimeMap2_timeToBeats
#define REAPERAPI_WANT_DockWindowAddEx
#define REAPERAPI_WANT_DockWindowRemove
#define REAPERAPI_WANT_DockWindowActivate
#define REAPERAPI_WANT_DockIsChildOfDock
#define REAPERAPI_WANT_Dock_UpdateDockID
#define REAPERAPI_WANT_GetConfigWantsDock
#define REAPERAPI_WANT_CountSelectedMediaItems
#define REAPERAPI_WANT_GetSelectedMediaItem
#define REAPERAPI_WANT_GetActiveTake
#define REAPERAPI_WANT_GetMediaItemTake_Source
#define REAPERAPI_WANT_GetMediaItemTakeInfo_Value
#define REAPERAPI_WANT_SetMediaItemTakeInfo_Value
#define REAPERAPI_WANT_UpdateArrange
#define REAPERAPI_WANT_CountTracks
#define REAPERAPI_WANT_GetTrack
#define REAPERAPI_WANT_CountTrackMediaItems
#define REAPERAPI_WANT_GetTrackMediaItem
#define REAPERAPI_WANT_GetMediaItemInfo_Value
#define REAPERAPI_WANT_SetMediaItemInfo_Value
#define REAPERAPI_IMPLEMENT
#include <reaper_plugin_functions.h>

#include "reals/audio/Engine.h"
#include "reals/bridge/Bridge.h"
#include "reals/config/Config.h"
#include "reals/i18n/I18n.h"
#include "reals/platform/Path.h"
#include "reals/util/Log.h"
#include "OleDrag.h"
#include "WebViewHost.h"
#include "resource.h"
#include <dwmapi.h>
#include <commctrl.h>
#pragma comment(lib, "comctl32.lib")

#ifndef REALS_UI_WEB_DIR_W
#define REALS_UI_WEB_DIR_W L"ui-web"
#endif

REAPER_PLUGIN_HINSTANCE g_hInstance = nullptr;

namespace {

constexpr wchar_t kWndClass[] = L"RealsLabHostWnd";
constexpr const char* kCommandId = "REALSLAB_SHOW_WINDOW";
constexpr const char* kCommandName = "Reals Lab: Show Window";
constexpr auto kTag = "ext";

HWND g_hwnd = nullptr;
bool g_visible = false;
bool g_comOwned = false;
bool g_hostCreating = false;
int g_cmdId = 0;
constexpr UINT WM_REALS_BEGINDRAG = WM_APP + 41;
constexpr UINT WM_REALS_FILEDROP = WM_APP + 42;
constexpr UINT WM_REALS_DROPHOVER = WM_APP + 43;
constexpr UINT WM_REALS_STARTDRAG = WM_APP + 44;
std::wstring g_dragPath;
std::vector<std::wstring> g_dropPaths;
std::unique_ptr<reals::shell::WebViewHost> g_web;
std::unique_ptr<reals::bridge::Bridge> g_bridge;
HBRUSH g_bgBrush = nullptr;
HICON g_hIconBig = nullptr;
HICON g_hIconSm = nullptr;

// Pending playrate and pitch shift for async InsertMedia and OLE Drag-and-Drop
struct PendingPlayrate {
    std::string path;
    std::string originalPath;
    double playrate;
    double pitchSemitones;
    uint64_t queuedTime;
    int tries;
};

std::vector<PendingPlayrate> g_pendingPlayrates;
std::mutex g_pendingMutex;

void queuePendingPlayrate(const std::string& path, double rate, double pitch = 0.0, const std::string& originalPath = "") {
    const std::lock_guard lock(g_pendingMutex);
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    g_pendingPlayrates.push_back({path, originalPath, rate, pitch, now, 0});
    char msg[256];
    std::snprintf(msg, sizeof(msg), "queuePendingPlayrate: path=%s rate=%.4f pitch=%.2f", path.c_str(), rate, pitch);
    LOG_INFO(kTag, msg);
}

void processPendingSyncPlayrates() {
    std::lock_guard lock(g_pendingMutex);
    if (g_pendingPlayrates.empty())
        return;

    const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    for (auto it = g_pendingPlayrates.begin(); it != g_pendingPlayrates.end(); ) {
        // Keep pending playrates valid for 60 seconds during drag operations
        if (now - it->queuedTime > 60000) {
            it = g_pendingPlayrates.erase(it);
            continue;
        }

        bool matchedAny = false;
        std::string normTarget = reals::platform::normalizePath(it->path);
        for (char& c : normTarget) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

        auto applyToTake = [&](MediaItem* item, MediaItem_Take* take, const std::string& rawPath = "") -> bool {
            if (!item || !take || !SetMediaItemTakeInfo_Value) return false;

            if (!it->originalPath.empty() && PCM_Source_CreateFromFileEx && GetSetMediaItemTakeInfo) {
                PCM_source* newSrc = PCM_Source_CreateFromFileEx(it->originalPath.c_str(), false);
                if (newSrc) {
                    // Fetch the old (pre-baked) source and destroy it after the
                    // swap — replacing P_SOURCE without deleting the previous
                    // source leaks a PCM_source on every drag.
                    PCM_source* oldSrc = static_cast<PCM_source*>(
                        GetSetMediaItemTakeInfo(take, "P_SOURCE", nullptr));
                    GetSetMediaItemTakeInfo(take, "P_SOURCE", newSrc);
                    if (oldSrc && oldSrc != newSrc) {
                        oldSrc->Delete();
                    }
                    SetMediaItemTakeInfo_Value(take, "D_PLAYRATE", it->playrate);
                    SetMediaItemTakeInfo_Value(take, "B_PPITCH", 1);
                    SetMediaItemTakeInfo_Value(take, "D_PITCH", it->pitchSemitones);
                    char msg[256];
                    std::snprintf(msg, sizeof(msg), "Mechanism C: Swapped source, playrate %.4f", it->playrate);
                    LOG_INFO(kTag, msg);
                    // D_LENGTH is inherited from the perfect ghost item, no need to adjust
                    return true;
                }
            }

            std::string pathLower = rawPath;
            for (char& c : pathLower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

            // Mechanism B Safeguard: If media item is pre-baked WAV, ensure playrate=1.0 and pitch=0.0
            if (pathLower.find("drag_") != std::string::npos || pathLower.find("drag_export") != std::string::npos) {
                SetMediaItemTakeInfo_Value(take, "D_PLAYRATE", 1.0);
                SetMediaItemTakeInfo_Value(take, "B_PPITCH", 1);
                SetMediaItemTakeInfo_Value(take, "D_PITCH", 0.0);
                char msg[256];
                std::snprintf(msg, sizeof(msg), "Mechanism B: Reset D_PLAYRATE=1.0, D_PITCH=0.0 on pre-baked item (%s)", rawPath.c_str());
                LOG_INFO(kTag, msg);
                return true;
            }

            // Mechanism A: Native REAPER Drag & Playrate Alignment
            double curRate = GetMediaItemTakeInfo_Value ? GetMediaItemTakeInfo_Value(take, "D_PLAYRATE") : 1.0;
            double curLen = GetMediaItemInfo_Value ? GetMediaItemInfo_Value(item, "D_LENGTH") : 0.0;

            SetMediaItemTakeInfo_Value(take, "D_PLAYRATE", it->playrate);
            SetMediaItemTakeInfo_Value(take, "B_PPITCH", 1); // preserve pitch when stretching
            SetMediaItemTakeInfo_Value(take, "D_PITCH", it->pitchSemitones);
            if (curLen > 0.0 && curRate > 0.0 && it->playrate > 0.0 && SetMediaItemInfo_Value) {
                // Adjust item boundary so the full loop fits the project tempo grid bar
                double origLen = curLen * curRate;
                double newLen = origLen / it->playrate;
                SetMediaItemInfo_Value(item, "D_LENGTH", newLen);
            }
            char msg[256];
            std::snprintf(msg, sizeof(msg), "Mechanism A: Synced item to playrate %.4f (pitch %.2f)", it->playrate, it->pitchSemitones);
            LOG_INFO(kTag, msg);
            return true;
        };

        // 1. Check selected media items (newly dropped or inserted item is selected by REAPER)
        if (CountSelectedMediaItems && GetSelectedMediaItem && GetActiveTake) {
            int selCount = CountSelectedMediaItems(0);
            for (int i = 0; i < selCount; ++i) {
                MediaItem* item = GetSelectedMediaItem(0, i);
                if (!item) continue;
                MediaItem_Take* take = GetActiveTake(item);
                if (!take) continue;

                bool isMatch = false;
                std::string rawSrcPath;
                if (GetMediaItemTake_Source) {
                    PCM_source* src = GetMediaItemTake_Source(take);
                    if (src && src->GetFileName()) {
                        rawSrcPath = src->GetFileName();
                        std::string srcPath = reals::platform::normalizePath(rawSrcPath);
                        for (char& c : srcPath) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
                        if (srcPath == normTarget || srcPath.find(normTarget) != std::string::npos || normTarget.find(srcPath) != std::string::npos) {
                            isMatch = true;
                        }
                    }
                } else {
                    isMatch = true;
                }

                if (isMatch) {
                    if (applyToTake(item, take, rawSrcPath.empty() ? it->path : rawSrcPath)) {
                        matchedAny = true;
                    }
                }
            }
        }

        // 2. Also search all tracks & items in project if not in selected items
        if (!matchedAny && CountTracks && GetTrack && CountTrackMediaItems && GetTrackMediaItem && GetActiveTake && GetMediaItemTake_Source) {
            int numTracks = CountTracks(0);
            for (int t = 0; t < numTracks && !matchedAny; ++t) {
                MediaTrack* trk = GetTrack(0, t);
                if (!trk) continue;
                int numItems = CountTrackMediaItems(trk);
                for (int m = 0; m < numItems && !matchedAny; ++m) {
                    MediaItem* item = GetTrackMediaItem(trk, m);
                    if (!item) continue;
                    MediaItem_Take* take = GetActiveTake(item);
                    if (!take) continue;
                    PCM_source* src = GetMediaItemTake_Source(take);
                    if (!src || !src->GetFileName()) continue;

                    std::string rawSrcPath = src->GetFileName();
                    std::string srcPath = reals::platform::normalizePath(rawSrcPath);
                    for (char& c : srcPath) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
                    if (srcPath == normTarget || srcPath.find(normTarget) != std::string::npos || normTarget.find(srcPath) != std::string::npos) {
                        double curRate = GetMediaItemTakeInfo_Value ? GetMediaItemTakeInfo_Value(take, "D_PLAYRATE") : 1.0;
                        if (std::abs(curRate - it->playrate) > 0.001 || (srcPath.find("drag_") != std::string::npos || srcPath.find("drag_export") != std::string::npos)) {
                            if (applyToTake(item, take, rawSrcPath)) {
                                matchedAny = true;
                            }
                        }
                    }
                }
            }
        }

        if (matchedAny) {
            if (UpdateArrange) UpdateArrange();
            it = g_pendingPlayrates.erase(it);
        } else {
            it->tries++;
            ++it;
        }
    }
}

std::wstring toWide(const std::string& utf8) {
    if (utf8.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1, w.data(), n);
    w.pop_back();
    return w;
}

void pushDockState(bool docked);
bool isDockedInternal();
void toggleDockInternal();
void applyDwmDarkTitle(HWND hwnd);

// ---------------------------------------------------------------------------
// Bridge host actions (touch REAPER)
// ---------------------------------------------------------------------------
class ExtHostActions final : public reals::bridge::IHostActions {
public:
    static bool isMediaFile(const std::string& path) {
        const size_t dot = path.find_last_of('.');
        if (dot == std::string::npos)
            return false;
        std::string ext = path.substr(dot + 1);
        for (char& c : ext)
            c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        static const char* kMedia[] = {"wav",  "wave", "mp3", "flac", "ogg",  "oga",
                                       "aif",  "aiff", "wma", "m4a",  "aac",  "opus",
                                       "w64",  "caf",  "mid", "midi", "mp4",  "mkv",
                                       "mov",  "avi",  "webm","wmv",  "rpp",
                                       "rtracktemplate", "rfxchain"};
        for (const char* m : kMedia)
            if (ext == m)
                return true;
        return false;
    }

    void insertMedia(const std::string& path) override {
        insertMedia(path, 1.0, 0.0);
    }
    void insertMedia(const std::string& path, double playrate) override {
        insertMedia(path, playrate, 0.0);
    }
    void insertMedia(const std::string& path, double playrate, double pitchSemitones) override {
        LOG_INFO(kTag, "insert: begin");
        if (!isMediaFile(path)) {
            LOG_INFO(kTag, "insert: not a media file");
            if (g_bridge) {
                json_event("toast", reals::i18n::tr("browser.toast.notMedia"));
            }
            return;
        }
        const bool doSync = (playrate > 0.25 && playrate < 4.0 && (std::abs(playrate - 1.0) > 0.001 || std::abs(pitchSemitones) > 0.001));
        char rateMsg[64];
        std::snprintf(rateMsg, sizeof(rateMsg), "insert: path=%s playrate=%.4f pitch=%.2f sync=%d", path.c_str(), playrate, pitchSemitones, doSync ? 1 : 0);
        LOG_INFO(kTag, rateMsg);

        // Remember selection before insert to find new items afterwards
        int selBefore = 0;
        if (CountSelectedMediaItems) selBefore = CountSelectedMediaItems(0);

        Undo_BeginBlock();
        // mode 1 = add new track (SDK: 0=current track, 1=new track, 3=takes)
        InsertMedia(path.c_str(), 1);

        // If sync is requested, set the new item's take playrate to match project tempo
        if (doSync) {
            queuePendingPlayrate(path, playrate, pitchSemitones);
            processPendingSyncPlayrates();
        }

        Undo_EndBlock("Reals Lab: Insert media", 0);
        json_event("toast", reals::i18n::tr("browser.toast.inserted"));
        char msg[512];
        std::snprintf(msg, sizeof(msg), "inserted %s (playrate %.3f, pitch %.2f)", path.c_str(), playrate, pitchSemitones);
        LOG_INFO(kTag, msg);
    }

    void revealInExplorer(const std::string& path) override {
        const std::wstring args = L"/select,\"" + toWide(path) + L"\"";
        ShellExecuteW(nullptr, L"open", L"explorer.exe", args.c_str(), nullptr, SW_SHOWNORMAL);
    }

    void sendToLab(const std::string& path, const std::string& job) override {
        json_event("toast", reals::i18n::tr("browser.toast.labQueued"));
        char log[512];
        std::snprintf(log, sizeof(log), "lab job queued (P2): %s [%s]", path.c_str(),
                      job.c_str());
        LOG_INFO(kTag, log);
    }

    void hideWindow() override {
        g_visible = false;
        if (g_web)
            g_web->setVisible(false);
        if (g_hwnd)
            ShowWindow(g_hwnd, SW_HIDE);
    }

    void minimizeWindow() override {
        if (g_web)
            g_web->setVisible(false);
        if (g_hwnd)
            ShowWindow(g_hwnd, SW_MINIMIZE);
    }

    void toggleMaximize() override {
        if (!g_hwnd)
            return;
        if (IsZoomed(g_hwnd))
            ShowWindow(g_hwnd, SW_RESTORE);
        else
            ShowWindow(g_hwnd, SW_MAXIMIZE);
    }

    void startDragWindow() override {
        if (g_hwnd)
            PostMessageW(g_hwnd, WM_REALS_STARTDRAG, 0, 0);
    }

    void startResizeWindow(const std::string& edge) override {
        if (!g_hwnd || IsZoomed(g_hwnd))
            return;
        WPARAM ht = HTCLIENT;
        if (edge == "left") ht = HTLEFT;
        else if (edge == "right") ht = HTRIGHT;
        else if (edge == "top") ht = HTTOP;
        else if (edge == "bottom") ht = HTBOTTOM;
        else if (edge == "top-left") ht = HTTOPLEFT;
        else if (edge == "top-right") ht = HTTOPRIGHT;
        else if (edge == "bottom-left") ht = HTBOTTOMLEFT;
        else if (edge == "bottom-right") ht = HTBOTTOMRIGHT;

        if (ht != HTCLIENT) {
            ReleaseCapture();
            SendMessageW(g_hwnd, WM_NCLBUTTONDOWN, ht, 0);
        }
    }

    void beginDrag(const std::string& path) override {
        // DoDragDrop pumps messages — never call it from the WebView2
        // WebMessageReceived callback (nested STA pump). Post to the host.
        g_dragPath = toWide(path);
        if (g_hwnd)
            PostMessageW(g_hwnd, WM_REALS_BEGINDRAG, 0, 0);
    }

    void queueSyncPlayrate(const std::string& path, double playrate) override {
        queuePendingPlayrate(path, playrate, 0.0);
    }
    void queueSyncPlayrate(const std::string& path, double playrate, double pitchSemitones, const std::string& originalPath = "") override {
        queuePendingPlayrate(path, playrate, pitchSemitones, originalPath);
    }

    double projectTempo() const override {
        return Master_GetTempo ? Master_GetTempo() : 0.0;
    }

    void togglePlay() override {
        if (Main_OnCommand)
            Main_OnCommand(40044, 0); // Transport: Play/stop
    }

    reals::bridge::HostTransport hostTransport() const override {
        reals::bridge::HostTransport t;
        if (GetPlayState)
            t.playState = GetPlayState();
        if ((t.playState & 1) && GetPlayPosition2)
            t.playPosition = GetPlayPosition2();
        else if (GetPlayPosition)
            t.playPosition = GetPlayPosition();
        if (Master_GetTempo)
            t.bpm = Master_GetTempo();
        if (TimeMap2_timeToBeats) {
            int m = 0, cml = 4, cdenom = 4;
            double fb = 0.0;
            TimeMap2_timeToBeats(nullptr, t.playPosition, &m, &cml, &fb, &cdenom);
            t.measure = m;
            t.beatsPerMeasure = cml > 0 ? cml : 4;
            t.denom = cdenom > 0 ? cdenom : 4;
            t.fullBeats = fb;
        }
        return t;
    }

    void toggleDock() override;
    bool isDocked() const override;

private:
    static void json_event(const char* event, const std::string& text) {
        if (!g_web)
            return;
        nlohmann::json j;
        j["event"] = event;
        j["data"] = {{"text", text}};
        g_web->postJson(j.dump());
    }
};

ExtHostActions g_actions;

// ---------------------------------------------------------------------------
// ui-web folder resolution: installed copy first, dev tree second.
// ---------------------------------------------------------------------------
std::wstring resolveUiWebDir() {
    // Dev tree first (compile-time path). Customers don't have it, so they
    // fall through to the installed copy under %APPDATA%\RealsLab\ui-web.
    const std::wstring dev(REALS_UI_WEB_DIR_W);
    if (!dev.empty() &&
        GetFileAttributesW((dev + L"\\index.html").c_str()) != INVALID_FILE_ATTRIBUTES)
        return dev;
    return toWide(reals::platform::joinPath(reals::platform::dataDir(), "ui-web"));
}

// ---------------------------------------------------------------------------
// Window + WebView lifecycle
// ---------------------------------------------------------------------------
LRESULT CALLBACK hostWndProc(const HWND h, const UINT msg, const WPARAM wParam,
                             const LPARAM lParam) {
    switch (msg) {
    case WM_SIZE: {
        if (g_web && wParam != SIZE_MINIMIZED) {
            g_web->resize(static_cast<LONG>(LOWORD(lParam)), static_cast<LONG>(HIWORD(lParam)));
            nlohmann::json j;
            j["event"] = "window.state";
            j["data"] = {{"maximized", wParam == SIZE_MAXIMIZED}};
            g_web->postJson(j.dump());
        }
        return 0;
    }
    case WM_REALS_BEGINDRAG:
        reals::shell::beginFileDrag(h, g_dragPath);
        processPendingSyncPlayrates();
        return 0;
    case WM_REALS_FILEDROP: {
        if (!g_bridge)
            return 0;
        nlohmann::json args;
        args["paths"] = nlohmann::json::array();
        for (const auto& w : g_dropPaths) {
            const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
            if (n <= 1)
                continue;
            std::string u8(static_cast<size_t>(n), '\0');
            WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, u8.data(), n, nullptr, nullptr);
            u8.pop_back();
            args["paths"].push_back(u8);
        }
        g_dropPaths.clear();
        nlohmann::json req;
        req["id"] = 0;
        req["cmd"] = "fs.dropPaths";
        req["args"] = args;
        (void)g_bridge->handle(req.dump());
        if (g_web) {
            for (const auto& ev : g_bridge->drainEvents())
                g_web->postJson(ev);
        }
        return 0;
    }
    case WM_REALS_DROPHOVER: {
        if (!g_web)
            return 0;
        nlohmann::json j;
        j["event"] = "fs.dropHover";
        j["data"] = {{"on", wParam != 0}};
        g_web->postJson(j.dump());
        return 0;
    }
    case WM_NCCALCSIZE: {
        if (wParam == TRUE)
            return 0;
        return 0;
    }
    case WM_NCHITTEST: {
        if (isDockedInternal() || IsZoomed(h))
            return HTCLIENT;
        POINT pt{static_cast<SHORT>(LOWORD(lParam)), static_cast<SHORT>(HIWORD(lParam))};
        RECT rc{};
        GetWindowRect(h, &rc);
        const int border = 8;
        const bool top = pt.y >= rc.top && pt.y < rc.top + border;
        const bool bottom = pt.y >= rc.bottom - border && pt.y < rc.bottom;
        const bool left = pt.x >= rc.left && pt.x < rc.left + border;
        const bool right = pt.x >= rc.right - border && pt.x < rc.right;

        if (top && left) return HTTOPLEFT;
        if (top && right) return HTTOPRIGHT;
        if (bottom && left) return HTBOTTOMLEFT;
        if (bottom && right) return HTBOTTOMRIGHT;
        if (top) return HTTOP;
        if (bottom) return HTBOTTOM;
        if (left) return HTLEFT;
        if (right) return HTRIGHT;
        return HTCLIENT;
    }
    case WM_REALS_STARTDRAG: {
        ReleaseCapture();
        SendMessageW(h, WM_NCLBUTTONDOWN, HTCAPTION, 0);
        return 0;
    }
    case WM_CLOSE:
        g_actions.hideWindow();
        return 0;
    default:
        break;
    }
    return DefWindowProcW(h, msg, wParam, lParam);
}

LRESULT CALLBACK webChildResizeSubclass(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                                       UINT_PTR /*uIdSubclass*/, DWORD_PTR /*dwRefData*/) {
    if (uMsg == WM_NCHITTEST) {
        if (g_hwnd && !IsZoomed(g_hwnd) && !isDockedInternal()) {
            POINT pt{static_cast<SHORT>(LOWORD(lParam)), static_cast<SHORT>(HIWORD(lParam))};
            RECT rc{};
            GetWindowRect(g_hwnd, &rc);
            const int border = 8;
            if (pt.x >= rc.left && pt.x < rc.right && pt.y >= rc.top && pt.y < rc.bottom) {
                if (pt.x < rc.left + border || pt.x >= rc.right - border ||
                    pt.y < rc.top + border || pt.y >= rc.bottom - border) {
                    return HTTRANSPARENT;
                }
            }
        }
    }
    return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void subclassChildWindowsForResize(HWND root) {
    if (!root)
        return;
    EnumChildWindows(
        root,
        [](HWND child, LPARAM) -> BOOL {
            SetWindowSubclass(child, webChildResizeSubclass, 0x5245414C /* REAL */, 0);
            return TRUE;
        },
        0);
}

void pushAudioState() {
    if (!g_web || !g_web->isReady() || !g_bridge)
        return;
    g_web->postJson(g_bridge->audioStateJson());
}

void timerHook() {
    try {
        // Chromium HWND can appear a few ticks after controller-ready.
        // Re-enum children so Explorer drops land on our IDropTarget and resize works.
        static int s_dropTreeTicks = 0;
        if (g_hwnd && g_web && g_web->isReady() && s_dropTreeTicks < 300) {
            ++s_dropTreeTicks;
            if (s_dropTreeTicks <= 60 || (s_dropTreeTicks % 30) == 0) {
                reals::shell::registerFileDropTargetTree(g_hwnd);
                subclassChildWindowsForResize(g_hwnd);
            }
        }
        if (!g_web || !g_bridge)
            return;

        static int s_lastDocked = -1;
        bool curDocked = isDockedInternal();
        if (s_lastDocked != (curDocked ? 1 : 0)) {
            bool wasDocked = (s_lastDocked == 1);
            s_lastDocked = (curDocked ? 1 : 0);
            if (wasDocked && !curDocked) {
                SetParent(g_hwnd, nullptr);
                LONG_PTR style = (WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
                SetWindowLongPtrW(g_hwnd, GWL_STYLE, style);
                LONG_PTR ex = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
                ex &= ~WS_EX_TOOLWINDOW;
                SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, ex);
                applyDwmDarkTitle(g_hwnd);
                ShowWindow(g_hwnd, SW_SHOW);
                RECT rc{};
                GetClientRect(g_hwnd, &rc);
                if (g_web) {
                    g_web->resize(rc.right, rc.bottom);
                    g_web->setVisible(true);
                }
            }
            pushDockState(curDocked);
        }

        // Drain lab job events (background threads) to the web UI.
        for (const auto& ev : g_bridge->drainEvents())
            g_web->postJson(ev);

        // Process pending playrate adjustments for recently inserted / dragged items
        processPendingSyncPlayrates();
        // Push audio playback state while playing. One extra frame is pushed when
        // playback stops (track ended / stop pressed) so the UI does not freeze
        // showing a stale "playing" state.
        static bool s_wasPlaying = false;
        const bool playing = reals::audio::Engine::instance().isPlaying();
        if (g_visible && (playing || s_wasPlaying))
            pushAudioState();
        s_wasPlaying = playing;
    } catch (const std::exception& e) {
        LOG_ERROR(kTag, std::string("timerHook exception: ") + e.what());
    } catch (...) {
        LOG_ERROR(kTag, "timerHook unknown exception");
    }
}

RECT g_floatingRect{100, 100, 880, 740};

void pushDockState(bool docked) {
    if (g_web && g_web->isReady()) {
        nlohmann::json d;
        d["event"] = "window.dockState";
        d["data"] = {{"docked", docked}};
        g_web->postJson(d.dump());
    }
}

bool isDockedInternal() {
    if (!g_hwnd) return false;
    bool isFloating = false;
    int dockId = DockIsChildOfDock ? DockIsChildOfDock(g_hwnd, &isFloating) : -1;
    return (dockId >= 0 && !isFloating);
}

void toggleDockInternal() {
    if (!g_hwnd) return;
    bool isFloating = false;
    int dockId = DockIsChildOfDock ? DockIsChildOfDock(g_hwnd, &isFloating) : -1;
    bool currentlyDocked = (dockId >= 0 && !isFloating);
    if (currentlyDocked) {
        // UNDOCK: Detach from REAPER Docker
        if (DockWindowRemove)
            DockWindowRemove(g_hwnd);

        SetParent(g_hwnd, nullptr);

        LONG_PTR style = (WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
        SetWindowLongPtrW(g_hwnd, GWL_STYLE, style);

        LONG_PTR ex = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
        ex &= ~WS_EX_TOOLWINDOW;
        SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, ex);

        int w = g_floatingRect.right - g_floatingRect.left;
        int h = g_floatingRect.bottom - g_floatingRect.top;
        if (w < 400 || h < 300) { w = 880; h = 640; }
        int x = g_floatingRect.left;
        int y = g_floatingRect.top;
        if (x < 0 || y < 0) { x = 100; y = 100; }

        SetWindowPos(g_hwnd, HWND_TOP, x, y, w, h,
                     SWP_FRAMECHANGED | SWP_SHOWWINDOW);

        applyDwmDarkTitle(g_hwnd);
        ShowWindow(g_hwnd, SW_SHOW);
        SetForegroundWindow(g_hwnd);

        RECT rc{};
        GetClientRect(g_hwnd, &rc);
        if (g_web) {
            g_web->resize(rc.right, rc.bottom);
            g_web->setVisible(true);
        }
        pushDockState(false);
        g_visible = true;
    } else {
        // DOCK: Save floating rect first
        RECT rc{};
        GetWindowRect(g_hwnd, &rc);
        if (rc.right - rc.left > 200 && rc.bottom - rc.top > 200) {
            g_floatingRect = rc;
        }

        if (DockWindowAddEx) {
            DockWindowAddEx(g_hwnd, "Reals Lab", "REALSLAB_DOCK", true);
        }
        if (DockWindowActivate) {
            DockWindowActivate(g_hwnd);
        }
        ShowWindow(g_hwnd, SW_SHOW);

        RECT clientRc{};
        GetClientRect(g_hwnd, &clientRc);
        if (g_web) {
            g_web->resize(clientRc.right, clientRc.bottom);
            g_web->setVisible(true);
        }
        pushDockState(true);
        g_visible = true;
    }
}

void ExtHostActions::toggleDock() {
    toggleDockInternal();
}

bool ExtHostActions::isDocked() const {
    return isDockedInternal();
}

bool isTransportCommand(const int command) {
    return command == 40044 || command == 1007 || command == 1016 ||
           command == 40073 || command == 40045 || command == 40042 ||
           command == 40043;
}

void applyDwmDarkTitle(const HWND hwnd) {
    if (!hwnd)
        return;
    BOOL darkMode = TRUE;
    DwmSetWindowAttribute(hwnd, 20 /* DWMWA_USE_IMMERSIVE_DARK_MODE */, &darkMode, sizeof(darkMode));
    DwmSetWindowAttribute(hwnd, 19 /* DWMWA_USE_IMMERSIVE_DARK_MODE_BEFORE_20H1 */, &darkMode, sizeof(darkMode));
    COLORREF captionColor = RGB(0x0D, 0x0E, 0x11);
    DwmSetWindowAttribute(hwnd, 35 /* DWMWA_CAPTION_COLOR */, &captionColor, sizeof(captionColor));
    COLORREF textColor = RGB(0xF2, 0xF3, 0xF5);
    DwmSetWindowAttribute(hwnd, 36 /* DWMWA_TEXT_COLOR */, &textColor, sizeof(textColor));
    COLORREF borderColor = RGB(0x24, 0x26, 0x2B);
    DwmSetWindowAttribute(hwnd, 34 /* DWMWA_BORDER_COLOR */, &borderColor, sizeof(borderColor));
    // DWMWCP_ROUND (2) for modern subtle rounded corners on Windows 11
    int cornerPref = 2;
    DwmSetWindowAttribute(hwnd, 33 /* DWMWA_WINDOW_CORNER_PREFERENCE */, &cornerPref, sizeof(cornerPref));
}

bool createHostWindow(const bool showImmediately) {
    if (g_hwnd || g_hostCreating)
        return g_hwnd != nullptr;
    g_hostCreating = true;
    LOG_INFO(kTag, showImmediately ? "host: creating window (shown)"
                                    : "host: creating window (prewarm, hidden)");
    if (!g_bgBrush)
        g_bgBrush = CreateSolidBrush(RGB(0x0D, 0x0E, 0x11));

    HINSTANCE hInst = reinterpret_cast<HINSTANCE>(g_hInstance);
    if (!g_hIconBig) {
        g_hIconBig = static_cast<HICON>(LoadImageW(
            hInst, MAKEINTRESOURCEW(IDI_REALS_ICON), IMAGE_ICON,
            GetSystemMetrics(SM_CXICON), GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR));
    }
    if (!g_hIconSm) {
        g_hIconSm = static_cast<HICON>(LoadImageW(
            hInst, MAKEINTRESOURCEW(IDI_REALS_ICON), IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR));
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = hostWndProc;
    wc.hInstance = g_hInstance;
    wc.hIcon = g_hIconBig;
    wc.hIconSm = g_hIconSm;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = g_bgBrush ? g_bgBrush : static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kWndClass;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        LOG_ERROR(kTag, "host: RegisterClassExW failed");
        g_hostCreating = false;
        return false;
    }

    // WS_EX_TOOLWINDOW + SW_HIDE: HWND exists so WebView2 can attach, but the
    // user never sees an empty window during REAPER startup.
    g_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kWndClass, L"Reals Lab", WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT, CW_USEDEFAULT, 760, 920, GetMainHwnd(), nullptr,
                             g_hInstance, nullptr);
    if (!g_hwnd) {
        char msg[96];
        std::snprintf(msg, sizeof(msg), "host: CreateWindowExW failed (err %lu)",
                      static_cast<unsigned long>(GetLastError()));
        LOG_ERROR(kTag, msg);
        g_hostCreating = false;
        return false;
    }

    if (g_hIconBig)
        SendMessageW(g_hwnd, WM_SETICON, ICON_BIG, reinterpret_cast<LPARAM>(g_hIconBig));
    if (g_hIconSm)
        SendMessageW(g_hwnd, WM_SETICON, ICON_SMALL, reinterpret_cast<LPARAM>(g_hIconSm));

    applyDwmDarkTitle(g_hwnd);

    ShowWindow(g_hwnd, SW_HIDE);

    // COM for the WebView2 async machinery (safe if REAPER already inited STA).
    const HRESULT comHr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    // S_OK/S_FALSE: we hold a COM reference -> CoUninitialize on unload.
    // RPC_E_CHANGED_MODE: host already MTA -> not ours to release.
    g_comOwned = SUCCEEDED(comHr) && comHr != RPC_E_CHANGED_MODE;
    OleInitialize(nullptr);
    reals::shell::registerFileDropTarget(
        g_hwnd,
        [](const std::vector<std::wstring>& paths) {
            g_dropPaths = paths;
            if (g_hwnd)
                PostMessageW(g_hwnd, WM_REALS_FILEDROP, 0, 0);
        },
        [](bool on) {
            if (g_hwnd)
                PostMessageW(g_hwnd, WM_REALS_DROPHOVER, on ? 1 : 0, 0);
        });
    {
        char msg[64];
        std::snprintf(msg, sizeof(msg), "host: CoInitializeEx hr 0x%08lX",
                      static_cast<unsigned long>(comHr));
        LOG_INFO(kTag, msg);
    }

    if (!g_web)
        g_web = std::make_unique<reals::shell::WebViewHost>();
    const std::wstring userData =
        toWide(reals::platform::joinPath(reals::platform::dataDir(), "WebView2"));
    const std::wstring uiDir = resolveUiWebDir();
    g_web->create(g_hwnd, userData, uiDir, [](bool ok) {
        g_hostCreating = false;
        if (ok) {
            LOG_INFO(kTag, "webview: UI live");
            RECT rc{};
            GetClientRect(g_hwnd, &rc);
            if (g_web)
                g_web->resize(rc.right, rc.bottom);
            if (g_visible && g_web)
                g_web->setVisible(true);
            reals::shell::registerFileDropTargetTree(g_hwnd);
        } else {
            LOG_ERROR(kTag, "host: webview init failed — see earlier log lines");
        }
    });

    g_web->setWebMessageHandler([](const std::string& msg) {
        if (g_bridge) {
            const std::string response = g_bridge->handle(msg);
            if (g_web)
                g_web->postJson(response);
        }
    });

    plugin_register("timer", reinterpret_cast<void*>(timerHook));
    return true;
}

void showHostWindow() {
    if (!g_hwnd)
        return;
    bool isFloating = false;
    int dockId = DockIsChildOfDock ? DockIsChildOfDock(g_hwnd, &isFloating) : -1;
    bool docked = (dockId >= 0 && !isFloating);
    if (!docked) {
        SetParent(g_hwnd, nullptr);
        const LONG_PTR ex = GetWindowLongPtrW(g_hwnd, GWL_EXSTYLE);
        if (ex & WS_EX_TOOLWINDOW) {
            SetWindowLongPtrW(g_hwnd, GWL_EXSTYLE, ex & ~WS_EX_TOOLWINDOW);
        }
        LONG_PTR style = (WS_POPUP | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS);
        SetWindowLongPtrW(g_hwnd, GWL_STYLE, style);
        applyDwmDarkTitle(g_hwnd);
        ShowWindow(g_hwnd, SW_SHOW);
        SetWindowPos(g_hwnd, HWND_TOP, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
        if (DockWindowActivate)
            DockWindowActivate(g_hwnd);
        ShowWindow(g_hwnd, SW_SHOW);
    }
    RECT rc{};
    GetClientRect(g_hwnd, &rc);
    if (g_web) {
        g_web->resize(rc.right, rc.bottom);
        if (g_web->isReady())
            g_web->setVisible(true);
    }
    SetForegroundWindow(g_hwnd);
    pushDockState(docked);
    LOG_INFO(kTag, "window shown");
}

void toggleWindow() {
    LOG_INFO(kTag, "toggle: called");
    if (!g_hwnd && !createHostWindow(true)) {
        LOG_ERROR(kTag, "toggle: createHostWindow failed");
        return;
    }
    g_visible = !g_visible;
    if (g_visible) {
        showHostWindow();
    } else {
        if (g_web)
            g_web->setVisible(false);
        ShowWindow(g_hwnd, SW_HIDE);
    }
}

// Both hook versions for dispatch reliability; dedup via tick window.
DWORD g_lastToggleTick = 0;

void toggleOnce() {
    const DWORD now = GetTickCount();
    if (now - g_lastToggleTick < 200)
        return;
    g_lastToggleTick = now;
    toggleWindow();
}

int commandHook(KbdSectionInfo* /*sec*/, const int command, const int /*val*/, const int /*val2*/,
                const int /*relmode*/, const HWND /*hwnd*/) {
    if (command == g_cmdId && g_cmdId != 0) {
        toggleOnce();
        return true;
    }
    if (isTransportCommand(command)) {
        if (reals::audio::Engine::instance().isPlaying()) {
            reals::audio::Engine::instance().stop();
            pushAudioState();
        }
    }
    return false;
}

int commandHookV1(const int command, const int /*val*/, const int /*valhw*/, const int /*relmode*/,
                  const HWND /*hwnd*/) {
    if (command == g_cmdId && g_cmdId != 0) {
        toggleOnce();
        return 1;
    }
    if (isTransportCommand(command)) {
        if (reals::audio::Engine::instance().isPlaying()) {
            reals::audio::Engine::instance().stop();
            pushAudioState();
        }
    }
    return 0;
}

} // namespace

extern "C" REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance,
                                                                 reaper_plugin_info_t* rec) {
    g_hInstance = hInstance;

    reals::platform::ensureDir(reals::platform::dataDir());
    reals::util::Log::init(reals::platform::joinPath(reals::platform::dataDir(), "reals_ext.log"));
    LOG_INFO(kTag, "entry: begin");

    if (!rec) {
        // Full teardown: unregister everything registered at load, release
        // bridge (joins lab workers) and webview (removes event handlers).
        plugin_register("-timer", reinterpret_cast<void*>(timerHook));
        plugin_register("-hookcommand2", reinterpret_cast<void*>(commandHook));
        plugin_register("-hookcommand", reinterpret_cast<void*>(commandHookV1));
        gaccel_register_t ga{};
        ga.accel.cmd = static_cast<decltype(ga.accel.cmd)>(g_cmdId);
        ga.desc = kCommandName;
        plugin_register("-gaccel", &ga);
        if (g_cmdId > 0)
            plugin_register("-command_id", const_cast<char*>(kCommandId));
        g_cmdId = 0;
        g_bridge.reset();
        g_web.reset();
        if (g_hwnd) {
            reals::shell::revokeFileDropTarget(nullptr);
            DestroyWindow(g_hwnd);
            g_hwnd = nullptr;
        }
        UnregisterClassW(kWndClass, g_hInstance);
        if (g_bgBrush) {
            DeleteObject(g_bgBrush);
            g_bgBrush = nullptr;
        }
        if (g_hIconBig) {
            DestroyIcon(g_hIconBig);
            g_hIconBig = nullptr;
        }
        if (g_hIconSm) {
            DestroyIcon(g_hIconSm);
            g_hIconSm = nullptr;
        }
        OleUninitialize();
        if (g_comOwned)
            CoUninitialize();
        LOG_INFO(kTag, "entry: unloaded");
        return 0;
    }

    try {
        // Force Trace level so all debug logs are captured to reals_ext.log
        reals::util::Log::setMinLevel(reals::util::LogLevel::Trace);
        LOG_INFO(kTag, "=== Reals Lab Plugin Starting ===");

        if (REAPERAPI_LoadAPI(rec->GetFunc) != 0) {
            LOG_ERROR(kTag, "entry: REAPERAPI_LoadAPI failed");
            return -1;
        }
        LOG_INFO(kTag, "entry: api loaded");

        reals::config::Config::instance().load();
        LOG_INFO(kTag, "entry: config ok");

        const char* resPath = GetResourcePath();
        reals::i18n::init(
            reals::platform::joinPath(resPath ? resPath : "", "RealsLab/assets/i18n").c_str());
        reals::i18n::setLanguage(reals::config::Config::instance().language());
        LOG_INFO(kTag, "entry: i18n ok");

        g_bridge = std::make_unique<reals::bridge::Bridge>(&g_actions);
        g_bridge->init();
        LOG_INFO(kTag, "entry: bridge ok");

        g_cmdId = plugin_register("command_id", const_cast<char*>(kCommandId));
        if (g_cmdId <= 0) {
            LOG_ERROR(kTag, "entry: command_id registration failed");
            return -1;
        }

        gaccel_register_t ga{};
        ga.accel.cmd = static_cast<decltype(ga.accel.cmd)>(g_cmdId);
        ga.desc = kCommandName;
        if (!plugin_register("gaccel", &ga)) {
            LOG_ERROR(kTag, "entry: gaccel registration failed");
            return -1;
        }
        if (!plugin_register("hookcommand2", reinterpret_cast<void*>(commandHook))) {
            LOG_ERROR(kTag, "entry: hookcommand2 registration failed");
            return -1;
        }
        plugin_register("hookcommand", reinterpret_cast<void*>(commandHookV1));

        char msg[64];
        std::snprintf(msg, sizeof(msg), "loaded, command id %d", g_cmdId);
        LOG_INFO(kTag, msg);

        // Warm WebView2 in the background so the first Show Window is instant.
        createHostWindow(false);
        return 1;
    } catch (const std::exception& e) {
        LOG_ERROR(kTag, e.what() ? e.what() : "std::exception");
        return -1;
    } catch (...) {
        LOG_ERROR(kTag, "entry: unknown exception");
        return -1;
    }
}

#else

// Non-Windows shells arrive via WKWebView/WebKitGTK in Phase 6 (SPEC.md).
int ReaperPluginEntry(void*, void*) {
    return 0;
}

#endif // _WIN32
