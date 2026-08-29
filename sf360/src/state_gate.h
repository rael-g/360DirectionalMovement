#pragma once

#include <stdbool.h>

// Decides whether the mod may turn the body at all. True while walking, jogging
// and jumping with the weapon put away; false while sprinting, sitting,
// climbing a ladder, in zero g, or holding a weapon.
//
// This is a broad locomotion test with vetoes hung off it, not an allowlist:
// the positive condition is too coarse for an unknown state to be safe by
// default, so each new broken situation costs a veto.
bool state_gate_allows(void *player);

// Clears the state that is only meant to last as long as one player object.
void state_gate_rebound(void);
