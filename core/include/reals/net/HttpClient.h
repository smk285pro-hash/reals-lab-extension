#pragma once

// HTTP client interface. The only allowed gateway for network in the project.
// Implementation wraps WinHTTP on Windows (see SPEC.md architecture rules);
// other platforms currently have no transport — the symbols stay defined so
// shells link, but the object file compiles empty there.
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

namespace reals::net {

struct Response {
    long statusCode = 0;
    std::string body;
    std::string error; // non-empty when the request failed at transport level
    std::map<std::string, std::string> headers;
};

// Cross-thread cancellation for an in-flight request. cancel() may be called
// from any thread: it marks the token and runs the transport's abort hook
// (WinHTTP: closes the request handle, which unblocks a pending read).
// The hook runs under the token mutex, so disarm() returning guarantees the
// hook either completed or will never run.
class CancelToken {
public:
    void cancel() {
        const std::lock_guard lock(m_mutex);
        if (m_cancelled)
            return;
        m_cancelled = true;
        if (m_abort) {
            m_abort();
            m_abort = nullptr;
            m_aborted = true;
        }
    }
    [[nodiscard]] bool isCancelled() const {
        const std::lock_guard lock(m_mutex);
        return m_cancelled;
    }
    // Installs the abort hook. Returns false (hook not installed) when the
    // token is already cancelled.
    bool arm(std::function<void()> abortFn) {
        const std::lock_guard lock(m_mutex);
        if (m_cancelled)
            return false;
        m_abort = std::move(abortFn);
        m_aborted = false;
        return true;
    }
    // Removes the hook. Returns true when the hook already ran (the transport
    // resource it owned is gone and must not be released again).
    bool disarm() {
        const std::lock_guard lock(m_mutex);
        m_abort = nullptr;
        const bool aborted = m_aborted;
        m_aborted = false;
        return aborted;
    }

private:
    mutable std::mutex m_mutex;
    bool m_cancelled = false;
    bool m_aborted = false;
    std::function<void()> m_abort;
};

struct Request {
    std::string method = "GET";
    std::string url;
    std::map<std::string, std::string> headers;
    std::string body;
    // Progress callback: receives bytes downloaded so far / total (0 if unknown).
    std::function<bool(size_t, size_t)> onProgress; // return false to cancel
    // Streaming body sink (e.g. SSE). When set and the status is 2xx, each
    // received chunk is passed here instead of being accumulated in
    // Response::body. Return false to stop reading (error = "cancelled").
    // Non-2xx bodies are still accumulated so callers can show the error.
    std::function<bool(const char*, size_t)> onData;
    // Optional cancel token; cancel() aborts the request from another thread.
    std::shared_ptr<CancelToken> cancel;
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
