#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace reals::ai {

struct MoodResult {
    std::string tag;    // e.g. "dark", "energetic", "relaxing", "happy"
    float score = 0.0f; // Sigmoid confidence score in [0.0, 1.0]
};

class MoodClassifier {
public:
    // Multi-label mood and theme classification across the 56 Jamendo tags
    [[nodiscard]] static std::vector<MoodResult> classify(
        const float* pcm, size_t frames, int sampleRate, float threshold = 0.20f);

    // Get list of all 56 supported mood tags
    [[nodiscard]] static const std::vector<std::string>& getMoodTags();
};

} // namespace reals::ai
