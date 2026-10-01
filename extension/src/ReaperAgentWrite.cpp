// Agent executor: tools that modify the project. The dispatcher wraps each
// call in an undo block (ReaperAgentTools.cpp), so handlers here only act.
#ifdef _WIN32

#include "ReaperAgentApi.h"

#include <algorithm>
#include <cmath>
#include <string>

namespace reals::ext::agent {

namespace {

constexpr ReaProject* kProj = nullptr; // current project

void setTrack(const json& a, const char* prop, double v) {
    RA(SetMediaTrackInfo_Value)(trackArg(a), prop, v);
}

void setItem(MediaItem* it, const char* prop, double v) { RA(SetMediaItemInfo_Value)(it, prop, v); }

json createTrack(const json& a) {
    const int count = RA(CountTracks)(kProj);
    int idx = argInt(a, "index", count + 1) - 1;
    idx = std::clamp(idx, 0, count);
    RA(InsertTrackAtIndex)(idx, true);
    MediaTrack* tr = RA(GetTrack)(kProj, idx);
    if (!tr)
        throw std::runtime_error("failed to create track");
    std::string name = argStr(a, "name");
    if (!name.empty()) {
        name.resize(std::min<size_t>(name.size(), 1000));
        RA(GetSetMediaTrackInfo_String)(tr, "P_NAME", name.data(), true);
    }
    if (hasArg(a, "color"))
        RA(SetMediaTrackInfo_Value)(tr, "I_CUSTOMCOLOR", parseHexColor(argStr(a, "color")));
    return {{"track", idx + 1}, {"name", name}};
}

json renameTrack(const json& a) {
    MediaTrack* tr = trackArg(a);
    std::string name = argStr(a, "name");
    RA(GetSetMediaTrackInfo_String)(tr, "P_NAME", name.data(), true);
    return {{"track", trackNumber(tr)}, {"name", name}};
}

json selectTracks(const json& a) {
    if (!a.contains("tracks") || !a["tracks"].is_array())
        throw std::runtime_error("tracks must be an array");
    if (argBool(a, "exclusive", true)) {
        const int n = RA(CountTracks)(kProj);
        for (int i = 0; i < n; ++i)
            if (MediaTrack* tr = RA(GetTrack)(kProj, i))
                RA(SetTrackSelected)(tr, false);
    }
    json done = json::array();
    for (const auto& v : a["tracks"]) {
        MediaTrack* tr = trackArg(json{{"track", v}});
        RA(SetTrackSelected)(tr, true);
        done.push_back(trackNumber(tr));
    }
    return {{"selected", done}};
}

json setTrackProp(const json& a) {
    const std::string prop = argStr(a, "prop");
    if (!isAllowedTrackProp(prop))
        throw std::runtime_error("property not allowed: " + prop);
    setTrack(a, prop.c_str(), argNum(a, "value"));
    return {{"prop", prop}, {"value", argNum(a, "value")}};
}

json createSend(const json& a) {
    MediaTrack* from = trackArg(a, "from");
    MediaTrack* to = trackArg(a, "to");
    if (from == to)
        throw std::runtime_error("cannot send a track to itself");
    const int idx = RA(CreateTrackSend)(from, to);
    if (idx < 0)
        throw std::runtime_error("send could not be created");
    return {{"send", idx + 1}};
}

json setFolder(const json& a) {
    MediaTrack* first = trackArg(a, "first");
    MediaTrack* last = trackArg(a, "last");
    const int f = argInt(a, "first"), l = argInt(a, "last");
    if (l <= f)
        throw std::runtime_error("last must be after first");
    RA(SetMediaTrackInfo_Value)(first, "I_FOLDERDEPTH", 1.0);
    const double lastDepth = RA(GetMediaTrackInfo_Value)(last, "I_FOLDERDEPTH");
    RA(SetMediaTrackInfo_Value)(last, "I_FOLDERDEPTH", std::min(lastDepth, 0.0) - 1.0);
    return {{"folder", f}, {"children", l - f}};
}

json setTempo(const json& a) {
    const double bpm = argNum(a, "bpm");
    if (bpm < 2.0 || bpm > 960.0)
        throw std::runtime_error("bpm must be between 2 and 960");
    RA(SetCurrentBPM)(kProj, bpm, false);
    return {{"tempo", RA(Master_GetTempo)()}};
}

json setTimeSig(const json& a) {
    const int num = argInt(a, "numerator"), den = argInt(a, "denominator");
    if (num < 1 || num > 64 || (den != 1 && den != 2 && den != 4 && den != 8 && den != 16 && den != 32))
        throw std::runtime_error("invalid time signature");
    const int idx = RA(CountTempoTimeSigMarkers)(kProj) > 0 ? 0 : -1;
    if (!RA(SetTempoTimeSigMarker)(kProj, idx, 0.0, -1, -1.0, RA(Master_GetTempo)(), num, den, false))
        throw std::runtime_error("failed to set time signature");
    return {{"time_signature", std::to_string(num) + "/" + std::to_string(den)}};
}

json setCursor(const json& a) {
    const double t = std::max(0.0, argNum(a, "time"));
    RA(SetEditCurPos2)(kProj, t, true, true);
    return {{"cursor", t}};
}

json setLoop(const json& a) {
    double s = argNum(a, "start"), e = argNum(a, "end");
    if (e <= s || s < 0)
        throw std::runtime_error("end must be greater than start (>= 0)");
    RA(GetSet_LoopTimeRange2)(kProj, true, true, &s, &e, false);
    return {{"start", s}, {"end", e}};
}

json transport(const json& a) {
    const std::string act = argStr(a, "action");
    if (act == "play")
        RA(OnPlayButton)();
    else if (act == "stop")
        RA(OnStopButton)();
    else if (act == "pause")
        RA(OnPauseButton)();
    else
        throw std::runtime_error("action must be play, stop or pause");
    return {{"action", act}};
}

json addMarker(const json& a, bool region) {
    const std::string name = argStr(a, "name");
    const double pos = region ? argNum(a, "start") : argNum(a, "time");
    const double end = region ? argNum(a, "end") : 0.0;
    if (region && end <= pos)
        throw std::runtime_error("region end must be after start");
    const int num = RA(AddProjectMarker2)(kProj, region, pos, end, name.c_str(), -1, 0);
    if (num < 0)
        throw std::runtime_error("failed to add marker");
    return {{"number", num}, {"is_region", region}};
}

json addFx(const json& a) {
    MediaTrack* tr = trackArg(a);
    const std::string name = argStr(a, "fx_name");
    if (name.empty())
        throw std::runtime_error("fx_name is required");
    const int idx = RA(TrackFX_AddByName)(tr, name.c_str(), false, -1);
    if (idx < 0)
        throw std::runtime_error("FX not found: " + name + " (check the exact name in the FX browser)");
    char real[512] = {};
    RA(TrackFX_GetFXName)(tr, idx, real, sizeof(real));
    return {{"fx", idx + 1}, {"name", real}};
}

int fxIndex(MediaTrack* tr, const json& a) {
    const int fx = argInt(a, "fx") - 1;
    if (fx < 0 || fx >= RA(TrackFX_GetCount)(tr))
        throw std::runtime_error("fx index out of range");
    return fx;
}

json setFxEnabled(const json& a) {
    MediaTrack* tr = trackArg(a);
    const int fx = fxIndex(tr, a);
    RA(TrackFX_SetEnabled)(tr, fx, argBool(a, "enabled", true));
    return {{"fx", fx + 1}, {"enabled", argBool(a, "enabled", true)}};
}

json setFxParam(const json& a) {
    MediaTrack* tr = trackArg(a);
    const int fx = fxIndex(tr, a);
    const int p = argInt(a, "param") - 1;
    if (p < 0 || p >= RA(TrackFX_GetNumParams)(tr, fx))
        throw std::runtime_error("param index out of range");
    const double v = std::clamp(argNum(a, "value"), 0.0, 1.0);
    RA(TrackFX_SetParamNormalized)(tr, fx, p, v);
    char fmt[256] = {};
    if (api().TrackFX_GetFormattedParamValue)
        api().TrackFX_GetFormattedParamValue(tr, fx, p, fmt, sizeof(fmt));
    return {{"fx", fx + 1}, {"param", p + 1}, {"value", v}, {"display", fmt}};
}

json removeFx(const json& a) {
    MediaTrack* tr = trackArg(a);
    const int fx = fxIndex(tr, a);
    if (!RA(TrackFX_Delete)(tr, fx))
        throw std::runtime_error("failed to remove fx");
    return {{"removed_fx", fx + 1}};
}

json insertMedia(const json& a) {
    const std::string path = argStr(a, "path");
    if (path.empty())
        throw std::runtime_error("path is required");
    const DWORD attr = GetFileAttributesA(path.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || (attr & FILE_ATTRIBUTE_DIRECTORY))
        throw std::runtime_error("file not found: " + path);
    const int r = RA(InsertMedia)(path.c_str(), argBool(a, "new_track", true) ? 1 : 0);
    return {{"inserted", path}, {"result", r}};
}

json selectItems(const json& a) {
    if (!a.contains("items") || !a["items"].is_array())
        throw std::runtime_error("items must be an array");
    if (argBool(a, "exclusive", true))
        RA(SelectAllMediaItems)(kProj, false);
    int n = 0;
    for (const auto& v : a["items"]) {
        RA(SetMediaItemSelected)(itemArg(v), true);
        ++n;
    }
    return {{"selected", n}};
}

json splitItem(const json& a) {
    MediaItem* it = itemArg(a);
    const double t = argNum(a, "time");
    const double pos = RA(GetMediaItemInfo_Value)(it, "D_POSITION");
    const double len = RA(GetMediaItemInfo_Value)(it, "D_LENGTH");
    if (t <= pos || t >= pos + len)
        throw std::runtime_error("split time must be inside the item (" + std::to_string(pos) + " .. " +
                                 std::to_string(pos + len) + ")");
    if (!RA(SplitMediaItem)(it, t))
        throw std::runtime_error("split failed");
    return {{"split_at", t}};
}

json moveItem(const json& a) {
    MediaItem* it = itemArg(a);
    const double p = std::max(0.0, argNum(a, "position"));
    setItem(it, "D_POSITION", p);
    return {{"position", p}};
}

json setItemLength(const json& a) {
    const double l = argNum(a, "length");
    if (l <= 0)
        throw std::runtime_error("length must be > 0");
    setItem(itemArg(a), "D_LENGTH", l);
    return {{"length", l}};
}

json setItemVolume(const json& a) {
    const double db = argNum(a, "db");
    setItem(itemArg(a), "D_VOL", fromDb(db));
    return {{"volume_db", db}};
}

json setItemMute(const json& a) {
    const bool m = argBool(a, "mute", true);
    setItem(itemArg(a), "B_MUTE", m ? 1.0 : 0.0);
    return {{"mute", m}};
}

json setItemFades(const json& a) {
    MediaItem* it = itemArg(a);
    json out = json::object();
    if (hasArg(a, "fade_in")) {
        const double v = std::max(0.0, argNum(a, "fade_in"));
        setItem(it, "D_FADEINLEN", v);
        out["fade_in"] = v;
    }
    if (hasArg(a, "fade_out")) {
        const double v = std::max(0.0, argNum(a, "fade_out"));
        setItem(it, "D_FADEOUTLEN", v);
        out["fade_out"] = v;
    }
    if (out.empty())
        throw std::runtime_error("give fade_in and/or fade_out");
    return out;
}

json setItemPitch(const json& a) {
    MediaItem* it = itemArg(a);
    MediaItem_Take* tk = activeTake(it);
    json out = json::object();
    if (hasArg(a, "semitones")) {
        const double st = std::clamp(argNum(a, "semitones"), -48.0, 48.0);
        RA(SetMediaItemTakeInfo_Value)(tk, "D_PITCH", st);
        out["semitones"] = st;
    }
    if (hasArg(a, "playrate")) {
        const double pr = std::clamp(argNum(a, "playrate"), 0.1, 10.0);
        const double oldRate = RA(GetMediaItemTakeInfo_Value)(tk, "D_PLAYRATE");
        RA(SetMediaItemTakeInfo_Value)(tk, "B_PPITCH", 1.0);
        RA(SetMediaItemTakeInfo_Value)(tk, "D_PLAYRATE", pr);
        // Keep the same source content audible: length scales with the rate.
        const double len = RA(GetMediaItemInfo_Value)(it, "D_LENGTH");
        if (oldRate > 0)
            setItem(it, "D_LENGTH", len * oldRate / pr);
        out["playrate"] = pr;
    }
    if (out.empty())
        throw std::runtime_error("give semitones and/or playrate");
    return out;
}

json setItemProp(const json& a) {
    const std::string prop = argStr(a, "prop");
    bool isTake = false;
    if (!isAllowedItemProp(prop, &isTake))
        throw std::runtime_error("property not allowed: " + prop);
    MediaItem* it = itemArg(a);
    const std::string key = prop.rfind("TAKE:", 0) == 0 ? prop.substr(5) : prop;
    const double v = argNum(a, "value");
    if (isTake)
        RA(SetMediaItemTakeInfo_Value)(activeTake(it), key.c_str(), v);
    else
        setItem(it, key.c_str(), v);
    return {{"prop", prop}, {"value", v}};
}

json deleteTrack(const json& a) {
    MediaTrack* tr = trackArg(a);
    if (trackNumber(tr) == 0)
        throw std::runtime_error("cannot delete the master track");
    const std::string name = trackName(tr);
    RA(DeleteTrack)(tr);
    return {{"deleted_track", argInt(a, "track")}, {"name", name}};
}

json deleteItem(const json& a) {
    MediaItem* it = itemArg(a);
    if (!RA(DeleteTrackMediaItem)(RA(GetMediaItem_Track)(it), it))
        throw std::runtime_error("delete failed");
    return {{"deleted", true}};
}

json removeMarker(const json& a) {
    const bool rgn = argBool(a, "is_region", false);
    if (!RA(DeleteProjectMarker)(kProj, argInt(a, "number"), rgn))
        throw std::runtime_error("marker/region not found");
    return {{"removed", argInt(a, "number")}, {"is_region", rgn}};
}

json runAction(const json& a) {
    const int id = resolveCommandId(argStr(a, "command"));
    RA(Main_OnCommand)(id, 0);
    return {{"command_id", id}};
}

} // namespace

void registerWriteTools(HandlerMap& m) {
    m["create_track"] = createTrack;
    m["rename_track"] = renameTrack;
    m["set_track_volume"] = [](const json& a) {
        setTrack(a, "D_VOL", fromDb(argNum(a, "db")));
        return json{{"volume_db", argNum(a, "db")}};
    };
    m["set_track_pan"] = [](const json& a) {
        const double p = std::clamp(argNum(a, "pan"), -1.0, 1.0);
        setTrack(a, "D_PAN", p);
        return json{{"pan", p}};
    };
    m["set_track_mute"] = [](const json& a) {
        setTrack(a, "B_MUTE", argBool(a, "mute", true) ? 1.0 : 0.0);
        return json{{"mute", argBool(a, "mute", true)}};
    };
    m["set_track_solo"] = [](const json& a) {
        setTrack(a, "I_SOLO", argBool(a, "solo", true) ? 2.0 : 0.0); // 2 = solo in place
        return json{{"solo", argBool(a, "solo", true)}};
    };
    m["set_track_arm"] = [](const json& a) {
        setTrack(a, "I_RECARM", argBool(a, "arm", true) ? 1.0 : 0.0);
        return json{{"armed", argBool(a, "arm", true)}};
    };
    m["set_track_color"] = [](const json& a) {
        setTrack(a, "I_CUSTOMCOLOR", parseHexColor(argStr(a, "color")));
        return json{{"color", argStr(a, "color")}};
    };
    m["select_tracks"] = selectTracks;
    m["set_track_property"] = setTrackProp;
    m["create_send"] = createSend;
    m["set_folder"] = setFolder;
    m["set_tempo"] = setTempo;
    m["set_time_signature"] = setTimeSig;
    m["set_cursor"] = setCursor;
    m["set_loop_range"] = setLoop;
    m["transport"] = transport;
    m["add_marker"] = [](const json& a) { return addMarker(a, false); };
    m["add_region"] = [](const json& a) { return addMarker(a, true); };
    m["add_fx"] = addFx;
    m["set_fx_enabled"] = setFxEnabled;
    m["set_fx_param"] = setFxParam;
    m["insert_media"] = insertMedia;
    m["select_items"] = selectItems;
    m["split_item"] = splitItem;
    m["move_item"] = moveItem;
    m["set_item_length"] = setItemLength;
    m["set_item_volume"] = setItemVolume;
    m["set_item_mute"] = setItemMute;
    m["set_item_fades"] = setItemFades;
    m["set_item_pitch"] = setItemPitch;
    m["set_item_property"] = setItemProp;
    m["undo"] = [](const json&) {
        const int r = RA(Undo_DoUndo2)(kProj);
        if (r == 0)
            throw std::runtime_error("nothing to undo");
        return json{{"undone", true}};
    };
    m["delete_track"] = deleteTrack;
    m["delete_item"] = deleteItem;
    m["remove_fx"] = removeFx;
    m["remove_marker"] = removeMarker;
    m["run_action"] = runAction;
    m["record"] = [](const json&) {
        RA(CSurf_OnRecord)();
        return json{{"recording", true}};
    };
    m["save_project"] = [](const json&) {
        RA(Main_SaveProject)(kProj, false);
        return json{{"saved", true}};
    };
}

} // namespace reals::ext::agent

#endif // _WIN32
