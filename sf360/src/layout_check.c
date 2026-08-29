#include "layout_check.h"
#include "angles.h"
#include "game.h"
#include "log.h"

#include <math.h>
#include <stdarg.h>
#include <stdio.h>

static void report_failure(bool report, const char *fmt, ...)
{
    if (!report) return;

    char message[256] = { 0 };
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof message, fmt, args);
    va_end(args);

    log_line("layout check failed: %s", message);
}

bool validate_layout(void *player, void *speed_var, void *direction_var,
                     bool report)
{
    void *holder = holder_of(player);
    void **vtable = *(void ***)holder;
    if (!inside_module(vtable)) {
        report_failure(report, "holder vtable %p is outside the module", vtable);
        return false;
    }

    static const size_t slots[] = {
        SLOT_FLOAT, SLOT_INT, SLOT_BOOL, SLOT_PRE_UPDATE, SLOT_POST_UPDATE
    };
    for (size_t i = 0; i < sizeof slots / sizeof *slots; ++i) {
        if (!inside_module(vtable[slots[i]])) {
            report_failure(report, "vtable slot 0x%zX = %p is outside the module",
                           slots[i], vtable[slots[i]]);
            return false;
        }
    }

    // If the holder offset moved, these lookups hit a subobject that does not
    // implement the interface and the reads fail.
    float probe = 0.0f;
    if (!read_graph_float(holder, speed_var, &probe)) {
        report_failure(report, "cannot read the Speed graph variable");
        return false;
    }
    if (!read_graph_float(holder, direction_var, &probe)) {
        report_failure(report, "cannot read the Direction graph variable");
        return false;
    }

    // The field we write must already hold a plausible heading.
    const float angle = get_angle_z(player);
    if (!isfinite(angle) || angle < -SF360_TWO_PI || angle > SF360_TWO_PI) {
        report_failure(report,
                       "angle at +0x%X reads %f, which is not a heading",
                       ANGLE_Z_OFFSET, angle);
        return false;
    }

    float x = 0.0f, y = 0.0f;
    get_planar_position(player, &x, &y);
    if (!isfinite(x) || !isfinite(y)) {
        report_failure(report, "position at +0x%X is not finite", LOCATION_OFFSET);
        return false;
    }

    // The actor state offset was found by dumping the object, and the first
    // address tried held a vtable pointer instead. That mistake reads as a sit
    // state that never clears, which silently costs every rotation, so it is
    // worth one explicit test.
    const void *const *state = (const void *const *)((char *)player
                                                     + ACTOR_STATE_OFFSET);
    if (inside_module(*state)) {
        report_failure(report,
                       "actor state at +0x%X holds %p, a pointer into the game",
                       ACTOR_STATE_OFFSET, *state);
        return false;
    }

    log_line("layout check passed");
    return true;
}
