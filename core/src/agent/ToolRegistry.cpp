#include "reals/agent/ToolRegistry.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

#include "reals/platform/Path.h"

namespace reals::agent {

using json = nlohmann::json;

namespace {

// Built-in catalog (generated from the tool spec). One small literal per tool
// keeps every string far below MSVC's literal size limit.
const char* const kBuiltinTools[] = {
        R"J({"name":"get_project_info","risk":"read","description":"Project overview: name, tempo, time signature, length, edit cursor, play state, loop range, track/item counts, selected tracks/items.","input_schema":{"type":"object","properties":{},"required":[]}})J",
        R"J({"name":"list_tracks","risk":"read","description":"List all tracks with number, name, volume dB, pan, mute, solo, rec-arm, selected, folder depth, FX count and item count.","input_schema":{"type":"object","properties":{},"required":[]}})J",
        R"J({"name":"get_track","risk":"read","description":"Detailed info for one track including its FX chain and sends.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"}},"required":["track"]}})J",
        R"J({"name":"list_items","risk":"read","description":"List media items (optionally only one track or only selected items) with position, length, mute, take name and source file path.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Restrict to this track number (optional)"},"selected_only":{"type":"boolean","description":"Only selected items"}},"required":[]}})J",
        R"J({"name":"list_fx","risk":"read","description":"List the FX on a track with index (1-based), name and enabled state.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"}},"required":["track"]}})J",
        R"J({"name":"get_fx_params","risk":"read","description":"List parameters of one FX: index (1-based), name, normalized value 0..1 and formatted value. Max 128 params.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"fx":{"type":"integer","description":"FX index, 1-based"},"filter":{"type":"string","description":"Optional case-insensitive substring to filter parameter names"}},"required":["track","fx"]}})J",
        R"J({"name":"list_markers","risk":"read","description":"List project markers and regions.","input_schema":{"type":"object","properties":{},"required":[]}})J",
        R"J({"name":"get_track_property","risk":"read","description":"Read a raw track property. Allowed: D_VOL, D_PAN, D_WIDTH, B_MUTE, I_SOLO, I_RECARM, I_RECMON, I_RECINPUT, B_PHASE, I_FOLDERDEPTH, B_SHOWINTCP, B_SHOWINMIXERCP, I_SELECTED, D_PANLAW, B_MAINSEND, I_CUSTOMCOLOR.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"prop":{"type":"string","description":"Property name"}},"required":["track","prop"]}})J",
        R"J({"name":"get_item_property","risk":"read","description":"Read a raw item property. Allowed: D_POSITION, D_LENGTH, D_VOL, B_MUTE, D_FADEINLEN, D_FADEOUTLEN, D_SNAPOFFSET, B_LOOPSRC, C_LOCK, B_UISEL, D_PLAYRATE (take), D_PITCH (take), B_PPITCH (take).","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"item":{"type":"integer","description":"Item number within the track, 1-based, ordered by position"},"prop":{"type":"string","description":"Property name"}},"required":["track","item","prop"]}})J",
        R"J({"name":"get_action_state","risk":"read","description":"Get the toggle state of a REAPER action (1 on, 0 off, -1 not a toggle). Accepts numeric command id or named id like _SWS_ABOUT.","input_schema":{"type":"object","properties":{"command":{"type":"string","description":"Numeric command id or named command id"}},"required":["command"]}})J",
        R"J({"name":"create_track","risk":"write","description":"Insert a new track.","input_schema":{"type":"object","properties":{"name":{"type":"string","description":"Track name"},"index":{"type":"integer","description":"Insert position, 1-based (default: end)"},"color":{"type":"string","description":"Optional hex color like #ff8800"}},"required":[]}})J",
        R"J({"name":"rename_track","risk":"write","description":"Rename a track.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"name":{"type":"string","description":"New name"}},"required":["track","name"]}})J",
        R"J({"name":"set_track_volume","risk":"write","description":"Set track volume in dB (0 = unity, -inf..+12).","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"db":{"type":"number","description":"Volume in dB"}},"required":["track","db"]}})J",
        R"J({"name":"set_track_pan","risk":"write","description":"Set track pan from -1 (left) to 1 (right).","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"pan":{"type":"number","description":"Pan -1..1"}},"required":["track","pan"]}})J",
        R"J({"name":"set_track_mute","risk":"write","description":"Mute or unmute a track.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"mute":{"type":"boolean","description":"true = muted"}},"required":["track","mute"]}})J",
        R"J({"name":"set_track_solo","risk":"write","description":"Solo or unsolo a track.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"solo":{"type":"boolean","description":"true = solo"}},"required":["track","solo"]}})J",
        R"J({"name":"set_track_arm","risk":"write","description":"Arm or disarm a track for recording.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"arm":{"type":"boolean","description":"true = armed"}},"required":["track","arm"]}})J",
        R"J({"name":"set_track_color","risk":"write","description":"Set a track color.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"color":{"type":"string","description":"Hex color like #ff8800"}},"required":["track","color"]}})J",
        R"J({"name":"select_tracks","risk":"write","description":"Select tracks.","input_schema":{"type":"object","properties":{"tracks":{"type":"array","items":{"type":"integer"},"description":"Track numbers"},"exclusive":{"type":"boolean","description":"Deselect all other tracks first (default true)"}},"required":["tracks"]}})J",
        R"J({"name":"set_track_property","risk":"write","description":"Set a raw track property (same whitelist as get_track_property).","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"prop":{"type":"string","description":"Property name"},"value":{"type":"number","description":"New value"}},"required":["track","prop","value"]}})J",
        R"J({"name":"create_send","risk":"write","description":"Create a send from one track to another.","input_schema":{"type":"object","properties":{"from":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"to":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"}},"required":["from","to"]}})J",
        R"J({"name":"set_folder","risk":"write","description":"Make a range of tracks a folder: the first track becomes the folder parent, the last closes it.","input_schema":{"type":"object","properties":{"first":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"last":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"}},"required":["first","last"]}})J",
        R"J({"name":"set_tempo","risk":"write","description":"Set the project tempo in BPM.","input_schema":{"type":"object","properties":{"bpm":{"type":"number","description":"Tempo 2..960"}},"required":["bpm"]}})J",
        R"J({"name":"set_time_signature","risk":"write","description":"Set the project time signature (first tempo marker).","input_schema":{"type":"object","properties":{"numerator":{"type":"integer","description":"Beats per bar"},"denominator":{"type":"integer","description":"Beat unit"}},"required":["numerator","denominator"]}})J",
        R"J({"name":"set_cursor","risk":"write","description":"Move the edit cursor to a time in seconds.","input_schema":{"type":"object","properties":{"time":{"type":"number","description":"Seconds"}},"required":["time"]}})J",
        R"J({"name":"set_loop_range","risk":"write","description":"Set the loop/time selection range in seconds.","input_schema":{"type":"object","properties":{"start":{"type":"number","description":"Start seconds"},"end":{"type":"number","description":"End seconds"}},"required":["start","end"]}})J",
        R"J({"name":"transport","risk":"write","description":"Control the transport: play, stop, pause.","input_schema":{"type":"object","properties":{"action":{"type":"string","enum":["play","stop","pause"]}},"required":["action"]}})J",
        R"J({"name":"add_marker","risk":"write","description":"Add a project marker.","input_schema":{"type":"object","properties":{"time":{"type":"number","description":"Seconds"},"name":{"type":"string","description":"Marker name"}},"required":["time"]}})J",
        R"J({"name":"add_region","risk":"write","description":"Add a project region.","input_schema":{"type":"object","properties":{"start":{"type":"number","description":"Start seconds"},"end":{"type":"number","description":"End seconds"},"name":{"type":"string","description":"Region name"}},"required":["start","end"]}})J",
        R"J({"name":"add_fx","risk":"write","description":"Add an FX to a track by name (e.g. 'ReaEQ', 'ReaComp', 'VST3: Pro-Q 3'). Returns the new FX index.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"fx_name":{"type":"string","description":"Plugin name as in the REAPER FX browser"}},"required":["track","fx_name"]}})J",
        R"J({"name":"set_fx_enabled","risk":"write","description":"Enable or bypass an FX.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"fx":{"type":"integer","description":"FX index, 1-based"},"enabled":{"type":"boolean","description":"true = enabled"}},"required":["track","fx","enabled"]}})J",
        R"J({"name":"set_fx_param","risk":"write","description":"Set an FX parameter by normalized value 0..1. Use get_fx_params first to find the index.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"fx":{"type":"integer","description":"FX index, 1-based"},"param":{"type":"integer","description":"Parameter index, 1-based"},"value":{"type":"number","description":"Normalized 0..1"}},"required":["track","fx","param","value"]}})J",
        R"J({"name":"insert_media","risk":"write","description":"Insert an audio/MIDI file at the edit cursor (on a new track by default).","input_schema":{"type":"object","properties":{"path":{"type":"string","description":"Absolute file path"},"new_track":{"type":"boolean","description":"Insert on a new track (default true)"}},"required":["path"]}})J",
        R"J({"name":"select_items","risk":"write","description":"Select media items.","input_schema":{"type":"object","properties":{"items":{"type":"array","items":{"type":"object","properties":{"track":{"type":"integer"},"item":{"type":"integer"}},"required":["track","item"]},"description":"Items to select"},"exclusive":{"type":"boolean","description":"Deselect all other items first (default true)"}},"required":["items"]}})J",
        R"J({"name":"split_item","risk":"write","description":"Split an item at a time (seconds, project time).","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"item":{"type":"integer","description":"Item number within the track, 1-based, ordered by position"},"time":{"type":"number","description":"Split time in seconds"}},"required":["track","item","time"]}})J",
        R"J({"name":"move_item","risk":"write","description":"Move an item to a new position in seconds.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"item":{"type":"integer","description":"Item number within the track, 1-based, ordered by position"},"position":{"type":"number","description":"Seconds"}},"required":["track","item","position"]}})J",
        R"J({"name":"set_item_length","risk":"write","description":"Set an item's length in seconds.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"item":{"type":"integer","description":"Item number within the track, 1-based, ordered by position"},"length":{"type":"number","description":"Seconds"}},"required":["track","item","length"]}})J",
        R"J({"name":"set_item_volume","risk":"write","description":"Set item volume in dB.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"item":{"type":"integer","description":"Item number within the track, 1-based, ordered by position"},"db":{"type":"number","description":"dB"}},"required":["track","item","db"]}})J",
        R"J({"name":"set_item_mute","risk":"write","description":"Mute or unmute an item.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"item":{"type":"integer","description":"Item number within the track, 1-based, ordered by position"},"mute":{"type":"boolean","description":"true = muted"}},"required":["track","item","mute"]}})J",
        R"J({"name":"set_item_fades","risk":"write","description":"Set fade-in/fade-out lengths in seconds.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"item":{"type":"integer","description":"Item number within the track, 1-based, ordered by position"},"fade_in":{"type":"number","description":"Seconds"},"fade_out":{"type":"number","description":"Seconds"}},"required":["track","item"]}})J",
        R"J({"name":"set_item_pitch","risk":"write","description":"Set take pitch in semitones and/or playrate.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"item":{"type":"integer","description":"Item number within the track, 1-based, ordered by position"},"semitones":{"type":"number","description":"Pitch shift"},"playrate":{"type":"number","description":"Playrate (1 = normal)"}},"required":["track","item"]}})J",
        R"J({"name":"set_item_property","risk":"write","description":"Set a raw item property (same whitelist as get_item_property).","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"item":{"type":"integer","description":"Item number within the track, 1-based, ordered by position"},"prop":{"type":"string","description":"Property name"},"value":{"type":"number","description":"New value"}},"required":["track","item","prop","value"]}})J",
        R"J({"name":"audio_lab_job","risk":"write","description":"Run a Reals Lab Audio Lab job (cloud GPU) on an audio file. analyze = BPM + key; keychord = key + timed chord progression (Reals Lab then automatically shows it in its Chord Track dock at the top of REAPER); stem = Demucs v4 separation (stems are automatically inserted into a new folder track below the original item, original muted); denoise = cleaned copy (not inserted automatically, use insert_media). By default waits for the job and returns the real result. Get path/position/length from list_items.","input_schema":{"type":"object","properties":{"job":{"type":"string","enum":["denoise","stem","analyze","keychord"]},"path":{"type":"string","description":"Absolute source file path (item source from list_items)"},"position":{"type":"number","description":"Project position (seconds) of the item the file belongs to, so chords/stems line up with it"},"length":{"type":"number","description":"Item length in seconds"},"strength":{"type":"integer","description":"Denoise strength 0..100 (default 80)"},"stems":{"type":"integer","description":"Stem mode 2/4/6/8 (default 4)"},"wait":{"type":"boolean","description":"Wait for the result (default true). false = start and return immediately; fetch later with get_lab_result"}},"required":["job","path"]}})J",
        R"J({"name":"get_lab_result","risk":"read","description":"Latest Audio Lab results known to Reals Lab in this session (key/BPM, chord progression with times, stem/denoise file paths) and any job still running. Use it instead of guessing when the user refers to an earlier analysis.","input_schema":{"type":"object","properties":{"job":{"type":"string","enum":["analyze","keychord","stem","denoise"],"description":"Only this job (optional)"}},"required":[]}})J",
        R"J({"name":"chord_track","risk":"write","description":"Reals Lab Chord Track. show = open the Chord Track dock at the top of REAPER with a progression (synced to the playhead); hide = close the dock; insert_midi = create/update the MIDI track named CHORD TRACK containing the chord notes. Uses the latest keychord result unless chords are given.","input_schema":{"type":"object","properties":{"action":{"type":"string","enum":["show","hide","insert_midi"]},"position":{"type":"number","description":"Project position (seconds) where chord time 0 sits (default: analyzed item position)"},"chords":{"type":"array","description":"Optional custom progression","items":{"type":"object","properties":{"chord":{"type":"string"},"time":{"type":"number","description":"Start (s, relative to position)"},"duration":{"type":"number"}},"required":["chord","time"]}},"bpm":{"type":"number"},"key":{"type":"string","description":"Root, e.g. A"},"scale":{"type":"string","description":"Major or Minor"}},"required":["action"]}})J",
        R"J({"name":"undo","risk":"write","description":"Undo the last action in REAPER.","input_schema":{"type":"object","properties":{},"required":[]}})J",
        R"J({"name":"delete_track","risk":"danger","description":"Delete a track and everything on it.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"}},"required":["track"]}})J",
        R"J({"name":"delete_item","risk":"danger","description":"Delete a media item.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"item":{"type":"integer","description":"Item number within the track, 1-based, ordered by position"}},"required":["track","item"]}})J",
        R"J({"name":"remove_fx","risk":"danger","description":"Remove an FX from a track.","input_schema":{"type":"object","properties":{"track":{"type":"integer","description":"Track number, 1-based as shown in REAPER (0 = master track where supported)"},"fx":{"type":"integer","description":"FX index, 1-based"}},"required":["track","fx"]}})J",
        R"J({"name":"remove_marker","risk":"danger","description":"Delete a marker or region by its displayed number.","input_schema":{"type":"object","properties":{"number":{"type":"integer","description":"Marker/region number"},"is_region":{"type":"boolean","description":"true for a region"}},"required":["number"]}})J",
        R"J({"name":"run_action","risk":"danger","description":"Run any REAPER action by numeric command id (e.g. 40001) or named id (e.g. _SWS_SAVESEL). Use for anything the semantic tools do not cover.","input_schema":{"type":"object","properties":{"command":{"type":"string","description":"Numeric command id or named command id"},"description":{"type":"string","description":"What the action does, shown to the user"}},"required":["command"]}})J",
        R"J({"name":"record","risk":"danger","description":"Start recording on armed tracks.","input_schema":{"type":"object","properties":{},"required":[]}})J",
        R"J({"name":"save_project","risk":"danger","description":"Save the current project file.","input_schema":{"type":"object","properties":{},"required":[]}})J",
};

ToolDef toolFromJson(const json& j) {
    ToolDef t;
    t.name = j.value("name", "");
    t.description = j.value("description", "");
    if (j.contains("input_schema") && j["input_schema"].is_object())
        t.inputSchema = j["input_schema"];
    else
        t.inputSchema = json{{"type", "object"}, {"properties", json::object()}};
    t.risk = riskFromName(j.value("risk", "write"));
    return t;
}

} // namespace

const json& ToolRegistry::builtinCatalog() {
    static const json catalog = [] {
        json arr = json::array();
        for (const char* s : kBuiltinTools)
            arr.push_back(json::parse(s));
        return arr;
    }();
    return catalog;
}

ToolRegistry::ToolRegistry() { loadJson(builtinCatalog()); }

bool ToolRegistry::loadJson(const json& arr) {
    if (!arr.is_array() || arr.empty())
        return false;
    std::vector<ToolDef> next;
    next.reserve(arr.size());
    for (const auto& j : arr) {
        if (!j.is_object())
            return false;
        ToolDef t = toolFromJson(j);
        if (t.name.empty())
            return false;
        next.push_back(std::move(t));
    }
    m_tools = std::move(next);
    return true;
}

bool ToolRegistry::loadFile(const std::string& utf8Path) {
    std::ifstream in(platform::u8path(utf8Path), std::ios::binary);
    if (!in)
        return false;
    std::stringstream ss;
    ss << in.rdbuf();
    const json j = json::parse(ss.str(), nullptr, false);
    if (j.is_discarded())
        return false;
    return loadJson(j.is_object() && j.contains("tools") ? j["tools"] : j);
}

void ToolRegistry::setAllowedTools(std::vector<std::string> names) {
    m_allowed.clear();
    for (auto& n : names)
        if (!n.empty())
            m_allowed.insert(std::move(n));
}

bool ToolRegistry::isAllowed(const std::string& name) const {
    if (!find(name))
        return false;
    return m_allowed.empty() || m_allowed.count(name) > 0;
}

std::vector<ToolDef> ToolRegistry::allowed() const {
    std::vector<ToolDef> out;
    for (const auto& t : m_tools)
        if (m_allowed.empty() || m_allowed.count(t.name) > 0)
            out.push_back(t);
    return out;
}

std::optional<ToolDef> ToolRegistry::find(const std::string& name) const {
    const auto it = std::find_if(m_tools.begin(), m_tools.end(),
                                 [&](const ToolDef& t) { return t.name == name; });
    if (it == m_tools.end())
        return std::nullopt;
    return *it;
}

bool ToolRegistry::needsConfirm(const std::string& name, PermissionMode mode) const {
    const auto t = find(name);
    const ToolRisk risk = t ? t->risk : ToolRisk::Danger; // unknown = dangerous
    switch (mode) {
    case PermissionMode::Full: return false;
    case PermissionMode::AskAll: return risk != ToolRisk::Read;
    case PermissionMode::AskDangerous: break;
    }
    return risk == ToolRisk::Danger;
}

} // namespace reals::agent
