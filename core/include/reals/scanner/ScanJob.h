#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace reals::scanner {

// CPU utilization mode for background scanning
enum class CpuMode {
    Low = 0,      // ~30% CPU (Quiet, yields to other apps & DAW)
    Normal = 1,   // ~50% CPU (Balanced, default recommended)
    High = 2      // ~85% CPU (Fast processing, high multi-core throughput)
};

// Options controlling the background directory scan behavior.
struct ScanOptions {
    size_t numThreads = 0;              // 0 = automatic (based on cpuMode and hardware concurrency)
    CpuMode cpuMode = CpuMode::Normal;  // Default: Normal (50%)
    int throttleSleepMs = 0;            // Optional throttle sleep between files
    bool forceRescan = false;           // If true, recalculates hashes even if size/mtime match
    bool extractAudioInfo = true;       // Extract duration, sample rate, channels via audio probe
    std::vector<std::string> customExtensions; // Custom audio extensions (default: wav, mp3, flac, etc.)
};

// Real-time snapshot of the scanning job progress.
struct ScanProgress {
    int totalFiles = 0;
    int processedFiles = 0;
    int addedCount = 0;
    int updatedCount = 0;
    int skippedCount = 0;
    int errorCount = 0;
    std::string currentFile;
    bool isComplete = false;
    bool isPaused = false;
    bool isCancelled = false;

    [[nodiscard]] double fraction() const {
        return totalFiles > 0 ? static_cast<double>(processedFiles) / totalFiles : 0.0;
    }
};

} // namespace reals::scanner
