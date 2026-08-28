#pragma once

#include <stdbool.h>

// Every field isolates one behaviour so it can be switched off independently.
// All default to enabled.
struct config
{
    bool  hook_post;           // also write from PostUpdate (slot 0x16)
    bool  recapture_on_settle; // recapture the offset once heading is stable
    bool  recapture_on_switch; // recapture when the player changes direction
    float max_step;            // radians per graph update; 0 means snap
    float min_speed;           // Speed above which the actor counts as moving
};

extern struct config g_config;

// Reads the file if it exists. An absent file leaves every default in place,
// which is the shipping behaviour; the file is never created.
void config_load(void);
