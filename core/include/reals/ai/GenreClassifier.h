#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace reals::ai {

struct GenreResult {
    std::string tag;    // e.g. "Trap-EDM", "Minimal House", "Deep House"
    float score = 0.0f; // Confidence score in [0.0, 1.0]
};

class GenreClassifier {
public:
    // Classify audio into top-K subgenres from the 400 Discogs-MAEST taxonomy
    [[nodiscard]] static std::vector<GenreResult> classify(
        const float* pcm, size_t frames, int sampleRate, int topK = 5);

    // Get list of all 400 supported subgenres
    [[nodiscard]] static const std::vector<std::string>& getTaxonomy();
};

} // namespace reals::ai
