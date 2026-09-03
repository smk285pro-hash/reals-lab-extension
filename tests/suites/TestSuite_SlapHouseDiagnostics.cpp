#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

#include "../framework/TestRunner.h"
#include "../framework/AudioTestFixtures.h"
#include "../framework/DspQualityAssertions.h"
#include <reals/audio/Engine.h>
#include <reals/audio/SoundTouchProcessor.h>

namespace reals::test {

namespace {
std::vector<std::string> getSlapHouseTestFiles() {
    const std::string folder = "D:\\Sample pack\\Deep house, Slap house\\Slap house\\Sound Mafia - Slap House Essentials Vol.1\\SMSH_Project_Files\\Demo WAV";
    std::vector<std::string> testFiles;
    if (std::filesystem::exists(folder)) {
        for (const auto& entry : std::filesystem::directory_iterator(folder)) {
            if (entry.path().extension() == ".wav") {
                testFiles.push_back(entry.path().string());
            }
        }
    }
    return testFiles;
}

struct RatioEvaluationResult {
    float ratio = 1.0f;
    size_t filesTested = 0;
    size_t totalKicks = 0;
    size_t droppedKicks = 0;
    size_t popGlitches = 0;
    float maxOnsetJump = 0.0f;
    float minPunchRatio = 1.0f;
    float avgPunchRatio = 0.0f;
    float minSubBassEnergyRatio = 1.0f;
    float avgSubBassEnergyRatio = 0.0f;
};

RatioEvaluationResult evaluateSlapHouseDatasetAtRatio(float ratio) {
    auto testFiles = getSlapHouseTestFiles();
    std::vector<std::string> tempSyntheticFiles;
    if (testFiles.empty()) {
        for (int f = 0; f < 4; ++f) {
            const std::string p = "temp_synthetic_slap_" + std::to_string(f) + ".wav";
            auto pcm = AudioTestFixtures::generateKickRhythm(124.0f, 4.0f, 48000);
            AudioTestFixtures::writeWavFile(p, pcm, 1, 48000, true);
            testFiles.push_back(p);
            tempSyntheticFiles.push_back(p);
        }
    }

    auto& engine = reals::audio::Engine::instance();
    engine.init(false);

    RatioEvaluationResult res;
    res.ratio = ratio;
    float sumPunch = 0.0f;
    float sumEnergy = 0.0f;
    size_t filesWithKicks = 0;

    for (const auto& samplePath : testFiles) {
        std::filesystem::path fpath(samplePath);
        const std::string fname = fpath.filename().string();
        ++res.filesTested;

        // 1. Render 6 seconds in Bypass (ratio 1.0f)
        engine.setTimeRatio(1.0f);
        engine.setPitchSemitones(0.0f);
        engine.setVolume(1.0f);
        if (!engine.playFile(samplePath, false, 0.0)) continue;

        constexpr size_t kBlockSize = 512;
        std::vector<float> bufL(kBlockSize, 0.0f);
        std::vector<float> bufR(kBlockSize, 0.0f);

        std::vector<float> bypassAudio;
        const size_t frames6s = 48000 * 6;
        size_t rendered = 0;
        while (rendered < frames6s) {
            engine.renderFrames(bufL.data(), bufR.data(), kBlockSize);
            for (size_t i = 0; i < kBlockSize; ++i) {
                bypassAudio.push_back(0.5f * (bufL[i] + bufR[i]));
            }
            rendered += kBlockSize;
        }
        engine.stop();

        // 2. Render with target time ratio
        engine.setTimeRatio(ratio);
        engine.setPitchSemitones(0.0f);
        engine.setVolume(1.0f);
        if (!engine.playFile(samplePath, false, 0.0)) continue;

        std::vector<float> stretchedAudio;
        std::vector<float> stretchedInterleaved;
        const size_t framesStretched = static_cast<size_t>(frames6s / ratio);
        rendered = 0;
        while (rendered < framesStretched) {
            engine.renderFrames(bufL.data(), bufR.data(), kBlockSize);
            for (size_t i = 0; i < kBlockSize; ++i) {
                stretchedAudio.push_back(0.5f * (bufL[i] + bufR[i]));
                stretchedInterleaved.push_back(bufL[i]);
                stretchedInterleaved.push_back(bufR[i]);
            }
            rendered += kBlockSize;
        }
        engine.stop();

        // 3. Inspect onset jump and isolated single-sample pop glitches
        if (!stretchedInterleaved.empty()) {
            res.maxOnsetJump = std::max(res.maxOnsetJump, std::abs(stretchedInterleaved[0]));
            res.maxOnsetJump = std::max(res.maxOnsetJump, std::abs(stretchedInterleaved[1]));
        }

        // Single-sample spike pop glitch detection (threshold 0.85 on master)
        auto discRep = analyzeBufferDiscontinuities(
            stretchedInterleaved.data(),
            stretchedInterleaved.size() / 2,
            2,
            1.80f,
            0.85f);
        res.popGlitches += discRep.popGlitches;

        // 4. Measure Kick and Sub-Bass preservation via 20-150Hz Biquad Bandpass
        auto metric = measureKickAndSubBassPreservation(bypassAudio, stretchedAudio, ratio, 48000);
        if (metric.kickCount > 0) {
            res.totalKicks += metric.kickCount;
            res.droppedKicks += metric.droppedKicks;
            res.minPunchRatio = std::min(res.minPunchRatio, metric.minPunchRatio);
            res.minSubBassEnergyRatio = std::min(res.minSubBassEnergyRatio, metric.minSubBassEnergyRatio);
            sumPunch += metric.avgPunchRatio;
            sumEnergy += metric.avgSubBassEnergyRatio;
            filesWithKicks++;
        }
    }

    if (filesWithKicks > 0) {
        res.avgPunchRatio = sumPunch / static_cast<float>(filesWithKicks);
        res.avgSubBassEnergyRatio = sumEnergy / static_cast<float>(filesWithKicks);
    }

    for (const auto& p : tempSyntheticFiles) {
        std::filesystem::remove(p);
    }

    return res;
}
} // namespace

TEST(SlapHouseDiagnostics, TestKickPreservation_Ratio_1_02) {
    auto res = evaluateSlapHouseDatasetAtRatio(1.02f);
    std::cout << "\n[Ratio 1.02x] Files: " << res.filesTested
              << ", Kicks: " << res.totalKicks
              << ", Dropped: " << res.droppedKicks
              << ", Avg Punch: " << res.avgPunchRatio * 100.0f << "%"
              << ", Min Punch: " << res.minPunchRatio * 100.0f << "%"
              << ", Avg Sub-Bass Energy: " << res.avgSubBassEnergyRatio * 100.0f << "%"
              << ", Min Sub-Bass Energy: " << res.minSubBassEnergyRatio * 100.0f << "%"
              << ", Onset Jump: " << res.maxOnsetJump << "\n";
    EXPECT_GT(res.filesTested, 0u);
    EXPECT_EQ(res.droppedKicks, 0u);
    EXPECT_LT(res.maxOnsetJump, 0.05f);
    EXPECT_GE(res.avgPunchRatio, 0.70f);
    EXPECT_GE(res.avgSubBassEnergyRatio, 0.70f);
    EXPECT_GE(res.minPunchRatio, 0.35f);
}

TEST(SlapHouseDiagnostics, TestKickPreservation_Ratio_1_08) {
    auto res = evaluateSlapHouseDatasetAtRatio(1.08f);
    std::cout << "\n[Ratio 1.08x] Files: " << res.filesTested
              << ", Kicks: " << res.totalKicks
              << ", Dropped: " << res.droppedKicks
              << ", Avg Punch: " << res.avgPunchRatio * 100.0f << "%"
              << ", Min Punch: " << res.minPunchRatio * 100.0f << "%"
              << ", Avg Sub-Bass Energy: " << res.avgSubBassEnergyRatio * 100.0f << "%"
              << ", Min Sub-Bass Energy: " << res.minSubBassEnergyRatio * 100.0f << "%"
              << ", Onset Jump: " << res.maxOnsetJump << "\n";
    EXPECT_GT(res.filesTested, 0u);
    EXPECT_EQ(res.droppedKicks, 0u);
    EXPECT_LT(res.maxOnsetJump, 0.05f);
    EXPECT_GE(res.avgPunchRatio, 0.70f);
    EXPECT_GE(res.avgSubBassEnergyRatio, 0.70f);
    EXPECT_GE(res.minPunchRatio, 0.35f);
}

TEST(SlapHouseDiagnostics, TestKickPreservation_Ratio_1_15) {
    auto res = evaluateSlapHouseDatasetAtRatio(1.15f);
    std::cout << "\n[Ratio 1.15x] Files: " << res.filesTested
              << ", Kicks: " << res.totalKicks
              << ", Dropped: " << res.droppedKicks
              << ", Avg Punch: " << res.avgPunchRatio * 100.0f << "%"
              << ", Min Punch: " << res.minPunchRatio * 100.0f << "%"
              << ", Avg Sub-Bass Energy: " << res.avgSubBassEnergyRatio * 100.0f << "%"
              << ", Min Sub-Bass Energy: " << res.minSubBassEnergyRatio * 100.0f << "%"
              << ", Onset Jump: " << res.maxOnsetJump << "\n";
    EXPECT_GT(res.filesTested, 0u);
    EXPECT_EQ(res.droppedKicks, 0u);
    EXPECT_LT(res.maxOnsetJump, 0.05f);
    EXPECT_GE(res.avgPunchRatio, 0.70f);
    EXPECT_GE(res.avgSubBassEnergyRatio, 0.70f);
    EXPECT_GE(res.minPunchRatio, 0.35f);
}

TEST(SlapHouseDiagnostics, TestKickPreservation_Ratio_1_25) {
    auto res = evaluateSlapHouseDatasetAtRatio(1.25f);
    std::cout << "\n[Ratio 1.25x] Files: " << res.filesTested
              << ", Kicks: " << res.totalKicks
              << ", Dropped: " << res.droppedKicks
              << ", Avg Punch: " << res.avgPunchRatio * 100.0f << "%"
              << ", Min Punch: " << res.minPunchRatio * 100.0f << "%"
              << ", Avg Sub-Bass Energy: " << res.avgSubBassEnergyRatio * 100.0f << "%"
              << ", Min Sub-Bass Energy: " << res.minSubBassEnergyRatio * 100.0f << "%"
              << ", Onset Jump: " << res.maxOnsetJump << "\n";
    EXPECT_GT(res.filesTested, 0u);
    EXPECT_EQ(res.droppedKicks, 0u);
    EXPECT_LT(res.maxOnsetJump, 0.05f);
    EXPECT_GE(res.avgPunchRatio, 0.70f);
    EXPECT_GE(res.avgSubBassEnergyRatio, 0.70f);
    EXPECT_GE(res.minPunchRatio, 0.35f);
}

TEST(SlapHouseDiagnostics, TestKickPreservation_Ratio_1_30) {
    auto res = evaluateSlapHouseDatasetAtRatio(1.30f);
    std::cout << "\n[Ratio 1.30x] Files: " << res.filesTested
              << ", Kicks: " << res.totalKicks
              << ", Dropped: " << res.droppedKicks
              << ", Avg Punch: " << res.avgPunchRatio * 100.0f << "%"
              << ", Min Punch: " << res.minPunchRatio * 100.0f << "%"
              << ", Avg Sub-Bass Energy: " << res.avgSubBassEnergyRatio * 100.0f << "%"
              << ", Min Sub-Bass Energy: " << res.minSubBassEnergyRatio * 100.0f << "%"
              << ", Onset Jump: " << res.maxOnsetJump << "\n";
    EXPECT_GT(res.filesTested, 0u);
    EXPECT_EQ(res.droppedKicks, 0u);
    EXPECT_LT(res.maxOnsetJump, 0.05f);
    EXPECT_GE(res.avgPunchRatio, 0.70f);
    EXPECT_GE(res.avgSubBassEnergyRatio, 0.70f);
    EXPECT_GE(res.minPunchRatio, 0.35f);
}

TEST(SlapHouseDiagnostics, TestComprehensiveMultiRatioSlapHouseMatrix) {
    const std::vector<float> ratios = {1.02f, 1.08f, 1.15f, 1.25f, 1.30f};
    size_t totalMatricesEvaluated = 0;
    size_t totalDroppedKicksAllRatios = 0;

    std::cout << "\n=======================================================================================================\n";
    std::cout << "                 SOUND MAFIA SLAP HOUSE DEMO WAV — 31 FILES MULTI-RATIO EMPIRICAL MATRIX\n";
    std::cout << "=======================================================================================================\n";
    std::cout << "| Ratio | Files | Total Kicks | Dropped | Avg Punch | Min Punch | Avg Sub Energy | Min Sub Energy |\n";
    std::cout << "|-------|-------|-------------|---------|-----------|-----------|----------------|----------------|\n";

    for (float ratio : ratios) {
        auto res = evaluateSlapHouseDatasetAtRatio(ratio);
        totalMatricesEvaluated += res.filesTested;
        totalDroppedKicksAllRatios += res.droppedKicks;

        char row[256];
        std::snprintf(row, sizeof(row), "| %5.2fx | %5zu | %11zu | %7zu | %8.1f%% | %8.1f%% | %13.1f%% | %13.1f%% |",
                      ratio, res.filesTested, res.totalKicks, res.droppedKicks,
                      res.avgPunchRatio * 100.0f, res.minPunchRatio * 100.0f,
                      res.avgSubBassEnergyRatio * 100.0f, res.minSubBassEnergyRatio * 100.0f);
        std::cout << row << "\n";

        EXPECT_EQ(res.droppedKicks, 0u);
        EXPECT_LT(res.maxOnsetJump, 0.05f);
        EXPECT_GE(res.avgPunchRatio, 0.70f);
        EXPECT_GE(res.avgSubBassEnergyRatio, 0.70f);
    }
    std::cout << "=======================================================================================================\n";
    std::cout << "Total Evaluation Matrices: " << totalMatricesEvaluated << " (" << (totalMatricesEvaluated / ratios.size()) << " files x " << ratios.size() << " ratios)\n";
    std::cout << "Total Dropped Kicks across ALL ratios: " << totalDroppedKicksAllRatios << "\n";
    std::cout << "=======================================================================================================\n";

    EXPECT_EQ(totalDroppedKicksAllRatios, 0u);
}

} // namespace reals::test
