#pragma once

#include <stdbool.h>

// Some states drive the actor's rotation from the animation itself: entering a
// chair, a cockpit seat, a bed, any paired or synchronised animation. During
// those the game turns the body to match the furniture, and this plugin fights
// it, which is what makes the character land in the seat crooked.
//
// Which graph variable names exist is not documented, so a list of candidates is
// probed once against the live graph and only the ones that answer are used.

// Probes the candidate names. Safe to call again after the player is rebuilt.
void restraint_bind(void *holder);

// True while the game owns the body's rotation.
bool restraint_blocks(void *holder);
