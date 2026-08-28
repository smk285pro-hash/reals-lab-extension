#include "reals/util/Log.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>
#ifdef _WIN32
#include <windows.h>
#endif

namespace reals::util {

namespace {
std::mutex g_mutex;
FILE* g_file = nullptr;
LogLevel g_minLevel = LogLevel::Trace;

const char* levelName(LogLevel level) {
    switch (level) {
    case LogLevel::Trace: return "TRACE";
    case LogLevel::Debug: return "DEBUG";
    case LogLevel::Info:  return "INFO ";
    case LogLevel::Warn:  return "WARN ";
    case LogLevel::Error: return "ERROR";
    }
    return "?????";
}
} // namespace

void Log::init(const std::string& filePath, LogLevel minLevel) {
    std::lock_guard lock(g_mutex);
    g_minLevel = minLevel;
    if (g_file) {
        std::fclose(g_file);
        g_file = nullptr;
    }
    if (!filePath.empty()) {
#ifdef _WIN32
        int n = MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, nullptr, 0);
        std::wstring w(static_cast<size_t>(n), L'\0');
        MultiByteToWideChar(CP_UTF8, 0, filePath.c_str(), -1, w.data(), n);
        _wfopen_s(&g_file, w.c_str(), L"a");
#else
        g_file = std::fopen(filePath.c_str(), "a");
#endif
    }
}

void Log::setMinLevel(LogLevel level) {
    std::lock_guard lock(g_mutex);
    g_minLevel = level;
}

void Log::write(LogLevel level, std::string_view tag, std::string_view message) {
    std::lock_guard lock(g_mutex);
    if (level < g_minLevel)
        return;

    const auto now = std::chrono::system_clock::now();
    const std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%H:%M:%S", &tm);

    char lineBuf[2048];
    std::snprintf(lineBuf, sizeof(lineBuf), "[%s] [%s] [%.*s] %.*s\n",
                  timeBuf, levelName(level),
                  static_cast<int>(tag.size()), tag.data(),
                  static_cast<int>(message.size()), message.data());

    std::printf("%s", lineBuf);
#ifdef _WIN32
    OutputDebugStringA(lineBuf);
#endif

    if (g_file) {
        char dateBuf[16];
        std::strftime(dateBuf, sizeof(dateBuf), "%Y-%m-%d", &tm);
        std::fprintf(g_file, "[%s %s] [%s] [%.*s] %.*s\n", dateBuf, timeBuf,
                     levelName(level), static_cast<int>(tag.size()), tag.data(),
                     static_cast<int>(message.size()), message.data());
        std::fflush(g_file);
    }
}

} // namespace reals::util

