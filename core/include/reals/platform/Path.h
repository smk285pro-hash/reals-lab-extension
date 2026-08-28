#pragma once

// Cross-platform filesystem/path helpers. All path handling in the project
// MUST go through this namespace (see AGENTS.md architecture rules).
#include <filesystem>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace reals::platform {

// OS-specific application data dir:
//   Windows: %APPDATA%/RealsLab
//   macOS:   ~/Library/Application Support/RealsLab
//   Linux:   /RealsLab or ~/.config/RealsLab
[[nodiscard]] std::string dataDir();

// OS-specific temporary directory (%TEMP%, /tmp, etc.)
[[nodiscard]] std::string tempDir();

// User music/samples default dir (best effort, may be empty).
[[nodiscard]] std::string defaultMusicDir();

// Native path join with the OS separator.
[[nodiscard]] std::string joinPath(std::string_view a, std::string_view b);

template<typename... Args>
[[nodiscard]] inline std::string joinPath(std::string_view a, std::string_view b, Args&&... rest) {
    std::string result = joinPath(a, b);
    ((result = joinPath(result, std::forward<Args>(rest))), ...);
    return result;
}

// Normalize separators to the current platform.
[[nodiscard]] std::string normalizePath(std::string_view path);

// UTF-8 <-> native filesystem path. On Windows this uses UTF-16, never ACP.
[[nodiscard]] std::filesystem::path u8path(std::string_view utf8);
[[nodiscard]] std::string pathToUtf8(const std::filesystem::path& p);

// Ensure a directory exists (recursive). Returns false on failure.
bool ensureDir(std::string_view path);

// List immediate subdirectories of a directory (names only, UTF-8).
[[nodiscard]] std::vector<std::string> listSubdirs(std::string_view dir);

// List immediate files of a directory (names only, UTF-8).
[[nodiscard]] std::vector<std::string> listFiles(std::string_view dir);

} // namespace reals::platform