#pragma once

// Private to the agent executor: REAPER API function table resolved via
// GetFunc (kept separate from reaper_plugin.cpp's REAPERAPI_IMPLEMENT globals
// so the two translation units never clash), plus shared helpers.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <functional>
#include <stdexcept>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <reaper_plugin.h>

namespace reals::ext::agent {

using json = nlohmann::json;

#define REALS_AGENT_API_LIST(X)                                                                   \
    X(void, Main_OnCommand, (int, int))                                                           \
    X(int, NamedCommandLookup, (const char*))                                                     \
    X(int, GetToggleCommandState, (int))                                                          \
    X(void, Undo_BeginBlock2, (ReaProject*))                                                      \
    X(void, Undo_EndBlock2, (ReaProject*, const char*, int))                                      \
    X(int, Undo_DoUndo2, (ReaProject*))                                                           \
    X(int, CountTracks, (ReaProject*))                                                            \
    X(MediaTrack*, GetTrack, (ReaProject*, int))                                                  \
    X(MediaTrack*, GetMasterTrack, (ReaProject*))                                                 \
    X(void, InsertTrackAtIndex, (int, bool))                                                      \
    X(void, DeleteTrack, (MediaTrack*))                                                           \
    X(bool, GetSetMediaTrackInfo_String, (MediaTrack*, const char*, char*, bool))                 \
    X(double, GetMediaTrackInfo_Value, (MediaTrack*, const char*))                                \
    X(bool, SetMediaTrackInfo_Value, (MediaTrack*, const char*, double))                          \
    X(void, SetTrackSelected, (MediaTrack*, bool))                                                \
    X(int, CountSelectedTracks, (ReaProject*))                                                    \
    X(int, ColorToNative, (int, int, int))                                                        \
    X(int, CreateTrackSend, (MediaTrack*, MediaTrack*))                                           \
    X(int, GetTrackNumSends, (MediaTrack*, int))                                                  \
    X(bool, GetTrackSendName, (MediaTrack*, int, char*, int))                                     \
    X(int, TrackFX_AddByName, (MediaTrack*, const char*, bool, int))                              \
    X(int, TrackFX_GetCount, (MediaTrack*))                                                       \
    X(bool, TrackFX_GetFXName, (MediaTrack*, int, char*, int))                                    \
    X(bool, TrackFX_Delete, (MediaTrack*, int))                                                   \
    X(bool, TrackFX_GetEnabled, (MediaTrack*, int))                                               \
    X(void, TrackFX_SetEnabled, (MediaTrack*, int, bool))                                         \
    X(int, TrackFX_GetNumParams, (MediaTrack*, int))                                              \
    X(bool, TrackFX_GetParamName, (MediaTrack*, int, int, char*, int))                            \
    X(bool, TrackFX_SetParamNormalized, (MediaTrack*, int, int, double))                          \
    X(double, TrackFX_GetParamNormalized, (MediaTrack*, int, int))                                \
    X(bool, TrackFX_GetFormattedParamValue, (MediaTrack*, int, int, char*, int))                  \
    X(void, SetCurrentBPM, (ReaProject*, double, bool))                                           \
    X(double, Master_GetTempo, ())                                                                \
    X(void, TimeMap_GetTimeSigAtTime, (ReaProject*, double, int*, int*, double*))                 \
    X(int, CountTempoTimeSigMarkers, (ReaProject*))                                               \
    X(bool, SetTempoTimeSigMarker, (ReaProject*, int, double, int, double, double, int, int, bool)) \
    X(double, GetCursorPosition, ())                                                              \
    X(void, SetEditCurPos2, (ReaProject*, double, bool, bool))                                    \
    X(void, GetSet_LoopTimeRange2, (ReaProject*, bool, bool, double*, double*, bool))             \
    X(double, GetProjectLength, (ReaProject*))                                                    \
    X(void, GetProjectName, (ReaProject*, char*, int))                                            \
    X(void, Main_SaveProject, (ReaProject*, bool))                                                \
    X(int, GetPlayState, ())                                                                      \
    X(double, GetPlayPosition, ())                                                                \
    X(void, OnPlayButton, ())                                                                     \
    X(void, OnStopButton, ())                                                                     \
    X(void, OnPauseButton, ())                                                                    \
    X(void, CSurf_OnRecord, ())                                                                   \
    X(int, CountMediaItems, (ReaProject*))                                                        \
    X(int, CountSelectedMediaItems, (ReaProject*))                                                \
    X(MediaItem*, GetSelectedMediaItem, (ReaProject*, int))                                       \
    X(int, CountTrackMediaItems, (MediaTrack*))                                                   \
    X(MediaItem*, GetTrackMediaItem, (MediaTrack*, int))                                          \
    X(MediaTrack*, GetMediaItem_Track, (MediaItem*))                                              \
    X(double, GetMediaItemInfo_Value, (MediaItem*, const char*))                                  \
    X(bool, SetMediaItemInfo_Value, (MediaItem*, const char*, double))                            \
    X(MediaItem*, SplitMediaItem, (MediaItem*, double))                                           \
    X(bool, DeleteTrackMediaItem, (MediaTrack*, MediaItem*))                                      \
    X(void, SetMediaItemSelected, (MediaItem*, bool))                                             \
    X(void, SelectAllMediaItems, (ReaProject*, bool))                                             \
    X(MediaItem_Take*, GetActiveTake, (MediaItem*))                                               \
    X(const char*, GetTakeName, (MediaItem_Take*))                                                \
    X(double, GetMediaItemTakeInfo_Value, (MediaItem_Take*, const char*))                         \
    X(bool, SetMediaItemTakeInfo_Value, (MediaItem_Take*, const char*, double))                   \
    X(PCM_source*, GetMediaItemTake_Source, (MediaItem_Take*))                                    \
    X(void, GetMediaSourceFileName, (PCM_source*, char*, int))                                    \
    X(int, InsertMedia, (const char*, int))                                                       \
    X(int, AddProjectMarker2, (ReaProject*, bool, double, double, const char*, int, int))         \
    X(bool, DeleteProjectMarker, (ReaProject*, int, bool))                                        \
    X(int, EnumProjectMarkers3,                                                                   \
      (ReaProject*, int, bool*, double*, double*, const char**, int*, int*))                      \
    X(void, UpdateArrange, ())                                                                    \
    X(void, TrackList_AdjustWindows, (bool))

struct Api {
#define REALS_AGENT_DECLARE(ret, name, params) ret(*name) params = nullptr;
    REALS_AGENT_API_LIST(REALS_AGENT_DECLARE)
#undef REALS_AGENT_DECLARE
};

// Global table (filled by init()).
Api& api();

// Throw when a required REAPER function is missing (older REAPER builds).
template <typename F>
F need(F fn, const char* name) {
    if (!fn)
        throw std::runtime_error(std::string("REAPER API not available: ") + name);
    return fn;
}
#define RA(name) need(::reals::ext::agent::api().name, #name)

// ---- argument helpers (throw std::runtime_error on bad input) --------------
bool hasArg(const json& a, const char* key);
int argInt(const json& a, const char* key);
int argInt(const json& a, const char* key, int def);
double argNum(const json& a, const char* key);
double argNum(const json& a, const char* key, double def);
bool argBool(const json& a, const char* key, bool def);
std::string argStr(const json& a, const char* key, const std::string& def = {});

// ---- REAPER object helpers ---------------------------------------------------
MediaTrack* trackArg(const json& a, const char* key = "track"); // 1-based, 0 = master
MediaItem* itemArg(const json& a);                               // track + item (1-based)
MediaItem_Take* activeTake(MediaItem* item);
std::string trackName(MediaTrack* tr);
int trackNumber(MediaTrack* tr); // 1-based, 0 = master
double toDb(double gain);
double fromDb(double db);
int parseHexColor(const std::string& hex); // native color with custom flag, throws on bad input
json trackSummary(MediaTrack* tr);
json itemSummary(MediaItem* item, int trackNo, int itemNo);
bool isAllowedTrackProp(const std::string& prop);
bool isAllowedItemProp(const std::string& prop, bool* isTakeProp);
int resolveCommandId(const std::string& cmd); // numeric or named (_SWS_...) action id

using Handler = std::function<json(const json& args)>;
using HandlerMap = std::unordered_map<std::string, Handler>;
void registerReadTools(HandlerMap& map);
void registerWriteTools(HandlerMap& map);

} // namespace reals::ext::agent
