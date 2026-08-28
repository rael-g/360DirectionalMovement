#pragma once

#include <stdbool.h>

// Address Library keeps the resolved IDs stable across game builds, but nothing
// validates the rest: struct offsets and vtable slot indices are layout facts a
// game update can move, and getting them wrong does not fault, it silently
// writes over an unrelated field.
//
// Confirms every assumption against the live object. The player object exists
// before its animation graph is ready, so a failure early in a load is expected
// and the caller retries; `report` silences the repeats.
bool validate_layout(void *player, void *speed_var, void *direction_var,
                     bool report);
