// P5 agent core: LLM client conversion, tool registry policy and the
// tool-use loop (mock transport + mock executor, no network, no REAPER).
#include <algorithm>
#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <nlohmann/json.hpp>

#include "../framework/TestRunner.h"
#include "reals/agent/AgentSession.h"
#include "reals/agent/LlmClient.h"
#include "reals/agent/ToolRegistry.h"
#include "../../bridge/src/AgentBridge.h"

using nlohmann::json;
using namespace reals::agent;

namespace {

LlmConfig testConfig(const std::string& provider = "anthropic") {
    LlmConfig c;
    c.provider = provider;
    c.baseUrl = "https://proxy.example.com";
    c.apiKey = "test-key";
    c.model = "test-model";
    return c;
}

reals::net::Response okResponse(const json& body) {
    reals::net::Response r;
    r.statusCode = 200;
    r.body = body.dump();
    return r;
}

json toolUseReply(const std::string& id, const std::string& name, const json& input) {
    return {{"content", json::array({{{"type", "tool_use"}, {"id", id}, {"name", name}, {"input", input}}})},
            {"stop_reason", "tool_use"}};
}

json textReply(const std::string& text) {
    return {{"content", json::array({{{"type", "text"}, {"text", text}}})}, {"stop_reason", "end_turn"}};
}

// Scripted LLM: returns the queued replies in order and records requests.
struct ScriptedLlm {
    std::vector<json> replies;
    std::vector<json> requests;
    size_t next = 0;
    std::mutex mutex;

    LlmClient::Transport transport() {
        return [this](const reals::net::Request& req) {
            const std::lock_guard lock(mutex);
            requests.push_back(json::parse(req.body));
            const json reply = next < replies.size() ? replies[next++] : textReply("done");
            return okResponse(reply);
        };
    }
};

struct EventLog {
    std::mutex mutex;
    std::vector<json> events;
    void push(const json& e) {
        const std::lock_guard lock(mutex);
        events.push_back(e);
    }
    int count(const std::string& name) {
        const std::lock_guard lock(mutex);
        int n = 0;
        for (const auto& e : events)
            if (e.value("event", "") == name)
                ++n;
        return n;
    }
    json first(const std::string& name) {
        const std::lock_guard lock(mutex);
        for (const auto& e : events)
            if (e.value("event", "") == name)
                return e.value("data", json::object());
        return json();
    }
};

} // namespace

TEST(Agent, EndpointForProviders) {
    LlmConfig c = testConfig();
    EXPECT_EQ(LlmClient::endpointFor(c), std::string("https://proxy.example.com/v1/messages"));
    c.baseUrl = "https://proxy.example.com/v1/";
    EXPECT_EQ(LlmClient::endpointFor(c), std::string("https://proxy.example.com/v1/messages"));
    c.baseUrl = "";
    EXPECT_EQ(LlmClient::endpointFor(c), std::string("https://api.anthropic.com/v1/messages"));
    c = testConfig("openai");
    EXPECT_EQ(LlmClient::endpointFor(c), std::string("https://proxy.example.com/v1/chat/completions"));
}

TEST(Agent, ParseAnthropicDropsUnknownBlocks) {
    const json body = {{"content", json::array({{{"type", "thinking"}, {"thinking", "x"}},
                                                {{"type", "text"}, {"text", "hi"}},
                                                {{"type", "tool_use"}, {"id", "t1"}, {"name", "list_tracks"},
                                                 {"input", json::object()}}})},
                       {"stop_reason", "tool_use"}};
    const LlmReply r = LlmClient::parseAnthropic(body);
    EXPECT_TRUE(r.error.empty());
    EXPECT_EQ(r.content.size(), static_cast<size_t>(2));
    EXPECT_EQ(r.text(), std::string("hi"));
    const auto calls = r.toolCalls();
    EXPECT_EQ(calls.size(), static_cast<size_t>(1));
    EXPECT_EQ(calls[0].name, std::string("list_tracks"));
    EXPECT_FALSE(LlmClient::parseAnthropic(json::object()).error.empty());
}

TEST(Agent, ParseOpenAiToolCalls) {
    const json body = {{"choices", json::array({{{"finish_reason", "tool_calls"},
                                                 {"message",
                                                  {{"content", "ok"},
                                                   {"tool_calls", json::array({{{"id", "c1"},
                                                                                {"function",
                                                                                 {{"name", "set_tempo"},
                                                                                  {"arguments", "{\"bpm\":120}"}}}}})}}}}})}};
    const LlmReply r = LlmClient::parseOpenAi(body);
    EXPECT_TRUE(r.error.empty());
    EXPECT_EQ(r.stopReason, std::string("tool_use"));
    const auto calls = r.toolCalls();
    EXPECT_EQ(calls.size(), static_cast<size_t>(1));
    EXPECT_EQ(calls[0].input.value("bpm", 0), 120);
}

TEST(Agent, OpenAiMessageConversion) {
    const json history = json::array(
        {{{"role", "user"}, {"content", json::array({{{"type", "text"}, {"text", "hello"}}})}},
         {{"role", "assistant"},
          {"content", json::array({{{"type", "tool_use"}, {"id", "c1"}, {"name", "list_tracks"},
                                    {"input", json::object()}}})}},
         {{"role", "user"},
          {"content", json::array({{{"type", "tool_result"}, {"tool_use_id", "c1"}, {"content", "[]"}}})}}});
    const json out = LlmClient::toOpenAiMessages("sys", history);
    EXPECT_TRUE(out.is_array());
    EXPECT_GE(out.size(), static_cast<size_t>(4));
    EXPECT_EQ(out[0].value("role", ""), std::string("system"));
    bool sawTool = false;
    for (const auto& m : out)
        if (m.value("role", "") == "tool" && m.value("tool_call_id", "") == "c1")
            sawTool = true;
    EXPECT_TRUE(sawTool);
}

TEST(Agent, ChatSendsAuthHeadersAndTools) {
    std::string url;
    std::string apiKeyHeader;
    json sent;
    LlmClient client(testConfig(), [&](const reals::net::Request& req) {
        url = req.url;
        auto it = req.headers.find("x-api-key");
        if (it != req.headers.end())
            apiKeyHeader = it->second;
        sent = json::parse(req.body);
        return okResponse(textReply("pong"));
    });
    ToolRegistry reg;
    const LlmReply r = client.chat("sys", json::array(), reg.allowed());
    EXPECT_TRUE(r.error.empty());
    EXPECT_EQ(r.text(), std::string("pong"));
    EXPECT_EQ(url, std::string("https://proxy.example.com/v1/messages"));
    EXPECT_EQ(apiKeyHeader, std::string("test-key"));
    EXPECT_EQ(sent.value("model", ""), std::string("test-model"));
    EXPECT_EQ(sent["tools"].size(), reg.all().size());
}

TEST(Agent, ChatReportsHttpErrors) {
    LlmClient client(testConfig(), [](const reals::net::Request&) {
        reals::net::Response r;
        r.statusCode = 401;
        r.body = R"({"error":{"type":"authentication_error","message":"bad key"}})";
        return r;
    });
    const LlmReply r = client.chat("sys", json::array(), {});
    EXPECT_FALSE(r.error.empty());
    LlmConfig noKey = testConfig();
    noKey.apiKey.clear();
    EXPECT_FALSE(LlmClient(noKey, [](const reals::net::Request&) { return reals::net::Response{}; })
                     .chat("s", json::array(), {})
                     .error.empty());
}

TEST(Agent, RegistryBuiltinsAndPolicy) {
    ToolRegistry reg;
    EXPECT_GE(reg.all().size(), static_cast<size_t>(40));
    EXPECT_TRUE(reg.find("list_tracks").has_value());
    EXPECT_FALSE(reg.find("no_such_tool").has_value());
    for (const auto& t : reg.all()) {
        EXPECT_FALSE(t.name.empty());
        EXPECT_TRUE(t.inputSchema.is_object());
    }
    // Read tools never prompt; write prompts only in AskAll; danger prompts unless Full.
    EXPECT_FALSE(reg.needsConfirm("list_tracks", PermissionMode::AskAll));
    EXPECT_TRUE(reg.needsConfirm("set_track_volume", PermissionMode::AskAll));
    EXPECT_FALSE(reg.needsConfirm("set_track_volume", PermissionMode::AskDangerous));
    EXPECT_TRUE(reg.needsConfirm("delete_track", PermissionMode::AskDangerous));
    EXPECT_FALSE(reg.needsConfirm("delete_track", PermissionMode::Full));
    EXPECT_TRUE(reg.needsConfirm("unknown_tool", PermissionMode::AskDangerous));

    reg.setAllowedTools({"list_tracks", "get_project_info"});
    EXPECT_EQ(reg.allowed().size(), static_cast<size_t>(2));
    EXPECT_TRUE(reg.isAllowed("list_tracks"));
    EXPECT_FALSE(reg.isAllowed("delete_track"));
}

TEST(Agent, RegistryLoadJsonReplacesCatalog) {
    ToolRegistry reg;
    const json arr = json::array({{{"name", "custom_tool"},
                                   {"risk", "danger"},
                                   {"description", "x"},
                                   {"input_schema", {{"type", "object"}, {"properties", json::object()}}}}});
    EXPECT_TRUE(reg.loadJson(arr));
    EXPECT_EQ(reg.all().size(), static_cast<size_t>(1));
    EXPECT_TRUE(reg.needsConfirm("custom_tool", PermissionMode::AskDangerous));
    EXPECT_FALSE(reg.loadJson(json::object()));
}

TEST(Agent, TrimHistoryKeepsValidStart) {
    json h = json::array();
    h.push_back({{"role", "assistant"}, {"content", json::array({{{"type", "text"}, {"text", "a"}}})}});
    h.push_back({{"role", "user"},
                 {"content", json::array({{{"type", "tool_result"}, {"tool_use_id", "x"}, {"content", "r"}}})}});
    h.push_back({{"role", "user"}, {"content", json::array({{{"type", "text"}, {"text", "q"}}})}});
    AgentSession::trimHistory(h, 10);
    EXPECT_EQ(h.size(), static_cast<size_t>(1));
    EXPECT_EQ(h[0]["content"][0].value("text", ""), std::string("q"));
}

TEST(Agent, LoopRunsToolAndFinishes) {
    ScriptedLlm llm;
    llm.replies = {toolUseReply("t1", "list_tracks", json::object()), textReply("There are 2 tracks.")};
    EventLog log;
    std::vector<std::string> executed;
    AgentSession session([&](const json& e) { log.push(e); },
                         [&](const ToolCall& c) {
                             executed.push_back(c.name);
                             return ToolOutcome{true, json::array({"Drums", "Bass"})};
                         });
    session.setClient(std::make_shared<LlmClient>(testConfig(), llm.transport()));
    session.runTurn("how many tracks?");

    EXPECT_FALSE(session.isBusy());
    EXPECT_EQ(executed.size(), static_cast<size_t>(1));
    EXPECT_EQ(log.count("agent.toolCall"), 1);
    EXPECT_EQ(log.count("agent.toolResult"), 1);
    EXPECT_EQ(log.count("agent.confirmRequest"), 0);
    EXPECT_EQ(log.first("agent.message").value("text", ""), std::string("There are 2 tracks."));
    EXPECT_EQ(llm.requests.size(), static_cast<size_t>(2));
    // Second request must carry the tool_result for t1.
    const json& msgs = llm.requests[1]["messages"];
    EXPECT_EQ(msgs.back()["content"][0].value("tool_use_id", ""), std::string("t1"));
    EXPECT_EQ(session.historySnapshot().size(), static_cast<size_t>(4));
}

TEST(Agent, DangerousToolWaitsForConfirmation) {
    for (const bool approve : {true, false}) {
        ScriptedLlm llm;
        llm.replies = {toolUseReply("d1", "delete_track", {{"track", 1}}), textReply("ok")};
        EventLog log;
        std::atomic<int> executed{0};
        AgentSession session([&](const json& e) { log.push(e); },
                             [&](const ToolCall&) {
                                 ++executed;
                                 return ToolOutcome{true, json::object()};
                             });
        session.setMode(PermissionMode::AskDangerous);
        session.setClient(std::make_shared<LlmClient>(testConfig(), llm.transport()));
        std::thread worker([&] { session.runTurn("delete track 1"); });
        for (int i = 0; i < 400 && log.count("agent.confirmRequest") == 0; ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        EXPECT_EQ(log.count("agent.confirmRequest"), 1);
        session.confirm("d1", approve);
        worker.join();
        EXPECT_EQ(executed.load(), approve ? 1 : 0);
        EXPECT_EQ(log.first("agent.toolResult").value("denied", !approve), !approve);
    }
}

TEST(Agent, FullModeSkipsConfirmation) {
    ScriptedLlm llm;
    llm.replies = {toolUseReply("d1", "delete_track", {{"track", 1}}), textReply("deleted")};
    EventLog log;
    AgentSession session([&](const json& e) { log.push(e); },
                         [](const ToolCall&) { return ToolOutcome{true, json::object()}; });
    session.setMode(PermissionMode::Full);
    session.setClient(std::make_shared<LlmClient>(testConfig(), llm.transport()));
    session.runTurn("delete track 1");
    EXPECT_EQ(log.count("agent.confirmRequest"), 0);
    EXPECT_EQ(log.count("agent.toolCall"), 1);
}

TEST(Agent, MissingClientReportsConfigError) {
    EventLog log;
    AgentSession session([&](const json& e) { log.push(e); },
                         [](const ToolCall&) { return ToolOutcome{true, json::object()}; });
    session.runTurn("hi");
    EXPECT_EQ(log.first("agent.error").value("code", ""), std::string("config"));
    EXPECT_FALSE(session.isBusy());
}

// ---- Streaming / thinking / cancel / tool protocol (P5.01) -------------------
namespace {

std::string sse(const std::string& event, const json& data) {
    return "event: " + event + "\r\ndata: " + data.dump() + "\r\n\r\n";
}

// Anthropic SSE transcript: thinking (unsigned) + text + one tool_use.
std::string anthropicStreamBody() {
    std::string s;
    s += sse("message_start", {{"type", "message_start"}, {"message", {{"content", json::array()}}}});
    s += ": keep-alive\n\n";
    s += sse("content_block_start", {{"type", "content_block_start"}, {"index", 0},
                                     {"content_block", {{"type", "thinking"}, {"thinking", ""}}}});
    s += sse("content_block_delta", {{"type", "content_block_delta"}, {"index", 0},
                                     {"delta", {{"type", "thinking_delta"}, {"thinking", "Need "}}}});
    s += sse("content_block_delta", {{"type", "content_block_delta"}, {"index", 0},
                                     {"delta", {{"type", "thinking_delta"}, {"thinking", "tracks."}}}});
    s += sse("content_block_stop", {{"type", "content_block_stop"}, {"index", 0}});
    s += sse("content_block_start", {{"type", "content_block_start"}, {"index", 1},
                                     {"content_block", {{"type", "text"}, {"text", ""}}}});
    s += sse("content_block_delta", {{"type", "content_block_delta"}, {"index", 1},
                                     {"delta", {{"type", "text_delta"}, {"text", "Xin "}}}});
    s += sse("content_block_delta", {{"type", "content_block_delta"}, {"index", 1},
                                     {"delta", {{"type", "text_delta"}, {"text", "chào"}}}});
    s += sse("content_block_start", {{"type", "content_block_start"}, {"index", 2},
                                     {"content_block", {{"type", "tool_use"}, {"id", "t1"},
                                                        {"name", "set_tempo"}, {"input", json::object()}}}});
    s += sse("content_block_delta", {{"type", "content_block_delta"}, {"index", 2},
                                     {"delta", {{"type", "input_json_delta"}, {"partial_json", "{\"bp"}}}});
    s += sse("content_block_delta", {{"type", "content_block_delta"}, {"index", 2},
                                     {"delta", {{"type", "input_json_delta"}, {"partial_json", "m\":128}"}}}});
    s += sse("message_delta", {{"type", "message_delta"}, {"delta", {{"stop_reason", "tool_use"}}}});
    s += sse("message_stop", {{"type", "message_stop"}});
    return s;
}

// Transport that streams a body through req.onData in small chunks.
reals::net::Response streamThrough(const reals::net::Request& req, const std::string& body,
                                   size_t chunk = 7) {
    reals::net::Response r;
    r.statusCode = 200;
    for (size_t i = 0; i < body.size(); i += chunk) {
        const size_t n = std::min(chunk, body.size() - i);
        if (!req.onData) {
            r.body.append(body, i, n);
        } else if (!req.onData(body.data() + i, n)) {
            r.error = "cancelled";
            break;
        }
    }
    return r;
}

} // namespace

TEST(Agent, SseParserHandlesSplitChunks) {
    const std::string body = "event: a\r\ndata: {\"x\":1}\r\n\r\n: ping\n\nevent: b\ndata: l1\ndata: l2\n\ndata: tail";
    for (size_t chunk = 1; chunk <= body.size(); chunk += 5) {
        std::vector<std::pair<std::string, std::string>> got;
        SseParser p;
        const SseParser::Handler h = [&](const std::string& e, const std::string& d) { got.emplace_back(e, d); };
        for (size_t i = 0; i < body.size(); i += chunk)
            p.feed(body.data() + i, std::min(chunk, body.size() - i), h);
        p.finish(h);
        EXPECT_EQ(got.size(), static_cast<size_t>(3));
        if (got.size() == 3) {
            EXPECT_EQ(got[0].first, std::string("a"));
            EXPECT_EQ(got[0].second, std::string("{\"x\":1}"));
            EXPECT_EQ(got[1].second, std::string("l1\nl2"));
            EXPECT_EQ(got[2].second, std::string("tail"));
        }
    }
}

TEST(Agent, AnthropicStreamAssemblesBlocks) {
    std::string text, thinking;
    StreamSink sink;
    sink.onText = [&](const std::string& t) { text += t; };
    sink.onThinking = [&](const std::string& t) { thinking += t; };
    AnthropicStream st(&sink);
    SseParser p;
    const std::string body = anthropicStreamBody();
    p.feed(body.data(), body.size(), [&](const std::string& e, const std::string& d) { st.onEvent(e, d); });
    EXPECT_TRUE(st.sawEvents());
    const LlmReply r = st.finish();
    EXPECT_TRUE(r.error.empty());
    EXPECT_EQ(text, std::string("Xin chào"));
    EXPECT_EQ(thinking, std::string("Need tracks."));
    EXPECT_EQ(r.thinking, std::string("Need tracks."));
    EXPECT_EQ(r.stopReason, std::string("tool_use"));
    // Unsigned thinking is display-only: content = text + tool_use.
    EXPECT_EQ(r.content.size(), static_cast<size_t>(2));
    EXPECT_EQ(r.text(), std::string("Xin chào"));
    const auto calls = r.toolCalls();
    EXPECT_EQ(calls.size(), static_cast<size_t>(1));
    if (!calls.empty())
        EXPECT_EQ(calls[0].input.value("bpm", 0), 128);
}

TEST(Agent, SignedThinkingIsReplayable) {
    const json blocks = json::array({{{"type", "thinking"}, {"thinking", "a"}, {"signature", "sig"}},
                                     {{"type", "thinking"}, {"thinking", "b"}},
                                     {{"type", "redacted_thinking"}, {"data", "x"}},
                                     {{"type", "text"}, {"text", "t"}}});
    EXPECT_EQ(replayableContent(blocks).size(), static_cast<size_t>(3));
}

TEST(Agent, OpenAiStreamAssemblesToolCalls) {
    std::string text;
    StreamSink sink;
    sink.onText = [&](const std::string& t) { text += t; };
    OpenAiStream st(&sink);
    st.onEvent("", json({{"choices", {{{"delta", {{"reasoning_content", "hmm"}}}}}}}).dump());
    st.onEvent("", json({{"choices", {{{"delta", {{"content", "Hi"}}}}}}}).dump());
    st.onEvent("", json({{"choices", {{{"delta", {{"tool_calls", {{{"index", 0}, {"id", "c1"},
        {"function", {{"name", "set_tempo"}, {"arguments", "{\"bpm\":"}}}}}}}}}}}}).dump());
    st.onEvent("", json({{"choices", {{{"delta", {{"tool_calls", {{{"index", 0},
        {"function", {{"arguments", "90}"}}}}}}}}, {"finish_reason", "tool_calls"}}}}}).dump());
    st.onEvent("", "[DONE]");
    const LlmReply r = st.finish();
    EXPECT_EQ(text, std::string("Hi"));
    EXPECT_EQ(r.thinking, std::string("hmm"));
    EXPECT_EQ(r.stopReason, std::string("tool_use"));
    const auto calls = r.toolCalls();
    EXPECT_EQ(calls.size(), static_cast<size_t>(1));
    if (!calls.empty())
        EXPECT_EQ(calls[0].input.value("bpm", 0), 90);
}

TEST(Agent, ChatStreamsWithThinkingParams) {
    json sent;
    LlmConfig cfg = testConfig();
    cfg.thinkingBudget = 2000;
    cfg.maxTokens = 1000;
    LlmClient client(cfg, [&](const reals::net::Request& req) {
        sent = json::parse(req.body);
        EXPECT_EQ(req.headers.at("Accept"), std::string("text/event-stream"));
        return streamThrough(req, anthropicStreamBody());
    });
    std::string text;
    StreamSink sink;
    sink.onText = [&](const std::string& t) { text += t; };
    const LlmReply r = client.chat("sys", json::array({{{"role", "user"}, {"content", "hi"}}}), {}, &sink);
    EXPECT_TRUE(r.error.empty());
    EXPECT_EQ(text, std::string("Xin chào"));
    EXPECT_TRUE(sent.value("stream", false));
    EXPECT_EQ(sent["thinking"].value("budget_tokens", 0), 2000);
    EXPECT_EQ(sent.value("max_tokens", 0), 4048); // budget + 2048 headroom
}

TEST(Agent, TextToolProtocolConversion) {
    const json msgs = json::array({
        {{"role", "user"}, {"content", json::array({{{"type", "text"}, {"text", "tempo 128"}}})}},
        {{"role", "assistant"}, {"content", json::array({{{"type", "thinking"}, {"thinking", "x"}},
                                                         {{"type", "tool_use"}, {"id", "t1"}, {"name", "set_tempo"},
                                                          {"input", {{"bpm", 128}}}}})}},
        {{"role", "user"}, {"content", json::array({{{"type", "tool_result"}, {"tool_use_id", "t1"},
                                                     {"content", "{\"ok\":true}"}}})}}});
    EXPECT_TRUE(LlmClient::hasToolResults(msgs));
    const json out = LlmClient::toTextToolProtocol(msgs);
    EXPECT_EQ(out.size(), static_cast<size_t>(3));
    EXPECT_EQ(out[1]["content"].get<std::string>(),
              std::string("<tool_call id=\"t1\" name=\"set_tempo\">{\"bpm\":128}</tool_call>"));
    EXPECT_EQ(out[2]["content"].get<std::string>(),
              std::string("<tool_result id=\"t1\" ok=\"true\">{\"ok\":true}</tool_result>"));
    EXPECT_FALSE(LlmClient::hasToolResults(out));
}

TEST(Agent, AutoProtocolFallsBackToText) {
    LlmClient::resetProtocolMemory();
    std::vector<json> bodies;
    LlmClient client(testConfig(), [&](const reals::net::Request& req) {
        bodies.push_back(json::parse(req.body));
        reals::net::Response r;
        if (LlmClient::hasToolResults(bodies.back()["messages"])) {
            r.statusCode = 502;
            r.body = "<html>Bad gateway</html>";
            return r;
        }
        return okResponse(textReply("done"));
    });
    const json msgs = json::array({
        {{"role", "user"}, {"content", "go"}},
        {{"role", "assistant"}, {"content", json::array({{{"type", "tool_use"}, {"id", "t1"}, {"name", "x"},
                                                          {"input", json::object()}}})}},
        {{"role", "user"}, {"content", json::array({{{"type", "tool_result"}, {"tool_use_id", "t1"},
                                                     {"content", "ok"}}})}}});
    LlmReply r = client.chat("sys", msgs, {});
    EXPECT_TRUE(r.error.empty());
    EXPECT_EQ(bodies.size(), static_cast<size_t>(2));
    EXPECT_FALSE(bodies[0].contains("thinking")); // unsigned history -> no thinking on native
    EXPECT_TRUE(bodies[1].contains("thinking"));
    // Endpoint is remembered: the next call goes straight to text mode.
    r = client.chat("sys", msgs, {});
    EXPECT_TRUE(r.error.empty());
    EXPECT_EQ(bodies.size(), static_cast<size_t>(3));
    LlmClient::resetProtocolMemory();
}

TEST(Agent, SessionStreamsDeltaEvents) {
    LlmClient::resetProtocolMemory();
    int call = 0;
    LlmClient::Transport t = [&](const reals::net::Request& req) {
        if (call++ == 0)
            return streamThrough(req, anthropicStreamBody());
        return okResponse(textReply("Đã đặt tempo 128."));
    };
    EventLog log;
    AgentSession session([&](const json& e) { log.push(e); },
                         [](const ToolCall&) { return ToolOutcome{true, {{"bpm", 128}}}; });
    session.deltaFlushMs = 0;
    session.setMode(PermissionMode::Full);
    session.setClient(std::make_shared<LlmClient>(testConfig(), t));
    session.runTurn("tempo 128");
    std::string deltas, thinking;
    {
        const std::lock_guard lock(log.mutex);
        for (const auto& e : log.events) {
            if (e.value("event", "") == "agent.delta")
                deltas += e["data"].value("text", "");
            if (e.value("event", "") == "agent.thinking")
                thinking += e["data"].value("text", "");
        }
    }
    EXPECT_EQ(deltas, std::string("Xin chàoĐã đặt tempo 128."));
    EXPECT_EQ(thinking, std::string("Need tracks."));
    EXPECT_EQ(log.count("agent.toolCall"), 1);
    EXPECT_EQ(log.count("agent.message"), 2);
}

TEST(Agent, CancelAbortsStreamingRequest) {
    std::atomic<bool> started{false};
    LlmClient::Transport t = [&](const reals::net::Request& req) {
        reals::net::Response r;
        r.statusCode = 200;
        const std::string first = sse("content_block_start", {{"type", "content_block_start"}, {"index", 0},
                                      {"content_block", {{"type", "text"}, {"text", "Đang "}}}});
        req.onData(first.data(), first.size());
        started = true;
        // Simulate a slow stream that only ends when the token aborts it.
        for (int i = 0; i < 2000 && !(req.cancel && req.cancel->isCancelled()); ++i)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        r.error = "cancelled";
        return r;
    };
    EventLog log;
    AgentSession session([&](const json& e) { log.push(e); },
                         [](const ToolCall&) { return ToolOutcome{true, json::object()}; });
    session.setClient(std::make_shared<LlmClient>(testConfig(), t));
    const auto t0 = std::chrono::steady_clock::now();
    std::thread worker([&] { session.runTurn("viết dài"); });
    for (int i = 0; i < 400 && !started; ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    session.cancel();
    worker.join();
    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - t0).count();
    EXPECT_TRUE(ms < 1500);
    EXPECT_EQ(log.count("agent.cancelled"), 1);
    EXPECT_EQ(log.count("agent.error"), 0);
    EXPECT_TRUE(log.first("agent.cancelled").value("partial", false));
    const json h = session.historySnapshot();
    EXPECT_EQ(h.size(), static_cast<size_t>(2)); // user + "(stopped)" assistant keeps roles valid
    EXPECT_FALSE(session.isBusy());
}

TEST(Agent, CancelTokenRunsAbortOnce) {
    reals::net::CancelToken tok;
    int aborted = 0;
    EXPECT_TRUE(tok.arm([&] { ++aborted; }));
    tok.cancel();
    tok.cancel();
    EXPECT_EQ(aborted, 1);
    EXPECT_TRUE(tok.disarm());
    EXPECT_FALSE(tok.arm([&] { ++aborted; }));
    EXPECT_TRUE(tok.isCancelled());
}

// ---- Reals Lab knowledge / Audio Lab results (P5.02) -------------------------
TEST(Agent, LabKeychordSummary) {
    const json data = {{"job", "keychord"},
                       {"payload", {{"telemetry", {{"bpm", 92.4}, {"master_key", "A"}, {"scale_mode", "Minor"}}},
                                    {"chords", json::array({{{"chord", "Am"}, {"time", 0.0}, {"duration", 2.0}},
                                                            {{"chord", "Am"}, {"time", 2.0}},
                                                            {{"chord", "F"}, {"time", 4.0}, {"roman", "VI"}},
                                                            {{"name", "G"}, {"time", 6.0}}})}}}};
    const json s = reals::bridge::AgentBridge::summarizeLab("midi", data);
    EXPECT_EQ(s.value("job", ""), std::string("keychord"));
    EXPECT_EQ(s.value("key", ""), std::string("A"));
    EXPECT_EQ(s.value("scale", ""), std::string("Minor"));
    EXPECT_EQ(s.value("progression", ""), std::string("Am - F - G"));
    EXPECT_EQ(s["chords"].size(), static_cast<size_t>(4));
    EXPECT_TRUE(s.value("auto", "").find("Chord Track") != std::string::npos);
}

TEST(Agent, LabStemSummaryAndPrompt) {
    const json data = {{"job", "stem"}, {"files", json::array({{{"name", "vocals"}, {"path", "C:/x/v.wav"}}})}};
    const json s = reals::bridge::AgentBridge::summarizeLab("stem", data);
    EXPECT_EQ(s["files"].size(), static_cast<size_t>(1));
    EXPECT_TRUE(s.value("auto", "").find("folder track") != std::string::npos);
    const std::string prompt = AgentSession::systemPrompt();
    EXPECT_TRUE(prompt.find("Chord Track") != std::string::npos);
    ToolRegistry reg;
    EXPECT_TRUE(reg.find("chord_track").has_value());
    EXPECT_TRUE(reg.find("get_lab_result").has_value());
}

TEST(Agent, ContextProviderReachesSystemPrompt) {
    ScriptedLlm llm;
    EventLog log;
    AgentSession session([&](const json& e) { log.push(e); },
                         [](const ToolCall&) { return ToolOutcome{true, json::object()}; });
    session.setClient(std::make_shared<LlmClient>(testConfig(), llm.transport()));
    session.setContextProvider([] { return std::string("- last keychord: key A Minor"); });
    session.runTurn("hợp âm bài này là gì?");
    EXPECT_EQ(llm.requests.size(), static_cast<size_t>(1));
    if (!llm.requests.empty())
        EXPECT_TRUE(llm.requests[0].value("system", "").find("last keychord: key A Minor") != std::string::npos);
}
