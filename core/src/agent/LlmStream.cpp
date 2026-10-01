#include "reals/agent/LlmStream.h"

#include <utility>

namespace reals::agent {

using json = nlohmann::json;

// ---- SseParser -----------------------------------------------------------------
void SseParser::feed(const char* data, size_t size, const Handler& handler) {
    m_buf.append(data, size);
    size_t start = 0;
    for (;;) {
        const size_t nl = m_buf.find('\n', start);
        if (nl == std::string::npos)
            break;
        size_t end = nl;
        if (end > start && m_buf[end - 1] == '\r')
            --end;
        processLine(std::string_view(m_buf).substr(start, end - start), handler);
        start = nl + 1;
    }
    m_buf.erase(0, start);
}

void SseParser::finish(const Handler& handler) {
    if (!m_buf.empty()) {
        std::string rest;
        rest.swap(m_buf);
        if (!rest.empty() && rest.back() == '\r')
            rest.pop_back();
        processLine(rest, handler);
    }
    dispatch(handler);
}

void SseParser::processLine(std::string_view line, const Handler& handler) {
    if (line.empty()) {
        dispatch(handler);
        return;
    }
    if (line.front() == ':')
        return; // comment / keep-alive
    const size_t colon = line.find(':');
    std::string_view field = line.substr(0, colon);
    std::string_view value;
    if (colon != std::string_view::npos) {
        value = line.substr(colon + 1);
        if (!value.empty() && value.front() == ' ')
            value.remove_prefix(1);
    }
    if (field == "event") {
        m_event.assign(value);
    } else if (field == "data") {
        if (m_hasData)
            m_data += '\n';
        m_data.append(value);
        m_hasData = true;
    }
}

void SseParser::dispatch(const Handler& handler) {
    if (m_hasData && handler)
        handler(m_event, m_data);
    m_event.clear();
    m_data.clear();
    m_hasData = false;
}

// ---- helpers -------------------------------------------------------------------
json replayableContent(const json& blocks) {
    json out = json::array();
    if (!blocks.is_array())
        return out;
    for (const auto& b : blocks) {
        const std::string t = b.value("type", "");
        if (t == "text" || t == "tool_use" || t == "redacted_thinking")
            out.push_back(b);
        else if (t == "thinking" && !b.value("signature", "").empty())
            out.push_back(b);
    }
    return out;
}

namespace {

std::string errorMessage(const json& j) {
    if (j.contains("error")) {
        const auto& e = j["error"];
        if (e.is_object())
            return e.value("message", e.dump());
        if (e.is_string())
            return e.get<std::string>();
    }
    return j.dump();
}

void emit(const std::function<void(const std::string&)>& fn, const std::string& s) {
    if (fn && !s.empty())
        fn(s);
}

} // namespace

// ---- AnthropicStream -----------------------------------------------------------
void AnthropicStream::onEvent(const std::string& event, const std::string& data) {
    const json j = json::parse(data, nullptr, false);
    if (j.is_discarded() || !j.is_object())
        return;
    m_saw = true;
    const std::string type = j.value("type", event);

    if (type == "error") {
        m_error = errorMessage(j);
    } else if (type == "content_block_start") {
        const int idx = j.value("index", 0);
        json block = j.value("content_block", json::object());
        const std::string bt = block.value("type", "");
        if (bt == "tool_use")
            block["input"] = json::object(); // filled from input_json_delta
        m_blocks[idx] = std::move(block);
        if (bt == "text")
            emit(m_sink ? m_sink->onText : nullptr, m_blocks[idx].value("text", ""));
        else if (bt == "thinking")
            emit(m_sink ? m_sink->onThinking : nullptr, m_blocks[idx].value("thinking", ""));
    } else if (type == "content_block_delta") {
        const int idx = j.value("index", 0);
        const json delta = j.value("delta", json::object());
        const std::string dt = delta.value("type", "");
        json& block = m_blocks[idx];
        if (dt == "text_delta") {
            const std::string t = delta.value("text", "");
            if (!block.contains("type"))
                block["type"] = "text";
            block["text"] = block.value("text", "") + t;
            emit(m_sink ? m_sink->onText : nullptr, t);
        } else if (dt == "thinking_delta") {
            const std::string t = delta.value("thinking", "");
            if (!block.contains("type"))
                block["type"] = "thinking";
            block["thinking"] = block.value("thinking", "") + t;
            emit(m_sink ? m_sink->onThinking : nullptr, t);
        } else if (dt == "signature_delta") {
            block["signature"] = block.value("signature", "") + delta.value("signature", "");
        } else if (dt == "input_json_delta") {
            m_partialJson[idx] += delta.value("partial_json", "");
        }
    } else if (type == "message_delta") {
        const json delta = j.value("delta", json::object());
        if (delta.contains("stop_reason") && delta["stop_reason"].is_string())
            m_stop = delta["stop_reason"].get<std::string>();
    } else if (type == "message_start") {
        const json msg = j.value("message", json::object());
        if (msg.contains("stop_reason") && msg["stop_reason"].is_string())
            m_stop = msg["stop_reason"].get<std::string>();
    }
}

LlmReply AnthropicStream::finish() {
    LlmReply r;
    if (!m_error.empty()) {
        r.error = m_error;
        return r;
    }
    json all = json::array();
    for (auto& [idx, block] : m_blocks) {
        const std::string bt = block.value("type", "");
        if (bt == "tool_use") {
            const auto it = m_partialJson.find(idx);
            if (it != m_partialJson.end() && !it->second.empty()) {
                json input = json::parse(it->second, nullptr, false);
                block["input"] = input.is_object() ? input : json::object();
            }
        } else if (bt == "thinking") {
            if (!r.thinking.empty())
                r.thinking += "\n";
            r.thinking += block.value("thinking", "");
        }
        all.push_back(block);
    }
    r.content = replayableContent(all);
    r.stopReason = m_stop;
    return r;
}

// ---- OpenAiStream --------------------------------------------------------------
void OpenAiStream::onEvent(const std::string&, const std::string& data) {
    if (data == "[DONE]") {
        m_saw = true;
        return;
    }
    const json j = json::parse(data, nullptr, false);
    if (j.is_discarded() || !j.is_object())
        return;
    m_saw = true;
    if (j.contains("error")) {
        m_error = errorMessage(j);
        return;
    }
    if (!j.contains("choices") || !j["choices"].is_array() || j["choices"].empty())
        return;
    const json& choice = j["choices"][0];
    const json delta = choice.value("delta", json::object());
    if (delta.contains("content") && delta["content"].is_string()) {
        const std::string t = delta["content"].get<std::string>();
        m_text += t;
        emit(m_sink ? m_sink->onText : nullptr, t);
    }
    for (const char* key : {"reasoning_content", "reasoning"}) {
        if (delta.contains(key) && delta[key].is_string()) {
            const std::string t = delta[key].get<std::string>();
            m_thinking += t;
            emit(m_sink ? m_sink->onThinking : nullptr, t);
            break;
        }
    }
    if (delta.contains("tool_calls") && delta["tool_calls"].is_array()) {
        for (const auto& tc : delta["tool_calls"]) {
            PartialCall& pc = m_calls[tc.value("index", 0)];
            if (tc.contains("id") && tc["id"].is_string())
                pc.id = tc["id"].get<std::string>();
            const json fn = tc.value("function", json::object());
            if (fn.contains("name") && fn["name"].is_string())
                pc.name += fn["name"].get<std::string>();
            if (fn.contains("arguments") && fn["arguments"].is_string())
                pc.args += fn["arguments"].get<std::string>();
        }
    }
    if (choice.contains("finish_reason") && choice["finish_reason"].is_string()) {
        const std::string fr = choice["finish_reason"].get<std::string>();
        m_stop = fr == "tool_calls" ? "tool_use" : fr;
    }
}

LlmReply OpenAiStream::finish() {
    LlmReply r;
    if (!m_error.empty()) {
        r.error = m_error;
        return r;
    }
    if (!m_text.empty())
        r.content.push_back({{"type", "text"}, {"text", m_text}});
    for (const auto& [idx, pc] : m_calls) {
        json input = json::parse(pc.args.empty() ? "{}" : pc.args, nullptr, false);
        if (input.is_discarded() || !input.is_object())
            input = json::object();
        r.content.push_back({{"type", "tool_use"},
                             {"id", pc.id.empty() ? "call_" + std::to_string(idx) : pc.id},
                             {"name", pc.name},
                             {"input", input}});
    }
    r.thinking = m_thinking;
    r.stopReason = m_stop;
    return r;
}

} // namespace reals::agent
