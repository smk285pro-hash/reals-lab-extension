#pragma once

// Directory change watcher. Windows: ReadDirectoryChangesW. Other OS: no-op.
// Callback fires on a worker thread — the receiver MUST be thread-safe
// (e.g. push onto the bridge event queue). core/ stays free of REAPER/UI.
#include <atomic>
#include <functional>
#include <string>
#include <thread>

namespace reals::platform {

class DirWatch {
public:
    using Fn = std::function<void(const std::string& dir)>;

    DirWatch();
    ~DirWatch();

    DirWatch(const DirWatch&) = delete;
    DirWatch& operator=(const DirWatch&) = delete;

    // Start watching `dir` (non-recursive). Restarts if already running.
    void start(const std::string& dir, Fn onChange);
    void stop();

    [[nodiscard]] std::string directory() const { return m_dir; }

private:
    void threadMain();

    std::string m_dir;
    Fn m_cb;
    std::atomic<bool> m_run{false};
    std::thread m_th;
#ifdef _WIN32
    std::atomic<void*> m_iocp{nullptr}; // HANDLE — posted to wake GetQueuedCompletionStatus
#endif
};

} // namespace reals::platform
