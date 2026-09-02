#include "frame_clock.h"

#include <windows.h>

#define LONGEST_FRAME 0.1f

static double seconds_per_tick(void)
{
    static double cached = 0.0;
    if (cached == 0.0) {
        LARGE_INTEGER frequency;
        // Measuring at the wrong scale beats not turning at all.
        if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart == 0)
            return 0.0;
        cached = 1.0 / (double)frequency.QuadPart;
    }
    return cached;
}

float frame_clock_step(struct frame_clock *clock)
{
    const double scale = seconds_per_tick();
    if (scale == 0.0) return LONGEST_FRAME;

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);

    const uint64_t previous = clock->last_tick;
    clock->last_tick = (uint64_t)now.QuadPart;
    if (previous == 0) return 0.0f;

    const float dt = (float)((double)(clock->last_tick - previous) * scale);
    if (dt <= 0.0f) return 0.0f;
    return dt < LONGEST_FRAME ? dt : LONGEST_FRAME;
}

void frame_clock_restart(struct frame_clock *clock)
{
    clock->last_tick = 0;
}
