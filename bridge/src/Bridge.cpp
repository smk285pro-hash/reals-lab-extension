#include "reals/bridge/Bridge.h"
#include "reals/ai/ClapEmbedder.h"
#include "reals/ai/GenreClassifier.h"
#include "reals/ai/KeyDetector.h"
#include "reals/ai/MoodClassifier.h"
#include "reals/ai/TempoDetector.h"
#include "reals/audio/Engine.h"
#include "reals/browser/BrowserModel.h"
#include "reals/config/Config.h"
#include "reals/db/Database.h"
#include "reals/platform/DirWatch.h"
#include "reals/platform/Path.h"
#include "reals/lab/LabApi.h"
#include "reals/scanner/BackgroundScanner.h"
#include "reals/search/QueryParser.h"
#include "reals/search/SearchEngine.h"
#include "reals/util/Log.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <regex>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <miniaudio.h>

namespace fs = std::filesystem;

namespace reals::bridge {
using json = nlohmann::json;

namespace {
constexpr auto kTag = "bridge";

std::string narrowPath(const std::string& p) {
    return platform::normalizePath(p);
}

json entryToJson(const browser::FileEntry& f) {
    json e;
    e["name"] = f.name;
    e["path"] = f.path;
    e["ext"] = f.ext;
    e["size"] = f.sizeBytes;
    e["modified"] = f.modifiedEpoch;
    e["isAudio"] = f.isAudio;
    e["isDir"] = f.isDir;
    e["bpm"] = f.bpm;
    e["key"] = f.key;
    e["camelot"] = f.camelot;
    e["duration"] = f.durationSec;
    return e;
}
} // namespace

struct SharedState {
    std::mutex evMutex;
    std::deque<std::string> events;
    std::atomic<bool> abortJobs{false};

    void pushEvent(json j) {
        const std::lock_guard lock(evMutex);
        events.push_back(j.dump());
    }
};

struct Bridge::Impl {
    IHostActions* actions = nullptr;
    browser::BrowserModel model;
    db::Database db;
    std::unique_ptr<search::SearchEngine> searchEngine;
    std::unique_ptr<scanner::BackgroundScanner> scanner;
    std::shared_ptr<SharedState> state = std::make_shared<SharedState>();
    platform::DirWatch watch;
    std::atomic<uint64_t> searchGen{0};
    std::shared_ptr<std::atomic<bool>> searchCancel;
    std::thread searchTh;

    // Workers are tracked so they are JOINED on destruction instead of
    // detaching: a detached thread still calling WinHTTP during DLL unload
    // can crash the host even when its captures stay alive.
    // Each worker carries a completion flag so finished threads can be
    // joined and removed during runtime — a finished-but-unjoined thread is
    // still "joinable", so without the flag the vector (and its OS thread
    // handles) would grow unbounded over a long session.
    struct TrackedWorker {
        std::thread th;
        std::shared_ptr<std::atomic<bool>> done = std::make_shared<std::atomic<bool>>(false);
    };
    std::mutex jobMutex;
    std::vector<TrackedWorker> workers;

    ~Impl() {
        if (scanner) {
            scanner->cancel();
            scanner->waitForCompletion();
        }
        watch.stop();
        if (searchCancel)
            searchCancel->store(true);
        if (searchTh.joinable())
            searchTh.join();
        if (state)
            state->abortJobs.store(true);
        const std::lock_guard lock(jobMutex);
        for (auto& w : workers)
            if (w.th.joinable())
                w.th.join();
    }

    void purgeFinishedWorkers() {
        for (auto it = workers.begin(); it != workers.end();) {
            if (it->done->load() && it->th.joinable()) {
                it->th.join(); // finished -> join returns immediately
                it = workers.erase(it);
            } else if (!it->th.joinable()) {
                it = workers.erase(it);
            } else {
                ++it;
            }
        }
    }

    // Spawn a tracked worker that flips its done flag on exit.
    template <typename F>
    void spawnWorker(F&& fn) {
        TrackedWorker tw;
        const auto done = tw.done;
        tw.th = std::thread([done, fn = std::forward<F>(fn)]() {
            fn();
            done->store(true);
        });
        workers.push_back(std::move(tw));
    }

    std::mutex cacheMutex;
    std::unordered_map<std::string, std::vector<float>> envCache;
    std::unordered_map<std::string, audio::TrackInfo> probeCache;

    // Sync BPM state (for preview and DAW insert auto-match)
    std::mutex syncMutex;
    bool syncEnabled = false;
    float syncRatio = 1.0f;
    std::string syncPath;
    float syncSampleBpm = 0.0f;

    // Native REAPER preview state. On the PlayPreviewEx path the core Engine is
    // stopped, so audio.setSyncBpm cannot rely on eng.currentTrack() to re-phase
    // a running preview. These mirror what was handed to playHostPreview.
    std::string previewPath;
    double previewDurationSeconds = 0.0;
    double previewLoopBeats = 0.0;
    double previewSampleBpm = 0.0;
    uint64_t previewNominalLoopFrames = 0;
    double lastPhaseFraction = -1.0;

    // Transport tracking for DAW seek / cursor movement detection
    double lastTransportBeats = -1.0;
    double lastTransportPos = -1.0;
    int lastPlayState = -1;
    std::chrono::steady_clock::time_point lastTransportCheckTime{};
    bool transportInitialized = false;

    // Helper: detect BPM for a file (Filename ground truth -> DB -> TempoDetector)
    float detectBpmForPath(const std::string& path) {
        if (path.empty()) return 0.0f;

        std::string fname;
        try {
            auto p = platform::u8path(path);
            fname = platform::pathToUtf8(p.filename());
        } catch (...) { fname = path; }
        if (fname.empty()) fname = path;

        // 1. Fast Filename Music Metadata parsing (Strict ground truth)
        db::SampleRecord fnRec;
        scanner::BackgroundScanner::parseFilenameMusicMetadata(fname, path, fnRec);

        // 2. DB lookup
        if (auto rec = db.getSampleByPath(path); rec.has_value()) {
            if (fnRec.bpm > 0.0 && std::abs(rec->bpm - fnRec.bpm) > 0.1) {
                auto r = rec.value();
                r.bpm = fnRec.bpm;
                db.upsertSample(r);
                return static_cast<float>(fnRec.bpm);
            }
            if (rec->bpm > 30.0 && rec->bpm < 300.0) {
                // Clear fake 50.0 BPM on one-shot kick/clap/snare
                if (std::abs(rec->bpm - 50.0) < 0.01 && fnRec.bpm == 0.0 &&
                    (rec->genre == "Kick" || rec->genre == "Clap" || rec->genre == "Snare" || rec->genre == "Hi-Hat" || rec->genre == "Percussion")) {
                    auto r = rec.value();
                    r.bpm = 0.0;
                    db.upsertSample(r);
                    return 0.0f;
                }
                return static_cast<float>(rec->bpm);
            }
        }

        if (fnRec.bpm > 0.0) {
            return static_cast<float>(fnRec.bpm);
        }

        // MIDI files do not have PCM audio waveforms for TempoDetector
        const std::string lowerP = platform::toLowerUtf8(path);
        if (lowerP.ends_with(".mid") || lowerP.ends_with(".midi")) {
            return 0.0f;
        }

        // 3. Local TempoDetector (decode up to 30s mono)
        float bpm = audio::Engine::detectBpm(path);
        if (bpm >= 40.0f && bpm <= 250.0f) {
            if (auto rec = db.getSampleByPath(path); rec.has_value()) {
                auto r = rec.value();
                r.bpm = bpm;
                db.upsertSample(r);
            }
            return bpm;
        }
        return 0.0f;
    }

    // Helper: detect musical key for a file (Filename ground truth -> DB -> KeyDetector)
    std::string detectKeyForPath(const std::string& path) {
        if (path.empty()) return {};

        std::string fname;
        try {
            auto p = platform::u8path(path);
            fname = platform::pathToUtf8(p.filename());
        } catch (...) { fname = path; }
        if (fname.empty()) fname = path;

        // 1. Fast Filename Music Metadata parsing (Strict ground truth)
        db::SampleRecord fnRec;
        scanner::BackgroundScanner::parseFilenameMusicMetadata(fname, path, fnRec);

        // 2. DB lookup
        if (auto rec = db.getSampleByPath(path); rec.has_value() && !rec->keyRoot.empty()) {
            // Check if DB is holding the bogus "F Major" while filename has an explicit key
            if (!fnRec.keyRoot.empty() && rec->keyRoot == "F" && rec->keyMode == "major" &&
                (fnRec.keyRoot != "F" || fnRec.keyMode != "major")) {
                auto r = rec.value();
                r.keyRoot = fnRec.keyRoot;
                r.keyMode = fnRec.keyMode;
                r.camelot = fnRec.camelot;
                db.upsertSample(r);
                return fnRec.keyMode == "minor" ? fnRec.keyRoot + "m" : fnRec.keyRoot;
            }

            std::string k = rec->keyRoot;
            if (!rec->keyMode.empty() && (rec->keyMode == "minor" || rec->keyMode == "Minor")) {
                k += "m";
            }
            return k;
        }

        if (!fnRec.keyRoot.empty()) {
            return fnRec.keyMode == "minor" ? fnRec.keyRoot + "m" : fnRec.keyRoot;
        }

        // MIDI files do not have PCM audio waveforms for KeyDetector
        const std::string lowerP = platform::toLowerUtf8(path);
        if (lowerP.ends_with(".mid") || lowerP.ends_with(".midi")) {
            return {};
        }

        // 3. Local KeyDetector
        std::string k = audio::Engine::detectKey(path);
        if (!k.empty()) {
            if (auto rec = db.getSampleByPath(path); rec.has_value()) {
                auto r = rec.value();
                std::string root = k;
                std::string mode = "major";
                if (!k.empty() && k.back() == 'm') {
                    root = k.substr(0, k.size() - 1);
                    mode = "minor";
                }
                r.keyRoot = root;
                r.keyMode = mode;
                r.camelot = ai::KeyDetector::toCamelot(root, mode);
                db.upsertSample(r);
            }
            return k;
        }
        return {};
    }

    void runEnvelopeScan(const std::string& path) {
        auto st = state;
        const std::lock_guard lock(jobMutex);
        purgeFinishedWorkers();
        spawnWorker([this, st, path]() {
            if (!st || st->abortJobs)
                return;
            const auto env = audio::Engine::computeEnvelope(path);
            if (!st || st->abortJobs)
                return;
            {
                const std::lock_guard lk(cacheMutex);
                envCache[path] = env;
            }
            audio::Engine::instance().setEnvelope(path, env);
            json ev;
            ev["event"] = "audio.envelope";
            ev["data"] = {{"path", path}, {"envelope", env}};
            st->pushEvent(ev);
        });
    }

    // Background lab job: start -> poll -> download results -> emit event.
    void runLabJob(const std::string& job, const std::string& path, const int modeOrStrength) {
        auto st = state;
        const std::lock_guard lock(jobMutex);
        // Join & purge finished workers so the vector cannot grow unbounded.
        purgeFinishedWorkers();
        spawnWorker([st, job, path, modeOrStrength]() {
            if (!st)
                return;
            try {
                json payload;
                if (job == "analyze" || job == "tempo") {
                    payload = lab::LabApi::analyze(path);
                } else if (job == "keychord" || job == "midi") {
                    payload = lab::LabApi::chords(path);
                } else if (job == "stem" || job == "denoise") {
                    json start = job == "stem" ? lab::LabApi::startSeparate(path, modeOrStrength)
                                               : lab::LabApi::startDenoise(path, modeOrStrength);
                    const std::string taskId = start.value("task_id", "");
                    if (taskId.empty())
                        throw std::runtime_error("no task_id from api");
                    json last;
                    int polls = 0;
                    constexpr int kMaxPolls = 300; // 10 minutes max
                    for (; polls < kMaxPolls; ++polls) {
                        if (st->abortJobs) return;
                        std::this_thread::sleep_for(std::chrono::seconds(2));
                        if (st->abortJobs) return;
                        last = lab::LabApi::pollJob(taskId);
                        const std::string status = last.value("status", "");
                        json prog;
                        prog["event"] = "lab.progress";
                        prog["data"] = {{"job", job},
                                        {"percent", last.value("percent", 0)},
                                        {"stage", last.value("stage", status)}};
                        st->pushEvent(prog);
                        if (status == "COMPLETE") {
                            payload = last;
                            break;
                        }
                        if (status == "FAILED")
                            throw std::runtime_error(last.value("error", "job failed"));
                    }
                    if (polls >= kMaxPolls)
                        throw std::runtime_error("job timed out");

                    if (st->abortJobs) return;

                    // Download results to the lab dir.
                    const std::string labDir = platform::joinPath(platform::dataDir(), "lab");
                    platform::ensureDir(labDir);
                    json out;
                    out["job"] = job;
                    out["files"] = json::array();
                    if (job == "stem") {
                        // Accept both {"result":{"stems":{"stems":{...}}}} and
                        // {"result":{"stems":{...}}} — and fail loudly instead
                        // of silently emitting an empty result.
                        const json* stems = nullptr;
                        if (last.contains("result") && last["result"].is_object()) {
                            const json& r = last["result"];
                            if (r.contains("stems") && r["stems"].is_object()) {
                                const json& s = r["stems"];
                                if (s.contains("stems") && s["stems"].is_object())
                                    stems = &s["stems"];
                                else
                                    stems = &s;
                            }
                        }
                        if (stems) {
                            const std::string taskId2 = last.value("task_id", "");
                            for (auto it = stems->begin(); it != stems->end(); ++it) {
                                if (st->abortJobs) return;
                                if (!it.value().is_object())
                                    continue;
                                const std::string url = it.value().value("url", "");
                                if (url.empty())
                                    continue;
                                const std::string dest = platform::joinPath(
                                    labDir, taskId2 + "_" + it.key() + ".wav");
                                if (lab::LabApi::downloadToFile(url, dest)) {
                                    out["files"].push_back({{"name", it.key()},
                                                            {"path", dest},
                                                            {"color",
                                                             it.value().value("color", "#cccccc")}});
                                }
                            }
                        }
                        std::string zipUrl;
                        if (last.contains("result") && last["result"].is_object())
                            zipUrl = last["result"].value("zip_url", "");
                        if (!zipUrl.empty() && !st->abortJobs) {
                            const std::string zipDest = platform::joinPath(
                                labDir, last.value("task_id", "stems") + "_stems.zip");
                            if (lab::LabApi::downloadToFile(zipUrl, zipDest))
                                out["zipPath"] = zipDest;
                        }
                    } else { // denoise
                        std::string url;
                        if (last.contains("result") && last["result"].is_object())
                            url = last["result"].value("denoise_url", "");
                        if (!url.empty()) {
                            const std::string dest = platform::joinPath(
                                labDir, last.value("task_id", "denoise") + "_clean.wav");
                            if (lab::LabApi::downloadToFile(url, dest))
                                out["files"].push_back({{"name", "clean"},
                                                        {"path", dest},
                                                        {"color", "#35D07F"}});
                        }
                    }
                    if (!out["files"].empty() || out.contains("zipPath")) {
                        if (st->abortJobs) return;
                        json ev;
                        ev["event"] = "lab.result";
                        ev["data"] = out;
                        st->pushEvent(ev);
                    } else {
                        throw std::runtime_error(
                            "unexpected result shape from api (no files downloaded)");
                    }
                    return;
                }

                if (st->abortJobs) return;
                json ev;
                ev["event"] = "lab.result";
                ev["data"] = {{"job", job}, {"payload", payload}};
                st->pushEvent(ev);
            } catch (const std::exception& e) {
                if (st->abortJobs) return;
                json ev;
                ev["event"] = "lab.error";
                ev["data"] = {{"job", job}, {"error", e.what()}};
                st->pushEvent(ev);
            } catch (...) {
                if (st->abortJobs) return;
                json ev;
                ev["event"] = "lab.error";
                ev["data"] = {{"job", job}, {"error", "unknown"}};
                st->pushEvent(ev);
            }
        });
    }

    // Recursive search on a worker using SearchEngine and filesystem crawler
    void runSearch(const std::string& base, const std::string& query, const bool audioOnly,
                   const size_t maxResults, uint64_t gen) {
        if (searchCancel)
            searchCancel->store(true);
        if (searchTh.joinable())
            searchTh.join();
        auto cancel = std::make_shared<std::atomic<bool>>(false);
        searchCancel = cancel;
        if (gen == 0)
            gen = ++searchGen;
        else
            searchGen.store(gen);
        auto st = state;
        searchTh = std::thread([this, st, cancel, gen, base, query, audioOnly, maxResults]() {
            if (!st)
                return;

            json arr = json::array();
            std::unordered_set<std::string> seenPaths;

            // 1. Intelligent search via SearchEngine
            if (searchEngine && db.isOpen()) {
                search::SearchOptions opts;
                opts.limit = static_cast<int>(maxResults);
                opts.basePath = base;
                opts.enableSemantic = true;
                opts.enableSyntax = true;

                const auto sResults = searchEngine->search(query, opts);
                for (const auto& sr : sResults) {
                    if (cancel->load() || gen != searchGen.load())
                        return;

                    seenPaths.insert(sr.sample.path);
                    json e;
                    e["name"] = sr.sample.filename;
                    e["path"] = sr.sample.path;
                    e["ext"] = "";
                    auto dot = sr.sample.filename.rfind('.');
                    if (dot != std::string::npos && dot + 1 < sr.sample.filename.size()) {
                        e["ext"] = sr.sample.filename.substr(dot + 1);
                    }
                    e["size"] = sr.sample.filesize;
                    e["modified"] = sr.sample.modifiedTime;
                    e["isAudio"] = true;
                    e["isDir"] = false;
                    e["duration"] = sr.sample.durationSec;
                    e["bpm"] = sr.sample.bpm;
                    e["key"] = sr.sample.keyRoot.empty() ? "" : (sr.sample.keyMode == "minor" ? sr.sample.keyRoot + "m" : sr.sample.keyRoot);
                    e["camelot"] = sr.sample.camelot;
                    e["genre"] = sr.sample.genre;
                    e["mood"] = sr.sample.mood;
                    e["score"] = sr.combinedScore;
                    if (!sr.matchedTags.empty()) {
                        e["matchedTags"] = sr.matchedTags;
                    }
                    arr.push_back(std::move(e));
                }
            }

            // 2. Directory crawler fallback for unindexed files
            if (arr.size() < maxResults) {
                if (!base.empty()) {
                    const auto results = model.search(base, query, audioOnly, maxResults - arr.size(), cancel.get());
                    if (cancel->load() || gen != searchGen.load())
                        return;
                    for (const auto& f : results) {
                        if (seenPaths.find(f.path) != seenPaths.end())
                            continue;
                        seenPaths.insert(f.path);
                        arr.push_back(entryToJson(f));
                    }
                } else {
                    // Global Search across all configured roots
                    const auto allRoots = model.roots();
                    for (const auto& r : allRoots) {
                        if (cancel->load() || gen != searchGen.load() || arr.size() >= maxResults)
                            break;
                        if (r.path.empty())
                            continue;
                        const auto results = model.search(r.path, query, audioOnly, maxResults - arr.size(), cancel.get());
                        if (cancel->load() || gen != searchGen.load())
                            return;
                        for (const auto& f : results) {
                            if (seenPaths.find(f.path) != seenPaths.end())
                                continue;
                            seenPaths.insert(f.path);
                            arr.push_back(entryToJson(f));
                        }
                    }
                }
            }

            if (cancel->load() || gen != searchGen.load())
                return;
            json ev;
            ev["event"] = "browser.searchResult";
            ev["data"] = {{"gen", gen}, {"results", arr}};
            st->pushEvent(ev);
        });
    }
};

Bridge::Bridge(IHostActions* actions, std::string browserStorePath)
    : m_actions(actions), m_impl(std::make_unique<Impl>()) {
    m_impl->actions = actions;
    if (!browserStorePath.empty()) {
        m_impl->model.setStorePath(std::move(browserStorePath));
    }
}

Bridge::~Bridge() = default;

void Bridge::init() {
    if (m_impl) {
        audio::Engine::instance().init(m_actions == nullptr);
        m_impl->model.loadStore();
        m_impl->db.open();
        m_impl->searchEngine = std::make_unique<search::SearchEngine>(
            std::shared_ptr<db::Database>(&m_impl->db, [](db::Database*){})
        );
        m_impl->scanner = std::make_unique<scanner::BackgroundScanner>(m_impl->db);

        // One-time asynchronous background repair pass to restore corrupted F Major and erroneous BPMs in database
        static std::atomic<bool> s_repairDone{false};
        if (!s_repairDone.exchange(true) && m_impl->db.isOpen()) {
            m_impl->spawnWorker([this]() {
                try {
                    scanner::BackgroundScanner::repairDatabaseMetadata(m_impl->db);
                } catch (...) {}
            });
        }

        auto st = m_impl->state;
        auto* se = m_impl->searchEngine.get();
        m_impl->scanner->setProgressCallback([st, se](const scanner::ScanProgress& prog) {
            if (!st) return;
            if (se && prog.isComplete) {
                se->refreshIndex();
            }
            json ev;
            ev["event"] = "scanner.progress";
            ev["data"] = {
                {"total", prog.totalFiles},
                {"processed", prog.processedFiles},
                {"added", prog.addedCount},
                {"skipped", prog.skippedCount},
                {"errors", prog.errorCount},
                {"currentFile", prog.currentFile},
                {"isComplete", prog.isComplete},
                {"isScanning", !prog.isComplete && !prog.isCancelled}
            };
            st->pushEvent(ev);
        });
    }
}

std::vector<std::string> Bridge::drainEvents() {
    std::vector<std::string> out;
    if (!m_impl || !m_impl->state) return out;
    const std::lock_guard lock(m_impl->state->evMutex);
    while (!m_impl->state->events.empty()) {
        out.push_back(std::move(m_impl->state->events.front()));
        m_impl->state->events.pop_front();
    }
    return out;
}

bool Bridge::isAudioActive() const {
    if (reals::audio::Engine::instance().isPlaying())
        return true;
    return m_actions && m_actions->isHostPreviewPlaying();
}

std::string Bridge::audioStateJson() const {
    auto& eng = reals::audio::Engine::instance();
    const bool isHostPlaying = (m_actions && m_actions->isHostPreviewPlaying());

    nlohmann::json j;
    j["event"] = "audio.state";
    nlohmann::json d;
    if (isHostPlaying) {
        d["playing"] = true;
        d["position"] = m_actions->hostPreviewPositionFraction();
        const float pk = m_actions->hostPreviewPeak();
        d["peak"] = pk;
        d["rms"] = pk * 0.707f;
        d["aboveThreshold"] = (pk > 0.01f);
        // Engine track is empty on this path (engine is stopped while the host
        // preview owns playback) — report the mirrored preview duration instead
        // so the UI never picks up a stale previous-track duration.
        {
            const std::lock_guard lock(m_impl->syncMutex);
            d["duration"] = m_impl->previewDurationSeconds;
        }
        const double hostRatio = m_actions->hostPreviewTimeRatio();
        const double hostPitch = m_actions->hostPreviewPitchSemitones();
        d["pitchSemitones"] = static_cast<float>(hostPitch != 0.0 ? hostPitch : eng.getPitchSemitones());
        d["timeRatio"] = static_cast<float>(hostRatio != 1.0 ? hostRatio : eng.getTimeRatio());
        d["syncBpm"] = (d["timeRatio"] != 1.0f);
    } else {
        const reals::audio::LevelState lvl = eng.level();
        d["playing"] = eng.isPlaying();
        d["position"] = eng.positionFraction();
        d["peak"] = lvl.peak;
        d["rms"] = lvl.rms;
        d["aboveThreshold"] = (lvl.rms > 0.01f);
        d["duration"] = eng.currentTrack().durationSeconds;
        d["pitchSemitones"] = eng.getPitchSemitones();
        d["timeRatio"] = eng.getTimeRatio();
        d["syncBpm"] = (eng.getTimeRatio() != 1.0f);
        if (!eng.isPlaying()) {
            const std::lock_guard lock(m_impl->syncMutex);
            if (m_impl->previewDurationSeconds > 0.0) {
                d["duration"] = m_impl->previewDurationSeconds;
            }
            if (m_impl->syncEnabled && m_impl->lastPhaseFraction >= 0.0) {
                d["position"] = m_impl->lastPhaseFraction;
                d["syncBpm"] = true;
            }
        }
    }
    j["data"] = d;
    return j.dump();
}

bool Bridge::updatePhaseSnapFromHostTransport() {
    if (!m_actions || !m_impl) return false;

    bool syncOn = false;
    double loopBeats = 16.0;
    double sampleBpm = 120.0;
    double durationSeconds = 0.0;
    {
        const std::lock_guard lock(m_impl->syncMutex);
        syncOn = m_impl->syncEnabled;
        loopBeats = m_impl->previewLoopBeats;
        sampleBpm = m_impl->previewSampleBpm;
        durationSeconds = m_impl->previewDurationSeconds;
    }

    if (!syncOn || durationSeconds < 1.0 || sampleBpm <= 30.0) {
        return false;
    }

    const auto transport = m_actions->hostTransport();
    if (transport.bpm <= 30.0) {
        return false;
    }

    const auto now = std::chrono::steady_clock::now();
    if (!m_impl->transportInitialized) {
        m_impl->lastTransportBeats = transport.fullBeats;
        m_impl->lastTransportPos = transport.playPosition;
        m_impl->lastPlayState = transport.playState;
        m_impl->lastTransportCheckTime = now;
        m_impl->transportInitialized = true;
        return false;
    }

    const bool wasPlaying = (m_impl->lastPlayState & 1) != 0;
    const bool isPlaying = transport.isPlaying();
    bool cursorMoved = false;

    if (isPlaying) {
        if (!wasPlaying) {
            cursorMoved = true;
        } else {
            double dt = std::chrono::duration<double>(now - m_impl->lastTransportCheckTime).count();
            dt = std::clamp(dt, 0.0, 1.0);
            double expectedBeatsDelta = dt * (transport.bpm / 60.0);
            double actualBeatsDelta = transport.fullBeats - m_impl->lastTransportBeats;
            double expectedPosDelta = dt;
            double actualPosDelta = transport.playPosition - m_impl->lastTransportPos;

            // Discontinuity detection:
            // 1. Beats or seconds jumped backwards (seek back or loop wrap)
            // 2. Beats or seconds jumped forward beyond playback tolerance
            if (actualBeatsDelta < -0.05 || actualPosDelta < -0.05 ||
                std::abs(actualBeatsDelta - expectedBeatsDelta) > 0.25 ||
                std::abs(actualPosDelta - expectedPosDelta) > 0.15) {
                cursorMoved = true;
            }
        }
    } else {
        if (wasPlaying) {
            cursorMoved = true;
        } else {
            double posDelta = std::abs(transport.playPosition - m_impl->lastTransportPos);
            double beatsDelta = std::abs(transport.fullBeats - m_impl->lastTransportBeats);
            if (posDelta > 0.005 || beatsDelta > 0.01) {
                cursorMoved = true;
            }
        }
    }

    m_impl->lastTransportBeats = transport.fullBeats;
    m_impl->lastTransportPos = transport.playPosition;
    m_impl->lastPlayState = transport.playState;
    m_impl->lastTransportCheckTime = now;

    if (!cursorMoved) {
        return false;
    }

    if (loopBeats <= 0.0) loopBeats = 16.0;
    double beatInLoop = std::fmod(transport.fullBeats, loopBeats);
    if (beatInLoop < 0.0) beatInLoop += loopBeats;

    if (isPlaying && transport.blockLatencySeconds > 0.0 && transport.bpm > 30.0) {
        beatInLoop += transport.blockLatencySeconds * transport.bpm / 60.0;
        beatInLoop = std::fmod(beatInLoop, loopBeats);
        if (beatInLoop < 0.0) beatInLoop += loopBeats;
    }

    const double targetFrac = std::clamp(beatInLoop / loopBeats, 0.0, 0.999);

    {
        const std::lock_guard lock(m_impl->syncMutex);
        m_impl->lastPhaseFraction = targetFrac;
    }

    const bool previewPlaying = m_actions->isHostPreviewPlaying();
    const bool enginePlaying = reals::audio::Engine::instance().isPlaying();

    if (previewPlaying) {
        double projectBpm = transport.bpm > 30.0 ? transport.bpm : m_actions->projectTempo();
        double seekReferenceSec = 0.0;
        if (loopBeats > 0.0 && projectBpm > 30.0) {
            seekReferenceSec = loopBeats * 60.0 / projectBpm;
        } else {
            seekReferenceSec = durationSeconds;
        }
        const double targetPosSec = targetFrac * (seekReferenceSec > 0.0 ? seekReferenceSec : 1.0);
        m_actions->setHostPreviewPosition(targetPosSec);
        m_actions->setHostPreviewPositionFraction(targetFrac);
    }

    if (enginePlaying) {
        reals::audio::Engine::instance().seekFraction(targetFrac);
    }

    LOG_INFO("SYNC_DIAG", "updatePhaseSnapFromHostTransport: re-aligned to targetFrac=" +
             std::to_string(targetFrac) + " fullBeats=" + std::to_string(transport.fullBeats) +
             " isPlaying=" + std::to_string(isPlaying) + " previewPlaying=" + std::to_string(previewPlaying));

    return true;
}

std::string Bridge::handle(const std::string& requestJson) {
    auto& eng = audio::Engine::instance();
    auto& cfg = config::Config::instance();
    auto& model = m_impl->model;
    json req;
    json res;
    try {
        req = json::parse(requestJson, nullptr, false);
    } catch (...) {
        res["id"] = 0;
        res["ok"] = false;
        res["error"] = "invalid json format";
        return res.dump();
    }
    res["id"] = req.contains("id") ? req["id"] : json(0);

    if (req.is_discarded() || !req.contains("cmd")) {
        res["ok"] = false;
        res["error"] = "bad request";
        return res.dump();
    }
    const std::string cmd = req.value("cmd", "");
    const json& args = req.contains("args") ? req["args"] : json::object();

    try {
        if (cmd == "app.info") {
            json d;
            d["version"] = "0.2.0";
            d["platform"] = "windows";
            res["ok"] = true;
            res["data"] = d;
        } else if (cmd == "config.getAll") {
            json d;
            d["language"] = cfg.language();
            d["navPosition"] = cfg.navPosition();
            d["accent"] = cfg.accent();
            d["noiseOverlay"] = cfg.noiseOverlay();
            res["ok"] = true;
            res["data"] = d;
        } else if (cmd == "config.set") {
            // Guard against a missing "value" key: const operator[] on a
            // nlohmann object with an absent key is undefined behaviour.
            cfg.set(args.value("key", ""),
                    args.contains("value") ? args["value"] : json(nullptr));
            res["ok"] = true;
        } else if (cmd == "fs.roots") {
            json arr = json::array();
            for (const auto& r : model.roots())
                arr.push_back({{"name", r.name}, {"path", r.path}});
            res["ok"] = true;
            res["data"] = arr;
        } else if (cmd == "fs.addRoot") {
            std::string rawP = narrowPath(args.value("path", ""));
            while (rawP.size() > 3 && (rawP.back() == '\\' || rawP.back() == '/'))
                rawP.pop_back();
            std::error_code ec;
            fs::path p = platform::u8path(rawP);
            if (fs::exists(p, ec)) {
                bool wasFile = false;
                if (fs::is_regular_file(p, ec)) {
                    p = p.parent_path();
                    wasFile = true;
                } else if (p.filename().empty() && p != p.root_path()) {
                    p = p.parent_path();
                }
                std::string finalPath = platform::normalizePath(platform::pathToUtf8(p));
                std::string name = args.value("name", "");
                if (name.empty() || name == rawP || wasFile) {
                    name = platform::pathToUtf8(p.filename());
                    if (name.empty())
                        name = finalPath;
                }
                model.addRoot(name, finalPath);
            } else {
                model.addRoot(args.value("name", ""), rawP);
            }
            res["ok"] = true;
        } else if (cmd == "fs.dropPaths") {
            json added = json::array();
            const json& paths = args.contains("paths") && args["paths"].is_array()
                                    ? args["paths"]
                                    : json::array();
            for (const auto& item : paths) {
                std::string rawP = narrowPath(item.is_string() ? item.get<std::string>() : "");
                while (rawP.size() > 3 && (rawP.back() == '\\' || rawP.back() == '/'))
                    rawP.pop_back();
                if (rawP.empty())
                    continue;
                std::error_code ec;
                fs::path p = platform::u8path(rawP);
                if (!fs::exists(p, ec))
                    continue;
                if (fs::is_regular_file(p, ec))
                    p = p.parent_path();
                if (!fs::is_directory(p, ec))
                    continue;
                const std::string finalPath = platform::normalizePath(platform::pathToUtf8(p));
                std::string name = platform::pathToUtf8(p.filename());
                if (name.empty())
                    name = finalPath;
                if (model.addRoot(name, finalPath))
                    added.push_back({{"name", name}, {"path", finalPath}});
            }
            json ev;
            ev["event"] = "fs.rootsChanged";
            ev["data"] = {{"added", added}};
            m_impl->state->pushEvent(ev);
            res["ok"] = true;
            res["data"] = {{"added", added}};
        } else if (cmd == "fs.removeRoot") {
            const std::string p = narrowPath(args.value("path", ""));
            const auto& rts = model.roots();
            for (size_t i = 0; i < rts.size(); ++i) {
                if (rts[i].path == p) {
                    model.removeRoot(i);
                    break;
                }
            }
            res["ok"] = true;
        } else if (cmd == "fs.subdirs") {
            json arr = json::array();
            for (const auto& s : platform::listSubdirs(args.value("path", "")))
                arr.push_back(s);
            res["ok"] = true;
            res["data"] = arr;
        } else if (cmd == "fs.list") {
            // Optional sort: 0=name 1=size 2=date. Changing it rebuilds the
            // cached listing (the model caches by directory).
            const int sortIdx = args.value("sort", -1);
            if (sortIdx >= 0 && sortIdx <= 2 &&
                sortIdx != static_cast<int>(model.sort())) {
                model.setSort(static_cast<browser::BrowserModel::Sort>(sortIdx));
                model.invalidateAll();
            }
            auto files = model.listDir(narrowPath(args.value("path", "")));

            // CRIT-METADATA-HYDRATE: Hydrate metadata from SQLite database for all audio files in the folder.
            // Do NOT remove this! Without this batch query, directory browsing leaves BPM/Key/Duration empty (0.0%),
            // starving the frontend and forcing it to guess using fragile filename regexes.
            std::vector<std::string> audioPaths;
            audioPaths.reserve(files.size());
            for (const auto& f : files) {
                if (f.isAudio && !f.isDir) {
                    audioPaths.push_back(f.path);
                }
            }
            if (!audioPaths.empty()) {
                auto metaMap = m_impl->db.getSamplesByPaths(audioPaths);
                for (auto& f : files) {
                    if (f.isAudio && !f.isDir) {
                        auto it = metaMap.find(f.path);
                        if (it != metaMap.end()) {
                            const auto& rec = it->second;
                            f.bpm = static_cast<float>(rec.bpm);
                            f.key = rec.keyRoot.empty() ? "" : (rec.keyMode == "minor" ? rec.keyRoot + "m" : rec.keyRoot);
                            f.camelot = rec.camelot;
                            f.durationSec = rec.durationSec;
                        }
                    }
                }
            }

            json arr = json::array();
            for (const auto& f : files)
                arr.push_back(entryToJson(f));
            res["ok"] = true;
            res["data"] = arr;
        } else if (cmd == "fs.invalidate") {
            model.invalidate(narrowPath(args.value("path", "")));
            res["ok"] = true;
        } else if (cmd == "fs.watch") {
            const std::string dir = narrowPath(args.value("path", ""));
            auto st = m_impl->state;
            m_impl->watch.start(dir, [st](const std::string& d) {
                json ev;
                ev["event"] = "fs.changed";
                ev["data"] = {{"path", d}};
                st->pushEvent(ev);
            });
            res["ok"] = true;
        } else if (cmd == "browser.search") {
            const std::string base = narrowPath(args.value("base", ""));
            const std::string query = args.value("query", "");
            const bool audioOnly = args.value("audioOnly", false);
            const size_t maxResults = static_cast<size_t>(args.value("maxResults", 400));
            const uint64_t gen = args.value("gen", static_cast<uint64_t>(0));
            m_impl->runSearch(base, query, audioOnly, maxResults, gen);
            json d;
            d["pending"] = true;
            d["gen"] = m_impl->searchGen.load();
            res["ok"] = true;
            res["data"] = d;
        } else if (cmd == "browser.suggestTags") {
            const std::string q = args.value("query", "");
            json arr = json::array();
            const std::vector<std::string> defaultTags = {
                "/bpm:120-130", "/bpm:140", "/key:Am", "/key:F#m", "/key:C", "/fav",
                "/trap", "/lo-fi", "/hiphop", "/house", "/drill", "/ambient", "/acoustic",
                "/edm", "/rock", "/kick", "/snare", "/hihat", "/808", "/vocal", "/bass",
                "/synth", "/piano", "/strings", "/guitar", "/brass", "/flute", "/pad",
                "/lead", "/pluck", "/drum", "/loop", "/oneshot"
            };
            for (const auto& tag : defaultTags) {
                if (q.empty() || tag.find(q) != std::string::npos) {
                    arr.push_back(tag);
                }
            }
            res["ok"] = true;
            res["data"] = {{"suggestions", arr}};
        } else if (cmd == "browser.favorites") {
            json arr = json::array();
            for (const auto& f : model.favorites())
                arr.push_back(f);
            res["ok"] = true;
            res["data"] = arr;
        } else if (cmd == "browser.getFavoriteEntries" || cmd == "browser.favorites.listEntries" || cmd == "browser.listFavorites") {
            const auto files = model.getFavoriteEntries();
            json arr = json::array();
            for (const auto& f : files)
                arr.push_back(entryToJson(f));
            res["ok"] = true;
            res["data"] = {{"files", arr}};
            res["result"] = {{"files", arr}};
        } else if (cmd == "browser.toggleFavorite") {
            model.toggleFavorite(narrowPath(args.value("path", "")));
            res["ok"] = true;
            res["data"] = model.isFavorite(narrowPath(args.value("path", "")));
        } else if (cmd == "browser.recents") {
            json arr = json::array();
            for (const auto& r : model.recents())
                arr.push_back(r);
            res["ok"] = true;
            res["data"] = arr;
        } else if (cmd == "browser.addRecent") {
            model.addRecent(narrowPath(args.value("path", "")));
            res["ok"] = true;
        } else if (cmd == "browser.clearRecents") {
            model.clearRecents();
            res["ok"] = true;
        } else if (cmd == "browser.tag") {
            model.setTag(narrowPath(args.value("path", "")), args.value("color", 0));
            res["ok"] = true;
        } else if (cmd == "browser.tags") {
            json d = json::object();
            if (args.contains("path")) {
                // tags are internal; expose per-path on demand
                d["ofPath"] = model.tagOf(narrowPath(args.value("path", "")));
            } else {
                // Full map so the UI can render tag dots in one round trip.
                json all = json::object();
                for (const auto& [path, color] : model.tags())
                    all[path] = color;
                d["tags"] = all;
            }
            res["ok"] = true;
            res["data"] = d;
        } else if (cmd == "browser.rename") {
            const std::string from = narrowPath(args.value("from", ""));
            const std::string to = narrowPath(args.value("to", ""));
            std::error_code ec;
            fs::rename(platform::u8path(from), platform::u8path(to), ec);
            model.rewritePath(from, to);
            model.invalidate(platform::pathToUtf8(platform::u8path(from).parent_path()));
            res["ok"] = !ec;
            if (ec)
                res["error"] = "rename failed";
        } else if (cmd == "browser.delete") {
            const std::string p = narrowPath(args.value("path", ""));
            std::error_code ec;
            fs::remove(platform::u8path(p), ec);
            model.forgetPath(p);
            model.invalidate(platform::pathToUtf8(platform::u8path(p).parent_path()));
            res["ok"] = !ec;
            if (ec)
                res["error"] = "delete failed";
        } else if (cmd == "audio.play") {
            const std::string p = narrowPath(args.value("path", ""));
            LOG_INFO("bridge", "handling audio.play for path: " + p + " args=" + args.dump());

            bool syncOn = args.value("syncBpm", false);
            {
                const std::lock_guard lock(m_impl->syncMutex);
                if (args.contains("syncBpm")) {
                    m_impl->syncEnabled = syncOn;
                } else if (m_impl->syncEnabled) {
                    syncOn = true;
                }
                LOG_INFO("SYNC_DIAG", "audio.play check: args.syncBpm=" + std::to_string(args.value("syncBpm", false)) +
                                      " m_impl->syncEnabled=" + std::to_string(m_impl->syncEnabled));
            }

            const float pitchShift = static_cast<float>(args.value("pitchSemitones", static_cast<double>(eng.getPitchSemitones())));
            eng.setPitchSemitones(pitchShift);
            float sampleBpm = args.value("sampleBpm", 0.0f);
            double projectBpm = 0.0;
            if (syncOn) {
                if (sampleBpm <= 0.0f) {
                    sampleBpm = m_impl->detectBpmForPath(p);
                }
                if (args.contains("bpm") && args.value("bpm", 0.0) > 30.0) {
                    projectBpm = args.value("bpm", 0.0);
                } else if (m_actions) {
                    const auto transport = m_actions->hostTransport();
                    if (transport.bpm > 30.0) projectBpm = transport.bpm;
                    else projectBpm = m_actions->projectTempo();
                }

                if (sampleBpm <= 0.0f && projectBpm > 0.0) {
                    sampleBpm = static_cast<float>(projectBpm);
                }

                if (projectBpm > 30.0 && sampleBpm > 30.0) {
                    float ratio = std::clamp(static_cast<float>(projectBpm / sampleBpm), 0.25f, 4.0f);
                    if (std::abs(projectBpm - sampleBpm) / projectBpm < 0.003) {
                        ratio = 1.0f; // Bit-perfect bypass when tempo difference < 0.3%
                    }
                    eng.setTimeRatio(ratio);
                    {
                        const std::lock_guard lock(m_impl->syncMutex);
                        m_impl->syncRatio = ratio;
                        m_impl->syncSampleBpm = sampleBpm;
                    }
                    LOG_INFO("SYNC_DIAG", "[AUDIO_PLAY] path=" + p +
                                          " projectBpm=" + std::to_string(projectBpm) +
                                          " sampleBpm=" + std::to_string(sampleBpm) +
                                          " ratio=" + std::to_string(ratio) +
                                          " pitchSemitones=" + std::to_string(pitchShift) +
                                          " mode=" + std::string(ratio == 1.0f && pitchShift == 0.0f ? "BIT_PERFECT_BYPASS" : "REAPER_ELASTIQUE_DSP"));
                }
            } else {
                eng.setTimeRatio(1.0f);
            }

            const auto info = audio::Engine::probeFile(p);

            // Phase synchronization via a late PhaseAnchor: the anchor runs
            // inside Engine::playFile AFTER the file is decoded and the DSP
            // chain is configured, but BEFORE the seek and sound start. The
            // DAW transport is sampled there — the last possible moment — so
            // the preview cannot lag behind the playhead by the decode time.
            bool phaseSynced = false;
            double loopBeats = 4.0;
            double startFraction = 0.0;
            uint64_t nominalLoopFrames = 0;
            audio::Engine::PhaseAnchor phaseAnchor;
            if (syncOn && m_actions && sampleBpm > 30.0f && info.durationSeconds >= 1.0) {
                const double rawBeats = (info.durationSeconds * sampleBpm) / 60.0;
                const auto transportNow = m_actions->hostTransport();
                const int timeSig = transportNow.beatsPerMeasure > 0 ? transportNow.beatsPerMeasure : 4;

                static const double kStandardBars[] = { 0.25, 0.5, 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0, 24.0, 32.0, 64.0 };
                double resolvedBeats = 0.0;
                // Check tight tolerance first (near-exact standard power-of-2 / standard phrase loops)
                for (double sb : kStandardBars) {
                    const double target = sb * static_cast<double>(timeSig);
                    const double tol = std::max(static_cast<double>(timeSig) / 16.0, target * 0.04);
                    if (std::abs(rawBeats - target) <= tol) {
                        resolvedBeats = target;
                        break;
                    }
                }
                // If not tightly matched, check if rawBeats has release tail / padding on a standard bar
                // (e.g. 4-bar loop of 16 beats with reverb tail up to 20% / 1-2 beats: 16.0 <= rawBeats <= 19.2)
                if (resolvedBeats <= 0.0) {
                    for (double sb : kStandardBars) {
                        const double target = sb * static_cast<double>(timeSig);
                        const double tailAllowance = std::max(1.0, target * 0.20);
                        if (rawBeats >= target - 0.25 && rawBeats <= target + tailAllowance) {
                            resolvedBeats = target;
                            break;
                        }
                    }
                }
                if (resolvedBeats <= 0.0) {
                    // Fallback to nearest integer beat count
                    resolvedBeats = std::max(1.0, std::round(rawBeats));
                }
                loopBeats = resolvedBeats;

                if (sampleBpm > 0.0f && info.sampleRate > 0) {
                    const double nominalLoopSec = (loopBeats * 60.0) / sampleBpm;
                    nominalLoopFrames = static_cast<uint64_t>(nominalLoopSec * info.sampleRate);
                }

                LOG_INFO("SYNC_DIAG",
                         "BAR_QUANTIZE: fileDur=" + std::to_string(info.durationSeconds) +
                         "s sampleBpm=" + std::to_string(sampleBpm) +
                         " rawBeats=" + std::to_string(rawBeats) +
                         " -> loopBeats=" + std::to_string(loopBeats) +
                         " nominalLoopFrames=" + std::to_string(nominalLoopFrames) +
                         " totalFrames=" + std::to_string(info.totalFrames));

                phaseAnchor = [this, &eng, &phaseSynced, loopBeats, nominalLoopFrames, sampleBpm, &startFraction](double /*presetFraction*/) -> double {
                    const auto transport = m_actions->hostTransport();
                    if (!transport.isPlaying() || transport.bpm <= 30.0) {
                        phaseSynced = false;
                        startFraction = 0.0;
                        LOG_INFO("SYNC_DIAG", "PHASE_ANCHOR: transport not playing or invalid BPM=" + std::to_string(transport.bpm));
                        return 0.0;
                    }

                    double beatInLoop = std::fmod(transport.fullBeats, loopBeats);
                    if (beatInLoop < 0.0)
                        beatInLoop += loopBeats;

                    startFraction = std::clamp(beatInLoop / loopBeats, 0.0, 0.999);
                    phaseSynced = true;

                    const uint64_t computedStartFrame = static_cast<uint64_t>(startFraction * nominalLoopFrames);

                    LOG_INFO("SYNC_DIAG",
                             "PHASE_ANCHOR_EXEC: playState=" + std::to_string(transport.playState) +
                             " pos(s)=" + std::to_string(transport.playPosition) +
                             " fullBeats=" + std::to_string(transport.fullBeats) +
                             " dawBpm=" + std::to_string(transport.bpm) +
                             " beatInLoop=" + std::to_string(beatInLoop) +
                             " startFraction=" + std::to_string(startFraction) +
                             " startFrame=" + std::to_string(computedStartFrame));

                    return startFraction;
                };
            }

            // Always prefer 100% Native REAPER PlayPreviewEx for mastering-grade r8brain resampling,
            // élastique 3 Pro DSP, and Monitoring FX routing!
            bool ok = false;
            if (m_actions) {
                eng.stop(); // Ensure custom core engine is idle so it does not mix redundantly

                // --- Phase-snap for host preview path (PhaseAnchor only runs inside eng.playFile fallback) ---
                // Snap to DAW beat position whether transport is playing OR stopped (cursor position).
                // Require the same >= 1.0 s minimum as the bar-quantize block above: short
                // one-shots leave loopBeats at its 4.0 default and must bypass phase sync.
                if (syncOn && sampleBpm > 30.0f && loopBeats > 0.0 && info.durationSeconds >= 1.0) {
                    const auto transport = m_actions->hostTransport();
                    LOG_INFO("SYNC_DIAG",
                             "HOST_PHASE_CHECK: syncOn=1 sampleBpm=" + std::to_string(sampleBpm) +
                             " loopBeats=" + std::to_string(loopBeats) +
                             " playState=" + std::to_string(transport.playState) +
                             " isPlaying=" + std::to_string(transport.isPlaying()) +
                             " bpm=" + std::to_string(transport.bpm) +
                             " fullBeats=" + std::to_string(transport.fullBeats));
                    if (transport.bpm > 30.0 && transport.fullBeats >= 0.0) {
                        double beatInLoop = std::fmod(transport.fullBeats, loopBeats);
                        if (beatInLoop < 0.0)
                            beatInLoop += loopBeats;
                        // The transport snapshot is captured before PlayPreviewEx opens the
                        // source and primes its DSP, so it is at least one host audio block
                        // stale by the time the preview is audible. Advance the phase by that
                        // block so the preview lands on the playhead instead of behind it.
                        if (transport.isPlaying() && transport.blockLatencySeconds > 0.0) {
                            beatInLoop += transport.blockLatencySeconds * transport.bpm / 60.0;
                            beatInLoop = std::fmod(beatInLoop, loopBeats);
                        }
                        startFraction = std::clamp(beatInLoop / loopBeats, 0.0, 0.999);
                        phaseSynced = true;
                        LOG_INFO("SYNC_DIAG",
                                 "HOST_PHASE_SNAP: fullBeats=" + std::to_string(transport.fullBeats) +
                                 " loopBeats=" + std::to_string(loopBeats) +
                                 " beatInLoop=" + std::to_string(beatInLoop) +
                                 " startFraction=" + std::to_string(startFraction));
                    }
                } else {
                    LOG_INFO("SYNC_DIAG",
                             "HOST_PHASE_SKIP: syncOn=" + std::to_string(syncOn) +
                             " sampleBpm=" + std::to_string(sampleBpm) +
                             " loopBeats=" + std::to_string(loopBeats));
                }

                // startFraction is a phase within the bar-quantized NOMINAL loop, so the
                // seek reference must be the nominal loop duration, not the full file. A
                // loop with a reverb tail has outputDuration > nominalLoopSec; multiplying
                // by the full duration would offset the start by the tail (beats of drift).
                const double timeRatio = static_cast<double>(eng.getTimeRatio());
                const double outputDuration = (timeRatio > 0.01 && info.durationSeconds > 0.0)
                    ? (info.durationSeconds / timeRatio)
                    : info.durationSeconds;
                double seekReferenceSec = outputDuration;
                if (loopBeats > 0.0 && projectBpm > 30.0) {
                    seekReferenceSec = loopBeats * 60.0 / projectBpm;
                }
                const double startPosSec = startFraction * (seekReferenceSec > 0.0 ? seekReferenceSec : 1.0);
                LOG_INFO("SYNC_DIAG",
                         "HOST_PLAY_POS: startFraction=" + std::to_string(startFraction) +
                         " rawDurSec=" + std::to_string(info.durationSeconds) +
                         " outputDurSec=" + std::to_string(outputDuration) +
                         " seekRefSec=" + std::to_string(seekReferenceSec) +
                         " ratio=" + std::to_string(timeRatio) +
                         " startPosSec=" + std::to_string(startPosSec) +
                         " phaseSynced=" + std::to_string(phaseSynced));
                ok = m_actions->playHostPreview(p, args.value("loop", false), startPosSec,
                                                eng.volume(), static_cast<double>(eng.getTimeRatio()),
                                                static_cast<double>(eng.getPitchSemitones()),
                                                sampleBpm, loopBeats, nominalLoopFrames);
                if (ok) {
                    LOG_INFO("bridge", "audio.play launched REAPER native preview (élastique/r8brain) for: " + p);
                }
            }

            // Fallback for standalone app (no REAPER host) or if native preview failed
            if (!ok) {
                if (m_actions) m_actions->stopHostPreview();
                ok = eng.playFile(p, args.value("loop", false), startFraction, phaseAnchor, nominalLoopFrames);
                LOG_INFO("bridge", "audio.play launched custom DSP engine (standalone fallback) for: " + p);
            }
            if (ok) {
                const std::lock_guard lock(m_impl->syncMutex);
                m_impl->previewPath = p;
                m_impl->previewDurationSeconds = info.durationSeconds > 0.0 ? info.durationSeconds : eng.currentTrack().durationSeconds;
                m_impl->previewLoopBeats = loopBeats;
                m_impl->previewSampleBpm = static_cast<double>(sampleBpm);
                m_impl->previewNominalLoopFrames = nominalLoopFrames;
                m_impl->lastPhaseFraction = startFraction;
            }
            LOG_INFO("bridge", "audio.play ok=" + std::to_string(ok));
            model.addRecent(p);
            json d;
            d["ok"] = ok;
            d["duration"] = info.durationSeconds > 0.0 ? info.durationSeconds : eng.currentTrack().durationSeconds;
            d["sampleRate"] = info.sampleRate > 0 ? info.sampleRate : eng.currentTrack().sampleRate;
            d["channels"] = info.channels > 0 ? info.channels : eng.currentTrack().channels;
            d["startFraction"] = startFraction;
            d["phaseSynced"] = phaseSynced;
            d["loopBeats"] = loopBeats;
            // Output playback rate so the UI can advance its playhead at the
            // real speed (output duration = raw duration / timeRatio).
            d["timeRatio"] = eng.getTimeRatio();

            std::vector<float> cachedEnv;
            {
                const std::lock_guard lock(m_impl->cacheMutex);
                auto it = m_impl->envCache.find(p);
                if (it != m_impl->envCache.end())
                    cachedEnv = it->second;
            }

            if (!cachedEnv.empty()) {
                d["envelope"] = cachedEnv;
            } else {
                m_impl->runEnvelopeScan(p);
            }
            res["ok"] = true;
            res["data"] = d;
        } else if (cmd == "audio.stop") {
            if (m_actions) m_actions->stopHostPreview();
            eng.stop();
            res["ok"] = true;
        } else if (cmd == "audio.setLoop") {
            const bool loop = args.value("value", false);
            if (m_actions) m_actions->setHostPreviewLoop(loop);
            eng.setLoop(loop);
            res["ok"] = true;
        } else if (cmd == "audio.setVolume") {
            const float vol = args.value("value", 1.0f);
            if (m_actions) m_actions->setHostPreviewVolume(vol);
            eng.setVolume(vol);
            res["ok"] = true;
        } else if (cmd == "audio.probe") {
            const std::string p = narrowPath(args.value("path", ""));
            const std::string lowerP = platform::toLowerUtf8(p);
            if (lowerP.ends_with(".mid") || lowerP.ends_with(".midi")) {
                json d;
                d["duration"] = 0.0;
                d["sampleRate"] = 0;
                d["channels"] = 0;
                d["ok"] = false;
                res["ok"] = true;
                res["data"] = d;
            } else {
                audio::TrackInfo info;
                bool found = false;
                std::vector<float> env;
                {
                    const std::lock_guard lock(m_impl->cacheMutex);
                    auto it = m_impl->probeCache.find(p);
                    if (it != m_impl->probeCache.end()) {
                        info = it->second;
                        found = true;
                    }
                    auto itEnv = m_impl->envCache.find(p);
                    if (itEnv != m_impl->envCache.end()) {
                        env = itEnv->second;
                    }
                }
                if (!found) {
                    info = audio::Engine::probeFile(p);
                    if (info.sampleRate > 0) {
                        const std::lock_guard lock(m_impl->cacheMutex);
                        m_impl->probeCache[p] = info;
                    }
                }
                if (env.empty() && info.sampleRate > 0) {
                    m_impl->runEnvelopeScan(p);
                }
                json d;
                d["duration"] = info.durationSeconds;
                d["sampleRate"] = info.sampleRate;
                d["channels"] = info.channels;
                d["envelope"] = env;
                d["ok"] = info.sampleRate > 0;
                res["ok"] = true;
                res["data"] = d;
            }
        } else if (cmd == "audio.readMidi" || cmd == "fs.readBase64") {
            const std::string p = narrowPath(args.value("path", ""));
            std::ifstream file(platform::u8path(p), std::ios::binary);
            if (file.is_open()) {
                std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                static const char base64_chars[] =
                    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                    "abcdefghijklmnopqrstuvwxyz"
                    "0123456789+/";
                std::string b64;
                b64.reserve(((buffer.size() + 2) / 3) * 4);
                size_t i = 0;
                while (i < buffer.size()) {
                    uint32_t octet_a = i < buffer.size() ? buffer[i++] : 0;
                    uint32_t octet_b = i < buffer.size() ? buffer[i++] : 0;
                    uint32_t octet_c = i < buffer.size() ? buffer[i++] : 0;
                    uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;
                    b64.push_back(base64_chars[(triple >> 18) & 0x3F]);
                    b64.push_back(base64_chars[(triple >> 12) & 0x3F]);
                    b64.push_back(i > buffer.size() + 1 ? '=' : base64_chars[(triple >> 6) & 0x3F]);
                    b64.push_back(i > buffer.size() ? '=' : base64_chars[triple & 0x3F]);
                }
                json d;
                d["base64"] = b64;
                d["size"] = buffer.size();
                res["ok"] = true;
                res["data"] = d;
            } else {
                res["ok"] = false;
                res["error"] = "Cannot open file";
            }
        } else if (cmd == "audio.seek") {
            const double frac = args.value("fraction", 0.0);
            if (m_actions) m_actions->setHostPreviewPositionFraction(frac);
            eng.seekFraction(frac);
            res["ok"] = true;
        } else if (cmd == "audio.setPitchShift") {
            const float semitones = args.value("semitones", 0.0f);
            if (m_actions) m_actions->setHostPreviewPitchSemitones(static_cast<double>(semitones));
            eng.setPitchSemitones(semitones);
            json d;
            d["pitchSemitones"] = eng.getPitchSemitones();
            res["ok"] = true;
            res["data"] = d;

            json ev;
            ev["event"] = "audio.syncState";
            {
                const std::lock_guard lock(m_impl->syncMutex);
                ev["data"] = {
                    {"syncBpm", m_impl->syncEnabled},
                    {"projectBpm", m_actions ? m_actions->projectTempo() : 0.0},
                    {"sampleBpm", 0.0f},
                    {"ratio", eng.getTimeRatio()},
                    {"semitones", eng.getPitchSemitones()}
                };
            }
            m_impl->state->pushEvent(ev);
        } else if (cmd == "audio.setSyncBpm") {
            const bool enabled = args.value("enabled", false);
            float ratio = 1.0f;
            float projectBpm = args.value("bpm", 0.0f);
            if (projectBpm <= 0.0f && m_actions) {
                const auto transport = m_actions->hostTransport();
                if (transport.bpm > 30.0) projectBpm = static_cast<float>(transport.bpm);
                else projectBpm = static_cast<float>(m_actions->projectTempo());
            }
            float sampleBpm = args.value("sampleBpm", 0.0f);
            std::string syncPath = args.value("path", "");
            if (syncPath.empty()) {
                syncPath = eng.currentTrack().path;
            }
            // Auto-detect BPM if missing and sync is being enabled
            if (enabled && sampleBpm <= 0.0f) {
                const std::string target = !syncPath.empty() ? syncPath : narrowPath(args.value("path", ""));
                if (!target.empty()) {
                    sampleBpm = m_impl->detectBpmForPath(target);
                    // also try current track if still 0
                    if (sampleBpm <= 0.0f && !eng.currentTrack().path.empty()) {
                        sampleBpm = m_impl->detectBpmForPath(eng.currentTrack().path);
                    }
                } else if (!eng.currentTrack().path.empty()) {
                    sampleBpm = m_impl->detectBpmForPath(eng.currentTrack().path);
                }
            }
            if (enabled) {
                if (projectBpm > 0.0f && sampleBpm > 0.0f) {
                    ratio = projectBpm / sampleBpm;
                    if (std::abs(projectBpm - sampleBpm) / projectBpm < 0.003) {
                        ratio = 1.0f; // Bit-perfect bypass when tempo difference < 0.3%
                    }
                } else if (args.contains("ratio")) {
                    ratio = args.value("ratio", 1.0f);
                } else {
                    // If we still have no sampleBpm, fallback to 120
                    if (projectBpm > 0.0f) ratio = projectBpm / 120.0f;
                }
            }
            if (m_actions) m_actions->setHostPreviewTimeRatio(static_cast<double>(ratio));
            eng.setTimeRatio(ratio);
            {
                const std::lock_guard lock(m_impl->syncMutex);
                m_impl->syncEnabled = enabled;
                m_impl->syncRatio = ratio;
                m_impl->syncPath = syncPath;
                m_impl->syncSampleBpm = sampleBpm;
            }
            // Re-align phase if the DAW is transporting and something is playing.
            // On the native path the core Engine is stopped, so also accept a
            // running host preview and source the duration from the stored
            // preview state instead of eng.currentTrack().
            const bool enginePlaying = eng.isPlaying();
            const bool previewPlaying = m_actions && m_actions->isHostPreviewPlaying();
            double rephaseDurationSeconds = 0.0;
            int rephaseSampleRate = 0;
            if (enginePlaying) {
                const auto& trk = eng.currentTrack();
                rephaseDurationSeconds = trk.durationSeconds;
                rephaseSampleRate = trk.sampleRate;
            }
            if (rephaseDurationSeconds < 1.0 && previewPlaying) {
                const std::lock_guard lock(m_impl->syncMutex);
                rephaseDurationSeconds = m_impl->previewDurationSeconds;
            }
            if (enabled && m_actions && (enginePlaying || previewPlaying)) {
                const auto transport = m_actions->hostTransport();
                if (transport.isPlaying() && transport.bpm > 30.0 && sampleBpm > 30.0) {
                    if (rephaseDurationSeconds >= 1.0) {
                        const double rawBeats = (rephaseDurationSeconds * sampleBpm) / 60.0;
                        const int timeSig = transport.beatsPerMeasure > 0 ? transport.beatsPerMeasure : 4;
                        static const double kStandardBars[] = { 0.25, 0.5, 1.0, 2.0, 3.0, 4.0, 6.0, 8.0, 12.0, 16.0, 24.0, 32.0, 64.0 };
                        double resolvedBeats = 0.0;
                        for (double sb : kStandardBars) {
                            const double target = sb * static_cast<double>(timeSig);
                            const double tol = std::max(static_cast<double>(timeSig) / 16.0, target * 0.04);
                            if (std::abs(rawBeats - target) <= tol) {
                                resolvedBeats = target;
                                break;
                            }
                        }
                        if (resolvedBeats <= 0.0) {
                            for (double sb : kStandardBars) {
                                const double target = sb * static_cast<double>(timeSig);
                                const double tailAllowance = std::max(1.0, target * 0.20);
                                if (rawBeats >= target - 0.25 && rawBeats <= target + tailAllowance) {
                                    resolvedBeats = target;
                                    break;
                                }
                            }
                        }
                        if (resolvedBeats <= 0.0) {
                            resolvedBeats = std::max(1.0, std::round(rawBeats));
                        }
                        double beatInLoop = std::fmod(transport.fullBeats, resolvedBeats);
                        if (beatInLoop < 0.0) beatInLoop += resolvedBeats;
                        const double syncFrac = std::clamp(beatInLoop / resolvedBeats, 0.0, 0.999);
                        if (enginePlaying) {
                            const double nominalLoopSec = (resolvedBeats * 60.0) / sampleBpm;
                            const int effectiveSr = (eng.targetSampleRate() > 0) ? eng.targetSampleRate() : rephaseSampleRate;
                            if (effectiveSr > 0) {
                                eng.setLoopBoundaryFrames(static_cast<uint64_t>(nominalLoopSec * effectiveSr));
                            }
                            eng.seekFraction(syncFrac);
                        }
                        // Seek the REAPER native preview to the same bar phase.
                        if (previewPlaying) {
                            m_actions->setHostPreviewPositionFraction(syncFrac);
                            const std::lock_guard lock(m_impl->syncMutex);
                            m_impl->previewLoopBeats = resolvedBeats;
                            m_impl->lastPhaseFraction = syncFrac;
                        }
                    }
                }
            }
            json d;
            d["syncBpm"] = enabled;
            d["ratio"] = ratio;
            d["projectBpm"] = projectBpm;
            d["sampleBpm"] = sampleBpm;
            res["ok"] = true;
            res["data"] = d;

            json ev;
            ev["event"] = "audio.syncState";
            ev["data"] = {
                {"syncBpm", enabled},
                {"projectBpm", projectBpm},
                {"sampleBpm", sampleBpm},
                {"ratio", ratio},
                {"semitones", eng.getPitchSemitones()}
            };
            m_impl->state->pushEvent(ev);
        } else if (cmd == "audio.setOriginalKey" || cmd == "audio.resetPitch") {
            if (m_actions) m_actions->setHostPreviewPitchSemitones(0.0);
            eng.setOriginalKey();
            json d;
            d["pitchSemitones"] = 0.0f;
            res["ok"] = true;
            res["data"] = d;

            json ev;
            ev["event"] = "audio.syncState";
            {
                const std::lock_guard lock(m_impl->syncMutex);
                ev["data"] = {
                    {"syncBpm", m_impl->syncEnabled},
                    {"projectBpm", m_actions ? m_actions->projectTempo() : 0.0},
                    {"sampleBpm", 0.0f},
                    {"ratio", eng.getTimeRatio()},
                    {"semitones", 0.0f}
                };
            }
            m_impl->state->pushEvent(ev);
        } else if (cmd == "audio.detectBpm") {
            const std::string p = narrowPath(args.value("path", ""));
            if (p.empty() && !eng.currentTrack().path.empty()) {
                float bpm = m_impl->detectBpmForPath(eng.currentTrack().path);
                json d; d["bpm"] = bpm; d["path"] = eng.currentTrack().path;
                d["ok"] = bpm > 0.0f;
                res["ok"] = true; res["data"] = d;
            } else {
                float bpm = m_impl->detectBpmForPath(p);
                json d; d["bpm"] = bpm; d["path"] = p;
                d["ok"] = bpm > 0.0f;
                res["ok"] = true; res["data"] = d;
            }
        } else if (cmd == "audio.getSampleMeta") {
            const std::string p = narrowPath(args.value("path", ""));
            const std::string target = !p.empty() ? p : eng.currentTrack().path;
            json d;
            d["path"] = target;
            if (target.empty()) {
                d["ok"] = false;
                d["error"] = "no path";
            } else {
                // Try DB first
                if (auto rec = m_impl->db.getSampleByPath(target); rec.has_value()) {
                    d["bpm"] = rec->bpm;
                    d["key"] = rec->keyRoot + (rec->keyMode == "minor" ? "m" : "");
                    d["camelot"] = rec->camelot;
                    d["genre"] = rec->genre;
                    d["mood"] = rec->mood;
                    d["duration"] = rec->durationSec;
                    d["sampleRate"] = rec->sampleRate;
                    d["channels"] = rec->channels;
                    d["ok"] = true;
                } else {
                    // Fallback to detection
                    float bpm = m_impl->detectBpmForPath(target);
                    std::string key = m_impl->detectKeyForPath(target);
                    auto info = audio::Engine::probeFile(target);
                    d["bpm"] = bpm;
                    d["key"] = key;
                    d["duration"] = info.durationSeconds;
                    d["sampleRate"] = info.sampleRate;
                    d["channels"] = info.channels;
                    d["ok"] = (bpm > 0.0f || !key.empty());
                }
            }
            res["ok"] = true;
            res["data"] = d;
        } else if (cmd == "ai.analyzeFile") {
            const std::string p = narrowPath(args.value("path", ""));
            std::error_code ec;
            if (!fs::exists(platform::u8path(p), ec)) {
                res["ok"] = false;
                res["error"] = "file not found";
            } else {
                const std::string lowerP = platform::toLowerUtf8(p);
                if (lowerP.ends_with(".mid") || lowerP.ends_with(".midi")) {
                    res["ok"] = false;
                    res["error"] = "MIDI files do not support audio AI analysis";
                } else {
                // Decode the REAL audio (mono, capped at 30 s). The previous
                // implementation synthesized a 440 Hz sine here, so every
                // tempo/key/genre/mood/embedding result was fabricated.
                std::vector<float> pcm;
                int sr = 44100;
                {
                    constexpr ma_uint64 kMaxFrames = 44100ull * 30ull;
                    ma_decoder_config decCfg = ma_decoder_config_init(ma_format_f32, 1, 0);
                    ma_decoder dec{};
                    bool decOk = false;
#ifdef _WIN32
                    const std::wstring wpath = platform::u8path(p).wstring();
                    decOk = ma_decoder_init_file_w(wpath.c_str(), &decCfg, &dec) == MA_SUCCESS;
#else
                    decOk = ma_decoder_init_file(p.c_str(), &decCfg, &dec) == MA_SUCCESS;
#endif
                    if (decOk) {
                        sr = dec.outputSampleRate > 0 ? dec.outputSampleRate : 44100;
                        std::vector<float> chunk(4096);
                        ma_uint64 totalRead = 0;
                        while (totalRead < kMaxFrames) {
                            const ma_uint64 framesToRead =
                                std::min<ma_uint64>(chunk.size(), kMaxFrames - totalRead);
                            ma_uint64 framesRead = 0;
                            if (ma_decoder_read_pcm_frames(&dec, chunk.data(), framesToRead, &framesRead) != MA_SUCCESS ||
                                framesRead == 0)
                                break;
                            pcm.insert(pcm.end(), chunk.begin(), chunk.begin() + static_cast<ptrdiff_t>(framesRead));
                            totalRead += framesRead;
                        }
                        ma_decoder_uninit(&dec);
                    }
                }

                if (pcm.size() < 4096) {
                    res["ok"] = false;
                    res["error"] = "cannot decode audio";
                } else {
                auto tempoRes = ai::TempoDetector::detect(pcm.data(), pcm.size(), sr);
                auto keyRes = ai::KeyDetector::detect(pcm.data(), pcm.size(), sr);
                auto genreRes = ai::GenreClassifier::classify(pcm.data(), pcm.size(), sr, 5);
                auto moodRes = ai::MoodClassifier::classify(pcm.data(), pcm.size(), sr, 0.2f);
                auto emb = ai::ClapEmbedder::embedAudio(pcm.data(), pcm.size(), sr);

                json genreArr = json::array();
                for (const auto& g : genreRes) genreArr.push_back({{"tag", g.tag}, {"score", g.score}});
                json moodArr = json::array();
                for (const auto& m : moodRes) moodArr.push_back({{"tag", m.tag}, {"score", m.score}});

                json analysis;
                analysis["tempo"] = {
                    {"bpm", tempoRes.bpm},
                    {"confidence", tempoRes.confidence},
                    {"method", tempoRes.method}
                };
                analysis["key"] = {
                    {"key", keyRes.key},
                    {"mode", keyRes.mode},
                    {"camelot", keyRes.camelot},
                    {"openKey", keyRes.openKey},
                    {"confidence", keyRes.confidence}
                };
                analysis["genres"] = genreArr;
                analysis["moods"] = moodArr;
                analysis["embeddingDim"] = emb.size();

                auto optSample = m_impl->db.getSampleByPath(p);
                if (optSample.has_value()) {
                    auto rec = optSample.value();
                    rec.bpm = tempoRes.bpm;
                    rec.keyRoot = keyRes.key;
                    rec.keyMode = keyRes.mode;
                    rec.camelot = keyRes.camelot;
                    if (!genreRes.empty()) rec.genre = genreRes[0].tag;
                    if (!moodRes.empty()) rec.mood = moodRes[0].tag;
                    rec.aiAnalyzed = true;
                    m_impl->db.upsertSample(rec);

                    db::AnalysisRecord ar;
                    ar.sampleId = rec.id;
                    ar.tempoConfidence = tempoRes.confidence;
                    ar.keyConfidence = keyRes.confidence;
                    for (const auto& g : genreRes) ar.genreTags.push_back(g.tag);
                    for (const auto& m : moodRes) ar.moodTags.push_back(m.tag);
                    ar.embedding = emb;
                    m_impl->db.updateAnalysis(rec.id, ar);
                }

                res["ok"] = true;
                res["data"] = {{"analysis", analysis}};
                }
                }
            }
        } else if (cmd == "ai.searchSemantic") {
            const std::string query = args.value("query", "");
            const int limit = args.value("limit", 20);
            const auto queryVec = ai::ClapEmbedder::embedText(query);
            const auto allEmbs = m_impl->db.getAllEmbeddings();

            std::vector<std::pair<float, int64_t>> scored;
            scored.reserve(allEmbs.size());
            for (const auto& [id, emb] : allEmbs) {
                if (emb.size() == queryVec.size()) {
                    float sim = ai::ClapEmbedder::cosineSimilarity(queryVec, emb);
                    scored.emplace_back(sim, id);
                }
            }
            std::sort(scored.begin(), scored.end(), [](const auto& a, const auto& b) {
                return a.first > b.first;
            });

            json arr = json::array();
            size_t take = std::min<size_t>(scored.size(), limit > 0 ? limit : 20);
            for (size_t i = 0; i < take; ++i) {
                auto optSample = m_impl->db.getSampleById(scored[i].second);
                if (optSample.has_value()) {
                    const auto& s = optSample.value();
                    json item;
                    item["id"] = s.id;
                    item["path"] = s.path;
                    item["filename"] = s.filename;
                    item["name"] = s.filename;
                    item["filesize"] = s.filesize;
                    item["size"] = s.filesize;
                    item["score"] = scored[i].first;
                    item["similarity"] = std::clamp(static_cast<int>(std::round(scored[i].first * 100.0f)), 1, 100);
                    item["bpm"] = s.bpm;
                    item["key"] = s.keyRoot;
                    item["mode"] = s.keyMode;
                    item["camelot"] = s.camelot;
                    item["genre"] = s.genre;
                    item["mood"] = s.mood;
                    item["duration"] = s.durationSec;
                    item["isDir"] = false;
                    item["isAudio"] = true;
                    arr.push_back(item);
                }
            }
            res["ok"] = true;
            res["data"] = {{"results", arr}, {"count", arr.size()}};
        } else if (cmd == "search.findSimilar" || cmd == "ai.findSimilar" || cmd == "browser.findSimilar") {
            const std::string path = narrowPath(args.value("path", ""));
            const int64_t sampleId = args.value("id", static_cast<int64_t>(0));
            const int limit = args.value("limit", 50);

            if (!m_impl->searchEngine) {
                m_impl->searchEngine = std::make_unique<search::SearchEngine>(
                    std::shared_ptr<db::Database>(&m_impl->db, [](db::Database*) {}));
            }

            std::vector<search::SearchResult> results;
            if (sampleId > 0) {
                results = m_impl->searchEngine->searchSimilar(sampleId, limit, 0.30f);
            } else if (!path.empty()) {
                results = m_impl->searchEngine->searchSimilarByPath(path, limit, 0.30f);
            }

            json arr = json::array();
            for (const auto& r : results) {
                json item;
                item["id"] = r.sample.id;
                item["path"] = r.sample.path;
                item["filename"] = r.sample.filename;
                item["name"] = r.sample.filename;
                item["filesize"] = r.sample.filesize;
                item["size"] = r.sample.filesize;
                item["duration"] = r.sample.durationSec;
                item["sampleRate"] = r.sample.sampleRate;
                item["channels"] = r.sample.channels;
                item["bpm"] = r.sample.bpm;
                item["key"] = r.sample.keyRoot;
                item["mode"] = r.sample.keyMode;
                item["camelot"] = r.sample.camelot;
                item["genre"] = r.sample.genre;
                item["mood"] = r.sample.mood;
                item["score"] = r.combinedScore;
                item["similarity"] = std::clamp(static_cast<int>(std::round(r.combinedScore * 100.0f)), 1, 100);
                item["aiAnalyzed"] = r.sample.aiAnalyzed;
                item["isDir"] = false;
                item["isAudio"] = true;
                arr.push_back(item);
            }
            res["ok"] = true;
            res["data"] = {{"results", arr}, {"count", arr.size()}};
        } else if (cmd == "diag.getLogs" || cmd == "system.getLogs") {
            const size_t limit = static_cast<size_t>(args.value("limit", 200));
            const auto logs = util::Log::recentLogs(limit);
            json arr = json::array();
            for (const auto& line : logs) {
                arr.push_back(line);
            }
            res["ok"] = true;
            res["data"] = {{"logs", arr}, {"count", arr.size()}};
        } else if (cmd == "db.search") {
            db::QueryFilter filter;
            filter.text = args.value("query", "");
            filter.genre = args.value("genre", "");
            filter.mood = args.value("mood", "");
            filter.keyRoot = args.value("key", "");
            filter.camelot = args.value("camelot", "");
            filter.minBpm = args.value("minBpm", 0.0);
            filter.maxBpm = args.value("maxBpm", 0.0);
            filter.limit = args.value("limit", 100);
            filter.offset = args.value("offset", 0);

            const auto samples = m_impl->db.querySamples(filter);
            json arr = json::array();
            for (const auto& s : samples) {
                json item;
                item["id"] = s.id;
                item["path"] = s.path;
                item["filename"] = s.filename;
                item["filesize"] = s.filesize;
                item["duration"] = s.durationSec;
                item["sampleRate"] = s.sampleRate;
                item["channels"] = s.channels;
                item["bpm"] = s.bpm;
                item["key"] = s.keyRoot;
                item["mode"] = s.keyMode;
                item["camelot"] = s.camelot;
                item["genre"] = s.genre;
                item["mood"] = s.mood;
                item["aiAnalyzed"] = s.aiAnalyzed;
                arr.push_back(item);
            }
            res["ok"] = true;
            res["data"] = {{"results", arr}, {"count", arr.size()}};
        } else if (cmd == "scanner.start") {
            std::vector<std::string> roots;
            if (args.contains("roots") && args["roots"].is_array()) {
                for (const auto& r : args["roots"]) {
                    if (r.is_string()) {
                        roots.push_back(narrowPath(r.get<std::string>()));
                    }
                }
            } else if (args.contains("path") && args["path"].is_string()) {
                roots.push_back(narrowPath(args["path"].get<std::string>()));
            }
            if (roots.empty()) {
                for (const auto& r : model.roots()) {
                    roots.push_back(r.path);
                }
            }
            if (!m_impl->scanner) {
                m_impl->scanner = std::make_unique<scanner::BackgroundScanner>(m_impl->db);
            }
            scanner::ScanOptions opts;
            opts.forceRescan = args.value("forceRescan", false);
            std::string cpuModeStr = args.value("cpuMode", "normal");
            if (cpuModeStr == "low") {
                opts.cpuMode = scanner::CpuMode::Low;
            } else if (cpuModeStr == "high") {
                opts.cpuMode = scanner::CpuMode::High;
            } else {
                opts.cpuMode = scanner::CpuMode::Normal;
            }
            bool started = m_impl->scanner->startScan(roots, opts);
            res["ok"] = started;
            res["data"] = {
                {"jobId", 1},
                {"isScanning", started},
                {"rootsCount", roots.size()}
            };
        } else if (cmd == "scanner.setCpuMode") {
            const std::string cpuModeStr = args.value("cpuMode", "normal");
            scanner::CpuMode mode = scanner::CpuMode::Normal;
            if (cpuModeStr == "low") mode = scanner::CpuMode::Low;
            else if (cpuModeStr == "high") mode = scanner::CpuMode::High;

            if (m_impl->scanner) {
                m_impl->scanner->setCpuMode(mode);
            }
            cfg.set("scannerCpuMode", cpuModeStr);
            res["ok"] = true;
            res["data"] = {{"cpuMode", cpuModeStr}};
        } else if (cmd == "scanner.cancel" || cmd == "scanner.stop") {
            if (m_impl->scanner) {
                m_impl->scanner->cancel();
            }
            res["ok"] = true;
            res["data"] = {{"cancelled", true}};
        } else if (cmd == "scanner.status") {
            if (!m_impl->scanner) {
                res["ok"] = true;
                res["data"] = {
                    {"isScanning", false},
                    {"total", 0},
                    {"processed", 0},
                    {"added", 0},
                    {"skipped", 0},
                    {"errors", 0},
                    {"currentFile", ""}
                };
            } else {
                const auto prog = m_impl->scanner->getProgress();
                res["ok"] = true;
                res["data"] = {
                    {"isScanning", m_impl->scanner->isScanning()},
                    {"isPaused", prog.isPaused},
                    {"isCancelled", prog.isCancelled},
                    {"isComplete", prog.isComplete},
                    {"total", prog.totalFiles},
                    {"processed", prog.processedFiles},
                    {"added", prog.addedCount},
                    {"skipped", prog.skippedCount},
                    {"errors", prog.errorCount},
                    {"currentFile", prog.currentFile}
                };
            }
        } else if (cmd == "scanner.repair" || cmd == "db.repair") {
            size_t count = scanner::BackgroundScanner::repairDatabaseMetadata(m_impl->db);
            json d;
            d["repaired"] = count;
            d["ok"] = true;
            res["ok"] = true;
            res["data"] = d;
        } else if (cmd == "reaper.insert") {
            const std::string p = narrowPath(args.value("path", ""));
            double playrate = 1.0;
            bool doSync = false;
            // Determine pitch shift
            double pitchShift = 0.0;
            if (args.contains("pitchSemitones")) {
                pitchShift = args.value("pitchSemitones", 0.0);
            } else {
                pitchShift = static_cast<double>(eng.getPitchSemitones());
            }

            // Prefer explicit playrate/ratio from caller
            if (args.contains("playrate")) {
                playrate = args.value("playrate", 1.0);
                doSync = (std::abs(playrate - 1.0) > 0.001);
            } else if (args.contains("ratio")) {
                playrate = args.value("ratio", 1.0);
                doSync = (std::abs(playrate - 1.0) > 0.001);
            } else {
                bool syncOn = args.value("syncBpm", false);
                float syncRatio = 1.0f;
                {
                    const std::lock_guard lock(m_impl->syncMutex);
                    if (m_impl->syncEnabled) syncOn = true;
                    syncRatio = m_impl->syncRatio;
                }
                if (syncOn) {
                    float sampleBpm = args.value("sampleBpm", 0.0f);
                    if (sampleBpm <= 0.0f) {
                        sampleBpm = m_impl->detectBpmForPath(p);
                    }
                    double projectBpm = 0.0;
                    if (m_actions) projectBpm = m_actions->projectTempo();
                    else if (args.contains("bpm")) projectBpm = args.value("bpm", 0.0);

                    if (sampleBpm > 30.0f && projectBpm > 30.0) {
                        playrate = projectBpm / sampleBpm;
                        doSync = true;
                    } else if (std::abs(syncRatio - 1.0f) > 0.001f) {
                        playrate = syncRatio;
                        doSync = true;
                    }
                }
            }
            // Clamp playrate to reasonable range (0.25 .. 4.0 like REAPER)
            playrate = std::clamp(playrate, 0.25, 4.0);
            if (m_actions) {
                if ((doSync && std::abs(playrate - 1.0) > 0.001) || std::abs(pitchShift) > 0.001)
                    m_actions->insertMedia(p, playrate, pitchShift);
                else
                    m_actions->insertMedia(p);
            }
            json d; d["playrate"] = playrate; d["pitch"] = pitchShift; d["synced"] = doSync;
            res["ok"] = true;
            res["data"] = d;
        } else if (cmd == "reaper.insertMany") {
            bool anySync = false;
            if (args.contains("paths") && args["paths"].is_array() && m_actions) {
                double projectBpm = 0.0;
                if (m_actions) projectBpm = m_actions->projectTempo();
                bool syncOn = false;
                float syncRatio = 1.0f;
                {
                    const std::lock_guard lock(m_impl->syncMutex);
                    syncOn = m_impl->syncEnabled;
                    syncRatio = m_impl->syncRatio;
                }
                double pitchShift = args.value("pitchSemitones", static_cast<double>(eng.getPitchSemitones()));
                for (const auto& pp : args["paths"]) {
                    std::string p = narrowPath(pp.get<std::string>());
                    double playrate = 1.0;
                    bool doSync = false;
                    if (syncOn) {
                        float sampleBpm = m_impl->detectBpmForPath(p);
                        if (sampleBpm > 30.0f && projectBpm > 30.0) {
                            playrate = projectBpm / sampleBpm;
                            doSync = true;
                        } else if (std::abs(syncRatio - 1.0f) > 0.001f) {
                            playrate = syncRatio;
                            doSync = true;
                        }
                        playrate = std::clamp(playrate, 0.25, 4.0);
                        if ((doSync && std::abs(playrate - 1.0) > 0.001) || std::abs(pitchShift) > 0.001)
                            m_actions->insertMedia(p, playrate, pitchShift);
                        else
                            m_actions->insertMedia(p);
                        anySync = anySync || doSync;
                    } else {
                        m_actions->insertMedia(p);
                    }
                }
            }
            res["ok"] = true;
            json d; d["synced"] = anySync;
            res["data"] = d;
        } else if (cmd == "lab.analyze" || cmd == "lab.tempo") {
            m_impl->runLabJob(cmd == "lab.tempo" ? "tempo" : "analyze", narrowPath(args.value("path", "")), 0);
            res["ok"] = true;
        } else if (cmd == "lab.keychord" || cmd == "lab.midi") {
            m_impl->runLabJob(cmd == "lab.midi" ? "midi" : "keychord", narrowPath(args.value("path", "")), 0);
            res["ok"] = true;
        } else if (cmd == "lab.stem") {
            m_impl->runLabJob("stem", narrowPath(args.value("path", "")),
                              args.value("mode", 4));
            res["ok"] = true;
        } else if (cmd == "lab.denoise") {
            m_impl->runLabJob("denoise", narrowPath(args.value("path", "")),
                              args.value("strength", 80));
            res["ok"] = true;
        } else if (cmd == "reaper.reveal") {
            if (m_actions)
                m_actions->revealInExplorer(narrowPath(args.value("path", "")));
            res["ok"] = true;
        } else if (cmd == "reaper.lab") {
            if (m_actions)
                m_actions->sendToLab(narrowPath(args.value("path", "")), args.value("job", "analyze").c_str());
            res["ok"] = true;
        } else if (cmd == "reaper.tempo") {
            json d;
            d["bpm"] = m_actions ? m_actions->projectTempo() : 0.0;
            res["ok"] = true;
            res["data"] = d;
        } else if (cmd == "reaper.playToggle") {
            eng.stop();
            if (m_actions) {
                m_actions->stopHostPreview();
                m_actions->togglePlay();
            }
            res["ok"] = true;
        } else if (cmd == "browser.beginDrag") {
            const std::string p = narrowPath(args.value("path", ""));
            if (m_actions) {
                bool syncOn = args.value("syncBpm", false);
                float syncRatio = 1.0f;
                {
                    const std::lock_guard lock(m_impl->syncMutex);
                    if (m_impl->syncEnabled)
                        syncOn = true;
                    syncRatio = m_impl->syncRatio;
                }
                double pitchShift = static_cast<double>(eng.getPitchSemitones());
                if (args.contains("pitchSemitones") && args["pitchSemitones"].is_number()) {
                    pitchShift = args["pitchSemitones"].get<double>();
                }

                // Mechanism A (SPEC.md / PROJECT.md R2.1): drag the ORIGINAL
                // file with zero lag and let REAPER apply the native take
                // stretch (D_PLAYRATE / B_PPITCH / D_PITCH / D_LENGTH) via the
                // queued sync playrate. Do NOT pre-render a temp WAV here —
                // rendering synchronously on the WebView message thread adds
                // drag latency and re-introduces the double-DSP problem the
                // DragExporter safeguard (Mechanism B) exists to clean up.
                // -------------------------------------------------------------------
                // RULE: When Sync BPM is OFF, playrate MUST ALWAYS be 1.000000.
                // Only stretch when Sync BPM is explicitly turned ON by the user.
                // (QUY TẮC: Khi không bật Sync, Playrate khi kéo vào DAW luôn là 1.000000).
                // -------------------------------------------------------------------
                double playrate = 1.0;
                if (syncOn) {
                    float sampleBpm = 0.0f;
                    if (args.contains("sampleBpm") && args["sampleBpm"].is_number()) {
                        sampleBpm = args["sampleBpm"].get<float>();
                    }
                    if (sampleBpm <= 0.0f) {
                        sampleBpm = m_impl->detectBpmForPath(p);
                    }
                    double projectBpm = m_actions->projectTempo();
                    if (sampleBpm > 30.0f && projectBpm > 30.0) {
                        playrate = projectBpm / sampleBpm;
                    } else if (std::abs(syncRatio - 1.0f) > 0.001f) {
                        playrate = static_cast<double>(syncRatio);
                    }
                    playrate = std::clamp(playrate, 0.25, 4.0);
                }

                if (syncOn || std::abs(pitchShift) > 0.001) {
                    m_actions->queueSyncPlayrate(p, playrate, pitchShift);
                }
                m_actions->beginDrag(p);
            }
            res["ok"] = true;
        } else if (cmd == "window.hide" || cmd == "window.close") {
            if (m_actions)
                m_actions->hideWindow();
            res["ok"] = true;
        } else if (cmd == "window.minimize") {
            if (m_actions)
                m_actions->minimizeWindow();
            res["ok"] = true;
        } else if (cmd == "window.toggleMaximize" || cmd == "window.maximize") {
            if (m_actions)
                m_actions->toggleMaximize();
            res["ok"] = true;
        } else if (cmd == "window.startDrag") {
            if (m_actions)
                m_actions->startDragWindow();
            res["ok"] = true;
        } else if (cmd == "window.startResize") {
            if (m_actions)
                m_actions->startResizeWindow(args.value("edge", ""));
            res["ok"] = true;
        } else if (cmd == "window.toggleDock") {
            if (m_actions)
                m_actions->toggleDock();
            res["ok"] = true;
        } else if (cmd == "window.isDocked") {
            res["docked"] = m_actions ? m_actions->isDocked() : false;
            res["ok"] = true;
        } else {
            res["ok"] = false;
            res["error"] = "unknown cmd: " + cmd;
        }
    } catch (const std::exception& e) {
        res["ok"] = false;
        res["error"] = e.what();
        LOG_ERROR(kTag, res["error"].get<std::string>().c_str());
    }
    return res.dump();
}

} // namespace reals::bridge
