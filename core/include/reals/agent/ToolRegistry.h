#pragma once

// Tool catalog for the agent. Definitions are data (JSON), not code: the
// built-in catalog ships with the client and can be overridden by a JSON file
// (later: synced from the RealS server). The host only needs a generic
// executor that receives {tool, args}.
#include <optional>
#include <string>
#include <unordered_set>
#include <vector>

#include "reals/agent/AgentTypes.h"

namespace reals::agent {

class ToolRegistry {
public:
    ToolRegistry();

    // Replace the catalog with a JSON array of tool objects
    // ({name, description, input_schema, risk}). Returns false on bad input.
    bool loadJson(const nlohmann::json& arr);
    bool loadFile(const std::string& utf8Path);

    // Server-side policy: empty = every tool allowed.
    void setAllowedTools(std::vector<std::string> names);

    [[nodiscard]] const std::vector<ToolDef>& all() const { return m_tools; }
    [[nodiscard]] std::vector<ToolDef> allowed() const;
    [[nodiscard]] bool isAllowed(const std::string& name) const;
    [[nodiscard]] std::optional<ToolDef> find(const std::string& name) const;

    // Does this call need a user confirmation under the given mode?
    [[nodiscard]] bool needsConfirm(const std::string& name, PermissionMode mode) const;

    [[nodiscard]] static const nlohmann::json& builtinCatalog();

private:
    std::vector<ToolDef> m_tools;
    std::unordered_set<std::string> m_allowed;
};

} // namespace reals::agent
