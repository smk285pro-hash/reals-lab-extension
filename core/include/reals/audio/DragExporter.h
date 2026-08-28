#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace reals::audio {

struct DragExportOptions {
    float timeRatio = 1.0f;       // projectBpm / sampleBpm (clamped [0.25, 4.0])
    float pitchSemitones = 0.0f;  // [-12.0, +12.0]
    std::string customOutputDir;  // Empty = %TEMP%/RealsLab/drag_export/
    bool forceFloat32 = false;    // false = 16-bit PCM (fastest I/O), true = 32-bit float
};

struct DragExportResult {
    bool success = false;
    std::string renderedPath;     // Absolute path to the rendered WAV
    double durationSeconds = 0.0;
    int sampleRate = 44100;
    int channels = 2;
    double renderTimeMs = 0.0;
    std::string errorMessage;
};

class DragExporter {
public:
    // Renders time-stretched / pitch-shifted audio to a temp WAV file (< 5ms)
    [[nodiscard]] static DragExportResult exportTempWav(
        const std::string& inputPath,
        const DragExportOptions& options
    );

    // Computes deterministic temp file path and cache key
    [[nodiscard]] static std::string getTempExportPath(
        const std::string& inputPath,
        float timeRatio,
        float pitchSemitones,
        const std::string& customOutputDir = ""
    );

    // Prunes temp export files older than maxAgeSeconds (default 24h)
    static void cleanupTempFiles(uint64_t maxAgeSeconds = 86400, const std::string& customDir = "");
};

} // namespace reals::audio
