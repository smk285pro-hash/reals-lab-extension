#include "reals/agent/AgentSession.h"

#include <chrono>
#include <string_view>
#include <fstream>
#include <sstream>
#include <utility>

#include "reals/platform/Path.h"
#include "reals/util/Log.h"

namespace reals::agent {

using json = nlohmann::json;

namespace {
constexpr const char* kTag = "agent";

bool isToolResultMessage(const json& m) {
    if (m.value("role", "") != "user" || !m.contains("content") || !m["content"].is_array())
        return false;
    for (const auto& b : m["content"])
        if (b.value("type", "") == "tool_result")
            return true;
    return false;
}

// Coalesces streamed text/thinking deltas into agent.delta / agent.thinking
// events so the UI bridge is not flooded with one event per token.
class DeltaBuffer {
public:
    DeltaBuffer(const AgentSession::Emit& emit, int flushMs) : m_emit(emit), m_flushMs(flushMs) {}

    void push(const char* kind, const std::string& text) {
        if (text.empty())
            return;
        if (m_kind != kind)
            flush();
        m_kind = kind;
        m_buf += text;
        if (std::string_view(kind) == "text")
            m_streamedText += text;
        const auto now = std::chrono::steady_clock::now();
        if (now - m_last >= std::chrono::milliseconds(m_flushMs))
            flush();
    }

    void flush() {
        if (!m_buf.empty() && m_kind) {
            const bool thinking = std::string_view(m_kind) == "thinking";
            m_emit({{"event", thinking ? "agent.thinking" : "agent.delta"},
                    {"data", {{"text", m_buf}}}});
        }
        m_buf.clear();
        m_last = std::chrono::steady_clock::now();
    }

    // Text streamed during the current LLM call (kept when cancelled).
    std::string takeStreamedText() {
        std::string out;
        out.swap(m_streamedText);
        return out;
    }

private:
    const AgentSession::Emit& m_emit;
    int m_flushMs;
    const char* m_kind = nullptr;
    std::string m_buf;
    std::string m_streamedText;
    std::chrono::steady_clock::time_point m_last = std::chrono::steady_clock::now();
};

std::string summarize(const json& j, size_t maxLen = 4000) {
    std::string s = j.dump();
    if (s.size() > maxLen)
        s = s.substr(0, maxLen) + "...(truncated)";
    return s;
}
} // namespace

AgentSession::AgentSession(Emit emit, Executor exec)
    : m_emit(std::move(emit)), m_exec(std::move(exec)) {}

AgentSession::~AgentSession() { cancel(); }

void AgentSession::setContextProvider(std::function<std::string()> provider) {
    const std::lock_guard lock(m_mutex);
    m_contextProvider = std::move(provider);
}

void AgentSession::setClient(std::shared_ptr<LlmClient> client) {
    const std::lock_guard lock(m_mutex);
    m_client = std::move(client);
}

std::string AgentSession::systemPrompt() {
    return "You are Reals Agent, the AI assistant built into Reals Lab, a REAPER extension by reals.media. "
           "You act on the user's REAPER project only through the provided tools.\n"
           "\n"
           "About Reals Lab (facts - never contradict them):\n"
           "- Reals Lab runs inside REAPER as a dockable window with tabs: Browser (sample library: text search plus "
           "/bpm:120-130 /key:Am /fav tokens, favorites, color tags, preview synced to project tempo/key, drag or insert "
           "into the timeline), Audio Lab (cloud GPU jobs), Marketplace, Agent (you) and Settings.\n"
           "- Audio Lab jobs (tool audio_lab_job): analyze = BPM + key; keychord = key + timed chord progression; "
           "stem = Demucs v4 separation (vocals/drums/bass/other); denoise = cleaned copy of the audio.\n"
           "- Reals Lab adds its own Chord Track to REAPER. When a keychord job finishes, Reals Lab automatically shows "
           "the progression in the Chord Track dock at the top of REAPER (aligned to the analyzed item, following the "
           "playhead) and lists it in the Audio Lab tab. chord_track(insert_midi) also creates a MIDI track named "
           "\"CHORD TRACK\" with the chord notes. So do NOT tell the user that REAPER has no chord track or suggest "
           "markers as a workaround - Reals Lab already provides it.\n"
           "- When a stem job finishes, Reals Lab automatically inserts the stems into a new folder track below the "
           "original item and mutes the original. Denoise output is not inserted automatically (use insert_media).\n"
           "- audio_lab_job waits for the job and returns the real result. Report what actually happened (key, BPM, "
           "chords, files, what was inserted). Never tell the user to wait or check a tab when you already have the "
           "result. For earlier results use get_lab_result.\n"
           "\n"
           "Rules:\n"
           "- Reply in the user's language (Vietnamese or English), short and friendly. Plain text; light markdown only.\n"
           "- Track and item numbers are 1-based, exactly as REAPER shows them. When you do not know the current "
           "state, inspect first (get_project_info, list_tracks, list_items) instead of guessing.\n"
           "- To process an item's audio, take source path, position and length from list_items and pass them to "
           "audio_lab_job so results line up with the item.\n"
           "- Prefer the semantic tools; use run_action only when nothing else fits, and say which action you ran.\n"
           "- Every change is wrapped in an undo point, so the user can press Ctrl+Z.\n"
           "- Batch work across many tracks/items by calling tools repeatedly; plan multi-step tasks first.\n"
           "- If a tool returns an error or the user denies it, explain briefly and suggest an alternative.\n"
           "- Never invent file paths. Keep the final answer to a short summary of what changed.";
}

void AgentSession::setHistoryPath(std::string utf8Path) {
    {
        const std::lock_guard lock(m_mutex);
        m_historyPath = std::move(utf8Path);
    }
    loadHistory();
}

void AgentSession::loadHistory() {
    const std::lock_guard lock(m_mutex);
    if (m_historyPath.empty())
        return;
    std::ifstream in(platform::u8path(m_historyPath), std::ios::binary);
    if (!in)
        return;
    std::stringstream ss;
    ss << in.rdbuf();
    const json j = json::parse(ss.str(), nullptr, false);
    if (!j.is_discarded() && j.is_array()) {
        m_history = j;
        trimHistory(m_history, kMaxHistoryMessages);
    }
}

void AgentSession::saveHistory() const {
    json copy;
    std::string path;
    {
        const std::lock_guard lock(m_mutex);
        copy = m_history;
        path = m_historyPath;
    }
    if (path.empty())
        return;
    std::ofstream out(platform::u8path(path), std::ios::binary | std::ios::trunc);
    if (out)
        out << copy.dump();
}

void AgentSession::trimHistory(json& history, size_t maxMessages) {
    if (!history.is_array())
        history = json::array();
    while (history.size() > maxMessages)
        history.erase(history.begin());
    // A conversation must start with a plain user message: drop leading
    // assistant turns and orphaned tool_result messages.
    while (!history.empty() &&
           (history[0].value("role", "") != "user" || isToolResultMessage(history[0])))
        history.erase(history.begin());
}

void AgentSession::clearHistory() {
    {
        const std::lock_guard lock(m_mutex);
        m_history = json::array();
    }
    saveHistory();
}

json AgentSession::historySnapshot() const {
    const std::lock_guard lock(m_mutex);
    return m_history;
}

void AgentSession::cancel() {
    m_cancel.store(true);
    std::shared_ptr<net::CancelToken> token;
    {
        const std::lock_guard lock(m_mutex);
        token = m_turnCancel;
    }
    if (token)
        token->cancel(); // aborts the blocking HTTP read on the worker thread
    const std::lock_guard lock(m_confirmMutex);
    for (auto& [id, v] : m_confirmations)
        if (v < 0)
            v = 0;
    m_confirmCv.notify_all();
}

void AgentSession::confirm(const std::string& callId, bool approved) {
    const std::lock_guard lock(m_confirmMutex);
    const auto it = m_confirmations.find(callId);
    if (it != m_confirmations.end() && it->second < 0) {
        it->second = approved ? 1 : 0;
        m_confirmCv.notify_all();
    }
}

bool AgentSession::waitForConfirm(const std::string& callId) {
    std::unique_lock lock(m_confirmMutex);
    m_confirmations[callId] = -1;
    const bool decided = m_confirmCv.wait_for(lock, std::chrono::milliseconds(confirmTimeoutMs), [&] {
        return m_cancel.load() || m_confirmations[callId] >= 0;
    });
    const bool ok = decided && !m_cancel.load() && m_confirmations[callId] == 1;
    m_confirmations.erase(callId);
    return ok;
}

void AgentSession::runTurn(const std::string& userText) {
    if (m_busy.exchange(true)) {
        m_emit({{"event", "agent.error"}, {"data", {{"message", "busy"}}}});
        return;
    }
    m_cancel.store(false);
    m_emit({{"event", "agent.state"}, {"data", {{"busy", true}}}});

    std::shared_ptr<LlmClient> client;
    json history;
    const auto token = std::make_shared<net::CancelToken>();
    {
        const std::lock_guard lock(m_mutex);
        m_turnCancel = token;
        client = m_client;
        m_history.push_back({{"role", "user"}, {"content", json::array({{{"type", "text"}, {"text", userText}}})}});
        trimHistory(m_history, kMaxHistoryMessages);
        history = m_history;
    }

    auto finish = [&](const json& hist) {
        {
            const std::lock_guard lock(m_mutex);
            m_turnCancel.reset();
            m_history = hist;
            trimHistory(m_history, kMaxHistoryMessages);
        }
        saveHistory();
        m_busy.store(false);
        m_emit({{"event", "agent.state"}, {"data", {{"busy", false}}}});
    };

    if (!client) {
        m_emit({{"event", "agent.error"}, {"data", {{"message", "not configured"}, {"code", "config"}}}});
        finish(history);
        return;
    }

    std::string system = systemPrompt();
    {
        std::function<std::string()> provider;
        {
            const std::lock_guard lock(m_mutex);
            provider = m_contextProvider;
        }
        if (provider) {
            const std::string ctx = provider();
            if (!ctx.empty())
                system += "\n\nCurrent Reals Lab state:\n" + ctx;
        }
    }
    const std::vector<ToolDef> tools = m_registry.allowed();

    DeltaBuffer deltas(m_emit, deltaFlushMs);
    StreamSink sink;
    sink.onText = [&](const std::string& t) { deltas.push("text", t); };
    sink.onThinking = [&](const std::string& t) { deltas.push("thinking", t); };
    std::string partialText; // streamed text of a cancelled call

    for (int step = 0; step < kMaxSteps; ++step) {
        if (m_cancel.load()) {
            m_emit({{"event", "agent.cancelled"}, {"data", json::object()}});
            break;
        }
        const LlmReply reply = client->chat(system, history, tools, &sink, token);
        deltas.flush();
        const std::string streamed = deltas.takeStreamedText();
        if (m_cancel.load() || reply.error == "cancelled") {
            partialText = streamed;
            m_emit({{"event", "agent.cancelled"}, {"data", {{"partial", !streamed.empty()}}}});
            break;
        }
        if (!reply.error.empty()) {
            LOG_WARN(kTag, "llm error: " + reply.error);
            m_emit({{"event", "agent.error"}, {"data", {{"message", reply.error}, {"code", "llm"}}}});
            break;
        }
        if (reply.content.empty()) {
            m_emit({{"event", "agent.message"}, {"data", {{"role", "assistant"}, {"text", ""}}}});
            break;
        }
        history.push_back({{"role", "assistant"}, {"content", reply.content}});
        const std::string text = reply.text();
        if (!text.empty())
            m_emit({{"event", "agent.message"}, {"data", {{"role", "assistant"}, {"text", text}}}});

        const std::vector<ToolCall> calls = reply.toolCalls();
        if (calls.empty())
            break;

        json results = json::array();
        for (const auto& call : calls) {
            const auto def = m_registry.find(call.name);
            const std::string risk = def ? riskName(def->risk) : "danger";
            ToolOutcome outcome;
            bool denied = false;
            if (m_cancel.load()) {
                outcome.data = {{"error", "cancelled by user"}};
                denied = true;
            } else if (!m_registry.isAllowed(call.name)) {
                outcome.data = {{"error", "tool not permitted by policy: " + call.name}};
                denied = true;
            } else if (m_registry.needsConfirm(call.name, mode())) {
                m_emit({{"event", "agent.confirmRequest"},
                        {"data", {{"id", call.id}, {"tool", call.name}, {"args", call.input}, {"risk", risk}}}});
                if (!waitForConfirm(call.id)) {
                    outcome.data = {{"error", m_cancel.load() ? "cancelled by user" : "user denied this action"}};
                    denied = true;
                }
            }
            if (!denied) {
                m_emit({{"event", "agent.toolCall"},
                        {"data", {{"id", call.id}, {"tool", call.name}, {"args", call.input}, {"risk", risk}}}});
                try {
                    outcome = m_exec ? m_exec(call) : ToolOutcome{false, {{"error", "no executor"}}};
                } catch (const std::exception& e) {
                    outcome = ToolOutcome{false, {{"error", std::string("executor exception: ") + e.what()}}};
                }
            }
            m_emit({{"event", "agent.toolResult"},
                    {"data", {{"id", call.id}, {"tool", call.name}, {"ok", outcome.ok && !denied},
                              {"denied", denied}, {"result", outcome.data}}}});
            json block = {{"type", "tool_result"}, {"tool_use_id", call.id},
                          {"content", summarize(outcome.data)}};
            if (!outcome.ok || denied)
                block["is_error"] = true;
            results.push_back(block);
        }
        history.push_back({{"role", "user"}, {"content", results}});

        if (step == kMaxSteps - 1)
            m_emit({{"event", "agent.error"}, {"data", {{"message", "step limit reached"}, {"code", "steps"}}}});
    }

    // Keep the transcript valid (alternating roles) if we stopped after a
    // user turn: tool results, a cancelled call or an LLM error.
    if (!history.empty() && history.back().value("role", "") == "user") {
        const std::string note = partialText.empty() ? "(stopped)" : partialText + "\n(stopped)";
        history.push_back({{"role", "assistant"},
                           {"content", json::array({{{"type", "text"}, {"text", note}}})}});
    }
    finish(history);
}

} // namespace reals::agent
