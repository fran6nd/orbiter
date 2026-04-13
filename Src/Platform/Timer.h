// Copyright (c) Martin Schweiger
// Licensed under the MIT License

// ============================================================================
// Timer.h
// High-resolution timer using std::chrono — replaces the Windows
// QueryPerformanceCounter / LARGE_INTEGER pattern in Orbiter.cpp.
//
// This header is safe to include on all platforms.  On Windows it provides
// the same precision as QueryPerformanceCounter (std::chrono::high_resolution
// _clock maps to QPC internally on MSVC).
// ============================================================================

#ifndef ORBITER_TIMER_H
#define ORBITER_TIMER_H

#include <chrono>

namespace orbiter {

class HighResTimer {
public:
    using clock     = std::chrono::high_resolution_clock;
    using time_point = clock::time_point;

    // Returns the timer resolution (seconds per tick) — analogous to
    // 1.0 / QueryPerformanceFrequency().
    static double resolution() noexcept {
        // std::chrono guarantees at least nanosecond period on modern
        // platforms; report the actual ratio.
        using ratio = clock::period;
        return static_cast<double>(ratio::num) / static_cast<double>(ratio::den);
    }

    // Capture the current time.
    static time_point now() noexcept { return clock::now(); }

    // Elapsed seconds between two captured time points.
    static double elapsed(time_point from, time_point to) noexcept {
        return std::chrono::duration<double>(to - from).count();
    }

    // Elapsed seconds from a captured point to now.
    static double since(time_point from) noexcept {
        return elapsed(from, now());
    }
};

} // namespace orbiter

#endif // ORBITER_TIMER_H
