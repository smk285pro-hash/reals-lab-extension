#pragma once

// Shared value types for the in-product REAPER agent (SPEC.md 5.4).
// Canonical conversation format = Anthropic Messages (content blocks); the
// OpenAI-compatible connector converts at the wire boundary.
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace reals::agent {

// Risk level of a tool, drives the client permission mode.
enum class ToolRisk { Read = 0, Write = 1, Danger = 2 };

// Client permission modes (SPEC 5.4): 0 ask all, 1 ask dangerous only, 2 full.
enum class PermissionMode { AskAll = 0, AskDangerous = 1, Full = 2 };

struct ToolDef {
    std::string name;
    std::string description;
    nlohmann::json inputSchema = nlohmann::json::object();
    ToolRisk risk = ToolRisk::Write;
};

struct ToolCall {
    std::string id;
    std::string name;
    nlohmann::json input = nlohmann::json::object();
};

struct ToolOutcome {
    bool ok = false;
    nlohmann::json data;  // result payload (ok) or {"error": "..."}
};

struct LlmConfig {
    std::string provider = "anthropic"; // "anthropic" | "openai"
    std::string baseUrl;                // e.g. https://api.anthropic.com
    std::string apiKey;
    std::string model;
    int maxTokens = 4096;
    bool stream = true;              // SSE streaming (text + thinking deltas)
    bool thinking = true;            // extended thinking (Anthropic only)
    int thinkingBudget = 4000;       // budget_tokens when thinking is on (>= 1024)
    std::string toolProtocol = "auto"; // "auto" | "native" | "text" (see LlmClient)
};

struct LlmReply {
    std::string error;                 // non-empty on failure
    nlohmann::json content = nlohmann::json::array(); // Anthropic content blocks
    std::string stopReason;
    std::string thinking;              // visible reasoning text (display only, never replayed)
    [[nodiscard]] std::string text() const;
    [[nodiscard]] std::vector<ToolCall> toolCalls() const;
};

[[nodiscard]] const char* riskName(ToolRisk r);
[[nodiscard]] ToolRisk riskFromName(const std::string& s);

} // namespace reals::agent
