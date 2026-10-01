#pragma once

// Streaming decoders for the agent LLM connector (SPEC.md 5.4):
//  - SseParser: incremental text/event-stream framing (chunks may split
//    anywhere, CRLF or LF line endings).
//  - AnthropicStream: assembles Anthropic Messages SSE events into an
//    LlmReply while forwarding text/thinking deltas to a StreamSink.
//  - OpenAiStream: same for OpenAI-compatible chat.completion.chunk events.
// Pure logic, no I/O: unit-tested in TestSuite_Agent.
#include <functional>
#include <map>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

#include "reals/agent/AgentTypes.h"

namespace reals::agent {

// Live callbacks while a reply streams in. Both are optional and run on the
// thread that performs the HTTP request.
struct StreamSink {
    std::function<void(const std::string&)> onText;
    std::function<void(const std::string&)> onThinking;
};

class SseParser {
public:
    using Handler = std::function<void(const std::string& event, const std::string& data)>;

    void feed(const char* data, size_t size, const Handler& handler);
    // Dispatches a trailing event that was not terminated by a blank line.
    void finish(const Handler& handler);

private:
    void processLine(std::string_view line, const Handler& handler);
    void dispatch(const Handler& handler);

    std::string m_buf;
    std::string m_event;
    std::string m_data;
    bool m_hasData = false;
};

// Content blocks that may be sent back to the model: text, tool_use,
// redacted_thinking and thinking blocks that carry a signature (unsigned
// thinking is rejected by the API on replay, so it is display-only).
[[nodiscard]] nlohmann::json replayableContent(const nlohmann::json& blocks);

class AnthropicStream {
public:
    explicit AnthropicStream(const StreamSink* sink = nullptr) : m_sink(sink) {}

    void onEvent(const std::string& event, const std::string& data);
    [[nodiscard]] bool sawEvents() const { return m_saw; }
    [[nodiscard]] LlmReply finish();

private:
    const StreamSink* m_sink;
    std::map<int, nlohmann::json> m_blocks;
    std::map<int, std::string> m_partialJson;
    std::string m_stop;
    std::string m_error;
    bool m_saw = false;
};

class OpenAiStream {
public:
    explicit OpenAiStream(const StreamSink* sink = nullptr) : m_sink(sink) {}

    void onEvent(const std::string& event, const std::string& data);
    [[nodiscard]] bool sawEvents() const { return m_saw; }
    [[nodiscard]] LlmReply finish();

private:
    struct PartialCall {
        std::string id;
        std::string name;
        std::string args;
    };
    const StreamSink* m_sink;
    std::string m_text;
    std::string m_thinking;
    std::map<int, PartialCall> m_calls;
    std::string m_stop;
    std::string m_error;
    bool m_saw = false;
};

} // namespace reals::agent
