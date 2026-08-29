#pragma once

// Samples the graph variables that are candidates for the state allowlist and
// logs the whole set whenever any of them changes. Off unless probeStates is
// set in the ini. Call once a frame, before any gate that can return early.
void probe_update(void *holder);
