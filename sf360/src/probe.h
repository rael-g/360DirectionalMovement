#pragma once

// Samples the graph variables that are candidates for the state allowlist and
// logs the whole set whenever any of them changes. Off unless probeStates is
// set in the ini. Call once a frame, before any gate that can return early.
void probe_update(void *holder);

// Logs the ActorState bitfields whenever they change, to find the weapon state
// the animation graph does not expose. Same switch as probe_update.
void probe_actor_state(void *player);
