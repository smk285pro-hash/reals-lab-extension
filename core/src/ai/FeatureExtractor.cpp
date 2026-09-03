#include "reals/ai/FeatureExtractor.h"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace reals::ai {

namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr float kTwoPi = 2.0f * kPi;
constexpr float kEpsilon = 1e-7f;

// Frequency to MIDI note number (440Hz = A4 = 69)
inline float freqToMidi(float freq) {
    if (freq <= 0.0f) return 0.0f;
    return 69.0f + 12.0f * std::log2(freq / 440.0f);
}

} // namespace

std::vector<float> FeatureExtractor::resampleMono(
    const float* pcm, size_t frames, int inChannels, int inSampleRate, int outSampleRate) {
    if (!pcm || frames == 0 || inChannels <= 0 || inSampleRate <= 0 || outSampleRate <= 0) {
        return {};
    }

    // Step 1: Downmix to mono
    std::vector<float> mono(frames);
    if (inChannels == 1) {
        std::copy(pcm, pcm + frames, mono.begin());
    } else {
        const float invCh = 1.0f / static_cast<float>(inChannels);
        for (size_t i = 0; i < frames; ++i) {
            float sum = 0.0f;
            for (int ch = 0; ch < inChannels; ++ch) {
                sum += pcm[i * inChannels + ch];
            }
            mono[i] = sum * invCh;
        }
    }

    if (inSampleRate == outSampleRate) {
        return mono;
    }

    // Step 2: Resample via high quality band-limited interpolation / linear interpolation
    const double ratio = static_cast<double>(outSampleRate) / static_cast<double>(inSampleRate);
    const size_t outFrames = static_cast<size_t>(std::ceil(frames * ratio));
    std::vector<float> output(outFrames);

    for (size_t i = 0; i < outFrames; ++i) {
        const double srcPos = static_cast<double>(i) / ratio;
        const size_t idx = static_cast<size_t>(srcPos);
        const double frac = srcPos - idx;

        if (idx + 1 < frames) {
            output[i] = static_cast<float>((1.0 - frac) * mono[idx] + frac * mono[idx + 1]);
        } else if (idx < frames) {
            output[i] = mono[idx];
        } else {
            output[i] = 0.0f;
        }
    }

    return output;
}

void FeatureExtractor::fft(std::vector<std::complex<float>>& x) {
    const size_t n = x.size();
    if (n <= 1) return;

    // Radix-2 Cooley-Tukey only works for power-of-two sizes. A non-power-of-
    // two length drives the butterfly indexing out of bounds (heap overflow,
    // MAJ-04). Bail out leaving the buffer untouched instead of corrupting
    // the heap; every current caller passes 1024/2048 so this is a guard
    // against future misuse only.
    if ((n & (n - 1)) != 0) return;

    // Bit reversal permutation
    size_t j = 0;
    for (size_t i = 0; i < n - 1; ++i) {
        if (i < j) {
            std::swap(x[i], x[j]);
        }
        size_t k = n / 2;
        while (k <= j) {
            j -= k;
            k /= 2;
        }
        j += k;
    }

    // Cooley-Tukey Radix-2 FFT
    for (size_t len = 2; len <= n; len <<= 1) {
        const float angle = -kTwoPi / static_cast<float>(len);
        const std::complex<float> wlen(std::cos(angle), std::sin(angle));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (size_t k = 0; k < len / 2; ++k) {
                const std::complex<float> u = x[i + k];
                const std::complex<float> v = x[i + k + len / 2] * w;
                x[i + k] = u + v;
                x[i + k + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

void FeatureExtractor::applyHannWindow(std::vector<float>& frame) {
    const size_t n = frame.size();
    if (n == 0) return;
    for (size_t i = 0; i < n; ++i) {
        const float w = 0.5f * (1.0f - std::cos(kTwoPi * static_cast<float>(i) / static_cast<float>(n)));
        frame[i] *= w;
    }
}

std::vector<std::vector<float>> FeatureExtractor::computeStftMagnitude(
    const std::vector<float>& audio, int nFft, int hopLength) {
    if (audio.empty() || nFft <= 0 || hopLength <= 0) {
        return {};
    }

    const size_t numBins = static_cast<size_t>(nFft / 2 + 1);
    const size_t numFrames = (audio.size() >= static_cast<size_t>(nFft))
                                 ? ((audio.size() - nFft) / hopLength + 1)
                                 : 0;

    if (numFrames == 0) return {};

    std::vector<std::vector<float>> stft(numFrames, std::vector<float>(numBins, 0.0f));
    std::vector<std::complex<float>> frameComplex(nFft);
    std::vector<float> frame(nFft);

    for (size_t t = 0; t < numFrames; ++t) {
        const size_t start = t * hopLength;
        for (int i = 0; i < nFft; ++i) {
            frame[i] = (start + i < audio.size()) ? audio[start + i] : 0.0f;
        }

        applyHannWindow(frame);

        for (int i = 0; i < nFft; ++i) {
            frameComplex[i] = std::complex<float>(frame[i], 0.0f);
        }

        fft(frameComplex);

        for (size_t k = 0; k < numBins; ++k) {
            stft[t][k] = std::abs(frameComplex[k]);
        }
    }

    return stft;
}

float FeatureExtractor::hzToMel(float hz) {
    return 2595.0f * std::log10(1.0f + hz / 700.0f);
}

float FeatureExtractor::melToHz(float mel) {
    return 700.0f * (std::pow(10.0f, mel / 2595.0f) - 1.0f);
}

std::vector<std::vector<float>> FeatureExtractor::createMelFilterbank(
    int nMels, int nFft, float sampleRate, float fMin, float fMax) {
    if (nMels <= 0 || nFft <= 0 || sampleRate <= 0.0f) return {};

    const int numBins = nFft / 2 + 1;
    const float minMel = hzToMel(fMin);
    const float maxMel = hzToMel(fMax);
    const float melStep = (maxMel - minMel) / static_cast<float>(nMels + 1);

    std::vector<float> melPoints(nMels + 2);
    std::vector<float> binPoints(nMels + 2);

    for (int i = 0; i < nMels + 2; ++i) {
        melPoints[i] = minMel + static_cast<float>(i) * melStep;
        const float hz = melToHz(melPoints[i]);
        binPoints[i] = std::floor((nFft + 1) * hz / sampleRate);
    }

    std::vector<std::vector<float>> filters(nMels, std::vector<float>(numBins, 0.0f));

    for (int m = 1; m <= nMels; ++m) {
        const int left = static_cast<int>(binPoints[m - 1]);
        const int center = static_cast<int>(binPoints[m]);
        const int right = static_cast<int>(binPoints[m + 1]);

        if (center > left) {
            for (int k = left; k < center && k < numBins; ++k) {
                filters[m - 1][k] = static_cast<float>(k - left) / static_cast<float>(center - left);
            }
        }
        if (right > center) {
            for (int k = center; k <= right && k < numBins; ++k) {
                filters[m - 1][k] = static_cast<float>(right - k) / static_cast<float>(right - center);
            }
        }

        // Slaney-style area normalization
        float enorm = 2.0f / (melToHz(melPoints[m + 1]) - melToHz(melPoints[m - 1]) + kEpsilon);
        for (int k = 0; k < numBins; ++k) {
            filters[m - 1][k] *= enorm;
        }
    }

    return filters;
}

std::vector<std::vector<float>> FeatureExtractor::computeLogMel(
    const std::vector<float>& audio, const SpectrogramConfig& config) {
    auto stft = computeStftMagnitude(audio, config.nFft, config.hopLength);
    if (stft.empty()) return {};

    auto filters = createMelFilterbank(
        config.nMels, config.nFft, static_cast<float>(config.sampleRate), config.fMin, config.fMax);

    const size_t numFrames = stft.size();
    const size_t numBins = stft[0].size();
    std::vector<std::vector<float>> logMel(numFrames, std::vector<float>(config.nMels, 0.0f));

    for (size_t t = 0; t < numFrames; ++t) {
        for (int m = 0; m < config.nMels; ++m) {
            float energy = 0.0f;
            for (size_t k = 0; k < numBins; ++k) {
                energy += stft[t][k] * filters[m][k];
            }
            logMel[t][m] = std::log(std::max(energy, kEpsilon));
        }
    }

    return logMel;
}

std::vector<std::vector<float>> FeatureExtractor::computeChromagram(
    const std::vector<float>& audio, int sampleRate, int nFft, int hopLength) {
    auto stft = computeStftMagnitude(audio, nFft, hopLength);
    if (stft.empty()) return {};

    const size_t numFrames = stft.size();
    const size_t numBins = stft[0].size();
    const float binHz = static_cast<float>(sampleRate) / static_cast<float>(nFft);

    std::vector<std::vector<float>> chroma(numFrames, std::vector<float>(12, 0.0f));

    for (size_t t = 0; t < numFrames; ++t) {
        float maxMag = 0.0f;
        for (size_t k = 1; k < numBins; ++k) {
            maxMag = std::max(maxMag, stft[t][k]);
        }
        const float thresh = maxMag * 0.10f;

        // Skip sub-bass below 75 Hz (eliminates 21.5Hz F-major bin trap) and above 3500 Hz
        for (size_t k = 1; k + 1 < numBins; ++k) {
            const float mag = stft[t][k];
            if (mag < thresh) continue;
            if (mag < stft[t][k - 1] || mag < stft[t][k + 1]) continue; // local peak picking

            const float freq = static_cast<float>(k) * binHz;
            if (freq < 75.0f || freq > 3500.0f) continue;

            const float midi = freqToMidi(freq);
            const float nearestNote = std::round(midi);
            const float dist = std::abs(midi - nearestNote);
            if (dist < 0.85f) {
                const int n = static_cast<int>(nearestNote);
                const int pc = (n % 12 + 12) % 12;
                const float weight = 1.0f - dist;
                const float compMag = std::pow(mag, 0.4f);
                chroma[t][pc] += compMag * weight;
            }
        }

        // Normalize each frame to unit L2 or max norm
        float sumSq = 0.0f;
        for (int c = 0; c < 12; ++c) {
            sumSq += chroma[t][c] * chroma[t][c];
        }
        const float norm = std::sqrt(sumSq);
        if (norm > kEpsilon) {
            for (int c = 0; c < 12; ++c) {
                chroma[t][c] /= norm;
            }
        }
    }

    return chroma;
}

std::vector<float> FeatureExtractor::computeGlobalChroma(
    const std::vector<float>& audio, int sampleRate, int nFft, int hopLength) {
    auto chromaMatrix = computeChromagram(audio, sampleRate, nFft, hopLength);
    std::vector<float> globalChroma(12, 0.0f);
    if (chromaMatrix.empty()) return globalChroma;

    for (const auto& frame : chromaMatrix) {
        for (int c = 0; c < 12; ++c) {
            globalChroma[c] += frame[c];
        }
    }

    float maxVal = 0.0f;
    for (int c = 0; c < 12; ++c) {
        maxVal = std::max(maxVal, globalChroma[c]);
    }
    if (maxVal > kEpsilon) {
        for (int c = 0; c < 12; ++c) {
            globalChroma[c] /= maxVal;
        }
    }

    return globalChroma;
}

std::vector<float> FeatureExtractor::computeOnsetEnvelope(
    const std::vector<float>& audio, int /*sampleRate*/, int nFft, int hopLength) {
    auto stft = computeStftMagnitude(audio, nFft, hopLength);
    if (stft.empty() || stft.size() < 2) return {};

    const size_t numFrames = stft.size();
    const size_t numBins = stft[0].size();
    std::vector<float> onset(numFrames, 0.0f);

    // Half-wave rectified spectral flux
    for (size_t t = 1; t < numFrames; ++t) {
        float flux = 0.0f;
        for (size_t k = 0; k < numBins; ++k) {
            const float diff = stft[t][k] - stft[t - 1][k];
            if (diff > 0.0f) {
                flux += diff;
            }
        }
        onset[t] = flux;
    }

    // Normalize envelope
    float maxFlux = 0.0f;
    for (float val : onset) {
        maxFlux = std::max(maxFlux, val);
    }
    if (maxFlux > kEpsilon) {
        for (float& val : onset) {
            val /= maxFlux;
        }
    }

    return onset;
}

FeatureExtractor::AudioMetrics FeatureExtractor::computeMetrics(
    const std::vector<float>& audio, int sampleRate) {
    AudioMetrics m;
    if (audio.empty()) return m;

    float sumSq = 0.0f;
    int zcrCount = 0;

    for (size_t i = 0; i < audio.size(); ++i) {
        const float val = audio[i];
        sumSq += val * val;
        m.peak = std::max(m.peak, std::abs(val));

        if (i > 0 && ((audio[i] >= 0.0f && audio[i - 1] < 0.0f) ||
                      (audio[i] < 0.0f && audio[i - 1] >= 0.0f))) {
            ++zcrCount;
        }
    }

    m.rms = std::sqrt(sumSq / static_cast<float>(audio.size()));
    m.zeroCrossingRate = static_cast<float>(zcrCount) / static_cast<float>(audio.size());

    // Frequency analysis
    const int nFft = 2048;
    const int hopLength = 1024;
    auto stft = computeStftMagnitude(audio, nFft, hopLength);
    if (!stft.empty()) {
        const size_t numBins = stft[0].size();
        const float binHz = static_cast<float>(sampleRate) / static_cast<float>(nFft);

        float totalCentroid = 0.0f;
        float totalBass = 0.0f;
        float totalHigh = 0.0f;
        float totalEnergy = 0.0f;

        for (const auto& frame : stft) {
            float frameWeighted = 0.0f;
            float frameEnergy = 0.0f;
            float frameBass = 0.0f;
            float frameHigh = 0.0f;

            for (size_t k = 0; k < numBins; ++k) {
                const float freq = static_cast<float>(k) * binHz;
                const float mag = frame[k];
                frameWeighted += freq * mag;
                frameEnergy += mag;

                if (freq <= 250.0f) {
                    frameBass += mag;
                } else if (freq >= 4000.0f) {
                    frameHigh += mag;
                }
            }

            if (frameEnergy > kEpsilon) {
                totalCentroid += frameWeighted / frameEnergy;
            }
            totalBass += frameBass;
            totalHigh += frameHigh;
            totalEnergy += frameEnergy;
        }

        const float framesCount = static_cast<float>(stft.size());
        m.spectralCentroid = totalCentroid / framesCount;
        if (totalEnergy > kEpsilon) {
            m.bassRatio = totalBass / totalEnergy;
            m.highRatio = totalHigh / totalEnergy;
        }
        m.spectralRolloff = m.spectralCentroid * 1.5f; // estimate
    }

    return m;
}

} // namespace reals::ai
