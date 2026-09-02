#pragma once

#include <stdint.h>

// Wall clock intervals, since the hooks do not fire at a fixed rate and the
// graph update carries no timestep reachable from here.
//
// Each user keeps its own instance: an interval starts when that caller last
// looked, not when any other did.
struct frame_clock
{
    uint64_t last_tick;
};

// Seconds since the previous call, zero on the first one. A gap longer than a
// plausible frame is charged as a single frame.
float frame_clock_step(struct frame_clock *clock);

// Makes the next step measure from now, discarding however long the clock has
// been idle.
void frame_clock_restart(struct frame_clock *clock);
