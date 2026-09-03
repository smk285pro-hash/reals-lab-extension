#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace reals::ai {

struct KeyResult {
    std::string key;        // e.g. "C", "F#", "A"
    std::string mode;       // "Major" or "Minor"
    std::string camelot;    // e.g. "8B", "11A", "2B"
    std::string openKey;    // e.g. "1d", "7m", "12d"
    float confidence = 0.0f;
};

class KeyDetector {
public:
    // Main key detection with EDMA + Temperley + Krumhansl ensemble voting
    [[nodiscard]] static KeyResult detect(
        const float* pcm, size_t frames, int sampleRate);

    // Helpers to convert tonic & mode to Camelot and OpenKey notations
    [[nodiscard]] static std::string toCamelot(const std::string& key, const std::string& mode);
    [[nodiscard]] static std::string toOpenKey(const std::string& key, const std::string& mode);
    [[nodiscard]] static std::pair<std::string, std::string> fromCamelot(const std::string& camelot);
};

} // namespace reals::ai
