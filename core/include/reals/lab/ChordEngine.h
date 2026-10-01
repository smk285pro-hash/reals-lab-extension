#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace reals::lab {

struct ChordEvent {
    std::string name;
    double startTime = 0.0;
    double endTime = 0.0;
    std::string roman;
    float confidence = 0.0f;
    std::vector<int> midiNotes; // e.g. {48, 52, 55} for C Major triad
};

struct ChordProgression {
    std::string masterKey;
    std::string scaleMode;
    double bpm = 120.0;
    std::string timeSignature = "4/4";
    double duration = 0.0;
    std::vector<ChordEvent> events;
};

class ChordEngine {
public:
    // Parses root and quality from chord name, e.g. "C#m7" -> root "C#", quality "m7"
    static bool parseChordName(const std::string& chordName, std::string& outRoot, std::string& outQuality);

    // Converts chord name to standard MIDI note numbers (octave 3 base: C3 = 48)
    static std::vector<int> getMidiNotes(const std::string& chordName, int rootOctave = 3);

    // Parses raw API JSON from Modal/LabApi into structured ChordProgression
    static ChordProgression parseApiResponse(const nlohmann::json& json);

    // Snaps chord events to nearest musical beats/measures given bpm and time signature
    static void snapToBeatGrid(ChordProgression& progression, double toleranceSeconds = 0.15);
};

} // namespace reals::lab
