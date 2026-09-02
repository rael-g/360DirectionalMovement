#pragma once

#include <stdint.h>

// Wall clock intervals. The graph update carries no timestep reachable from
// here and the hooks do not fire at a fixed rate, so every duration in the
// plugin is measured rather than counted in frames.
//
// Each user keeps its own instance: the interval a caller cares about starts
// when that caller last looked, not when any other did.
struct frame_clock
{
    uint64_t last_tick;
};

// Seconds since the previous call, zero on the first one. A gap longer than a
// plausible frame was a load screen and is charged as a single frame, so time
// spent outside play never lands in one step.
float frame_clock_step(struct frame_clock *clock);

// Makes the next step measure from now, discarding however long the clock has
// been idle.
void frame_clock_restart(struct frame_clock *clock);
