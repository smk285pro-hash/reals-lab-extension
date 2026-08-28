#include "reals/lab/LabApi.h"

#ifdef _WIN32

#include "reals/config/Config.h"
#include "reals/net/HttpClient.h"
#include "reals/util/Log.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace reals::lab {

namespace {

// Base URL is configurable (Config "labApiBaseUrl"); the compile-time constant
// is only a fallback.
std::string baseUrl() {
    const std::string fromConfig = config::Config::instance().labApiBaseUrl();
    return fromConfig.empty() ? std::string(LabApi::kBase) : fromConfig;
}

net::HttpClient& client() {
    auto& c = net::HttpClient::instance();
    c.setBaseUrl(baseUrl());
    c.setAuthToken(config::Config::instance().authToken());
    return c;
}

std::string withQuery(const std::string& path, const std::string& query) {
    return query.empty() ? path : path + "?" + query;
}

nlohmann::json parseOrThrow(const net::Response& r) {
    if (r.statusCode < 200 || r.statusCode >= 300) {
        std::string msg = "HTTP " + std::to_string(r.statusCode);
        if (!r.error.empty())
            msg += ": " + r.error;
        throw std::runtime_error(msg);
    }
    auto j = nlohmann::json::parse(r.body, nullptr, false);
    if (j.is_discarded())
        throw std::runtime_error("bad json from api");
    return j;
}

nlohmann::json uploadAndParse(const std::string& pathWithQuery, const std::string& filePath) {
    return parseOrThrow(client().uploadFile(pathWithQuery, "file", filePath));
}

} // namespace

nlohmann::json LabApi::analyze(const std::string& filePath) {
    return uploadAndParse("/api/v1/analyze", filePath);
}

nlohmann::json LabApi::chords(const std::string& filePath) {
    return uploadAndParse("/api/v1/chords", filePath);
}

nlohmann::json LabApi::startSeparate(const std::string& filePath, const int stemMode) {
    return uploadAndParse(withQuery("/api/v1/separate", "stem_mode=" + std::to_string(stemMode)),
                          filePath);
}

nlohmann::json LabApi::startDenoise(const std::string& filePath, const int strength) {
    return uploadAndParse(withQuery("/api/v1/denoise", "strength=" + std::to_string(strength)),
                          filePath);
}

nlohmann::json LabApi::pollJob(const std::string& taskId) {
    net::Request req;
    req.method = "GET";
    req.url = withQuery("/api/v1/jobs/" + taskId, "");
    return parseOrThrow(client().send(req));
}

bool LabApi::downloadToFile(const std::string& urlOrPath, const std::string& destPath) {
    // Absolute URLs pass through; root-relative paths resolve against the base.
    const long status = client().downloadToFile(urlOrPath, destPath);
    return status >= 200 && status < 300;
}

} // namespace reals::lab

#else // !_WIN32 — MAJ-09

// Non-Windows builds: no network transport exists yet (HttpClient is
// WinHTTP-only). Previously this whole file compiled to an empty TU, which
// left Bridge.cpp's calls to LabApi::* as unresolved symbols on macOS/Linux.
// Keep the symbols defined so every shell links, and fail at runtime with a
// clear error — the Bridge lab workers catch std::runtime_error and surface
// it to the UI as a lab.error event.
#include <stdexcept>

namespace reals::lab {

namespace {
[[noreturn]] void throwUnsupported() {
    throw std::runtime_error("LabApi: network transport is only available on Windows in this build");
}
} // namespace

nlohmann::json LabApi::analyze(const std::string&) { throwUnsupported(); }
nlohmann::json LabApi::chords(const std::string&) { throwUnsupported(); }
nlohmann::json LabApi::startSeparate(const std::string&, int) { throwUnsupported(); }
nlohmann::json LabApi::startDenoise(const std::string&, int) { throwUnsupported(); }
nlohmann::json LabApi::pollJob(const std::string&) { throwUnsupported(); }
bool LabApi::downloadToFile(const std::string&, const std::string&) { throwUnsupported(); }

} // namespace reals::lab

#endif // _WIN32
