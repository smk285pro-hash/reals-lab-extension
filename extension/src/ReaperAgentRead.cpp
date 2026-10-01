// Agent executor: read-only tools (never modify the project).
#ifdef _WIN32

#include "ReaperAgentApi.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>

namespace reals::ext::agent {

namespace {

std::string lower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return s;
}

int resolveCommand(const std::string& cmd) {
    if (cmd.empty())
        throw std::runtime_error("command is required");
    if (std::all_of(cmd.begin(), cmd.end(), [](unsigned char c) { return std::isdigit(c) != 0; }))
        return std::atoi(cmd.c_str());
    const std::string named = cmd[0] == '_' ? cmd : "_" + cmd;
    const int id = RA(NamedCommandLookup)(named.c_str());
    if (id <= 0)
        throw std::runtime_error("unknown action: " + cmd);
    return id;
}

json projectInfo(const json&) {
    json j;
    char name[1024] = {};
    if (api().GetProjectName)
        api().GetProjectName(nullptr, name, sizeof(name));
    j["name"] = name;
    j["tempo"] = RA(Master_GetTempo)();
    const double cursor = RA(GetCursorPosition)();
    if (api().TimeMap_GetTimeSigAtTime) {
        int num = 4, den = 4;
        double tempo = 0.0;
        api().TimeMap_GetTimeSigAtTime(nullptr, cursor, &num, &den, &tempo);
        j["time_signature"] = std::to_string(num) + "/" + std::to_string(den);
    }
    j["length_sec"] = RA(GetProjectLength)(nullptr);
    j["cursor_sec"] = cursor;
    const int ps = RA(GetPlayState)();
    j["play_state"] = (ps & 4) ? "recording" : (ps & 2) ? "paused" : (ps & 1) ? "playing" : "stopped";
    if (ps & 1)
        j["play_position_sec"] = RA(GetPlayPosition)();
    double ls = 0.0, le = 0.0;
    RA(GetSet_LoopTimeRange2)(nullptr, false, true, &ls, &le, false);
    if (le > ls)
        j["loop_range"] = {{"start", ls}, {"end", le}};
    j["track_count"] = RA(CountTracks)(nullptr);
    j["item_count"] = RA(CountMediaItems)(nullptr);
    json selTracks = json::array();
    const int tc = RA(CountTracks)(nullptr);
    for (int i = 0; i < tc; ++i)
        if (MediaTrack* tr = RA(GetTrack)(nullptr, i); tr && RA(GetMediaTrackInfo_Value)(tr, "I_SELECTED") != 0.0)
            selTracks.push_back(i + 1);
    j["selected_tracks"] = selTracks;
    j["selected_item_count"] = RA(CountSelectedMediaItems)(nullptr);
    return j;
}

json listTracks(const json&) {
    json arr = json::array();
    const int n = RA(CountTracks)(nullptr);
    for (int i = 0; i < n && i < 512; ++i)
        if (MediaTrack* tr = RA(GetTrack)(nullptr, i))
            arr.push_back(trackSummary(tr));
    return {{"tracks", arr}, {"count", n}};
}

json fxList(MediaTrack* tr) {
    json arr = json::array();
    const int n = RA(TrackFX_GetCount)(tr);
    for (int i = 0; i < n; ++i) {
        char name[512] = {};
        RA(TrackFX_GetFXName)(tr, i, name, sizeof(name));
        arr.push_back({{"fx", i + 1}, {"name", name},
                       {"enabled", api().TrackFX_GetEnabled ? api().TrackFX_GetEnabled(tr, i) : true}});
    }
    return arr;
}

json getTrack(const json& a) {
    MediaTrack* tr = trackArg(a);
    json j = trackSummary(tr);
    j["fx"] = fxList(tr);
    json sends = json::array();
    if (api().GetTrackNumSends) {
        const int ns = api().GetTrackNumSends(tr, 0);
        for (int i = 0; i < ns; ++i) {
            char nm[512] = {};
            if (api().GetTrackSendName)
                api().GetTrackSendName(tr, i, nm, sizeof(nm));
            sends.push_back({{"send", i + 1}, {"dest", nm}});
        }
    }
    j["sends"] = sends;
    return j;
}

json listItems(const json& a) {
    const bool selectedOnly = argBool(a, "selected_only", false);
    json arr = json::array();
    int first = 0, last = RA(CountTracks)(nullptr) - 1;
    if (hasArg(a, "track")) {
        trackArg(a); // validates
        first = last = argInt(a, "track") - 1;
    }
    for (int t = first; t <= last && t >= 0; ++t) {
        MediaTrack* tr = RA(GetTrack)(nullptr, t);
        if (!tr)
            continue;
        const int n = RA(CountTrackMediaItems)(tr);
        for (int i = 0; i < n && arr.size() < 500; ++i) {
            MediaItem* it = RA(GetTrackMediaItem)(tr, i);
            if (!it)
                continue;
            if (selectedOnly && RA(GetMediaItemInfo_Value)(it, "B_UISEL") == 0.0)
                continue;
            json s = itemSummary(it, t + 1, i + 1);
            s["track_name"] = trackName(tr);
            arr.push_back(s);
        }
    }
    return {{"items", arr}, {"count", arr.size()}};
}

json listFx(const json& a) { return {{"fx", fxList(trackArg(a))}}; }

json fxParams(const json& a) {
    MediaTrack* tr = trackArg(a);
    const int fx = argInt(a, "fx") - 1;
    if (fx < 0 || fx >= RA(TrackFX_GetCount)(tr))
        throw std::runtime_error("fx index out of range");
    const std::string filter = lower(argStr(a, "filter"));
    json arr = json::array();
    const int n = RA(TrackFX_GetNumParams)(tr, fx);
    for (int p = 0; p < n && arr.size() < 128; ++p) {
        char nm[256] = {};
        RA(TrackFX_GetParamName)(tr, fx, p, nm, sizeof(nm));
        if (!filter.empty() && lower(nm).find(filter) == std::string::npos)
            continue;
        char fmt[256] = {};
        if (api().TrackFX_GetFormattedParamValue)
            api().TrackFX_GetFormattedParamValue(tr, fx, p, fmt, sizeof(fmt));
        arr.push_back({{"param", p + 1}, {"name", nm},
                       {"value", RA(TrackFX_GetParamNormalized)(tr, fx, p)}, {"display", fmt}});
    }
    return {{"params", arr}, {"total", n}};
}

json listMarkers(const json&) {
    json arr = json::array();
    for (int i = 0; i < 1000; ++i) {
        bool isRgn = false;
        double pos = 0.0, end = 0.0;
        const char* name = nullptr;
        int num = 0, color = 0;
        if (RA(EnumProjectMarkers3)(nullptr, i, &isRgn, &pos, &end, &name, &num, &color) == 0)
            break;
        json m = {{"number", num}, {"is_region", isRgn}, {"position", pos}, {"name", name ? name : ""}};
        if (isRgn)
            m["end"] = end;
        arr.push_back(m);
    }
    return {{"markers", arr}};
}

json getTrackProp(const json& a) {
    const std::string prop = argStr(a, "prop");
    if (!isAllowedTrackProp(prop))
        throw std::runtime_error("property not allowed: " + prop);
    return {{"prop", prop}, {"value", RA(GetMediaTrackInfo_Value)(trackArg(a), prop.c_str())}};
}

json getItemProp(const json& a) {
    const std::string prop = argStr(a, "prop");
    bool isTake = false;
    if (!isAllowedItemProp(prop, &isTake))
        throw std::runtime_error("property not allowed: " + prop);
    MediaItem* it = itemArg(a);
    const std::string key = prop.rfind("TAKE:", 0) == 0 ? prop.substr(5) : prop;
    const double v = isTake ? RA(GetMediaItemTakeInfo_Value)(activeTake(it), key.c_str())
                            : RA(GetMediaItemInfo_Value)(it, key.c_str());
    return {{"prop", prop}, {"value", v}};
}

json actionState(const json& a) {
    const int id = resolveCommand(argStr(a, "command"));
    return {{"command_id", id}, {"state", RA(GetToggleCommandState)(id)}};
}

} // namespace

int resolveCommandId(const std::string& cmd) { return resolveCommand(cmd); }

void registerReadTools(HandlerMap& m) {
    m["get_project_info"] = projectInfo;
    m["list_tracks"] = listTracks;
    m["get_track"] = getTrack;
    m["list_items"] = listItems;
    m["list_fx"] = listFx;
    m["get_fx_params"] = fxParams;
    m["list_markers"] = listMarkers;
    m["get_track_property"] = getTrackProp;
    m["get_item_property"] = getItemProp;
    m["get_action_state"] = actionState;
}

} // namespace reals::ext::agent

#endif // _WIN32
