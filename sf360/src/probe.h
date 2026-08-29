#pragma once

// Logs the ActorState bitfields whenever they change. This is where the sit and
// weapon states live; the animation graph exposes neither.
void probe_actor_state(void *player);

// Sweeps every Integer and Boolean variable the game declares and logs each one
// that changes, so a state can be found without its name being guessed first.
// Loud: a short session is worth hundreds of lines.
void probe_scan(void *holder);
