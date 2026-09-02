#pragma once

#include <stdbool.h>

// Every field isolates one behaviour so it can be switched off independently.
// All default to enabled.
struct config
{
    bool  hook_post;           // also write from PostUpdate (slot 0x16)
    bool  recapture_on_settle; // recapture the offset once heading is stable
    bool  recapture_on_switch; // recapture when the player changes direction
    float turn_rate;           // degrees per second the body may turn at most;
                               // 0 means no limit
    float turn_smoothing;      // roughly the seconds a turn takes to settle;
                               // 0 removes the easing at both ends
    float direction_hold;      // seconds a new direction must hold before the
                               // body follows it; 0 follows every sample
    float start_hold;          // seconds a movement must last before the
                               // body turns into it; a tap turns nothing
    bool  snap_on_start;       // jump straight to the heading when a movement
                               // begins instead of turning into it
    float min_speed;           // Speed above which the actor counts as moving
    bool  yield_when_sitting;  // leave the body alone while sitting or sleeping
    bool  yield_when_sneaking; // leave the body alone while sneaking
    bool  freeze_when_jumping; // hold the heading until the feet are down
                               // again, rather than handing rotation back
    bool  allow_sprint;        // turn the body while sprinting too
    int   toggle_key;          // virtual key code that switches the mod on and
                               // off in play; 0 removes the shortcut
    bool  probe_states;        // log the graph variables the allowlist is being
                               // built from; off, it is a diagnostic
    bool  scan_states;         // log every state variable that changes; loud,
                               // and only useful for finding a new state
    bool  state_allowlist;     // act only in the on foot states known to be
                               // safe, instead of everywhere but the vetoes
};

extern struct config g_config;

// Reads the file if it exists. An absent file leaves every default in place,
// which is the shipping behaviour; the file is never created.
void config_load(void);
