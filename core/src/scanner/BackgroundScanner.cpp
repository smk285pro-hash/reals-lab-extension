#include "reals/scanner/BackgroundScanner.h"
#include "reals/ai/KeyDetector.h"
#include "reals/ai/TempoDetector.h"
#include "reals/ai/FeatureExtractor.h"
#include "reals/ai/ClapEmbedder.h"
#include "reals/audio/Engine.h"
#include "reals/platform/Path.h"
#include "reals/util/Hash.h"
#include "reals/util/Log.h"
#include <miniaudio.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <filesystem>
#include <regex>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace reals::scanner {

namespace {
constexpr auto kTag = "scanner";

std::string toLower(std::string s) {
    for (char& c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

bool isIgnoredDir(std::string_view dirName) {
    if (dirName.empty())
        return false;
    if (dirName.front() == '.' && dirName != ".")
        return true;
    if (dirName == "node_modules" || dirName == "$RECYCLE.BIN" || dirName == "System Volume Information" ||
        dirName == ".git" || dirName == ".svn" || dirName == "__pycache__")
        return true;
    return false;
}

int64_t toUnixEpoch(const fs::file_time_type& ftime) {
    try {
        const auto sysTime = std::chrono::clock_cast<std::chrono::system_clock>(ftime);
        return std::chrono::duration_cast<std::chrono::seconds>(sysTime.time_since_epoch()).count();
    } catch (...) {
        return 0;
    }
}

static const std::unordered_set<std::string> kStopWords = {
    "dry", "wet", "drop", "daw", "bpm", "eq", "fx", "mid", "max", "min", "sub", "top",
    "out", "in", "vol", "rec", "raw", "rev", "pan", "cut", "air", "amp", "bar", "bass",
    "beat", "big", "bit", "bop", "bow", "box", "boy", "bus", "cab", "cap", "cat", "cho",
    "clap", "clip", "cold", "core", "cru", "cue", "cup", "dac", "dam", "damp", "dap",
    "dark", "dash", "dat", "date", "day", "db", "dc", "dec", "deck", "deep", "def",
    "deg", "del", "dell", "demo", "den", "dep", "dept", "der", "des", "det", "dev",
    "dex", "di", "dia", "dial", "dib", "dice", "dick", "dict", "did", "die", "diet",
    "diff", "dig", "digi", "dim", "din", "dine", "ding", "dink", "dint", "dio", "diod",
    "dip", "dir", "direct", "dirt", "dis", "disc", "dish", "disk", "diss", "dist",
    "dit", "dive", "dj", "dna", "dnb", "do", "doc", "dock", "dod", "doe", "dog",
    "dol", "dole", "doll", "dome", "don", "done", "doo", "doom", "door", "dop",
    "dope", "dor", "dorm", "dos", "dose", "dot", "dou", "doug", "doux", "dove",
    "dow", "down", "dr", "dra", "drab", "drag", "dram", "draw", "dray", "dread",
    "dream", "dreg", "dress", "drew", "dri", "drib", "drif", "drill", "drim",
    "drin", "drip", "dris", "driv", "drive", "dro", "droi", "dron", "drone",
    "drop", "drove", "drow", "drub", "drug", "drum", "drums", "dry", "dsp", "dub",
    "dubb", "duce", "duck", "duct", "dud", "dude", "due", "duel", "duet", "duff",
    "dug", "duke", "dull", "duly", "dumb", "dump", "dun", "dunc", "dune", "dung",
    "dunk", "dunn", "duo", "duos", "dup", "dupe", "dur", "dura", "dure", "durn",
    "durr", "dusk", "dust", "dut", "dutch", "duty", "duv", "dux", "dv", "dvd",
    "dvm", "dw", "dwa", "dwel", "dwig", "dwin", "dx", "dy", "dye", "dyer",
    "dyke", "dyn", "dyna", "dyne", "dz", "echo", "edit", "effect", "fast", "fill",
    "free", "gain", "good", "growl", "heavy", "high", "hard", "horn", "hype",
    "hit", "intro", "jump", "just", "kick", "loop", "lead", "main", "mini",
    "muted", "noise", "oct", "open", "off", "on", "organ", "orig", "outro",
    "pad", "pan", "part", "perc", "pluck", "post", "pre", "pro", "raw", "real",
    "rest", "rise", "roll", "sample", "side", "slap", "slow", "snap", "soft",
    "solo", "song", "stem", "stop", "synth", "tail", "tape", "tech", "test",
    "tight", "time", "tone", "track", "trap", "trim", "trio", "tune", "type",
    "unit", "vibe", "vinyl", "vocal", "voice", "vox", "warm", "wave", "wide",
    "wood", "zero", "zone", "sc", "no", "line", "basic"
};

std::vector<std::string> splitTokens(std::string_view str) {
    std::vector<std::string> tokens;
    std::string cur;
    for (char c : str) {
        if (c == '_' || c == '-' || c == ' ' || c == '.' || c == '[' || c == ']' ||
            c == '(' || c == ')' || c == '{' || c == '}' || c == '+' || c == ',') {
            if (!cur.empty()) {
                tokens.push_back(std::move(cur));
                cur.clear();
            }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) {
        tokens.push_back(std::move(cur));
    }
    return tokens;
}

void parseFilenameMusicMetadata(const std::string& filename, const std::string& fullPath, db::SampleRecord& rec) {
    std::string fn = filename;
    const size_t dot = fn.find_last_of('.');
    if (dot != std::string::npos) fn = fn.substr(0, dot);

    const auto tokens = splitTokens(fn);
    std::vector<std::string> tokensLower;
    tokensLower.reserve(tokens.size());
    for (const auto& t : tokens) {
        tokensLower.push_back(toLower(t));
    }

    auto hasToken = [&](const std::vector<std::string>& candidates) {
        for (const auto& t : tokensLower) {
            for (const auto& c : candidates) {
                if (t == c) return true;
            }
        }
        return false;
    };

    // 1. Detect Instruments / Category (Strict Priority: Percussion/Drums > Melodic Instruments)
    std::string category;
    if (hasToken({"clap", "claps", "snap", "snaps"})) {
        category = "Clap";
    } else if (hasToken({"hihat", "hihats", "hat", "hats", "openhat", "closedhat", "cymbal", "cymbals", "ride", "crash"})) {
        category = "Hi-Hat";
    } else if (hasToken({"kick", "kicks"})) {
        category = "Kick";
    } else if (hasToken({"snare", "snares", "rim", "rimshot"})) {
        category = "Snare";
    } else if (hasToken({"tom", "toms", "perc", "percs", "percussion", "shaker", "shakers", "bongo", "conga", "tambourine"})) {
        category = "Percussion";
    } else if (hasToken({"drum", "drums", "drumloop", "drumkit", "toploop"})) {
        category = "Drums";
    } else if (hasToken({"piano", "grandpiano", "rhodes", "wurlitzer", "upright", "pianoloop"})) {
        category = "Piano";
    } else if (hasToken({"strings", "string", "violin", "cello", "viola", "orchestral", "orchestra"})) {
        category = "Strings";
    } else if (hasToken({"guitar", "guitars", "gtr", "acoustic", "nylon", "electricgtr"})) {
        category = "Guitar";
    } else if (hasToken({"brass", "trumpet", "trumpets", "sax", "saxophone", "trombone", "horn", "horns"})) {
        category = "Brass";
    } else if (hasToken({"flute", "flutes", "woodwind", "clarinet"})) {
        category = "Flute";
    } else if (hasToken({"vocal", "vocals", "vox", "acapella", "choir", "chant", "adlib", "adlibs", "harmony"})) {
        category = "Vocal";
    } else if (hasToken({"808", "808s", "sub", "subbass", "bass", "bassline", "reese", "synthbass"})) {
        category = "Bass";
    } else if (hasToken({"synth", "synths", "pad", "pads", "lead", "leads", "pluck", "plucks", "arp", "arps", "keys"})) {
        category = "Synth";
    } else if (hasToken({"fx", "riser", "risers", "downlifter", "sweep", "impact", "impacts", "foley"})) {
        category = "FX";
    } else if (hasToken({"trap"})) {
        category = "Trap";
    } else if (hasToken({"hiphop", "boombap"})) {
        category = "Hip Hop";
    } else if (hasToken({"house"})) {
        category = "House";
    } else if (hasToken({"techno"})) {
        category = "Techno";
    } else if (hasToken({"drill"})) {
        category = "Drill";
    } else if (hasToken({"phonk"})) {
        category = "Phonk";
    } else if (hasToken({"lofi"})) {
        category = "Lo-Fi";
    }

    // Fallback to directory name if filename did not match
    if (category.empty() && !fullPath.empty()) {
        const auto pathTokens = splitTokens(fullPath);
        for (const auto& pt : pathTokens) {
            const std::string ptl = toLower(pt);
            if (ptl == "piano" || ptl == "keys") { category = "Piano"; break; }
            if (ptl == "strings" || ptl == "string") { category = "Strings"; break; }
            if (ptl == "guitar" || ptl == "guitars") { category = "Guitar"; break; }
            if (ptl == "vocal" || ptl == "vocals" || ptl == "vox") { category = "Vocal"; break; }
            if (ptl == "drums" || ptl == "drum") { category = "Drums"; break; }
            if (ptl == "bass") { category = "Bass"; break; }
            if (ptl == "synth" || ptl == "synths") { category = "Synth"; break; }
            if (ptl == "brass") { category = "Brass"; break; }
        }
    }

    if (!category.empty()) {
        rec.genre = category;
    }

    // 2. Extract BPM from isolated tokens
    if (rec.bpm <= 0.0) {
        static const std::regex bpmNumRegex(R"(^(\d{2,3}(?:\.\d+)?)(?:bpm|BPM)?$)");
        for (const auto& t : tokens) {
            std::smatch m;
            if (std::regex_match(t, m, bpmNumRegex)) {
                try {
                    double val = std::stod(m[1].str());
                    if (val >= 50.0 && val <= 220.0) {
                        rec.bpm = val;
                        break;
                    }
                } catch (...) {}
            }
        }
    }

    // 3. Extract Musical Key (ONLY for tonal/melodic samples; drum/perc/fx are unpitched)
    bool isUnpitched = (category == "Clap" || category == "Hi-Hat" || category == "Kick" ||
                        category == "Snare" || category == "Percussion" || category == "Drums" ||
                        category == "FX");
    if (!isUnpitched && rec.keyRoot.empty()) {
        static const std::regex keyTokenRegex(R"(^([A-Ga-g][#b]?)(m|min|minor|maj|major)?$)");
        for (const auto& t : tokens) {
            const std::string tl = toLower(t);
            if (kStopWords.find(tl) != kStopWords.end()) {
                continue;
            }

            std::smatch m;
            if (std::regex_match(t, m, keyTokenRegex)) {
                std::string root = m[1].str();
                root[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(root[0])));
                if (root.size() > 1 && root[1] == 'b') {
                    root[1] = 'b';
                }

                // If single letter like 'D' or 'E', reject if part of typical noise or short words
                if (t.size() == 1 && tl.size() == 1 && (tl == "d" || tl == "e" || tl == "a" || tl == "c" || tl == "b" || tl == "f" || tl == "g")) {
                    // Only accept if filename explicitly contains Key_ or in_ or Camelot
                    continue;
                }

                std::string modeStr = m.size() >= 3 ? m[2].str() : "";
                std::string mode = "major";
                std::string lowerMode = toLower(modeStr);
                if (lowerMode == "m" || lowerMode == "min" || lowerMode == "minor" || (t.size() == 2 && t[1] == 'm')) {
                    mode = "minor";
                }
                rec.keyRoot = root;
                rec.keyMode = mode;
                rec.camelot = ai::KeyDetector::toCamelot(root, mode);
                break;
            }
        }
    } else if (isUnpitched) {
        // Clear any accidental key from drum/perc files
        rec.keyRoot.clear();
        rec.keyMode.clear();
        rec.camelot.clear();
    }
}

void analyzeAudioRealWaveform(const std::string& filePath, db::SampleRecord& rec, std::vector<float>& outEmbedding) {
    constexpr ma_uint64 kMaxFrames = 44100 * 8;
    ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, 1, 0);
    ma_decoder dec{};
#ifdef _WIN32
    const std::wstring wpath = platform::u8path(filePath).wstring();
    const ma_result decRes = ma_decoder_init_file_w(wpath.c_str(), &cfg, &dec);
#else
    const ma_result decRes = ma_decoder_init_file(filePath.c_str(), &cfg, &dec);
#endif
    if (decRes != MA_SUCCESS) {
        return;
    }

    const int sr = dec.outputSampleRate > 0 ? dec.outputSampleRate : 44100;
    std::vector<float> pcm;
    pcm.reserve(static_cast<size_t>(std::min<ma_uint64>(kMaxFrames, 44100 * 4)));
    std::vector<float> chunk(4096);
    ma_uint64 totalRead = 0;
    while (totalRead < kMaxFrames) {
        ma_uint64 framesToRead = std::min<ma_uint64>(chunk.size(), kMaxFrames - totalRead);
        ma_uint64 framesRead = 0;
        ma_result r = ma_decoder_read_pcm_frames(&dec, chunk.data(), framesToRead, &framesRead);
        if (r != MA_SUCCESS || framesRead == 0) break;
        pcm.insert(pcm.end(), chunk.begin(), chunk.begin() + framesRead);
        totalRead += framesRead;
    }
    ma_decoder_uninit(&dec);

    if (pcm.size() < 2048) {
        return;
    }

    // 1. Real DSP Key Detection (EDMA + Temperley + Krumhansl Chromagram)
    // Works on all audio including EDM Tuned Kicks, 808s, Toms, Piano, Strings, Bass, Vocals
    auto keyRes = ai::KeyDetector::detect(pcm.data(), pcm.size(), sr);
    if (keyRes.confidence >= 0.35f && !keyRes.key.empty()) {
        rec.keyRoot = keyRes.key;
        rec.keyMode = (keyRes.mode == "Minor" || keyRes.mode == "minor") ? "minor" : "major";
        rec.camelot = keyRes.camelot;
    }

    // 2. Real DSP Tempo & BPM Detection (Onset envelope + autocorrelation)
    if (rec.bpm <= 0.0) {
        auto tempoRes = ai::TempoDetector::detect(pcm.data(), pcm.size(), sr);
        if (tempoRes.confidence >= 0.35f && tempoRes.bpm >= 45.0f && tempoRes.bpm <= 220.0f) {
            rec.bpm = tempoRes.bpm;
        }
    }

    // 3. Real DSP Spectral Feature Analysis & Timbre Classification
    auto metrics = ai::FeatureExtractor::computeMetrics(pcm, sr);
    if (rec.genre.empty()) {
        if (metrics.bassRatio > 0.45f && metrics.spectralCentroid < 650.0f) {
            rec.genre = "Bass";
        } else if (metrics.highRatio > 0.40f && metrics.zeroCrossingRate > 0.12f) {
            rec.genre = "Hi-Hat";
        } else if (metrics.bassRatio > 0.30f && metrics.peak > 0.4f && metrics.spectralCentroid < 1200.0f && rec.durationSec < 1.5) {
            rec.genre = "Kick";
        } else if (metrics.highRatio > 0.20f && metrics.zeroCrossingRate > 0.06f && rec.durationSec < 1.8) {
            rec.genre = "Snare";
        }
    }

    // 4. Real CLAP 512-dimensional Acoustic/Semantic Vector Embedding
    outEmbedding = ai::ClapEmbedder::embedAudio(pcm.data(), pcm.size(), sr);
}

} // namespace

bool BackgroundScanner::isSupportedAudioExtension(std::string_view filename) {
    static const std::unordered_set<std::string> kExtensions = {
        "wav", "wave", "mp3", "flac", "ogg", "oga", "aiff", "aif", "wma",
        "m4a", "aac", "opus", "mid", "midi", "w64", "caf", "sfz", "rex", "rx2"
    };

    const size_t dot = filename.find_last_of('.');
    if (dot == std::string::npos || dot + 1 >= filename.size())
        return false;

    const std::string ext = toLower(std::string(filename.substr(dot + 1)));
    return kExtensions.find(ext) != kExtensions.end();
}

BackgroundScanner::BackgroundScanner(db::Database& db)
    : m_db(db) {
}

BackgroundScanner::~BackgroundScanner() {
    cancel();
    waitForCompletion();
}

bool BackgroundScanner::startScan(const std::vector<std::string>& roots, const ScanOptions& options) {
    if (m_isScanning.load()) {
        LOG_WARN(kTag, "Scan already in progress");
        return false;
    }

    waitForCompletion();

    m_isScanning.store(true);
    m_isPaused.store(false);
    m_isCancelled.store(false);
    m_discoveryComplete.store(false);

    {
        const std::lock_guard lock(m_progressMutex);
        m_progress = ScanProgress{};
        m_lastProgressEmit = std::chrono::steady_clock::now();
    }

    {
        const std::lock_guard lock(m_queueMutex);
        std::queue<std::string> emptyQueue;
        std::swap(m_workQueue, emptyQueue);
    }

    setCpuMode(options.cpuMode);
    if (options.throttleSleepMs > 0) {
        m_throttleSleepMs.store(options.throttleSleepMs);
    }

    m_coordinatorThread = std::thread(&BackgroundScanner::coordinatorThreadFunc, this, roots, options);
    return true;
}

void BackgroundScanner::pause() {
    m_isPaused.store(true);
    emitProgress(true);
}

void BackgroundScanner::resume() {
    m_isPaused.store(false);
    m_pauseCv.notify_all();
    emitProgress(true);
}

void BackgroundScanner::cancel() {
    m_isCancelled.store(true);
    m_isPaused.store(false);
    m_pauseCv.notify_all();
    m_queueCv.notify_all();
    emitProgress(true);
}

void BackgroundScanner::waitForCompletion() {
    if (m_coordinatorThread.joinable()) {
        m_coordinatorThread.join();
    }
    for (auto& w : m_workers) {
        if (w.joinable()) {
            w.join();
        }
    }
    m_workers.clear();
}

bool BackgroundScanner::isScanning() const {
    return m_isScanning.load();
}

bool BackgroundScanner::isPaused() const {
    return m_isPaused.load();
}

bool BackgroundScanner::isCancelled() const {
    return m_isCancelled.load();
}

ScanProgress BackgroundScanner::getProgress() const {
    const std::lock_guard lock(m_progressMutex);
    ScanProgress p = m_progress;
    p.isPaused = m_isPaused.load();
    p.isCancelled = m_isCancelled.load();
    p.isComplete = !m_isScanning.load() && m_discoveryComplete.load();
    return p;
}

void BackgroundScanner::setCpuMode(CpuMode mode) {
    m_cpuMode.store(mode);
    const unsigned int hw = std::max(1u, std::thread::hardware_concurrency());
    size_t activeCap = hw;
    int sleepMs = 0;

    switch (mode) {
        case CpuMode::Low: // 30% CPU
            activeCap = std::max(1u, (hw * 30) / 100);
            sleepMs = 30; // 30ms throttle sleep per file
            break;
        case CpuMode::Normal: // 50% CPU
            activeCap = std::max(1u, (hw * 50) / 100);
            sleepMs = 10; // 10ms throttle sleep per file
            break;
        case CpuMode::High: // 85% CPU
        default:
            activeCap = std::max(1u, (hw * 85) / 100);
            sleepMs = 0; // 0ms throttle sleep
            break;
    }

    m_maxActiveWorkers.store(activeCap);
    m_throttleSleepMs.store(sleepMs);

    // Wake up any workers waiting on concurrency slot
    {
        std::lock_guard lock(m_activeWorkersMutex);
        m_activeWorkersCv.notify_all();
    }
}

CpuMode BackgroundScanner::getCpuMode() const {
    return m_cpuMode.load();
}

void BackgroundScanner::setProgressCallback(ProgressCallback cb) {
    const std::lock_guard lock(m_progressMutex);
    m_progressCallback = std::move(cb);
}

void BackgroundScanner::setSampleCallback(SampleCallback cb) {
    const std::lock_guard lock(m_progressMutex);
    m_sampleCallback = std::move(cb);
}

void BackgroundScanner::checkPause() {
    while (m_isPaused.load() && !m_isCancelled.load()) {
        std::unique_lock lock(m_queueMutex);
        m_pauseCv.wait_for(lock, std::chrono::milliseconds(100), [this]() {
            return !m_isPaused.load() || m_isCancelled.load();
        });
    }
}

void BackgroundScanner::emitProgress(bool force) {
    ProgressCallback cb;
    ScanProgress p;
    {
        const std::lock_guard lock(m_progressMutex);
        const auto now = std::chrono::steady_clock::now();
        if (!force && std::chrono::duration_cast<std::chrono::milliseconds>(now - m_lastProgressEmit).count() < 50) {
            return;
        }
        m_lastProgressEmit = now;
        cb = m_progressCallback;
        p = m_progress;
        p.isPaused = m_isPaused.load();
        p.isCancelled = m_isCancelled.load();
        p.isComplete = !m_isScanning.load();
    }

    if (cb) {
        cb(p);
    }
}

void BackgroundScanner::coordinatorThreadFunc(std::vector<std::string> roots, ScanOptions options) {
    try {
        const unsigned int hw = std::max(1u, std::thread::hardware_concurrency());
        size_t numThreads = options.numThreads > 0 ? options.numThreads : hw;

        m_workers.clear();
        m_workers.reserve(numThreads);
        for (size_t i = 0; i < numThreads; ++i) {
            m_workers.emplace_back(&BackgroundScanner::workerThreadFunc, this, options);
        }

        // Phase 1: Directory discovery & filtering
        for (const auto& root : roots) {
            if (m_isCancelled.load())
                break;

            std::error_code ec;
            const auto u8Root = platform::u8path(root);
            if (!fs::exists(u8Root, ec) || !fs::is_directory(u8Root, ec))
                continue;

            fs::recursive_directory_iterator iter(u8Root, fs::directory_options::skip_permission_denied, ec);
            const fs::recursive_directory_iterator end;

            while (iter != end && !m_isCancelled.load()) {
                checkPause();

                try {
                    const auto& entry = *iter;
                    const auto filename = entry.path().filename().string();

                    if (entry.is_directory(ec)) {
                        if (isIgnoredDir(filename)) {
                            iter.disable_recursion_pending();
                        }
                        iter.increment(ec);
                        continue;
                    }

                    if (entry.is_regular_file(ec)) {
                        bool isAudio = false;
                        if (!options.customExtensions.empty()) {
                            const size_t dot = filename.find_last_of('.');
                            if (dot != std::string::npos) {
                                const std::string ext = toLower(filename.substr(dot + 1));
                                for (const auto& custom : options.customExtensions) {
                                    if (toLower(custom) == ext) {
                                        isAudio = true;
                                        break;
                                    }
                                }
                            }
                        } else {
                            isAudio = isSupportedAudioExtension(filename);
                        }

                        if (isAudio) {
                            const std::string pathUtf8 = platform::normalizePath(platform::pathToUtf8(entry.path()));
                            {
                                const std::lock_guard lock(m_queueMutex);
                                m_workQueue.push(pathUtf8);
                            }
                            {
                                const std::lock_guard lock(m_progressMutex);
                                m_progress.totalFiles++;
                            }
                            m_queueCv.notify_one();
                            emitProgress();
                        }
                    }
                } catch (...) {
                }

                iter.increment(ec);
            }
        }
    } catch (const std::exception& ex) {
        LOG_ERROR(kTag, "coordinator exception: " + std::string(ex.what()));
    } catch (...) {
        LOG_ERROR(kTag, "coordinator unknown exception");
    }

    m_discoveryComplete.store(true);
    m_queueCv.notify_all();

    // Wait for all workers to complete
    for (auto& w : m_workers) {
        if (w.joinable()) {
            w.join();
        }
    }
    m_workers.clear();

    m_isScanning.store(false);
    emitProgress(true);
}

void BackgroundScanner::workerThreadFunc(ScanOptions options) {
#ifdef _WIN32
    if (m_cpuMode.load() == CpuMode::Low) {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_LOWEST);
    } else {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
    }
#endif

    while (!m_isCancelled.load()) {
        try {
            checkPause();

            // Dynamic concurrency gate
            {
                std::unique_lock lock(m_activeWorkersMutex);
                m_activeWorkersCv.wait(lock, [this]() {
                    return m_currentActiveWorkers.load() < m_maxActiveWorkers.load() || m_isCancelled.load();
                });
            }

            if (m_isCancelled.load()) break;

            std::string filePath;
            {
                std::unique_lock lock(m_queueMutex);
                m_queueCv.wait(lock, [this]() {
                    return !m_workQueue.empty() || m_discoveryComplete.load() || m_isCancelled.load();
                });

                if (m_isCancelled.load())
                    break;

                if (m_workQueue.empty() && m_discoveryComplete.load())
                    break;

                if (!m_workQueue.empty()) {
                    filePath = std::move(m_workQueue.front());
                    m_workQueue.pop();
                }
            }

            if (filePath.empty())
                continue;

            m_currentActiveWorkers++;

            struct ActiveWorkerScopeGuard {
                std::atomic<size_t>& counter;
                std::condition_variable& cv;
                std::mutex& mtx;
                ~ActiveWorkerScopeGuard() {
                    counter--;
                    std::lock_guard lock(mtx);
                    cv.notify_one();
                }
            } activeGuard{m_currentActiveWorkers, m_activeWorkersCv, m_activeWorkersMutex};

            std::error_code ec;
            const auto u8p = platform::u8path(filePath);
            if (!fs::exists(u8p, ec) || !fs::is_regular_file(u8p, ec)) {
                const std::lock_guard lock(m_progressMutex);
                m_progress.processedFiles++;
                m_progress.errorCount++;
                continue;
            }

            const uint64_t fileSize = fs::file_size(u8p, ec);
            const auto ftime = fs::last_write_time(u8p, ec);
            const int64_t modTime = toUnixEpoch(ftime);
            const std::string filename = platform::pathToUtf8(u8p.filename());

            // Check if existing record in DB is up-to-date
            const auto existing = m_db.getSampleByPath(filePath);
            bool isSkipped = false;
            bool isUpdated = false;

            if (existing.has_value() && !options.forceRescan) {
                if (existing->filesize == fileSize && existing->modifiedTime == modTime) {
                    isSkipped = true;
                }
            }

            if (isSkipped) {
                const std::lock_guard lock(m_progressMutex);
                m_progress.processedFiles++;
                m_progress.skippedCount++;
                m_progress.currentFile = filename;
                emitProgress();
                continue;
            }

            // Calculate fast xxHash64
            const std::string hash = util::Hash::fileXx64Hex(filePath);
            if (hash.empty() && fileSize > 0) {
                const std::lock_guard lock(m_progressMutex);
                m_progress.processedFiles++;
                m_progress.errorCount++;
                continue;
            }

            db::SampleRecord rec;
            if (existing.has_value()) {
                rec = existing.value();
                isUpdated = true;
            }

            rec.path = filePath;
            rec.filename = filename;
            rec.filesize = fileSize;
            rec.modifiedTime = modTime;
            rec.hash = hash;

            std::vector<float> embedding;
            if (options.extractAudioInfo) {
                try {
                    const auto info = audio::Engine::probeFile(filePath);
                    rec.durationSec = info.durationSeconds;
                    rec.sampleRate = info.sampleRate;
                    rec.channels = info.channels;
                    rec.bitDepth = 16; // default standard
                } catch (...) {
                }

                try {
                    analyzeAudioRealWaveform(filePath, rec, embedding);
                } catch (...) {
                }
            }

            parseFilenameMusicMetadata(filename, filePath, rec);

            if (!embedding.empty()) {
                rec.aiAnalyzed = true;
            }

            const int64_t id = m_db.upsertSample(rec);
            if (id > 0) {
                rec.id = id;
                if (!embedding.empty()) {
                    db::AnalysisRecord an;
                    an.sampleId = id;
                    an.tempoConfidence = rec.bpm > 0.0 ? 0.8 : 0.0;
                    an.keyConfidence = !rec.keyRoot.empty() ? 0.8 : 0.0;
                    if (!rec.genre.empty()) an.genreTags.push_back(rec.genre);
                    if (!rec.mood.empty()) an.moodTags.push_back(rec.mood);
                    an.embedding = std::move(embedding);
                    an.analyzedAt = modTime;
                    m_db.updateAnalysis(id, an);
                }

                SampleCallback scb;
                {
                    const std::lock_guard lock(m_progressMutex);
                    m_progress.processedFiles++;
                    if (isUpdated) {
                        m_progress.updatedCount++;
                    } else {
                        m_progress.addedCount++;
                    }
                    m_progress.currentFile = filename;
                    scb = m_sampleCallback;
                }
                if (scb) {
                    try {
                        scb(rec);
                    } catch (...) {
                    }
                }
            } else {
                const std::lock_guard lock(m_progressMutex);
                m_progress.processedFiles++;
                m_progress.errorCount++;
            }

            emitProgress();

            const int curSleep = m_throttleSleepMs.load();
            if (curSleep > 0 && !m_isCancelled.load()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(curSleep));
            }
        } catch (const std::exception& ex) {
            LOG_ERROR(kTag, "worker exception: " + std::string(ex.what()));
            const std::lock_guard lock(m_progressMutex);
            m_progress.processedFiles++;
            m_progress.errorCount++;
        } catch (...) {
            LOG_ERROR(kTag, "worker unknown exception");
            const std::lock_guard lock(m_progressMutex);
            m_progress.processedFiles++;
            m_progress.errorCount++;
        }
    }
}

} // namespace reals::scanner
