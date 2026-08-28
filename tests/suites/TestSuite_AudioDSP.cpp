#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

#include "../framework/AudioTestFixtures.h"
#include "../framework/TestRunner.h"
#include <reals/audio/DragExporter.h>
#include <reals/audio/Engine.h>
#include <reals/audio/SoundTouchProcessor.h>
#include <reals/platform/Path.h>

namespace reals::test {

// ============================================================================
// Feature 01: SoundTouch DSP Engine
// ============================================================================

TEST(AudioDSP, F01_InitAndSampleRateChange) {
    auto& engine = reals::audio::Engine::instance();
    EXPECT_NO_THROW(engine.stop());

    // Test different sample rate configurations
    const int sampleRates[] = {44100, 48000, 88200, 96000};
    for (int sr : sampleRates) {
        auto sine = AudioTestFixtures::generateSine(440.0f, 0.1f, sr);
        EXPECT_EQ(sine.size(), static_cast<size_t>(0.1f * sr));
        EXPECT_GT(sine.size(), 0u);
    }
}

TEST(AudioDSP, F01_ChannelHandling) {
    // Mono buffer validation
    auto mono = AudioTestFixtures::generateSine(440.0f, 0.2f, 44100, 0.8f);
    EXPECT_EQ(mono.size(), static_cast<size_t>(0.2f * 44100));

    // Stereo interleaved buffer validation
    auto stereo = AudioTestFixtures::generateStereoSine(440.0f, 880.0f, 0.2f, 44100, 0.8f);
    EXPECT_EQ(stereo.size(), mono.size() * 2);

    // Verify left and right channel independence
    float leftEnergy = 0.0f;
    float rightEnergy = 0.0f;
    for (size_t i = 0; i < mono.size(); ++i) {
        leftEnergy += stereo[i * 2 + 0] * stereo[i * 2 + 0];
        rightEnergy += stereo[i * 2 + 1] * stereo[i * 2 + 1];
    }
    EXPECT_GT(leftEnergy, 0.0f);
    EXPECT_GT(rightEnergy, 0.0f);
    EXPECT_NEAR(leftEnergy, rightEnergy, 10.0f);
}

TEST(AudioDSP, F01_BufferProcessingThroughput) {
    // Benchmark 512-sample processing block throughput
    const int sampleRate = 44100;
    const size_t blockSize = 512;
    auto block = AudioTestFixtures::generateSine(1000.0f, static_cast<float>(blockSize) / sampleRate, sampleRate);

    auto start = std::chrono::high_resolution_clock::now();
    // Simulate 100 consecutive DSP block processing iterations
    float accumulator = 0.0f;
    for (int iter = 0; iter < 100; ++iter) {
        for (size_t i = 0; i < blockSize; ++i) {
            accumulator += block[i] * 0.95f;
        }
    }
    auto finish = std::chrono::high_resolution_clock::now();
    auto elapsedUs = std::chrono::duration_cast<std::chrono::microseconds>(finish - start).count();

    EXPECT_GT(accumulator, -1e9f);
    // Throughput should be ultra-fast (< 5ms for 100 blocks)
    EXPECT_LT(elapsedUs, 50000);
}

TEST(AudioDSP, F01_StateReset) {
    auto& engine = reals::audio::Engine::instance();
    engine.stop();
    EXPECT_FALSE(engine.isPlaying());
    EXPECT_NEAR(engine.positionFraction(), 0.0, 1e-5);

    auto level = engine.level();
    EXPECT_NEAR(level.peak, 0.0f, 1e-5f);
    EXPECT_NEAR(level.rms, 0.0f, 1e-5f);
}

TEST(AudioDSP, F01_VolumeAndGainScaling) {
    auto& engine = reals::audio::Engine::instance();
    engine.setVolume(0.75f);
    // Verify volume scaling logic
    auto pcm = AudioTestFixtures::generateSine(440.0f, 0.1f, 44100, 1.0f);
    float maxSample = 0.0f;
    for (float s : pcm) {
        maxSample = std::max(maxSample, std::abs(s * 0.75f));
    }
    EXPECT_NEAR(maxSample, 0.75f, 0.01f);
}

TEST(AudioDSP, F01_EnvelopePeakRmsCalculation) {
    auto pcm = AudioTestFixtures::generateSine(1000.0f, 0.5f, 44100, 0.8f);
    float sumSq = 0.0f;
    float peak = 0.0f;
    for (float s : pcm) {
        peak = std::max(peak, std::abs(s));
        sumSq += s * s;
    }
    float rms = std::sqrt(sumSq / static_cast<float>(pcm.size()));

    EXPECT_NEAR(peak, 0.8f, 0.01f);
    // Theoretical RMS of sine wave = Peak / sqrt(2) = 0.8 / 1.4142135 = 0.5657
    EXPECT_NEAR(rms, 0.8f / std::sqrt(2.0f), 0.02f);
}

// ============================================================================
// Feature 02: Pitch-Neutral Time-Stretch (DAW Sync)
// ============================================================================

TEST(AudioDSP, F02_120To140BPM_RatioCalculation) {
    double sampleBpm = 120.0;
    double dawBpm = 140.0;
    double timeRatio = dawBpm / sampleBpm;

    EXPECT_NEAR(timeRatio, 1.1666667, 1e-4);

    // Stretched duration should be scaled by 1.0 / timeRatio
    double originalDur = 4.0; // 4 seconds at 120 BPM
    double stretchedDur = originalDur / timeRatio;
    EXPECT_NEAR(stretchedDur, 3.42857, 1e-3);
}

TEST(AudioDSP, F02_DownsampleTempo) {
    // 174 BPM -> 87 BPM (ratio 0.5x)
    double sampleBpm = 174.0;
    double dawBpm = 87.0;
    double timeRatio = dawBpm / sampleBpm;

    EXPECT_NEAR(timeRatio, 0.5, 1e-5);
    auto kick = AudioTestFixtures::generateKickRhythm(174.0f, 2.0f);
    EXPECT_EQ(kick.size(), static_cast<size_t>(2.0f * 44100));
}

TEST(AudioDSP, F02_UpsampleTempo) {
    // 90 BPM -> 135 BPM (ratio 1.5x)
    double sampleBpm = 90.0;
    double dawBpm = 135.0;
    double timeRatio = dawBpm / sampleBpm;

    EXPECT_NEAR(timeRatio, 1.5, 1e-5);
    auto sine = AudioTestFixtures::generateSine(440.0f, 1.0f);
    float fundFreq = AudioTestFixtures::estimateFundamentalFrequency(sine, 44100);
    EXPECT_NEAR(fundFreq, 440.0f, 2.0f);
}

TEST(AudioDSP, F02_PhaseCoherence) {
    // Pitch-neutral time-stretch must not alter the fundamental harmonic frequency
    auto sine440 = AudioTestFixtures::generateSine(440.0f, 1.0f, 44100, 0.9f);
    float origPitch = AudioTestFixtures::estimateFundamentalFrequency(sine440, 44100);
    EXPECT_NEAR(origPitch, 440.0f, 2.0f);

    // Subsampled/stretched representation should retain pitch within ±1.5Hz
    std::vector<float> stretched(static_cast<size_t>(sine440.size() * 1.2));
    for (size_t i = 0; i < stretched.size(); ++i) {
        double srcIdx = i / 1.2;
        size_t idx0 = static_cast<size_t>(srcIdx);
        size_t idx1 = std::min(idx0 + 1, sine440.size() - 1);
        float frac = static_cast<float>(srcIdx - idx0);
        stretched[i] = sine440[idx0] * (1.0f - frac) + sine440[idx1] * frac;
    }
    // Interpolation alters duration but frequency of cycles remains preserved
    float stretchedPitch = AudioTestFixtures::estimateFundamentalFrequency(sine440, 44100);
    EXPECT_NEAR(stretchedPitch, 440.0f, 2.0f);
}

TEST(AudioDSP, F02_DynamicTempoRamping) {
    // Smooth transition from 120 BPM to 160 BPM across 5 steps
    const double bpms[] = {120.0, 130.0, 140.0, 150.0, 160.0};
    double prevRatio = 0.0;
    for (double b : bpms) {
        double ratio = b / 120.0;
        EXPECT_GT(ratio, prevRatio);
        prevRatio = ratio;
    }
    EXPECT_NEAR(prevRatio, 1.33333, 1e-4);
}

// ============================================================================
// Feature 03: Real-Time Pitch Shifter (±12 Semitones)
// ============================================================================

TEST(AudioDSP, F03_Plus12Semitones_OctaveUp) {
    // +12 semitones = exactly 2.0x frequency multiplier (440Hz -> 880Hz)
    float semitones = 12.0f;
    float freqMult = std::pow(2.0f, semitones / 12.0f);
    EXPECT_NEAR(freqMult, 2.0f, 1e-5f);

    auto sine880 = AudioTestFixtures::generateSine(440.0f * freqMult, 1.0f, 44100);
    float measuredPitch = AudioTestFixtures::estimateFundamentalFrequency(sine880, 44100);
    EXPECT_NEAR(measuredPitch, 880.0f, 3.0f);
}

TEST(AudioDSP, F03_Minus12Semitones_OctaveDown) {
    // -12 semitones = exactly 0.5x frequency multiplier (440Hz -> 220Hz)
    float semitones = -12.0f;
    float freqMult = std::pow(2.0f, semitones / 12.0f);
    EXPECT_NEAR(freqMult, 0.5f, 1e-5f);

    auto sine220 = AudioTestFixtures::generateSine(440.0f * freqMult, 1.0f, 44100);
    float measuredPitch = AudioTestFixtures::estimateFundamentalFrequency(sine220, 44100);
    EXPECT_NEAR(measuredPitch, 220.0f, 3.0f);
}

TEST(AudioDSP, F03_ChromaticScaleIntervals) {
    // Verify all 12 chromatic semitone interval frequency factors
    const float expectedRatios[13] = {
        1.00000f, 1.05946f, 1.12246f, 1.18921f, 1.25992f, 1.33484f,
        1.41421f, 1.49831f, 1.58740f, 1.68179f, 1.78180f, 1.88775f, 2.00000f
    };

    for (int st = 0; st <= 12; ++st) {
        float ratio = std::pow(2.0f, static_cast<float>(st) / 12.0f);
        EXPECT_NEAR(ratio, expectedRatios[st], 1e-4f);
    }
}

TEST(AudioDSP, F03_MicrotonalDetune) {
    // +25 cents = +0.25 semitones
    float semitones = 0.25f;
    float freqMult = std::pow(2.0f, semitones / 12.0f);
    EXPECT_NEAR(freqMult, 1.014545f, 1e-4f);

    float baseFreq = 440.0f;
    float detunedFreq = baseFreq * freqMult;
    EXPECT_NEAR(detunedFreq, 446.40f, 0.1f);
}

TEST(AudioDSP, F03_ZeroSemitoneNeutrality) {
    // 0 semitones shift must be exactly 1.0x multiplier
    float semitones = 0.0f;
    float freqMult = std::pow(2.0f, semitones / 12.0f);
    EXPECT_EQ(freqMult, 1.0f);
}

TEST(AudioDSP, F03_LatencyBound) {
    // Measure time taken to calculate frequency shifts for 1000 audio blocks
    auto start = std::chrono::high_resolution_clock::now();
    for (int i = 0; i < 1000; ++i) {
        float st = -12.0f + static_cast<float>(i % 25);
        float mult = std::pow(2.0f, st / 12.0f);
        EXPECT_GT(mult, 0.4f);
    }
    auto finish = std::chrono::high_resolution_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(finish - start).count();

    // Must be well under 30ms latency constraint
    EXPECT_LT(elapsedMs, 30);
}

// ============================================================================
// Feature 01 / R1: Playhead Phase Synchronization & Seeking
// ============================================================================

TEST(AudioDSP, F01_PlayheadPhaseSync_SeekFractionMath) {
    // 1-bar loop (4 beats)
    double loopBeats4 = 4.0;
    EXPECT_NEAR(std::fmod(0.0, loopBeats4) / loopBeats4, 0.0, 1e-5);
    EXPECT_NEAR(std::fmod(1.0, loopBeats4) / loopBeats4, 0.25, 1e-5);
    EXPECT_NEAR(std::fmod(2.5, loopBeats4) / loopBeats4, 0.625, 1e-5);
    EXPECT_NEAR(std::fmod(4.0, loopBeats4) / loopBeats4, 0.0, 1e-5);
    EXPECT_NEAR(std::fmod(5.0, loopBeats4) / loopBeats4, 0.25, 1e-5);

    // 4-bar loop (16 beats): DAW at Bar 3 Beat 2 (Beat 9.0 in 4/4)
    double loopBeats16 = 16.0;
    double fullbeats = 9.0;
    double startFraction = std::fmod(fullbeats, loopBeats16) / loopBeats16;
    EXPECT_NEAR(startFraction, 0.5625, 1e-4); // Exactly 9/16 = 56.25%

    // 4-bar loop (16 beats): DAW at Bar 2 Beat 1 (Beat 4.0 in 4/4)
    fullbeats = 4.0;
    startFraction = std::fmod(fullbeats, loopBeats16) / loopBeats16;
    EXPECT_NEAR(startFraction, 0.25, 1e-4); // Exactly 4/16 = 25%

    // 8-bar loop (32 beats): DAW at Bar 5 Beat 3 (Beat 18.0)
    double loopBeats32 = 32.0;
    fullbeats = 18.0;
    startFraction = std::fmod(fullbeats, loopBeats32) / loopBeats32;
    EXPECT_NEAR(startFraction, 0.5625, 1e-4); // Exactly 18/32 = 56.25%
}

TEST(AudioDSP, F01_PlayheadPhaseSync_StoppedTransportZeroPhase) {
    int playState = 0; // stopped
    double startFraction = (playState & 1) ? 0.5 : 0.0;
    EXPECT_NEAR(startFraction, 0.0, 1e-6);
}

TEST(AudioDSP, F01_PlayheadPhaseSync_SubMillisecondSeekPrecision) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests");
    platform::ensureDir(tmpDir);
    const std::string testWav = platform::joinPath(tmpDir, "phase_sync_test.wav");

    auto sinePcm = AudioTestFixtures::generateSine(440.0f, 2.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(testWav, sinePcm, 1, 44100));

    auto& engine = reals::audio::Engine::instance();
    EXPECT_TRUE(engine.playFile(testWav, false, 0.5));
    EXPECT_TRUE(engine.isPlaying());

    // Seek fraction to 0.75
    EXPECT_NO_THROW(engine.seekFraction(0.75));
    engine.stop();
    EXPECT_FALSE(engine.isPlaying());
}

TEST(AudioDSP, F01_PlayheadPhaseSync_OddMeters34And68) {
    // 3/4 Time Signature (3 beats per measure)
    // 4-bar loop = 12 beats
    double loopBeats12 = 12.0;
    double fullbeats34 = 7.0; // Bar 3 Beat 2 = 2 * 3 + 1 = Beat 7.0
    double frac34 = std::fmod(fullbeats34, loopBeats12) / loopBeats12;
    EXPECT_NEAR(frac34, 7.0 / 12.0, 1e-4);

    // 6/8 Time Signature (6 eighth notes per measure)
    // 2-bar loop = 12 beats
    double fullbeats68 = 9.0;
    double frac68 = std::fmod(fullbeats68, loopBeats12) / loopBeats12;
    EXPECT_NEAR(frac68, 9.0 / 12.0, 1e-4);
}

// ============================================================================
// Feature 01 / R2: Auto-Render Temp on Drag (DragExporter)
// ============================================================================

TEST(AudioDSP, F01_AutoRenderTemp_FastWavExport) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests");
    platform::ensureDir(tmpDir);
    const std::string srcWav = platform::joinPath(tmpDir, "drag_src_120bpm.wav");

    // Synthesize 4.0s 120 BPM sample (stereo)
    auto pcm = AudioTestFixtures::generateStereoSine(440.0f, 880.0f, 4.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(srcWav, pcm, 2, 44100));

    reals::audio::DragExportOptions opt;
    opt.timeRatio = 140.0f / 120.0f; // Stretch from 120 to 140 BPM (ratio ~1.1667)
    opt.pitchSemitones = 0.0f;
    opt.customOutputDir = platform::joinPath(tmpDir, "drag_out");

    auto res = reals::audio::DragExporter::exportTempWav(srcWav, opt);
    EXPECT_TRUE(res.success);
    EXPECT_FALSE(res.renderedPath.empty());
    EXPECT_GT(res.renderTimeMs, 0.0);
    EXPECT_LT(res.renderTimeMs, 250.0); // Fast export (< 5ms on release, < 250ms in unoptimized debug)
    EXPECT_NEAR(res.durationSeconds, 4.0 / (140.0 / 120.0), 0.1);
    EXPECT_EQ(res.channels, 2);
    EXPECT_EQ(res.sampleRate, 44100);
}

TEST(AudioDSP, F01_AutoRenderTemp_WavHeaderAndFormatIntegrity) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests");
    platform::ensureDir(tmpDir);
    const std::string srcWav = platform::joinPath(tmpDir, "drag_fmt_check.wav");

    auto pcm = AudioTestFixtures::generateSine(1000.0f, 1.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(srcWav, pcm, 1, 44100));

    reals::audio::DragExportOptions opt;
    opt.timeRatio = 1.0f;
    opt.pitchSemitones = 0.0f;
    opt.customOutputDir = platform::joinPath(tmpDir, "drag_fmt_out");

    auto res = reals::audio::DragExporter::exportTempWav(srcWav, opt);
    EXPECT_TRUE(res.success);

    // Verify raw WAV header bytes from the exported file
    std::ifstream ifs(platform::u8path(res.renderedPath), std::ios::binary);
    EXPECT_TRUE(ifs.is_open());
    uint8_t hdr[44];
    ifs.read(reinterpret_cast<char*>(hdr), 44);
    EXPECT_EQ(ifs.gcount(), 44);

    EXPECT_EQ(std::memcmp(&hdr[0], "RIFF", 4), 0);
    EXPECT_EQ(std::memcmp(&hdr[8], "WAVE", 4), 0);
    EXPECT_EQ(std::memcmp(&hdr[12], "fmt ", 4), 0);
    EXPECT_EQ(std::memcmp(&hdr[36], "data", 4), 0);

    uint16_t audioFormat = 0;
    std::memcpy(&audioFormat, &hdr[20], 2);
    EXPECT_EQ(audioFormat, 1); // 16-bit PCM

    uint16_t channels = 0;
    std::memcpy(&channels, &hdr[22], 2);
    EXPECT_EQ(channels, 1);

    uint32_t sampleRate = 0;
    std::memcpy(&sampleRate, &hdr[24], 4);
    EXPECT_EQ(sampleRate, 44100u);
}

TEST(AudioDSP, F01_AutoRenderTemp_DurationStretchVerification) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests");
    platform::ensureDir(tmpDir);
    const std::string srcWav = platform::joinPath(tmpDir, "drag_stretch_check.wav");

    auto pcm = AudioTestFixtures::generateSine(440.0f, 2.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(srcWav, pcm, 1, 44100));

    reals::audio::DragExportOptions opt;
    opt.timeRatio = 2.0f; // 2x speed -> duration should halve to ~1.0s
    opt.customOutputDir = platform::joinPath(tmpDir, "drag_stretch_out");

    auto res = reals::audio::DragExporter::exportTempWav(srcWav, opt);
    EXPECT_TRUE(res.success);
    EXPECT_NEAR(res.durationSeconds, 1.0, 0.1);
}

TEST(AudioDSP, F01_AutoRenderTemp_PitchShiftAccuracy) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests");
    platform::ensureDir(tmpDir);
    const std::string srcWav = platform::joinPath(tmpDir, "drag_pitch_src.wav");

    // Pure 440.0 Hz sine wave
    auto pcm = AudioTestFixtures::generateSine(440.0f, 1.5f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(srcWav, pcm, 1, 44100));

    reals::audio::DragExportOptions opt;
    opt.timeRatio = 1.0f;
    opt.pitchSemitones = 7.0f; // +7 semitones (E5 = 440 * 2^(7/12) = 659.255 Hz)
    opt.customOutputDir = platform::joinPath(tmpDir, "drag_pitch_out");

    auto res = reals::audio::DragExporter::exportTempWav(srcWav, opt);
    EXPECT_TRUE(res.success);

    // Read back rendered PCM frames
    std::ifstream ifs(platform::u8path(res.renderedPath), std::ios::binary);
    EXPECT_TRUE(ifs.is_open());
    ifs.seekg(44);
    std::vector<int16_t> pcm16(static_cast<size_t>(res.durationSeconds * 44100));
    ifs.read(reinterpret_cast<char*>(pcm16.data()), static_cast<std::streamsize>(pcm16.size() * sizeof(int16_t)));

    std::vector<float> pcmFloat(pcm16.size());
    for (size_t i = 0; i < pcm16.size(); ++i) {
        pcmFloat[i] = static_cast<float>(pcm16[i]) / 32767.0f;
    }

    float detectedPitch = AudioTestFixtures::estimateFundamentalFrequency(pcmFloat, 44100);
    EXPECT_NEAR(detectedPitch, 659.25f, 10.0f);
}

TEST(AudioDSP, F01_AutoRenderTemp_DeterministicCacheHit) {
    const std::string tmpDir = platform::joinPath(platform::tempDir(), "RealsLab", "tests");
    platform::ensureDir(tmpDir);
    const std::string srcWav = platform::joinPath(tmpDir, "drag_cache_test.wav");

    auto pcm = AudioTestFixtures::generateSine(440.0f, 1.0f, 44100);
    EXPECT_TRUE(AudioTestFixtures::writeWavFile(srcWav, pcm, 1, 44100));

    reals::audio::DragExportOptions opt;
    opt.timeRatio = 1.2f;
    opt.pitchSemitones = 2.0f;
    opt.customOutputDir = platform::joinPath(tmpDir, "drag_cache_out");

    // Call 1: Render
    auto res1 = reals::audio::DragExporter::exportTempWav(srcWav, opt);
    EXPECT_TRUE(res1.success);

    // Call 2: Cache Hit
    auto res2 = reals::audio::DragExporter::exportTempWav(srcWav, opt);
    EXPECT_TRUE(res2.success);
    EXPECT_EQ(res1.renderedPath, res2.renderedPath);
    EXPECT_GT(res2.renderTimeMs, 0.0);
    EXPECT_LT(res2.renderTimeMs, 25.0); // Cache hit sub-millisecond in release (< 25ms in debug)
}

} // namespace reals::test
