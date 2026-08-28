#pragma once

#include "reals/db/Database.h"
#include "reals/scanner/ScanJob.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace reals::scanner {

class BackgroundScanner {
public:
    using ProgressCallback = std::function<void(const ScanProgress&)>;
    using SampleCallback = std::function<void(const db::SampleRecord&)>;

    explicit BackgroundScanner(db::Database& db);
    ~BackgroundScanner();

    BackgroundScanner(const BackgroundScanner&) = delete;
    BackgroundScanner& operator=(const BackgroundScanner&) = delete;
    BackgroundScanner(BackgroundScanner&&) = delete;
    BackgroundScanner& operator=(BackgroundScanner&&) = delete;

    // Start background scan over specified root directories.
    // Asynchronous: returns immediately and spawns worker pool.
    bool startScan(const std::vector<std::string>& roots, const ScanOptions& options = {});

    // Execution controls
    void pause();
    void resume();
    void cancel();
    void waitForCompletion();

    // Status queries
    [[nodiscard]] bool isScanning() const;
    [[nodiscard]] bool isPaused() const;
    [[nodiscard]] bool isCancelled() const;
    [[nodiscard]] ScanProgress getProgress() const;

    // Dynamic runtime CPU configuration (takes effect immediately without restart)
    void setCpuMode(CpuMode mode);
    [[nodiscard]] CpuMode getCpuMode() const;

    // Callbacks
    void setProgressCallback(ProgressCallback cb);
    void setSampleCallback(SampleCallback cb);

    // Default supported audio extensions check
    [[nodiscard]] static bool isSupportedAudioExtension(std::string_view filename);

private:
    void coordinatorThreadFunc(std::vector<std::string> roots, ScanOptions options);
    void workerThreadFunc(ScanOptions options);
    void checkPause();
    void emitProgress(bool force = false);

    db::Database& m_db;

    std::atomic<bool> m_isScanning{false};
    std::atomic<bool> m_isPaused{false};
    std::atomic<bool> m_isCancelled{false};
    std::atomic<bool> m_discoveryComplete{false};
    std::atomic<CpuMode> m_cpuMode{CpuMode::Normal};
    std::atomic<int> m_throttleSleepMs{5};
    std::atomic<size_t> m_maxActiveWorkers{4};
    std::atomic<size_t> m_currentActiveWorkers{0};
    mutable std::mutex m_activeWorkersMutex;
    std::condition_variable m_activeWorkersCv;

    // Queue & worker synchronization
    std::queue<std::string> m_workQueue;
    mutable std::mutex m_queueMutex;
    std::condition_variable m_queueCv;
    std::condition_variable m_pauseCv;
    std::condition_variable m_completionCv;

    std::thread m_coordinatorThread;
    std::vector<std::thread> m_workers;

    // Progress metrics
    mutable std::mutex m_progressMutex;
    ScanProgress m_progress;
    std::chrono::steady_clock::time_point m_lastProgressEmit;

    ProgressCallback m_progressCallback;
    SampleCallback m_sampleCallback;
};

} // namespace reals::scanner
