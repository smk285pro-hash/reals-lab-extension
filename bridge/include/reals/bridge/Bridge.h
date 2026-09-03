#pragma once

// JSON-RPC style dispatcher between the web UI and native capabilities.
// Request:  {"id":N,"cmd":"browser.list","args":{...}}
// Response: {"id":N,"ok":true,"data":...} | {"id":N,"ok":false,"error":"..."}
// Events (push): {"event":"audio.state","data":{...}}
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace reals::bridge {

// Host transport state for DAW playhead synchronization (REAPER GetPlayState/GetPlayPosition/TimeMap2_timeToBeats)
struct HostTransport {
    int playState = 0;          // 0 = stopped, 1 = playing, 2 = paused, 5 = recording
    double playPosition = 0.0;  // position in seconds
    double fullBeats = 0.0;     // continuous beat count from project start
    int measure = 0;            // measure index (0-indexed)
    int beatsPerMeasure = 4;    // time signature numerator (cml)
    int denom = 4;              // time signature denominator
    double bpm = 120.0;         // project tempo
    double blockLatencySeconds = 0.0; // host audio block anticipation latency (pos2 - pos1)

    [[nodiscard]] bool isPlaying() const { return (playState & 1) != 0; }
};

// Shell-provided actions that touch the host (REAPER or desktop app).
struct IHostActions {
    virtual ~IHostActions() = default;
    virtual void insertMedia(const std::string& path) = 0;
    // Extended insert with explicit playrate and pitch shift for tempo/key sync (1.0 = no stretch)
    virtual void insertMedia(const std::string& path, double playrate) {
        insertMedia(path, playrate, 0.0);
    }
    virtual void insertMedia(const std::string& path, double playrate, double pitchSemitones) {
        (void)playrate;
        (void)pitchSemitones;
        insertMedia(path);
    }
    virtual void revealInExplorer(const std::string& path) = 0;
    virtual void sendToLab(const std::string& path, const std::string& job) = 0;
    virtual void hideWindow() = 0;
    virtual void minimizeWindow() = 0;
    virtual void toggleMaximize() = 0;
    virtual void startDragWindow() = 0;
    virtual void startResizeWindow(const std::string& edge) = 0;
    virtual void toggleDock() {}
    virtual bool isDocked() const { return false; }
    // OLE drag of a file into REAPER (or any drop target). Blocks until drop/cancel.
    virtual void beginDrag(const std::string& path) = 0;
    // Queue a pending playrate for the next inserted media (used for drag sync)
    virtual void queueSyncPlayrate(const std::string& path, double playrate) {
        queueSyncPlayrate(path, playrate, 0.0, "");
    }
    virtual void queueSyncPlayrate(const std::string& path, double playrate, double pitchSemitones, const std::string& originalPath = "") {
        (void)path;
        (void)playrate;
        (void)pitchSemitones;
        (void)originalPath;
    }
    // Host project tempo in BPM, or 0 if unknown (standalone app).
    virtual double projectTempo() const = 0;
    // Toggle play/stop in host DAW transport.
    virtual void togglePlay() = 0;
    // Query current host DAW transport (play state, playhead position in seconds & beats, tempo)
    virtual HostTransport hostTransport() const { return {}; }

    // Native host audio preview (e.g. REAPER Audio Hook routed through Master Hardware Output)
    virtual bool playHostPreview(const std::string& path, bool loop, double startPosSeconds, double volume, double playrate, double pitchSemitones, double sampleBpm = 120.0, double loopBeats = 16.0, uint64_t nominalLoopFrames = 0) {
        (void)path; (void)loop; (void)startPosSeconds; (void)volume; (void)playrate; (void)pitchSemitones; (void)sampleBpm; (void)loopBeats; (void)nominalLoopFrames;
        return false;
    }
    virtual void stopHostPreview() {}
    virtual bool isHostPreviewPlaying() const { return false; }
    virtual double hostPreviewPositionFraction() const { return 0.0; }
    virtual float hostPreviewPeak() const { return 0.0f; }
    virtual float hostPreviewRms() const { return 0.0f; }
    virtual void setHostPreviewVolume(double vol) { (void)vol; }
    virtual void setHostPreviewPosition(double posSeconds) { (void)posSeconds; }
    virtual void setHostPreviewPositionFraction(double frac) { (void)frac; }
    virtual void setHostPreviewLoop(bool loop) { (void)loop; }
    virtual void setHostPreviewTimeRatio(double ratio) { (void)ratio; }
    virtual void setHostPreviewPitchSemitones(double semitones) { (void)semitones; }
    virtual double hostPreviewTimeRatio() const { return 1.0; }
    virtual double hostPreviewPitchSemitones() const { return 0.0; }
};

class Bridge {
public:
    explicit Bridge(IHostActions* actions, std::string browserStorePath = {});
    ~Bridge();

    // Wire core singletons (Engine, BrowserModel, Config). Call once at startup.
    void init();

    // Handle one request; returns the JSON response (empty for none).
    std::string handle(const std::string& requestJson);

    // True while either playback path produces audio: the core Engine
    // (standalone fallback) or the native host preview (REAPER PlayPreviewEx).
    // Shells use this to decide whether to poll/push audio.state — checking
    // Engine::isPlaying() alone misses the native preview path.
    [[nodiscard]] bool isAudioActive() const;

    // Serialized snapshot of the audio state (posted periodically by the shell).
    std::string audioStateJson() const;

    // Drain events queued by background lab jobs (called from the UI thread).
    std::vector<std::string> drainEvents();

    // Check if the DAW host transport cursor has moved/seeked or looped,
    // and if so, re-align the phase of the active preview (audio + waveform).
    // Returns true if a phase re-alignment was triggered.
    bool updatePhaseSnapFromHostTransport();

private:
    IHostActions* m_actions;
    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

} // namespace reals::bridge
