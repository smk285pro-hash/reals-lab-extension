#pragma once

// Client for the RealS audio analysis API (FastAPI on Modal).
// Endpoints doc: API_DOCS.md (analyze, chords, separate, denoise, jobs).
// Transport goes through net::HttpClient (AGENTS.md: network only there).
// Base URL comes from Config "labApiBaseUrl"; kBase is the fallback default.
//
// The class is declared unconditionally (MAJ-09): callers like Bridge.cpp
// compile on every platform. The Windows TU implements it on WinHTTP; other
// platforms get throwing stubs (see LabApi.cpp) so links stay clean.
#include <nlohmann/json.hpp>

#include <string>

namespace reals::lab {

class LabApi {
public:
    static constexpr const char* kBase =
        "https://smk285pro--ai-audio-lab-fastapi-web.modal.run";

    // Sync: tempo + key (2-5s server-side).
    static nlohmann::json analyze(const std::string& filePath);
    // Sync: chords (15-60s server-side).
    static nlohmann::json chords(const std::string& filePath);
    // Async: start stem separation, returns {task_id, status_url}.
    static nlohmann::json startSeparate(const std::string& filePath, int stemMode);
    // Async: start denoise, returns {task_id, status_url}.
    static nlohmann::json startDenoise(const std::string& filePath, int strength);
    // Poll job: {status, percent, stage, error, result?}.
    static nlohmann::json pollJob(const std::string& taskId);
    // Download urlOrPath (absolute URL or root-relative) to destPath.
    static bool downloadToFile(const std::string& urlOrPath, const std::string& destPath);
};

} // namespace reals::lab
