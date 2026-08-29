#pragma once

#include <stdbool.h>

// True only in the states the mod is meant to act in: walking, jogging and
// jumping, with the weapon put away. Everything else reads as false.
bool locomotion_allowed(void *player);
