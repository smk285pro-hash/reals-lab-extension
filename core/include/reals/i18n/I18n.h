#pragma once

// Runtime UI translation. All user-facing strings go through tr("key").
// Tables live in assets/i18n/strings_vi.json and strings_en.json.
#include <string>
#include <string_view>

namespace reals::i18n {

// Load tables from the given directory (expects strings_vi.json / strings_en.json).
void init(std::string_view assetsDir);

// Switch language at runtime: "vi" or "en".
void setLanguage(std::string_view lang);

[[nodiscard]] std::string currentLanguage();

// Translate a key. Falls back to English, then to the key itself.
[[nodiscard]] std::string tr(std::string_view key);

} // namespace reals::i18n
