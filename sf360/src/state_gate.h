#pragma once

#include <stdbool.h>

// Decides whether the mod may turn the body at all. True while walking, jogging
// and jumping with the weapon put away; false while sprinting, sitting,
// climbing a ladder, in zero g, or holding a weapon.
//
// This is a broad locomotion test with a list of vetoes hung off it, not an
// allowlist. It was built as one, and ladders proved it was not: unknown states
// are not safe by default here, because the positive condition does not
// discriminate finely enough for that to follow. Each new broken situation
// costs a veto until something narrower turns up.
bool state_gate_allows(void *player);

// Clears the state that is only meant to last as long as one player object.
void state_gate_rebound(void);
