#pragma once

#include <stdbool.h>

// Owns the angle the player body is driven to, and the writes that enforce it.
//
// The game applies its own rotation around the animation graph update, so a
// write placed at an arbitrary moment lands on either side of it at random.
// Writing from the graph update hooks makes the moment deterministic.

// Installs the vtable hooks on the player's graph holder. Safe to call again
// when the player object is rebuilt.
bool rotator_install(void *player);

// The angle to drive towards. Clearing the target disarms every write.
void rotator_set_target(float radians);
void rotator_clear_target(void);

// Starts a new movement from the body's current angle, and makes the first
// step instant rather than eased.
void rotator_reset(float current_angle);

// Times the hook has fired, used to relate log entries to graph updates.
long rotator_hook_calls(void);

// True when the hooks could not be installed and the caller has to write the
// angle itself.
bool rotator_failed(void);
