// Agent executor dispatcher: JSON in -> REAPER -> JSON out. Mutating tools
// are wrapped in one undo block each so the user can Ctrl+Z any agent step.
#ifdef _WIN32

#include "ReaperAgentTools.h"
#include "ReaperAgentApi.h"

#include <unordered_set>

namespace reals::ext::agent {

namespace {

const HandlerMap& handlers() {
    static const HandlerMap map = [] {
        HandlerMap m;
        registerReadTools(m);
        registerWriteTools(m);
        return m;
    }();
    return map;
}

bool isReadOnly(const std::string& tool) {
    static const std::unordered_set<std::string> kRead = {
        "get_project_info", "list_tracks",       "get_track",         "list_items",       "list_fx",
        "get_fx_params",    "list_markers",      "get_track_property", "get_item_property", "get_action_state"};
    return kRead.count(tool) > 0;
}

// Tools that must not be wrapped in an undo block (they ARE undo/transport).
bool skipsUndoBlock(const std::string& tool) {
    return tool == "undo" || tool == "transport" || tool == "record" || tool == "save_project";
}

std::string fail(const std::string& msg) { return json{{"ok", false}, {"error", msg}}.dump(); }

} // namespace

std::string execute(const std::string& tool, const std::string& argsJson) {
    const auto& map = handlers();
    const auto it = map.find(tool);
    if (it == map.end())
        return fail("unknown tool: " + tool);
    json args = json::parse(argsJson.empty() ? "{}" : argsJson, nullptr, false);
    if (args.is_discarded() || !args.is_object())
        args = json::object();

    const bool wrap = !isReadOnly(tool) && !skipsUndoBlock(tool) && api().Undo_BeginBlock2 &&
                      api().Undo_EndBlock2;
    if (wrap)
        api().Undo_BeginBlock2(nullptr);
    std::string out;
    bool ok = false;
    try {
        json data = it->second(args);
        out = json{{"ok", true}, {"data", data}}.dump();
        ok = true;
    } catch (const std::exception& e) {
        out = fail(e.what());
    } catch (...) {
        out = fail("unknown error");
    }
    if (wrap) {
        const std::string desc = "Reals Agent: " + tool;
        // extraflags -1 = all; on failure nothing changed, the block is empty.
        api().Undo_EndBlock2(nullptr, desc.c_str(), ok ? -1 : 0);
    }
    if (!isReadOnly(tool)) {
        if (api().TrackList_AdjustWindows)
            api().TrackList_AdjustWindows(false);
        if (api().UpdateArrange)
            api().UpdateArrange();
    }
    return out;
}

} // namespace reals::ext::agent

#endif // _WIN32
