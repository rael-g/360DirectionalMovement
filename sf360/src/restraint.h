#pragma once

#include <stdbool.h>

// Some states drive the actor's rotation from the animation itself: entering a
// chair, a cockpit seat, a bed. During those the game turns the body to match
// the furniture, and this plugin fights it, which is what makes the character
// land in the seat crooked.

// Records the actor state and the graph's rotation flags when they change.
// Called on every update, including the ones where the actor is too slow to
// count as moving, because that is where sitting may well live.
void restraint_observe(void *player, float speed);

// True while the game owns the body's rotation. Called only on the moving path,
// so that a wrong answer here always costs something visible.
bool restraint_blocks(void *player);
