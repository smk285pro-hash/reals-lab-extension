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
#include <unordered_set>
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
#define REAPERAPI_WANT_GetPlayPosition2Ex
#define REAPERAPI_WANT_GetCursorPosition
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
#define REAPERAPI_IMPLEMENT
#include <reaper_plugin_functions.h>

#include <miniaudio.h>
#include "reals/audio/DragExporter.h"
#include "reals/audio/Engine.h"
#include "reals/audio/ITimeStretchProcessor.h"
#include "reals/bridge/Bridge.h"
#include "reals/config/Config.h"
#include "reals/embedded/EmbeddedAssets.h"
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
class DspPreviewSource final : public PCM_source {
public:
    DspPreviewSource(PCM_source* underlyingSrc, double timeRatio, double pitchSemitones)
        : m_src(underlyingSrc), m_timeRatio(timeRatio), m_pitchSemitones(pitchSemitones) {
        if (m_src) {
            m_srate = m_src->GetSampleRate();
            m_srcChannels = m_src->GetNumChannels();
            if (m_srate <= 0.0) m_srate = 44100.0;
            if (m_srcChannels <= 0) m_srcChannels = 2;
            // REAPER preview output is always stereo (block->nch == 2).
            // The shifter must also be stereo so its output stride matches
            // the interleaved stereo buffer that PlayPreviewEx provides.
            // Mono source audio is demuxed to L+R before feeding the shifter.
            m_nch = std::max(2, m_srcChannels);
        }
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
        // Wait briefly for any in-flight GetSamples call on REAPER audio thread to exit
        for (int i = 0; i < 50 && m_activeReaders.load(std::memory_order_acquire) > 0; ++i) {
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
        const double rawLen = m_src->GetLength();
        const double r = m_timeRatio.load(std::memory_order_relaxed);
        const double fullOutLen = r > 0.01 ? (rawLen / r) : rawLen;
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
                LOG_INFO(kTag, "DspPreviewSource: re-configured shifter srate from " +
                               std::to_string(m_srate) + " → " + std::to_string(m_operatingRate) +
                               " (project rate)");
            }
        }

        // Apply updated parameters if changed from UI thread
        if (m_paramsDirty.exchange(false, std::memory_order_acq_rel)) {
            applyParams();
        }

        const double currentRatio = m_timeRatio.load(std::memory_order_relaxed);
        const double currentPitch = m_pitchSemitones.load(std::memory_order_relaxed);
        const int requestedLength = block->length;
        const int outChannels = block->nch > 0 ? block->nch : m_nch;

        // ── Bit-perfect bypass: ratio ≈ 1.0 and pitch ≈ 0.0 ──
        // Read directly from underlying source without élastique processing.
        // Handles mono→stereo or multichannel routing cleanly without buffer overrun.
        const bool isBypass = (std::abs(currentRatio - 1.0) <= 0.003 &&
                               std::abs(currentPitch) <= 0.02);
        if (isBypass) {
            if (m_srcChannels == outChannels) {
                // Channels match — direct passthrough, zero overhead
                m_src->GetSamples(block);
                m_rawTimePos = block->time_s + static_cast<double>(block->samples_out) / blockRate;
                m_lastTimelinePos = block->time_s + static_cast<double>(block->samples_out) / blockRate;
                if (block->samples_out == 0) m_streamFinished.store(true, std::memory_order_relaxed);
                return;
            }
            if (m_rawReadBuf.size() < static_cast<size_t>(requestedLength * m_srcChannels)) {
                m_rawReadBuf.resize(static_cast<size_t>(requestedLength * m_srcChannels));
            }
            PCM_source_transfer_t rawTransfer{};
            rawTransfer.time_s = block->time_s;
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
            m_rawTimePos = block->time_s + static_cast<double>(got) / blockRate;
            m_lastTimelinePos = block->time_s + static_cast<double>(got) / blockRate;
            if (got == 0) m_streamFinished.store(true, std::memory_order_relaxed);
            return;
        }

        // ── DSP path: route through IReaperPitchShift (élastique 3 Pro) ──
        if (!m_shifter) {
            block->samples_out = 0;
            return;
        }

        // Diagnostic: log first DSP GetSamples call
        if (m_diagCounter < 3) {
            LOG_INFO(kTag, "DspPreviewSource::GetSamples DSP #" + std::to_string(m_diagCounter) +
                           ": block->time_s=" + std::to_string(block->time_s) +
                           " block->length=" + std::to_string(requestedLength) +
                           " block->nch=" + std::to_string(block->nch) +
                           " block->samplerate=" + std::to_string(block->samplerate) +
                           " outChannels=" + std::to_string(outChannels) +
                           " ratio=" + std::to_string(currentRatio) +
                           " pitch=" + std::to_string(currentPitch) +
                           " m_rawTimePos=" + std::to_string(m_rawTimePos) +
                           " m_nch=" + std::to_string(m_nch) +
                           " m_srcChannels=" + std::to_string(m_srcChannels));
            ++m_diagCounter;
        }

        // Detect seek / transport discontinuity
        const double expectedTime = m_lastTimelinePos;
        constexpr double kSeekThresholdSeconds = 0.02; // 20ms tolerance
        if (m_lastTimelinePos < 0.0 || std::abs(block->time_s - expectedTime) > kSeekThresholdSeconds) {
            m_shifter->Reset();
            m_rawTimePos = block->time_s * currentRatio;
            m_eofReached = false;
            m_streamFinished.store(false, std::memory_order_relaxed);
        }

        int totalFramesRendered = 0;
        constexpr int kChunkFrames = 2048;

        if (m_rawReadBuf.size() < static_cast<size_t>(kChunkFrames * m_srcChannels)) {
            m_rawReadBuf.resize(static_cast<size_t>(kChunkFrames * m_srcChannels));
        }
        const bool needsDemux = (m_srcChannels < m_nch);
        std::vector<ReaSample>& demuxBuf = m_demuxBuf;
        if (needsDemux && demuxBuf.size() < static_cast<size_t>(kChunkFrames * m_nch)) {
            demuxBuf.resize(static_cast<size_t>(kChunkFrames * m_nch));
        }

        // Dedicated scratch buffer if output channels != 2 (prevents stride/buffer overflow)
        if (outChannels != 2 && m_shifterOutBuf.size() < static_cast<size_t>(requestedLength * m_nch)) {
            m_shifterOutBuf.resize(static_cast<size_t>(requestedLength * m_nch));
        }

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

            // Feed more raw audio from underlying source into pitch shifter
            PCM_source_transfer_t rawTransfer{};
            rawTransfer.time_s = m_rawTimePos;
            rawTransfer.samplerate = blockRate; // Read at project rate — PCM_source resamples internally
            rawTransfer.nch = m_srcChannels;    // Read in source's native channel count
            rawTransfer.length = kChunkFrames;
            rawTransfer.samples = m_rawReadBuf.data();
            rawTransfer.samples_out = 0;

            m_src->GetSamples(&rawTransfer);

            if (rawTransfer.samples_out > 0) {
                m_rawTimePos += static_cast<double>(rawTransfer.samples_out) / blockRate;
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

private:
    void initShifter() {
        if (ReaperGetPitchShiftAPI) {
            m_shifter = ReaperGetPitchShiftAPI(REAPER_PITCHSHIFT_API_VER);
            if (m_shifter) {
                m_shifter->set_srate(m_srate);
                m_shifter->set_nch(m_nch);
                m_shifter->SetQualityParameter(-1); // Project Default (élastique 3 Pro)
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
    double m_srate = 44100.0;
    double m_operatingRate = 0.0;  // Project rate captured from first GetSamples() call
    int m_srcChannels = 2;   // Original source channel count (may be 1 for mono)
    int m_nch = 2;           // Shifter/output channel count (always >= 2)
    double m_rawTimePos = 0.0;
    double m_lastTimelinePos = -1.0;
    bool m_eofReached = false;
    std::vector<ReaSample> m_rawReadBuf;
    std::vector<ReaSample> m_demuxBuf;      // Scratch buffer for mono→stereo demuxing
    std::vector<ReaSample> m_shifterOutBuf; // Scratch buffer for non-stereo output channel mapping
    int m_diagCounter = 0;                  // Diagnostic: limit log spam
};

struct ReaperHostPreviewState {
    preview_register_t reg{};
    DspPreviewSource* dspWrapper = nullptr;
    std::string currentPath;
    double durationSeconds = 0.0;
    std::atomic<bool> isPlaying{false};
    bool csInitialized = false;

    void initCS() {
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
        if (!csInitialized) return;

        // 1. Tell REAPER to stop playing OUTSIDE the critical section.
        // Calling StopPreview outside reg.cs prevents ABBA deadlocks where REAPER's
        // audio preview thread is blocked on reg.cs while StopPreview is waiting for it!
        if (isPlaying.exchange(false, std::memory_order_acq_rel)) {
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

                int qualityParam = -1; // -1 = REAPER Project Default (Exact algorithm REAPER uses natively)
                if (EnumPitchShiftModes) {
                    for (int m = 0; ; ++m) {
                        const char* modeName = nullptr;
                        if (!EnumPitchShiftModes(m, &modeName)) break;
                        if (!modeName) continue;
                        const std::string sMode = modeName;
                        if (EnumPitchShiftSubModes) {
                            for (int s = 0; s < 6; ++s) {
                                const char* subName = EnumPitchShiftSubModes(m, s);
                                if (subName) {
                                    LOG_INFO(kTag, "PitchShift mode " + std::to_string(m) + " (" + sMode + ") submode " + std::to_string(s) + ": " + subName);
                                }
                            }
                        }
                    }
                }
                m_shifter->SetQualityParameter(qualityParam);
                applyParams();
                LOG_INFO(kTag, "IReaperPitchShift initialized (qualityParam=" + std::to_string(qualityParam) + ", project default)");
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
    if (g_hostPreview.isPlaying.load(std::memory_order_relaxed)) {
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

    reals::bridge::HostTransport hostTransport() const override {
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
        g_hostPreview.dspWrapper = dsp;
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
        g_hostPreview.reg.src = dsp;
        g_hostPreview.durationSeconds = dsp->GetLength();
        g_hostPreview.reg.curpos = std::max(0.0, startPosSeconds);
        g_hostPreview.reg.loop = loop;
        g_hostPreview.reg.volume = std::clamp(volume, 0.0, 2.0);
        g_hostPreview.reg.m_out_chan = 0; // Standard REAPER preview output channel (Monitoring FX compliant)
        g_hostPreview.reg.preview_track = nullptr;
        g_hostPreview.currentPath = path;

        const int res = PlayPreviewEx(&g_hostPreview.reg, 1, -1.0);
        if (res != 0) {
            g_hostPreview.isPlaying.store(true, std::memory_order_relaxed);
        }
#ifdef _WIN32
        LeaveCriticalSection(&g_hostPreview.reg.cs);
#else
        pthread_mutex_unlock(&g_hostPreview.reg.mutex);
#endif

        LOG_INFO(kTag, "playHostPreview: started path=" + path + " dur=" + std::to_string(g_hostPreview.durationSeconds) + " res=" + std::to_string(res));
        return res != 0;
    }

    void stopHostPreview() override {
        g_hostPreview.stopAndClear();
        LOG_INFO(kTag, "stopHostPreview: stopped");
    }

    bool isHostPreviewPlaying() const override {
        if (!g_hostPreview.isPlaying.load(std::memory_order_relaxed)) {
            return false;
        }
        if (g_hostPreview.dspWrapper && g_hostPreview.dspWrapper->isStreamFinished()) {
            g_hostPreview.isPlaying.store(false, std::memory_order_relaxed);
            return false;
        }
        return true;
    }

    double hostPreviewPositionFraction() const override {
        if (!g_hostPreview.isPlaying.load(std::memory_order_relaxed) || g_hostPreview.durationSeconds <= 0.0) {
            return 0.0;
        }
        if (g_hostPreview.dspWrapper && g_hostPreview.dspWrapper->isStreamFinished()) {
            g_hostPreview.isPlaying.store(false, std::memory_order_relaxed);
            return 1.0;
        }
#ifdef _WIN32
        EnterCriticalSection(&g_hostPreview.reg.cs);
        const double pos = g_hostPreview.reg.curpos;
        LeaveCriticalSection(&g_hostPreview.reg.cs);
#else
        pthread_mutex_lock(&g_hostPreview.reg.mutex);
        const double pos = g_hostPreview.reg.curpos;
        pthread_mutex_unlock(&g_hostPreview.reg.mutex);
#endif
        if (pos >= g_hostPreview.durationSeconds && !g_hostPreview.reg.loop) {
            g_hostPreview.isPlaying.store(false, std::memory_order_relaxed);
        }
        return std::clamp(pos / g_hostPreview.durationSeconds, 0.0, 1.0);
    }

    float hostPreviewPeak() const override {
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

    void setHostPreviewPositionFraction(double frac) override {
        if (!g_hostPreview.isPlaying.load(std::memory_order_relaxed) || g_hostPreview.durationSeconds <= 0.0) return;
        const double pos = std::clamp(frac, 0.0, 1.0) * g_hostPreview.durationSeconds;
        setHostPreviewPosition(pos);
    }

    void setHostPreviewLoop(bool loop) override {
        if (!g_hostPreview.isPlaying.load(std::memory_order_relaxed)) return;
        if (g_hostPreview.dspWrapper) {
            g_hostPreview.dspWrapper->setLoopActive(loop);
        }
#ifdef _WIN32
        EnterCriticalSection(&g_hostPreview.reg.cs);
        g_hostPreview.reg.loop = loop;
        LeaveCriticalSection(&g_hostPreview.reg.cs);
#else
        pthread_mutex_lock(&g_hostPreview.reg.mutex);
        g_hostPreview.reg.loop = loop;
        pthread_mutex_unlock(&g_hostPreview.reg.mutex);
#endif
    }

    void setHostPreviewTimeRatio(double ratio) override {
        if (g_hostPreview.dspWrapper) {
            g_hostPreview.dspWrapper->setTimeRatio(ratio);
        }
    }

    void setHostPreviewPitchSemitones(double semitones) override {
        if (g_hostPreview.dspWrapper) {
            g_hostPreview.dspWrapper->setPitchSemitones(semitones);
        }
    }

    double hostPreviewTimeRatio() const override {
        if (g_hostPreview.dspWrapper) {
            return g_hostPreview.dspWrapper->getTimeRatio();
        }
        return 1.0;
    }

    double hostPreviewPitchSemitones() const override {
        if (g_hostPreview.dspWrapper) {
            return g_hostPreview.dspWrapper->getPitchSemitones();
        }
        return 0.0;
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

int commandHook(KbdSectionInfo* /*sec*/, const int command, const int /*val*/, const int /*val2*/,
                const int /*relmode*/, const HWND /*hwnd*/) {
    if (command == g_cmdId && g_cmdId != 0) {
        toggleOnce();
        return true;
    }
    if (isTransportCommand(command)) {
        if (g_hostPreview.isPlaying.load(std::memory_order_relaxed)) {
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
    if (isTransportCommand(command)) {
        if (g_hostPreview.isPlaying.load(std::memory_order_relaxed)) {
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
    LOG_INFO(kTag, "entry: begin");

    if (!rec) {
        // Full teardown: unregister everything registered at load, release
        // bridge (joins lab workers) and webview (removes event handlers).
        plugin_register("-accelerator", &g_accelReg);
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
