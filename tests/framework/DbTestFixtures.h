#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace reals::test {

namespace fs = std::filesystem;
using json = nlohmann::json;

struct TestSampleRecord {
    int64_t id = 0;
    std::string filePath;
    std::string fileName;
    std::string fileHash;
    uint64_t fileSize = 0;
    int64_t mtime = 0;
    float durationSec = 0.0f;
    float bpm = 0.0f;
    float bpmConfidence = 0.0f;
    std::string keyName;
    std::string camelot;
    std::string openKey;
    float keyConfidence = 0.0f;
    std::vector<std::string> genres;
    std::vector<std::string> moods;
    std::vector<std::string> tags;
    std::vector<float> embedding; // 512-dim vector
    bool isFavorite = false;
};

// In-Memory & File-based Test Database Fixture
class DbTestFixtures {
public:
    explicit DbTestFixtures(std::string dbName = "test_library.db") {
        m_tempDir = fs::temp_directory_path() / ("reals_test_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()));
        fs::create_directories(m_tempDir);
        m_dbPath = (m_tempDir / dbName).string();
    }

    ~DbTestFixtures() {
        cleanup();
    }

    void cleanup() {
        std::error_code ec;
        if (fs::exists(m_tempDir)) {
            fs::remove_all(m_tempDir, ec);
        }
    }

    [[nodiscard]] const std::string& dbPath() const { return m_dbPath; }
    [[nodiscard]] const fs::path& tempDir() const { return m_tempDir; }

    // Encode 512-float vector into byte BLOB
    static std::vector<uint8_t> serializeEmbedding(const std::vector<float>& vec) {
        std::vector<uint8_t> bytes(vec.size() * sizeof(float));
        std::memcpy(bytes.data(), vec.data(), bytes.size());
        return bytes;
    }

    // Decode byte BLOB back to 512-float vector
    static std::vector<float> deserializeEmbedding(const uint8_t* data, size_t byteSize) {
        size_t count = byteSize / sizeof(float);
        std::vector<float> vec(count);
        std::memcpy(vec.data(), data, count * sizeof(float));
        return vec;
    }

    // Generate deterministic 512-dim normalized embedding vector
    static std::vector<float> generateUnitEmbedding(uint32_t seed) {
        std::vector<float> vec(512);
        std::mt19937 rng(seed);
        std::normal_distribution<float> dist(0.0f, 1.0f);

        float normSq = 0.0f;
        for (size_t i = 0; i < 512; ++i) {
            vec[i] = dist(rng);
            normSq += vec[i] * vec[i];
        }

        float invNorm = 1.0f / std::sqrt(normSq);
        for (size_t i = 0; i < 512; ++i) {
            vec[i] *= invNorm;
        }
        return vec;
    }

    // Generate seed dataset of N realistic audio sample records
    static std::vector<TestSampleRecord> generateSampleDataset(size_t count) {
        static const std::vector<std::string> kGenres = {
            "Trap-EDM", "Future Bass", "Tech House", "Minimal Techno", "Hip Hop",
            "Drum & Bass", "Dubstep", "Lo-Fi Hip Hop", "Synthwave", "Ambient",
            "Deep House", "Nu Metal", "Afrobeats", "R&B", "Pop"
        };

        static const std::vector<std::string> kMoods = {
            "dark", "aggressive", "energetic", "relaxed", "happy",
            "sad", "party", "atmospheric", "punchy", "spacey"
        };

        static const std::vector<std::pair<std::string, std::pair<std::string, std::string>>> kKeys = {
            {"C Major", {"8B", "1d"}}, {"A Minor", {"8A", "1m"}},
            {"G Major", {"9B", "2d"}}, {"E Minor", {"9A", "2m"}},
            {"D Major", {"10B", "3d"}}, {"B Minor", {"10A", "3m"}},
            {"F Major", {"7B", "12d"}}, {"D Minor", {"7A", "12m"}},
            {"F# Minor", {"11A", "4m"}}, {"C# Minor", {"12A", "5m"}},
            {"Bb Major", {"6B", "11d"}}, {"Eb Major", {"5B", "10d"}}
        };

        static const std::vector<std::string> kSampleTypes = {
            "Kick", "Snare", "HiHat", "808_Bass", "Vocal_Chant", "Synth_Lead", "Piano_Loop", "Acoustic_Guitar"
        };

        std::vector<TestSampleRecord> records;
        records.reserve(count);

        std::mt19937 rng(1337);
        std::uniform_real_distribution<float> bpmDist(70.0f, 175.0f);
        std::uniform_real_distribution<float> durDist(0.5f, 16.0f);
        std::uniform_int_distribution<size_t> genreDist(0, kGenres.size() - 1);
        std::uniform_int_distribution<size_t> moodDist(0, kMoods.size() - 1);
        std::uniform_int_distribution<size_t> keyDist(0, kKeys.size() - 1);
        std::uniform_int_distribution<size_t> typeDist(0, kSampleTypes.size() - 1);

        for (size_t i = 0; i < count; ++i) {
            TestSampleRecord rec;
            rec.id = static_cast<int64_t>(i + 1);

            const auto& type = kSampleTypes[typeDist(rng)];
            const auto& keyInfo = kKeys[keyDist(rng)];
            const auto& g1 = kGenres[genreDist(rng)];
            const auto& g2 = kGenres[genreDist(rng)];
            const auto& m1 = kMoods[moodDist(rng)];
            const auto& m2 = kMoods[moodDist(rng)];

            rec.fileName = "Sample_" + std::to_string(i + 1) + "_" + type + ".wav";
            rec.filePath = "C:/Samples/Pack_" + std::to_string((i % 5) + 1) + "/" + rec.fileName;

            std::stringstream ss;
            ss << std::hex << (0x1000000000000000ULL + i * 0x9e3779b97f4a7c15ULL);
            rec.fileHash = ss.str();

            rec.fileSize = 100000 + static_cast<uint64_t>(i * 4096);
            rec.mtime = 1700000000 + static_cast<int64_t>(i * 60);
            rec.durationSec = std::round(durDist(rng) * 10.0f) / 10.0f;
            rec.bpm = std::round(bpmDist(rng) * 10.0f) / 10.0f;
            rec.bpmConfidence = 0.95f;
            rec.keyName = keyInfo.first;
            rec.camelot = keyInfo.second.first;
            rec.openKey = keyInfo.second.second;
            rec.keyConfidence = 0.92f;

            rec.genres = { g1 };
            if (g2 != g1) rec.genres.push_back(g2);

            rec.moods = { m1 };
            if (m2 != m1) rec.moods.push_back(m2);

            rec.tags = { type, g1, m1 };
            rec.embedding = generateUnitEmbedding(static_cast<uint32_t>(i + 100));
            rec.isFavorite = (i % 7 == 0);

            records.push_back(std::move(rec));
        }

        return records;
    }

    // In-memory Database Store simulator providing real SQLite table & query behavior
    class MockDbStore {
    public:
        void insert(const TestSampleRecord& rec) {
            std::lock_guard lock(m_mutex);
            m_records[rec.filePath] = rec;
        }

        [[nodiscard]] size_t count() const {
            std::lock_guard lock(m_mutex);
            return m_records.size();
        }

        [[nodiscard]] const TestSampleRecord* getByPath(const std::string& path) const {
            std::lock_guard lock(m_mutex);
            auto it = m_records.find(path);
            if (it != m_records.end()) return &it->second;
            return nullptr;
        }

        [[nodiscard]] std::vector<TestSampleRecord> queryByFilter(
            const std::string& tag, float minBpm, float maxBpm, const std::string& key, bool onlyFav) const {

            std::lock_guard lock(m_mutex);
            std::vector<TestSampleRecord> result;
            for (const auto& [path, r] : m_records) {
                if (onlyFav && !r.isFavorite) continue;
                if (minBpm > 0.0f && r.bpm < minBpm) continue;
                if (maxBpm > 0.0f && r.bpm > maxBpm) continue;
                if (!key.empty() && r.keyName != key && r.camelot != key && r.openKey != key) continue;

                if (!tag.empty()) {
                    bool hasTag = false;
                    for (const auto& t : r.tags) {
                        if (t == tag) { hasTag = true; break; }
                    }
                    for (const auto& g : r.genres) {
                        if (g == tag) { hasTag = true; break; }
                    }
                    for (const auto& m : r.moods) {
                        if (m == tag) { hasTag = true; break; }
                    }
                    if (!hasTag) continue;
                }

                result.push_back(r);
            }
            return result;
        }

    private:
        mutable std::mutex m_mutex;
        std::unordered_map<std::string, TestSampleRecord> m_records;
    };

private:
    fs::path m_tempDir;
    std::string m_dbPath;
};

} // namespace reals::test
