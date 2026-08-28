#include "reals/config/Config.h"

#include "reals/platform/Path.h"
#include "reals/util/Log.h"

#include <fstream>

namespace reals::config {

namespace {
constexpr auto kTag = "config";
constexpr const char* kDefaults = R"({
  "language": "vi",
  "navPosition": "top",
  "accent": "orange",
  "noiseOverlay": true,
  "apiBaseUrl": "https://reals.media",
  "labApiBaseUrl": "https://smk285pro--ai-audio-lab-fastapi-web.modal.run",
  "authToken": ""
})";
} // namespace

Config& Config::instance() {
    static Config inst;
    return inst;
}

void Config::load() {
    const std::lock_guard lock(m_mutex);
    m_filePath = platform::joinPath(platform::dataDir(), "config.json");

    m_data = nlohmann::json::parse(kDefaults, nullptr, false);
    if (m_data.is_discarded()) {
        LOG_ERROR(kTag, "internal defaults corrupted");
        m_data = nlohmann::json::object();
    }

    std::ifstream in(platform::u8path(m_filePath)); // MAJ-01: platform layer, not a local Win32 helper
    if (in) {
        try {
            nlohmann::json stored;
            in >> stored;
            if (stored.is_object())
                m_data.update(stored);
        } catch (const std::exception&) {
            LOG_WARN(kTag, "config parse failed, using defaults");
        }
    }
    platform::ensureDir(platform::dataDir());
    save();
}

void Config::save() {
    const std::lock_guard lock(m_mutex);
    std::ofstream out(platform::u8path(m_filePath)); // MAJ-01: platform layer
    if (out)
        out << m_data.dump(2);
    else
        LOG_ERROR(kTag, "cannot write config file");
}

std::string Config::getString(std::string_view key, std::string_view fallback) const {
    const std::lock_guard lock(m_mutex);
    if (const auto it = m_data.find(key); it != m_data.end() && it->is_string())
        return it->get<std::string>();
    return std::string(fallback);
}

bool Config::getBool(std::string_view key, bool fallback) const {
    const std::lock_guard lock(m_mutex);
    if (const auto it = m_data.find(key); it != m_data.end() && it->is_boolean())
        return it->get<bool>();
    return fallback;
}

int Config::getInt(std::string_view key, int fallback) const {
    const std::lock_guard lock(m_mutex);
    if (const auto it = m_data.find(key); it != m_data.end() && it->is_number_integer())
        return it->get<int>();
    return fallback;
}

void Config::set(std::string_view key, const nlohmann::json& value) {
    {
        const std::lock_guard lock(m_mutex);
        m_data[key] = value;
    }
    save();
}

std::string Config::language() const { return getString("language", "vi"); }
std::string Config::navPosition() const { return getString("navPosition", "top"); }
std::string Config::accent() const { return getString("accent", "orange"); }
bool Config::noiseOverlay() const { return getBool("noiseOverlay", true); }
std::string Config::apiBaseUrl() const { return getString("apiBaseUrl", "https://reals.media"); }
std::string Config::labApiBaseUrl() const {
    return getString("labApiBaseUrl",
                     "https://smk285pro--ai-audio-lab-fastapi-web.modal.run");
}
std::string Config::authToken() const { return getString("authToken", ""); }

} // namespace reals::config
