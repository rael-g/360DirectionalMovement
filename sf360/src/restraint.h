#pragma once

#include <stdbool.h>

// Some states drive the actor's rotation from the animation itself: entering a
// chair, a cockpit seat, a bed. During those the game turns the body to match
// the furniture, and this plugin fights it, which is what makes the character
// land in the seat crooked.
//
// The state is read from the actor's own memory rather than from the animation
// graph. Eighteen candidate graph variable names were probed against the live
// graph and not one of them existed, so the Skyrim naming does not carry over.

// True while the game owns the body's rotation.
bool restraint_blocks(void *player);
