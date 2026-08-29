#pragma once

#include <stdbool.h>

// Reads the toggle key and flips the state on a fresh press. Call once a frame.
void toggle_update(void);

// False while the player has switched the mod off.
bool toggle_enabled(void);
