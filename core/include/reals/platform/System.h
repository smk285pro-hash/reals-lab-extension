#pragma once

// Cross-platform system-level helpers (thread priority, debug console).
// core/ modules MUST use these instead of calling Win32 APIs directly
// (AGENTS.md architecture rules, MAJ-01).
#include <string_view>

namespace reals::platform {

enum class ThreadPriority {
    Lowest,       // background work the user should never feel
    BelowNormal,  // default for scanner/worker threads
    Normal,
};

// Set the priority of the CALLING thread. Safe no-op on platforms without a
// usable per-thread priority knob (returns false).
bool setThreadPriority(ThreadPriority priority);

// Write a line to the OS debug console (OutputDebugStringA on Windows,
// no-op elsewhere — stdout logging already happens in util::Log).
void debugOutput(std::string_view message);

} // namespace reals::platform
