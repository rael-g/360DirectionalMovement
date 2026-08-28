#pragma once

// Decides where the body should face. It never writes the angle itself: it
// hands a target to the rotator, which owns the writes.
//
// Called once per frame on the game's main thread.
void movement_director_update(void);
