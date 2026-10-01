#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace reals::ai {

struct TempoResult {
    float bpm = 0.0f;
    float confidence = 0.0f;
    std::vector<float> beatOnsets; // Beat onset timestamps in seconds
    std::string method;           // "tempo_cnn" or "rhythm_extractor_2013"
};

class TempoDetector {
public:
    // Main detection entrypoint: tries TempoCNN, falls back to RhythmExtractor2013
    [[nodiscard]] static TempoResult detect(
        const float* pcm, size_t frames, int sampleRate);

    // Essentia TempoCNN inference mode
    [[nodiscard]] static TempoResult detectCnn(
        const float* pcm, size_t frames, int sampleRate);

    // Algorithmic fallback mode (RhythmExtractor2013 autocorrelation + comb-filter)
    [[nodiscard]] static TempoResult detectAlgorithmic(
        const float* pcm, size_t frames, int sampleRate);

    // Duration-bars heuristic: vocal beds / pads are almost always cut to an
    // exact bar count, so bars*4*60/duration lands close to an integer BPM.
    // Used when the onset/ACF detector cannot decide. Returns 0.0 when no
    // clean integer tempo is found. durationSec must be >= ~3s to be meaningful.
    [[nodiscard]] static float detectFromDuration(double durationSec);
};

} // namespace reals::ai
