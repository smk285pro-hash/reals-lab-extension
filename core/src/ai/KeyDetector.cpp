#include "reals/ai/KeyDetector.h"
#include "reals/ai/FeatureExtractor.h"
#include "reals/ai/ModelManager.h"
#include "reals/ai/OnnxEngine.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>

namespace reals::ai {

namespace {

const std::array<std::string, 12> kPitchNames = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"
};

// 1. EDMA (Electronic Dance Music Algorithm) profiles
const std::array<float, 12> kEdmaMajor = {
    6.0f, 1.0f, 3.5f, 1.0f, 4.0f, 3.0f, 1.0f, 5.0f, 1.5f, 3.5f, 1.0f, 2.5f
};
const std::array<float, 12> kEdmaMinor = {
    6.0f, 1.0f, 3.0f, 5.0f, 1.5f, 3.5f, 1.0f, 4.5f, 3.5f, 1.5f, 2.0f, 3.0f
};

// 2. Temperley (1999) profiles
const std::array<float, 12> kTemperleyMajor = {
    5.0f, 2.0f, 3.5f, 2.0f, 4.5f, 4.0f, 2.0f, 4.5f, 2.0f, 3.5f, 1.5f, 4.0f
};
const std::array<float, 12> kTemperleyMinor = {
    5.0f, 2.0f, 3.5f, 4.5f, 2.0f, 4.0f, 2.0f, 4.5f, 3.5f, 2.0f, 1.5f, 4.0f
};

// 3. Krumhansl-Schmuckler (1990) profiles
const std::array<float, 12> kKrumhanslMajor = {
    6.35f, 2.23f, 3.48f, 2.33f, 4.38f, 4.09f, 2.52f, 5.19f, 2.39f, 3.66f, 2.29f, 2.88f
};
const std::array<float, 12> kKrumhanslMinor = {
    6.33f, 2.68f, 3.52f, 5.38f, 2.60f, 3.53f, 2.54f, 4.75f, 3.98f, 2.69f, 3.34f, 3.17f
};

// Compute normalized Pearson correlation coefficient between chroma and rotated profile
float correlateProfile(const std::vector<float>& chroma, const std::array<float, 12>& profile, int shift) {
    float meanC = 0.0f;
    float meanP = 0.0f;
    for (int i = 0; i < 12; ++i) {
        meanC += chroma[i];
        meanP += profile[i];
    }
    meanC /= 12.0f;
    meanP /= 12.0f;

    float dot = 0.0f;
    float varC = 0.0f;
    float varP = 0.0f;
    for (int c = 0; c < 12; ++c) {
        int pIdx = (c - shift + 12) % 12;
        float diffC = chroma[c] - meanC;
        float diffP = profile[pIdx] - meanP;
        dot += diffC * diffP;
        varC += diffC * diffC;
        varP += diffP * diffP;
    }
    const float denom = std::sqrt(varC * varP);
    if (denom <= 1e-7f) return 0.0f;
    return dot / denom;
}

} // namespace

KeyResult KeyDetector::detect(const float* pcm, size_t frames, int sampleRate) {
    if (!pcm || frames == 0 || sampleRate <= 0) {
        return KeyResult{"C", "Major", "8B", "1d", 0.0f};
    }

    // 1. Resample to standard 44.1kHz mono
    constexpr int kTargetRate = 44100;
    auto audio = FeatureExtractor::resampleMono(pcm, frames, 1, sampleRate, kTargetRate);
    if (audio.empty()) {
        return KeyResult{"C", "Major", "8B", "1d", 0.0f};
    }

    // 2. Compute 12-bin global chromagram
    auto chroma = FeatureExtractor::computeGlobalChroma(audio, kTargetRate, 2048, 512);

    // 3. Ensemble voting across 24 candidates (12 major, 12 minor)
    float bestScore = -2.0f;
    int bestPitch = 0;
    bool isMajor = true;

    // Weights: EDMA (0.45), Temperley (0.30), Krumhansl (0.25)
    constexpr float wEdma = 0.45f;
    constexpr float wTemp = 0.30f;
    constexpr float wKrum = 0.25f;

    for (int p = 0; p < 12; ++p) {
        // Major correlation
        float edmaMaj = correlateProfile(chroma, kEdmaMajor, p);
        float tempMaj = correlateProfile(chroma, kTemperleyMajor, p);
        float krumMaj = correlateProfile(chroma, kKrumhanslMajor, p);
        float scoreMaj = wEdma * edmaMaj + wTemp * tempMaj + wKrum * krumMaj;

        if (scoreMaj > bestScore) {
            bestScore = scoreMaj;
            bestPitch = p;
            isMajor = true;
        }

        // Minor correlation
        float edmaMin = correlateProfile(chroma, kEdmaMinor, p);
        float tempMin = correlateProfile(chroma, kTemperleyMinor, p);
        float krumMin = correlateProfile(chroma, kKrumhanslMinor, p);
        float scoreMin = wEdma * edmaMin + wTemp * tempMin + wKrum * krumMin;

        if (scoreMin > bestScore) {
            bestScore = scoreMin;
            bestPitch = p;
            isMajor = false;
        }
    }

    KeyResult res;
    res.key = kPitchNames[bestPitch];
    res.mode = isMajor ? "Major" : "Minor";
    res.camelot = toCamelot(res.key, res.mode);
    res.openKey = toOpenKey(res.key, res.mode);
    res.confidence = std::clamp((bestScore + 1.0f) * 0.5f, 0.1f, 0.99f);

    return res;
}

std::string KeyDetector::toCamelot(const std::string& key, const std::string& mode) {
    const bool isMajor = (mode == "Major");

    // Standard Camelot wheel notation
    if (key == "C" || key == "B#") return isMajor ? "8B" : "5A";
    if (key == "C#" || key == "Db") return isMajor ? "3B" : "12A";
    if (key == "D") return isMajor ? "10B" : "7A";
    if (key == "D#" || key == "Eb") return isMajor ? "5B" : "2A";
    if (key == "E") return isMajor ? "12B" : "9A";
    if (key == "F") return isMajor ? "7B" : "4A";
    if (key == "F#" || key == "Gb") return isMajor ? "2B" : "11A";
    if (key == "G") return isMajor ? "9B" : "6A";
    if (key == "G#" || key == "Ab") return isMajor ? "4B" : "1A";
    if (key == "A") return isMajor ? "11B" : "8A";
    if (key == "A#" || key == "Bb") return isMajor ? "6B" : "3A";
    if (key == "B" || key == "Cb") return isMajor ? "1B" : "10A";

    return isMajor ? "8B" : "5A";
}

std::string KeyDetector::toOpenKey(const std::string& key, const std::string& mode) {
    const bool isMajor = (mode == "Major");

    // Standard OpenKey notation (1d-12d for major, 1m-12m for minor)
    if (key == "C" || key == "B#") return isMajor ? "1d" : "1m";
    if (key == "C#" || key == "Db") return isMajor ? "8d" : "8m";
    if (key == "D") return isMajor ? "3d" : "3m";
    if (key == "D#" || key == "Eb") return isMajor ? "10d" : "10m";
    if (key == "E") return isMajor ? "5d" : "5m";
    if (key == "F") return isMajor ? "12d" : "12m";
    if (key == "F#" || key == "Gb") return isMajor ? "7d" : "7m";
    if (key == "G") return isMajor ? "2d" : "2m";
    if (key == "G#" || key == "Ab") return isMajor ? "9d" : "9m";
    if (key == "A") return isMajor ? "4d" : "4m";
    if (key == "A#" || key == "Bb") return isMajor ? "11d" : "11m";
    if (key == "B" || key == "Cb") return isMajor ? "6d" : "6m";

    return isMajor ? "1d" : "1m";
}

} // namespace reals::ai
