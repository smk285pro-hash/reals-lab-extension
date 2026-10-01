#pragma once

// REAPER executor for the in-product agent (P5). Generic entry point: the
// bridge hands over {tool, argsJson}; tool definitions live in
// core/src/agent/ToolRegistry.cpp (later: server). Must run on REAPER's main
// thread — Bridge marshals calls through its timer-driven queue.
#include <string>

namespace reals::ext::agent {

// Resolve the REAPER API functions used by the agent. Call once from the
// plugin entry point with rec->GetFunc.
void init(void* (*getFunc)(const char* name));

// Execute one tool. Returns {"ok":true,"data":...} | {"ok":false,"error":"..."}.
std::string execute(const std::string& tool, const std::string& argsJson);

} // namespace reals::ext::agent
