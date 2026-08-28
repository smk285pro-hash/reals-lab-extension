#pragma once

#include <string>
#include <string_view>

namespace reals::util {

enum class LogLevel { Trace, Debug, Info, Warn, Error };

// Minimal leveled logger writing to console + rotating file in the data dir.
// Thread-safe. Usage: LOG_INFO("browser", "loaded {} entries", count);
class Log {
public:
    static void init(const std::string& filePath, LogLevel minLevel = LogLevel::Info);
    static void write(LogLevel level, std::string_view tag, std::string_view message);

    static void setMinLevel(LogLevel level);
};

} // namespace reals::util

#define LOG_TRACE(tag, msg) ::reals::util::Log::write(::reals::util::LogLevel::Trace, tag, msg)
#define LOG_DEBUG(tag, msg) ::reals::util::Log::write(::reals::util::LogLevel::Debug, tag, msg)
#define LOG_INFO(tag, msg)  ::reals::util::Log::write(::reals::util::LogLevel::Info, tag, msg)
#define LOG_WARN(tag, msg)  ::reals::util::Log::write(::reals::util::LogLevel::Warn, tag, msg)
#define LOG_ERROR(tag, msg) ::reals::util::Log::write(::reals::util::LogLevel::Error, tag, msg)
