#include <reals/lab/ChordEngine.h>
#include "../framework/TestRunner.h"

#include <nlohmann/json.hpp>

namespace reals::test {

TEST(ChordEngine, ParseChordNames) {
    std::string root, quality;

    ASSERT_TRUE(lab::ChordEngine::parseChordName("C", root, quality));
    ASSERT_EQ(root, "C");
    ASSERT_EQ(quality, "");

    ASSERT_TRUE(lab::ChordEngine::parseChordName("C#m", root, quality));
    ASSERT_EQ(root, "C#");
    ASSERT_EQ(quality, "m");

    ASSERT_TRUE(lab::ChordEngine::parseChordName("Bbm7", root, quality));
    ASSERT_EQ(root, "Bb");
    ASSERT_EQ(quality, "m7");

    ASSERT_TRUE(lab::ChordEngine::parseChordName("F#maj7", root, quality));
    ASSERT_EQ(root, "F#");
    ASSERT_EQ(quality, "maj7");

    ASSERT_TRUE(lab::ChordEngine::parseChordName("Dsus4", root, quality));
    ASSERT_EQ(root, "D");
    ASSERT_EQ(quality, "sus4");
}

TEST(ChordEngine, GenerateMidiNotes) {
    // C Major: C3(48), E3(52), G3(55)
    auto cNotes = lab::ChordEngine::getMidiNotes("C", 3);
    ASSERT_EQ(cNotes.size(), 3u);
    ASSERT_EQ(cNotes[0], 48);
    ASSERT_EQ(cNotes[1], 52);
    ASSERT_EQ(cNotes[2], 55);

    // A Minor: A3(57), C4(60), E4(64)
    auto amNotes = lab::ChordEngine::getMidiNotes("Am", 3);
    ASSERT_EQ(amNotes.size(), 3u);
    ASSERT_EQ(amNotes[0], 57);
    ASSERT_EQ(amNotes[1], 60);
    ASSERT_EQ(amNotes[2], 64);

    // G7: G3(55), B3(59), D4(62), F4(65)
    auto g7Notes = lab::ChordEngine::getMidiNotes("G7", 3);
    ASSERT_EQ(g7Notes.size(), 4u);
    ASSERT_EQ(g7Notes[0], 55);
    ASSERT_EQ(g7Notes[1], 59);
    ASSERT_EQ(g7Notes[2], 62);
    ASSERT_EQ(g7Notes[3], 65);
}

TEST(ChordEngine, ParseApiResponseAndSnap) {
    nlohmann::json apiJson = {
        {"telemetry", {
            {"bpm", 120.0},
            {"master_key", "C"},
            {"scale_mode", "major"},
            {"time_signature", "4/4"},
            {"duration", 8.0}
        }},
        {"chords", {
            {{"chord", "C"}, {"timestamp", 0.04}, {"end_time", 1.95}, {"roman_numeral", "I"}},
            {{"chord", "Am"}, {"timestamp", 2.05}, {"end_time", 3.98}, {"roman_numeral", "vi"}},
            {{"chord", "F"}, {"timestamp", 4.02}, {"end_time", 5.99}, {"roman_numeral", "IV"}},
            {{"chord", "G"}, {"timestamp", 6.01}, {"end_time", 7.95}, {"roman_numeral", "V"}}
        }}
    };

    auto prog = lab::ChordEngine::parseApiResponse(apiJson);
    ASSERT_EQ(prog.bpm, 120.0);
    ASSERT_EQ(prog.masterKey, "C");
    ASSERT_EQ(prog.events.size(), 4u);
    ASSERT_EQ(prog.events[0].name, "C");
    ASSERT_EQ(prog.events[1].name, "Am");

    // Snap to 120 BPM grid (0.5s per beat, 2.0s per half measure / measure)
    lab::ChordEngine::snapToBeatGrid(prog, 0.15);
    ASSERT_EQ(prog.events[0].startTime, 0.0);
    ASSERT_EQ(prog.events[0].endTime, 2.0);
    ASSERT_EQ(prog.events[1].startTime, 2.0);
    ASSERT_EQ(prog.events[1].endTime, 4.0);
}

} // namespace reals::test
