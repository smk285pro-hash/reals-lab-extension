#include <reals/lab/ChordEngine.h>

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace reals::lab {

namespace {

const std::unordered_map<std::string, int> kNoteOffsets = {
    {"C", 0}, {"C#", 1}, {"Db", 1},
    {"D", 2}, {"D#", 3}, {"Eb", 3},
    {"E", 4}, {"Fb", 4}, {"E#", 5},
    {"F", 5}, {"F#", 6}, {"Gb", 6},
    {"G", 7}, {"G#", 8}, {"Ab", 8},
    {"A", 9}, {"A#", 10}, {"Bb", 10},
    {"B", 11}, {"Cb", 11}, {"B#", 0}
};

const std::unordered_map<std::string, std::vector<int>> kChordIntervals = {
    {"", {0, 4, 7}},
    {"maj", {0, 4, 7}},
    {"M", {0, 4, 7}},
    {"m", {0, 3, 7}},
    {"min", {0, 3, 7}},
    {"-", {0, 3, 7}},
    {"7", {0, 4, 7, 10}},
    {"dom7", {0, 4, 7, 10}},
    {"maj7", {0, 4, 7, 11}},
    {"M7", {0, 4, 7, 11}},
    {"m7", {0, 3, 7, 10}},
    {"min7", {0, 3, 7, 10}},
    {"dim", {0, 3, 6}},
    {"dim7", {0, 3, 6, 9}},
    {"m7b5", {0, 3, 6, 10}},
    {"aug", {0, 4, 8}},
    {"+", {0, 4, 8}},
    {"sus4", {0, 5, 7}},
    {"sus", {0, 5, 7}},
    {"sus2", {0, 2, 7}},
    {"6", {0, 4, 7, 9}},
    {"m6", {0, 3, 7, 9}},
    {"9", {0, 4, 7, 10, 14}},
    {"maj9", {0, 4, 7, 11, 14}},
    {"m9", {0, 3, 7, 10, 14}},
    {"add9", {0, 4, 7, 14}}
};

} // namespace

bool ChordEngine::parseChordName(const std::string& chordName, std::string& outRoot, std::string& outQuality) {
    outRoot.clear();
    outQuality.clear();
    if (chordName.empty()) {
        return false;
    }

    // Single or double letter root (e.g. C, C#, Bb)
    char first = chordName[0];
    if (first >= 'a' && first <= 'g') {
        first = static_cast<char>(first - 'a' + 'A');
    }
    if (first < 'A' || first > 'G') {
        return false;
    }

    outRoot += first;
    size_t idx = 1;
    if (idx < chordName.size() && (chordName[idx] == '#' || chordName[idx] == 'b')) {
        outRoot += chordName[idx];
        ++idx;
    }

    if (idx < chordName.size()) {
        outQuality = chordName.substr(idx);
    }
    return true;
}

std::vector<int> ChordEngine::getMidiNotes(const std::string& chordName, int rootOctave) {
    std::string root;
    std::string quality;
    if (!parseChordName(chordName, root, quality)) {
        return {};
    }

    auto rootIt = kNoteOffsets.find(root);
    if (rootIt == kNoteOffsets.end()) {
        return {};
    }

    int baseNote = (rootOctave + 1) * 12 + rootIt->second;

    auto it = kChordIntervals.find(quality);
    const std::vector<int>& intervals = (it != kChordIntervals.end()) ? it->second : kChordIntervals.at("");

    std::vector<int> notes;
    notes.reserve(intervals.size());
    for (int offset : intervals) {
        notes.push_back(baseNote + offset);
    }
    return notes;
}

ChordProgression ChordEngine::parseApiResponse(const nlohmann::json& json) {
    ChordProgression prog;
    if (!json.is_object()) {
        return prog;
    }

    if (json.contains("telemetry") && json["telemetry"].is_object()) {
        const auto& t = json["telemetry"];
        prog.bpm = t.value("bpm", 120.0);
        prog.masterKey = t.value("master_key", "");
        prog.scaleMode = t.value("scale_mode", "");
        prog.timeSignature = t.value("time_signature", "4/4");
        prog.duration = t.value("duration", 0.0);
    }

    if (json.contains("chords") && json["chords"].is_array()) {
        for (const auto& item : json["chords"]) {
            if (!item.is_object()) continue;

            ChordEvent evt;
            evt.name = item.value("chord", item.value("name", ""));
            evt.startTime = item.value("timestamp", item.value("start", item.value("time", 0.0)));
            if (item.contains("end_time") && item["end_time"].is_number()) {
                evt.endTime = item["end_time"].get<double>();
            } else if (item.contains("end") && item["end"].is_number()) {
                evt.endTime = item["end"].get<double>();
            } else if (item.contains("duration") && item["duration"].is_number()) {
                evt.endTime = evt.startTime + item["duration"].get<double>();
            } else {
                evt.endTime = evt.startTime + 2.0;
            }
            evt.roman = item.value("roman_numeral", item.value("roman", ""));
            evt.confidence = item.value("confidence", 1.0f);

            if (!evt.name.empty() && evt.name != "N" && evt.name != "None") {
                evt.midiNotes = getMidiNotes(evt.name, 3);
                prog.events.push_back(evt);
            }
        }
    }

    return prog;
}

void ChordEngine::snapToBeatGrid(ChordProgression& progression, double toleranceSeconds) {
    if (progression.bpm <= 0.0 || progression.events.empty()) {
        return;
    }

    const double secondsPerBeat = 60.0 / progression.bpm;
    for (auto& evt : progression.events) {
        // Snap start time
        double beatExact = evt.startTime / secondsPerBeat;
        double nearestBeat = std::round(beatExact);
        double snappedTime = nearestBeat * secondsPerBeat;
        if (std::abs(snappedTime - evt.startTime) <= toleranceSeconds) {
            evt.startTime = snappedTime;
        }

        // Snap end time
        double endBeatExact = evt.endTime / secondsPerBeat;
        double nearestEndBeat = std::round(endBeatExact);
        double snappedEndTime = nearestEndBeat * secondsPerBeat;
        if (std::abs(snappedEndTime - evt.endTime) <= toleranceSeconds) {
            evt.endTime = snappedEndTime;
        }

        if (evt.endTime <= evt.startTime) {
            evt.endTime = evt.startTime + secondsPerBeat;
        }
    }

    // Merge consecutive identical chords if their timestamps touch
    std::vector<ChordEvent> merged;
    for (const auto& evt : progression.events) {
        if (!merged.empty() && merged.back().name == evt.name &&
            std::abs(merged.back().endTime - evt.startTime) < 0.05) {
            merged.back().endTime = evt.endTime;
        } else {
            merged.push_back(evt);
        }
    }
    progression.events = std::move(merged);
}

} // namespace reals::lab
