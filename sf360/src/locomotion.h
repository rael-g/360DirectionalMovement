#pragma once

#include <stdbool.h>

// True only in the on foot states the mod is meant to act in: walking and
// jogging. Everything else, known or not, reads as false.
bool locomotion_allowed(void *holder);
