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

    // Helper: detect BPM for a file (DB -> filename -> TempoDetector)
    float detectBpmForPath(const std::string& path) {
        if (path.empty()) return 0.0f;
        // 1. DB lookup
        if (auto rec = db.getSampleByPath(path); rec.has_value() && rec->bpm > 30.0 && rec->bpm < 300.0) {
            LOG_INFO("SYNC_DIAG", "detectBpmForPath: found in DB: " + path + " -> " + std::to_string(rec->bpm));
            return static_cast<float>(rec->bpm);
        }
        // 2. Filename regex: e.g. "Loop_128bpm" or "128 BPM"
        try {
            std::regex re(R"((\d{2,3})\s*bpm)", std::regex_constants::icase);
            std::smatch m;
            std::string fname;
            try {
                auto p = platform::u8path(path);
                fname = platform::pathToUtf8(p.filename());
            } catch (...) { fname = path; }
            if (fname.empty()) fname = path;
            if (std::regex_search(fname, m, re) && m.size() > 1) {
                float v = std::stof(m[1].str());
                if (v >= 40.0f && v <= 250.0f) {
                    LOG_INFO("SYNC_DIAG", "detectBpmForPath: found from filename regex: " + fname + " -> " + std::to_string(v));
                    return v;
                }
            }
            // Also try full path
            if (std::regex_search(path, m, re) && m.size() > 1) {
                float v = std::stof(m[1].str());
                if (v >= 40.0f && v <= 250.0f) {
                    LOG_INFO("SYNC_DIAG", "detectBpmForPath: found from full path regex: " + path + " -> " + std::to_string(v));
                    return v;
                }
            }
        } catch (...) {}
        // 3. Local TempoDetector (decode up to 30s mono)
        float bpm = audio::Engine::detectBpm(path);
        if (bpm >= 40.0f && bpm <= 250.0f) {
            // cache into DB for next time
            if (auto rec = db.getSampleByPath(path); rec.has_value()) {
                auto r = rec.value();
                r.bpm = bpm;
                db.upsertSample(r);
            }
            LOG_INFO("SYNC_DIAG", "detectBpmForPath: detected via TempoDetector: " + path + " -> " + std::to_string(bpm));
            return bpm;
        }
        LOG_INFO("SYNC_DIAG", "detectBpmForPath: could not detect BPM for: " + path);
        return 0.0f;
    }

    // Helper: detect musical key for a file (DB -> filename -> KeyDetector)
    std::string detectKeyForPath(const std::string& path) {
        if (path.empty()) return {};
        // 1. DB lookup
        if (auto rec = db.getSampleByPath(path); rec.has_value() && !rec->keyRoot.empty()) {
            std::string k = rec->keyRoot;
            if (!rec->keyMode.empty()) {
                if (rec->keyMode == "minor" || rec->keyMode == "Minor") k += "m";
            }
            return k;
        }
        // 2. Filename regex: _C#m, _F#_ etc.
        try {
            std::regex re(R"(_([A-G][#b]?(?:m|maj|min|minor|major)?)(?:_|\.|$))", std::regex_constants::icase);
            std::smatch m;
            std::string fname;
            try {
                auto p = platform::u8path(path);
                fname = platform::pathToUtf8(p.filename());
            } catch (...) { fname = path; }
            if (fname.empty()) fname = path;
            if (std::regex_search(fname, m, re) && m.size() > 1) {
                std::string k = m[1].str();
                // Normalize to uppercase
                for (auto& c : k) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                return k;
            }
        } catch (...) {}
        // 3. Local KeyDetector
        std::string k = audio::Engine::detectKey(path);
        if (!k.empty()) {
            if (auto rec = db.getSampleByPath(path); rec.has_value()) {
                auto r = rec.value();
                // Parse k into root/mode for DB
                std::string root = k;
                std::string mode = "major";
                if (!k.empty() && k.back() == 'm') {
                    root = k.substr(0, k.size()-1);
                    mode = "minor";
                }
                r.keyRoot = root;
                r.keyMode = mode;
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

            // 2. Directory crawler fallback for unindexed files in base folder
            if (arr.size() < maxResults && !base.empty()) {
                const auto results = model.search(base, query, audioOnly, maxResults - arr.size(), cancel.get());
                if (cancel->load() || gen != searchGen.load())
                    return;
                for (const auto& f : results) {
                    if (seenPaths.find(f.path) != seenPaths.end())
                        continue;
                    seenPaths.insert(f.path);
                    arr.push_back(entryToJson(f));
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

Bridge::Bridge(IHostActions* actions) : m_actions(actions), m_impl(std::make_unique<Impl>()) {
    m_impl->actions = actions;
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

std::string Bridge::audioStateJson() const {
    auto& eng = reals::audio::Engine::instance();
    const reals::audio::LevelState lvl = eng.level();

    nlohmann::json j;
    j["event"] = "audio.state";
    nlohmann::json d;
    d["playing"] = eng.isPlaying();
    d["position"] = eng.positionFraction();
    d["peak"] = lvl.peak;
    d["rms"] = lvl.rms;
    d["aboveThreshold"] = (lvl.rms > 0.01f);
    d["duration"] = eng.currentTrack().durationSeconds;
    d["pitchSemitones"] = eng.getPitchSemitones();
    d["timeRatio"] = eng.getTimeRatio();
    d["syncBpm"] = (eng.getTimeRatio() != 1.0f);
    j["data"] = d;
    return j.dump();
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
            const auto files = model.listDir(narrowPath(args.value("path", "")));
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
            LOG_INFO("bridge", "handling audio.play for path: " + p);

            bool syncOn = args.value("syncBpm", false);
            {
                const std::lock_guard lock(m_impl->syncMutex);
                if (m_impl->syncEnabled)
                    syncOn = true;
            }

            const float pitchShift = static_cast<float>(args.value("pitchSemitones", static_cast<double>(eng.getPitchSemitones())));
            eng.setPitchSemitones(pitchShift);
            float sampleBpm = args.value("sampleBpm", 0.0f);
            if (syncOn) {
                if (sampleBpm <= 0.0f) {
                    sampleBpm = m_impl->detectBpmForPath(p);
                }
                double projectBpm = 0.0;
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
                    const float ratio = std::clamp(static_cast<float>(projectBpm / sampleBpm), 0.25f, 4.0f);
                    eng.setTimeRatio(ratio);
                    {
                        const std::lock_guard lock(m_impl->syncMutex);
                        m_impl->syncRatio = ratio;
                        m_impl->syncSampleBpm = sampleBpm;
                    }
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
                    const int effectiveSr = (eng.targetSampleRate() > 0) ? eng.targetSampleRate() : info.sampleRate;
                    nominalLoopFrames = static_cast<uint64_t>(nominalLoopSec * effectiveSr);
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

            // Play directly via high-performance core::Engine with sample-accurate phaseAnchor
            const bool ok = eng.playFile(p, args.value("loop", false), startFraction, phaseAnchor, nominalLoopFrames);
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
            eng.setLoop(args.value("value", false));
            res["ok"] = true;
        } else if (cmd == "audio.setVolume") {
            eng.setVolume(args.value("value", 0.9f));
            res["ok"] = true;
        } else if (cmd == "audio.probe") {
            const std::string p = narrowPath(args.value("path", ""));
            audio::TrackInfo info;
            bool found = false;
            {
                const std::lock_guard lock(m_impl->cacheMutex);
                auto it = m_impl->probeCache.find(p);
                if (it != m_impl->probeCache.end()) {
                    info = it->second;
                    found = true;
                }
            }
            if (!found) {
                info = audio::Engine::probeFile(p);
                if (info.sampleRate > 0) {
                    const std::lock_guard lock(m_impl->cacheMutex);
                    m_impl->probeCache[p] = info;
                }
            }
            json d;
            d["duration"] = info.durationSeconds;
            d["sampleRate"] = info.sampleRate;
            d["channels"] = info.channels;
            d["ok"] = info.sampleRate > 0;
            res["ok"] = true;
            res["data"] = d;
        } else if (cmd == "audio.seek") {
            eng.seekFraction(args.value("fraction", 0.0));
            res["ok"] = true;
        } else if (cmd == "audio.setPitchShift") {
            const float semitones = args.value("semitones", 0.0f);
            eng.setPitchSemitones(semitones);
            json d;
            d["pitchSemitones"] = eng.getPitchSemitones();
            res["ok"] = true;
            res["data"] = d;

            json ev;
            ev["event"] = "audio.syncState";
            ev["data"] = {
                {"syncBpm", eng.getTimeRatio() != 1.0f},
                {"projectBpm", m_actions ? m_actions->projectTempo() : 0.0},
                {"sampleBpm", 0.0f},
                {"ratio", eng.getTimeRatio()},
                {"semitones", eng.getPitchSemitones()}
            };
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
                } else if (args.contains("ratio")) {
                    ratio = args.value("ratio", 1.0f);
                } else {
                    // If we still have no sampleBpm, fallback to 120
                    if (projectBpm > 0.0f) ratio = projectBpm / 120.0f;
                }
            }
            eng.setTimeRatio(ratio);
            {
                const std::lock_guard lock(m_impl->syncMutex);
                m_impl->syncEnabled = enabled;
                m_impl->syncRatio = ratio;
                m_impl->syncPath = syncPath;
                m_impl->syncSampleBpm = sampleBpm;
            }
            // Re-align phase if DAW is actively playing and engine is loaded
            if (enabled && eng.isPlaying() && m_actions) {
                const auto transport = m_actions->hostTransport();
                if (transport.isPlaying() && transport.bpm > 30.0 && sampleBpm > 30.0) {
                    const auto& trk = eng.currentTrack();
                    if (trk.durationSeconds >= 1.0) {
                        const double rawBeats = (trk.durationSeconds * sampleBpm) / 60.0;
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
                        const double nominalLoopSec = (resolvedBeats * 60.0) / sampleBpm;
                        const int effectiveSr = (eng.targetSampleRate() > 0) ? eng.targetSampleRate() : trk.sampleRate;
                        if (effectiveSr > 0) {
                            eng.setLoopBoundaryFrames(static_cast<uint64_t>(nominalLoopSec * effectiveSr));
                        }
                        eng.seekFraction(syncFrac);
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
            eng.setOriginalKey();
            json d;
            d["pitchSemitones"] = 0.0f;
            res["ok"] = true;
            res["data"] = d;

            json ev;
            ev["event"] = "audio.syncState";
            ev["data"] = {
                {"syncBpm", eng.getTimeRatio() != 1.0f},
                {"projectBpm", m_actions ? m_actions->projectTempo() : 0.0},
                {"sampleBpm", 0.0f},
                {"ratio", eng.getTimeRatio()},
                {"semitones", 0.0f}
            };
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
                    item["score"] = scored[i].first;
                    item["bpm"] = s.bpm;
                    item["key"] = s.keyRoot;
                    item["mode"] = s.keyMode;
                    item["genre"] = s.genre;
                    item["mood"] = s.mood;
                    item["duration"] = s.durationSec;
                    arr.push_back(item);
                }
            }
            res["ok"] = true;
            res["data"] = {{"results", arr}, {"count", arr.size()}};
        } else if (cmd == "search.findSimilar" || cmd == "ai.findSimilar") {
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
                item["filesize"] = r.sample.filesize;
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
            if (m_actions)
                m_actions->togglePlay();
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
                double pitchShift = args.value("pitchSemitones", static_cast<double>(eng.getPitchSemitones()));

                // Mechanism A (SPEC.md / PROJECT.md R2.1): drag the ORIGINAL
                // file with zero lag and let REAPER apply the native take
                // stretch (D_PLAYRATE / B_PPITCH / D_PITCH / D_LENGTH) via the
                // queued sync playrate. Do NOT pre-render a temp WAV here —
                // rendering synchronously on the WebView message thread adds
                // drag latency and re-introduces the double-DSP problem the
                // DragExporter safeguard (Mechanism B) exists to clean up.
                if (syncOn || std::abs(pitchShift) > 0.001) {
                    float sampleBpm = args.value("sampleBpm", 0.0f);
                    if (sampleBpm <= 0.0f) {
                        sampleBpm = m_impl->detectBpmForPath(p);
                    }
                    double projectBpm = m_actions->projectTempo();
                    double playrate = 1.0;
                    if (sampleBpm > 30.0f && projectBpm > 30.0) {
                        playrate = projectBpm / sampleBpm;
                    } else if (std::abs(syncRatio - 1.0f) > 0.001f) {
                        playrate = static_cast<double>(syncRatio);
                    }
                    playrate = std::clamp(playrate, 0.25, 4.0);

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
