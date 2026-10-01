#include "reals/agent/LlmClient.h"

#include <algorithm>
#include <mutex>
#include <set>
#include <utility>

namespace reals::agent {

using json = nlohmann::json;

// ---- AgentTypes helpers ------------------------------------------------------
std::string LlmReply::text() const {
    std::string out;
    if (!content.is_array())
        return out;
    for (const auto& b : content) {
        if (b.value("type", "") == "text") {
            if (!out.empty())
                out += "\n";
            out += b.value("text", "");
        }
    }
    return out;
}

std::vector<ToolCall> LlmReply::toolCalls() const {
    std::vector<ToolCall> out;
    if (!content.is_array())
        return out;
    for (const auto& b : content) {
        if (b.value("type", "") != "tool_use")
            continue;
        ToolCall c;
        c.id = b.value("id", "");
        c.name = b.value("name", "");
        c.input = b.contains("input") && b["input"].is_object() ? b["input"] : json::object();
        out.push_back(std::move(c));
    }
    return out;
}

const char* riskName(ToolRisk r) {
    switch (r) {
    case ToolRisk::Read: return "read";
    case ToolRisk::Danger: return "danger";
    case ToolRisk::Write: break;
    }
    return "write";
}

ToolRisk riskFromName(const std::string& s) {
    if (s == "read")
        return ToolRisk::Read;
    if (s == "danger")
        return ToolRisk::Danger;
    return ToolRisk::Write;
}

// ---- LlmClient ---------------------------------------------------------------
namespace {

std::string trimSlash(std::string s) {
    while (!s.empty() && s.back() == '/')
        s.pop_back();
    return s;
}

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

std::string extractError(const net::Response& r) {
    std::string msg = "HTTP " + std::to_string(r.statusCode);
    const json j = json::parse(r.body, nullptr, false);
    if (!j.is_discarded() && j.is_object() && j.contains("error")) {
        const auto& e = j["error"];
        if (e.is_object())
            msg += ": " + e.value("message", e.dump());
        else if (e.is_string())
            msg += ": " + e.get<std::string>();
    } else if (!r.body.empty()) {
        msg += ": " + r.body.substr(0, 300);
    }
    return msg;
}

// Endpoints that rejected native tool_result blocks during this process run.
std::mutex g_textProtoMutex;
std::set<std::string> g_textProtoEndpoints;

bool endpointNeedsText(const std::string& endpoint) {
    const std::lock_guard lock(g_textProtoMutex);
    return g_textProtoEndpoints.count(endpoint) != 0;
}

void rememberTextEndpoint(const std::string& endpoint) {
    const std::lock_guard lock(g_textProtoMutex);
    g_textProtoEndpoints.insert(endpoint);
}

std::string blockContentText(const json& b) {
    if (!b.contains("content"))
        return {};
    const json& c = b["content"];
    if (c.is_string())
        return c.get<std::string>();
    if (c.is_array()) { // [{type:text,text:...}] form
        std::string out;
        for (const auto& part : c) {
            if (part.is_object() && part.value("type", "") == "text") {
                if (!out.empty())
                    out += "\n";
                out += part.value("text", "");
            } else {
                out += part.dump();
            }
        }
        return out;
    }
    return c.dump();
}

std::string attrEscape(const std::string& s) {
    std::string out;
    for (const char ch : s) {
        if (ch == '"')
            out += "&quot;";
        else if (ch == '<')
            out += "&lt;";
        else if (ch == '>')
            out += "&gt;";
        else if (ch == '&')
            out += "&amp;";
        else
            out += ch;
    }
    return out;
}

constexpr const char* kTextProtocolNote =
    "\n\n[Tool transport] Earlier tool calls appear in this conversation as "
    "<tool_call> text and their results arrive as <tool_result> text inside user "
    "turns. Treat those results as real tool output. Keep calling tools through the "
    "normal tool-use mechanism; never write <tool_call> or <tool_result> tags yourself.";

// Anthropic rejects thinking + tool history unless every assistant tool_use
// turn starts with a (signed) thinking block. Unsigned thinking is never
// replayed, so only enable thinking when the native history allows it.
bool historyAllowsThinking(const json& messages) {
    for (const auto& m : messages) {
        if (m.value("role", "") != "assistant" || !m.contains("content") ||
            !m["content"].is_array())
            continue;
        bool hasTool = false;
        for (const auto& b : m["content"])
            hasTool = hasTool || b.value("type", "") == "tool_use";
        if (!hasTool)
            continue;
        const auto& first = m["content"].empty() ? json() : m["content"][0];
        const std::string t = first.is_object() ? first.value("type", "") : "";
        if (t != "thinking" && t != "redacted_thinking")
            return false;
    }
    return true;
}

} // namespace

json LlmClient::toTextToolProtocol(const json& messages) {
    json out = json::array();
    for (const auto& m : messages) {
        const std::string role = m.value("role", "user");
        std::string text;
        if (m.contains("content") && m["content"].is_string()) {
            text = m["content"].get<std::string>();
        } else if (m.contains("content") && m["content"].is_array()) {
            for (const auto& b : m["content"]) {
                const std::string t = b.value("type", "");
                std::string piece;
                if (t == "text") {
                    piece = b.value("text", "");
                } else if (t == "tool_use") {
                    piece = "<tool_call id=\"" + attrEscape(b.value("id", "")) + "\" name=\"" +
                            attrEscape(b.value("name", "")) + "\">" +
                            (b.contains("input") ? b["input"].dump() : std::string("{}")) +
                            "</tool_call>";
                } else if (t == "tool_result") {
                    const bool isError = b.value("is_error", false);
                    piece = "<tool_result id=\"" + attrEscape(b.value("tool_use_id", "")) +
                            "\" ok=\"" + (isError ? "false" : "true") + "\">" +
                            blockContentText(b) + "</tool_result>";
                } // thinking / redacted_thinking: dropped (not replayable as text)
                if (piece.empty())
                    continue;
                if (!text.empty())
                    text += "\n";
                text += piece;
            }
        }
        if (text.empty())
            text = "(empty)";
        // Merge consecutive same-role turns so roles keep alternating.
        if (!out.empty() && out.back().value("role", "") == role) {
            out.back()["content"] = out.back()["content"].get<std::string>() + "\n" + text;
        } else {
            out.push_back({{"role", role}, {"content", text}});
        }
    }
    return out;
}

bool LlmClient::hasToolResults(const json& messages) {
    for (const auto& m : messages) {
        if (!m.contains("content") || !m["content"].is_array())
            continue;
        for (const auto& b : m["content"])
            if (b.value("type", "") == "tool_result")
                return true;
    }
    return false;
}

void LlmClient::resetProtocolMemory() {
    const std::lock_guard lock(g_textProtoMutex);
    g_textProtoEndpoints.clear();
}

LlmClient::LlmClient(LlmConfig cfg, Transport transport)
    : m_cfg(std::move(cfg)), m_transport(std::move(transport)) {
    if (!m_transport) {
#ifdef _WIN32
        m_transport = [](const net::Request& req) { return net::HttpClient::instance().send(req); };
#else
        m_transport = [](const net::Request&) {
            net::Response r;
            r.error = "no network transport on this platform yet (P6)";
            return r;
        };
#endif
    }
}

std::string LlmClient::endpointFor(const LlmConfig& cfg) {
    std::string base = trimSlash(cfg.baseUrl);
    if (cfg.provider == "openai") {
        if (endsWith(base, "/chat/completions"))
            return base;
        if (!endsWith(base, "/v1"))
            base += "/v1";
        return base + "/chat/completions";
    }
    if (base.empty())
        base = "https://api.anthropic.com";
    if (endsWith(base, "/messages"))
        return base;
    if (!endsWith(base, "/v1"))
        base += "/v1";
    return base + "/messages";
}

json LlmClient::toOpenAiMessages(const std::string& system, const json& messages) {
    json out = json::array();
    if (!system.empty())
        out.push_back({{"role", "system"}, {"content", system}});
    for (const auto& m : messages) {
        const std::string role = m.value("role", "user");
        if (m.contains("content") && m["content"].is_string()) {
            out.push_back({{"role", role}, {"content", m["content"]}});
            continue;
        }
        if (!m.contains("content") || !m["content"].is_array())
            continue;
        std::string text;
        json toolCalls = json::array();
        for (const auto& b : m["content"]) {
            const std::string t = b.value("type", "");
            if (t == "text") {
                if (!text.empty())
                    text += "\n";
                text += b.value("text", "");
            } else if (t == "tool_use") {
                toolCalls.push_back({{"id", b.value("id", "")},
                                     {"type", "function"},
                                     {"function",
                                      {{"name", b.value("name", "")},
                                       {"arguments", b.contains("input") ? b["input"].dump() : "{}"}}}});
            } else if (t == "tool_result") {
                std::string content;
                if (b.contains("content"))
                    content = b["content"].is_string() ? b["content"].get<std::string>() : b["content"].dump();
                out.push_back({{"role", "tool"},
                               {"tool_call_id", b.value("tool_use_id", "")},
                               {"content", content}});
            }
        }
        if (role == "assistant") {
            json msg = {{"role", "assistant"}, {"content", text}};
            if (!toolCalls.empty())
                msg["tool_calls"] = toolCalls;
            out.push_back(msg);
        } else if (!text.empty()) {
            out.push_back({{"role", role}, {"content", text}});
        }
    }
    return out;
}

LlmReply LlmClient::parseAnthropic(const json& body) {
    LlmReply r;
    if (!body.is_object() || !body.contains("content") || !body["content"].is_array()) {
        r.error = "unexpected response shape";
        return r;
    }
    for (const auto& b : body["content"]) {
        if (b.value("type", "") == "thinking") {
            if (!r.thinking.empty())
                r.thinking += "\n";
            r.thinking += b.value("thinking", "");
        }
    }
    r.content = replayableContent(body["content"]); // drops unsigned thinking
    r.stopReason = body.value("stop_reason", "");
    return r;
}

LlmReply LlmClient::parseOpenAi(const json& body) {
    LlmReply r;
    if (!body.is_object() || !body.contains("choices") || !body["choices"].is_array() ||
        body["choices"].empty()) {
        r.error = "unexpected response shape";
        return r;
    }
    const auto& choice = body["choices"][0];
    const json msg = choice.value("message", json::object());
    for (const char* key : {"reasoning_content", "reasoning"}) {
        if (msg.contains(key) && msg[key].is_string()) {
            r.thinking = msg[key].get<std::string>();
            break;
        }
    }
    if (msg.contains("content") && msg["content"].is_string() &&
        !msg["content"].get<std::string>().empty())
        r.content.push_back({{"type", "text"}, {"text", msg["content"]}});
    if (msg.contains("tool_calls") && msg["tool_calls"].is_array()) {
        for (const auto& tc : msg["tool_calls"]) {
            const json fn = tc.value("function", json::object());
            json input = json::parse(fn.value("arguments", "{}"), nullptr, false);
            if (input.is_discarded() || !input.is_object())
                input = json::object();
            r.content.push_back({{"type", "tool_use"},
                                 {"id", tc.value("id", "")},
                                 {"name", fn.value("name", "")},
                                 {"input", input}});
        }
    }
    const std::string fr = choice.value("finish_reason", "");
    r.stopReason = fr == "tool_calls" ? "tool_use" : fr;
    return r;
}

LlmReply LlmClient::chat(const std::string& system, const json& messages,
                         const std::vector<ToolDef>& tools, const StreamSink* sink,
                         const std::shared_ptr<net::CancelToken>& cancel) const {
    LlmReply reply;
    if (m_cfg.apiKey.empty()) {
        reply.error = "missing API key";
        return reply;
    }
    if (m_cfg.model.empty()) {
        reply.error = "missing model";
        return reply;
    }

    const std::string endpoint = endpointFor(m_cfg);
    const bool withResults = hasToolResults(messages);
    bool text = m_cfg.toolProtocol == "text" ||
                (m_cfg.toolProtocol == "auto" && withResults && endpointNeedsText(endpoint));

    long status = 0;
    reply = roundTrip(system, messages, tools, text, sink, cancel, status);
    if (!text && withResults && m_cfg.toolProtocol == "auto" && !reply.error.empty() &&
        reply.error != "cancelled" && (status == 400 || status >= 500)) {
        // Some proxies (Cloudflare-fronted relays) 502/400 on tool_result
        // blocks; retry once with the text encoding and remember the endpoint.
        LlmReply retry = roundTrip(system, messages, tools, true, sink, cancel, status);
        if (retry.error.empty())
            rememberTextEndpoint(endpoint);
        if (retry.error.empty() || retry.error == "cancelled")
            return retry;
        reply.error += " | text-protocol retry: " + retry.error;
    }
    return reply;
}

LlmReply LlmClient::roundTrip(const std::string& system, const json& messages,
                              const std::vector<ToolDef>& tools, bool textProtocol,
                              const StreamSink* sink,
                              const std::shared_ptr<net::CancelToken>& cancel,
                              long& httpStatus) const {
    LlmReply reply;
    httpStatus = 0;
    if (cancel && cancel->isCancelled()) {
        reply.error = "cancelled";
        return reply;
    }

    const bool openai = m_cfg.provider == "openai";
    const json wireMessages = textProtocol ? toTextToolProtocol(messages) : messages;
    const std::string wireSystem = textProtocol && hasToolResults(messages)
                                       ? system + kTextProtocolNote
                                       : system;

    net::Request req;
    req.method = "POST";
    req.url = endpointFor(m_cfg);
    req.cancel = cancel;
    // Always send an explicit Authorization header so HttpClient never
    // attaches the reals.media auth token to a third-party endpoint.
    req.headers["Authorization"] = "Bearer " + m_cfg.apiKey;

    json body;
    body["model"] = m_cfg.model;
    body["max_tokens"] = m_cfg.maxTokens;
    if (m_cfg.stream) {
        body["stream"] = true;
        req.headers["Accept"] = "text/event-stream";
    }
    if (openai) {
        body["messages"] = toOpenAiMessages(wireSystem, wireMessages);
        if (!tools.empty()) {
            json arr = json::array();
            for (const auto& t : tools)
                arr.push_back({{"type", "function"},
                               {"function",
                                {{"name", t.name},
                                 {"description", t.description},
                                 {"parameters", t.inputSchema}}}});
            body["tools"] = arr;
        }
    } else {
        req.headers["x-api-key"] = m_cfg.apiKey;
        req.headers["anthropic-version"] = "2023-06-01";
        body["system"] = wireSystem;
        body["messages"] = wireMessages;
        if (m_cfg.thinking && (textProtocol || historyAllowsThinking(messages))) {
            const int budget = std::max(1024, m_cfg.thinkingBudget);
            body["thinking"] = {{"type", "enabled"}, {"budget_tokens", budget}};
            body["max_tokens"] = std::max(m_cfg.maxTokens, budget + 2048);
        }
        if (!tools.empty()) {
            json arr = json::array();
            for (const auto& t : tools)
                arr.push_back({{"name", t.name},
                               {"description", t.description},
                               {"input_schema", t.inputSchema}});
            body["tools"] = arr;
        }
    }
    req.body = body.dump();

    SseParser sse;
    AnthropicStream anthropicStream(sink);
    OpenAiStream openaiStream(sink);
    const SseParser::Handler onEvent = [&](const std::string& ev, const std::string& data) {
        if (openai)
            openaiStream.onEvent(ev, data);
        else
            anthropicStream.onEvent(ev, data);
    };
    std::string raw; // non-SSE fallback (endpoint ignored "stream")
    if (m_cfg.stream) {
        req.onData = [&](const char* p, size_t n) {
            if (cancel && cancel->isCancelled())
                return false;
            if (raw.size() < (size_t{16} << 20))
                raw.append(p, n);
            sse.feed(p, n, onEvent);
            return true;
        };
    }

    const net::Response res = m_transport(req);
    httpStatus = res.statusCode;
    if (cancel && cancel->isCancelled()) {
        reply.error = "cancelled";
        return reply;
    }
    if (!res.error.empty()) {
        reply.error = res.error == "cancelled" ? "cancelled" : "network: " + res.error;
        return reply;
    }
    if (res.statusCode < 200 || res.statusCode >= 300) {
        reply.error = extractError(res);
        return reply;
    }

    if (m_cfg.stream) {
        sse.finish(onEvent);
        const bool saw = openai ? openaiStream.sawEvents() : anthropicStream.sawEvents();
        if (saw)
            return openai ? openaiStream.finish() : anthropicStream.finish();
        // Not SSE: the body was streamed to us anyway; parse it whole.
        const std::string& whole = res.body.empty() ? raw : res.body;
        const json parsed = json::parse(whole, nullptr, false);
        if (parsed.is_discarded()) {
            reply.error = "invalid response from LLM endpoint";
            return reply;
        }
        reply = openai ? parseOpenAi(parsed) : parseAnthropic(parsed);
    } else {
        const json parsed = json::parse(res.body, nullptr, false);
        if (parsed.is_discarded()) {
            reply.error = "invalid JSON from LLM endpoint";
            return reply;
        }
        reply = openai ? parseOpenAi(parsed) : parseAnthropic(parsed);
    }
    if (sink) { // non-streamed body: still surface the content once
        if (sink->onThinking && !reply.thinking.empty())
            sink->onThinking(reply.thinking);
        if (sink->onText && !reply.text().empty())
            sink->onText(reply.text());
    }
    return reply;
}

} // namespace reals::agent
