#pragma once

#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>
#include <reals/bridge/Bridge.h>

namespace reals::test {

using json = nlohmann::json;

struct MediaInsertRecord {
    std::string path;
};

struct LabSendRecord {
    std::string path;
    std::string job;
};

// Snapshot of one playHostPreview() call — used by native-path phase-snap tests
// to assert that Bridge passed the correct startPosSeconds / loopBeats /
// nominalLoopFrames through to the REAPER preview API.
struct HostPreviewRecord {
    std::string path;
    bool loop = false;
    double startPosSeconds = 0.0;
    double volume = 0.0;
    double playrate = 1.0;
    double pitchSemitones = 0.0;
    double sampleBpm = 120.0;
    double loopBeats = 16.0;
    uint64_t nominalLoopFrames = 0;
};

// Full mock implementation of IHostActions for testing REAPER host interactions and Bridge RPC
class MockHostActions : public reals::bridge::IHostActions {
public:
    explicit MockHostActions(double initialTempo = 120.0)
        : m_projectTempo(initialTempo) {}

    void insertMedia(const std::string& path) override {
        insertMedia(path, 1.0, 0.0);
    }
    void insertMedia(const std::string& path, double playrate) override {
        insertMedia(path, playrate, 0.0);
    }
    void insertMedia(const std::string& path, double playrate, double pitchSemitones) override {
        std::lock_guard lock(m_mutex);
        m_insertedMedia.push_back({path});
        m_lastPlayrate = playrate;
        m_insertedPlayrates.push_back(playrate);
        (void)pitchSemitones;
    }
    void queueSyncPlayrate(const std::string& path, double playrate) override {
        queueSyncPlayrate(path, playrate, 0.0, "");
    }
    void queueSyncPlayrate(const std::string& path, double playrate, double pitchSemitones, const std::string& originalPath) override {
        std::lock_guard lock(m_mutex);
        m_queuedSyncPaths.push_back(path);
        m_queuedSyncRates.push_back(playrate);
        m_queuedSyncPitches.push_back(pitchSemitones);
        m_lastPlayrate = playrate;
        m_lastPitch = pitchSemitones;
        (void)originalPath;
    }

    void revealInExplorer(const std::string& path) override {
        std::lock_guard lock(m_mutex);
        m_revealedPaths.push_back(path);
    }

    void sendToLab(const std::string& path, const std::string& job) override {
        std::lock_guard lock(m_mutex);
        m_labSends.push_back({path, job});
    }

    void hideWindow() override {
        std::lock_guard lock(m_mutex);
        m_windowHidden = true;
    }

    void minimizeWindow() override {
        std::lock_guard lock(m_mutex);
        m_windowMinimized = true;
    }

    void toggleMaximize() override {
        std::lock_guard lock(m_mutex);
        m_windowMaximized = !m_windowMaximized;
    }

    void startDragWindow() override {
        std::lock_guard lock(m_mutex);
        m_dragStarted = true;
    }

    void startResizeWindow(const std::string& edge) override {
        std::lock_guard lock(m_mutex);
        m_resizedEdge = edge;
    }

    void toggleDock() override {
        std::lock_guard lock(m_mutex);
        m_docked = !m_docked;
    }

    [[nodiscard]] bool isDocked() const override {
        std::lock_guard lock(m_mutex);
        return m_docked;
    }

    void beginDrag(const std::string& path) override {
        std::lock_guard lock(m_mutex);
        m_draggedPaths.push_back(path);
    }

    [[nodiscard]] double projectTempo() const override {
        std::lock_guard lock(m_mutex);
        return m_projectTempo;
    }

    void setProjectTempo(double bpm) {
        std::lock_guard lock(m_mutex);
        m_projectTempo = bpm;
    }

    void togglePlay() override {
        std::lock_guard lock(m_mutex);
        m_hostPlayToggled = !m_hostPlayToggled;
    }

    bool hostPlayToggled() const {
        std::lock_guard lock(m_mutex);
        return m_hostPlayToggled;
    }

    void setHostPlayToggled(bool val) {
        std::lock_guard lock(m_mutex);
        m_hostPlayToggled = val;
    }

    // ── Native host preview (REAPER PlayPreviewEx path) ─────────────────────
    // Returns true by default so Bridge exercises the native path instead of
    // falling back to Engine::playFile. Tests inspect what was recorded.
    bool playHostPreview(const std::string& path, bool loop, double startPosSeconds, double volume,
                         double playrate, double pitchSemitones, double sampleBpm = 120.0,
                         double loopBeats = 16.0, uint64_t nominalLoopFrames = 0) override {
        std::lock_guard lock(m_mutex);
        HostPreviewRecord rec;
        rec.path = path;
        rec.loop = loop;
        rec.startPosSeconds = startPosSeconds;
        rec.volume = volume;
        rec.playrate = playrate;
        rec.pitchSemitones = pitchSemitones;
        rec.sampleBpm = sampleBpm;
        rec.loopBeats = loopBeats;
        rec.nominalLoopFrames = nominalLoopFrames;
        m_previewCalls.push_back(rec);
        // Native preview is opt-in: by default return false so existing Bridge
        // tests keep exercising the Engine/SoundTouch fallback path. Tests that
        // target the REAPER PlayPreviewEx path enable it explicitly.
        if (!m_nativePreviewEnabled) {
            return false;
        }
        m_previewPlaying = true;
        m_previewPath = path;
        m_previewLoop = loop;
        m_previewPosSeconds = startPosSeconds;
        m_previewTimeRatio = playrate;
        m_previewPitch = pitchSemitones;
        m_previewLoopBeats = loopBeats;
        m_previewNominalLoopFrames = nominalLoopFrames;
        // Simulated full-file duration in output space (tests can override).
        if (m_previewSimulatedDuration > 0.0) {
            m_previewDurationSeconds = m_previewSimulatedDuration;
        } else {
            m_previewDurationSeconds = (loopBeats > 0.0 && sampleBpm > 0.0)
                ? (loopBeats * 60.0 / sampleBpm)
                : 0.0;
        }
        return true;
    }

    void stopHostPreview() override {
        std::lock_guard lock(m_mutex);
        m_previewPlaying = false;
        m_previewPath.clear();
        m_previewPosSeconds = 0.0;
    }

    [[nodiscard]] bool isHostPreviewPlaying() const override {
        std::lock_guard lock(m_mutex);
        return m_previewPlaying;
    }

    [[nodiscard]] double hostPreviewPositionFraction() const override {
        std::lock_guard lock(m_mutex);
        if (!m_previewPlaying || m_previewDurationSeconds <= 0.0) return 0.0;
        return std::clamp(m_previewPosSeconds / m_previewDurationSeconds, 0.0, 1.0);
    }

    void setHostPreviewPositionFraction(double frac) override {
        std::lock_guard lock(m_mutex);
        m_previewFractionCalls.push_back(frac);
        m_previewPosSeconds = std::clamp(frac, 0.0, 1.0) * m_previewDurationSeconds;
    }

    void setHostPreviewPosition(double posSeconds) override {
        std::lock_guard lock(m_mutex);
        m_previewPositionCalls.push_back(posSeconds);
        m_previewPosSeconds = std::max(0.0, posSeconds);
    }

    void setHostPreviewTimeRatio(double ratio) override {
        std::lock_guard lock(m_mutex);
        m_previewTimeRatio = ratio;
    }

    void setHostPreviewPitchSemitones(double semitones) override {
        std::lock_guard lock(m_mutex);
        m_previewPitch = semitones;
    }

    [[nodiscard]] double hostPreviewTimeRatio() const override {
        std::lock_guard lock(m_mutex);
        return m_previewTimeRatio;
    }

    [[nodiscard]] double hostPreviewPitchSemitones() const override {
        std::lock_guard lock(m_mutex);
        return m_previewPitch;
    }

    // Test helpers ───────────────────────────────────────────────────────────
    [[nodiscard]] std::vector<HostPreviewRecord> previewCalls() const {
        std::lock_guard lock(m_mutex);
        return m_previewCalls;
    }
    [[nodiscard]] HostPreviewRecord lastPreviewCall() const {
        std::lock_guard lock(m_mutex);
        return m_previewCalls.empty() ? HostPreviewRecord{} : m_previewCalls.back();
    }
    [[nodiscard]] std::vector<double> previewFractionCalls() const {
        std::lock_guard lock(m_mutex);
        return m_previewFractionCalls;
    }
    [[nodiscard]] std::vector<double> previewPositionCalls() const {
        std::lock_guard lock(m_mutex);
        return m_previewPositionCalls;
    }
    [[nodiscard]] bool previewPlaying() const {
        std::lock_guard lock(m_mutex);
        return m_previewPlaying;
    }
    void setPreviewSimulatedDuration(double sec) {
        std::lock_guard lock(m_mutex);
        m_previewSimulatedDuration = sec;
    }
    // Opt this mock into simulating a successful REAPER PlayPreviewEx so the
    // Bridge takes the native preview path instead of the Engine fallback.
    void setNativePreviewEnabled(bool enabled) {
        std::lock_guard lock(m_mutex);
        m_nativePreviewEnabled = enabled;
    }
    void clearPreviewHistory() {
        std::lock_guard lock(m_mutex);
        m_previewCalls.clear();
        m_previewFractionCalls.clear();
        m_previewPlaying = false;
        m_previewPath.clear();
        m_previewPosSeconds = 0.0;
    }

    [[nodiscard]] std::vector<MediaInsertRecord> getInsertedMedia() const {
        std::lock_guard lock(m_mutex);
        return m_insertedMedia;
    }

    [[nodiscard]] std::vector<std::string> getRevealedPaths() const {
        std::lock_guard lock(m_mutex);
        return m_revealedPaths;
    }

    [[nodiscard]] std::vector<LabSendRecord> getLabSends() const {
        std::lock_guard lock(m_mutex);
        return m_labSends;
    }

    [[nodiscard]] std::vector<std::string> getDraggedPaths() const {
        std::lock_guard lock(m_mutex);
        return m_draggedPaths;
    }

    [[nodiscard]] std::string lastDraggedPath() const {
        std::lock_guard lock(m_mutex);
        return m_draggedPaths.empty() ? "" : m_draggedPaths.back();
    }

    void setHostTransport(const reals::bridge::HostTransport& t) {
        std::lock_guard lock(m_mutex);
        m_hostTransport = t;
        m_projectTempo = t.bpm;
    }

    void setHostTransport(int state, double pos, double fullBeats, double bpm = 120.0, int measure = 0, int beatsPerMeasure = 4, int denom = 4) {
        std::lock_guard lock(m_mutex);
        m_hostTransport.playState = state;
        m_hostTransport.playPosition = pos;
        m_hostTransport.fullBeats = fullBeats;
        m_hostTransport.bpm = bpm;
        m_hostTransport.measure = measure;
        m_hostTransport.beatsPerMeasure = beatsPerMeasure;
        m_hostTransport.denom = denom;
        m_projectTempo = bpm;
    }

    [[nodiscard]] reals::bridge::HostTransport hostTransport() const override {
        std::lock_guard lock(m_mutex);
        return m_hostTransport;
    }

    void clearHistory() {
        std::lock_guard lock(m_mutex);
        m_insertedMedia.clear();
        m_revealedPaths.clear();
        m_labSends.clear();
        m_draggedPaths.clear();
        m_extState.clear();
        m_extStatePersist.clear();
        m_previewCalls.clear();
        m_previewFractionCalls.clear();
        m_previewPlaying = false;
        m_previewPath.clear();
        m_previewPosSeconds = 0.0;
    }

    void setExtState(const std::string& section, const std::string& key, const std::string& val, bool persist = true) {
        std::lock_guard lock(m_mutex);
        m_extState[section + ":" + key] = val;
        m_extStatePersist[section + ":" + key] = persist;
    }

    [[nodiscard]] std::string getExtState(const std::string& section, const std::string& key) const {
        std::lock_guard lock(m_mutex);
        auto it = m_extState.find(section + ":" + key);
        if (it != m_extState.end()) return it->second;
        return "";
    }

    [[nodiscard]] bool hasExtState(const std::string& section, const std::string& key) const {
        std::lock_guard lock(m_mutex);
        return m_extState.find(section + ":" + key) != m_extState.end();
    }

    void deleteExtState(const std::string& section, const std::string& key, bool persist = true) {
        std::lock_guard lock(m_mutex);
        m_extState.erase(section + ":" + key);
        m_extStatePersist.erase(section + ":" + key);
        (void)persist;
    }

    [[nodiscard]] bool isExtStatePersisted(const std::string& section, const std::string& key) const {
        std::lock_guard lock(m_mutex);
        auto it = m_extStatePersist.find(section + ":" + key);
        if (it != m_extStatePersist.end()) return it->second;
        return false;
    }

private:
    mutable std::mutex m_mutex;
    double m_projectTempo = 120.0;
    reals::bridge::HostTransport m_hostTransport;
    bool m_docked = false;
    bool m_windowHidden = false;
    bool m_windowMinimized = false;
    bool m_windowMaximized = false;
    bool m_dragStarted = false;
    bool m_hostPlayToggled = false;
    std::string m_resizedEdge;
    std::vector<MediaInsertRecord> m_insertedMedia;
    std::vector<double> m_insertedPlayrates;
    double m_lastPlayrate = 1.0;
    double m_lastPitch = 0.0;
    std::vector<std::string> m_queuedSyncPaths;
    std::vector<double> m_queuedSyncRates;
    std::vector<double> m_queuedSyncPitches;
    std::vector<std::string> m_revealedPaths;
    std::vector<LabSendRecord> m_labSends;
    std::vector<std::string> m_draggedPaths;
    std::map<std::string, std::string> m_extState;
    std::map<std::string, bool> m_extStatePersist;
    // Native host preview state
    bool m_nativePreviewEnabled = false;
    std::vector<HostPreviewRecord> m_previewCalls;
    std::vector<double> m_previewFractionCalls;
    std::vector<double> m_previewPositionCalls;
    bool m_previewPlaying = false;
    std::string m_previewPath;
    bool m_previewLoop = false;
    double m_previewPosSeconds = 0.0;
    double m_previewTimeRatio = 1.0;
    double m_previewPitch = 0.0;
    double m_previewLoopBeats = 16.0;
    uint64_t m_previewNominalLoopFrames = 0;
    double m_previewDurationSeconds = 0.0;
    double m_previewSimulatedDuration = 0.0; // 0 = auto-derive from loopBeats/sampleBpm
public:
    double lastPlayrate() const { std::lock_guard lock(m_mutex); return m_lastPlayrate; }
    double lastQueuedPlayrate() const { std::lock_guard lock(m_mutex); return m_lastPlayrate; }
    double lastPitch() const { std::lock_guard lock(m_mutex); return m_lastPitch; }
    std::vector<double> insertedPlayrates() const { std::lock_guard lock(m_mutex); return m_insertedPlayrates; }
    std::vector<std::string> queuedSyncPaths() const { std::lock_guard lock(m_mutex); return m_queuedSyncPaths; }
    std::vector<double> queuedSyncRates() const { std::lock_guard lock(m_mutex); return m_queuedSyncRates; }
    std::vector<double> queuedSyncPitches() const { std::lock_guard lock(m_mutex); return m_queuedSyncPitches; }
};

// Bridge Test Harness wrapping Bridge and MockHostActions
class BridgeTestHarness {
public:
    explicit BridgeTestHarness(double initialDawTempo = 120.0)
        : m_host(std::make_unique<MockHostActions>(initialDawTempo)),
          m_bridge(std::make_unique<reals::bridge::Bridge>(m_host.get())) {
        m_bridge->init();
    }

    ~BridgeTestHarness() {
        if (m_bridge) {
            m_bridge->handle(R"({"cmd":"audio.stop","args":{}})");
        }
    }

    [[nodiscard]] MockHostActions& host() { return *m_host; }
    [[nodiscard]] reals::bridge::Bridge& bridge() { return *m_bridge; }

    // Execute JSON string RPC command and return parsed json response
    json call(const std::string& jsonRequest) {
        std::string responseStr = m_bridge->handle(jsonRequest);
        return json::parse(responseStr);
    }

    // Helper to format cmd + args into JSON format
    json call(const std::string& cmd, const json& args, int id = 1) {
        json req = {
            {"id", id},
            {"cmd", cmd},
            {"args", args}
        };
        return call(req.dump());
    }

    // Retrieve and clear pending events
    std::vector<std::string> drainEvents() {
        return m_bridge->drainEvents();
    }

    std::string audioStateJson() const {
        return m_bridge->audioStateJson();
    }

private:
    std::unique_ptr<MockHostActions> m_host;
    std::unique_ptr<reals::bridge::Bridge> m_bridge;
};

} // namespace reals::test
