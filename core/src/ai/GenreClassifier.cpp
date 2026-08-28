#include "reals/ai/GenreClassifier.h"
#include "reals/ai/FeatureExtractor.h"
#include "reals/ai/ModelManager.h"
#include "reals/ai/OnnxEngine.h"
#include "reals/ai/TempoDetector.h"

#include <algorithm>
#include <cmath>

namespace reals::ai {

namespace {

const std::vector<std::string> kDiscogsTaxonomy = {
    // Electronic / Dance
    "House", "Tech House", "Deep House", "Progressive House", "Electro House", "Future House", "Bass House",
    "Acid House", "Tropical House", "Minimal House", "Melodic House", "Afro House", "G-House", "Hard House",
    "Techno", "Minimal Techno", "Peak Time Techno", "Raw Deep Techno", "Acid Techno", "Industrial Techno",
    "Hard Techno", "Dub Techno", "Detroit Techno", "Melodic Techno", "Dark Techno", "Trance", "Psy-Trance",
    "Goa Trance", "Progressive Trance", "Uplifting Trance", "Tech Trance", "Hard Trance", "Vocal Trance",
    "Drum and Bass", "Liquid DnB", "Neurofunk", "Jump Up", "Jungle", "Halftime", "Crossbreed", "Ragga Jungle",
    "Dubstep", "Brostep", "Riddim", "Deep Dubstep", "Melodic Dubstep", "Deathstep", "Trap-EDM", "Future Bass",
    "Midtempo", "Hardstyle", "Rawstyle", "Hardcore", "Frenchcore", "Happy Hardcore", "Gabber", "Speedcore",
    "Breakbeat", "Big Beat", "Nu Skool Breaks", "Electro", "IDM", "Glitch", "Ambient", "Drone", "Dark Ambient",
    "Downtempo", "Trip Hop", "Chillout", "Lounge", "Synthwave", "Darksynth", "Retrowave", "Vaporwave", "Future Funk",
    "Eurodance", "Hi-NRG", "Italo Disco", "Nu Disco", "Space Disco", "Chiptune", "Hyperpop", "Garage", "UK Garage",
    "Speed Garage", "2-Step", "Bassline", "Grime", "Footwork", "Juke", "Jersey Club", "Baile Funk", "Moombahton",
    "Electro Swing", "Witch House", "Synthpop", "Futurepop", "EBM", "Darkwave", "Industrial", "Noise", "Harsh Noise",

    // Hip Hop / Urban
    "Hip Hop", "Boom Bap", "Trap", "East Coast Hip Hop", "West Coast Hip Hop", "Southern Hip Hop", "Dirty South",
    "Cloud Rap", "Lo-Fi Hip Hop", "Jazz Rap", "Conscious Hip Hop", "Hardcore Hip Hop", "Gangsta Rap", "G-Funk",
    "Drill", "UK Drill", "NY Drill", "Phonk", "Drift Phonk", "Memphis Rap", "Emo Rap", "SoundCloud Rap", "Pop Rap",
    "Alternative Hip Hop", "Underground Hip Hop", "Instrumental Hip Hop", "Turntablism", "Crunk", "Bounce",
    "R&B", "Contemporary R&B", "Contemporary Urban", "Neo Soul", "Quiet Storm", "Soul", "Northern Soul", "Southern Soul",
    "Funk", "P-Funk", "Boogie", "Afrobeats", "Afropop", "Amapiano", "Dancehall", "Reggaeton", "Moombahcore",

    // Rock / Metal
    "Rock", "Classic Rock", "Hard Rock", "Psychedelic Rock", "Progressive Rock", "Art Rock", "Glam Rock",
    "Soft Rock", "Pop Rock", "Arena Rock", "Garage Rock", "Pub Rock", "Blues Rock", "Southern Rock", "Folk Rock",
    "Country Rock", "Alternative Rock", "Indie Rock", "Grunge", "Post-Grunge", "Britpop", "Shoegaze", "Dream Pop",
    "Post-Rock", "Math Rock", "Noise Rock", "Stoner Rock", "Desert Rock", "Krautrock", "Space Rock", "Gothic Rock",
    "Punk", "Pop Punk", "Hardcore Punk", "Post-Punk", "Skate Punk", "Melodic Hardcore", "Crust Punk", "Emo",
    "Midwest Emo", "Screamo", "Post-Hardcore", "Metalcore", "Deathcore", "Mathcore", "Melodic Metalcore",
    "Heavy Metal", "Thrash Metal", "Death Metal", "Black Metal", "Power Metal", "Doom Metal", "Sludge Metal",
    "Stoner Metal", "Symphonic Metal", "Gothic Metal", "Folk Metal", "Industrial Metal", "Nu Metal", "Groove Metal",
    "Progressive Metal", "Djent", "Technical Death Metal", "Melodic Death Metal", "Atmospheric Black Metal", "Grindcore",

    // Pop / Folk / World / Acoustic
    "Pop", "Dance Pop", "Electropop", "Indie Pop", "Art Pop", "Baroque Pop", "Bubblegum Pop", "Teen Pop", "K-Pop",
    "J-Pop", "C-Pop", "V-Pop", "Latin Pop", "Europop", "Singer-Songwriter", "Acoustic Pop", "Folk", "Contemporary Folk",
    "Indie Folk", "Freak Folk", "Neofolk", "Traditional Folk", "Bluegrass", "Country", "Contemporary Country",
    "Outlaw Country", "Americana", "Alt-Country", "Blues", "Delta Blues", "Chicago Blues", "Electric Blues",
    "Texas Blues", "Jump Blues", "Jazz", "Traditional Jazz", "Dixieland", "Swing", "Bebop", "Hard Bop", "Cool Jazz",
    "Modal Jazz", "Free Jazz", "Avant-Garde Jazz", "Fusion", "Jazz-Funk", "Smooth Jazz", "Acid Jazz", "Nu Jazz",
    "Latin Jazz", "Bossa Nova", "Samba", "Tango", "Salsa", "Bachata", "Merengue", "Cumbia", "Flamenco", "Mariachi",
    "Reggae", "Roots Reggae", "Dub", "Ska", "Rocksteady", "Ska Punk", "Calypso", "Soca", "Zouk", "Kizomba",

    // Classical / Cinematic / Experimental
    "Classical", "Early Music", "Medieval", "Renaissance", "Baroque", "Classical Period", "Romantic", "Modern Classical",
    "Contemporary Classical", "Minimalism", "Post-Minimalism", "Neoclassical", "Opera", "Choral", "Chamber Music",
    "Symphonic", "Cinematic", "Film Score", "Trailer Music", "Epic Orchestral", "Video Game Music", "Library Music",
    "Production Music", "Soundtrack", "Easy Listening", "Musique Concrete", "Field Recordings", "Spoken Word",
    "Poetry", "Comedy", "Radioplay", "Audiobook", "Holiday", "Children's", "Religious", "Gospel", "Christian Rock",
    "Gregorian Chant", "Sufi", "Qawwali", "Carnatic", "Hindustani", "Raga", "Gamelan", "Celtic", "Nordic Folk",
    "Tuvan Throat Singing", "Polka", "Schlager", "Chanson", "Enka", "Trot", "Dangdut", "Luk Thung", "Morlam",
    "Taarab", "Highlife", "Juju", "Soukous", "Mbalax", "Gqom", "Kwaito", "Gengetone", "Bongo Flava", "Sertanejo",
    "Forro", "Axé", "MPB", "Tropicalia", "Pagode", "Choro", "Bolero", "Ranchera", "Norteño", "Corrido", "Banda",
    "Son Cubano", "Mambo", "Cha-Cha-Cha", "Guaracha", "Boogaloo", "Reggae En Español", "Latin Trap", "Urban Latin",
    "Dubstep VIP", "Melodic Techno & House", "Dark Disco", "Afro House & Amapiano", "UK Funky",
    "Gqom Electronic", "Footwork Jungle", "Wonky", "Future Garage", "Glitch Hop",
    "Trap Metal", "Rage Trap", "Jersey Drill", "Cloud Trap", "Screamo Rap",
    "Mathcore Metal", "Post-Black Metal", "Blackgaze", "Deathgrind", "Symphonic Black Metal",
    "Math Rock Acoustic", "Indie Surf Rock", "Midwest Emo Revival", "Neo-Psychedelia", "Desert Blues",
    "Tropicalia Acoustic", "Bossa Nova Jazz", "Flamenco Nuevo", "Gipsy Jazz", "Afrobeat Modern",
    "Chamber Pop", "Dark Ambient Drone", "Modular Synth Soundscape"
};

} // namespace

const std::vector<std::string>& GenreClassifier::getTaxonomy() {
    return kDiscogsTaxonomy;
}

std::vector<GenreResult> GenreClassifier::classify(
    const float* pcm, size_t frames, int sampleRate, int topK) {
    if (!pcm || frames == 0 || sampleRate <= 0 || topK <= 0) {
        return {};
    }

    // 1. Resample to standard 44.1kHz mono
    constexpr int kTargetRate = 44100;
    auto audio = FeatureExtractor::resampleMono(pcm, frames, 1, sampleRate, kTargetRate);
    if (audio.empty()) {
        return {};
    }

    // Check if ONNX model is ready
    if (ModelManager::instance().isModelAvailable("discogs_maest") &&
        ModelManager::instance().ensureModelLoaded("discogs_maest")) {
        // Run neural inference
        SpectrogramConfig cfg;
        cfg.sampleRate = kTargetRate;
        cfg.nFft = 2048;
        cfg.hopLength = 512;
        cfg.nMels = 128;
        auto logMel = FeatureExtractor::computeLogMel(audio, cfg);

        if (!logMel.empty()) {
            std::vector<float> inputFlat;
            inputFlat.reserve(logMel.size() * 128);
            for (const auto& row : logMel) {
                inputFlat.insert(inputFlat.end(), row.begin(), row.end());
            }

            std::vector<float> outputProbs;
            const std::vector<int64_t> shape = {1, static_cast<int64_t>(logMel.size()), 128};
            if (OnnxEngine::instance().runSingle("discogs_maest", inputFlat, shape, outputProbs) &&
                outputProbs.size() >= kDiscogsTaxonomy.size()) {
                std::vector<GenreResult> results;
                for (size_t i = 0; i < kDiscogsTaxonomy.size(); ++i) {
                    results.push_back({kDiscogsTaxonomy[i], outputProbs[i]});
                }
                std::sort(results.begin(), results.end(), [](const GenreResult& a, const GenreResult& b) {
                    return a.score > b.score;
                });
                if (static_cast<int>(results.size()) > topK) {
                    results.resize(topK);
                }
                return results;
            }
        }
    }

    // 2. Algorithmic spectral signature classifier fallback
    auto metrics = FeatureExtractor::computeMetrics(audio, kTargetRate);
    auto tempoRes = TempoDetector::detectAlgorithmic(pcm, frames, sampleRate);
    const float bpm = tempoRes.bpm;

    std::vector<GenreResult> scored;
    scored.reserve(kDiscogsTaxonomy.size());

    for (const auto& genre : kDiscogsTaxonomy) {
        float score = 0.10f; // base prior

        // Electronic / Dance styles
        if (genre == "Trap-EDM" || genre == "Trap") {
            if (bpm >= 135.0f && bpm <= 165.0f) score += 0.35f;
            if (metrics.bassRatio > 0.30f) score += 0.25f;
            if (metrics.zeroCrossingRate > 0.05f) score += 0.15f;
        } else if (genre == "House" || genre == "Deep House" || genre == "Tech House") {
            if (bpm >= 118.0f && bpm <= 128.0f) score += 0.40f;
            if (metrics.bassRatio > 0.25f) score += 0.20f;
        } else if (genre == "Techno" || genre == "Peak Time Techno") {
            if (bpm >= 126.0f && bpm <= 142.0f) score += 0.40f;
            if (metrics.spectralCentroid > 1500.0f) score += 0.20f;
        } else if (genre == "Drum and Bass" || genre == "Liquid DnB" || genre == "Neurofunk") {
            if (bpm >= 165.0f && bpm <= 180.0f) score += 0.55f;
            if (metrics.bassRatio > 0.25f) score += 0.20f;
        } else if (genre == "Dubstep" || genre == "Brostep") {
            if (bpm >= 138.0f && bpm <= 152.0f) score += 0.40f;
            if (metrics.bassRatio > 0.35f) score += 0.25f;
        } else if (genre == "Future Bass") {
            if (bpm >= 140.0f && bpm <= 160.0f) score += 0.35f;
            if (metrics.highRatio > 0.20f) score += 0.20f;
        } else if (genre == "Lo-Fi Hip Hop" || genre == "Downtempo" || genre == "Chillout") {
            if (bpm >= 70.0f && bpm <= 95.0f) score += 0.40f;
            if (metrics.spectralCentroid < 2000.0f) score += 0.25f;
        } else if (genre == "Synthwave" || genre == "Retrowave") {
            if (bpm >= 100.0f && bpm <= 125.0f) score += 0.30f;
            if (metrics.highRatio > 0.25f) score += 0.25f;
        } else if (genre == "Ambient" || genre == "Drone" || genre == "Cinematic") {
            if (metrics.rms < 0.15f) score += 0.30f;
            if (metrics.zeroCrossingRate < 0.04f) score += 0.25f;
        } else if (genre == "Nu Metal" || genre == "Hard Rock" || genre == "Heavy Metal") {
            if (metrics.zeroCrossingRate > 0.08f) score += 0.35f;
            if (metrics.rms > 0.20f) score += 0.20f;
        } else if (genre == "Acoustic Pop" || genre == "Singer-Songwriter" || genre == "Folk") {
            if (metrics.bassRatio < 0.20f) score += 0.25f;
            if (metrics.spectralCentroid >= 1000.0f && metrics.spectralCentroid <= 3000.0f) score += 0.25f;
        } else if (genre == "Boom Bap" || genre == "Hip Hop") {
            if (bpm >= 85.0f && bpm <= 100.0f) score += 0.35f;
            if (metrics.bassRatio > 0.25f) score += 0.20f;
        } else {
            // General heuristic scoring
            if (bpm >= 120.0f && bpm <= 130.0f) score += 0.05f;
        }

        score = std::clamp(score, 0.01f, 0.95f);
        scored.push_back({genre, score});
    }

    std::sort(scored.begin(), scored.end(), [](const GenreResult& a, const GenreResult& b) {
        return a.score > b.score;
    });

    if (static_cast<int>(scored.size()) > topK) {
        scored.resize(topK);
    }

    return scored;
}

} // namespace reals::ai
