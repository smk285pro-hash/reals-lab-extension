#pragma once

// LLM connector for the agent. Speaks Anthropic Messages natively and the
// OpenAI-compatible chat/completions protocol via conversion, so the endpoint
// can point at api.anthropic.com, a compatible proxy, or the RealS server.
// Network goes through net::HttpClient only (AGENTS.md).
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "reals/agent/AgentTypes.h"
#include "reals/agent/LlmStream.h"
#include "reals/net/HttpClient.h"

namespace reals::agent {

class LlmClient {
public:
    using Transport = std::function<net::Response(const net::Request&)>;

    explicit LlmClient(LlmConfig cfg, Transport transport = {});

    // One round trip. messages use Anthropic format. When cfg.stream is on
    // and a sink is given, text/thinking deltas are forwarded live. cancel
    // aborts the HTTP request from another thread (reply.error="cancelled").
    //
    // Tool protocol (cfg.toolProtocol): "native" sends tool_use/tool_result
    // blocks as-is; "text" re-encodes past tool calls/results as tagged text
    // (for proxies that reject tool_result blocks); "auto" starts native and
    // falls back to text once per endpoint when a request carrying
    // tool_result fails with HTTP 400/5xx.
    [[nodiscard]] LlmReply chat(const std::string& system, const nlohmann::json& messages,
                                const std::vector<ToolDef>& tools,
                                const StreamSink* sink = nullptr,
                                const std::shared_ptr<net::CancelToken>& cancel = {}) const;

    [[nodiscard]] const LlmConfig& config() const { return m_cfg; }

    // Exposed for tests.
    [[nodiscard]] static std::string endpointFor(const LlmConfig& cfg);
    [[nodiscard]] static nlohmann::json toOpenAiMessages(const std::string& system,
                                                         const nlohmann::json& messages);
    [[nodiscard]] static LlmReply parseAnthropic(const nlohmann::json& body);
    [[nodiscard]] static LlmReply parseOpenAi(const nlohmann::json& body);
    [[nodiscard]] static nlohmann::json toTextToolProtocol(const nlohmann::json& messages);
    [[nodiscard]] static bool hasToolResults(const nlohmann::json& messages);
    // Forget endpoints remembered as text-protocol-only (tests).
    static void resetProtocolMemory();

private:
    [[nodiscard]] LlmReply roundTrip(const std::string& system, const nlohmann::json& messages,
                                     const std::vector<ToolDef>& tools, bool textProtocol,
                                     const StreamSink* sink,
                                     const std::shared_ptr<net::CancelToken>& cancel,
                                     long& httpStatus) const;

    LlmConfig m_cfg;
    Transport m_transport;
};

} // namespace reals::agent
