// ============================================================================
// TestSuite_KeyTempoAccuracy.cpp
//
// Verifies accurate musical Key & BPM parsing from filenames, prevents
// erroneous F Major & 50.0 BPM corruption, and tests the database repair pass.
// ============================================================================

#include "../framework/TestRunner.h"
#include <reals/ai/KeyDetector.h>
#include <reals/db/Database.h>
#include <reals/scanner/BackgroundScanner.h>

#include <cmath>
#include <string>

namespace reals::test {

TEST(KeyTempoAccuracy, VendorLoopMetadata) {
    db::SampleRecord r1;
    scanner::BackgroundScanner::parseFilenameMusicMetadata(
        "DS_MDH2_122_bass_sub_loop_assertive_Bbmin.wav",
        "D:/Samples/DS_MDH2_122_bass_sub_loop_assertive_Bbmin.wav",
        r1
    );
    EXPECT_NEAR(r1.bpm, 122.0, 0.01);
    EXPECT_EQ(r1.keyRoot, "Bb");
    EXPECT_EQ(r1.keyMode, "minor");
    EXPECT_EQ(r1.camelot, "3A");

    db::SampleRecord r2;
    scanner::BackgroundScanner::parseFilenameMusicMetadata(
        "DS_MDH2_122_bass_synth_loop_mirror_Amin.wav",
        "D:/Samples/DS_MDH2_122_bass_synth_loop_mirror_Amin.wav",
        r2
    );
    EXPECT_NEAR(r2.bpm, 122.0, 0.01);
    EXPECT_EQ(r2.keyRoot, "A");
    EXPECT_EQ(r2.keyMode, "minor");
    EXPECT_EQ(r2.camelot, "8A");

    db::SampleRecord r3;
    scanner::BackgroundScanner::parseFilenameMusicMetadata(
        "DS_MDH2_122_bass_synth_loop_block_G#min.wav",
        "D:/Samples/DS_MDH2_122_bass_synth_loop_block_G#min.wav",
        r3
    );
    EXPECT_NEAR(r3.bpm, 122.0, 0.01);
    EXPECT_EQ(r3.keyRoot, "G#");
    EXPECT_EQ(r3.keyMode, "minor");
    EXPECT_EQ(r3.camelot, "1A");

    db::SampleRecord r4;
    scanner::BackgroundScanner::parseFilenameMusicMetadata(
        "DS_MDH2_123_bass_sub_loop_dive_Cmin.wav",
        "D:/Samples/DS_MDH2_123_bass_sub_loop_dive_Cmin.wav",
        r4
    );
    EXPECT_NEAR(r4.bpm, 123.0, 0.01);
    EXPECT_EQ(r4.keyRoot, "C");
    EXPECT_EQ(r4.keyMode, "minor");
    EXPECT_EQ(r4.camelot, "5A");
}

TEST(KeyTempoAccuracy, OneShotExclusionFromBpm) {
    // One-shots must NEVER be assigned fake BPMs from their index numbers
    db::SampleRecord r1;
    r1.durationSec = 0.35;
    scanner::BackgroundScanner::parseFilenameMusicMetadata(
        "SMSH_Kick_50.wav", "D:/Samples/SMSH_Kick_50.wav", r1
    );
    EXPECT_EQ(r1.bpm, 0.0);
    EXPECT_TRUE(r1.keyRoot.empty());

    db::SampleRecord r2;
    r2.durationSec = 0.40;
    scanner::BackgroundScanner::parseFilenameMusicMetadata(
        "Ultrasonic - Slap House Essentials - Clap 50.wav",
        "D:/Samples/Ultrasonic - Slap House Essentials - Clap 50.wav",
        r2
    );
    EXPECT_EQ(r2.bpm, 0.0);

    db::SampleRecord r3;
    r3.durationSec = 0.08;
    scanner::BackgroundScanner::parseFilenameMusicMetadata(
        "Ultrasonic - Future Bass Essentials - Closed Hi-Hat 50.wav",
        "D:/Samples/Ultrasonic - Future Bass Essentials - Closed Hi-Hat 50.wav",
        r3
    );
    EXPECT_EQ(r3.bpm, 0.0);

    // Tuned snare with key A at the end
    db::SampleRecord r4;
    r4.durationSec = 1.19;
    scanner::BackgroundScanner::parseFilenameMusicMetadata(
        "ESW Future Bass - Snare 50 - A.wav",
        "D:/Samples/ESW Future Bass - Snare 50 - A.wav",
        r4
    );
    EXPECT_EQ(r4.bpm, 0.0);
    EXPECT_EQ(r4.keyRoot, "A");
}

TEST(KeyTempoAccuracy, BpmPrecedenceOverIndex) {
    // When a filename contains both an index number (e.g. 50, 64) and an explicit BPM token (117BPM, 160 BPM),
    // the parser MUST select the real BPM!
    db::SampleRecord r1;
    scanner::BackgroundScanner::parseFilenameMusicMetadata(
        "SMSH_TopLoop_50_117BPM.wav", "D:/Samples/SMSH_TopLoop_50_117BPM.wav", r1
    );
    EXPECT_NEAR(r1.bpm, 117.0, 0.01);

    db::SampleRecord r2;
    scanner::BackgroundScanner::parseFilenameMusicMetadata(
        "Oversampled_CYBERPACK_glitch_199_90bpm.wav",
        "D:/Samples/Oversampled_CYBERPACK_glitch_199_90bpm.wav",
        r2
    );
    EXPECT_NEAR(r2.bpm, 90.0, 0.01);

    db::SampleRecord r3;
    scanner::BackgroundScanner::parseFilenameMusicMetadata(
        "Cymatics - Apex Vocals 64 - 160 BPM Am.wav",
        "D:/Samples/Cymatics - Apex Vocals 64 - 160 BPM Am.wav",
        r3
    );
    EXPECT_NEAR(r3.bpm, 160.0, 0.01);
    EXPECT_EQ(r3.keyRoot, "A");
    EXPECT_EQ(r3.keyMode, "minor");
    EXPECT_EQ(r3.camelot, "8A");
}

TEST(KeyTempoAccuracy, TrailingPitchedNotes) {
    db::SampleRecord r1;
    scanner::BackgroundScanner::parseFilenameMusicMetadata(
        "SMGP1_Bass_Shot_50_E.wav", "D:/Samples/SMGP1_Bass_Shot_50_E.wav", r1
    );
    EXPECT_EQ(r1.keyRoot, "E");
    EXPECT_EQ(r1.keyMode, "major");
    EXPECT_EQ(r1.camelot, "12B");

    db::SampleRecord r2;
    scanner::BackgroundScanner::parseFilenameMusicMetadata(
        "Lead_Loop_128_F#m.wav", "D:/Samples/Lead_Loop_128_F#m.wav", r2
    );
    EXPECT_NEAR(r2.bpm, 128.0, 0.01);
    EXPECT_EQ(r2.keyRoot, "F#");
    EXPECT_EQ(r2.keyMode, "minor");
    EXPECT_EQ(r2.camelot, "11A");
}

TEST(KeyTempoAccuracy, CamelotMapping) {
    auto [k1, m1] = ai::KeyDetector::fromCamelot("8A");
    EXPECT_EQ(k1, "A");
    EXPECT_EQ(m1, "minor");

    auto [k2, m2] = ai::KeyDetector::fromCamelot("8B");
    EXPECT_EQ(k2, "C");
    EXPECT_EQ(m2, "major");

    auto [k3, m3] = ai::KeyDetector::fromCamelot("1A");
    EXPECT_EQ(k3, "G#");
    EXPECT_EQ(m3, "minor");

    auto [k4, m4] = ai::KeyDetector::fromCamelot("7B");
    EXPECT_EQ(k4, "F");
    EXPECT_EQ(m4, "major");
}

TEST(KeyTempoAccuracy, DatabaseRepairPass) {
    db::Database db;
    EXPECT_TRUE(db.open(":memory:"));

    // Insert 3 samples simulating the old bugs:
    // 1. Bbmin bass loop that got corrupted to F Major
    db::SampleRecord s1;
    s1.path = "D:/Samples/DS_MDH2_122_bass_sub_loop_assertive_Bbmin.wav";
    s1.filename = "DS_MDH2_122_bass_sub_loop_assertive_Bbmin.wav";
    s1.bpm = 122.0;
    s1.keyRoot = "F";
    s1.keyMode = "major";
    s1.camelot = "7B";
    EXPECT_TRUE(db.upsertSample(s1) > 0);

    // 2. Kick 50 that got assigned 50.0 BPM
    db::SampleRecord s2;
    s2.path = "D:/Samples/SMSH_Kick_50.wav";
    s2.filename = "SMSH_Kick_50.wav";
    s2.genre = "Kick";
    s2.bpm = 50.0;
    s2.durationSec = 0.35;
    EXPECT_TRUE(db.upsertSample(s2) > 0);

    // 3. TopLoop 50 that took 50.0 BPM instead of 117.0 BPM
    db::SampleRecord s3;
    s3.path = "D:/Samples/SMSH_TopLoop_50_117BPM.wav";
    s3.filename = "SMSH_TopLoop_50_117BPM.wav";
    s3.genre = "Drums";
    s3.bpm = 50.0;
    s3.durationSec = 4.0;
    EXPECT_TRUE(db.upsertSample(s3) > 0);

    // Run repair pass
    size_t repaired = scanner::BackgroundScanner::repairDatabaseMetadata(db);
    EXPECT_EQ(repaired, 3u);

    // Verify s1 is now Bb minor
    auto res1 = db.getSampleByPath(s1.path);
    EXPECT_TRUE(res1.has_value());
    EXPECT_EQ(res1->keyRoot, "Bb");
    EXPECT_EQ(res1->keyMode, "minor");
    EXPECT_EQ(res1->camelot, "3A");

    // Verify s2 BPM is now 0.0 (cleared from 50.0)
    auto res2 = db.getSampleByPath(s2.path);
    EXPECT_TRUE(res2.has_value());
    EXPECT_EQ(res2->bpm, 0.0);

    // Verify s3 BPM is now 117.0
    auto res3 = db.getSampleByPath(s3.path);
    EXPECT_TRUE(res3.has_value());
    EXPECT_NEAR(res3->bpm, 117.0, 0.01);
}

} // namespace reals::test
