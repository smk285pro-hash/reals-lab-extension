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
};

} // namespace reals::ai
