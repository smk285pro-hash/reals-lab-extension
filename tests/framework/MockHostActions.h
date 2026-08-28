#pragma once

#include <deque>
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
