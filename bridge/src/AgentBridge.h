#pragma once

// Bridge-side glue for the in-product agent (SPEC 5.4 / P5).
// - Owns the AgentSession and the worker thread running a turn.
// - Marshals every tool execution onto the UI/host thread: the worker posts a
//   task and blocks; Bridge::drainEvents() (called from the host timer on the
//   main thread) pumps the queue. REAPER API calls therefore never run on a
//   background thread.
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include <nlohmann/json.hpp>

#include "reals/agent/AgentSession.h"
#include "reals/bridge/Bridge.h"

namespace reals::bridge {

class AgentBridge {
public:
    using Emit = std::function<void(const nlohmann::json&)>;
    // Runs an Audio Lab job (job, path, param) — wired to Bridge::Impl::runLabJob.
    using LabJob = std::function<void(const std::string&, const std::string&, int)>;

    AgentBridge(IHostActions* host, Emit emit, LabJob labJob, std::string historyPath);
    ~AgentBridge();

    AgentBridge(const AgentBridge&) = delete;
    AgentBridge& operator=(const AgentBridge&) = delete;

    // Handle an "agent.*" command; fills res["ok"], res["data"]/res["error"].
    void handle(const std::string& cmd, const nlohmann::json& args, nlohmann::json& res);

    // Execute queued tool calls. MUST be called on the host/UI thread.
    void pumpMainThread();

    // Observer for Audio Lab completion events (lab.result / lab.error).
    // Called from lab worker threads; must not call back into Bridge.
    void onLabEvent(const nlohmann::json& ev);

    // Compact, model-facing summary of a lab result event payload (tests).
    [[nodiscard]] static nlohmann::json summarizeLab(const std::string& job, const nlohmann::json& data);

    // Current LLM settings (Config first, then environment, then defaults).
    [[nodiscard]] static agent::LlmConfig resolveConfig(std::string* keySource = nullptr);

    // Simplified transcript for the chat UI.
    [[nodiscard]] static nlohmann::json uiHistory(const nlohmann::json& history);

private:
    struct Task {
        agent::ToolCall call;
        std::promise<agent::ToolOutcome> promise;
    };

    agent::ToolOutcome executeOnMainThread(const agent::ToolCall& call);
    agent::ToolOutcome executeNow(const agent::ToolCall& call);
    // Audio Lab tools run on the turn thread (they wait for cloud jobs).
    agent::ToolOutcome runLabJobTool(const agent::ToolCall& call);
    agent::ToolOutcome labResultTool(const agent::ToolCall& call) const;
    // chord_track runs on the main thread (host calls).
    agent::ToolOutcome chordTrackTool(const agent::ToolCall& call);
    [[nodiscard]] std::string labContext() const;
    void startTurn(const std::string& text);
    void fetchModels();
    void joinFinished();

    IHostActions* m_host;
    Emit m_emit;
    LabJob m_labJob;
    std::unique_ptr<agent::AgentSession> m_session;

    std::atomic<bool> m_shutdown{false};
    std::mutex m_taskMutex;
    std::deque<std::shared_ptr<Task>> m_tasks;

    std::mutex m_threadMutex;
    std::thread m_turnThread;
    std::thread m_modelsThread;
    std::atomic<bool> m_modelsBusy{false};

    // Audio Lab results seen this session (job -> {seq, ok, data|error, path, position, length}).
    mutable std::mutex m_labMutex;
    std::condition_variable m_labCv;
    uint64_t m_labSeq = 0;
    nlohmann::json m_labLast = nlohmann::json::object();
    nlohmann::json m_labPending = nlohmann::json::object(); // job -> {path, position, length}
};

} // namespace reals::bridge
