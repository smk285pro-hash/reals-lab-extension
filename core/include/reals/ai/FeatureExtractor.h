#pragma once

#include <complex>
#include <cstddef>
#include <vector>

namespace reals::ai {

struct SpectrogramConfig {
    int sampleRate = 44100;
    int nFft = 2048;
    int hopLength = 512;
    int nMels = 128;
    float fMin = 20.0f;
    float fMax = 20000.0f;
};

class FeatureExtractor {
public:
    // Mono downmix and resample buffer to target sample rate
    [[nodiscard]] static std::vector<float> resampleMono(
        const float* pcm, size_t frames, int inChannels, int inSampleRate, int outSampleRate);

    // Discrete/Fast Fourier Transform (Radix-2 Cooley-Tukey)
    static void fft(std::vector<std::complex<float>>& x);

    // Apply Hann window in-place
    static void applyHannWindow(std::vector<float>& frame);

    // Short-Time Fourier Transform (magnitude spectrogram: [time_frames x (nFft/2 + 1)])
    [[nodiscard]] static std::vector<std::vector<float>> computeStftMagnitude(
        const std::vector<float>& audio, int nFft, int hopLength);

    // Triangular Mel filterbank matrix: [nMels x (nFft/2 + 1)]
    [[nodiscard]] static std::vector<std::vector<float>> createMelFilterbank(
        int nMels, int nFft, float sampleRate, float fMin, float fMax);

    // Log-Mel Spectrogram matrix: [time_frames x nMels]
    [[nodiscard]] static std::vector<std::vector<float>> computeLogMel(
        const std::vector<float>& audio, const SpectrogramConfig& config);

    // 12-dimensional Chromagram matrix: [time_frames x 12]
    [[nodiscard]] static std::vector<std::vector<float>> computeChromagram(
        const std::vector<float>& audio, int sampleRate, int nFft = 4096, int hopLength = 512);

    // Global 12-dimensional chroma vector averaged and normalized
    [[nodiscard]] static std::vector<float> computeGlobalChroma(
        const std::vector<float>& audio, int sampleRate, int nFft = 4096, int hopLength = 512);

    // Spectral flux / Onset strength novelty curve across time frames
    [[nodiscard]] static std::vector<float> computeOnsetEnvelope(
        const std::vector<float>& audio, int sampleRate, int nFft = 1024, int hopLength = 512);

    // Audio acoustic descriptor metrics
    struct AudioMetrics {
        float rms = 0.0f;
        float peak = 0.0f;
        float spectralCentroid = 0.0f;
        float spectralRolloff = 0.0f;
        float zeroCrossingRate = 0.0f;
        float bassRatio = 0.0f;
        float highRatio = 0.0f;
    };

    [[nodiscard]] static AudioMetrics computeMetrics(
        const std::vector<float>& audio, int sampleRate);

    // Helper: Hz to Mel and Mel to Hz
    [[nodiscard]] static float hzToMel(float hz);
    [[nodiscard]] static float melToHz(float mel);
};

} // namespace reals::ai
