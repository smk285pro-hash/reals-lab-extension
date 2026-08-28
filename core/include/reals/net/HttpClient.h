#pragma once

// HTTP client interface. The only allowed gateway for network in the project.
// Implementation wraps WinHTTP on Windows (see SPEC.md architecture rules);
// other platforms currently have no transport — the symbols stay defined so
// shells link, but the object file compiles empty there.
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace reals::net {

struct Response {
    long statusCode = 0;
    std::string body;
    std::string error; // non-empty when the request failed at transport level
    std::map<std::string, std::string> headers;
};

struct Request {
    std::string method = "GET";
    std::string url;
    std::map<std::string, std::string> headers;
    std::string body;
    // Progress callback: receives bytes downloaded so far / total (0 if unknown).
    std::function<bool(size_t, size_t)> onProgress; // return false to cancel
};

class HttpClient {
public:
    static HttpClient& instance();

    ~HttpClient();

    void setBaseUrl(std::string_view url);
    void setAuthToken(std::string_view token);

    [[nodiscard]] Response send(const Request& req);

    // Download to file with resume support. Returns HTTP status or 0 on error.
    long downloadToFile(const std::string& url, const std::string& destPath,
                        const std::function<bool(size_t, size_t)>& onProgress = {});

    // Multipart file upload (used by audio analysis).
    Response uploadFile(const std::string& url, const std::string& fieldName,
                        const std::string& filePath,
                        const std::map<std::string, std::string>& extraFields = {});

private:
    HttpClient();
    HttpClient(const HttpClient&) = delete;
    HttpClient& operator=(const HttpClient&) = delete;
    struct Impl;
    std::unique_ptr<Impl> m_impl; // PIMPL: keeps WinHTTP out of public headers (MAJ-07)
};

} // namespace reals::net
