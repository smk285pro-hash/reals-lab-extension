#pragma once

// Agent conversation loop: user text -> LLM -> tool calls -> executor ->
// tool results -> LLM ... until the model answers without tools.
// Runs on a worker thread; the executor callback is responsible for
// marshalling host calls to the right thread (Bridge does this).
#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>

#include "reals/agent/AgentTypes.h"
#include "reals/agent/LlmClient.h"
#include "reals/agent/ToolRegistry.h"

namespace reals::agent {

class AgentSession {
public:
    using Emit = std::function<void(const nlohmann::json& event)>;
    using Executor = std::function<ToolOutcome(const ToolCall&)>;

    AgentSession(Emit emit, Executor exec);
    ~AgentSession();

    AgentSession(const AgentSession&) = delete;
    AgentSession& operator=(const AgentSession&) = delete;

    void setClient(std::shared_ptr<LlmClient> client);
    // Live host state appended to the system prompt at the start of each turn
    // (e.g. latest Audio Lab results). Called on the turn thread.
    void setContextProvider(std::function<std::string()> provider);
    // True while a cancel request is pending for the current turn.
    [[nodiscard]] bool cancelRequested() const { return m_cancel.load(); }
    void setMode(PermissionMode mode) { m_mode.store(static_cast<int>(mode)); }
    [[nodiscard]] PermissionMode mode() const { return static_cast<PermissionMode>(m_mode.load()); }
    ToolRegistry& registry() { return m_registry; }

    // Optional persistence file for history (UTF-8 path). Loads immediately.
    void setHistoryPath(std::string utf8Path);

    // Process one user turn synchronously (call from a worker thread).
    void runTurn(const std::string& userText);

    // Thread-safe controls. cancel() also aborts the in-flight HTTP request,
    // so a streaming reply stops immediately.
    void cancel();
    [[nodiscard]] bool isBusy() const { return m_busy.load(); }
    // Resolve a pending confirmation (from the UI).
    void confirm(const std::string& callId, bool approved);
    void clearHistory();
    [[nodiscard]] nlohmann::json historySnapshot() const;

    static constexpr int kMaxSteps = 24;
    static constexpr size_t kMaxHistoryMessages = 80;
    [[nodiscard]] static std::string systemPrompt();

    // Exposed for tests: drop oldest turns but never orphan a tool_result.
    static void trimHistory(nlohmann::json& history, size_t maxMessages);

    // Confirmation wait timeout (ms). Tests shorten it.
    int confirmTimeoutMs = 5 * 60 * 1000;
    // Streaming deltas are coalesced into UI events at most this often (ms).
    int deltaFlushMs = 40;

private:
    bool waitForConfirm(const std::string& callId);
    void saveHistory() const;
    void loadHistory();

    Emit m_emit;
    Executor m_exec;
    std::shared_ptr<LlmClient> m_client;
    ToolRegistry m_registry;
    std::atomic<int> m_mode{static_cast<int>(PermissionMode::AskDangerous)};
    std::atomic<bool> m_cancel{false};
    std::atomic<bool> m_busy{false};

    mutable std::mutex m_mutex; // guards m_history, m_client, m_historyPath, m_turnCancel
    std::shared_ptr<net::CancelToken> m_turnCancel; // aborts the in-flight LLM request
    std::function<std::string()> m_contextProvider;
    nlohmann::json m_history = nlohmann::json::array();
    std::string m_historyPath;

    std::mutex m_confirmMutex;
    std::condition_variable m_confirmCv;
    std::unordered_map<std::string, int> m_confirmations; // id -> -1 pending, 0 deny, 1 ok
};

} // namespace reals::agent
