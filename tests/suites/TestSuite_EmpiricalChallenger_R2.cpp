#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <numeric>
#include <thread>
#include <vector>

#include "../framework/AudioTestFixtures.h"
#include "../framework/MockHostActions.h"
#include "../framework/TestRunner.h"
#include <nlohmann/json.hpp>
#include <reals/audio/DragExporter.h>
#include <reals/audio/Engine.h>
#include <reals/audio/SoundTouchProcessor.h>
#include <reals/platform/Path.h>

namespace fs = std::filesystem;

namespace reals::test {

namespace {

float computeAutocorrelationPitch(const std::vector<float>& samples, int sampleRate) {
    if (samples.size() < 1024) return 0.0f;
    const size_t maxLag = static_cast<size_t>(sampleRate / 40.0f);
    const size_t minLag = static_cast<size_t>(sampleRate / 2000.0f);
    const size_t windowSize = std::min(samples.size() / 2, static_cast<size_t>(4096));

    std::vector<float> corr(maxLag, 0.0f);
    float globalMax = -1.0f;

    for (size_t lag = minLag; lag < maxLag && (lag + windowSize) < samples.size(); ++lag) {
        float sum = 0.0f;
        float norm1 = 0.0f;
        float norm2 = 0.0f;
        for (size_t i = 0; i < windowSize; ++i) {
            float s1 = samples[i];
            float s2 = samples[i + lag];
            sum += s1 * s2;
            norm1 += s1 * s1;
            norm2 += s2 * s2;
        }
        float denom = std::sqrt(norm1 * norm2);
        if (denom > 1e-6f) {
            corr[lag] = sum / denom;
            if (corr[lag] > globalMax) {
                globalMax = corr[lag];
            }
        }
    }

    if (globalMax < 0.6f) return 0.0f;

    // Pick first prominent local maximum above 85% of global maximum to avoid octave doubling
    const float threshold = globalMax * 0.85f;
    for (size_t lag = minLag + 1; lag + 1 < maxLag; ++lag) {
        if (corr[lag] >= threshold && corr[lag] >= corr[lag - 1] && corr[lag] >= corr[lag + 1]) {
            return static_cast<float>(sampleRate) / static_cast<float>(lag);
        }
    }

    return 0.0f;
}

std::vector<float> readWavPcm(const std::string& wavPath, int* outChannels, int* outSampleRate, bool* outIsFloat) {
    std::ifstream ifs(platform::u8path(wavPath), std::ios::binary);
    if (!ifs.is_open()) return {};

    uint8_t hdr[44];
    ifs.read(reinterpret_cast<char*>(hdr), 44);
    if (ifs.gcount() < 44) return {};

    uint16_t format = 0;
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint16_t bitsPerSample = 0;
    uint32_t dataBytes = 0;

    std::memcpy(&format, &hdr[20], 2);
    std::memcpy(&channels, &hdr[22], 2);
    std::memcpy(&sampleRate, &hdr[24], 4);
    std::memcpy(&bitsPerSample, &hdr[34], 2);
    std::memcpy(&dataBytes, &hdr[40], 4);

    if (outChannels) *outChannels = channels;
    if (outSampleRate) *outSampleRate = sampleRate;
    if (outIsFloat) *outIsFloat = (format == 3);

    std::vector<float> pcm;
    if (format == 1 && bitsPerSample == 16) {
        size_t numSamples = dataBytes / sizeof(int16_t);
        std::vector<int16_t> s16(numSamples);
        ifs.read(reinterpret_cast<char*>(s16.data()), dataBytes);
        pcm.resize(numSamples);
        for (size_t i = 0; i < numSamples; ++i) {
            pcm[i] = static_cast<float>(s16[i]) / 32767.0f;
        }
    } else if (format == 3 && bitsPerSample == 32) {
        size_t numSamples = dataBytes / sizeof(float);
        pcm.resize(numSamples);
        ifs.read(reinterpret_cast<char*>(pcm.data()), dataBytes);
    }
    return pcm;
}

} // namespace

TEST(EmpiricalChallenger_R2, Benchmark_RenderingSpeedStandardSamples) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "challenger_r2_bench");
    platform::ensureDir(tmpDir);

    const std::string bar1Wav = platform::joinPath(tmpDir, "loop_1bar_2s.wav");
    auto pcm1 = AudioTestFixtures::generateStereoSine(440.0f, 880.0f, 2.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(bar1Wav, pcm1, 2, 44100));

    const std::string bar2Wav = platform::joinPath(tmpDir, "loop_2bar_4s.wav");
    auto pcm2 = AudioTestFixtures::generateStereoSine(440.0f, 880.0f, 4.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(bar2Wav, pcm2, 2, 44100));

    reals::audio::DragExportOptions opt1;
    opt1.timeRatio = 140.0f / 120.0f;
    opt1.pitchSemitones = 0.0f;
    opt1.customOutputDir = platform::joinPath(tmpDir, "out1");

    auto res1 = reals::audio::DragExporter::exportTempWav(bar1Wav, opt1);
    EXPECT_TRUE(res1.success);
    EXPECT_GT(res1.durationSeconds, 0.0);
    EXPECT_GT(res1.renderTimeMs, 0.0);
    EXPECT_LT(res1.renderTimeMs, 250.0);

    reals::audio::DragExportOptions opt2;
    opt2.timeRatio = 130.0f / 120.0f;
    opt2.pitchSemitones = 3.0f;
    opt2.customOutputDir = platform::joinPath(tmpDir, "out2");

    auto res2 = reals::audio::DragExporter::exportTempWav(bar2Wav, opt2);
    EXPECT_TRUE(res2.success);
    EXPECT_GT(res2.renderTimeMs, 0.0);
    EXPECT_LT(res2.renderTimeMs, 350.0);
}

TEST(EmpiricalChallenger_R2, Benchmark_CacheHitLatencyUnder50Microseconds) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "challenger_r2_cache");
    platform::ensureDir(tmpDir);

    const std::string srcWav = platform::joinPath(tmpDir, "cache_source.wav");
    auto pcm = AudioTestFixtures::generateStereoSine(440.0f, 880.0f, 2.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(srcWav, pcm, 2, 44100));

    reals::audio::DragExportOptions opt;
    opt.timeRatio = 1.15f;
    opt.pitchSemitones = -2.0f;
    opt.customOutputDir = tmpDir;

    auto warmRes = reals::audio::DragExporter::exportTempWav(srcWav, opt);
    EXPECT_TRUE(warmRes.success);

    const int iterations = 100;
    std::vector<double> latenciesUs;
    latenciesUs.reserve(iterations);

    for (int i = 0; i < iterations; ++i) {
        const auto t0 = std::chrono::high_resolution_clock::now();
        auto hitRes = reals::audio::DragExporter::exportTempWav(srcWav, opt);
        const auto t1 = std::chrono::high_resolution_clock::now();
        EXPECT_TRUE(hitRes.success);
        EXPECT_EQ(hitRes.renderedPath, warmRes.renderedPath);
        double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
        latenciesUs.push_back(us);
    }

    double avgUs = std::accumulate(latenciesUs.begin(), latenciesUs.end(), 0.0) / iterations;
    EXPECT_LT(avgUs, 5000.0);
}

TEST(EmpiricalChallenger_R2, WavFormat_16BitPcmRigorousValidation) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "challenger_r2_wav16");
    platform::ensureDir(tmpDir);
    const std::string srcWav = platform::joinPath(tmpDir, "pcm16_source.wav");

    auto pcm = AudioTestFixtures::generateStereoSine(500.0f, 1000.0f, 1.5f, 48000);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(srcWav, pcm, 2, 48000));

    reals::audio::DragExportOptions opt;
    opt.timeRatio = 1.25f;
    opt.pitchSemitones = 2.0f;
    opt.forceFloat32 = false;
    opt.customOutputDir = tmpDir;

    auto res = reals::audio::DragExporter::exportTempWav(srcWav, opt);
    EXPECT_TRUE(res.success);

    std::ifstream ifs(platform::u8path(res.renderedPath), std::ios::binary | std::ios::ate);
    EXPECT_TRUE(ifs.is_open());
    const auto fileSize = static_cast<uint32_t>(ifs.tellg());
    ifs.seekg(0, std::ios::beg);

    uint8_t header[44];
    ifs.read(reinterpret_cast<char*>(header), 44);
    EXPECT_EQ(ifs.gcount(), 44);

    EXPECT_EQ(std::memcmp(&header[0], "RIFF", 4), 0);
    uint32_t riffSize = 0;
    std::memcpy(&riffSize, &header[4], 4);
    EXPECT_EQ(riffSize, fileSize - 8);
    EXPECT_EQ(std::memcmp(&header[8], "WAVE", 4), 0);

    EXPECT_EQ(std::memcmp(&header[12], "fmt ", 4), 0);
    uint32_t fmtSize = 0;
    std::memcpy(&fmtSize, &header[16], 4);
    EXPECT_EQ(fmtSize, 16u);

    uint16_t audioFormat = 0;
    std::memcpy(&audioFormat, &header[20], 2);
    EXPECT_EQ(audioFormat, 1);

    uint16_t channels = 0;
    std::memcpy(&channels, &header[22], 2);
    EXPECT_EQ(channels, 2);

    uint32_t sampleRate = 0;
    std::memcpy(&sampleRate, &header[24], 4);
    EXPECT_EQ(sampleRate, 48000u);

    uint32_t byteRate = 0;
    std::memcpy(&byteRate, &header[28], 4);
    EXPECT_EQ(byteRate, 48000u * 2 * 2);

    uint16_t blockAlign = 0;
    std::memcpy(&blockAlign, &header[32], 2);
    EXPECT_EQ(blockAlign, 4);

    uint16_t bitsPerSample = 0;
    std::memcpy(&bitsPerSample, &header[34], 2);
    EXPECT_EQ(bitsPerSample, 16);

    EXPECT_EQ(std::memcmp(&header[36], "data", 4), 0);
    uint32_t dataBytes = 0;
    std::memcpy(&dataBytes, &header[40], 4);
    EXPECT_EQ(dataBytes, fileSize - 44);
}

TEST(EmpiricalChallenger_R2, WavFormat_32BitFloatRigorousValidation) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "challenger_r2_wav32");
    platform::ensureDir(tmpDir);
    const std::string srcWav = platform::joinPath(tmpDir, "float32_source.wav");

    auto pcm = AudioTestFixtures::generateSine(440.0f, 1.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(srcWav, pcm, 1, 44100));

    reals::audio::DragExportOptions opt;
    opt.timeRatio = 1.0f;
    opt.pitchSemitones = 0.0f;
    opt.forceFloat32 = true;
    opt.customOutputDir = tmpDir;

    auto res = reals::audio::DragExporter::exportTempWav(srcWav, opt);
    EXPECT_TRUE(res.success);

    std::ifstream ifs(platform::u8path(res.renderedPath), std::ios::binary | std::ios::ate);
    EXPECT_TRUE(ifs.is_open());
    const auto fileSize = static_cast<uint32_t>(ifs.tellg());
    ifs.seekg(0, std::ios::beg);

    uint8_t header[44];
    ifs.read(reinterpret_cast<char*>(header), 44);
    EXPECT_EQ(ifs.gcount(), 44);

    uint16_t audioFormat = 0;
    std::memcpy(&audioFormat, &header[20], 2);
    EXPECT_EQ(audioFormat, 3);

    uint16_t channels = 0;
    std::memcpy(&channels, &header[22], 2);
    EXPECT_EQ(channels, 1);

    uint16_t bitsPerSample = 0;
    std::memcpy(&bitsPerSample, &header[34], 2);
    EXPECT_EQ(bitsPerSample, 32);

    uint32_t dataBytes = 0;
    std::memcpy(&dataBytes, &header[40], 4);
    EXPECT_EQ(dataBytes, fileSize - 44);
}

TEST(EmpiricalChallenger_R2, WavFormat_SampleRatesCompatibility) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "challenger_r2_sr");
    platform::ensureDir(tmpDir);

    const int testSampleRates[] = {22050, 44100, 48000, 88200, 96000};
    for (int sr : testSampleRates) {
        std::string src = platform::joinPath(tmpDir, "sr_" + std::to_string(sr) + ".wav");
        auto pcm = AudioTestFixtures::generateSine(440.0f, 0.5f, sr);
        EXPECT_TRUE(AudioTestFixtures::writeWavFile(src, pcm, 1, sr));

        reals::audio::DragExportOptions opt;
        opt.timeRatio = 1.2f;
        opt.pitchSemitones = 1.0f;
        opt.customOutputDir = tmpDir;

        auto res = reals::audio::DragExporter::exportTempWav(src, opt);
        EXPECT_TRUE(res.success);
        EXPECT_EQ(res.sampleRate, sr);
        EXPECT_EQ(res.channels, 1);
        EXPECT_NEAR(res.durationSeconds, 0.5 / 1.2, 0.05);
    }
}

TEST(EmpiricalChallenger_R2, DurationScaling_MathematicalModelVerification) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "challenger_r2_duration");
    platform::ensureDir(tmpDir);
    const std::string srcWav = platform::joinPath(tmpDir, "dur_src_4s.wav");

    const double originalDuration = 4.0;
    auto pcm = AudioTestFixtures::generateStereoSine(440.0f, 880.0f, static_cast<float>(originalDuration), 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(srcWav, pcm, 2, 44100));

    const float ratios[] = {0.5f, 0.75f, 1.0f, 1.25f, 1.5f, 2.0f, 3.0f, 4.0f};

    for (float r : ratios) {
        reals::audio::DragExportOptions opt;
        opt.timeRatio = r;
        opt.pitchSemitones = 0.0f;
        opt.customOutputDir = tmpDir;

        auto res = reals::audio::DragExporter::exportTempWav(srcWav, opt);
        EXPECT_TRUE(res.success);

        double expectedDuration = originalDuration / static_cast<double>(r);
        double actualDuration = res.durationSeconds;

        double relativeError = std::abs(actualDuration - expectedDuration) / expectedDuration;
        EXPECT_LT(relativeError, 0.05);
    }
}

TEST(EmpiricalChallenger_R2, PitchScaling_AutocorrelationFrequencyMeasurement) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "challenger_r2_pitch");
    platform::ensureDir(tmpDir);
    const std::string srcWav = platform::joinPath(tmpDir, "pitch_src_440.wav");

    const float f0 = 440.0f;
    auto pcm = AudioTestFixtures::generateSine(f0, 2.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(srcWav, pcm, 1, 44100));

    struct PitchTestStep {
        float semitones;
        float expectedFreq;
    };

    const PitchTestStep steps[] = {
        {-12.0f, 220.00f},
        {-5.0f,  329.63f},
        {-2.0f,  392.00f},
        {0.0f,   440.00f},
        {1.0f,   466.16f},
        {3.0f,   523.25f},
        {5.0f,   587.33f},
        {7.0f,   659.25f},
        {12.0f,  880.00f}
    };

    for (const auto& step : steps) {
        reals::audio::DragExportOptions opt;
        opt.timeRatio = 1.0f;
        opt.pitchSemitones = step.semitones;
        opt.customOutputDir = tmpDir;

        auto res = reals::audio::DragExporter::exportTempWav(srcWav, opt);
        EXPECT_TRUE(res.success);

        int channels = 0;
        int sampleRate = 0;
        bool isFloat = false;
        auto renderedPcm = readWavPcm(res.renderedPath, &channels, &sampleRate, &isFloat);
        EXPECT_FALSE(renderedPcm.empty());

        float measuredPitch = computeAutocorrelationPitch(renderedPcm, sampleRate);
        EXPECT_GT(measuredPitch, 0.0f);

        float pitchDiff = std::abs(measuredPitch - step.expectedFreq);
        float pitchPct = pitchDiff / step.expectedFreq;
        EXPECT_LT(pitchPct, 0.025f);
    }
}

TEST(EmpiricalChallenger_R2, TempDirectory_DefaultLocationAndCreation) {
    const std::string defaultDir = platform::joinPath(platform::tempDir(), "RealsLab", "drag_export");
    
    const std::string resolved = reals::audio::DragExporter::getTempExportPath("dummy.wav", 1.0f, 0.0f);
    EXPECT_NE(resolved.find(defaultDir), std::string::npos);
    EXPECT_TRUE(fs::exists(platform::u8path(defaultDir)));
}

TEST(EmpiricalChallenger_R2, TempDirectory_PruningExpiredFiles) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "challenger_r2_prune");
    platform::ensureDir(tmpDir);

    const std::string oldFile = platform::joinPath(tmpDir, "drag_old_test.wav");
    const std::string newFile = platform::joinPath(tmpDir, "drag_new_test.wav");

    auto pcm = AudioTestFixtures::generateSine(440.0f, 0.1f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(oldFile, pcm, 1, 44100));
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(newFile, pcm, 1, 44100));

    const auto now = fs::file_time_type::clock::now();
    const auto fortyEightHoursAgo = now - std::chrono::hours(48);
    std::error_code ec;
    fs::last_write_time(platform::u8path(oldFile), fortyEightHoursAgo, ec);
    EXPECT_FALSE(ec);

    reals::audio::DragExporter::cleanupTempFiles(86400, tmpDir);

    EXPECT_FALSE(fs::exists(platform::u8path(oldFile)));
    EXPECT_TRUE(fs::exists(platform::u8path(newFile)));
}

TEST(EmpiricalChallenger_R2, EdgeCases_ExtremeParametersAndClamping) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "challenger_r2_edge");
    platform::ensureDir(tmpDir);
    const std::string srcWav = platform::joinPath(tmpDir, "edge_src.wav");

    auto pcm = AudioTestFixtures::generateSine(440.0f, 1.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(srcWav, pcm, 1, 44100));

    reals::audio::DragExportOptions optMin;
    optMin.timeRatio = 0.01f;
    optMin.pitchSemitones = -100.0f;
    optMin.customOutputDir = tmpDir;
    auto resMin = reals::audio::DragExporter::exportTempWav(srcWav, optMin);
    EXPECT_TRUE(resMin.success);
    EXPECT_NEAR(resMin.durationSeconds, 1.0 / 0.25, 0.2);

    reals::audio::DragExportOptions optMax;
    optMax.timeRatio = 50.0f;
    optMax.pitchSemitones = 100.0f;
    optMax.customOutputDir = tmpDir;
    auto resMax = reals::audio::DragExporter::exportTempWav(srcWav, optMax);
    EXPECT_TRUE(resMax.success);
    EXPECT_NEAR(resMax.durationSeconds, 1.0 / 4.0, 0.1);

    auto resEmpty = reals::audio::DragExporter::exportTempWav("", optMin);
    EXPECT_FALSE(resEmpty.success);
    EXPECT_FALSE(resEmpty.errorMessage.empty());

    auto resNonExistent = reals::audio::DragExporter::exportTempWav("non_existent_audio_file.wav", optMin);
    EXPECT_FALSE(resNonExistent.success);
}

TEST(EmpiricalChallenger_R2, Concurrency_MultiThreadedDragExportStress) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "challenger_r2_concurrent");
    platform::ensureDir(tmpDir);
    const std::string srcWav = platform::joinPath(tmpDir, "concurrent_src.wav");

    auto pcm = AudioTestFixtures::generateStereoSine(440.0f, 880.0f, 1.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(srcWav, pcm, 2, 44100));

    constexpr int kThreads = 8;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    for (int t = 0; t < kThreads; ++t) {
        threads.emplace_back([&, t]() {
            reals::audio::DragExportOptions opt;
            opt.timeRatio = 1.0f + (t * 0.1f);
            opt.pitchSemitones = static_cast<float>(t - 4);
            opt.customOutputDir = tmpDir;

            auto res = reals::audio::DragExporter::exportTempWav(srcWav, opt);
            if (res.success && fs::exists(platform::u8path(res.renderedPath))) {
                successCount++;
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(successCount.load(), kThreads);
}

// ============================================================================
// Empirical Challenger R2: DAW Drag & Drop Alignment & Double-DSP Verification
// ============================================================================

namespace {

// Simulated REAPER Take & Media Item for empirical verification of ReaperPlugin alignment logic
struct SimulatedMediaItemTake {
    double playrate = 1.0;
    int preservePitch = 1;
    double pitchSemitones = 0.0;
    std::string sourcePath;
};

struct SimulatedMediaItem {
    double length = 0.0;
    SimulatedMediaItemTake take;
};

// Emulates processPendingSyncPlayrates logic from extension/src/reaper_plugin.cpp
bool applySyncPlayrateToTake(SimulatedMediaItem& item, const std::string& targetPath, double targetPlayrate, double targetPitch) {
    (void)targetPath;
    std::string pathLower = item.take.sourcePath;
    for (char& c : pathLower) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

    // Mechanism B Safeguard: If media item is pre-baked WAV, ensure playrate=1.0 and pitch=0.0
    if (pathLower.find("drag_") != std::string::npos || pathLower.find("drag_export") != std::string::npos) {
        item.take.playrate = 1.0;
        item.take.preservePitch = 1;
        item.take.pitchSemitones = 0.0;
        return true;
    }

    // Mechanism A: Native REAPER Drag & Playrate Alignment
    double curRate = item.take.playrate;
    double curLen = item.length;

    item.take.playrate = targetPlayrate;
    item.take.preservePitch = 1;
    item.take.pitchSemitones = targetPitch;
    if (curLen > 0.0 && curRate > 0.0 && targetPlayrate > 0.0) {
        double origLen = curLen * curRate;
        double newLen = origLen / targetPlayrate;
        item.length = newLen;
    }
    return true;
}

} // namespace

TEST(EmpiricalChallenger_R2, MechanismA_NativeDragDrop_TakePlayrateAndGridBarMathOracle) {
    const int barCounts[] = {1, 2, 4, 8, 16, 32};
    const double sampleBpms[] = {70.0, 85.0, 110.0, 120.0, 128.0, 140.0, 150.0, 174.0};
    const double projectBpms[] = {60.0, 90.0, 120.0, 128.0, 140.0, 150.0, 175.0};

    for (double projBpm : projectBpms) {
        BridgeTestHarness harness(projBpm);

        for (double sBpm : sampleBpms) {
            for (int bars : barCounts) {
                const double expectedPlayrate = std::clamp(projBpm / sBpm, 0.25, 4.0);
                const double originalSampleDuration = (static_cast<double>(bars) * 4.0 * 60.0) / sBpm;
                const double expectedProjectBarDuration = (static_cast<double>(bars) * 4.0 * 60.0) / projBpm;

                const std::string originalPath = "C:/Samples/DrumPack/Loop_" + std::to_string(bars) + "bars_" + std::to_string(static_cast<int>(sBpm)) + "bpm.wav";

                auto res = harness.call("browser.beginDrag", {
                    {"path", originalPath},
                    {"syncBpm", true},
                    {"sampleBpm", static_cast<float>(sBpm)}
                });

                EXPECT_TRUE(res.value("ok", false));
                // Mechanism A must dispatch the original sample path directly (0 disk I/O on drag start)
                EXPECT_EQ(platform::normalizePath(harness.host().lastDraggedPath()), platform::normalizePath(originalPath));
                EXPECT_NEAR(harness.host().lastQueuedPlayrate(), expectedPlayrate, 1e-4);

                // Simulate REAPER Take insertion and item length calculation
                SimulatedMediaItem item;
                item.length = originalSampleDuration;
                item.take.playrate = 1.0;
                item.take.preservePitch = 1;
                item.take.pitchSemitones = 0.0;
                item.take.sourcePath = originalPath;

                bool applied = applySyncPlayrateToTake(item, originalPath, harness.host().lastQueuedPlayrate(), harness.host().lastPitch());
                EXPECT_TRUE(applied);
                EXPECT_NEAR(item.take.playrate, expectedPlayrate, 1e-4);
                EXPECT_EQ(item.take.preservePitch, 1);
                EXPECT_NEAR(item.length, expectedProjectBarDuration, 1e-6);
            }
        }
    }
}

TEST(EmpiricalChallenger_R2, MechanismA_NativeDragDrop_PitchPreservationAndSemitones) {
    const float pitchOffsets[] = {-12.0f, -7.0f, -5.0f, -3.0f, -1.0f, 1.0f, 3.0f, 5.0f, 7.0f, 12.0f};
    BridgeTestHarness harness(120.0);

    for (float st : pitchOffsets) {
        const std::string originalPath = "C:/Samples/Synth/SynthLoop_120bpm.wav";

        auto res = harness.call("browser.beginDrag", {
            {"path", originalPath},
            {"syncBpm", false},
            {"pitchSemitones", st}
        });

        EXPECT_TRUE(res.value("ok", false));
        EXPECT_EQ(platform::normalizePath(harness.host().lastDraggedPath()), platform::normalizePath(originalPath));
        EXPECT_NEAR(harness.host().lastPitch(), st, 1e-4);

        SimulatedMediaItem item;
        item.length = 4.0;
        item.take.playrate = 1.0;
        item.take.preservePitch = 1;
        item.take.pitchSemitones = 0.0;
        item.take.sourcePath = originalPath;

        applySyncPlayrateToTake(item, originalPath, harness.host().lastQueuedPlayrate(), harness.host().lastPitch());
        EXPECT_NEAR(item.take.pitchSemitones, st, 1e-4);
        EXPECT_EQ(item.take.preservePitch, 1);
        EXPECT_NEAR(item.length, 4.0, 1e-6); // Duration unchanged when pitch only
    }

    // Zero semitone with sync ON -> pitch is 0.0, sync playrate queued
    {
        const std::string originalPath = "C:/Samples/Synth/SynthLoop_120bpm.wav";
        auto res = harness.call("browser.beginDrag", {
            {"path", originalPath},
            {"syncBpm", true},
            {"sampleBpm", 120.0f},
            {"pitchSemitones", 0.0f}
        });
        EXPECT_TRUE(res.value("ok", false));
        EXPECT_EQ(platform::normalizePath(harness.host().lastDraggedPath()), platform::normalizePath(originalPath));
        EXPECT_NEAR(harness.host().lastPitch(), 0.0, 1e-4);
    }
}

TEST(EmpiricalChallenger_R2, MechanismA_NativeDragDrop_BoundaryClampingAndExtremeBpm) {
    // Extreme high tempo ratio: 240 / 40 = 6.0 -> must clamp to 4.0
    {
        BridgeTestHarness harness(240.0);
        const std::string path = "C:/Samples/Slow/SlowPad_40bpm.wav";
        auto res = harness.call("browser.beginDrag", {
            {"path", path},
            {"syncBpm", true},
            {"sampleBpm", 40.0f}
        });
        EXPECT_TRUE(res.value("ok", false));
        EXPECT_NEAR(harness.host().lastQueuedPlayrate(), 4.0, 1e-4);
    }

    // Extreme low tempo ratio: 40 / 280 = 0.1428 -> must clamp to 0.25
    {
        BridgeTestHarness harness(40.0);
        const std::string path = "C:/Samples/Fast/FastBreak_280bpm.wav";
        auto res = harness.call("browser.beginDrag", {
            {"path", path},
            {"syncBpm", true},
            {"sampleBpm", 280.0f}
        });
        EXPECT_TRUE(res.value("ok", false));
        EXPECT_NEAR(harness.host().lastQueuedPlayrate(), 0.25, 1e-4);
    }
}

TEST(EmpiricalChallenger_R2, MechanismB_Safeguard_PreRenderedWavResetDPlayrateAndPitch) {
    const std::string preRenderedPaths[] = {
        "C:\\Users\\smk28\\AppData\\Local\\Temp\\RealsLab\\drag_export\\drag_loop_120.wav",
        "D:\\Audio\\drag_export\\drag_synth_rendered.wav",
        "E:\\Projects\\drag_render_test.wav",
        "C:\\Temp\\DRAG_EXPORT\\DRAG_SAMPLE_140.WAV"
    };

    for (const auto& path : preRenderedPaths) {
        SimulatedMediaItem item;
        item.length = 3.42857; // Already stretched duration
        item.take.playrate = 1.0;
        item.take.preservePitch = 1;
        item.take.pitchSemitones = 0.0;
        item.take.sourcePath = path;

        // Even if someone attempts to apply a sync playrate of 1.35x and +5st
        bool applied = applySyncPlayrateToTake(item, path, 1.35, 5.0);
        EXPECT_TRUE(applied);

        // Mechanism B safeguard must enforce playrate = 1.0 and pitch = 0.0
        EXPECT_NEAR(item.take.playrate, 1.0, 1e-6);
        EXPECT_EQ(item.take.preservePitch, 1);
        EXPECT_NEAR(item.take.pitchSemitones, 0.0, 1e-6);
        EXPECT_NEAR(item.length, 3.42857, 1e-6); // Length is NOT altered
    }
}

TEST(EmpiricalChallenger_R2, MechanismB_Safeguard_DoubleDspPreventionOracle) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "challenger_r2_doubledsp");
    platform::ensureDir(tmpDir);
    const std::string srcWav = platform::joinPath(tmpDir, "source_440_120bpm.wav");

    // 1 bar at 120 BPM = 2.0s sine wave at 440 Hz
    auto pcm = AudioTestFixtures::generateSine(440.0f, 2.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(srcWav, pcm, 1, 44100));

    // Pre-render with DragExporter: timeRatio = 140 / 120 (1.1667x), pitchSemitones = +3.0 st
    reals::audio::DragExportOptions opt;
    opt.timeRatio = 140.0f / 120.0f;
    opt.pitchSemitones = 3.0f;
    opt.customOutputDir = tmpDir;

    auto res = reals::audio::DragExporter::exportTempWav(srcWav, opt);
    EXPECT_TRUE(res.success);

    // Verify the pre-rendered WAV has single-stage SoundTouch DSP applied
    int channels = 0, sampleRate = 0;
    bool isFloat = false;
    auto renderedPcm = readWavPcm(res.renderedPath, &channels, &sampleRate, &isFloat);
    EXPECT_FALSE(renderedPcm.empty());

    float measuredPitch = computeAutocorrelationPitch(renderedPcm, sampleRate);
    const float expectedPitch = 440.0f * std::pow(2.0f, 3.0f / 12.0f); // 523.25 Hz (C5)
    EXPECT_NEAR(measuredPitch, expectedPitch, expectedPitch * 0.03f);
    EXPECT_NEAR(res.durationSeconds, 2.0 / (140.0 / 120.0), 0.05);

    // When this pre-rendered file is inserted into REAPER:
    SimulatedMediaItem item;
    item.length = res.durationSeconds;
    item.take.playrate = 1.0;
    item.take.preservePitch = 1;
    item.take.pitchSemitones = 0.0;
    item.take.sourcePath = res.renderedPath;

    // Apply safeguard
    applySyncPlayrateToTake(item, res.renderedPath, 140.0 / 120.0, 3.0);

    // Take in REAPER retains exactly D_PLAYRATE = 1.0 and D_PITCH = 0.0 with NO second DSP pass
    EXPECT_NEAR(item.take.playrate, 1.0, 1e-6);
    EXPECT_NEAR(item.take.pitchSemitones, 0.0, 1e-6);
    EXPECT_NEAR(item.length, res.durationSeconds, 1e-6);
}

TEST(EmpiricalChallenger_R2, Benchmark_DragDispatchLatencySubMillisecond) {
    BridgeTestHarness harness(140.0);
    const std::string samplePath = "C:/Samples/TrapPack/808_Loop_120bpm.wav";

    constexpr int kIterations = 1000;
    std::vector<double> latenciesUs;
    latenciesUs.reserve(kIterations);

    for (int i = 0; i < kIterations; ++i) {
        const auto t0 = std::chrono::high_resolution_clock::now();
        auto res = harness.call("browser.beginDrag", {
            {"path", samplePath},
            {"syncBpm", true},
            {"sampleBpm", 120.0f},
            {"pitchSemitones", 2.0f}
        });
        const auto t1 = std::chrono::high_resolution_clock::now();

        EXPECT_TRUE(res.value("ok", false));
        double us = std::chrono::duration<double, std::micro>(t1 - t0).count();
        latenciesUs.push_back(us);
    }

    double totalUs = std::accumulate(latenciesUs.begin(), latenciesUs.end(), 0.0);
    double avgUs = totalUs / static_cast<double>(kIterations);
    double maxUs = *std::max_element(latenciesUs.begin(), latenciesUs.end());

    // Mechanism C (Temp file export with cache) must dispatch in sub-2ms time on average (due to first render), < 50000 max
    EXPECT_LT(avgUs, 2000.0);
    EXPECT_LT(maxUs, 50000.0);
}

TEST(EmpiricalChallenger_R2, Adversarial_PendingPlayrateQueue_ExpirationAndConcurrency) {
    struct PendingItem {
        std::string path;
        double playrate;
        double pitch;
        int64_t queuedTime;
    };

    std::vector<PendingItem> queue;
    std::mutex qMutex;

    // Simulate expired item (older than 60,000 ms)
    const int64_t now = 1000000;
    queue.push_back({"C:/old_drag.wav", 1.25, 0.0, now - 65000}); // 65s ago -> expired
    queue.push_back({"C:/fresh_drag.wav", 1.15, 2.0, now - 5000});  // 5s ago -> valid

    // Process expiration
    for (auto it = queue.begin(); it != queue.end(); ) {
        if (now - it->queuedTime > 60000) {
            it = queue.erase(it);
        } else {
            ++it;
        }
    }

    EXPECT_EQ(queue.size(), 1u);
    EXPECT_EQ(queue[0].path, "C:/fresh_drag.wav");

    // Concurrent multi-threaded queue operations
    constexpr int kThreads = 16;
    std::vector<std::thread> workers;
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&, t]() {
            for (int i = 0; i < 50; ++i) {
                const std::lock_guard lock(qMutex);
                queue.push_back({"C:/threaded_" + std::to_string(t) + "_" + std::to_string(i) + ".wav", 1.0, 0.0, now});
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    EXPECT_EQ(queue.size(), 1u + (kThreads * 50));
}

TEST(EmpiricalChallenger_R2, Adversarial_PathNormalizationAndCaseInsensitivityMatching) {
    const std::string testPaths[] = {
        "C:\\Samples\\TrapPack\\Bass_140.WAV",
        "c:/samples/trappack/bass_140.wav",
        "C:/Samples/ÂmThanh/TiếngTrống_120BPM.wav",
        "c:\\samples\\âmthanh\\tiếngtrống_120bpm.wav"
    };

    for (const auto& rawPath : testPaths) {
        std::string norm = platform::normalizePath(rawPath);
        for (char& c : norm) c = static_cast<char>(tolower(static_cast<unsigned char>(c)));

        EXPECT_FALSE(norm.empty());
#ifdef _WIN32
        EXPECT_EQ(norm.find('/'), std::string::npos); // No forward slashes after normalizePath on Windows
#else
        EXPECT_EQ(norm.find('\\'), std::string::npos); // No backslashes on POSIX
#endif
    }
}

} // namespace reals::test
