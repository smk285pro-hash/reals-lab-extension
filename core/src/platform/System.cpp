#include "reals/platform/System.h"

#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <pthread.h>
#endif

namespace reals::platform {

bool setThreadPriority(const ThreadPriority priority) {
#ifdef _WIN32
    int winPrio = THREAD_PRIORITY_NORMAL;
    switch (priority) {
    case ThreadPriority::Lowest:       winPrio = THREAD_PRIORITY_LOWEST; break;
    case ThreadPriority::BelowNormal:  winPrio = THREAD_PRIORITY_BELOW_NORMAL; break;
    case ThreadPriority::Normal:       winPrio = THREAD_PRIORITY_NORMAL; break;
    }
    return SetThreadPriority(GetCurrentThread(), winPrio) != 0;
#else
    // POSIX has no portable "below normal" priority knob that works without
    // privileges. SCHED_BATCH is the standard hint for CPU-bound background
    // work (the scheduler deprioritizes batch threads under interactive load),
    // which matches the scanner's use case.
    sched_param param{};
    param.sched_priority = 0;
    const int rc = pthread_setschedparam(pthread_self(), SCHED_BATCH, &param);
    if (rc == 0)
        return true;
    // Fall back to a plain no-op success for the Normal case: leaving the
    // default scheduling policy IS "normal priority".
    return priority == ThreadPriority::Normal;
#endif
}

void debugOutput(const std::string_view message) {
#ifdef _WIN32
    OutputDebugStringA(std::string(message).c_str());
#else
    (void)message; // stdout logging already covers non-Windows platforms
#endif
}

} // namespace reals::platform
