// Agent executor helpers: REAPER API table + argument/object helpers.
#ifdef _WIN32

#include "ReaperAgentApi.h"
#include "ReaperAgentTools.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace reals::ext::agent {

Api& api() {
    static Api s_api;
    return s_api;
}

void init(void* (*getFunc)(const char* name)) {
    if (!getFunc)
        return;
    Api& a = api();
#define REALS_AGENT_LOAD(ret, name, params) a.name = reinterpret_cast<ret(*) params>(getFunc(#name));
    REALS_AGENT_API_LIST(REALS_AGENT_LOAD)
#undef REALS_AGENT_LOAD
}

// ---- arguments -----------------------------------------------------------------
bool hasArg(const json& a, const char* key) {
    return a.is_object() && a.contains(key) && !a[key].is_null();
}

static double numberOf(const json& v, const char* key) {
    if (v.is_number())
        return v.get<double>();
    if (v.is_boolean())
        return v.get<bool>() ? 1.0 : 0.0;
    if (v.is_string()) {
        const std::string s = v.get<std::string>();
        char* end = nullptr;
        const double d = std::strtod(s.c_str(), &end);
        if (end && end != s.c_str())
            return d;
    }
    throw std::runtime_error(std::string("argument '") + key + "' must be a number");
}

int argInt(const json& a, const char* key) {
    if (!hasArg(a, key))
        throw std::runtime_error(std::string("missing argument: ") + key);
    return static_cast<int>(std::lround(numberOf(a[key], key)));
}

int argInt(const json& a, const char* key, int def) { return hasArg(a, key) ? argInt(a, key) : def; }

double argNum(const json& a, const char* key) {
    if (!hasArg(a, key))
        throw std::runtime_error(std::string("missing argument: ") + key);
    const double d = numberOf(a[key], key);
    if (!std::isfinite(d))
        throw std::runtime_error(std::string("argument '") + key + "' is not finite");
    return d;
}

double argNum(const json& a, const char* key, double def) { return hasArg(a, key) ? argNum(a, key) : def; }

bool argBool(const json& a, const char* key, bool def) {
    if (!hasArg(a, key))
        return def;
    const json& v = a[key];
    if (v.is_boolean())
        return v.get<bool>();
    if (v.is_number())
        return v.get<double>() != 0.0;
    if (v.is_string()) {
        const std::string s = v.get<std::string>();
        return s == "true" || s == "1" || s == "yes" || s == "on";
    }
    return def;
}

std::string argStr(const json& a, const char* key, const std::string& def) {
    if (!hasArg(a, key))
        return def;
    const json& v = a[key];
    return v.is_string() ? v.get<std::string>() : v.dump();
}

// ---- objects ---------------------------------------------------------------------
MediaTrack* trackArg(const json& a, const char* key) {
    const int n = argInt(a, key);
    if (n == 0) {
        MediaTrack* m = RA(GetMasterTrack)(nullptr);
        if (!m)
            throw std::runtime_error("master track not available");
        return m;
    }
    const int count = RA(CountTracks)(nullptr);
    if (n < 1 || n > count)
        throw std::runtime_error("track " + std::to_string(n) + " does not exist (project has " +
                                 std::to_string(count) + " tracks)");
    MediaTrack* tr = RA(GetTrack)(nullptr, n - 1);
    if (!tr)
        throw std::runtime_error("track " + std::to_string(n) + " not found");
    return tr;
}

MediaItem* itemArg(const json& a) {
    MediaTrack* tr = trackArg(a, "track");
    const int n = argInt(a, "item");
    const int count = RA(CountTrackMediaItems)(tr);
    if (n < 1 || n > count)
        throw std::runtime_error("item " + std::to_string(n) + " does not exist on that track (" +
                                 std::to_string(count) + " items)");
    MediaItem* it = RA(GetTrackMediaItem)(tr, n - 1);
    if (!it)
        throw std::runtime_error("item not found");
    return it;
}

MediaItem_Take* activeTake(MediaItem* item) {
    MediaItem_Take* tk = RA(GetActiveTake)(item);
    if (!tk)
        throw std::runtime_error("item has no active take");
    return tk;
}

std::string trackName(MediaTrack* tr) {
    char buf[1024] = {};
    if (api().GetSetMediaTrackInfo_String)
        api().GetSetMediaTrackInfo_String(tr, "P_NAME", buf, false);
    return buf;
}

int trackNumber(MediaTrack* tr) {
    const double n = RA(GetMediaTrackInfo_Value)(tr, "IP_TRACKNUMBER");
    return n < 0 ? 0 : static_cast<int>(n); // -1 = master
}

double toDb(double gain) { return gain > 1e-8 ? 20.0 * std::log10(gain) : -150.0; }

double fromDb(double db) {
    db = std::clamp(db, -150.0, 24.0);
    return db <= -150.0 ? 0.0 : std::pow(10.0, db / 20.0);
}

int parseHexColor(const std::string& hexIn) {
    std::string hex = hexIn;
    if (!hex.empty() && hex[0] == '#')
        hex.erase(0, 1);
    if (hex.size() != 6 || !std::all_of(hex.begin(), hex.end(), [](unsigned char c) { return std::isxdigit(c) != 0; }))
        throw std::runtime_error("color must be a hex value like #ff8800");
    const long v = std::strtol(hex.c_str(), nullptr, 16);
    const int r = static_cast<int>((v >> 16) & 0xff), g = static_cast<int>((v >> 8) & 0xff), b = static_cast<int>(v & 0xff);
    return RA(ColorToNative)(r, g, b) | 0x1000000;
}

json trackSummary(MediaTrack* tr) {
    auto v = [&](const char* p) { return RA(GetMediaTrackInfo_Value)(tr, p); };
    json j;
    j["track"] = trackNumber(tr);
    j["name"] = trackName(tr);
    j["volume_db"] = std::round(toDb(v("D_VOL")) * 100.0) / 100.0;
    j["pan"] = std::round(v("D_PAN") * 100.0) / 100.0;
    j["mute"] = v("B_MUTE") != 0.0;
    j["solo"] = v("I_SOLO") != 0.0;
    j["armed"] = v("I_RECARM") != 0.0;
    j["selected"] = v("I_SELECTED") != 0.0;
    j["folder_depth"] = static_cast<int>(v("I_FOLDERDEPTH"));
    j["fx_count"] = api().TrackFX_GetCount ? api().TrackFX_GetCount(tr) : 0;
    j["item_count"] = api().CountTrackMediaItems ? api().CountTrackMediaItems(tr) : 0;
    const int color = static_cast<int>(v("I_CUSTOMCOLOR"));
    if (color & 0x1000000) {
        char hex[16];
        // Native Windows COLORREF is 0x00BBGGRR.
        std::snprintf(hex, sizeof(hex), "#%02x%02x%02x", color & 0xff, (color >> 8) & 0xff, (color >> 16) & 0xff);
        j["color"] = hex;
    }
    return j;
}

json itemSummary(MediaItem* item, int trackNo, int itemNo) {
    auto v = [&](const char* p) { return RA(GetMediaItemInfo_Value)(item, p); };
    json j;
    j["track"] = trackNo;
    j["item"] = itemNo;
    j["position"] = v("D_POSITION");
    j["length"] = v("D_LENGTH");
    j["mute"] = v("B_MUTE") != 0.0;
    j["selected"] = v("B_UISEL") != 0.0;
    j["volume_db"] = std::round(toDb(v("D_VOL")) * 100.0) / 100.0;
    if (MediaItem_Take* tk = api().GetActiveTake ? api().GetActiveTake(item) : nullptr) {
        if (api().GetTakeName) {
            const char* nm = api().GetTakeName(tk);
            j["take"] = nm ? nm : "";
        }
        if (api().GetMediaItemTake_Source && api().GetMediaSourceFileName) {
            if (PCM_source* src = api().GetMediaItemTake_Source(tk)) {
                char path[2048] = {};
                api().GetMediaSourceFileName(src, path, sizeof(path));
                j["source"] = path;
            }
        }
        if (api().GetMediaItemTakeInfo_Value) {
            j["pitch"] = api().GetMediaItemTakeInfo_Value(tk, "D_PITCH");
            j["playrate"] = api().GetMediaItemTakeInfo_Value(tk, "D_PLAYRATE");
        }
    }
    return j;
}

bool isAllowedTrackProp(const std::string& p) {
    static const char* kProps[] = {"D_VOL",       "D_PAN",         "D_WIDTH",    "B_MUTE",     "I_SOLO",
                                   "I_RECARM",    "I_RECMON",      "I_RECINPUT", "B_PHASE",    "I_FOLDERDEPTH",
                                   "B_SHOWINTCP", "B_SHOWINMIXER", "I_SELECTED", "D_PANLAW",   "B_MAINSEND",
                                   "I_CUSTOMCOLOR", "I_FOLDERCOMPACT", "B_FREEMODE"};
    return std::any_of(std::begin(kProps), std::end(kProps), [&](const char* k) { return p == k; });
}

bool isAllowedItemProp(const std::string& p, bool* isTakeProp) {
    static const char* kItem[] = {"D_POSITION", "D_LENGTH",   "D_VOL",    "B_MUTE", "D_FADEINLEN",
                                  "D_FADEOUTLEN", "D_SNAPOFFSET", "B_LOOPSRC", "C_LOCK", "B_UISEL"};
    static const char* kTake[] = {"D_PLAYRATE", "D_PITCH", "B_PPITCH", "D_STARTOFFS", "D_VOL", "D_PAN"};
    if (std::any_of(std::begin(kItem), std::end(kItem), [&](const char* k) { return p == k; })) {
        if (isTakeProp)
            *isTakeProp = false;
        return true;
    }
    if (p.rfind("TAKE:", 0) == 0) {
        const std::string t = p.substr(5);
        if (std::any_of(std::begin(kTake), std::end(kTake), [&](const char* k) { return t == k; })) {
            if (isTakeProp)
                *isTakeProp = true;
            return true;
        }
        return false;
    }
    if (p == "D_PLAYRATE" || p == "D_PITCH" || p == "B_PPITCH") {
        if (isTakeProp)
            *isTakeProp = true;
        return true;
    }
    return false;
}

} // namespace reals::ext::agent

#endif // _WIN32
