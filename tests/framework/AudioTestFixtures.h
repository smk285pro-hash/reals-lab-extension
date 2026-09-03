#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <random>
#include <string>
#include <vector>

#include <reals/platform/Path.h>

namespace reals::test {

namespace fs = std::filesystem;

enum class WavCorruptionType {
    None,
    ZeroByte,
    TruncatedRiffHeader,
    CorruptedFmtChunkSize,
    InvalidChannelCount,
    CorruptedBitsPerSample,
    TruncatedDataChunk,
    TrailingGarbageBytes
};

class AudioTestFixtures {
public:
    static constexpr double kPi = 3.14159265358979323846;

    // Generate pure sine wave in float [-1.0, 1.0]
    static std::vector<float> generateSine(float freqHz, float durationSec, int sampleRate = 44100, float amplitude = 0.8f) {
        const size_t totalFrames = static_cast<size_t>(durationSec * sampleRate);
        std::vector<float> buffer(totalFrames);
        for (size_t i = 0; i < totalFrames; ++i) {
            double t = static_cast<double>(i) / sampleRate;
            buffer[i] = amplitude * static_cast<float>(std::sin(2.0 * kPi * freqHz * t));
        }
        return buffer;
    }

    // Generate interleaved stereo sine wave
    static std::vector<float> generateStereoSine(float leftFreq, float rightFreq, float durationSec, int sampleRate = 44100, float amplitude = 0.8f) {
        const size_t totalFrames = static_cast<size_t>(durationSec * sampleRate);
        std::vector<float> buffer(totalFrames * 2);
        for (size_t i = 0; i < totalFrames; ++i) {
            double t = static_cast<double>(i) / sampleRate;
            buffer[i * 2 + 0] = amplitude * static_cast<float>(std::sin(2.0 * kPi * leftFreq * t));
            buffer[i * 2 + 1] = amplitude * static_cast<float>(std::sin(2.0 * kPi * rightFreq * t));
        }
        return buffer;
    }

    // Generate rhythmic kick clicks at exact BPM intervals
    static std::vector<float> generateKickRhythm(float bpm, float durationSec, int sampleRate = 44100) {
        const size_t totalFrames = static_cast<size_t>(durationSec * sampleRate);
        std::vector<float> buffer(totalFrames, 0.0f);

        double secondsPerBeat = 60.0 / bpm;
        size_t framesPerBeat = static_cast<size_t>(secondsPerBeat * sampleRate);

        for (size_t beatStart = 0; beatStart < totalFrames; beatStart += framesPerBeat) {
            // Synthesize 50ms pitch-dropping kick transient
            size_t kickFrames = std::min(static_cast<size_t>(0.05 * sampleRate), totalFrames - beatStart);
            for (size_t i = 0; i < kickFrames; ++i) {
                double t = static_cast<double>(i) / sampleRate;
                double freq = 150.0 * std::exp(-t * 50.0) + 45.0;
                double env = std::exp(-t * 30.0);
                buffer[beatStart + i] = static_cast<float>(env * std::sin(2.0 * kPi * freq * t));
            }
        }
        return buffer;
    }

    // Generate polyphonic triad chords (e.g. C Major: 261.63Hz + 329.63Hz + 392.00Hz)
    static std::vector<float> generateChordTriad(float rootHz, bool isMinor, float durationSec, int sampleRate = 44100, float amplitude = 0.7f) {
        const size_t totalFrames = static_cast<size_t>(durationSec * sampleRate);
        std::vector<float> buffer(totalFrames, 0.0f);

        float thirdInterval = isMinor ? std::pow(2.0f, 3.0f / 12.0f) : std::pow(2.0f, 4.0f / 12.0f);
        float fifthInterval = std::pow(2.0f, 7.0f / 12.0f);

        float fRoot = rootHz;
        float fThird = rootHz * thirdInterval;
        float fFifth = rootHz * fifthInterval;

        for (size_t i = 0; i < totalFrames; ++i) {
            double t = static_cast<double>(i) / sampleRate;
            double sample = (std::sin(2.0 * kPi * fRoot * t) +
                             std::sin(2.0 * kPi * fThird * t) +
                             std::sin(2.0 * kPi * fFifth * t)) / 3.0;
            buffer[i] = static_cast<float>(amplitude * sample);
        }
        return buffer;
    }

    // Generate digital silence
    static std::vector<float> generateSilent(float durationSec, int sampleRate = 44100) {
        const size_t totalFrames = static_cast<size_t>(durationSec * sampleRate);
        return std::vector<float>(totalFrames, 0.0f);
    }

    // Generate DC offset signal
    static std::vector<float> generateDcOffset(float durationSec, int sampleRate = 44100, float offset = 0.5f) {
        const size_t totalFrames = static_cast<size_t>(durationSec * sampleRate);
        return std::vector<float>(totalFrames, offset);
    }

    // Generate White Noise
    static std::vector<float> generateNoise(float durationSec, int sampleRate = 44100, float amplitude = 0.5f) {
        const size_t totalFrames = static_cast<size_t>(durationSec * sampleRate);
        std::vector<float> buffer(totalFrames);
        std::mt19937 rng(42); // Deterministic seed
        std::uniform_real_distribution<float> dist(-amplitude, amplitude);
        for (size_t i = 0; i < totalFrames; ++i) {
            buffer[i] = dist(rng);
        }
        return buffer;
    }

    // Write standard 16-bit PCM or 32-bit Float RIFF WAV to memory buffer
    static std::vector<uint8_t> encodeWavMemory(
        const std::vector<float>& pcm, int channels, int sampleRate, bool float32 = false) {

        std::vector<uint8_t> out;
        const size_t totalFrames = pcm.size() / channels;
        const uint16_t bitsPerSample = float32 ? 32 : 16;
        const uint16_t blockAlign = static_cast<uint16_t>(channels * (bitsPerSample / 8));
        const uint32_t byteRate = sampleRate * blockAlign;
        const uint32_t dataBytes = static_cast<uint32_t>(totalFrames * blockAlign);
        const uint32_t riffChunkSize = 36 + dataBytes;

        out.resize(44 + dataBytes);

        // RIFF Header
        std::memcpy(&out[0], "RIFF", 4);
        std::memcpy(&out[4], &riffChunkSize, 4);
        std::memcpy(&out[8], "WAVE", 4);

        // fmt chunk
        std::memcpy(&out[12], "fmt ", 4);
        uint32_t fmtSize = 16;
        std::memcpy(&out[16], &fmtSize, 4);
        uint16_t audioFormat = float32 ? 3 : 1; // 1 = PCM, 3 = IEEE Float
        std::memcpy(&out[20], &audioFormat, 2);
        uint16_t numChannels = static_cast<uint16_t>(channels);
        std::memcpy(&out[22], &numChannels, 2);
        uint32_t sampleRateU32 = static_cast<uint32_t>(sampleRate);
        std::memcpy(&out[24], &sampleRateU32, 4);
        std::memcpy(&out[28], &byteRate, 4);
        std::memcpy(&out[32], &blockAlign, 2);
        std::memcpy(&out[34], &bitsPerSample, 2);

        // data chunk
        std::memcpy(&out[36], "data", 4);
        std::memcpy(&out[40], &dataBytes, 4);

        // Sample payload
        uint8_t* payload = &out[44];
        if (float32) {
            std::memcpy(payload, pcm.data(), pcm.size() * sizeof(float));
        } else {
            int16_t* pcm16 = reinterpret_cast<int16_t*>(payload);
            for (size_t i = 0; i < pcm.size(); ++i) {
                float clamped = std::clamp(pcm[i], -1.0f, 1.0f);
                pcm16[i] = static_cast<int16_t>(clamped * 32767.0f);
            }
        }

        return out;
    }

    // Save WAV to disk file
    static bool writeWavFile(
        const std::string& filePath, const std::vector<float>& pcm, int channels, int sampleRate, bool float32 = false) {
        auto mem = encodeWavMemory(pcm, channels, sampleRate, float32);
        auto p = platform::u8path(filePath);
        if (p.has_parent_path() && !p.parent_path().empty()) {
            fs::create_directories(p.parent_path());
        }
        std::ofstream ofs(p, std::ios::binary);
        if (!ofs) return false;
        ofs.write(reinterpret_cast<const char*>(mem.data()), mem.size());
        return ofs.good();
    }

    // Generate corrupted WAV file on disk for Tier 2 boundary testing
    static bool writeCorruptedWavFile(const std::string& filePath, WavCorruptionType type) {
        auto p = platform::u8path(filePath);
        if (p.has_parent_path() && !p.parent_path().empty()) {
            fs::create_directories(p.parent_path());
        }
        std::ofstream ofs(p, std::ios::binary);
        if (!ofs) return false;

        auto validMem = encodeWavMemory(generateSine(440.0f, 0.5f), 1, 44100, false);

        switch (type) {
            case WavCorruptionType::ZeroByte:
                // 0 byte file
                break;
            case WavCorruptionType::TruncatedRiffHeader:
                // Write only 12 bytes of header
                ofs.write(reinterpret_cast<const char*>(validMem.data()), 12);
                break;
            case WavCorruptionType::CorruptedFmtChunkSize:
                // Corrupt fmt chunk size to 0xFFFFFFFF
                {
                    uint32_t badSize = 0xFFFFFFFF;
                    std::memcpy(&validMem[16], &badSize, 4);
                    ofs.write(reinterpret_cast<const char*>(validMem.data()), validMem.size());
                }
                break;
            case WavCorruptionType::InvalidChannelCount:
                // Set channels to 0
                {
                    uint16_t zeroChannels = 0;
                    std::memcpy(&validMem[22], &zeroChannels, 2);
                    ofs.write(reinterpret_cast<const char*>(validMem.data()), validMem.size());
                }
                break;
            case WavCorruptionType::CorruptedBitsPerSample:
                // Set bits per sample to 7
                {
                    uint16_t badBits = 7;
                    std::memcpy(&validMem[34], &badBits, 2);
                    ofs.write(reinterpret_cast<const char*>(validMem.data()), validMem.size());
                }
                break;
            case WavCorruptionType::TruncatedDataChunk:
                // Cut file in the middle of data chunk
                ofs.write(reinterpret_cast<const char*>(validMem.data()), 50);
                break;
            case WavCorruptionType::TrailingGarbageBytes:
                // Add 1024 bytes of garbage after valid WAV
                ofs.write(reinterpret_cast<const char*>(validMem.data()), validMem.size());
                {
                    std::vector<char> garbage(1024, static_cast<char>(0xAA));
                    ofs.write(garbage.data(), garbage.size());
                }
                break;
            default:
                ofs.write(reinterpret_cast<const char*>(validMem.data()), validMem.size());
                break;
        }

        return ofs.good();
    }

    // Estimate dominant frequency of a buffer using normalized autocorrelation and parabolic peak refinement
    static float estimateFundamentalFrequency(const std::vector<float>& pcm, int sampleRate) {
        if (pcm.size() < 1024 || sampleRate <= 0) return 0.0f;

        const size_t N = std::min(pcm.size(), size_t(4096));
        const size_t minLag = sampleRate / 2000; // max 2000Hz
        const size_t maxLag = std::min(static_cast<size_t>(sampleRate / 40), N / 2); // min 40Hz

        std::vector<float> r(maxLag, 0.0f);
        float maxCorr = 0.0f;

        for (size_t lag = minLag; lag < maxLag; ++lag) {
            float num = 0.0f;
            float den0 = 0.0f;
            float denLag = 0.0f;
            for (size_t i = 0; i < N / 2; ++i) {
                float x0 = pcm[i];
                float xLag = pcm[i + lag];
                num += x0 * xLag;
                den0 += x0 * x0;
                denLag += xLag * xLag;
            }
            float den = std::sqrt(den0) * std::sqrt(denLag);
            if (den > 1e-7f) {
                r[lag] = num / den;
                if (r[lag] > maxCorr) {
                    maxCorr = r[lag];
                }
            }
        }

        if (maxCorr < 0.3f) return 0.0f;

        // Pick the first prominent peak (lowest lag) to avoid subharmonic errors
        size_t bestLag = 0;
        const float threshold = std::max(0.6f, maxCorr * 0.85f);
        for (size_t lag = minLag + 1; lag < maxLag - 1; ++lag) {
            if (r[lag] >= threshold && r[lag] >= r[lag - 1] && r[lag] >= r[lag + 1]) {
                bestLag = lag;
                break;
            }
        }

        if (bestLag == 0) {
            for (size_t lag = minLag; lag < maxLag; ++lag) {
                if (r[lag] == maxCorr) {
                    bestLag = lag;
                    break;
                }
            }
        }

        if (bestLag == 0) return 0.0f;

        // Parabolic interpolation for exact peak refinement
        float alpha = (bestLag > 0) ? r[bestLag - 1] : r[bestLag];
        float beta = r[bestLag];
        float gamma = (bestLag + 1 < maxLag) ? r[bestLag + 1] : r[bestLag];
        float delta = 0.0f;
        float denom = (alpha - 2.0f * beta + gamma);
        if (std::abs(denom) > 1e-7f) {
            delta = 0.5f * (alpha - gamma) / denom;
        }

        double refinedLag = static_cast<double>(bestLag) + std::clamp(delta, -0.5f, 0.5f);
        return static_cast<float>(static_cast<double>(sampleRate) / refinedLag);
    }
};

} // namespace reals::test
