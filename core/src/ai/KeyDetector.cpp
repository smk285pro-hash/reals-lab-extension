#include "reals/ai/KeyDetector.h"
#include "reals/ai/FeatureExtractor.h"

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

constexpr float kPi = 3.14159265358979323846f;
constexpr float kTwoPi = 6.28318530717958647692f;
constexpr int kTargetRate = 44100;
constexpr int kNfft = 4096;
constexpr int kHopLength = 1024;
constexpr float kBinHz = static_cast<float>(kTargetRate) / static_cast<float>(kNfft); // ~10.7666 Hz

struct SpectralPeak {
    float freq = 0.0f;
    float amp = 0.0f;
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

// Emilia Gómez (2006) / Essentia Harmonic Pitch Class Profile (HPCP) Extractor
std::vector<float> computeHpcpChroma(const std::vector<float>& audio) {
    if (audio.empty()) return std::vector<float>(12, 0.0f);

    // Ensure audio has at least kNfft samples (zero-pad if shorter)
    std::vector<float> paddedAudio;
    const std::vector<float>* audioPtr = &audio;
    if (audio.size() < static_cast<size_t>(kNfft)) {
        paddedAudio = audio;
        paddedAudio.resize(kNfft, 0.0f);
        audioPtr = &paddedAudio;
    }

    const size_t totalSamples = audioPtr->size();
    const size_t numFrames = (totalSamples >= static_cast<size_t>(kNfft))
        ? ((totalSamples - kNfft) / kHopLength + 1)
        : 1;

    // Pre-calculate Hann window
    std::vector<float> hannWindow(kNfft);
    for (int i = 0; i < kNfft; ++i) {
        hannWindow[i] = 0.5f * (1.0f - std::cos(kTwoPi * static_cast<float>(i) / static_cast<float>(kNfft)));
    }

    const int kMinBin = std::max(1, static_cast<int>(std::floor(70.0f / kBinHz)));
    const int kMaxBin = std::min(kNfft / 2 - 1, static_cast<int>(std::ceil(4500.0f / kBinHz)));

    std::vector<std::vector<SpectralPeak>> allFramePeaks;
    allFramePeaks.reserve(numFrames);

    std::vector<float> tuningHist(100, 0.0f);
    float totalTuningEnergy = 0.0f;

    std::vector<std::complex<float>> complexFrame(kNfft);
    std::vector<float> frameMags(kNfft / 2 + 1);

    // Step 1 & 2: STFT, Peak Extraction & Parabolic Interpolation
    for (size_t t = 0; t < numFrames; ++t) {
        const size_t start = t * kHopLength;
        for (int i = 0; i < kNfft; ++i) {
            float sample = (start + i < totalSamples) ? (*audioPtr)[start + i] : 0.0f;
            complexFrame[i] = std::complex<float>(sample * hannWindow[i], 0.0f);
        }

        FeatureExtractor::fft(complexFrame);

        float maxMag = 0.0f;
        for (int k = 0; k <= kNfft / 2; ++k) {
            float m = std::abs(complexFrame[k]);
            frameMags[k] = m;
            if (k >= 1 && k < kNfft / 2) {
                maxMag = std::max(maxMag, m);
            }
        }

        if (maxMag < 1e-6f) {
            allFramePeaks.emplace_back();
            continue;
        }

        const float relThreshold = maxMag * 0.001f; // -60dB relative threshold
        std::vector<SpectralPeak> framePeaks;
        framePeaks.reserve(64);

        for (int k = kMinBin; k <= kMaxBin; ++k) {
            const float mag = frameMags[k];
            if (mag < relThreshold) continue;
            if (mag <= frameMags[k - 1] || mag <= frameMags[k + 1]) continue; // local peak picking

            // Step 2: Parabolic (Quadratic) Peak Interpolation on Log Magnitude
            const float alpha = 20.0f * std::log10(frameMags[k - 1] + 1e-12f);
            const float beta  = 20.0f * std::log10(mag + 1e-12f);
            const float gamma = 20.0f * std::log10(frameMags[k + 1] + 1e-12f);

            const float denom = alpha - 2.0f * beta + gamma;
            float delta = 0.0f;
            if (std::abs(denom) > 1e-7f) {
                delta = 0.5f * (alpha - gamma) / denom;
                delta = std::clamp(delta, -0.5f, 0.5f);
            }

            const float fp = (static_cast<float>(k) + delta) * kBinHz;
            const float dbp = beta - 0.25f * (alpha - gamma) * delta;
            const float ampp = std::pow(10.0f, dbp / 20.0f);

            framePeaks.push_back({fp, ampp});

            // Accumulate tuning deviations for reference tuning compensation (100 Hz to 2000 Hz)
            if (fp >= 100.0f && fp <= 2000.0f) {
                const float freqMod = fp / 440.0f;
                const float cents = 1200.0f * std::log2(freqMod);
                const float centDev = cents - 100.0f * std::round(cents / 100.0f);
                const int histBin = std::clamp(static_cast<int>(std::floor(centDev + 50.0f)), 0, 99);
                const float wTune = std::pow(ampp, 0.75f);
                tuningHist[histBin] += wTune;
                totalTuningEnergy += wTune;
            }
        }

        // Keep top 60 peaks by amplitude
        if (framePeaks.size() > 60) {
            std::partial_sort(framePeaks.begin(), framePeaks.begin() + 60, framePeaks.end(),
                [](const SpectralPeak& a, const SpectralPeak& b) {
                    return a.amp > b.amp;
                });
            framePeaks.resize(60);
        }

        allFramePeaks.push_back(std::move(framePeaks));
    }

    // Step 3: Reference Tuning Compensation
    float deltaTuneCents = 0.0f;
    if (totalTuningEnergy > 1e-5f) {
        // Smooth tuning histogram with [0.25, 0.5, 0.25] kernel
        std::vector<float> smoothedHist(100, 0.0f);
        for (int i = 0; i < 100; ++i) {
            float prev = (i > 0) ? tuningHist[i - 1] : tuningHist[i];
            float next = (i < 99) ? tuningHist[i + 1] : tuningHist[i];
            smoothedHist[i] = 0.25f * prev + 0.5f * tuningHist[i] + 0.25f * next;
        }

        int modeBin = 50;
        float maxWeight = 0.0f;
        for (int i = 0; i < 100; ++i) {
            if (smoothedHist[i] > maxWeight) {
                maxWeight = smoothedHist[i];
                modeBin = i;
            }
        }
        deltaTuneCents = (static_cast<float>(modeBin) + 0.5f) - 50.0f;
    }

    // Step 4: Harmonic Summation on 36-Bin Sub-Semitone Grid
    constexpr int kGridSize = 36;
    std::vector<float> globalHpcp(kGridSize, 0.0f);

    for (const auto& peaks : allFramePeaks) {
        if (peaks.empty()) continue;

        std::vector<float> frameHpcp(kGridSize, 0.0f);

        for (const auto& peak : peaks) {
            for (int h = 1; h <= 8; ++h) {
                const float f0 = peak.freq / static_cast<float>(h);
                if (f0 < 50.0f || f0 > 2500.0f) continue;

                const float wh = std::pow(0.6f, static_cast<float>(h));
                const float pc = 12.0f * std::log2((f0 / 440.0f) * std::pow(2.0f, -deltaTuneCents / 1200.0f)) + 69.0f;
                const float pNorm = std::fmod(std::fmod(pc, 12.0f) + 12.0f, 12.0f);
                const float continuousBin = pNorm * 3.0f;

                const int centerBin = static_cast<int>(std::round(continuousBin));
                for (int off = -3; off <= 3; ++off) {
                    int bIdx = (centerBin + off) % kGridSize;
                    if (bIdx < 0) bIdx += kGridSize;

                    float d = std::abs(continuousBin - static_cast<float>(centerBin + off));
                    if (d <= 3.0f) {
                        float cosVal = std::cos((kPi * 0.5f) * (d / 3.0f));
                        float wBin = cosVal * cosVal;
                        frameHpcp[bIdx] += wh * std::pow(peak.amp, 0.75f) * wBin;
                    }
                }
            }
        }

        // Frame normalization
        float maxFrame = 0.0f;
        for (int b = 0; b < kGridSize; ++b) {
            maxFrame = std::max(maxFrame, frameHpcp[b]);
        }
        if (maxFrame > 1e-7f) {
            for (int b = 0; b < kGridSize; ++b) {
                globalHpcp[b] += frameHpcp[b] / maxFrame;
            }
        }
    }

    // Global HPCP normalization
    float maxGlobal = 0.0f;
    for (int b = 0; b < kGridSize; ++b) {
        maxGlobal = std::max(maxGlobal, globalHpcp[b]);
    }
    if (maxGlobal > 1e-7f) {
        for (int b = 0; b < kGridSize; ++b) {
            globalHpcp[b] /= maxGlobal;
        }
    }

    // Step 5: Fold 36 sub-bins into 12-semitone chroma vector
    std::vector<float> chroma(12, 0.0f);
    for (int c = 0; c < 12; ++c) {
        int left = (3 * c - 1 + kGridSize) % kGridSize;
        int center = 3 * c;
        int right = (3 * c + 1) % kGridSize;
        chroma[c] = globalHpcp[left] + globalHpcp[center] + globalHpcp[right];
    }

    float maxC = 0.0f;
    for (int c = 0; c < 12; ++c) {
        maxC = std::max(maxC, chroma[c]);
    }
    if (maxC > 1e-7f) {
        for (int c = 0; c < 12; ++c) {
            chroma[c] /= maxC;
        }
    }

    return chroma;
}

} // namespace

KeyResult KeyDetector::detect(const float* pcm, size_t frames, int sampleRate) {
    if (!pcm || frames == 0 || sampleRate <= 0) {
        return KeyResult{"C", "Major", "8B", "1d", 0.0f};
    }

    // Fast-path early exit for pure digital silence or sub-noise floor
    // (bypasses expensive resampling and FFT calculations)
    bool hasPcmEnergy = false;
    for (size_t i = 0; i < frames; ++i) {
        if (std::abs(pcm[i]) >= 1e-6f) {
            hasPcmEnergy = true;
            break;
        }
    }
    if (!hasPcmEnergy) {
        return KeyResult{"C", "Major", "8B", "1d", 0.0f};
    }

    // 1. Resample to standard 44.1kHz mono
    auto audio = FeatureExtractor::resampleMono(pcm, frames, 1, sampleRate, kTargetRate);
    if (audio.empty()) {
        return KeyResult{"C", "Major", "8B", "1d", 0.0f};
    }

    // 2. High-Resolution Emilia Gómez 2006 / Essentia HPCP Chroma Extraction
    auto chroma = computeHpcpChroma(audio);

    // Guard against zero-energy chroma (e.g. non-tonal silence or out-of-band signals)
    float maxChroma = 0.0f;
    float chromaSum = 0.0f;
    for (float c : chroma) {
        maxChroma = std::max(maxChroma, c);
        chromaSum += c;
    }
    if (maxChroma < 1e-6f || chromaSum < 1e-6f) {
        return KeyResult{"C", "Major", "8B", "1d", 0.0f};
    }

    // 3. Ensemble voting across 24 candidates (12 major, 12 minor)
    float bestScore = -2.0f;
    float secondBestScore = -2.0f;
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
            secondBestScore = bestScore;
            bestScore = scoreMaj;
            bestPitch = p;
            isMajor = true;
        } else if (scoreMaj > secondBestScore) {
            secondBestScore = scoreMaj;
        }

        // Minor correlation
        float edmaMin = correlateProfile(chroma, kEdmaMinor, p);
        float tempMin = correlateProfile(chroma, kTemperleyMinor, p);
        float krumMin = correlateProfile(chroma, kKrumhanslMinor, p);
        float scoreMin = wEdma * edmaMin + wTemp * tempMin + wKrum * krumMin;

        if (scoreMin > bestScore) {
            secondBestScore = bestScore;
            bestScore = scoreMin;
            bestPitch = p;
            isMajor = false;
        } else if (scoreMin > secondBestScore) {
            secondBestScore = scoreMin;
        }
    }

    KeyResult res;
    res.key = kPitchNames[bestPitch];
    res.mode = isMajor ? "Major" : "Minor";
    res.camelot = toCamelot(res.key, res.mode);
    res.openKey = toOpenKey(res.key, res.mode);

    const float margin = std::max(0.0f, bestScore - secondBestScore);
    res.confidence = std::clamp((bestScore + 1.0f) * 0.4f + margin * 0.5f, 0.2f, 0.99f);

    return res;
}

std::string KeyDetector::toCamelot(const std::string& key, const std::string& mode) {
    const bool isMajor = (mode == "Major" || mode == "major");

    // Standard Camelot wheel notation
    if (key == "C" || key == "B#") return isMajor ? "8B" : "5A";
    if (key == "C#" || key == "Db") return isMajor ? "3B" : "12A";
    if (key == "D") return isMajor ? "10B" : "7A";
    if (key == "D#" || key == "Eb") return isMajor ? "5B" : "2A";
    if (key == "E" || key == "Fb") return isMajor ? "12B" : "9A";
    if (key == "F" || key == "E#") return isMajor ? "7B" : "4A";
    if (key == "F#" || key == "Gb") return isMajor ? "2B" : "11A";
    if (key == "G") return isMajor ? "9B" : "6A";
    if (key == "G#" || key == "Ab") return isMajor ? "4B" : "1A";
    if (key == "A") return isMajor ? "11B" : "8A";
    if (key == "A#" || key == "Bb") return isMajor ? "6B" : "3A";
    if (key == "B" || key == "Cb") return isMajor ? "1B" : "10A";

    return isMajor ? "8B" : "5A";
}

std::string KeyDetector::toOpenKey(const std::string& key, const std::string& mode) {
    const bool isMajor = (mode == "Major" || mode == "major");

    // Standard OpenKey notation (1d-12d major, 1m-12m minor). Relative
    // major/minor pairs share the same number (1d = C major, 1m = A minor),
    // mirroring the Camelot wheel. This table matches ModelMocks/DbTestFixtures.
    if (key == "C" || key == "B#") return isMajor ? "1d" : "10m";
    if (key == "C#" || key == "Db") return isMajor ? "8d" : "5m";
    if (key == "D") return isMajor ? "3d" : "12m";
    if (key == "D#" || key == "Eb") return isMajor ? "10d" : "7m";
    if (key == "E" || key == "Fb") return isMajor ? "5d" : "2m";
    if (key == "F" || key == "E#") return isMajor ? "12d" : "9m";
    if (key == "F#" || key == "Gb") return isMajor ? "7d" : "4m";
    if (key == "G") return isMajor ? "2d" : "11m";
    if (key == "G#" || key == "Ab") return isMajor ? "9d" : "6m";
    if (key == "A") return isMajor ? "4d" : "1m";
    if (key == "A#" || key == "Bb") return isMajor ? "11d" : "8m";
    if (key == "B" || key == "Cb") return isMajor ? "6d" : "3m";

    return isMajor ? "1d" : "10m";
}

std::pair<std::string, std::string> KeyDetector::fromCamelot(const std::string& camelot) {
    if (camelot.size() < 2) return {"", ""};
    std::string c = camelot;
    for (char& ch : c) ch = static_cast<char>(std::toupper(static_cast<unsigned char>(ch)));

    if (c == "1A") return {"G#", "minor"};
    if (c == "2A") return {"D#", "minor"};
    if (c == "3A") return {"A#", "minor"};
    if (c == "4A") return {"F", "minor"};
    if (c == "5A") return {"C", "minor"};
    if (c == "6A") return {"G", "minor"};
    if (c == "7A") return {"D", "minor"};
    if (c == "8A") return {"A", "minor"};
    if (c == "9A") return {"E", "minor"};
    if (c == "10A") return {"B", "minor"};
    if (c == "11A") return {"F#", "minor"};
    if (c == "12A") return {"C#", "minor"};

    if (c == "1B") return {"B", "major"};
    if (c == "2B") return {"F#", "major"};
    if (c == "3B") return {"C#", "major"};
    if (c == "4B") return {"G#", "major"};
    if (c == "5B") return {"D#", "major"};
    if (c == "6B") return {"A#", "major"};
    if (c == "7B") return {"F", "major"};
    if (c == "8B") return {"C", "major"};
    if (c == "9B") return {"G", "major"};
    if (c == "10B") return {"D", "major"};
    if (c == "11B") return {"A", "major"};
    if (c == "12B") return {"E", "major"};

    return {"", ""};
}

} // namespace reals::ai
