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
#include <cstring>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

#include <reaper_plugin.h>

// #region agent log
namespace {
void AgentDebugLog(const char* hypothesisId, const char* location, const char* message,
                   const nlohmann::json& data) {
    FILE* f = nullptr;
#if defined(_MSC_VER)
    fopen_s(&f, "C:\\Users\\smk28\\Desktop\\reals lab extension\\debug-50e7ab.log", "a");
#else
    f = std::fopen("C:\\Users\\smk28\\Desktop\\reals lab extension\\debug-50e7ab.log", "a");
#endif
    if (!f) return;
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count();
    const std::string line =
        nlohmann::json{{"sessionId", "50e7ab"},
                       {"hypothesisId", hypothesisId},
                       {"location", location},
                       {"message", message},
                       {"data", data},
                       {"timestamp", ms}}
            .dump();
    std::fprintf(f, "%s\n", line.c_str());
    std::fclose(f);
}
} // namespace
// #endregion

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
#define REAPERAPI_WANT_GetPlayPosition2Ex
#define REAPERAPI_WANT_GetCursorPosition
#define REAPERAPI_WANT_SetEditCurPos2
#define REAPERAPI_WANT_EnumProjects
#define REAPERAPI_WANT_GetAudioDeviceInfo
#define REAPERAPI_WANT_GetMasterTrack
#define REAPERAPI_WANT_Audio_RegHardwareHook
#define REAPERAPI_WANT_PCM_Source_CreateFromFileEx
#define REAPERAPI_WANT_PCM_Source_Destroy
#define REAPERAPI_WANT_PlayTrackPreview
#define REAPERAPI_WANT_PlayTrackPreview2
#define REAPERAPI_WANT_PlayTrackPreview2Ex
#define REAPERAPI_WANT_PlayPreview
#define REAPERAPI_WANT_PlayPreviewEx
#define REAPERAPI_WANT_StopTrackPreview
#define REAPERAPI_WANT_StopTrackPreview2
#define REAPERAPI_WANT_StopPreview
#define REAPERAPI_WANT_GetSetMediaItemTakeInfo
#define REAPERAPI_WANT_TimeMap2_timeToBeats
#define REAPERAPI_WANT_TimeMap_GetDividedBpmAtTime
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
#define REAPERAPI_WANT_UpdateItemInProject
#define REAPERAPI_WANT_CountTracks
#define REAPERAPI_WANT_GetTrack
#define REAPERAPI_WANT_CountTrackMediaItems
#define REAPERAPI_WANT_GetTrackMediaItem
#define REAPERAPI_WANT_GetMediaItemInfo_Value
#define REAPERAPI_WANT_SetMediaItemInfo_Value
#define REAPERAPI_WANT_GetExtState
#define REAPERAPI_WANT_SetExtState
#define REAPERAPI_WANT_ReaperGetPitchShiftAPI
#define REAPERAPI_WANT_EnumPitchShiftModes
#define REAPERAPI_WANT_EnumPitchShiftSubModes
#define REAPERAPI_WANT_InsertTrackAtIndex
#define REAPERAPI_WANT_GetSetMediaTrackInfo_String
#define REAPERAPI_WANT_SetTrackColor
#define REAPERAPI_WANT_CreateNewMIDIItemInProj
#define REAPERAPI_WANT_MIDI_GetPPQPosFromProjTime
#define REAPERAPI_WANT_MIDI_InsertNote
#define REAPERAPI_WANT_MIDI_InsertTextSysexEvt
#define REAPERAPI_WANT_MIDI_Sort
#define REAPERAPI_WANT_GetHZoomLevel
#define REAPERAPI_WANT_GetSet_ArrangeView2
#define REAPERAPI_WANT_TimeMap2_beatsToTime
#define REAPERAPI_WANT_CSurf_OnZoom
#define REAPERAPI_WANT_CSurf_OnScroll
#define REAPERAPI_WANT_MIDI_CountEvts
#define REAPERAPI_WANT_MIDI_GetProjTimeFromPPQPos
#define REAPERAPI_WANT_MIDI_GetTextSysexEvt
#define REAPERAPI_WANT_GetMediaItem_Track
#define REAPERAPI_WANT_GetMediaTrackInfo_Value
#define REAPERAPI_WANT_SetMediaTrackInfo_Value
#define REAPERAPI_WANT_AddMediaItemToTrack
#define REAPERAPI_WANT_AddTakeToMediaItem
#define REAPERAPI_WANT_SetMediaItemTake_Source
#define REAPERAPI_WANT_GetSetMediaItemTakeInfo_String
#define REAPERAPI_WANT_TrackList_AdjustWindows
#define REAPERAPI_WANT_GetThingFromPoint
#define REAPERAPI_WANT_GetSetProjectInfo
#define REAPERAPI_IMPLEMENT
#include <reaper_plugin_functions.h>

#include <miniaudio.h>
#include "reals/audio/DragExporter.h"
#include "reals/audio/Engine.h"
#include "reals/audio/ITimeStretchProcessor.h"
#include "reals/audio/PreviewLoopCursor.h"
#include "reals/bridge/Bridge.h"
#include "reals/config/Config.h"
#include "reals/embedded/EmbeddedAssets.h"
#include "reals/i18n/I18n.h"
#include "reals/lab/ChordEngine.h"
#include "reals/platform/Path.h"
#include "reals/util/Log.h"
#include "OleDrag.h"
#include "ReaperAgentTools.h"
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
    std::unordered_set<MediaItem*> preExistingItems;
};

std::vector<PendingPlayrate> g_pendingPlayrates;
std::mutex g_pendingMutex;

static std::unordered_set<MediaItem*> captureProjectMediaItems() {
    std::unordered_set<MediaItem*> items;
    if (CountTracks && GetTrack && CountTrackMediaItems && GetTrackMediaItem) {
        const int numTracks = CountTracks(0);
        for (int t = 0; t < numTracks; ++t) {
            MediaTrack* trk = GetTrack(0, t);
            if (!trk) continue;
            const int numItems = CountTrackMediaItems(trk);
            for (int m = 0; m < numItems; ++m) {
                MediaItem* itm = GetTrackMediaItem(trk, m);
                if (itm) items.insert(itm);
            }
        }
    }
    return items;
}

void queuePendingPlayrate(const std::string& path, double rate, double pitch = 0.0, const std::string& originalPath = "") {
    const std::lock_guard lock(g_pendingMutex);
    uint64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
    auto preExisting = captureProjectMediaItems();
    g_pendingPlayrates.push_back({path, originalPath, rate, pitch, now, 0, std::move(preExisting)});
    char msg[256];
    std::snprintf(msg, sizeof(msg), "queuePendingPlayrate: path=%s rate=%.4f pitch=%.2f preExisting=%zu",
                  path.c_str(), rate, pitch, g_pendingPlayrates.back().preExistingItems.size());
    LOG_INFO(kTag, msg);
}

static std::string getFilenameOnly(const std::string& path) {
    size_t pos = path.find_last_of("/\\");
    if (pos != std::string::npos) return path.substr(pos + 1);
    return path;
}

void processPendingSyncPlayrates() {
    std::lock_guard lock(g_pendingMutex);
    if (g_pendingPlayrates.empty())
        return;

    const int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    for (auto it = g_pendingPlayrates.begin(); it != g_pendingPlayrates.end(); ) {
        // Allow up to 15 seconds for user to complete the drag-and-drop gesture into REAPER
        if (now - it->queuedTime > 15000) {
            it = g_pendingPlayrates.erase(it);
            continue;
        }

        bool matchedAny = false;
        std::string normTarget = reals::platform::normalizePath(it->path);
        for (char& c : normTarget) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
        std::string targetBase = getFilenameOnly(normTarget);

        auto applyToTake = [&](MediaItem* item, MediaItem_Take* take, const std::string& rawPath = "") -> bool {
            if (!item || !take || !SetMediaItemTakeInfo_Value) return false;

            if (!it->originalPath.empty() && PCM_Source_CreateFromFileEx && GetSetMediaItemTakeInfo) {
                PCM_source* newSrc = PCM_Source_CreateFromFileEx(it->originalPath.c_str(), false);
                if (newSrc) {
                    PCM_source* oldSrc = static_cast<PCM_source*>(
                        GetSetMediaItemTakeInfo(take, "P_SOURCE", nullptr));
                    GetSetMediaItemTakeInfo(take, "P_SOURCE", newSrc);
                    if (oldSrc && oldSrc != newSrc) {
                        delete oldSrc;
                    }
                    SetMediaItemTakeInfo_Value(take, "D_PLAYRATE", it->playrate);
                    SetMediaItemTakeInfo_Value(take, "B_PPITCH", 1);
                    SetMediaItemTakeInfo_Value(take, "D_PITCH", it->pitchSemitones);
                    char msg[256];
                    std::snprintf(msg, sizeof(msg), "Mechanism C: Swapped source, playrate %.4f pitch %.2f", it->playrate, it->pitchSemitones);
                    LOG_INFO(kTag, msg);
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
            if (UpdateItemInProject) UpdateItemInProject(item);
            char msg[256];
            std::snprintf(msg, sizeof(msg), "Mechanism A: Synced item to playrate %.4f (pitch %.2f)", it->playrate, it->pitchSemitones);
            LOG_INFO(kTag, msg);
            return true;
        };

        auto checkAndApply = [&](MediaItem* item) -> bool {
            if (!item) return false;
            // -----------------------------------------------------------
            // TIMELINE ISOLATION SAFEGUARD:
            // If this MediaItem already existed BEFORE the current drag/insert operation,
            // NEVER touch it! (QUY TẮC: Không chỉnh sửa các item cũ đã nằm trên timeline).
            // -----------------------------------------------------------
            if (it->preExistingItems.count(item) > 0) {
                return false;
            }

            MediaItem_Take* take = GetActiveTake ? GetActiveTake(item) : nullptr;
            if (!take) return false;

            if (GetMediaItemTake_Source) {
                PCM_source* src = GetMediaItemTake_Source(take);
                while (src && src->GetSource()) {
                    src = src->GetSource();
                }
                if (src && src->GetFileName()) {
                    std::string rawSrcPath = src->GetFileName();
                    std::string srcPath = reals::platform::normalizePath(rawSrcPath);
                    for (char& c : srcPath) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
                    std::string srcBase = getFilenameOnly(srcPath);

                    if (srcPath == normTarget || srcBase == targetBase ||
                        (!it->originalPath.empty() && srcPath == it->originalPath)) {
                        return applyToTake(item, take, rawSrcPath);
                    }
                }
            }
            return false;
        };

        // 1. Check selected media items (newly dropped or inserted item is selected by REAPER)
        if (CountSelectedMediaItems && GetSelectedMediaItem) {
            int selCount = CountSelectedMediaItems(0);
            for (int i = 0; i < selCount; ++i) {
                MediaItem* item = GetSelectedMediaItem(0, i);
                if (checkAndApply(item)) {
                    matchedAny = true;
                }
            }
        }

        // 2. Also search all tracks & items in project (in case REAPER did not auto-select the dropped item)
        if (!matchedAny && CountTracks && GetTrack && CountTrackMediaItems && GetTrackMediaItem) {
            const int numTracks = CountTracks(0);
            for (int t = 0; t < numTracks && !matchedAny; ++t) {
                MediaTrack* trk = GetTrack(0, t);
                if (!trk) continue;
                const int numItems = CountTrackMediaItems(trk);
                for (int m = 0; m < numItems && !matchedAny; ++m) {
                    MediaItem* item = GetTrackMediaItem(trk, m);
                    if (checkAndApply(item)) {
                        matchedAny = true;
                    }
                }
            }
        }

        if (matchedAny) {
            if (UpdateArrange) UpdateArrange();
            it = g_pendingPlayrates.erase(it);
        } else {
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

struct LiveAudioTransportState {
    std::atomic<double> playPos{0.0};
    std::atomic<double> fullBeats{0.0};
    std::atomic<double> beatPhase{0.0}; // 0.0 - 1.0 within 1 beat
    std::atomic<double> bpm{120.0};
    std::atomic<int> playState{0};
    std::atomic<uint64_t> blockCounter{0};
    std::atomic<uint64_t> discontinuityCounter{0};
    // Host audio block duration in seconds (len / srate). The transport snapshot
    // read by the Bridge is this stale by the time a preview is audible, so the
    // phase-snap advances by it.
    std::atomic<double> blockLatencySeconds{0.0};
    double lastPos = -1.0;
};

static LiveAudioTransportState g_liveTransport;

struct ReaperAudioHookState {
    audio_hook_register_t hook{};
    bool isRegistered = false;

    void cleanup() {
        if (isRegistered && Audio_RegHardwareHook) {
            Audio_RegHardwareHook(false, &hook);
            isRegistered = false;
        }
    }
};

static ReaperAudioHookState g_audioHook;

// ============================================================================
// DspPreviewSource — Custom PCM_source wrapper with real-time élastique 3 Pro
// ============================================================================
// Wraps any underlying PCM_source and routes its audio through REAPER's native
// IReaperPitchShift engine inside GetSamples(). REAPER's PlayPreviewEx treats
// this as a standard PCM source, providing mastering-grade r8brain 64-bit
// resampling, Monitoring FX chain routing, and hardware output mixing.

// Locate "élastique 3.3.3 Pro / Normal" via EnumPitchShiftModes and return
// (mode<<16)+submode. NEVER fall back to -1 ("project default"): the user's
// REAPER preferences would silently change the preview DSP (a non-élastique
// default audibly alters the previewed sample's tone). Returns -1 only if the
// API is unavailable or the mode list does not contain it.
inline int elastiqueProQualityParam() {
    if (!EnumPitchShiftModes) return -1;
    for (int m = 0; ; ++m) {
        const char* modeName = nullptr;
        if (!EnumPitchShiftModes(m, &modeName)) break;
        if (!modeName) continue;
        // Name contains an 'é' that may arrive in any encoding — match the
        // ASCII-safe "3.3.3 Pro" suffix instead.
        if (std::strstr(modeName, "3.3.3 Pro") != nullptr) {
            return (m << 16) + 0; // submode 0 = Normal
        }
    }
    return -1;
}

class DspPreviewSource final : public PCM_source {
public:
    DspPreviewSource(PCM_source* underlyingSrc, double timeRatio, double pitchSemitones)
        : m_src(underlyingSrc), m_timeRatio(timeRatio), m_pitchSemitones(pitchSemitones) {
        if (m_src) {
            m_srate = m_src->GetSampleRate();
            m_rawLengthSeconds = m_src->GetLength();
            m_srcChannels = m_src->GetNumChannels();
            if (m_srate <= 0.0) m_srate = 44100.0;
            if (m_srcChannels <= 0) m_srcChannels = 2;
            // REAPER preview output is always stereo (block->nch == 2).
            // The shifter must also be stereo so its output stride matches
            // the interleaved stereo buffer that PlayPreviewEx provides.
            // Mono source audio is demuxed to L+R before feeding the shifter.
            m_nch = std::max(2, m_srcChannels);
        }
        constexpr int kRealtimeMaxFrames = 8192;
        m_rawReadBuf.resize(static_cast<size_t>(kRealtimeMaxFrames * m_srcChannels));
        m_shifterOutBuf.resize(static_cast<size_t>(kRealtimeMaxFrames * m_nch));
        initShifter();
        LOG_INFO(kTag, "DspPreviewSource CTOR: srcChannels=" + std::to_string(m_srcChannels) +
                       " nch(shifter)=" + std::to_string(m_nch) +
                       " srate=" + std::to_string(m_srate) +
                       " timeRatio=" + std::to_string(timeRatio) +
                       " pitch=" + std::to_string(pitchSemitones) +
                       " rawLen=" + std::to_string(m_src ? m_src->GetLength() : 0.0) +
                       " reportedLen=" + std::to_string(GetLength()));
    }

    ~DspPreviewSource() override {
        // The host preview is unregistered and reg.src is detached before this
        // destructor runs, so no new reader can start. Never free the DSP/source
        // under an in-flight audio callback: a fixed timeout can turn a slow
        // decoder block into a use-after-free.
        while (m_activeReaders.load(std::memory_order_acquire) > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        cleanup();
        if (m_src) {
            delete m_src;
            m_src = nullptr;
        }
    }

    DspPreviewSource(const DspPreviewSource&) = delete;
    DspPreviewSource& operator=(const DspPreviewSource&) = delete;

    PCM_source* Duplicate() override {
        PCM_source* dupSrc = m_src ? m_src->Duplicate() : nullptr;
        return new DspPreviewSource(dupSrc, m_timeRatio.load(std::memory_order_relaxed),
                                    m_pitchSemitones.load(std::memory_order_relaxed));
    }

    bool IsAvailable() override { return m_src ? m_src->IsAvailable() : false; }
    void SetAvailable(bool avail) override { if (m_src) m_src->SetAvailable(avail); }
    const char* GetType() override { return "DSP_PREVIEW"; }
    const char* GetFileName() override { return m_src ? m_src->GetFileName() : nullptr; }
    bool SetFileName(const char* newfn) override { return m_src ? m_src->SetFileName(newfn) : false; }
    PCM_source* GetSource() override { return m_src; }
    int GetNumChannels() override { return m_nch; }
    double GetSampleRate() override {
        // Return the operating rate (project rate once captured, else source rate).
        // REAPER PlayPreviewEx calls GetSamples() at project rate, so we must
        // report the same rate back so REAPER's timeline math is consistent.
        return m_operatingRate > 0.0 ? m_operatingRate : m_srate;
    }
    double GetLength() override {
        if (!m_src) return 0.0;
        const double r = m_timeRatio.load(std::memory_order_relaxed);
        // Self-healing loop cap: if the raw source actually ran out at
        // m_effectiveRawEnd (header length overstated real data — encoder
        // padding etc.), clamp the loop to the REAL end so REAPER wraps
        // exactly there instead of hitting EOF mid-loop (dead air) forever.
        double fullOutLen = r > 0.01 ? (m_src->GetLength() / r) : m_src->GetLength();
        const double effRaw = m_effectiveRawEnd.load(std::memory_order_relaxed);
        if (effRaw > 0.001 && r > 0.01) {
            fullOutLen = std::min(fullOutLen, effRaw / r);
        }
        // Bar-grid looping: wrap at the nominal loop instead of the full file so a
        // reverb tail / encoder padding does not push every cycle off the DAW grid.
        if (m_loopActive.load(std::memory_order_relaxed)) {
            const double loopBeats = m_loopBeats.load(std::memory_order_relaxed);
            const double sBpm = m_loopSampleBpm.load(std::memory_order_relaxed);
            if (loopBeats > 0.0 && sBpm > 0.0 && r > 0.01) {
                const double nominalOutLen = (loopBeats * 60.0) / (sBpm * r);
                if (nominalOutLen > 0.0)
                    return std::min(fullOutLen, nominalOutLen);
            }
        }
        return fullOutLen;
    }
    int GetBitsPerSample() override { return m_src ? m_src->GetBitsPerSample() : 16; }
    int PropertiesWindow(HWND hwndParent) override { return m_src ? m_src->PropertiesWindow(hwndParent) : 0; }

    void GetSamples(PCM_source_transfer_t* block) override {
        if (!block || block->length <= 0 || !block->samples || !m_src) {
            if (block) block->samples_out = 0;
            return;
        }

        struct ReaderGuard {
            std::atomic<int>& count;
            explicit ReaderGuard(std::atomic<int>& c) : count(c) { count.fetch_add(1, std::memory_order_acquire); }
            ~ReaderGuard() { count.fetch_sub(1, std::memory_order_release); }
        };
        ReaderGuard rg(m_activeReaders);

        if (!m_src) {
            block->samples_out = 0;
            return;
        }

        // Capture operating sample rate from REAPER's first GetSamples call.
        // PlayPreviewEx calls us at PROJECT rate (e.g. 48000), which may differ
        // from the source file rate (e.g. 44100). We must operate at project
        // rate so output frames match REAPER's timeline expectations.
        const double blockRate = block->samplerate > 0.0 ? block->samplerate : m_srate;
        if (m_operatingRate <= 0.0 && blockRate > 0.0) {
            m_operatingRate = blockRate;
            if (m_shifter && std::abs(m_operatingRate - m_srate) > 1.0) {
                m_shifter->set_srate(m_operatingRate);
            }
        }

        // Apply updated parameters if changed from UI thread
        if (m_paramsDirty.exchange(false, std::memory_order_acq_rel)) {
            applyParams();
        }

        const double currentRatio = m_timeRatio.load(std::memory_order_relaxed);
        const double currentPitch = m_pitchSemitones.load(std::memory_order_relaxed);
        constexpr int kRealtimeMaxFrames = 8192;
        int requestedLength = std::min(block->length, kRealtimeMaxFrames);

        const double outputBoundary = GetLength();
        const double timelineTime = block->time_s;

        // Keep the host callback on the same output-frame grid as the source
        // boundary.  REAPER normally asks for one full hardware block even
        // when only a few frames remain before a looping preview wraps.  If we
        // pass that request through unchanged, the decoder/DSP can emit frames
        // from the next cycle while the host still attributes them to the old
        // cycle (the old buffered path was observed running hundreds of ms
        // past this point).  Return only the frames that belong to this
        // timeline segment; the next callback starts at the wrapped cursor.
        if (outputBoundary > 0.0 && std::isfinite(outputBoundary)) {
            const int boundaryFrames = reals::audio::PreviewLoopCursor::FramesUntilBoundary(
                timelineTime, outputBoundary, blockRate, requestedLength);
            if (boundaryFrames < requestedLength) {
                requestedLength = boundaryFrames;
                if (requestedLength <= 0) {
                    block->samples_out = 0;
                    m_lastTimelinePos = outputBoundary;
                    const bool loopActive = m_loopActive.load(std::memory_order_relaxed);
                    m_eofReached = false;
                    m_streamFinished.store(!loopActive, std::memory_order_relaxed);
                    return;
                }
            }
        }
        const int outChannels = block->nch > 0 ? block->nch : m_nch;

        // ── Bit-perfect bypass: ratio ≈ 1.0 and pitch ≈ 0.0 ──
        // Read directly from underlying source without élastique processing.
        // Handles mono→stereo or multichannel routing cleanly without buffer overrun.
        const bool isBypass = (std::abs(currentRatio - 1.0) <= 0.003 &&
                               std::abs(currentPitch) <= 0.02);
        if (isBypass) {
            if (m_srcChannels == outChannels) {
                // Channels match — direct passthrough, zero overhead
                PCM_source_transfer_t limited = *block;
                limited.length = requestedLength;
                limited.samples_out = 0;
                m_src->GetSamples(&limited);
                block->samples_out = std::clamp(limited.samples_out, 0, requestedLength);
                m_rawTimePos = timelineTime + static_cast<double>(block->samples_out) / blockRate;
                m_lastTimelinePos = timelineTime + static_cast<double>(block->samples_out) / blockRate;
                // A zero-frame read at a loop boundary is a transient decoder
                // result while REAPER rewinds the preview cursor. It is not a
                // terminal EOF when the host registration is looping.
                const bool loopActive = m_loopActive.load(std::memory_order_relaxed);
                m_streamFinished.store(block->samples_out == 0 && !loopActive,
                                       std::memory_order_relaxed);
                return;
            }
            PCM_source_transfer_t rawTransfer{};
            rawTransfer.time_s = timelineTime;
            rawTransfer.samplerate = blockRate;
            rawTransfer.nch = m_srcChannels;
            rawTransfer.length = requestedLength;
            rawTransfer.samples = m_rawReadBuf.data();
            rawTransfer.samples_out = 0;
            m_src->GetSamples(&rawTransfer);
            const int got = rawTransfer.samples_out;

            if (outChannels == 1) {
                // Stereo/multichannel downmixed to mono
                for (int i = 0; i < got; ++i) {
                    double sum = 0.0;
                    for (int ch = 0; ch < m_srcChannels; ++ch) sum += m_rawReadBuf[i * m_srcChannels + ch];
                    block->samples[i] = static_cast<ReaSample>(sum / m_srcChannels);
                }
            } else {
                // Mono or stereo mapped to stereo / multichannel output
                for (int i = 0; i < got; ++i) {
                    const ReaSample sL = m_rawReadBuf[i * m_srcChannels];
                    const ReaSample sR = (m_srcChannels > 1) ? m_rawReadBuf[i * m_srcChannels + 1] : sL;
                    block->samples[i * outChannels + 0] = sL;
                    block->samples[i * outChannels + 1] = sR;
                    for (int ch = 2; ch < outChannels; ++ch) {
                        block->samples[i * outChannels + ch] = 0.0;
                    }
                }
            }
            block->samples_out = got;
            m_rawTimePos = timelineTime + static_cast<double>(got) / blockRate;
            m_lastTimelinePos = timelineTime + static_cast<double>(got) / blockRate;
            // Keep a looping preview alive across a transient zero-frame read;
            // REAPER will issue the next request at the wrapped cursor.
            const bool loopActive = m_loopActive.load(std::memory_order_relaxed);
            m_streamFinished.store(got == 0 && !loopActive, std::memory_order_relaxed);
            return;
        }

        // ── DSP path: route through IReaperPitchShift (élastique 3 Pro) ──
        if (!m_shifter) {
            block->samples_out = 0;
            return;
        }

        // Detect seek / transport discontinuity
        const double expectedTime = m_lastTimelinePos;
        constexpr double kSeekThresholdSeconds = 0.02;  // reposition trigger (20ms)
        constexpr double kResetMinJumpSeconds = 0.35;   // only REAL seeks reset élastique
        if (m_lastTimelinePos < 0.0 || std::abs(timelineTime - expectedTime) > kSeekThresholdSeconds) {
            const double jump = std::abs(timelineTime - expectedTime);
            // Distinguish a LOOP WRAP (REAPER wrapped curpos back to the loop
            // start) from a real user seek. A wrap must keep the élastique
            // pipeline primed: Reset() here drained the whole lookahead buffer
            // and left dead air before every loop cycle (audible off-beat gap).
            // NOTE: deliberately NOT gated on m_loopActive — REAPER's own
            // reg.loop wraps at GetLength() whether or not our nominal bar
            // loop boundary is active (e.g. loopBeats/sampleBpm missing).
            const double len = GetLength();
            const bool isLoopWrap =
                m_lastTimelinePos >= 0.0 &&
                len > 0.0 &&
                expectedTime >= len * 0.75 &&   // was playing near the loop end
                timelineTime < expectedTime && // jumped BACKWARDS
                timelineTime <= 1.0;           // now at the loop start
            const bool softSeek = m_softSeekArmed.exchange(false, std::memory_order_acq_rel);
            // The raw feeder is a continuous, modulo loop stream.  At a host
            // timeline wrap the elastique instance can still have lookahead
            // frames queued from the end of the previous cycle; rewinding this
            // cursor to block->time_s here would feed the loop head a second
            // time and move the first transient of the new cycle.  Keep the
            // feeder cursor continuous across a host wrap.  Explicit phase
            // seeks still map output time back to the raw source below.  If a
            // phase-timer soft-seek flag happens to arrive in the same host
            // block as the actual loop wrap, the host wrap wins: the elastique
            // lookahead already contains the loop head and rewinding here
            // would feed those frames twice.
            if (!isLoopWrap) {
                m_rawTimePos = timelineTime * currentRatio;
            }
            m_eofReached = false;
            m_streamFinished.store(false, std::memory_order_relaxed);
            // A genuine (non-soft) user seek re-learns the effective data end.
            if (!softSeek && !isLoopWrap) {
                m_effectiveRawEnd.store(0.0, std::memory_order_relaxed);
            }
            // Small jumps (REAPER scheduling jitter 21-300ms) and soft seeks
            // (phase-timer re-alignments) must NEVER reset élastique — a reset
            // re-primes the entire lookahead and lands as dead air mid-loop.
            const bool willReset = !isLoopWrap && !softSeek &&
                (m_lastTimelinePos < 0.0 || jump > kResetMinJumpSeconds);
            // #region agent log
            AgentDebugLog("B,E", "reaper_plugin.cpp:DspPreviewSource", "dsp_seek_decision",
                          {{"jump", jump},
                           {"softSeek", softSeek},
                           {"isLoopWrap", isLoopWrap},
                           {"willReset", willReset},
                           {"timelineTime", timelineTime},
                           {"expectedTime", expectedTime},
                           {"kResetMinJumpSeconds", kResetMinJumpSeconds}});
            // #endregion
            if (willReset) {
                m_shifter->Reset();
            }
        }

        int totalFramesRendered = 0;
        constexpr int kChunkFrames = 2048;

        const bool needsDemux = (m_srcChannels < m_nch);

        int maxIterations = (requestedLength / kChunkFrames + 4) * 4;
        while (totalFramesRendered < requestedLength && maxIterations-- > 0) {
            const int remaining = requestedLength - totalFramesRendered;
            ReaSample* outPtr = (outChannels == 2)
                ? (block->samples + (totalFramesRendered * 2))
                : m_shifterOutBuf.data();

            const int got = m_shifter->GetSamples(remaining, outPtr);
            if (got > 0) {
                if (outChannels != 2) {
                    if (outChannels == 1) {
                        for (int i = 0; i < got; ++i) {
                            block->samples[totalFramesRendered + i] = 0.5 * (m_shifterOutBuf[i * 2] + m_shifterOutBuf[i * 2 + 1]);
                        }
                    } else {
                        for (int i = 0; i < got; ++i) {
                            const int dstIdx = (totalFramesRendered + i) * outChannels;
                            block->samples[dstIdx + 0] = m_shifterOutBuf[i * 2 + 0];
                            block->samples[dstIdx + 1] = m_shifterOutBuf[i * 2 + 1];
                            for (int ch = 2; ch < outChannels; ++ch) {
                                block->samples[dstIdx + ch] = 0.0;
                            }
                        }
                    }
                }
                totalFramesRendered += got;
                if (totalFramesRendered >= requestedLength) break;
            }

            if (m_eofReached) {
                if (m_loopActive.load(std::memory_order_relaxed)) {
                    // Seamless loop: the raw source ran out but the preview is
                    // looping — wrap the read position back to the start and
                    // KEEP feeding. FlushSamples+break here drained élastique
                    // and forced a full re-prime (dead air) on every cycle.
                    m_rawTimePos = 0.0;
                    m_eofReached = false;
                    m_streamFinished.store(false, std::memory_order_relaxed);
                    // fall through to the "feed more raw audio" step below
                } else {
                    m_shifter->FlushSamples();
                    const int remainingFlush = requestedLength - totalFramesRendered;
                    ReaSample* flushPtr = (outChannels == 2)
                        ? (block->samples + (totalFramesRendered * 2))
                        : m_shifterOutBuf.data();
                    const int flushed = m_shifter->GetSamples(remainingFlush, flushPtr);
                    if (flushed > 0) {
                        if (outChannels != 2) {
                            if (outChannels == 1) {
                                for (int i = 0; i < flushed; ++i) {
                                    block->samples[totalFramesRendered + i] = 0.5 * (m_shifterOutBuf[i * 2] + m_shifterOutBuf[i * 2 + 1]);
                                }
                            } else {
                                for (int i = 0; i < flushed; ++i) {
                                    const int dstIdx = (totalFramesRendered + i) * outChannels;
                                    block->samples[dstIdx + 0] = m_shifterOutBuf[i * 2 + 0];
                                    block->samples[dstIdx + 1] = m_shifterOutBuf[i * 2 + 1];
                                    for (int ch = 2; ch < outChannels; ++ch) {
                                        block->samples[dstIdx + ch] = 0.0;
                                    }
                                }
                            }
                        }
                        totalFramesRendered += flushed;
                    }
                    break;
                }
            }

            // Feed more raw audio from underlying source into pitch shifter.
            // When a nominal bar loop is active, stop each read at the raw
            // loop boundary.  Reading through encoder/reverb tail bytes until
            // the physical file EOF makes the host wrap while elastique is
            // still outputting tail content; the next cycle then starts with
            // a deterministic phase offset.  Splitting the read keeps the
            // pitch-shifter pipeline full while presenting an exact periodic
            // raw stream to it.
            const double rawLoopLength = loopRawLengthSeconds();
            int rawReadLength = kChunkFrames;
            if (rawLoopLength > 0.0) {
                const double blockRateForLoop = blockRate > 0.0 ? blockRate : m_srate;
                rawReadLength = reals::audio::PreviewLoopCursor::FramesUntilBoundary(
                    m_rawTimePos, rawLoopLength, blockRateForLoop, kChunkFrames);
                // A source read can leave a sub-frame rounding remainder at
                // the boundary. Wrap that remainder before requesting again;
                // never ask the decoder for frames beyond the nominal loop.
                if (rawReadLength == 0) {
                    m_rawTimePos = 0.0;
                    m_zeroReadAtHeadCount = 0;
                    rawReadLength = reals::audio::PreviewLoopCursor::FramesUntilBoundary(
                        m_rawTimePos, rawLoopLength, blockRateForLoop, kChunkFrames);
                }
            }
            PCM_source_transfer_t rawTransfer{};
            rawTransfer.time_s = m_rawTimePos;
            rawTransfer.samplerate = blockRate; // Read at project rate — PCM_source resamples internally
            rawTransfer.nch = m_srcChannels;    // Read in source's native channel count
            rawTransfer.length = rawReadLength;
            rawTransfer.samples = m_rawReadBuf.data();
            rawTransfer.samples_out = 0;

            m_src->GetSamples(&rawTransfer);

            if (rawTransfer.samples_out > 0) {
                m_zeroReadAtHeadCount = 0;
                m_rawTimePos = rawLoopLength > 0.0
                    ? reals::audio::PreviewLoopCursor::AdvanceWrapped(
                          m_rawTimePos, rawTransfer.samples_out, blockRate, rawLoopLength)
                    : m_rawTimePos + static_cast<double>(rawTransfer.samples_out) / blockRate;
                ReaSample* inBuf = m_shifter->GetBuffer(rawTransfer.samples_out);
                if (inBuf) {
                    if (needsDemux) {
                        for (int i = 0; i < rawTransfer.samples_out; ++i) {
                            const ReaSample s = m_rawReadBuf[i * m_srcChannels];
                            for (int ch = 0; ch < m_nch; ++ch) {
                                inBuf[i * m_nch + ch] = s;
                            }
                        }
                    } else {
                        std::memcpy(inBuf, m_rawReadBuf.data(),
                                    static_cast<size_t>(rawTransfer.samples_out * m_nch) * sizeof(ReaSample));
                    }
                    m_shifter->BufferDone(rawTransfer.samples_out);
                }
                // Keep the logical raw cursor inside the nominal loop.  This
                // handles resampler rounding where samples_out is one frame
                // larger than the floor() bound above without ever exposing
                // the source's tail to the next cycle.
                if (rawLoopLength > 0.0 && m_rawTimePos == 0.0) {
                    m_zeroReadAtHeadCount = 0;
                }
            } else if (m_loopActive.load(std::memory_order_relaxed)) {
                    // A zero-frame read can be either a transient decoder
                    // underrun immediately after REAPER rewinds, or the real
                    // end of the source.  The old `rawTimePos <= 0.5` test
                    // classified every short file as a head underrun: a
                    // 350 ms one-shot/loop then stayed at its end forever and
                    // never reached the wrap path.  Use the source's actual
                    // reported length to distinguish the two cases.  Keep a
                    // single retry at the head; at the source end always wrap.
                    const double rawLength = m_rawLengthSeconds > 0.0
                        ? m_rawLengthSeconds
                        : m_src->GetLength();
                    const double headTolerance = std::max(0.005, 2.0 / blockRate);
                    const bool atSourceEnd = rawLength > 0.0 &&
                        m_rawTimePos >= rawLength - headTolerance;
                    if (!atSourceEnd && m_rawTimePos <= headTolerance &&
                        m_zeroReadAtHeadCount < 1) {
                        ++m_zeroReadAtHeadCount;
                        if (m_diagCounter < 80) {
                            LOG_WARN(kTag, "DspPreviewSource: LOOP-HEAD UNDERRUN (rawPos=" +
                                               std::to_string(m_rawTimePos) +
                                               ", rawLen=" + std::to_string(rawLength) +
                                               ") — retrying next block");
                            ++m_diagCounter;
                        }
                        continue;
                    }
                    m_zeroReadAtHeadCount = 0;
                    // EOF mid-loop at a position beyond the loop head: the raw
                    // source ran out past the nominal loop (tail) — wrap read
                    // position and keep feeding, do NOT mark EOF. Record the
                    // REAL data end so GetLength() clamps the loop there
                    // (self-heal: no more mid-loop EOF on following cycles).
                    m_effectiveRawEnd.store(std::max(m_effectiveRawEnd.load(std::memory_order_relaxed), m_rawTimePos), std::memory_order_relaxed);
                    m_rawTimePos = 0.0;
                    m_eofReached = false;
                    if (m_diagCounter < 80) {
                        LOG_INFO(kTag, "DspPreviewSource: TAIL EOF — wrapped rawPos to 0 (pipeline kept)");
                        ++m_diagCounter;
                    }
                } else {
                    m_eofReached = true;
                }
        }

        block->samples_out = totalFramesRendered;
        m_lastTimelinePos = block->time_s + static_cast<double>(totalFramesRendered) / blockRate;
        if (totalFramesRendered == 0 && m_eofReached) {
            m_streamFinished.store(true, std::memory_order_relaxed);
        }
    }

    void GetPeakInfo(PCM_source_peaktransfer_t* block) override {
        if (m_src) m_src->GetPeakInfo(block);
    }
    void SaveState(ProjectStateContext* ctx) override { if (m_src) m_src->SaveState(ctx); }
    int LoadState(const char* firstline, ProjectStateContext* ctx) override {
        return m_src ? m_src->LoadState(firstline, ctx) : -1;
    }
    void Peaks_Clear(bool deleteFile) override { if (m_src) m_src->Peaks_Clear(deleteFile); }
    int PeaksBuild_Begin() override { return m_src ? m_src->PeaksBuild_Begin() : 0; }
    int PeaksBuild_Run() override { return m_src ? m_src->PeaksBuild_Run() : 0; }
    void PeaksBuild_Finish() override { if (m_src) m_src->PeaksBuild_Finish(); }

    void setTimeRatio(double ratio) {
        m_timeRatio.store(std::clamp(ratio, 0.25, 4.0), std::memory_order_relaxed);
        m_paramsDirty.store(true, std::memory_order_release);
        m_diagCounter = 0; // Reset diag to log after param change
        LOG_INFO(kTag, "DspPreviewSource::setTimeRatio(" + std::to_string(ratio) +
                       ") → clamped=" + std::to_string(m_timeRatio.load()) +
                       " newGetLength=" + std::to_string(GetLength()));
    }

    void setPitchSemitones(double semitones) {
        m_pitchSemitones.store(std::clamp(semitones, -12.0, 12.0), std::memory_order_relaxed);
        m_paramsDirty.store(true, std::memory_order_release);
    }

    // Enable bar-grid looping: GetLength() then reports the nominal loop
    // (loopBeats at the sample's native BPM, scaled by the live time ratio)
    // so PlayPreviewEx wraps on the DAW bar instead of at the full file end.
    void setLoopBoundary(bool active, double loopBeats, double sampleBpm) {
        m_loopActive.store(active, std::memory_order_relaxed);
        m_loopBeats.store(loopBeats, std::memory_order_relaxed);
        m_loopSampleBpm.store(sampleBpm, std::memory_order_relaxed);
        LOG_INFO(kTag, "DspPreviewSource::setLoopBoundary(active=" + std::to_string(active) +
                       ", loopBeats=" + std::to_string(loopBeats) +
                       ", sampleBpm=" + std::to_string(sampleBpm) +
                       ") → GetLength=" + std::to_string(GetLength()));
    }

    // Flip bar-grid looping on/off using the previously stored loop metrics.
    void setLoopActive(bool active) {
        m_loopActive.store(active, std::memory_order_relaxed);
    }

    [[nodiscard]] double getTimeRatio() const { return m_timeRatio.load(std::memory_order_relaxed); }
    [[nodiscard]] double getPitchSemitones() const { return m_pitchSemitones.load(std::memory_order_relaxed); }
    [[nodiscard]] bool isStreamFinished() const { return m_streamFinished.load(std::memory_order_relaxed); }

    // Arm a soft seek: the NEXT position jump (phase-timer re-alignment) is
    // applied WITHOUT resetting élastique — content jumps but the pipeline
    // keeps flowing, so realignment never lands as dead air.
    void armSoftSeek() { m_softSeekArmed.store(true, std::memory_order_release); }

private:
    // Return the raw-source duration that belongs to one musical loop cycle.
    // The host-facing GetLength() is in output-time seconds (after tempo
    // stretching), while m_rawTimePos is in the decoder's source-time domain.
    // Keeping this conversion in one place prevents a tail/padding segment from
    // being fed into the next elastique cycle.
    [[nodiscard]] double loopRawLengthSeconds() const {
        if (!m_loopActive.load(std::memory_order_relaxed)) return 0.0;
        const double loopBeats = m_loopBeats.load(std::memory_order_relaxed);
        const double sampleBpm = m_loopSampleBpm.load(std::memory_order_relaxed);
        if (loopBeats <= 0.0 || sampleBpm <= 30.0) return 0.0;

        const double nominal = (loopBeats * 60.0) / sampleBpm;
        if (nominal <= 0.0) return 0.0;

        // Do not claim more raw data than the decoder advertises.  If a file is
        // shorter than its metadata-derived nominal loop, the physical end (or
        // the self-healed end learned after the first short read) is the only
        // safe wrap point available.
        double available = m_rawLengthSeconds;
        const double effectiveEnd = m_effectiveRawEnd.load(std::memory_order_relaxed);
        if (effectiveEnd > 0.001) {
            available = available > 0.001
                ? std::min(available, effectiveEnd)
                : effectiveEnd;
        }
        if (available > 0.001) return std::min(nominal, available);
        return nominal;
    }

    void initShifter() {
        if (ReaperGetPitchShiftAPI) {
            m_shifter = ReaperGetPitchShiftAPI(REAPER_PITCHSHIFT_API_VER);
            if (m_shifter) {
                m_shifter->set_srate(m_srate);
                m_shifter->set_nch(m_nch);
                m_shifter->SetQualityParameter(elastiqueProQualityParam());
                applyParams();
            }
        }
    }

    void cleanup() {
        if (m_shifter) {
            delete m_shifter;
            m_shifter = nullptr;
        }
    }

    void applyParams() {
        if (!m_shifter) return;
        const double r = m_timeRatio.load(std::memory_order_relaxed);
        const double p = m_pitchSemitones.load(std::memory_order_relaxed);
        m_shifter->set_tempo(r);
        const double shiftRatio = std::pow(2.0, p / 12.0);
        m_shifter->set_shift(shiftRatio);
    }

    PCM_source* m_src = nullptr;
    IReaperPitchShift* m_shifter = nullptr;
    std::atomic<double> m_timeRatio{1.0};
    std::atomic<double> m_pitchSemitones{0.0};
    std::atomic<bool> m_paramsDirty{false};
    std::atomic<bool> m_loopActive{false};
    std::atomic<double> m_loopBeats{0.0};
    std::atomic<double> m_loopSampleBpm{0.0};
    std::atomic<int> m_activeReaders{0};
    std::atomic<bool> m_streamFinished{false};
    std::atomic<bool> m_softSeekArmed{false}; // next position jump skips élastique Reset
    std::atomic<double> m_effectiveRawEnd{0.0}; // real raw-data end discovered on EOF wrap (self-healing loop cap)
    double m_srate = 44100.0;
    // Length reported by the underlying decoder in its native time domain.
    // Keep this immutable on the audio thread so EOF classification remains
    // correct for short files (<500 ms) as well as normal loops.
    double m_rawLengthSeconds = 0.0;
    double m_operatingRate = 0.0;  // Project rate captured from first GetSamples() call
    int m_srcChannels = 2;   // Original source channel count (may be 1 for mono)
    int m_nch = 2;           // Shifter/output channel count (always >= 2)
    double m_rawTimePos = 0.0;
    double m_lastTimelinePos = -1.0;
    bool m_eofReached = false;
    int m_zeroReadAtHeadCount = 0; // audio-thread-only bounded retry counter
    std::vector<ReaSample> m_rawReadBuf;
    std::vector<ReaSample> m_shifterOutBuf; // Scratch buffer for non-stereo output channel mapping
    int m_diagCounter = 0;                  // Diagnostic: limit log spam
};

struct ReaperHostPreviewState {
    // Serializes launch/stop and every UI/timer access to the raw DSP pointer.
    // REAPER's audio callback never takes this mutex; it stays real-time safe.
    mutable std::recursive_mutex lifecycleMutex;
    preview_register_t reg{};
    DspPreviewSource* dspWrapper = nullptr;
    std::string currentPath;
    double durationSeconds = 0.0;
    std::atomic<bool> isPlaying{false};
    std::atomic<bool> registrationActive{false};
    bool csInitialized = false;

    void initCS() {
        const std::lock_guard<std::recursive_mutex> lifecycleLock(lifecycleMutex);
        if (!csInitialized) {
#ifdef _WIN32
            InitializeCriticalSection(&reg.cs);
#else
            pthread_mutex_init(&reg.mutex, nullptr);
#endif
            csInitialized = true;
        }
    }

    void stopAndClear() {
        const std::lock_guard<std::recursive_mutex> lifecycleLock(lifecycleMutex);
        if (!csInitialized) return;

        // 1. Tell REAPER to stop playing OUTSIDE the critical section.
        // Calling StopPreview outside reg.cs prevents ABBA deadlocks where REAPER's
        // audio preview thread is blocked on reg.cs while StopPreview is waiting for it!
        isPlaying.store(false, std::memory_order_release);
        // Treat an armed launch as registered for cleanup purposes.  The flag
        // is set before PlayPreviewEx() so a failed/partially accepted launch
        // still receives StopPreview() before its source is destroyed.
        if (registrationActive.exchange(false, std::memory_order_acq_rel)) {
            if (StopPreview) {
                StopPreview(&reg);
            }
        }

        // 2. Detach pointers under critical section lock
        PCM_source* srcToDelete = nullptr;
        {
#ifdef _WIN32
            EnterCriticalSection(&reg.cs);
#else
            pthread_mutex_lock(&reg.mutex);
#endif
            srcToDelete = reg.src;
            reg.src = nullptr;
            dspWrapper = nullptr;
            currentPath.clear();
            durationSeconds = 0.0;
#ifdef _WIN32
            LeaveCriticalSection(&reg.cs);
#else
            pthread_mutex_unlock(&reg.mutex);
#endif
        }

        // 3. Delete detached source OUTSIDE the lock.
        // DspPreviewSource destructor safely waits for any active reader to finish.
        if (srcToDelete) {
            delete srcToDelete;
        }
    }

    ~ReaperHostPreviewState() {
        stopAndClear();
        if (csInitialized) {
#ifdef _WIN32
            DeleteCriticalSection(&reg.cs);
#else
            pthread_mutex_destroy(&reg.mutex);
#endif
            csInitialized = false;
        }
    }
};

class ReaperPitchShiftProcessor final : public reals::audio::ITimeStretchProcessor {
public:
    ReaperPitchShiftProcessor() {
        initShifter();
    }

    ~ReaperPitchShiftProcessor() override {
        cleanup();
    }

    void setSampleRate(int sampleRate) override {
        if (sampleRate <= 0) return;
        m_sampleRate = sampleRate;
        if (m_shifter) m_shifter->set_srate(static_cast<double>(sampleRate));
        applyParams();
    }

    void setSampleRates(int inSampleRate, int outSampleRate) override {
        if (inSampleRate > 0) m_inputSampleRate = inSampleRate;
        if (outSampleRate > 0) m_sampleRate = outSampleRate;
        if (m_shifter && m_sampleRate > 0) {
            m_shifter->set_srate(static_cast<double>(m_sampleRate));
        }
        applyParams();
    }

    [[nodiscard]] int getSampleRate() const override {
        return m_sampleRate;
    }

    void setChannels(int channels) override {
        if (channels <= 0) return;
        m_channels = channels;
        if (m_shifter) m_shifter->set_nch(channels);
    }

    [[nodiscard]] int getChannels() const override {
        return m_channels;
    }

    void setTimeRatio(float ratio) override {
        m_timeRatio = ratio;
        applyParams();
    }

    [[nodiscard]] float getTimeRatio() const override {
        return m_timeRatio;
    }

    void setPitchSemitones(float semitones) override {
        m_pitchSemitones = semitones;
        applyParams();
    }

    void applyParams() {
        if (!m_shifter) return;
        m_shifter->set_tempo(static_cast<double>(m_timeRatio));
        const double shiftRatio = std::pow(2.0, static_cast<double>(m_pitchSemitones) / 12.0);
        m_shifter->set_shift(shiftRatio);
    }

    [[nodiscard]] float getPitchSemitones() const override {
        return m_pitchSemitones;
    }

    void putSamples(const float* interleavedSamples, size_t numFrames) override {
        if (!m_shifter || !interleavedSamples || numFrames == 0) return;

        ReaSample* inBuf = m_shifter->GetBuffer(static_cast<int>(numFrames));
        if (!inBuf) return;

        const size_t totalSamples = numFrames * static_cast<size_t>(m_channels);
        for (size_t i = 0; i < totalSamples; ++i) {
            inBuf[i] = static_cast<ReaSample>(interleavedSamples[i]);
        }
        m_shifter->BufferDone(static_cast<int>(numFrames));
    }

    [[nodiscard]] size_t receiveSamples(float* outInterleavedSamples, size_t maxFrames) override {
        if (!m_shifter || !outInterleavedSamples || maxFrames == 0) return 0;

        const size_t neededReaSamples = maxFrames * static_cast<size_t>(m_channels);
        if (m_tempReaBuffer.size() < neededReaSamples) {
            m_tempReaBuffer.resize(neededReaSamples);
        }

        const int framesReceived = m_shifter->GetSamples(static_cast<int>(maxFrames), m_tempReaBuffer.data());
        if (framesReceived <= 0) return 0;

        const size_t totalSamples = static_cast<size_t>(framesReceived) * static_cast<size_t>(m_channels);
        for (size_t i = 0; i < totalSamples; ++i) {
            outInterleavedSamples[i] = static_cast<float>(m_tempReaBuffer[i]);
        }
        return static_cast<size_t>(framesReceived);
    }

    [[nodiscard]] size_t numSamplesAvailable() const override {
        return 0;
    }

    void clear() override {
        if (m_shifter) m_shifter->Reset();
    }

    void flush() override {
        if (m_shifter) m_shifter->FlushSamples();
    }

    [[nodiscard]] int latencyFrames() const override {
        return 2048;
    }

private:
    void initShifter() {
        if (ReaperGetPitchShiftAPI) {
            m_shifter = ReaperGetPitchShiftAPI(REAPER_PITCHSHIFT_API_VER);
            if (m_shifter) {
                m_shifter->set_srate(static_cast<double>(m_sampleRate));
                m_shifter->set_nch(m_channels);

                int qualityParam = elastiqueProQualityParam();
                m_shifter->SetQualityParameter(qualityParam);
                applyParams();
                LOG_INFO(kTag, "IReaperPitchShift initialized (qualityParam=" + std::to_string(qualityParam) + ", elastique 3.3.3 Pro locked)");
            }
        }
    }

    void cleanup() {
        if (m_shifter) {
            delete m_shifter;
            m_shifter = nullptr;
        }
    }

    IReaperPitchShift* m_shifter = nullptr;
    int m_inputSampleRate = 48000;
    int m_sampleRate = 48000;
    int m_channels = 2;
    float m_timeRatio = 1.0f;
    float m_pitchSemitones = 0.0f;
    std::vector<ReaSample> m_tempReaBuffer;
};

static ReaperHostPreviewState g_hostPreview;

// Spacebar preview cycle guard: when Reals Lab itself starts/stops the DAW
// transport for the preview-cycle workflow, commandHook must NOT kill the
// running sample preview (the whole point of the cycle is preview + DAW
// playing together). Set around Main_OnCommand(1007/1016) calls.
static std::atomic<bool> g_previewCycleGuard{false};
// One-shot Space-start timing handoff. The audio thread only writes atomics;
// hostTransport() publishes the diagnostic later from the UI/timer thread.
static std::atomic<int64_t> g_spaceCommandMicros{0};
static std::atomic<int64_t> g_spaceFirstOutputMicros{0};
static std::atomic<bool> g_spaceFirstOutputArmed{false};

static void ReaperOnAudioBuffer(bool isPost, int len, double srate, struct audio_hook_register_t* reg) {
    if (len <= 0 || !reg) return;

    if (srate > 0.0) {
        reals::audio::Engine::instance().setTargetSampleRate(static_cast<int>(srate));
    }

    if (!isPost) {
        // Pre-processing: Track transport and sample-accurate phase
        const int playState = GetPlayState ? GetPlayState() : 0;
        g_liveTransport.playState.store(playState, std::memory_order_relaxed);

        const bool isPlaying = (playState & 1) != 0;
        if (isPlaying && g_spaceFirstOutputArmed.exchange(false, std::memory_order_acq_rel)) {
            const auto firstOutputMicros = std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::steady_clock::now().time_since_epoch()).count();
            g_spaceFirstOutputMicros.store(firstOutputMicros, std::memory_order_release);
        }

        // 1. Exact audio block position for DSP (GetPlayPosition2Ex)
        double playPos = 0.0;
        if (isPlaying) {
            if (GetPlayPosition2Ex) {
                ReaProject* proj = EnumProjects ? EnumProjects(-1, nullptr, 0) : nullptr;
                playPos = GetPlayPosition2Ex(proj);
            } else if (GetPlayPosition2) {
                playPos = GetPlayPosition2();
            }
        } else {
            if (GetCursorPosition) playPos = GetCursorPosition();
        }

        // 2. Exact tempo & continuous beats
        double bpm = 120.0;
        if (TimeMap_GetDividedBpmAtTime) {
            bpm = TimeMap_GetDividedBpmAtTime(playPos);
        } else if (Master_GetTempo) {
            bpm = Master_GetTempo();
        }

        int m = 0, cml = 4, cdenom = 4;
        double beats = 0.0;
        if (TimeMap2_timeToBeats) {
            TimeMap2_timeToBeats(nullptr, playPos, &m, &cml, &beats, &cdenom);
        }

        double phase = beats - floor(beats); // 0.0 - 1.0

        // 3. Discontinuity detection (seek/loop/rewind)
        if (isPlaying) {
            double expectedDelta = (srate > 0.0) ? ((double)len / srate) : 0.0;
            if (g_liveTransport.lastPos >= 0.0 && fabs((playPos - g_liveTransport.lastPos) - expectedDelta) > 0.01) {
                g_liveTransport.discontinuityCounter.fetch_add(1, std::memory_order_relaxed);
            }
        }
        g_liveTransport.lastPos = playPos;

        // 4. Update lock-free atomic transport values
        g_liveTransport.playPos.store(playPos, std::memory_order_relaxed);
        g_liveTransport.fullBeats.store(beats, std::memory_order_relaxed);
        g_liveTransport.beatPhase.store(phase, std::memory_order_relaxed);
        g_liveTransport.bpm.store(bpm, std::memory_order_relaxed);
        if (srate > 0.0) {
            g_liveTransport.blockLatencySeconds.store(static_cast<double>(len) / srate, std::memory_order_relaxed);
        }
        g_liveTransport.blockCounter.fetch_add(1, std::memory_order_relaxed);
        return;
    }

    // Post-processing (isPost == true): REAPER has finished mixing all tracks.
    // If native REAPER preview is actively playing via PlayPreviewEx, do NOT mix custom engine frames!
    if (g_hostPreview.registrationActive.load(std::memory_order_acquire)) {
        return;
    }

    // Mix preview audio on top of master hardware output buffer!
    ReaSample* outL = reg->GetBuffer(true, 0);
    ReaSample* outR = reg->GetBuffer(true, 1);
    if (outL || outR) {
        constexpr int kMaxHookFrames = 8192;
        static thread_local float tempL[kMaxHookFrames];
        static thread_local float tempR[kMaxHookFrames];

        int framesRemaining = len;
        int frameOffset = 0;
        while (framesRemaining > 0) {
            const int chunk = std::min(framesRemaining, kMaxHookFrames);
            std::memset(tempL, 0, chunk * sizeof(float));
            std::memset(tempR, 0, chunk * sizeof(float));

            // renderFrames outputs 32-bit floats
            reals::audio::Engine::instance().renderFrames(tempL, tempR, chunk);

            // Mix into REAPER's 64-bit ReaSample buffer
            if (outL && outR) {
                for (int i = 0; i < chunk; ++i) {
                    outL[frameOffset + i] += static_cast<ReaSample>(tempL[i]);
                    outR[frameOffset + i] += static_cast<ReaSample>(tempR[i]);
                }
            } else if (outL) {
                for (int i = 0; i < chunk; ++i) {
                    outL[frameOffset + i] += static_cast<ReaSample>(tempL[i]);
                }
            } else if (outR) {
                for (int i = 0; i < chunk; ++i) {
                    outR[frameOffset + i] += static_cast<ReaSample>(tempR[i]);
                }
            }

            frameOffset += chunk;
            framesRemaining -= chunk;
        }
    }
}

// ---------------------------------------------------------------------------
// Bridge host actions (touch REAPER)
// ---------------------------------------------------------------------------
class ExtHostActions final : public reals::bridge::IHostActions {
public:
    // P5 agent: generic tool executor (main thread, marshalled by Bridge).
    std::string executeAgentTool(const std::string& tool, const std::string& argsJson) override {
        return reals::ext::agent::execute(tool, argsJson);
    }

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
        if (g_hwnd && !isDocked())
            // Defer past the WebView2 callback; a synchronous SC_MOVE loop stalls while Chromium still holds mouse capture.
            PostMessageW(g_hwnd, WM_REALS_STARTDRAG, 0, 0);
    }

    void startResizeWindow(const std::string& edge) override {
        if (!g_hwnd || IsZoomed(g_hwnd) || isDocked())
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
        double bpm = g_liveTransport.bpm.load(std::memory_order_relaxed);
        if (bpm > 30.0) return bpm;

        double playPos = 0.0;
        const int playState = GetPlayState ? GetPlayState() : 0;
        if (playState & 1) {
            if (GetPlayPosition2Ex) {
                ReaProject* proj = EnumProjects ? EnumProjects(-1, nullptr, 0) : nullptr;
                playPos = GetPlayPosition2Ex(proj);
            } else if (GetPlayPosition2) {
                playPos = GetPlayPosition2();
            } else if (GetPlayPosition) {
                playPos = GetPlayPosition();
            }
        } else {
            if (GetCursorPosition) playPos = GetCursorPosition();
        }

        if (TimeMap_GetDividedBpmAtTime) {
            bpm = TimeMap_GetDividedBpmAtTime(playPos);
            if (bpm > 30.0) return bpm;
        }

        if (Master_GetTempo) {
            bpm = Master_GetTempo();
            if (bpm > 30.0) return bpm;
        }
        return 120.0;
    }

    void togglePlay() override {
        if (Main_OnCommand)
            Main_OnCommand(40044, 0); // Transport: Play/stop
    }

    void startTransport() override {
        g_previewCycleGuard.store(true, std::memory_order_release);
        if (Main_OnCommand)
            Main_OnCommand(1007, 0); // Transport: Play
    }

    void startTransport(double startPositionSeconds) override {
        g_previewCycleGuard.store(true, std::memory_order_release);
        // Align the stopped edit cursor to the already-audible preview phase
        // before Play so Space does not later hard-seek/Reset the DSP.
        if (SetEditCurPos2) {
            ReaProject* proj = EnumProjects ? EnumProjects(-1, nullptr, 0) : nullptr;
            SetEditCurPos2(proj, std::max(0.0, startPositionSeconds), true, false);
        }
        if (Main_OnCommand)
            Main_OnCommand(1007, 0); // Transport: Play
        // #region agent log
        AgentDebugLog("C", "reaper_plugin.cpp:startTransport", "daw_cursor_aligned",
                      {{"startPositionSeconds", startPositionSeconds}});
        // #endregion
    }

    void stopTransport() override {
        // Spacebar preview cycle step 2: stop the DAW transport. The preview
        // is stopped separately by the bridge (stopHostPreview first).
        g_previewCycleGuard.store(true, std::memory_order_release);
        if (Main_OnCommand)
            Main_OnCommand(1016, 0); // Transport: Stop
    }

    reals::bridge::HostTransport hostTransport() const override {
        const int64_t firstOutputMicros = g_spaceFirstOutputMicros.exchange(0, std::memory_order_acq_rel);
        if (firstOutputMicros > 0) {
            const int64_t commandMicros = g_spaceCommandMicros.exchange(0, std::memory_order_acq_rel);
            LOG_INFO("SYNC_DIAG", "SPACE_START audio-first-output mono_us=" +
                     std::to_string(firstOutputMicros) + " commandToAudio_us=" +
                     std::to_string(commandMicros > 0 ? firstOutputMicros - commandMicros : 0));
        }
        reals::bridge::HostTransport t;
        t.playState = g_liveTransport.playState.load(std::memory_order_relaxed);
        if (t.playState == 0 && GetPlayState) {
            t.playState = GetPlayState();
        }

        if (t.playState & 1) {
            t.playPosition = g_liveTransport.playPos.load(std::memory_order_relaxed);
            t.fullBeats = g_liveTransport.fullBeats.load(std::memory_order_relaxed);
            t.bpm = g_liveTransport.bpm.load(std::memory_order_relaxed);
            t.blockLatencySeconds = g_liveTransport.blockLatencySeconds.load(std::memory_order_relaxed);

            if (t.playPosition <= 0.0) {
                if (GetPlayPosition2Ex) {
                    ReaProject* proj = EnumProjects ? EnumProjects(-1, nullptr, 0) : nullptr;
                    t.playPosition = GetPlayPosition2Ex(proj);
                } else if (GetPlayPosition2) {
                    t.playPosition = GetPlayPosition2();
                } else if (GetPlayPosition) {
                    t.playPosition = GetPlayPosition();
                }
            }
            if (t.bpm <= 0.0) {
                if (TimeMap_GetDividedBpmAtTime) t.bpm = TimeMap_GetDividedBpmAtTime(t.playPosition);
                else if (Master_GetTempo) t.bpm = Master_GetTempo();
            }
            if (TimeMap2_timeToBeats) {
                int m = 0, cml = 4, cdenom = 4;
                double fb = 0.0;
                TimeMap2_timeToBeats(nullptr, t.playPosition, &m, &cml, &fb, &cdenom);
                t.measure = m;
                t.beatsPerMeasure = cml > 0 ? cml : 4;
                t.denom = cdenom > 0 ? cdenom : 4;
                if (t.fullBeats <= 0.0) t.fullBeats = fb;
            }
        } else {
            double cursorPos = 0.0;
            // Keep the last measured hardware block latency available while
            // stopped. Spacebar pre-alignment runs in this state and must
            // compensate the first DAW output block as well.
            t.blockLatencySeconds = g_liveTransport.blockLatencySeconds.load(std::memory_order_relaxed);
            if (GetCursorPosition) cursorPos = GetCursorPosition();
            t.playPosition = cursorPos;
            if (Master_GetTempo) t.bpm = Master_GetTempo();
            if (TimeMap2_timeToBeats) {
                int m = 0, cml = 4, cdenom = 4;
                double fb = 0.0;
                TimeMap2_timeToBeats(nullptr, t.playPosition, &m, &cml, &fb, &cdenom);
                t.measure = m;
                t.beatsPerMeasure = cml > 0 ? cml : 4;
                t.denom = cdenom > 0 ? cdenom : 4;
                t.fullBeats = fb;
            }
        }
        return t;
    }

    bool playHostPreview(const std::string& path, bool loop, double startPosSeconds, double volume, double playrate, double pitchSemitones, double sampleBpm = 120.0, double loopBeats = 16.0, uint64_t nominalLoopFrames = 0) override {
        (void)nominalLoopFrames; // loop extent is derived from loopBeats + sampleBpm + live ratio
        if (!PCM_Source_CreateFromFileEx || !PlayPreviewEx || !StopPreview) {
            LOG_ERROR(kTag, "playHostPreview: REAPER preview APIs not available");
            return false;
        }

        // Keep launch atomic with respect to transport hooks, timer controls,
        // and a second audio.play request. In particular, a stop cannot detach
        // reg.src while PlayPreviewEx is still registering it.
        const std::lock_guard<std::recursive_mutex> lifecycleLock(g_hostPreview.lifecycleMutex);
        g_hostPreview.initCS();
        g_hostPreview.stopAndClear();

        // Open PCM source and create DspPreviewSource OUTSIDE the critical section
        // to prevent holding reg.cs during disk I/O or codec initialization.
        PCM_source* rawSrc = PCM_Source_CreateFromFileEx(path.c_str(), true);
        if (!rawSrc) {
            LOG_ERROR(kTag, "playHostPreview: PCM_Source_CreateFromFileEx failed for " + path);
            return false;
        }

        // Always wrap with DspPreviewSource so real-time setTimeRatio/setPitchSemitones
        // can be applied later when user toggles Sync BPM. The wrapper handles bit-perfect
        // bypass internally when ratio ≈ 1.0 and pitch ≈ 0.0 — zero processing overhead.
        auto* dsp = new DspPreviewSource(rawSrc, playrate, pitchSemitones);
        // Keep the streaming path alive whenever the host loop flag is set,
        // even if sample BPM metadata is unavailable. The nominal bar boundary
        // remains optional; REAPER still owns the full-file wrap in that case.
        dsp->setLoopActive(loop);
        // Bar-grid looping: wrap at the nominal loop (loopBeats at sampleBpm) so a
        // reverb tail / encoder padding does not drift the loop off the DAW grid.
        // Store the metrics even when starting non-looping so a live loop toggle
        // can activate the boundary without re-probing the file.
        if (loopBeats > 0.0 && sampleBpm > 30.0) {
            dsp->setLoopBoundary(loop, loopBeats, sampleBpm);
        }
        if (std::abs(playrate - 1.0) > 0.003 || std::abs(pitchSemitones) > 0.02) {
            LOG_INFO(kTag, "playHostPreview: DspPreviewSource DSP active (élastique 3 Pro: playrate=" +
                           std::to_string(playrate) + ", pitch=" + std::to_string(pitchSemitones) + ")");
        } else {
            LOG_INFO(kTag, "playHostPreview: DspPreviewSource bypass mode (ratio≈1.0, pitch≈0.0)");
        }

#ifdef _WIN32
        EnterCriticalSection(&g_hostPreview.reg.cs);
#else
        pthread_mutex_lock(&g_hostPreview.reg.mutex);
#endif
        // Arm cleanup before publishing reg.src.  PlayPreviewEx may enter the
        // host callback synchronously, and it may also fail after touching the
        // preview registration.  stopAndClear() must call StopPreview() in
        // either case before detaching/deleting this source.
        g_hostPreview.registrationActive.store(true, std::memory_order_release);
        g_hostPreview.reg.src = dsp;
        g_hostPreview.durationSeconds = dsp->GetLength();
        g_hostPreview.reg.curpos = std::max(0.0, startPosSeconds);
        g_hostPreview.reg.loop = loop;
        g_hostPreview.reg.volume = std::clamp(volume, 0.0, 2.0);
        g_hostPreview.reg.m_out_chan = 0; // Standard REAPER preview output channel (Monitoring FX compliant)
        g_hostPreview.reg.preview_track = nullptr;
        g_hostPreview.dspWrapper = dsp;
        g_hostPreview.currentPath = path;
#ifdef _WIN32
        LeaveCriticalSection(&g_hostPreview.reg.cs);
#else
        pthread_mutex_unlock(&g_hostPreview.reg.mutex);
#endif

        // REAPER can enter the preview callback while PlayPreviewEx runs. Do not
        // hold reg.cs here: the callback also uses it and StopPreview may wait for
        // that callback during cleanup.
        // Do not ask REAPER to buffer this source.  DspPreviewSource already
        // owns the real-time élastique pipeline and its lookahead; the SDK
        // buffer-source mode (bit 0) asks REAPER to queue another copy of that
        // timeline.  At a nominal loop boundary that extra queue made the
        // callback run about 245 ms past GetLength(), so the next cycle began
        // with audio from the previous cycle.  Direct callbacks keep the host
        // cursor and the DSP timeline on the same boundary.
        constexpr int kPreviewBufferFlags = 0;
        // PlayPreviewEx may synchronously enter the source callback. Publish
        // audible intent first so host queries during that launch never report
        // previewPlaying=0 while a valid registration is already rendering.
        g_hostPreview.isPlaying.store(true, std::memory_order_release);
        const int res = PlayPreviewEx(&g_hostPreview.reg, kPreviewBufferFlags, -1.0);
        if (res == 0) {
            LOG_WARN(kTag, "playHostPreview: PlayPreviewEx rejected path=" + path);
            g_hostPreview.stopAndClear();
            return false;
        }
        LOG_INFO(kTag, "playHostPreview: started path=" + path + " dur=" +
                       std::to_string(g_hostPreview.durationSeconds) + " res=" + std::to_string(res));
        return true;
    }

    void stopHostPreview() override {
        g_hostPreview.stopAndClear();
        LOG_INFO(kTag, "stopHostPreview: stopped");
    }

    bool isHostPreviewPlaying() const override {
        const std::lock_guard<std::recursive_mutex> lifecycleLock(g_hostPreview.lifecycleMutex);
        if (!g_hostPreview.isPlaying.load(std::memory_order_relaxed)) {
            // A non-looping preview can reach its end through REAPER's cursor
            // without setting m_streamFinished on the wrapper.  Do not leave a
            // stale native registration suppressing the post hook in that
            // case; explicit stop/play paths are already idempotent here.
            if (g_hostPreview.registrationActive.load(std::memory_order_acquire)) {
                g_hostPreview.stopAndClear();
            }
            return false;
        }
        bool loopEnabled = false;
#ifdef _WIN32
        EnterCriticalSection(&g_hostPreview.reg.cs);
        loopEnabled = g_hostPreview.reg.loop;
        LeaveCriticalSection(&g_hostPreview.reg.cs);
#else
        pthread_mutex_lock(&g_hostPreview.reg.mutex);
        loopEnabled = g_hostPreview.reg.loop;
        pthread_mutex_unlock(&g_hostPreview.reg.mutex);
#endif
        if (g_hostPreview.dspWrapper && g_hostPreview.dspWrapper->isStreamFinished() && !loopEnabled) {
            // EOF is terminal for a non-looping native preview.  Release the
            // host registration immediately; otherwise the post hook keeps
            // suppressing the fallback mixer and the source remains attached
            // until a later play/stop/unload call.
            g_hostPreview.stopAndClear();
            return false;
        }
        return true;
    }

    double hostPreviewPositionFraction() const override {
        const std::lock_guard<std::recursive_mutex> lifecycleLock(g_hostPreview.lifecycleMutex);
        if (!g_hostPreview.isPlaying.load(std::memory_order_relaxed) || g_hostPreview.durationSeconds <= 0.0) {
            return 0.0;
        }
#ifdef _WIN32
        EnterCriticalSection(&g_hostPreview.reg.cs);
        const double pos = g_hostPreview.reg.curpos;
        const bool loopEnabled = g_hostPreview.reg.loop;
        LeaveCriticalSection(&g_hostPreview.reg.cs);
#else
        pthread_mutex_lock(&g_hostPreview.reg.mutex);
        const double pos = g_hostPreview.reg.curpos;
        const bool loopEnabled = g_hostPreview.reg.loop;
        pthread_mutex_unlock(&g_hostPreview.reg.mutex);
#endif
        if (g_hostPreview.dspWrapper && g_hostPreview.dspWrapper->isStreamFinished() && !loopEnabled) {
            g_hostPreview.stopAndClear();
            return 1.0;
        }
        const bool ended = pos >= g_hostPreview.durationSeconds && !loopEnabled;
        if (ended) {
            g_hostPreview.stopAndClear();
            return 1.0;
        }
        return std::clamp(pos / g_hostPreview.durationSeconds, 0.0, 1.0);
    }

    float hostPreviewPeak() const override {
        const std::lock_guard<std::recursive_mutex> lifecycleLock(g_hostPreview.lifecycleMutex);
        if (!g_hostPreview.isPlaying.load(std::memory_order_relaxed)) return 0.0f;
#ifdef _WIN32
        EnterCriticalSection(&g_hostPreview.reg.cs);
        const double p0 = std::abs(g_hostPreview.reg.peakvol[0]);
        const double p1 = std::abs(g_hostPreview.reg.peakvol[1]);
        LeaveCriticalSection(&g_hostPreview.reg.cs);
#else
        pthread_mutex_lock(&g_hostPreview.reg.mutex);
        const double p0 = std::abs(g_hostPreview.reg.peakvol[0]);
        const double p1 = std::abs(g_hostPreview.reg.peakvol[1]);
        pthread_mutex_unlock(&g_hostPreview.reg.mutex);
#endif
        return static_cast<float>(std::max(p0, p1));
    }

    void setHostPreviewVolume(double vol) override {
        const std::lock_guard<std::recursive_mutex> lifecycleLock(g_hostPreview.lifecycleMutex);
        if (!g_hostPreview.isPlaying.load(std::memory_order_relaxed)) return;
#ifdef _WIN32
        EnterCriticalSection(&g_hostPreview.reg.cs);
        g_hostPreview.reg.volume = std::clamp(vol, 0.0, 2.0);
        LeaveCriticalSection(&g_hostPreview.reg.cs);
#else
        pthread_mutex_lock(&g_hostPreview.reg.mutex);
        g_hostPreview.reg.volume = std::clamp(vol, 0.0, 2.0);
        pthread_mutex_unlock(&g_hostPreview.reg.mutex);
#endif
    }

    void setHostPreviewPosition(double posSeconds) override {
        const std::lock_guard<std::recursive_mutex> lifecycleLock(g_hostPreview.lifecycleMutex);
        if (!g_hostPreview.isPlaying.load(std::memory_order_relaxed)) return;
#ifdef _WIN32
        EnterCriticalSection(&g_hostPreview.reg.cs);
        g_hostPreview.reg.curpos = std::max(0.0, posSeconds);
        LeaveCriticalSection(&g_hostPreview.reg.cs);
#else
        pthread_mutex_lock(&g_hostPreview.reg.mutex);
        g_hostPreview.reg.curpos = std::max(0.0, posSeconds);
        pthread_mutex_unlock(&g_hostPreview.reg.mutex);
#endif
    }

    void setHostPreviewPositionSoft(double posSeconds) override {
        const std::lock_guard<std::recursive_mutex> lifecycleLock(g_hostPreview.lifecycleMutex);
        // Phase-timer re-alignment: arm the DSP wrapper so the resulting
        // position jump does NOT reset élastique (no dead-air gap).
        if (g_hostPreview.dspWrapper) {
            g_hostPreview.dspWrapper->armSoftSeek();
        }
        setHostPreviewPosition(posSeconds);
    }

    void setHostPreviewPositionFraction(double frac) override {
        const std::lock_guard<std::recursive_mutex> lifecycleLock(g_hostPreview.lifecycleMutex);
        if (!g_hostPreview.isPlaying.load(std::memory_order_relaxed) || g_hostPreview.durationSeconds <= 0.0) return;
        const double pos = std::clamp(frac, 0.0, 1.0) * g_hostPreview.durationSeconds;
        setHostPreviewPosition(pos);
    }

    void setHostPreviewPositionFractionSoft(double frac) override {
        const std::lock_guard<std::recursive_mutex> lifecycleLock(g_hostPreview.lifecycleMutex);
        if (!g_hostPreview.isPlaying.load(std::memory_order_relaxed) || g_hostPreview.durationSeconds <= 0.0) return;
        const double pos = std::clamp(frac, 0.0, 1.0) * g_hostPreview.durationSeconds;
        setHostPreviewPositionSoft(pos);
    }

    void setHostPreviewLoop(bool loop) override {
        const std::lock_guard<std::recursive_mutex> lifecycleLock(g_hostPreview.lifecycleMutex);
        if (!g_hostPreview.isPlaying.load(std::memory_order_relaxed)) return;
#ifdef _WIN32
        EnterCriticalSection(&g_hostPreview.reg.cs);
        if (g_hostPreview.dspWrapper) {
            g_hostPreview.dspWrapper->setLoopActive(loop);
            g_hostPreview.durationSeconds = g_hostPreview.dspWrapper->GetLength();
        }
        g_hostPreview.reg.loop = loop;
        LeaveCriticalSection(&g_hostPreview.reg.cs);
#else
        pthread_mutex_lock(&g_hostPreview.reg.mutex);
        if (g_hostPreview.dspWrapper) {
            g_hostPreview.dspWrapper->setLoopActive(loop);
            g_hostPreview.durationSeconds = g_hostPreview.dspWrapper->GetLength();
        }
        g_hostPreview.reg.loop = loop;
        pthread_mutex_unlock(&g_hostPreview.reg.mutex);
#endif
    }

    void setHostPreviewTimeRatio(double ratio) override {
        const std::lock_guard<std::recursive_mutex> lifecycleLock(g_hostPreview.lifecycleMutex);
        if (!g_hostPreview.csInitialized) return;
#ifdef _WIN32
        EnterCriticalSection(&g_hostPreview.reg.cs);
#else
        pthread_mutex_lock(&g_hostPreview.reg.mutex);
#endif
        if (g_hostPreview.dspWrapper) {
            g_hostPreview.dspWrapper->setTimeRatio(ratio);
            g_hostPreview.durationSeconds = g_hostPreview.dspWrapper->GetLength();
        }
#ifdef _WIN32
        LeaveCriticalSection(&g_hostPreview.reg.cs);
#else
        pthread_mutex_unlock(&g_hostPreview.reg.mutex);
#endif
    }

    void setHostPreviewLoopBoundary(double loopBeats, double sampleBpm) override {
        const std::lock_guard<std::recursive_mutex> lifecycleLock(g_hostPreview.lifecycleMutex);
        if (!g_hostPreview.csInitialized || loopBeats <= 0.0 || sampleBpm <= 30.0) return;
#ifdef _WIN32
        EnterCriticalSection(&g_hostPreview.reg.cs);
#else
        pthread_mutex_lock(&g_hostPreview.reg.mutex);
#endif
        if (g_hostPreview.dspWrapper) {
            g_hostPreview.dspWrapper->setLoopBoundary(g_hostPreview.reg.loop, loopBeats, sampleBpm);
            g_hostPreview.durationSeconds = g_hostPreview.dspWrapper->GetLength();
        }
#ifdef _WIN32
        LeaveCriticalSection(&g_hostPreview.reg.cs);
#else
        pthread_mutex_unlock(&g_hostPreview.reg.mutex);
#endif
    }

    void setHostPreviewPitchSemitones(double semitones) override {
        const std::lock_guard<std::recursive_mutex> lifecycleLock(g_hostPreview.lifecycleMutex);
        if (g_hostPreview.dspWrapper) {
            g_hostPreview.dspWrapper->setPitchSemitones(semitones);
        }
    }

    double hostPreviewTimeRatio() const override {
        const std::lock_guard<std::recursive_mutex> lifecycleLock(g_hostPreview.lifecycleMutex);
        if (g_hostPreview.dspWrapper) {
            return g_hostPreview.dspWrapper->getTimeRatio();
        }
        return 1.0;
    }

    double hostPreviewPitchSemitones() const override {
        const std::lock_guard<std::recursive_mutex> lifecycleLock(g_hostPreview.lifecycleMutex);
        if (g_hostPreview.dspWrapper) {
            return g_hostPreview.dspWrapper->getPitchSemitones();
        }
        return 0.0;
    }

    void toggleDock() override;
    bool isDocked() const override;

    std::string getSelectedMediaItemPath() override {
        if (!GetSelectedMediaItem || !GetActiveTake || !GetMediaItemTake_Source)
            return "";
        MediaItem* item = GetSelectedMediaItem(nullptr, 0);
        if (!item)
            return "";
        MediaItem_Take* take = GetActiveTake(item);
        if (!take)
            return "";
        PCM_source* src = GetMediaItemTake_Source(take);
        if (!src || !src->GetFileName())
            return "";
        return src->GetFileName();
    }

    double getSelectedMediaItemPosition() override {
        if (!GetSelectedMediaItem || !GetMediaItemInfo_Value)
            return 0.0;
        MediaItem* item = GetSelectedMediaItem(nullptr, 0);
        return item ? GetMediaItemInfo_Value(item, "D_POSITION") : 0.0;
    }

    double getSelectedMediaItemLength() override {
        if (!GetSelectedMediaItem || !GetMediaItemInfo_Value)
            return 0.0;
        MediaItem* item = GetSelectedMediaItem(nullptr, 0);
        return item ? GetMediaItemInfo_Value(item, "D_LENGTH") : 0.0;
    }

    static void updateChordDockerStateFromProgression(const reals::lab::ChordProgression& prog);

    bool insertChordTrack(const std::string& progressionJson) override {
        LOG_INFO(kTag, "insertChordTrack: begin");
        if (!CreateNewMIDIItemInProj || !MIDI_GetPPQPosFromProjTime ||
            !MIDI_InsertNote || !MIDI_InsertTextSysexEvt || !MIDI_Sort ||
            !CountTracks || !GetTrack || !GetSetMediaTrackInfo_String || !InsertTrackAtIndex) {
            LOG_ERROR(kTag, "insertChordTrack: required REAPER MIDI APIs missing");
            return false;
        }

        nlohmann::json j;
        try {
            j = nlohmann::json::parse(progressionJson);
        } catch (...) {
            LOG_ERROR(kTag, "insertChordTrack: failed to parse progressionJson");
            return false;
        }

        if (j.contains("progression")) {
            j = j["progression"];
        } else if (j.contains("data")) {
            j = j["data"];
        }

        reals::lab::ChordProgression prog = reals::lab::ChordEngine::parseApiResponse(j);
        if (prog.events.empty()) {
            LOG_ERROR(kTag, "insertChordTrack: progression has no events");
            return false;
        }

        double offsetSec = 0.0;
        if (j.contains("itemPosition") && j["itemPosition"].is_number()) {
            offsetSec = j["itemPosition"].get<double>();
        } else if (j.contains("itemPos") && j["itemPos"].is_number()) {
            offsetSec = j["itemPos"].get<double>();
        }
        if (offsetSec > 0.0) {
            for (auto& evt : prog.events) {
                evt.startTime += offsetSec;
                evt.endTime += offsetSec;
            }
        }

        double projBpm = projectTempo();
        if (projBpm > 20.0) {
            prog.bpm = projBpm;
        }
        reals::lab::ChordEngine::snapToBeatGrid(prog, 0.20);

        Undo_BeginBlock();

        MediaTrack* chordTrack = nullptr;
        const int numTracks = CountTracks(nullptr);
        for (int i = 0; i < numTracks; ++i) {
            MediaTrack* tr = GetTrack(nullptr, i);
            if (!tr) continue;
            char nameBuf[256] = {0};
            GetSetMediaTrackInfo_String(tr, "P_NAME", nameBuf, false);
            std::string sName = nameBuf;
            std::transform(sName.begin(), sName.end(), sName.begin(),
                           [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
            if (sName == "CHORD TRACK" || sName == "CHORDS") {
                chordTrack = tr;
                break;
            }
        }

        if (!chordTrack) {
            InsertTrackAtIndex(0, true);
            chordTrack = GetTrack(nullptr, 0);
            if (chordTrack) {
                GetSetMediaTrackInfo_String(chordTrack, "P_NAME", const_cast<char*>("CHORD TRACK"), true);
                if (SetTrackColor) {
                    SetTrackColor(chordTrack, 0x1000000 | 0x7C3AEA); // Studio purple accent
                }
            }
        }

        if (!chordTrack) {
            Undo_EndBlock("Reals Lab: Insert Chord Track (failed)", 0);
            return false;
        }

        double startTime = prog.events.front().startTime;
        double endTime = prog.events.back().endTime;
        if (endTime <= startTime) {
            endTime = startTime + 4.0;
        }

        MediaItem* midiItem = CreateNewMIDIItemInProj(chordTrack, startTime, endTime, nullptr);
        if (!midiItem) {
            Undo_EndBlock("Reals Lab: Insert Chord Track (failed)", 0);
            return false;
        }

        MediaItem_Take* take = GetActiveTake ? GetActiveTake(midiItem) : nullptr;
        if (!take) {
            Undo_EndBlock("Reals Lab: Insert Chord Track (failed)", 0);
            return false;
        }

        const bool noSort = true;
        for (const auto& evt : prog.events) {
            double startPPQ = MIDI_GetPPQPosFromProjTime(take, evt.startTime);
            double endPPQ = MIDI_GetPPQPosFromProjTime(take, evt.endTime);
            if (endPPQ <= startPPQ) {
                endPPQ = startPPQ + 960.0;
            }

            for (int pitch : evt.midiNotes) {
                MIDI_InsertNote(take, false, false, startPPQ, endPPQ, 0, pitch, 96, &noSort);
            }

            std::string label = evt.name;
            if (!evt.roman.empty()) {
                label += " (" + evt.roman + ")";
            }
            MIDI_InsertTextSysexEvt(take, false, false, startPPQ, 1, label.c_str(), static_cast<int>(label.size()));
        }

        MIDI_Sort(take);
        if (UpdateItemInProject) UpdateItemInProject(midiItem);
        if (UpdateArrange) UpdateArrange();

        Undo_EndBlock("Reals Lab: Insert Chord Track", 0);
        LOG_INFO(kTag, "insertChordTrack: successfully created chord track with " + std::to_string(prog.events.size()) + " chords");
        updateChordDockerStateFromProgression(prog);
        return true;
    }

    bool insertStemsToFolder(const std::string& stemsJson) override {
        LOG_INFO(kTag, "insertStemsToFolder: begin");
        if (!CountTracks || !GetTrack || !InsertTrackAtIndex ||
            !AddMediaItemToTrack || !AddTakeToMediaItem || !SetMediaItemTake_Source ||
            !PCM_Source_CreateFromFileEx) {
            LOG_ERROR(kTag, "insertStemsToFolder: required REAPER APIs missing");
            return false;
        }

        nlohmann::json j;
        try {
            j = nlohmann::json::parse(stemsJson);
        } catch (...) {
            LOG_ERROR(kTag, "insertStemsToFolder: failed to parse stemsJson");
            return false;
        }

        const nlohmann::json* stemsArr = nullptr;
        if (j.contains("stems") && j["stems"].is_array()) {
            stemsArr = &j["stems"];
        } else if (j.contains("files") && j["files"].is_array()) {
            stemsArr = &j["files"];
        }

        if (!stemsArr || stemsArr->empty()) {
            LOG_ERROR(kTag, "insertStemsToFolder: no stems provided in payload");
            return false;
        }

        const std::string origPath = j.value("originalPath", "");
        const double fallbackPos = j.value("itemPosition", 0.0);
        const double fallbackLen = j.value("itemLength", 0.0);

        if (Undo_BeginBlock) Undo_BeginBlock();

        // Helper to match paths robustly (case-insensitive, slash-normalized)
        auto pathMatches = [](const std::string& p1, const std::string& p2) {
            if (p1.empty() || p2.empty()) return false;
            std::string n1 = reals::platform::normalizePath(p1);
            std::string n2 = reals::platform::normalizePath(p2);
            for (char& c : n1) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            for (char& c : n2) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));
            return n1 == n2;
        };

        // 1. Locate original MediaItem and track in REAPER
        MediaItem* origItem = nullptr;
        if (GetSelectedMediaItem) {
            MediaItem* sel = GetSelectedMediaItem(nullptr, 0);
            if (sel) {
                MediaItem_Take* selTk = GetActiveTake ? GetActiveTake(sel) : nullptr;
                PCM_source* selSrc = (selTk && GetMediaItemTake_Source) ? GetMediaItemTake_Source(selTk) : nullptr;
                if (selSrc && selSrc->GetFileName() && pathMatches(origPath, selSrc->GetFileName())) {
                    origItem = sel;
                }
            }
        }

        if (!origItem && !origPath.empty() && CountTracks && GetTrack && CountTrackMediaItems && GetTrackMediaItem) {
            const int nTr = CountTracks(nullptr);
            for (int t = 0; t < nTr && !origItem; ++t) {
                MediaTrack* tr = GetTrack(nullptr, t);
                if (!tr) continue;
                const int nItems = CountTrackMediaItems(tr);
                for (int i = 0; i < nItems; ++i) {
                    MediaItem* it = GetTrackMediaItem(tr, i);
                    if (!it) continue;
                    MediaItem_Take* tk = GetActiveTake ? GetActiveTake(it) : nullptr;
                    PCM_source* src = (tk && GetMediaItemTake_Source) ? GetMediaItemTake_Source(tk) : nullptr;
                    if (src && src->GetFileName() && pathMatches(origPath, src->GetFileName())) {
                        origItem = it;
                        break;
                    }
                }
            }
        }

        if (!origItem && GetSelectedMediaItem) {
            origItem = GetSelectedMediaItem(nullptr, 0);
        }

        MediaTrack* origTrack = (origItem && GetMediaItem_Track) ? GetMediaItem_Track(origItem) : nullptr;
        double itemPos = fallbackPos;
        double itemLen = fallbackLen;
        double startOffs = 0.0;
        double playrate = 1.0;
        char origTakeName[512] = {0};

        if (origItem) {
            if (GetMediaItemInfo_Value) {
                itemPos = GetMediaItemInfo_Value(origItem, "D_POSITION");
                const double l = GetMediaItemInfo_Value(origItem, "D_LENGTH");
                if (l > 0.0) itemLen = l;
            }
            MediaItem_Take* origTake = GetActiveTake ? GetActiveTake(origItem) : nullptr;
            if (origTake) {
                if (GetMediaItemTakeInfo_Value) {
                    startOffs = GetMediaItemTakeInfo_Value(origTake, "D_STARTOFFS");
                    const double r = GetMediaItemTakeInfo_Value(origTake, "D_PLAYRATE");
                    if (r > 0.0) playrate = r;
                }
                if (GetSetMediaItemTakeInfo_String) {
                    GetSetMediaItemTakeInfo_String(origTake, "P_NAME", origTakeName, false);
                }
            }
            // Mute the original item so it does not conflict with the separated stems
            if (SetMediaItemInfo_Value) {
                SetMediaItemInfo_Value(origItem, "B_MUTE", 1.0);
            }
            if (UpdateItemInProject) {
                UpdateItemInProject(origItem);
            }
        }

        // 2. Determine insertion index (immediately below origTrack)
        int origTrackIdx = -1;
        if (origTrack && GetMediaTrackInfo_Value) {
            origTrackIdx = static_cast<int>(GetMediaTrackInfo_Value(origTrack, "IP_TRACKNUMBER")) - 1;
        }
        if (origTrackIdx < 0 && origTrack && CountTracks && GetTrack) {
            const int totalTr = CountTracks(nullptr);
            for (int i = 0; i < totalTr; ++i) {
                if (GetTrack(nullptr, i) == origTrack) {
                    origTrackIdx = i;
                    break;
                }
            }
        }
        if (origTrackIdx < 0) {
            origTrackIdx = (CountTracks ? CountTracks(nullptr) : 0) - 1;
        }

        const int insertIdx = (origTrackIdx >= 0) ? (origTrackIdx + 1) : 0;

        // If origTrack had negative depth (closed folder), transfer that closing depth to the last stem track
        double origDepth = 0.0;
        if (origTrack && GetMediaTrackInfo_Value) {
            origDepth = GetMediaTrackInfo_Value(origTrack, "I_FOLDERDEPTH");
            if (origDepth < 0.0 && SetMediaTrackInfo_Value) {
                SetMediaTrackInfo_Value(origTrack, "I_FOLDERDEPTH", 0.0);
            }
        }

        // 3. Create folder parent track
        InsertTrackAtIndex(insertIdx, true);
        MediaTrack* folderTrack = GetTrack ? GetTrack(nullptr, insertIdx) : nullptr;
        if (folderTrack) {
            if (SetMediaTrackInfo_Value) {
                SetMediaTrackInfo_Value(folderTrack, "I_FOLDERDEPTH", 1.0); // Start folder
            }
            std::string folderName;
            if (origTakeName[0] != '\0') {
                folderName = std::string(origTakeName) + " (Stems)";
            } else if (!origPath.empty()) {
                const auto slash = origPath.find_last_of("/\\");
                std::string base = (slash != std::string::npos) ? origPath.substr(slash + 1) : origPath;
                const auto dot = base.find_last_of('.');
                if (dot != std::string::npos) base = base.substr(0, dot);
                folderName = base + " (Stems)";
            } else {
                folderName = "Stems Group";
            }

            if (GetSetMediaTrackInfo_String) {
                GetSetMediaTrackInfo_String(folderTrack, "P_NAME", const_cast<char*>(folderName.c_str()), true);
            }
            if (SetTrackColor) {
                SetTrackColor(folderTrack, 0x1000000 | RGB(139, 92, 246)); // Theme Purple
            }
        }

        // 4. Create child stem tracks
        const size_t stemCount = stemsArr->size();
        for (size_t i = 0; i < stemCount; ++i) {
            const auto& stem = (*stemsArr)[i];
            const int stemTrackIdx = insertIdx + 1 + static_cast<int>(i);
            InsertTrackAtIndex(stemTrackIdx, true);
            MediaTrack* stemTrack = GetTrack ? GetTrack(nullptr, stemTrackIdx) : nullptr;
            if (!stemTrack) continue;

            const double depth = (i == stemCount - 1)
                                     ? (-1.0 + (origDepth < 0.0 ? origDepth : 0.0))
                                     : 0.0;
            if (SetMediaTrackInfo_Value) {
                SetMediaTrackInfo_Value(stemTrack, "I_FOLDERDEPTH", depth);
            }

            std::string stemName = stem.value("name", "Stem");
            if (!stemName.empty() && std::islower(static_cast<unsigned char>(stemName[0]))) {
                stemName[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(stemName[0])));
            }
            if (GetSetMediaTrackInfo_String) {
                GetSetMediaTrackInfo_String(stemTrack, "P_NAME", const_cast<char*>(stemName.c_str()), true);
            }

            // Assign color
            int trackColor = RGB(53, 208, 127); // Default Green
            std::string lowerStem = stemName;
            std::transform(lowerStem.begin(), lowerStem.end(), lowerStem.begin(),
                           [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            if (lowerStem.find("vocal") != std::string::npos) {
                trackColor = RGB(255, 107, 44);   // Vocals Orange
            } else if (lowerStem.find("drum") != std::string::npos) {
                trackColor = RGB(85, 165, 255);   // Drums Blue
            } else if (lowerStem.find("bass") != std::string::npos) {
                trackColor = RGB(234, 179, 8);    // Bass Yellow
            } else {
                trackColor = RGB(53, 208, 127);   // Other Green
            }
            if (SetTrackColor) {
                SetTrackColor(stemTrack, 0x1000000 | trackColor);
            }

            const std::string stemPath = stem.value("path", "");
            if (!stemPath.empty() && AddMediaItemToTrack && AddTakeToMediaItem && SetMediaItemTake_Source) {
                MediaItem* stemItem = AddMediaItemToTrack(stemTrack);
                if (stemItem) {
                    if (SetMediaItemInfo_Value) {
                        SetMediaItemInfo_Value(stemItem, "D_POSITION", itemPos);
                        SetMediaItemInfo_Value(stemItem, "D_LENGTH", itemLen > 0.0 ? itemLen : 10.0);
                    }
                    MediaItem_Take* stemTake = AddTakeToMediaItem(stemItem);
                    if (stemTake) {
                        PCM_source* stemSrc = PCM_Source_CreateFromFileEx(stemPath.c_str(), false);
                        if (stemSrc) {
                            SetMediaItemTake_Source(stemTake, stemSrc);
                            if (itemLen <= 0.0) {
                                const double srcLen = stemSrc->GetLength();
                                if (srcLen > 0.0 && SetMediaItemInfo_Value) {
                                    SetMediaItemInfo_Value(stemItem, "D_LENGTH", srcLen);
                                }
                            }
                        }
                        if (SetMediaItemTakeInfo_Value) {
                            SetMediaItemTakeInfo_Value(stemTake, "D_STARTOFFS", startOffs);
                            SetMediaItemTakeInfo_Value(stemTake, "D_PLAYRATE", playrate);
                        }
                        if (GetSetMediaItemTakeInfo_String) {
                            GetSetMediaItemTakeInfo_String(stemTake, "P_NAME", const_cast<char*>(stemName.c_str()), true);
                        }
                    }
                    if (UpdateItemInProject) {
                        UpdateItemInProject(stemItem);
                    }
                }
            }
        }

        if (TrackList_AdjustWindows) {
            TrackList_AdjustWindows(false);
        }
        if (UpdateArrange) {
            UpdateArrange();
        }
        if (Undo_EndBlock) {
            Undo_EndBlock("Reals Lab: Separate Stems to Folder", -1);
        }

        LOG_INFO(kTag, "insertStemsToFolder: successfully created stem folder with " + std::to_string(stemCount) + " tracks");
        return true;
    }

    void showChordDocker(bool show, const std::string& chordDataJson = "") override;
    void insertChordTrackFromDocker();

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
// ui-web folder resolution: Dev tree -> Portable folder -> Embedded Auto-Extraction
// ---------------------------------------------------------------------------
std::wstring resolveUiWebDir() {
    // 1. Dev tree (compile-time path on developer machine)
    const std::wstring dev(REALS_UI_WEB_DIR_W);
    if (!dev.empty() &&
        GetFileAttributesW((dev + L"\\index.html").c_str()) != INVALID_FILE_ATTRIBUTES)
        return dev;

    // 2. Portable: check ui-web next to the DLL (if manually provided)
    if (g_hInstance) {
        wchar_t dllPath[MAX_PATH];
        if (GetModuleFileNameW(static_cast<HMODULE>(g_hInstance), dllPath, MAX_PATH) > 0) {
            std::wstring dllDir = dllPath;
            const size_t lastSlash = dllDir.find_last_of(L"\\/");
            if (lastSlash != std::wstring::npos) {
                dllDir = dllDir.substr(0, lastSlash);
                const std::wstring candidate = dllDir + L"\\ui-web";
                if (GetFileAttributesW((candidate + L"\\index.html").c_str()) != INVALID_FILE_ATTRIBUTES)
                    return candidate;
            }
        }
    }

    // 3. Embedded Assets: Auto-extract to %APPDATA%\RealsLab\ui-web on customer machine
    const std::string appDataUi = reals::platform::joinPath(reals::platform::dataDir(), "ui-web");
    reals::embedded::ensureUiWebExtracted(appDataUi);

    return toWide(appDataUi);
}

// ---------------------------------------------------------------------------
// Window + WebView lifecycle
// ---------------------------------------------------------------------------
RECT g_floatingRect{100, 100, 880, 740};

void pushDockState(bool docked);

bool isDockedInternal() {
    if (!g_hwnd) return false;
    if (GetParent(g_hwnd) != nullptr) return true;
    bool isFloating = false;
    int dockId = DockIsChildOfDock ? DockIsChildOfDock(g_hwnd, &isFloating) : -1;
    return (dockId >= 0 && !isFloating);
}

LRESULT CALLBACK hostWndProc(const HWND h, const UINT msg, const WPARAM wParam,
                             const LPARAM lParam) {
    switch (msg) {
    case WM_GETMINMAXINFO: {
        auto* mmi = reinterpret_cast<MINMAXINFO*>(lParam);
        if (mmi) {
            mmi->ptMinTrackSize.x = 180;
            mmi->ptMinTrackSize.y = 360;
        }
        return 0;
    }
    case WM_EXITSIZEMOVE: {
        if (g_hwnd && !isDockedInternal() && !IsIconic(g_hwnd) && !IsZoomed(g_hwnd)) {
            RECT rc{};
            GetWindowRect(g_hwnd, &rc);
            if (rc.right - rc.left >= 180 && rc.bottom - rc.top >= 200) {
                g_floatingRect = rc;
                char posBuf[64];
                std::snprintf(posBuf, sizeof(posBuf), "%ld,%ld,%ld,%ld",
                              rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
                if (SetExtState)
                    SetExtState("REALSLAB", "window_pos", posBuf, true);
            }
        }
        return 0;
    }
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
        SetForegroundWindow(h);
        SetCapture(h);
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

        // Check DAW transport cursor movement/seek to track phase-snap in real time
        bool phaseSnapped = false;
        if (g_bridge) {
            phaseSnapped = g_bridge->updatePhaseSnapFromHostTransport();
        }

        // Push audio playback state while playing. One extra frame is pushed when
        // playback stops (track ended / stop pressed) so the UI does not freeze
        // showing a stale "playing" state. Also push when phase-snap re-aligned so
        // the UI waveform playhead tracks the DAW cursor immediately.
        static bool s_wasPlaying = false;
        // Push state while EITHER path is audible: the core Engine (fallback)
        // or the native host preview. Engine::isPlaying() alone is always false
        // on the PlayPreviewEx path, which starved the UI of position updates.
        const bool playing = g_bridge ? g_bridge->isAudioActive()
                                      : reals::audio::Engine::instance().isPlaying();
        if (g_visible && (playing || s_wasPlaying || phaseSnapped))
            pushAudioState();
        s_wasPlaying = playing;
        // REAPER may dispatch the second transport hook asynchronously after
        // Main_OnCommand returns. Keep the internal-cycle guard alive until
        // this timer tick has completed, then release it for user commands.
        g_previewCycleGuard.store(false, std::memory_order_release);
    } catch (const std::exception& e) {
        LOG_ERROR(kTag, std::string("timerHook exception: ") + e.what());
    } catch (...) {
        LOG_ERROR(kTag, "timerHook unknown exception");
    }
}

void pushDockState(bool docked) {
    if (g_web && g_web->isReady()) {
        nlohmann::json d;
        d["event"] = "window.dockState";
        d["data"] = {{"docked", docked}};
        g_web->postJson(d.dump());
    }
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
        if (SetExtState)
            SetExtState("REALSLAB", "docked", "0", true);
        g_visible = true;
    } else {
        // DOCK: Save floating rect first
        RECT rc{};
        GetWindowRect(g_hwnd, &rc);
        if (rc.right - rc.left > 200 && rc.bottom - rc.top > 200) {
            g_floatingRect = rc;
            char posBuf[64];
            std::snprintf(posBuf, sizeof(posBuf), "%ld,%ld,%ld,%ld",
                          rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
            if (SetExtState)
                SetExtState("REALSLAB", "window_pos", posBuf, true);
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
        if (SetExtState)
            SetExtState("REALSLAB", "docked", "1", true);
        g_visible = true;
    }
}

void ExtHostActions::toggleDock() {
    toggleDockInternal();
}

bool ExtHostActions::isDocked() const {
    return isDockedInternal();
}

// ---------------------------------------------------------------------------
// Dedicated REAPER Chord Track Docker ("REALS_DOCKER" / Top Bar)
// ---------------------------------------------------------------------------
struct ChordTrackItem {
    double timeSeconds = 0.0;
    double duration = 2.0;
    std::string name;
    std::string roman;
};

struct ChordTrackState {
    std::mutex mtx;
    double bpm = 120.0;
    std::string masterKey = "C";
    std::string scaleMode = "Major";
    std::vector<ChordTrackItem> chords;
    double scrollTime = 0.0;
    float pixelsPerBeat = 40.0f;
    bool isVisible = false;
    bool isDragging = false;
    int dragStartX = 0;
};

static ChordTrackState g_chordState;
static HWND g_chordHwnd = nullptr;
static int g_cmdToggleChord = 0;

static void showChordDocker(bool show);
static HWND createChordWindow();
static void paintChordDocker(HDC hdc, const RECT& rc);
static LRESULT CALLBACK chordWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

void ExtHostActions::updateChordDockerStateFromProgression(const reals::lab::ChordProgression& prog) {
    std::lock_guard<std::mutex> lk(g_chordState.mtx);
    g_chordState.chords.clear();
    for (const auto& evt : prog.events) {
        ChordTrackItem item;
        item.timeSeconds = evt.startTime;
        item.duration = evt.endTime - evt.startTime;
        item.name = evt.name;
        item.roman = evt.roman;
        g_chordState.chords.push_back(item);
    }
    g_chordState.bpm = prog.bpm;
    if (!prog.masterKey.empty()) {
        g_chordState.masterKey = prog.masterKey;
    }
    if (!prog.scaleMode.empty()) {
        g_chordState.scaleMode = prog.scaleMode;
    }
    if (g_chordHwnd && IsWindow(g_chordHwnd)) {
        InvalidateRect(g_chordHwnd, nullptr, FALSE);
    }
}

struct FindArrangeData {
    HWND found = nullptr;
};

static BOOL CALLBACK enumChildFindArrange(HWND hwnd, LPARAM lParam) {
    if (!hwnd || !IsWindow(hwnd)) return TRUE;
    auto* data = reinterpret_cast<FindArrangeData*>(lParam);

    // Primary check: REAPER's Track View control ID is 1000
    if (GetDlgCtrlID(hwnd) == 1000) {
        data->found = hwnd;
        return FALSE;
    }

    // Secondary check: known REAPER class names
    char cls[128] = {0};
    if (GetClassNameA(hwnd, cls, sizeof(cls))) {
        if (strcmp(cls, "REAPERtrackview") == 0 ||
            strstr(cls, "trackview") != nullptr ||
            strstr(cls, "TrackView") != nullptr ||
            strstr(cls, "TrackList") != nullptr) {
            data->found = hwnd;
            return FALSE;
        }
    }
    return TRUE;
}

static HWND findArrangeWindow() {
    HWND mainHwnd = GetMainHwnd ? GetMainHwnd() : nullptr;
    if (!mainHwnd) return nullptr;

    // Layer 1: Direct GetDlgItem(1000)
    HWND direct = GetDlgItem(mainHwnd, 1000);
    if (direct && IsWindow(direct)) {
        return direct;
    }

    // Layer 2: EnumChildWindows searching for ID 1000 or class
    FindArrangeData data;
    EnumChildWindows(mainHwnd, enumChildFindArrange, reinterpret_cast<LPARAM>(&data));
    if (data.found && IsWindow(data.found)) {
        return data.found;
    }

    // Layer 3: GetThingFromPoint native hit testing
    if (GetThingFromPoint) {
        RECT mainRc{};
        if (GetWindowRect(mainHwnd, &mainRc)) {
            int testY = mainRc.top + (mainRc.bottom - mainRc.top) / 2;
            for (int sx = mainRc.left + 50; sx < mainRc.right - 50; sx += 30) {
                char infoBuf[64] = {0};
                GetThingFromPoint(sx, testY, infoBuf, sizeof(infoBuf));
                if (strcmp(infoBuf, "arrange") == 0) {
                    POINT pt{ sx, testY };
                    HWND hit = WindowFromPoint(pt);
                    if (hit && IsWindow(hit)) {
                        return hit;
                    }
                }
            }
        }
    }

    return nullptr;
}

static HWND getArrangeWindow() {
    static HWND s_cached = nullptr;
    static uint32_t s_lastCheckTick = 0;
    uint32_t now = GetTickCount();
    if (s_cached && IsWindow(s_cached) && (now - s_lastCheckTick < 2000)) {
        return s_cached;
    }
    s_lastCheckTick = now;
    s_cached = findArrangeWindow();
    return s_cached;
}

static int findArrangeLeftScreenX() {
    HWND arrangeWnd = getArrangeWindow();
    if (arrangeWnd && IsWindow(arrangeWnd)) {
        POINT pt{ 0, 0 };
        ClientToScreen(arrangeWnd, &pt);
        return pt.x;
    }

    if (GetThingFromPoint) {
        HWND mainHwnd = GetMainHwnd ? GetMainHwnd() : nullptr;
        if (mainHwnd) {
            RECT mainRc{};
            if (GetWindowRect(mainHwnd, &mainRc)) {
                int testY = static_cast<int>(mainRc.top + (mainRc.bottom - mainRc.top) / 2);
                int foundX = -1;
                const int startX = static_cast<int>(mainRc.left) + 20;
                const int endX = static_cast<int>(mainRc.right) - 50;
                for (int x = startX; x < endX; x += 16) {
                    char info[64] = {0};
                    GetThingFromPoint(x, testY, info, sizeof(info));
                    if (strcmp(info, "arrange") == 0) {
                        int lo = std::max<int>(static_cast<int>(mainRc.left), x - 16);
                        int hi = x;
                        while (lo < hi) {
                            int mid = lo + (hi - lo) / 2;
                            char midInfo[64] = {0};
                            GetThingFromPoint(mid, testY, midInfo, sizeof(midInfo));
                            if (strcmp(midInfo, "arrange") == 0) {
                                hi = mid;
                            } else {
                                lo = mid + 1;
                            }
                        }
                        foundX = lo;
                        break;
                    }
                }
                if (foundX > 0) {
                    return foundX;
                }
            }
        }
    }
    return -1;
}

static int findArrangeWidth(int startScreenX) {
    (void)startScreenX;
    HWND arrangeWnd = getArrangeWindow();
    if (arrangeWnd && IsWindow(arrangeWnd)) {
        RECT rc{};
        GetClientRect(arrangeWnd, &rc);
        if (rc.right > 80) {
            return rc.right;
        }
    }

    if (GetSetProjectInfo) {
        double w = GetSetProjectInfo(nullptr, "ARRANGE_W", 0, false);
        if (w > 80.0) {
            return static_cast<int>(w);
        }
    }
    return -1;
}

struct ChordDockerLayout {
    int padX = 6;
    int padY = 4;
    int cardH = 24;
    int statusX = 6;
    int statusW = 190;
    int timelineX = 204;
    int timelineW = 400;
    int controlsX = 600;
    int controlsW = 96;
};

static ChordDockerLayout getChordDockerLayout(HWND hwnd) {
    ChordDockerLayout layout;
    if (!hwnd || !IsWindow(hwnd)) return layout;

    RECT rc;
    GetClientRect(hwnd, &rc);
    int totalW = rc.right - rc.left;
    int totalH = rc.bottom - rc.top;
    if (totalW <= 0 || totalH <= 0) return layout;

    layout.cardH = totalH - layout.padY * 2;
    if (layout.cardH < 24) layout.cardH = 24;

    layout.statusX = layout.padX;
    layout.statusW = (totalW < 600) ? 140 : 190;
    layout.controlsW = 96;
    layout.controlsX = totalW - layout.controlsW - layout.padX;

    layout.timelineX = layout.padX + layout.statusW + 8;
    layout.timelineW = totalW - layout.timelineX - layout.controlsW - 8 - layout.padX;

    int arrangeScreenX = findArrangeLeftScreenX();
    if (arrangeScreenX > 0) {
        POINT ptLeft{ arrangeScreenX, 0 };
        ScreenToClient(hwnd, &ptLeft);

        if (ptLeft.x > layout.padX && ptLeft.x < totalW - layout.controlsW - 40) {
            layout.timelineX = ptLeft.x;
            layout.statusW = std::max(120, layout.timelineX - layout.padX - 6);
        }

        int arrangeW = findArrangeWidth(arrangeScreenX);
        if (arrangeW > 80) {
            POINT ptRight{ arrangeScreenX + arrangeW, 0 };
            ScreenToClient(hwnd, &ptRight);
            if (ptRight.x > layout.timelineX + 80 && ptRight.x <= totalW - layout.padX) {
                layout.timelineW = ptRight.x - layout.timelineX;
            } else {
                layout.timelineW = totalW - layout.timelineX - layout.controlsW - 8 - layout.padX;
            }
        }
    }

    if (layout.timelineX + layout.timelineW > layout.controlsX - 8) {
        layout.timelineW = layout.controlsX - 8 - layout.timelineX;
    }
    if (layout.timelineW < 80) layout.timelineW = 80;

    return layout;
}

static bool syncChordsFromProjectTrackInternal() {
    if (!CountTracks || !GetTrack || !GetSetMediaTrackInfo_String ||
        !CountTrackMediaItems || !GetTrackMediaItem || !GetActiveTake ||
        !MIDI_CountEvts || !MIDI_GetTextSysexEvt || !MIDI_GetProjTimeFromPPQPos) {
        return false;
    }

    MediaTrack* chordTrack = nullptr;
    const int numTracks = CountTracks(nullptr);
    for (int i = 0; i < numTracks; ++i) {
        MediaTrack* tr = GetTrack(nullptr, i);
        if (!tr) continue;
        char nameBuf[256] = {0};
        GetSetMediaTrackInfo_String(tr, "P_NAME", nameBuf, false);
        std::string sName = nameBuf;
        for (char& c : sName) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        if (sName == "CHORD TRACK" || sName == "CHORDS" || sName.find("CHORD") != std::string::npos || sName.find("HOP AM") != std::string::npos) {
            chordTrack = tr;
            break;
        }
    }
    if (!chordTrack) return false;

    const int numItems = CountTrackMediaItems(chordTrack);
    if (numItems <= 0) return false;

    std::vector<ChordTrackItem> loadedChords;

    for (int m = 0; m < numItems; ++m) {
        MediaItem* item = GetTrackMediaItem(chordTrack, m);
        if (!item) continue;
        MediaItem_Take* take = GetActiveTake(item);
        if (!take) continue;

        int numNotes = 0, numCC = 0, numTextSysex = 0;
        if (!MIDI_CountEvts(take, &numNotes, &numCC, &numTextSysex)) continue;
        if (numTextSysex <= 0) continue;

        double itemPos = GetMediaItemInfo_Value ? GetMediaItemInfo_Value(item, "D_POSITION") : 0.0;
        double itemLen = GetMediaItemInfo_Value ? GetMediaItemInfo_Value(item, "D_LENGTH") : 0.0;

        for (int i = 0; i < numTextSysex; ++i) {
            bool selected = false, muted = false;
            double ppqpos = 0.0;
            int type = 0;
            char msgBuf[256] = {0};
            int msgBufLen = sizeof(msgBuf) - 1;
            if (MIDI_GetTextSysexEvt(take, i, &selected, &muted, &ppqpos, &type, msgBuf, &msgBufLen)) {
                if (type == 1 && msgBufLen > 0) {
                    std::string label(msgBuf, msgBufLen);
                    std::string chordName = label;
                    std::string roman = "";
                    size_t parenOpen = label.find('(');
                    size_t parenClose = label.find(')');
                    if (parenOpen != std::string::npos && parenClose != std::string::npos && parenClose > parenOpen) {
                        chordName = label.substr(0, parenOpen);
                        while (!chordName.empty() && chordName.back() == ' ') chordName.pop_back();
                        roman = label.substr(parenOpen + 1, parenClose - parenOpen - 1);
                    }
                    double projTime = MIDI_GetProjTimeFromPPQPos(take, ppqpos);
                    ChordTrackItem ci;
                    ci.timeSeconds = projTime;
                    ci.name = chordName;
                    ci.roman = roman;
                    ci.duration = 2.0;
                    loadedChords.push_back(ci);
                }
            }
        }

        for (size_t i = 0; i < loadedChords.size(); ++i) {
            if (i + 1 < loadedChords.size()) {
                double diff = loadedChords[i + 1].timeSeconds - loadedChords[i].timeSeconds;
                if (diff > 0.05) loadedChords[i].duration = diff;
            } else if (itemLen > 0.0) {
                double rem = (itemPos + itemLen) - loadedChords[i].timeSeconds;
                if (rem > 0.05) loadedChords[i].duration = rem;
            }
        }
    }

    if (loadedChords.empty()) return false;

    {
        std::lock_guard<std::mutex> lk(g_chordState.mtx);
        g_chordState.chords = std::move(loadedChords);
        if (Master_GetTempo) {
            g_chordState.bpm = Master_GetTempo();
        }
    }
    return true;
}

void ExtHostActions::insertChordTrackFromDocker() {
    std::vector<ChordTrackItem> items;
    double bpmVal = 120.0;
    {
        std::lock_guard<std::mutex> lk(g_chordState.mtx);
        items = g_chordState.chords;
        bpmVal = (g_chordState.bpm > 0) ? g_chordState.bpm : (Master_GetTempo ? Master_GetTempo() : 120.0);
    }
    if (items.empty()) return;

    nlohmann::json j;
    j["bpm"] = bpmVal;
    j["chords"] = nlohmann::json::array();
    for (const auto& c : items) {
        nlohmann::json ch;
        ch["time"] = c.timeSeconds;
        ch["duration"] = c.duration;
        ch["chord"] = c.name;
        ch["roman"] = c.roman;
        j["chords"].push_back(ch);
    }
    insertChordTrack(j.dump());
}

void ExtHostActions::showChordDocker(bool show, const std::string& chordDataJson) {
    nlohmann::json chordData;
    if (!chordDataJson.empty()) {
        try {
            chordData = nlohmann::json::parse(chordDataJson);
        } catch (...) {}
    }
    if (chordData.contains("toggle") && chordData["toggle"].get<bool>()) {
        show = !g_chordState.isVisible;
    }
    if (!chordData.is_null() && !chordData.empty()) {
        std::lock_guard<std::mutex> lk(g_chordState.mtx);
        if (chordData.contains("bpm") && chordData["bpm"].is_number()) {
            g_chordState.bpm = chordData["bpm"].get<double>();
        } else if (Master_GetTempo) {
            g_chordState.bpm = Master_GetTempo();
        }
        if (chordData.contains("masterKey") && chordData["masterKey"].is_string()) {
            g_chordState.masterKey = chordData["masterKey"].get<std::string>();
        }
        if (chordData.contains("scaleMode") && chordData["scaleMode"].is_string()) {
            g_chordState.scaleMode = chordData["scaleMode"].get<std::string>();
        }

        double offsetSec = 0.0;
        if (chordData.contains("itemPosition") && chordData["itemPosition"].is_number()) {
            offsetSec = chordData["itemPosition"].get<double>();
        } else if (chordData.contains("itemPos") && chordData["itemPos"].is_number()) {
            offsetSec = chordData["itemPos"].get<double>();
        }
        if (offsetSec <= 0.0 && GetSelectedMediaItem && GetMediaItemInfo_Value) {
            MediaItem* selItem = GetSelectedMediaItem(nullptr, 0);
            if (selItem) {
                double pos = GetMediaItemInfo_Value(selItem, "D_POSITION");
                if (pos > 0.0) {
                    offsetSec = pos;
                }
            }
        }

        if (chordData.contains("chords") && chordData["chords"].is_array() && !chordData["chords"].empty()) {
            g_chordState.chords.clear();
            for (const auto& c : chordData["chords"]) {
                ChordTrackItem item;

                double startTime = 0.0;
                if (c.contains("start") && c["start"].is_number()) {
                    startTime = c["start"].get<double>();
                } else if (c.contains("time") && c["time"].is_number()) {
                    startTime = c["time"].get<double>();
                } else if (c.contains("timestamp") && c["timestamp"].is_number()) {
                    startTime = c["timestamp"].get<double>();
                }
                item.timeSeconds = startTime + offsetSec;

                double dur = 0.0;
                if (c.contains("duration") && c["duration"].is_number()) {
                    dur = c["duration"].get<double>();
                } else if (c.contains("end") && c["end"].is_number()) {
                    double e = c["end"].get<double>();
                    if (e > startTime) dur = e - startTime;
                } else if (c.contains("end_time") && c["end_time"].is_number()) {
                    double e = c["end_time"].get<double>();
                    if (e > startTime) dur = e - startTime;
                }
                if (dur <= 0.01) {
                    dur = (60.0 / (g_chordState.bpm > 0 ? g_chordState.bpm : 120.0)) * 2.0;
                }
                item.duration = dur;

                if (c.contains("chord") && c["chord"].is_string()) {
                    item.name = c["chord"].get<std::string>();
                } else if (c.contains("name") && c["name"].is_string()) {
                    item.name = c["name"].get<std::string>();
                }
                if (c.contains("roman") && c["roman"].is_string()) {
                    item.roman = c["roman"].get<std::string>();
                } else if (c.contains("roman_numeral") && c["roman_numeral"].is_string()) {
                    item.roman = c["roman_numeral"].get<std::string>();
                }

                if (!item.name.empty() && item.name != "N" && item.name != "None") {
                    g_chordState.chords.push_back(item);
                }
            }

            for (size_t i = 0; i < g_chordState.chords.size(); ++i) {
                if (i + 1 < g_chordState.chords.size()) {
                    double gap = g_chordState.chords[i + 1].timeSeconds - g_chordState.chords[i].timeSeconds;
                    if (gap > 0.05) {
                        g_chordState.chords[i].duration = gap;
                    }
                }
            }
        }
    }
    if (g_chordState.chords.empty()) {
        syncChordsFromProjectTrackInternal();
    }
    ::showChordDocker(show);
}

static HWND createChordWindow() {
    if (g_chordHwnd && IsWindow(g_chordHwnd)) {
        return g_chordHwnd;
    }
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.lpfnWndProc = chordWndProc;
    wc.hInstance = g_hInstance;
    wc.lpszClassName = L"RealSChordDockerClass";
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    RegisterClassExW(&wc);

    HWND parent = GetMainHwnd ? GetMainHwnd() : nullptr;
    g_chordHwnd = CreateWindowExW(
        0,
        L"RealSChordDockerClass",
        L"RealS",
        WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE,
        0, 0, 800, 48,
        parent,
        nullptr,
        g_hInstance,
        nullptr
    );

    if (g_chordHwnd && DockWindowAddEx) {
        DockWindowAddEx(g_chordHwnd, "RealS", "REALS_DOCKER", true);
    }
    return g_chordHwnd;
}

static void showChordDocker(bool show) {
    if (show) {
        if (!g_chordHwnd || !IsWindow(g_chordHwnd)) {
            createChordWindow();
        }
        if (g_chordHwnd) {
            ShowWindow(g_chordHwnd, SW_SHOW);
            if (DockWindowActivate) {
                DockWindowActivate(g_chordHwnd);
            }
            g_chordState.isVisible = true;
            InvalidateRect(g_chordHwnd, nullptr, TRUE);
        }
    } else {
        if (g_chordHwnd && IsWindow(g_chordHwnd)) {
            ShowWindow(g_chordHwnd, SW_HIDE);
            g_chordState.isVisible = false;
        }
    }
}

struct ChordDockerTheme {
    std::string name;
    COLORREF bgDocker;        // Background of docker strip
    COLORREF borderDocker;    // Top/bottom edge border
    COLORREF cardBg;          // Status & controls capsule bg
    COLORREF cardBorder;      // Capsule border
    COLORREF textPrimary;     // Main text ("REALS CHORD")
    COLORREF textSecondary;   // Secondary text (BPM, key, ruler)
    COLORREF textTertiary;    // Dim text / close button
    COLORREF accent;          // Dot, +MIDI button bg
    COLORREF accentBorder;    // Button border
    COLORREF accentText;      // Button text
    COLORREF timelineBg;      // Timeline background
    COLORREF timelineBorder;  // Timeline container border
    COLORREF rulerDivider;    // Line under ruler
    COLORREF measLine;        // Measure grid line
    COLORREF beatLine;        // Beat grid line
    COLORREF chordBg;         // Chord item pill background
    COLORREF chordBorder;     // Chord item border
    COLORREF chordText;       // Chord item label text
    COLORREF playhead;        // Playhead cursor color
};

static ChordDockerTheme getChordDockerTheme() {
    std::string theme = "dark-studio";
    if (GetExtState) {
        const char* raw = GetExtState("REALSLAB", "theme");
        if (raw && *raw) theme = raw;
    }
    if (theme.empty()) {
        theme = reals::config::Config::instance().getString("theme", "dark-studio");
    }

    ChordDockerTheme t;
    t.name = theme;

    if (theme == "pastel-pink") {
        // Pastel Pink (Cutecore Light)
        t.bgDocker       = RGB(255, 245, 248); // #FFF5F8
        t.borderDocker   = RGB(250, 212, 226); // #FAD4E2
        t.cardBg         = RGB(255, 255, 255); // #FFFFFF
        t.cardBorder     = RGB(245, 198, 216); // #F5C6D8
        t.textPrimary    = RGB(69, 51, 64);    // #453340
        t.textSecondary  = RGB(133, 110, 128); // #856E80
        t.textTertiary   = RGB(150, 130, 145); // #968291
        t.accent         = RGB(255, 92, 138);  // #FF5C8A
        t.accentBorder   = RGB(230, 69, 115);  // #E64573
        t.accentText     = RGB(255, 255, 255); // #FFFFFF
        t.timelineBg     = RGB(255, 241, 246); // #FFF1F6
        t.timelineBorder = RGB(245, 198, 216); // #F5C6D8
        t.rulerDivider   = RGB(235, 181, 202); // #EBB5CA
        t.measLine       = RGB(224, 166, 190); // #E0A6BE
        t.beatLine       = RGB(245, 201, 219); // #F5C9DB
        t.chordBg        = RGB(255, 255, 255); // #FFFFFF
        t.chordBorder    = RGB(255, 92, 138);  // #FF5C8A
        t.chordText      = RGB(69, 51, 64);    // #453340
        t.playhead       = RGB(255, 92, 138);  // #FF5C8A
    } else if (theme == "cyberpunk") {
        // Cyberpunk (Neon High-Contrast)
        t.bgDocker       = RGB(11, 11, 20);    // #0B0B14
        t.borderDocker   = RGB(32, 35, 61);    // #20233D
        t.cardBg         = RGB(21, 22, 36);    // #151624
        t.cardBorder     = RGB(40, 43, 77);    // #282B4D
        t.textPrimary    = RGB(240, 244, 255); // #F0F4FF
        t.textSecondary  = RGB(138, 149, 199); // #8A95C7
        t.textTertiary   = RGB(91, 102, 153);  // #5B6699
        t.accent         = RGB(0, 240, 255);   // #00F0FF
        t.accentBorder   = RGB(0, 196, 209);   // #00C4D1
        t.accentText     = RGB(4, 4, 7);       // #040407
        t.timelineBg     = RGB(8, 8, 14);      // #08080E
        t.timelineBorder = RGB(32, 35, 61);    // #20233D
        t.rulerDivider   = RGB(32, 35, 61);    // #20233D
        t.measLine       = RGB(56, 60, 110);   // #383C6E
        t.beatLine       = RGB(32, 35, 61);    // #20233D
        t.chordBg        = RGB(22, 23, 42);    // #16172A
        t.chordBorder    = RGB(0, 240, 255);   // #00F0FF
        t.chordText      = RGB(240, 244, 255); // #F0F4FF
        t.playhead       = RGB(0, 240, 255);   // #00F0FF
    } else {
        // Dark Studio (Default Dark Premium)
        t.bgDocker       = RGB(18, 19, 22);    // #121316
        t.borderDocker   = RGB(36, 38, 43);    // #24262B
        t.cardBg         = RGB(26, 28, 32);    // #1A1C20
        t.cardBorder     = RGB(44, 47, 53);    // #2C2F35
        t.textPrimary    = RGB(242, 243, 245); // #F2F3F5
        t.textSecondary  = RGB(163, 166, 173); // #A3A6AD
        t.textTertiary   = RGB(115, 119, 128); // #737780
        t.accent         = RGB(255, 107, 44);  // #FF6B2C
        t.accentBorder   = RGB(234, 88, 12);   // #EA580C
        t.accentText     = RGB(255, 255, 255); // #FFFFFF
        t.timelineBg     = RGB(13, 14, 17);    // #0D0E11
        t.timelineBorder = RGB(36, 38, 43);    // #24262B
        t.rulerDivider   = RGB(36, 38, 43);    // #24262B
        t.measLine       = RGB(60, 63, 72);    // #3C3F48
        t.beatLine       = RGB(36, 38, 43);    // #24262B
        t.chordBg        = RGB(32, 35, 43);    // #20232B
        t.chordBorder    = RGB(54, 57, 65);    // #363941
        t.chordText      = RGB(255, 255, 255); // #FFFFFF
        t.playhead       = RGB(255, 107, 44);  // #FF6B2C
    }
    return t;
}

static void paintChordDocker(HDC hdc, const RECT& rc) {
    int totalW = rc.right - rc.left;
    int totalH = rc.bottom - rc.top;
    if (totalW <= 0 || totalH <= 0) return;

    ChordDockerTheme t = getChordDockerTheme();

    HBRUSH bgBrush = CreateSolidBrush(t.bgDocker);
    FillRect(hdc, &rc, bgBrush);
    DeleteObject(bgBrush);

    // Subtle top & bottom border line
    HPEN borderPen = CreatePen(PS_SOLID, 1, t.borderDocker);
    HPEN oldPen = (HPEN)SelectObject(hdc, borderPen);
    MoveToEx(hdc, rc.left, rc.top, nullptr);
    LineTo(hdc, rc.right, rc.top);
    MoveToEx(hdc, rc.left, rc.bottom - 1, nullptr);
    LineTo(hdc, rc.right, rc.bottom - 1);
    SelectObject(hdc, oldPen);
    DeleteObject(borderPen);

    ChordDockerLayout layout = getChordDockerLayout(g_chordHwnd);
    int padY = layout.padY;
    int cardH = layout.cardH;
    int statusW = layout.statusW;
    int statusX = layout.statusX;
    int statusY = padY;
    int timelineX = layout.timelineX;
    int timelineW = layout.timelineW;
    int controlsX = layout.controlsX;
    int controlsW = layout.controlsW;
    int controlsY = padY;

    // 1. Status Capsule (Left)
    {
        RECT statusRc{ statusX, statusY, statusX + statusW, statusY + cardH };
        HBRUSH statusBg = CreateSolidBrush(t.cardBg);
        HPEN statusPen = CreatePen(PS_SOLID, 1, t.cardBorder);
        HBRUSH oldB = (HBRUSH)SelectObject(hdc, statusBg);
        HPEN oldP = (HPEN)SelectObject(hdc, statusPen);
        RoundRect(hdc, statusRc.left, statusRc.top, statusRc.right, statusRc.bottom, 12, 12);
        SelectObject(hdc, oldB);
        SelectObject(hdc, oldP);
        DeleteObject(statusBg);
        DeleteObject(statusPen);

        int dotSize = 12;
        int dotX = statusRc.left + 10;
        int dotY = statusRc.top + (cardH - dotSize) / 2;
        HBRUSH dotBg = CreateSolidBrush(t.accent);
        HPEN dotPen = CreatePen(PS_SOLID, 1, t.accentBorder);
        oldB = (HBRUSH)SelectObject(hdc, dotBg);
        oldP = (HPEN)SelectObject(hdc, dotPen);
        Ellipse(hdc, dotX, dotY, dotX + dotSize, dotY + dotSize);
        SelectObject(hdc, oldB);
        SelectObject(hdc, oldP);
        DeleteObject(dotBg);
        DeleteObject(dotPen);

        SetBkMode(hdc, TRANSPARENT);
        HFONT hTitleFont = CreateFontW(
            -11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hSubFont = CreateFontW(
            -10, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");

        HFONT oldFont = (HFONT)SelectObject(hdc, hTitleFont);
        SetTextColor(hdc, t.textPrimary);
        RECT rTitle{ dotX + dotSize + 8, statusRc.top + 2, statusRc.right - 6, statusRc.top + cardH / 2 + 1 };
        DrawTextW(hdc, L"REALS CHORD", -1, &rTitle, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, hSubFont);
        SetTextColor(hdc, t.textSecondary);
        RECT rSub{ dotX + dotSize + 8, statusRc.top + cardH / 2 - 1, statusRc.right - 6, statusRc.bottom - 2 };

        std::string bpmStr = "---";
        std::string keyStr = "---";
        {
            std::lock_guard<std::mutex> lk(g_chordState.mtx);
            if (g_chordState.bpm > 0) {
                bpmStr = std::to_string(static_cast<int>(std::round(g_chordState.bpm)));
            } else if (Master_GetTempo) {
                bpmStr = std::to_string(static_cast<int>(std::round(Master_GetTempo())));
            }
            if (!g_chordState.masterKey.empty()) {
                keyStr = g_chordState.masterKey;
                if (!g_chordState.scaleMode.empty()) {
                    keyStr += " " + g_chordState.scaleMode;
                }
            }
        }
        std::wstring subW = toWide(bpmStr + " BPM / " + keyStr);
        DrawTextW(hdc, subW.c_str(), -1, &rSub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, oldFont);
        DeleteObject(hTitleFont);
        DeleteObject(hSubFont);
    }

    // 2. Controls Capsule (Right)
    {
        RECT ctrlRc{ controlsX, controlsY, controlsX + controlsW, controlsY + cardH };
        HBRUSH ctrlBg = CreateSolidBrush(t.cardBg);
        HPEN ctrlPen = CreatePen(PS_SOLID, 1, t.cardBorder);
        HBRUSH oldB = (HBRUSH)SelectObject(hdc, ctrlBg);
        HPEN oldP = (HPEN)SelectObject(hdc, ctrlPen);
        RoundRect(hdc, ctrlRc.left, ctrlRc.top, ctrlRc.right, ctrlRc.bottom, 12, 12);
        SelectObject(hdc, oldB);
        SelectObject(hdc, oldP);
        DeleteObject(ctrlBg);
        DeleteObject(ctrlPen);

        RECT midiBtnRc{ ctrlRc.left + 6, ctrlRc.top + 4, ctrlRc.right - 30, ctrlRc.bottom - 4 };
        HBRUSH midiBg = CreateSolidBrush(t.accent);
        HPEN midiPen = CreatePen(PS_SOLID, 1, t.accentBorder);
        oldB = (HBRUSH)SelectObject(hdc, midiBg);
        oldP = (HPEN)SelectObject(hdc, midiPen);
        RoundRect(hdc, midiBtnRc.left, midiBtnRc.top, midiBtnRc.right, midiBtnRc.bottom, 6, 6);
        SelectObject(hdc, oldB);
        SelectObject(hdc, oldP);
        DeleteObject(midiBg);
        DeleteObject(midiPen);

        HFONT hBtnFont = CreateFontW(
            -10, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT oldFont = (HFONT)SelectObject(hdc, hBtnFont);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, t.accentText);
        DrawTextW(hdc, L"+ MIDI", -1, &midiBtnRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        RECT closeBtnRc{ ctrlRc.right - 26, ctrlRc.top + 4, ctrlRc.right - 6, ctrlRc.bottom - 4 };
        SetTextColor(hdc, t.textTertiary);
        DrawTextW(hdc, L"✕", -1, &closeBtnRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

        SelectObject(hdc, oldFont);
        DeleteObject(hBtnFont);
    }

    // 3. Timeline Container (Middle) — Strictly Aligned with REAPER Arrange View
    {
        RECT tlRc{ timelineX, padY, timelineX + timelineW, padY + cardH };
        HBRUSH tlBg = CreateSolidBrush(t.timelineBg);
        HPEN tlPen = CreatePen(PS_SOLID, 1, t.timelineBorder);
        HBRUSH oldB = (HBRUSH)SelectObject(hdc, tlBg);
        HPEN oldP = (HPEN)SelectObject(hdc, tlPen);
        RoundRect(hdc, tlRc.left, tlRc.top, tlRc.right, tlRc.bottom, 8, 8);
        SelectObject(hdc, oldB);
        SelectObject(hdc, oldP);
        DeleteObject(tlBg);
        DeleteObject(tlPen);

        HRGN clipRgn = CreateRoundRectRgn(tlRc.left + 1, tlRc.top + 1, tlRc.right - 1, tlRc.bottom - 1, 6, 6);
        SelectClipRgn(hdc, clipRgn);

        int rulerH = 14;
        RECT rulerRc{ tlRc.left, tlRc.top, tlRc.right, tlRc.top + rulerH };
        RECT cellsRc{ tlRc.left, tlRc.top + rulerH, tlRc.right, tlRc.bottom };

        HPEN divPen = CreatePen(PS_SOLID, 1, t.rulerDivider);
        oldP = (HPEN)SelectObject(hdc, divPen);
        MoveToEx(hdc, rulerRc.left, rulerRc.bottom, nullptr);
        LineTo(hdc, rulerRc.right, rulerRc.bottom);
        SelectObject(hdc, oldP);
        DeleteObject(divPen);

        double arrangeStartTime = 0.0;
        double arrangeEndTime = 0.0;
        if (GetSet_ArrangeView2) {
            GetSet_ArrangeView2(nullptr, false, 0, 0, &arrangeStartTime, &arrangeEndTime);
        }
        double hZoom = (GetHZoomLevel) ? GetHZoomLevel() : 40.0;
        if (hZoom <= 0.0) hZoom = 40.0;
        if (arrangeEndTime <= arrangeStartTime) {
            arrangeEndTime = arrangeStartTime + static_cast<double>(timelineW) / hZoom;
        }

        std::vector<ChordTrackItem> chords;
        {
            std::lock_guard<std::mutex> lk(g_chordState.mtx);
            chords = g_chordState.chords;
        }
        if (chords.empty()) {
            syncChordsFromProjectTrackInternal();
            {
                std::lock_guard<std::mutex> lk(g_chordState.mtx);
                chords = g_chordState.chords;
            }
        }

        bool isPlaying = (GetPlayState && (GetPlayState() & 1));
        double curTime = 0.0;
        if (isPlaying) {
            curTime = GetPlayPosition ? GetPlayPosition() : 0.0;
        } else {
            curTime = GetCursorPosition ? GetCursorPosition() : 0.0;
        }

        HFONT hRulerFont = CreateFontW(
            -9, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hChordFont = CreateFontW(
            -11, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI");
        HFONT hBeatSubFont = CreateFontW(
            -7, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Consolas");

        HPEN measPen = CreatePen(PS_SOLID, 1, t.measLine);
        HPEN beatPen = CreatePen(PS_SOLID, 1, t.beatLine);

        HFONT oldF = (HFONT)SelectObject(hdc, hRulerFont);
        SetBkMode(hdc, TRANSPARENT);

        // Draw Measure lines according to REAPER TimeMap
        if (TimeMap2_timeToBeats && TimeMap2_beatsToTime) {
            int startMeas = 0;
            TimeMap2_timeToBeats(nullptr, arrangeStartTime, &startMeas, nullptr, nullptr, nullptr);
            if (startMeas < 0) startMeas = 0;

            for (int m = std::max(0, startMeas - 1); m < startMeas + 300; ++m) {
                int measIndex = m;
                double mTime = TimeMap2_beatsToTime(nullptr, 0.0, &measIndex);
                if (mTime > arrangeEndTime + 2.0) break;

                int mx = timelineX + static_cast<int>(std::round((mTime - arrangeStartTime) * hZoom));
                if (mx < timelineX - 2) continue;
                if (mx > timelineX + timelineW) break;

                SelectObject(hdc, measPen);
                MoveToEx(hdc, mx, rulerRc.top, nullptr);
                LineTo(hdc, mx, rulerRc.bottom);

                int mNum = m + 1;
                std::wstring mLabel = std::to_wstring(mNum) + L".1.00";
                RECT mRc{ mx + 3, rulerRc.top, mx + 80, rulerRc.bottom };
                SetTextColor(hdc, t.textSecondary);
                DrawTextW(hdc, mLabel.c_str(), -1, &mRc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

                MoveToEx(hdc, mx, cellsRc.top, nullptr);
                LineTo(hdc, mx, cellsRc.bottom);
            }
        }

        // Draw Chord Blocks
        if (!chords.empty()) {
            for (const auto& ch : chords) {
                int startX = timelineX + static_cast<int>(std::round((ch.timeSeconds - arrangeStartTime) * hZoom));
                int chordW = static_cast<int>(std::round(ch.duration * hZoom));
                if (startX + chordW < timelineX || startX > timelineX + timelineW) continue;

                int drawLeft = std::max(startX, timelineX);
                int drawRight = std::min(startX + chordW, timelineX + timelineW);
                if (drawRight <= drawLeft) continue;

                int pillW = drawRight - drawLeft;
                RECT chPill{ drawLeft + 1, cellsRc.top + 2, drawRight - 1, cellsRc.bottom - 2 };
                if (chPill.right <= chPill.left) chPill.right = chPill.left + 1;

                HBRUSH cellBg = CreateSolidBrush(t.chordBg);
                HPEN cellPen = CreatePen(PS_SOLID, 1, t.chordBorder);
                HBRUSH prevB = (HBRUSH)SelectObject(hdc, cellBg);
                HPEN prevP = (HPEN)SelectObject(hdc, cellPen);
                RoundRect(hdc, chPill.left, chPill.top, chPill.right, chPill.bottom, 4, 4);
                SelectObject(hdc, prevB);
                SelectObject(hdc, prevP);
                DeleteObject(cellBg);
                DeleteObject(cellPen);

                if (pillW >= 14) {
                    HFONT fontToUse = (pillW < 32) ? hBeatSubFont : hChordFont;
                    SelectObject(hdc, fontToUse);
                    SetTextColor(hdc, t.chordText);
                    std::string label = ch.name;
                    if (!ch.roman.empty() && (pillW > 55)) {
                        label += " (" + ch.roman + ")";
                    }
                    std::wstring labelW = toWide(label);
                    DrawTextW(hdc, labelW.c_str(), -1, &chPill, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
                }
            }
        } else {
            SelectObject(hdc, hRulerFont);
            SetTextColor(hdc, t.textTertiary);
            RECT hintRc = cellsRc;
            DrawTextW(hdc, L"Chưa có hợp âm • Hãy chọn item trong DAW rồi bấm Dò Hợp Âm trong Audio Lab", -1, &hintRc, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        }

        // Draw Vertical Accent Playhead
        int playheadX = timelineX + static_cast<int>(std::round((curTime - arrangeStartTime) * hZoom));
        if (playheadX >= timelineX && playheadX <= timelineX + timelineW) {
            HPEN playPen = CreatePen(PS_SOLID, 2, t.playhead);
            SelectObject(hdc, playPen);
            MoveToEx(hdc, playheadX, tlRc.top, nullptr);
            LineTo(hdc, playheadX, tlRc.bottom);
            DeleteObject(playPen);

            POINT tri[3] = {
                { playheadX - 4, tlRc.top },
                { playheadX + 4, tlRc.top },
                { playheadX, tlRc.top + 6 }
            };
            HBRUSH triBrush = CreateSolidBrush(t.playhead);
            HPEN triPen = CreatePen(PS_SOLID, 1, t.playhead);
            SelectObject(hdc, triBrush);
            SelectObject(hdc, triPen);
            Polygon(hdc, tri, 3);
            DeleteObject(triBrush);
            DeleteObject(triPen);
        }

        SelectObject(hdc, oldF);
        DeleteObject(hRulerFont);
        DeleteObject(hChordFont);
        DeleteObject(hBeatSubFont);
        DeleteObject(measPen);
        DeleteObject(beatPen);

        SelectClipRgn(hdc, nullptr);
        DeleteObject(clipRgn);
    }
}

static LRESULT CALLBACK chordWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_CREATE: {
            SetTimer(hwnd, 1, 33, nullptr);
            return 0;
        }
        case WM_TIMER: {
            if (wParam == 1 && IsWindowVisible(hwnd)) {
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;
            if (w > 0 && h > 0) {
                HDC memDC = CreateCompatibleDC(hdc);
                HBITMAP memBmp = CreateCompatibleBitmap(hdc, w, h);
                HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

                paintChordDocker(memDC, rc);

                BitBlt(hdc, 0, 0, w, h, memDC, 0, 0, SRCCOPY);
                SelectObject(memDC, oldBmp);
                DeleteObject(memBmp);
                DeleteDC(memDC);
            }
            EndPaint(hwnd, &ps);
            return 0;
        }
        case WM_LBUTTONDOWN: {
            int mouseX = LOWORD(lParam);
            int mouseY = HIWORD(lParam);

            ChordDockerLayout layout = getChordDockerLayout(hwnd);

            if (mouseX >= layout.controlsX && mouseX <= layout.controlsX + layout.controlsW &&
                mouseY >= layout.padY && mouseY <= layout.padY + layout.cardH) {
                if (mouseX >= layout.controlsX + 6 && mouseX <= layout.controlsX + layout.controlsW - 30) {
                    g_actions.insertChordTrackFromDocker();
                    return 0;
                }
                if (mouseX >= layout.controlsX + layout.controlsW - 26 && mouseX <= layout.controlsX + layout.controlsW - 6) {
                    showChordDocker(false);
                    return 0;
                }
            }

            if (mouseX >= layout.timelineX && mouseX <= layout.timelineX + layout.timelineW) {
                double arrangeStartTime = 0.0;
                double arrangeEndTime = 0.0;
                if (GetSet_ArrangeView2) {
                    GetSet_ArrangeView2(nullptr, false, 0, 0, &arrangeStartTime, &arrangeEndTime);
                }
                double hZoom = (GetHZoomLevel) ? GetHZoomLevel() : 40.0;
                if (hZoom <= 0.0) hZoom = 40.0;

                double targetSec = arrangeStartTime + static_cast<double>(mouseX - layout.timelineX) / hZoom;
                if (targetSec < 0.0) targetSec = 0.0;

                if (SetEditCurPos2) {
                    SetEditCurPos2(nullptr, targetSec, true, false);
                }
                g_chordState.isDragging = true;
                g_chordState.dragStartX = mouseX;
                SetCapture(hwnd);
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_MOUSEMOVE: {
            if (g_chordState.isDragging && (wParam & MK_LBUTTON)) {
                int mouseX = LOWORD(lParam);
                ChordDockerLayout layout = getChordDockerLayout(hwnd);

                double arrangeStartTime = 0.0;
                double arrangeEndTime = 0.0;
                if (GetSet_ArrangeView2) {
                    GetSet_ArrangeView2(nullptr, false, 0, 0, &arrangeStartTime, &arrangeEndTime);
                }
                double hZoom = (GetHZoomLevel) ? GetHZoomLevel() : 40.0;
                if (hZoom <= 0.0) hZoom = 40.0;

                double targetSec = arrangeStartTime + static_cast<double>(mouseX - layout.timelineX) / hZoom;
                if (targetSec < 0.0) targetSec = 0.0;

                if (SetEditCurPos2) {
                    SetEditCurPos2(nullptr, targetSec, false, false);
                }
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;
        }
        case WM_LBUTTONUP: {
            if (g_chordState.isDragging) {
                g_chordState.isDragging = false;
                ReleaseCapture();
            }
            return 0;
        }
        case WM_MOUSEWHEEL: {
            short delta = GET_WHEEL_DELTA_WPARAM(wParam);
            if (wParam & MK_CONTROL) {
                if (CSurf_OnZoom) {
                    CSurf_OnZoom(delta > 0 ? 1 : -1, 0);
                } else if (Main_OnCommand) {
                    Main_OnCommand(delta > 0 ? 1011 : 1012, 0);
                }
            } else {
                if (CSurf_OnScroll) {
                    CSurf_OnScroll(delta > 0 ? -1 : 1, 0);
                } else if (Main_OnCommand) {
                    Main_OnCommand(delta > 0 ? 40140 : 40141, 0);
                }
            }
            InvalidateRect(hwnd, nullptr, FALSE);
            return 0;
        }
        case WM_RBUTTONUP: {
            POINT pt;
            GetCursorPos(&pt);
            HMENU hMenu = CreatePopupMenu();
            AppendMenuW(hMenu, MF_STRING, 1, L"Chèn Track MIDI vào REAPER");
            AppendMenuW(hMenu, MF_STRING, 2, L"Đồng bộ từ Track Dự án (CHORD TRACK)");
            AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
            AppendMenuW(hMenu, MF_STRING, 3, L"Ẩn Chord Track");
            int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_RIGHTBUTTON, pt.x, pt.y, 0, hwnd, nullptr);
            DestroyMenu(hMenu);
            if (cmd == 1) {
                g_actions.insertChordTrackFromDocker();
            } else if (cmd == 2) {
                syncChordsFromProjectTrackInternal();
                InvalidateRect(hwnd, nullptr, FALSE);
            } else if (cmd == 3) {
                showChordDocker(false);
            }
            return 0;
        }
        case WM_DESTROY: {
            KillTimer(hwnd, 1);
            return 0;
        }
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
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
    int initX = CW_USEDEFAULT, initY = CW_USEDEFAULT, initW = 440, initH = 880;
    const char* rawPos = GetExtState ? GetExtState("REALSLAB", "window_pos") : nullptr;
    if (rawPos && *rawPos) {
        int rx = 0, ry = 0, rw = 0, rh = 0;
        if (std::sscanf(rawPos, "%d,%d,%d,%d", &rx, &ry, &rw, &rh) == 4 && rw >= 180 && rh >= 200) {
            POINT pt{ rx + 20, ry + 20 };
            HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONULL);
            if (hMon) {
                initX = rx; initY = ry; initW = rw; initH = rh;
                g_floatingRect = { rx, ry, rx + rw, ry + rh };
            }
        }
    }

    g_hwnd = CreateWindowExW(WS_EX_TOOLWINDOW, kWndClass, L"Reals Lab", WS_OVERLAPPEDWINDOW,
                             initX, initY, initW, initH, GetMainHwnd(), nullptr,
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

    // Check if it should be docked on startup
    const char* rawDock = GetExtState ? GetExtState("REALSLAB", "docked") : nullptr;
    if (rawDock && std::string_view(rawDock) == "1" && DockWindowAddEx) {
        DockWindowAddEx(g_hwnd, "Reals Lab", "REALSLAB_DOCK", true);
    }

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

            const char* rawTheme = GetExtState ? GetExtState("REALSLAB", "theme") : nullptr;
            std::string theme = (rawTheme && *rawTheme)
                ? std::string(rawTheme)
                : reals::config::Config::instance().getString("theme", "dark-studio");
            if (theme.empty())
                theme = "dark-studio";
            if (g_web) {
                const std::wstring script = L"window.themeManager && window.themeManager.applyTheme('" +
                                            toWide(theme) + L"', false);";
                g_web->executeScript(script);
            }
        } else {
            LOG_ERROR(kTag, "host: webview init failed — see earlier log lines");
        }
    });

    g_web->setWebMessageHandler([](const std::string& msg) {
        constexpr std::string_view kThemePrefix = "THEME_CHANGED:";
        if (msg.rfind(kThemePrefix, 0) == 0) {
            const std::string themeName = msg.substr(kThemePrefix.length());
            if (!themeName.empty()) {
                if (SetExtState)
                    SetExtState("REALSLAB", "theme", themeName.c_str(), true);
                reals::config::Config::instance().set("theme", themeName);
                if (g_chordHwnd && IsWindow(g_chordHwnd)) {
                    InvalidateRect(g_chordHwnd, nullptr, TRUE);
                }
            }
            return;
        }

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
        if (g_web->isReady()) {
            g_web->setVisible(true);
            const char* rawTheme = GetExtState ? GetExtState("REALSLAB", "theme") : nullptr;
            std::string theme = (rawTheme && *rawTheme)
                ? std::string(rawTheme)
                : reals::config::Config::instance().getString("theme", "dark-studio");
            if (theme.empty())
                theme = "dark-studio";
            const std::wstring script = L"window.themeManager && window.themeManager.applyTheme('" +
                                        toWide(theme) + L"', false);";
            g_web->executeScript(script);
        }
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

int g_cmdAnalyzeItem = 0;
int g_cmdSeparateItem = 0;
int g_cmdChordsItem = 0;
int g_cmdDenoiseItem = 0;

void CustomMenuHook(const char* menuidstr, void* menu, const int flag) {
    if (!menuidstr || !menu) return;
    if (std::strcmp(menuidstr, "Media item context") == 0) {
        auto* hMenu = reinterpret_cast<HMENU>(menu);
        if (flag == 0) {
            HMENU hSub = CreatePopupMenu();
            AppendMenuW(hSub, MF_STRING, static_cast<UINT_PTR>(g_cmdAnalyzeItem), L"Gửi vào Audio Lab");
            AppendMenuW(hSub, MF_STRING, static_cast<UINT_PTR>(g_cmdSeparateItem), L"Tách Stems (Vocals, Drums, Bass, Other)");
            AppendMenuW(hSub, MF_STRING, static_cast<UINT_PTR>(g_cmdChordsItem), L"Dò Hợp Âm (Chord Track)");
            AppendMenuW(hSub, MF_STRING, static_cast<UINT_PTR>(g_cmdDenoiseItem), L"Lọc Noise (Denoise)");
            InsertMenuW(hMenu, 0, MF_BYPOSITION | MF_POPUP, reinterpret_cast<UINT_PTR>(hSub), L"🎵 Reals Lab");
            InsertMenuW(hMenu, 1, MF_BYPOSITION | MF_SEPARATOR, 0, nullptr);
        }
    }
}

void handleItemContextAction(int command) {
    if (!GetSelectedMediaItem || !GetActiveTake || !GetMediaItemTake_Source) return;
    MediaItem* item = GetSelectedMediaItem(nullptr, 0);
    if (!item) {
        if (g_web) {
            nlohmann::json j;
            j["event"] = "toast";
            j["data"] = {{"text", reals::i18n::tr("toast.noItemSelected")}};
            g_web->postJson(j.dump());
        }
        return;
    }
    MediaItem_Take* take = GetActiveTake(item);
    if (!take) return;
    PCM_source* src = GetMediaItemTake_Source(take);
    if (!src || !src->GetFileName()) return;
    std::string filePath = src->GetFileName();

    double itemPos = 0.0;
    double itemLen = 0.0;
    if (GetMediaItemInfo_Value) {
        itemPos = GetMediaItemInfo_Value(item, "D_POSITION");
        itemLen = GetMediaItemInfo_Value(item, "D_LENGTH");
    }

    if (!g_visible) {
        toggleOnce();
    }

    std::string actionName = "analyze";
    if (command == g_cmdSeparateItem) actionName = "stem";
    else if (command == g_cmdChordsItem) {
        actionName = "keychord";
        showChordDocker(true);
    }
    else if (command == g_cmdDenoiseItem) actionName = "denoise";

    nlohmann::json evt;
    evt["event"] = "lab.itemSelected";
    evt["data"] = {
        {"path", filePath},
        {"action", actionName},
        {"itemPosition", itemPos},
        {"itemLength", itemLen}
    };
    if (g_web) {
        g_web->postJson(evt.dump());
    }
}

int commandHook(KbdSectionInfo* /*sec*/, const int command, const int /*val*/, const int /*val2*/,
                const int /*relmode*/, const HWND /*hwnd*/) {
    if (command == g_cmdId && g_cmdId != 0) {
        toggleOnce();
        return true;
    }
    if (command == g_cmdToggleChord && g_cmdToggleChord != 0) {
        showChordDocker(!g_chordState.isVisible);
        return true;
    }
    if (command == g_cmdAnalyzeItem || command == g_cmdSeparateItem ||
        command == g_cmdChordsItem || command == g_cmdDenoiseItem) {
        handleItemContextAction(command);
        return true;
    }
    if (isTransportCommand(command)) {
        // Spacebar preview cycle: transport commands issued by Reals Lab's
        // own start/stop path must keep the sample preview running (that is
        // the entire feature — preview + DAW playing together). Consume the
        // guard flag and leave the preview untouched.
        if (g_previewCycleGuard.load(std::memory_order_acquire)) {
            pushAudioState();
            return false;
        }
        if (g_hostPreview.registrationActive.load(std::memory_order_acquire)) {
            g_hostPreview.stopAndClear();
        }
        if (reals::audio::Engine::instance().isPlaying()) {
            reals::audio::Engine::instance().stop();
        }
        pushAudioState();
    }
    return false;
}

int commandHookV1(const int command, const int /*val*/, const int /*valhw*/, const int /*relmode*/,
                  const HWND /*hwnd*/) {
    if (command == g_cmdId && g_cmdId != 0) {
        toggleOnce();
        return 1;
    }
    if (command == g_cmdAnalyzeItem || command == g_cmdSeparateItem ||
        command == g_cmdChordsItem || command == g_cmdDenoiseItem) {
        handleItemContextAction(command);
        return 1;
    }
    if (isTransportCommand(command)) {
        // Same preview-cycle guard as commandHook (V1 fallback path).
        if (g_previewCycleGuard.load(std::memory_order_acquire)) {
            pushAudioState();
            return 0;
        }
        if (g_hostPreview.registrationActive.load(std::memory_order_acquire)) {
            g_hostPreview.stopAndClear();
        }
        if (reals::audio::Engine::instance().isPlaying()) {
            reals::audio::Engine::instance().stop();
        }
        pushAudioState();
    }
    return 0;
}

static int realsTranslateAccel(MSG* /*msg*/, accelerator_register_t* /*ctx*/) {
    if (!g_hwnd || !IsWindow(g_hwnd)) return 0;
    HWND focus = GetFocus();
    if (focus && (focus == g_hwnd || IsChild(g_hwnd, focus))) {
        // When Reals Lab or its child (WebView2) has keyboard focus, return -1
        // to route keystrokes directly into our WebView2 DOM.
        // Inside app.js, Spacebar is intentionally wired to trigger 'reaper.playToggle'
        // (DAW Transport: Play/stop) so producers can start/stop project playback
        // seamlessly while browsing samples, without losing window focus.
        return -1;
    }
    return 0;
}
static accelerator_register_t g_accelReg = { realsTranslateAccel, true, nullptr };

} // namespace

extern "C" REAPER_PLUGIN_DLL_EXPORT int REAPER_PLUGIN_ENTRYPOINT(REAPER_PLUGIN_HINSTANCE hInstance,
                                                                 reaper_plugin_info_t* rec) {
    g_hInstance = hInstance;

    reals::platform::ensureDir(reals::platform::dataDir());
    reals::util::Log::init(reals::platform::joinPath(reals::platform::dataDir(), "reals_ext.log"));
#ifdef NDEBUG
    LOG_INFO(kTag, "entry: begin (BUILD=Release)");
#else
    LOG_INFO(kTag, "entry: begin (BUILD=Debug — NOT fit for realtime DSP, do not ship)");
#endif

    if (!rec) {
        // Full teardown: unregister everything registered at load, release
        // bridge (joins lab workers) and webview (removes event handlers).
        plugin_register("-accelerator", &g_accelReg);
        plugin_register("-timer", reinterpret_cast<void*>(timerHook));
        plugin_register("-hookcommand2", reinterpret_cast<void*>(commandHook));
        plugin_register("-hookcommand", reinterpret_cast<void*>(commandHookV1));
        plugin_register("-hookcustommenu", reinterpret_cast<void*>(CustomMenuHook));
        gaccel_register_t ga{};
        ga.accel.cmd = static_cast<decltype(ga.accel.cmd)>(g_cmdId);
        ga.desc = kCommandName;
        plugin_register("-gaccel", &ga);
        if (g_cmdId > 0)
            plugin_register("-command_id", const_cast<char*>(kCommandId));
        g_cmdId = 0;
        if (g_cmdAnalyzeItem > 0) {
            gaccel_register_t gaItem{};
            gaItem.accel.cmd = static_cast<decltype(gaItem.accel.cmd)>(g_cmdAnalyzeItem);
            gaItem.desc = "Reals Lab: Send selected item to Audio Lab";
            plugin_register("-gaccel", &gaItem);
            plugin_register("-command_id", const_cast<char*>("REALSLAB_ANALYZE_ITEM"));
            g_cmdAnalyzeItem = 0;
        }
        if (g_cmdSeparateItem > 0) {
            gaccel_register_t gaItem{};
            gaItem.accel.cmd = static_cast<decltype(gaItem.accel.cmd)>(g_cmdSeparateItem);
            gaItem.desc = "Reals Lab: Separate stems for selected item";
            plugin_register("-gaccel", &gaItem);
            plugin_register("-command_id", const_cast<char*>("REALSLAB_SEPARATE_ITEM"));
            g_cmdSeparateItem = 0;
        }
        if (g_cmdChordsItem > 0) {
            gaccel_register_t gaItem{};
            gaItem.accel.cmd = static_cast<decltype(gaItem.accel.cmd)>(g_cmdChordsItem);
            gaItem.desc = "Reals Lab: Detect chords and create Chord Track for selected item";
            plugin_register("-gaccel", &gaItem);
            plugin_register("-command_id", const_cast<char*>("REALSLAB_CHORDS_ITEM"));
            g_cmdChordsItem = 0;
        }
        if (g_cmdDenoiseItem > 0) {
            gaccel_register_t gaItem{};
            gaItem.accel.cmd = static_cast<decltype(gaItem.accel.cmd)>(g_cmdDenoiseItem);
            gaItem.desc = "Reals Lab: Denoise selected item";
            plugin_register("-gaccel", &gaItem);
            plugin_register("-command_id", const_cast<char*>("REALSLAB_DENOISE_ITEM"));
            g_cmdDenoiseItem = 0;
        }
        if (g_cmdToggleChord > 0) {
            gaccel_register_t gaItem{};
            gaItem.accel.cmd = static_cast<decltype(gaItem.accel.cmd)>(g_cmdToggleChord);
            gaItem.desc = "Reals Lab: Toggle Chord Track Docker (Top)";
            plugin_register("-gaccel", &gaItem);
            plugin_register("-command_id", const_cast<char*>("REALSLAB_TOGGLE_CHORD"));
            g_cmdToggleChord = 0;
        }
        if (g_chordHwnd) {
            if (DockWindowRemove) {
                DockWindowRemove(g_chordHwnd);
            }
            DestroyWindow(g_chordHwnd);
            g_chordHwnd = nullptr;
        }
        UnregisterClassW(L"RealSChordDockerClass", g_hInstance);
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
        g_audioHook.cleanup();
        reals::audio::Engine::instance().setTimeStretchProcessor(nullptr);
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
        reals::ext::agent::init(rec->GetFunc);

        if (GetAudioDeviceInfo) {
            char srateBuf[64] = {0};
            GetAudioDeviceInfo("SRATE", srateBuf, sizeof(srateBuf));
            int devSr = std::atoi(srateBuf);
            if (devSr > 0) {
                reals::audio::Engine::instance().setTargetSampleRate(devSr);
                LOG_INFO(kTag, "entry: seeded host sample rate devSr=" + std::to_string(devSr));
            }
        }

        if (Audio_RegHardwareHook) {
            memset(&g_audioHook.hook, 0, sizeof(g_audioHook.hook));
            g_audioHook.hook.OnAudioBuffer = ReaperOnAudioBuffer;
            int hookRes = Audio_RegHardwareHook(true, &g_audioHook.hook);
            g_audioHook.isRegistered = (hookRes != 0);
            LOG_INFO(kTag, "entry: Audio_RegHardwareHook registered res=" + std::to_string(hookRes));
            reals::audio::Engine::instance().init(!g_audioHook.isRegistered);
        } else {
            LOG_ERROR(kTag, "entry: Audio_RegHardwareHook API not available");
            reals::audio::Engine::instance().init(true);
        }

        if (ReaperGetPitchShiftAPI) {
            auto reaperShifter = std::make_shared<ReaperPitchShiftProcessor>();
            reals::audio::Engine::instance().setTimeStretchProcessor(reaperShifter);
            LOG_INFO(kTag, "entry: Registered REAPER native élastique 3 Pro pitch shifter with audio Engine");
        }

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
        plugin_register("accelerator", &g_accelReg);

        g_cmdAnalyzeItem = plugin_register("command_id", const_cast<char*>("REALSLAB_ANALYZE_ITEM"));
        if (g_cmdAnalyzeItem > 0) {
            gaccel_register_t gaItem{};
            gaItem.accel.cmd = static_cast<decltype(gaItem.accel.cmd)>(g_cmdAnalyzeItem);
            gaItem.desc = "Reals Lab: Send selected item to Audio Lab";
            plugin_register("gaccel", &gaItem);
        }
        g_cmdSeparateItem = plugin_register("command_id", const_cast<char*>("REALSLAB_SEPARATE_ITEM"));
        if (g_cmdSeparateItem > 0) {
            gaccel_register_t gaItem{};
            gaItem.accel.cmd = static_cast<decltype(gaItem.accel.cmd)>(g_cmdSeparateItem);
            gaItem.desc = "Reals Lab: Separate stems for selected item";
            plugin_register("gaccel", &gaItem);
        }
        g_cmdChordsItem = plugin_register("command_id", const_cast<char*>("REALSLAB_CHORDS_ITEM"));
        if (g_cmdChordsItem > 0) {
            gaccel_register_t gaItem{};
            gaItem.accel.cmd = static_cast<decltype(gaItem.accel.cmd)>(g_cmdChordsItem);
            gaItem.desc = "Reals Lab: Detect chords and create Chord Track for selected item";
            plugin_register("gaccel", &gaItem);
        }
        g_cmdDenoiseItem = plugin_register("command_id", const_cast<char*>("REALSLAB_DENOISE_ITEM"));
        if (g_cmdDenoiseItem > 0) {
            gaccel_register_t gaItem{};
            gaItem.accel.cmd = static_cast<decltype(gaItem.accel.cmd)>(g_cmdDenoiseItem);
            gaItem.desc = "Reals Lab: Denoise selected item";
            plugin_register("gaccel", &gaItem);
        }
        g_cmdToggleChord = plugin_register("command_id", const_cast<char*>("REALSLAB_TOGGLE_CHORD"));
        if (g_cmdToggleChord > 0) {
            gaccel_register_t gaChord{};
            gaChord.accel.cmd = static_cast<decltype(gaChord.accel.cmd)>(g_cmdToggleChord);
            gaChord.desc = "Reals Lab: Toggle Chord Track Docker (Top)";
            plugin_register("gaccel", &gaChord);
        }
        plugin_register("hookcustommenu", reinterpret_cast<void*>(CustomMenuHook));

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
