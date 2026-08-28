#pragma once

// Application configuration persisted as JSON in platform::dataDir().
#include <mutex>
#include <string>

#include <nlohmann/json.hpp>

namespace reals::config {

class Config {
public:
    static Config& instance();

    // Load from disk (or create defaults). Safe to call multiple times.
    void load();
    void save();

    [[nodiscard]] std::string getString(std::string_view key, std::string_view fallback = "") const;
    [[nodiscard]] bool getBool(std::string_view key, bool fallback = false) const;
    [[nodiscard]] int getInt(std::string_view key, int fallback = 0) const;

    void set(std::string_view key, const nlohmann::json& value);

    // Convenience typed accessors used across the app.
    [[nodiscard]] std::string language() const;    // "vi" | "en"
    [[nodiscard]] std::string navPosition() const; // top|bottom|left|right
    [[nodiscard]] std::string accent() const;      // orange|amber|muted|gray
    [[nodiscard]] bool noiseOverlay() const;
    [[nodiscard]] std::string apiBaseUrl() const;
    [[nodiscard]] std::string labApiBaseUrl() const;
    [[nodiscard]] std::string authToken() const;

private:
    Config() = default;

    // recursive_mutex: load()/set() hold the lock and call save() which locks
    // again on the same thread (plain mutex would throw resource_deadlock).
    mutable std::recursive_mutex m_mutex;
    nlohmann::json m_data = nlohmann::json::object();
    std::string m_filePath;
};

} // namespace reals::config
