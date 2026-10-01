#include "AgentBridge.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <sstream>
#include <utility>

#include "reals/config/Config.h"
#include "reals/net/HttpClient.h"
#include "reals/util/Log.h"

namespace reals::bridge {

using json = nlohmann::json;

namespace {
constexpr const char* kTag = "agent";
constexpr const char* kDefaultModel = "claude-sonnet-4-6";
constexpr auto kToolTimeout = std::chrono::seconds(60);

std::string envVar(const char* name) {
#if defined(_MSC_VER)
    char* buf = nullptr;
    size_t len = 0;
    std::string out;
    if (_dupenv_s(&buf, &len, name) == 0 && buf) {
        out = buf;
        free(buf);
    }
    return out;
#else
    const char* v = std::getenv(name);
    return v ? std::string(v) : std::string();
#endif
}

std::vector<std::string> splitCsv(const std::string& s) {
    std::vector<std::string> out;
    std::stringstream ss(s);
    std::string item;
    while (std::getline(ss, item, ',')) {
        const auto b = item.find_first_not_of(" \t");
        const auto e = item.find_last_not_of(" \t");
        if (b != std::string::npos)
            out.push_back(item.substr(b, e - b + 1));
    }
    return out;
}

std::string maskKey(const std::string& k) {
    if (k.size() <= 8)
        return k.empty() ? "" : "****";
    return k.substr(0, 5) + "…" + k.substr(k.size() - 4);
}
} // namespace

AgentBridge::AgentBridge(IHostActions* host, Emit emit, LabJob labJob, std::string historyPath)
    : m_host(host), m_emit(std::move(emit)), m_labJob(std::move(labJob)) {
    m_session = std::make_unique<agent::AgentSession>(
        [this](const json& ev) {
            if (!m_shutdown.load())
                m_emit(ev);
        },
        [this](const agent::ToolCall& call) { return executeOnMainThread(call); });
    m_session->setContextProvider([this] { return labContext(); });
    auto& cfg = config::Config::instance();
    const int mode = cfg.getInt("agentMode", 1);
    m_session->setMode(static_cast<agent::PermissionMode>(mode < 0 || mode > 2 ? 1 : mode));
    m_session->registry().setAllowedTools(splitCsv(cfg.getString("agentAllowedTools", "")));
    if (!historyPath.empty())
        m_session->setHistoryPath(std::move(historyPath));
}

AgentBridge::~AgentBridge() {
    m_shutdown.store(true);
    if (m_session)
        m_session->cancel();
    {
        const std::lock_guard lock(m_taskMutex);
        for (auto& t : m_tasks)
            t->promise.set_value({false, {{"error", "shutting down"}}});
        m_tasks.clear();
    }
    const std::lock_guard lock(m_threadMutex);
    if (m_turnThread.joinable())
        m_turnThread.join();
    if (m_modelsThread.joinable())
        m_modelsThread.join();
}

agent::LlmConfig AgentBridge::resolveConfig(std::string* keySource) {
    auto& cfg = config::Config::instance();
    agent::LlmConfig c;
    c.provider = cfg.getString("agentProvider", "anthropic");
    if (c.provider != "openai")
        c.provider = "anthropic";
    c.baseUrl = cfg.getString("agentBaseUrl", "");
    if (c.baseUrl.empty())
        c.baseUrl = envVar(c.provider == "openai" ? "OPENAI_BASE_URL" : "ANTHROPIC_BASE_URL");
    if (c.baseUrl.empty())
        c.baseUrl = c.provider == "openai" ? "https://api.openai.com" : "https://api.anthropic.com";
    c.apiKey = cfg.getString("agentApiKey", "");
    std::string src = c.apiKey.empty() ? "" : "config";
    if (c.apiKey.empty()) {
        c.apiKey = envVar(c.provider == "openai" ? "OPENAI_API_KEY" : "ANTHROPIC_API_KEY");
        if (!c.apiKey.empty())
            src = "env";
    }
    c.model = cfg.getString("agentModel", "");
    if (c.model.empty())
        c.model = envVar("ANTHROPIC_MODEL");
    if (c.model.empty())
        c.model = kDefaultModel;
    c.maxTokens = cfg.getInt("agentMaxTokens", 4096);
    c.stream = cfg.getBool("agentStream", true);
    c.thinking = cfg.getBool("agentThinking", true);
    c.thinkingBudget = std::max(1024, cfg.getInt("agentThinkingBudget", 4000));
    c.toolProtocol = cfg.getString("agentToolProtocol", "auto");
    if (c.toolProtocol != "native" && c.toolProtocol != "text")
        c.toolProtocol = "auto";
    if (keySource)
        *keySource = src;
    return c;
}

json AgentBridge::uiHistory(const json& history) {
    json out = json::array();
    if (!history.is_array())
        return out;
    for (const auto& m : history) {
        const std::string role = m.value("role", "");
        if (!m.contains("content") || !m["content"].is_array())
            continue;
        for (const auto& b : m["content"]) {
            const std::string t = b.value("type", "");
            if (t == "text" && !b.value("text", "").empty())
                out.push_back({{"role", role}, {"text", b.value("text", "")}});
            else if (t == "tool_use")
                out.push_back({{"role", "tool"}, {"tool", b.value("name", "")},
                               {"args", b.value("input", json::object())}});
        }
    }
    return out;
}

void AgentBridge::joinFinished() {
    const std::lock_guard lock(m_threadMutex);
    if (m_turnThread.joinable() && !m_session->isBusy())
        m_turnThread.join();
    if (m_modelsThread.joinable() && !m_modelsBusy.load())
        m_modelsThread.join();
}

void AgentBridge::startTurn(const std::string& text) {
    joinFinished();
    std::string keySource;
    const agent::LlmConfig cfg = resolveConfig(&keySource);
    m_session->setClient(cfg.apiKey.empty() ? nullptr : std::make_shared<agent::LlmClient>(cfg));
    m_session->registry().setAllowedTools(
        splitCsv(config::Config::instance().getString("agentAllowedTools", "")));
    const std::lock_guard lock(m_threadMutex);
    if (m_turnThread.joinable())
        m_turnThread.join(); // previous turn finished (busy flag checked by caller)
    m_turnThread = std::thread([this, text]() {
        try {
            m_session->runTurn(text);
        } catch (const std::exception& e) {
            LOG_ERROR(kTag, std::string("turn exception: ") + e.what());
            m_emit({{"event", "agent.error"}, {"data", {{"message", e.what()}}}});
            m_emit({{"event", "agent.state"}, {"data", {{"busy", false}}}});
        }
    });
}

void AgentBridge::fetchModels() {
    if (m_modelsBusy.exchange(true))
        return;
    joinFinished();
    const agent::LlmConfig cfg = resolveConfig();
    const std::lock_guard lock(m_threadMutex);
    if (m_modelsThread.joinable())
        m_modelsThread.join();
    m_modelsThread = std::thread([this, cfg]() {
        json ev = {{"event", "agent.models"}};
        json data = {{"models", json::array()}};
#ifdef _WIN32
        net::Request req;
        req.method = "GET";
        std::string base = cfg.baseUrl;
        while (!base.empty() && base.back() == '/')
            base.pop_back();
        if (base.size() < 3 || base.compare(base.size() - 3, 3, "/v1") != 0)
            base += "/v1";
        req.url = base + "/models";
        req.headers["Authorization"] = "Bearer " + cfg.apiKey;
        req.headers["x-api-key"] = cfg.apiKey;
        req.headers["anthropic-version"] = "2023-06-01";
        const net::Response r = net::HttpClient::instance().send(req);
        const json body = json::parse(r.body, nullptr, false);
        if (r.error.empty() && r.statusCode == 200 && body.is_object() && body.contains("data") &&
            body["data"].is_array()) {
            for (const auto& m : body["data"])
                data["models"].push_back({{"id", m.value("id", "")},
                                          {"name", m.value("display_name", m.value("id", ""))},
                                          {"description", m.value("description", "")}});
        } else {
            data["error"] = r.error.empty() ? "HTTP " + std::to_string(r.statusCode) : r.error;
        }
#else
        data["error"] = "unsupported platform";
#endif
        ev["data"] = data;
        if (!m_shutdown.load())
            m_emit(ev);
        m_modelsBusy.store(false);
    });
}

agent::ToolOutcome AgentBridge::executeOnMainThread(const agent::ToolCall& call) {
    // Audio Lab tools only talk to the lab worker / cached results, so they
    // run right here on the turn thread and may block while a job runs.
    if (call.name == "audio_lab_job")
        return runLabJobTool(call);
    if (call.name == "get_lab_result")
        return labResultTool(call);
    auto task = std::make_shared<Task>();
    task->call = call;
    auto fut = task->promise.get_future();
    {
        const std::lock_guard lock(m_taskMutex);
        if (m_shutdown.load())
            return {false, {{"error", "shutting down"}}};
        m_tasks.push_back(task);
    }
    const auto deadline = std::chrono::steady_clock::now() + kToolTimeout;
    while (fut.wait_for(std::chrono::milliseconds(50)) != std::future_status::ready) {
        if (m_shutdown.load() || std::chrono::steady_clock::now() > deadline) {
            const std::lock_guard lock(m_taskMutex);
            for (auto it = m_tasks.begin(); it != m_tasks.end(); ++it) {
                if (*it == task) {
                    m_tasks.erase(it);
                    return {false, {{"error", m_shutdown.load() ? "shutting down" : "host did not respond (timeout)"}}};
                }
            }
            // Task already taken by the main thread: wait for it to finish.
            fut.wait();
            break;
        }
    }
    return fut.get();
}

void AgentBridge::pumpMainThread() {
    for (int i = 0; i < 16; ++i) { // bound work per timer tick
        std::shared_ptr<Task> task;
        {
            const std::lock_guard lock(m_taskMutex);
            if (m_tasks.empty())
                return;
            task = m_tasks.front();
            m_tasks.pop_front();
        }
        agent::ToolOutcome out;
        try {
            out = executeNow(task->call);
        } catch (const std::exception& e) {
            out = {false, {{"error", std::string("exception: ") + e.what()}}};
        } catch (...) {
            out = {false, {{"error", "unknown exception"}}};
        }
        task->promise.set_value(std::move(out));
    }
}

agent::ToolOutcome AgentBridge::executeNow(const agent::ToolCall& call) {
    if (call.name == "chord_track")
        return chordTrackTool(call);
    if (!m_host)
        return {false, {{"error", "no host (REAPER) available"}}};
    const std::string raw = m_host->executeAgentTool(call.name, call.input.dump());
    const json r = json::parse(raw, nullptr, false);
    if (r.is_discarded() || !r.is_object())
        return {false, {{"error", "host returned invalid JSON"}}};
    const bool ok = r.value("ok", false);
    if (!ok)
        return {false, {{"error", r.value("error", std::string("failed"))}}};
    return {true, r.contains("data") ? r["data"] : json::object()};
}

// ---- Audio Lab integration -----------------------------------------------------
namespace {

std::string normalizeLabJob(const std::string& job) {
    if (job == "tempo")
        return "analyze";
    if (job == "midi")
        return "keychord";
    return job;
}

double round2(double v) { return std::round(v * 100.0) / 100.0; }

double numberOr(const json& j, const char* key, double fallback) {
    if (j.is_object() && j.contains(key) && j[key].is_number())
        return j[key].get<double>();
    return fallback;
}

std::string stringOr(const json& j, const char* key, const std::string& fallback = "") {
    if (j.is_object() && j.contains(key) && j[key].is_string())
        return j[key].get<std::string>();
    return fallback;
}

// Chords may sit at payload.chords, payload.result.chords or payload.data.chords.
const json* findChords(const json& p) {
    for (const json* cand : {&p, p.is_object() && p.contains("result") ? &p["result"] : nullptr,
                             p.is_object() && p.contains("data") ? &p["data"] : nullptr}) {
        if (cand && cand->is_object() && cand->contains("chords") && (*cand)["chords"].is_array())
            return &(*cand)["chords"];
    }
    return nullptr;
}

const json& findTelemetry(const json& p) {
    static const json kEmpty = json::object();
    if (p.is_object() && p.contains("telemetry") && p["telemetry"].is_object())
        return p["telemetry"];
    return p.is_object() ? p : kEmpty;
}

std::string truncatedDump(const json& j, size_t maxLen = 1500) {
    std::string s = j.dump();
    if (s.size() > maxLen)
        s = s.substr(0, maxLen) + "...";
    return s;
}

} // namespace

json AgentBridge::summarizeLab(const std::string& rawJob, const json& data) {
    const std::string job = normalizeLabJob(rawJob);
    json out = {{"job", job}};
    if (job == "keychord" || job == "analyze") {
        const json p = data.value("payload", json::object());
        const json& tel = findTelemetry(p);
        const double bpm = numberOr(tel, "bpm", numberOr(p, "bpm", 0.0));
        const std::string key = stringOr(tel, "master_key", stringOr(p, "master_key", stringOr(p, "key")));
        const std::string scale = stringOr(tel, "scale_mode", stringOr(p, "scale_mode", stringOr(p, "scale")));
        if (bpm > 0)
            out["bpm"] = round2(bpm);
        if (!key.empty())
            out["key"] = key;
        if (!scale.empty())
            out["scale"] = scale;
        const double dur = numberOr(tel, "duration", numberOr(p, "duration", 0.0));
        if (dur > 0)
            out["duration_sec"] = round2(dur);
        if (job == "keychord") {
            json list = json::array();
            std::string progression;
            std::string prev;
            int distinct = 0;
            if (const json* chords = findChords(p)) {
                for (const auto& c : *chords) {
                    const std::string name = stringOr(c, "chord", stringOr(c, "name"));
                    if (name.empty())
                        continue;
                    if (list.size() < 96) {
                        json e = {{"chord", name}, {"time", round2(numberOr(c, "time", numberOr(c, "start", 0.0)))}};
                        if (c.contains("duration") && c["duration"].is_number())
                            e["duration"] = round2(c["duration"].get<double>());
                        if (c.contains("roman") && c["roman"].is_string())
                            e["roman"] = c["roman"];
                        list.push_back(std::move(e));
                    }
                    if (name != prev && distinct < 24) {
                        progression += (progression.empty() ? "" : " - ") + name;
                        ++distinct;
                    }
                    prev = name;
                }
                out["chord_count"] = chords->size();
            }
            out["progression"] = progression;
            out["chords"] = list;
            out["auto"] = "Reals Lab automatically opened its Chord Track dock (top of REAPER) with this progression "
                          "aligned to the analyzed item, and listed it in the Audio Lab tab. Use "
                          "chord_track(action=insert_midi) to also create the CHORD TRACK MIDI track.";
            if (list.empty())
                out["raw"] = truncatedDump(p);
        } else if (bpm <= 0 && key.empty()) {
            out["raw"] = truncatedDump(p);
        }
        return out;
    }
    json files = json::array();
    if (data.contains("files") && data["files"].is_array())
        for (const auto& f : data["files"])
            files.push_back({{"name", f.value("name", "")}, {"path", f.value("path", "")}});
    out["files"] = files;
    if (data.contains("zipPath"))
        out["zip_path"] = data["zipPath"];
    if (job == "stem")
        out["auto"] = "Reals Lab automatically inserted these stems into a new folder track below the original item "
                      "and muted the original item.";
    else if (job == "denoise")
        out["auto"] = "Not inserted automatically. Use insert_media with the path to add the clean audio.";
    return out;
}

void AgentBridge::onLabEvent(const json& ev) {
    const std::string name = ev.value("event", "");
    if (name != "lab.result" && name != "lab.error")
        return;
    const json data = ev.value("data", json::object());
    const std::string job = normalizeLabJob(data.value("job", ""));
    if (job.empty())
        return;
    {
        const std::lock_guard lock(m_labMutex);
        json rec = {{"seq", ++m_labSeq}, {"ok", name == "lab.result"}};
        if (name == "lab.result")
            rec["data"] = data;
        else
            rec["error"] = data.value("error", std::string("failed"));
        if (m_labPending.contains(job)) {
            const json& pend = m_labPending[job];
            rec["path"] = pend.value("path", "");
            rec["position"] = pend.value("position", 0.0);
            rec["length"] = pend.value("length", 0.0);
            m_labPending.erase(job);
        }
        m_labLast[job] = std::move(rec);
    }
    m_labCv.notify_all();
}

agent::ToolOutcome AgentBridge::runLabJobTool(const agent::ToolCall& call) {
    const json& in = call.input;
    const std::string job = in.value("job", "");
    const std::string path = in.value("path", "");
    if (path.empty())
        return {false, {{"error", "path is required"}}};
    if (job != "denoise" && job != "stem" && job != "analyze" && job != "keychord")
        return {false, {{"error", "unknown job: " + job}}};
    if (!m_labJob)
        return {false, {{"error", "Audio Lab not available"}}};
    const double position = numberOr(in, "position", 0.0);
    const double length = numberOr(in, "length", 0.0);
    const int param = job == "denoise" ? in.value("strength", 80) : job == "stem" ? in.value("stems", 4) : 0;
    const bool wait = in.value("wait", true);

    uint64_t startSeq = 0;
    {
        const std::lock_guard lock(m_labMutex);
        startSeq = m_labSeq;
        m_labPending[job] = {{"path", path}, {"position", position}, {"length", length}};
    }
    // Let the Audio Lab tab adopt the job (file, item position for chord/stem
    // placement, progress UI) exactly as if the user had started it.
    if (!m_shutdown.load())
        m_emit({{"event", "lab.agentJob"},
                {"data", {{"job", job}, {"path", path}, {"itemPosition", position}, {"itemLength", length}}}});
    m_labJob(job, path, param);
    if (!wait)
        return {true, {{"started", true}, {"job", job}, {"path", path},
                       {"note", "Running in background; call get_lab_result later."}}};

    const auto timeout = std::chrono::minutes(job == "stem" || job == "denoise" ? 11 : 5);
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    std::unique_lock lock(m_labMutex);
    for (;;) {
        const auto it = m_labLast.find(job);
        if (it != m_labLast.end() && it->value("seq", uint64_t{0}) > startSeq) {
            const json rec = *it;
            lock.unlock();
            if (!rec.value("ok", false))
                return {false, {{"error", "Audio Lab " + job + " failed: " + rec.value("error", std::string("unknown"))}}};
            json sum = summarizeLab(job, rec.value("data", json::object()));
            sum["path"] = path;
            if (position > 0)
                sum["item_position"] = position;
            return {true, sum};
        }
        if (m_shutdown.load())
            return {false, {{"error", "shutting down"}}};
        if (m_session && m_session->cancelRequested())
            return {false, {{"error", "cancelled by user (the lab job keeps running in background)"}}};
        if (std::chrono::steady_clock::now() > deadline) {
            m_labPending.erase(job);
            return {true, {{"started", true}, {"job", job}, {"still_running", true},
                           {"note", "No result yet; check later with get_lab_result."}}};
        }
        m_labCv.wait_for(lock, std::chrono::milliseconds(200));
    }
}

agent::ToolOutcome AgentBridge::labResultTool(const agent::ToolCall& call) const {
    const std::string only = call.input.value("job", "");
    const std::lock_guard lock(m_labMutex);
    json results = json::array();
    for (auto it = m_labLast.begin(); it != m_labLast.end(); ++it) {
        if (!only.empty() && it.key() != only)
            continue;
        const json& rec = it.value();
        json e;
        if (rec.value("ok", false))
            e = summarizeLab(it.key(), rec.value("data", json::object()));
        else
            e = {{"job", it.key()}, {"error", rec.value("error", std::string("failed"))}};
        if (rec.contains("path"))
            e["path"] = rec["path"];
        if (rec.value("position", 0.0) > 0)
            e["item_position"] = rec["position"];
        results.push_back(std::move(e));
    }
    json running = json::array();
    for (auto it = m_labPending.begin(); it != m_labPending.end(); ++it)
        if (only.empty() || it.key() == only)
            running.push_back({{"job", it.key()}, {"path", it.value().value("path", "")}});
    return {true, {{"results", results}, {"running", running},
                   {"note", results.empty() ? "No Audio Lab result in this session yet." : ""}}};
}

agent::ToolOutcome AgentBridge::chordTrackTool(const agent::ToolCall& call) {
    if (!m_host)
        return {false, {{"error", "no host (REAPER) available"}}};
    const json& in = call.input;
    const std::string action = in.value("action", "");
    if (action == "hide") {
        m_host->showChordDocker(false, json{{"show", false}}.dump());
        return {true, {{"dock", "hidden"}}};
    }
    if (action != "show" && action != "insert_midi")
        return {false, {{"error", "unknown action: " + action}}};

    json chords = json::array();
    double bpm = numberOr(in, "bpm", 0.0);
    std::string key = stringOr(in, "key");
    std::string scale = stringOr(in, "scale");
    double position = numberOr(in, "position", -1.0);
    if (in.contains("chords") && in["chords"].is_array() && !in["chords"].empty()) {
        for (const auto& c : in["chords"]) {
            const std::string name = stringOr(c, "chord", stringOr(c, "name"));
            if (name.empty())
                continue;
            json e = {{"chord", name}, {"name", name}, {"time", numberOr(c, "time", 0.0)}};
            if (c.contains("duration") && c["duration"].is_number())
                e["duration"] = c["duration"];
            chords.push_back(std::move(e));
        }
    } else {
        const std::lock_guard lock(m_labMutex);
        const auto it = m_labLast.find("keychord");
        if (it == m_labLast.end() || !it->value("ok", false))
            return {false, {{"error", "no chord progression yet: run audio_lab_job(job=keychord) first or pass chords"}}};
        const json p = it->value("data", json::object()).value("payload", json::object());
        if (const json* found = findChords(p))
            chords = *found;
        const json& tel = findTelemetry(p);
        if (bpm <= 0)
            bpm = numberOr(tel, "bpm", 0.0);
        if (key.empty())
            key = stringOr(tel, "master_key");
        if (scale.empty())
            scale = stringOr(tel, "scale_mode");
        if (position < 0)
            position = it->value("position", 0.0);
    }
    if (chords.empty())
        return {false, {{"error", "progression has no chords"}}};
    if (bpm <= 0)
        bpm = m_host->projectTempo() > 0 ? m_host->projectTempo() : 120.0;
    if (position < 0)
        position = 0.0;
    if (key.empty())
        key = "C";
    if (scale.empty())
        scale = "Major";

    if (action == "show") {
        const json payload = {{"show", true}, {"bpm", bpm}, {"masterKey", key}, {"scaleMode", scale},
                              {"itemPosition", position}, {"chords", chords}};
        m_host->showChordDocker(true, payload.dump());
        return {true, {{"dock", "shown"}, {"chords", chords.size()}, {"position", position}}};
    }
    const json payload = {{"chords", chords}, {"bpm", bpm}, {"itemPosition", position},
                          {"masterKey", key}, {"scaleMode", scale}};
    if (!m_host->insertChordTrack(payload.dump()))
        return {false, {{"error", "REAPER could not create the chord track"}}};
    return {true, {{"track", "CHORD TRACK"}, {"chords", chords.size()}, {"position", position}}};
}

std::string AgentBridge::labContext() const {
    const std::lock_guard lock(m_labMutex);
    std::string out;
    for (auto it = m_labLast.begin(); it != m_labLast.end(); ++it) {
        const json& rec = it.value();
        std::string line = "- last " + it.key();
        if (rec.contains("path"))
            line += " of \"" + rec.value("path", "") + "\"";
        if (!rec.value("ok", false)) {
            line += ": failed (" + rec.value("error", std::string()) + ")";
        } else {
            const json sum = summarizeLab(it.key(), rec.value("data", json::object()));
            if (sum.contains("key"))
                line += ": key " + sum.value("key", "") + " " + sum.value("scale", "");
            if (sum.contains("bpm"))
                line += ", " + std::to_string(static_cast<int>(std::lround(sum.value("bpm", 0.0)))) + " BPM";
            if (!sum.value("progression", "").empty())
                line += ", chords " + sum.value("progression", "") + " (shown in the Chord Track dock)";
            if (sum.contains("files") && !sum["files"].empty())
                line += ", " + std::to_string(sum["files"].size()) + " file(s)";
        }
        out += line + "\n";
    }
    for (auto it = m_labPending.begin(); it != m_labPending.end(); ++it)
        out += "- " + it.key() + " job running on \"" + it.value().value("path", "") + "\"\n";
    return out;
}

void AgentBridge::handle(const std::string& cmd, const json& args, json& res) {
    auto& cfg = config::Config::instance();
    if (cmd == "agent.send") {
        const std::string text = args.value("text", "");
        if (text.empty()) {
            res["ok"] = false;
            res["error"] = "empty message";
        } else if (m_session->isBusy()) {
            res["ok"] = false;
            res["error"] = "busy";
        } else {
            startTurn(text);
            res["ok"] = true;
        }
    } else if (cmd == "agent.cancel") {
        m_session->cancel();
        res["ok"] = true;
    } else if (cmd == "agent.confirm") {
        m_session->confirm(args.value("id", ""), args.value("approved", false));
        res["ok"] = true;
    } else if (cmd == "agent.setMode") {
        int mode = args.value("mode", 1);
        if (mode < 0 || mode > 2)
            mode = 1;
        m_session->setMode(static_cast<agent::PermissionMode>(mode));
        cfg.set("agentMode", mode);
        res["ok"] = true;
    } else if (cmd == "agent.config") {
        std::string keySource;
        const agent::LlmConfig c = resolveConfig(&keySource);
        res["ok"] = true;
        res["data"] = {{"provider", c.provider},
                       {"baseUrl", c.baseUrl},
                       {"model", c.model},
                       {"hasKey", !c.apiKey.empty()},
                       {"keySource", keySource},
                       {"keyMasked", maskKey(c.apiKey)},
                       {"stream", c.stream},
                       {"thinking", c.thinking},
                       {"toolProtocol", c.toolProtocol},
                       {"mode", static_cast<int>(m_session->mode())},
                       {"busy", m_session->isBusy()},
                       {"hostAvailable", m_host != nullptr}};
    } else if (cmd == "agent.setConfig") {
        if (args.contains("provider"))
            cfg.set("agentProvider", args.value("provider", "anthropic"));
        if (args.contains("baseUrl"))
            cfg.set("agentBaseUrl", args.value("baseUrl", ""));
        if (args.contains("model"))
            cfg.set("agentModel", args.value("model", ""));
        if (args.contains("apiKey"))
            cfg.set("agentApiKey", args.value("apiKey", "")); // empty = fall back to env
        if (args.contains("stream"))
            cfg.set("agentStream", args.value("stream", true));
        if (args.contains("thinking"))
            cfg.set("agentThinking", args.value("thinking", true));
        res["ok"] = true;
    } else if (cmd == "agent.models") {
        fetchModels();
        res["ok"] = true;
        res["data"] = {{"pending", true}};
    } else if (cmd == "agent.history") {
        res["ok"] = true;
        res["data"] = uiHistory(m_session->historySnapshot());
    } else if (cmd == "agent.clear") {
        if (m_session->isBusy()) {
            res["ok"] = false;
            res["error"] = "busy";
        } else {
            m_session->clearHistory();
            res["ok"] = true;
        }
    } else if (cmd == "agent.tools") {
        json arr = json::array();
        for (const auto& t : m_session->registry().allowed())
            arr.push_back({{"name", t.name}, {"description", t.description}, {"risk", agent::riskName(t.risk)}});
        res["ok"] = true;
        res["data"] = arr;
    } else {
        res["ok"] = false;
        res["error"] = "unknown cmd: " + cmd;
    }
}

} // namespace reals::bridge
