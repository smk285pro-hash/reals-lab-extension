#include "reals/util/Log.h"

#include "reals/platform/Path.h"
#include "reals/platform/System.h"

#include <chrono>
#include <cstdio>
#include <ctime>
#include <mutex>

#include <deque>
#include <vector>

namespace reals::util {

namespace {
std::mutex g_mutex;
FILE* g_file = nullptr;
LogLevel g_minLevel = LogLevel::Trace;
std::deque<std::string> g_recentLogs;
constexpr size_t kMaxRecentLogs = 500;

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
        g_file = platform::openAppend(filePath);
        if (!g_file) {
            // If primary log is locked by another process, fallback to active log
            const std::string fallback = filePath + ".active.log";
            g_file = platform::openAppend(fallback);
        }
    }
}

void Log::setMinLevel(LogLevel level) {
    std::lock_guard lock(g_mutex);
    g_minLevel = level;
}

std::vector<std::string> Log::recentLogs(const size_t maxCount) {
    std::lock_guard lock(g_mutex);
    const size_t count = std::min(maxCount, g_recentLogs.size());
    std::vector<std::string> result;
    result.reserve(count);
    auto it = g_recentLogs.end() - count;
    while (it != g_recentLogs.end()) {
        result.push_back(*it++);
    }
    return result;
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
    std::snprintf(lineBuf, sizeof(lineBuf), "[%s] [%s] [%.*s] %.*s",
                  timeBuf, levelName(level),
                  static_cast<int>(tag.size()), tag.data(),
                  static_cast<int>(message.size()), message.data());

    // In-memory ring buffer
    g_recentLogs.emplace_back(lineBuf);
    while (g_recentLogs.size() > kMaxRecentLogs) {
        g_recentLogs.pop_front();
    }

    std::printf("%s\n", lineBuf);
    platform::debugOutput(std::string(lineBuf) + "\n");

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

