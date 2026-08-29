#include "restraint.h"
#include "config.h"
#include "game.h"
#include "log.h"

#include <stddef.h>

// Both spellings of each concept are listed because the graph uses a `b` prefix
// for some booleans and not others, and guessing wrong costs a whole playtest.
static const char *const CANDIDATES[] = {
    "IsSitting", "bIsSitting", "bSitting",
    "IsInFurniture", "bInFurniture", "bIsInFurniture",
    "IsSittingDown", "bIsSittingDown",
    "IsSleeping", "bIsSleeping",
    "IsPilotingShip", "bIsPilotingShip", "bIsPiloting",
    "bIsSynced", "bIsSyncedAnim",
    // Set while the graph, not the movement code, owns the actor transform.
    // Furniture entry is exactly that, so it catches the case even if every
    // name above is wrong.
    "bAnimationDriven", "IsAnimationDriven", "bMotionDriven",
};

#define CANDIDATE_COUNT (sizeof CANDIDATES / sizeof CANDIDATES[0])

static void *g_name[CANDIDATE_COUNT];
static bool  g_live[CANDIDATE_COUNT];
static bool  g_was_set[CANDIDATE_COUNT];

void restraint_bind(void *holder)
{
    for (size_t i = 0; i < CANDIDATE_COUNT; ++i) {
        if (!g_name[i]) g_name[i] = intern_string(CANDIDATES[i]);

        bool value = false;
        g_live[i] = g_name[i] && read_graph_bool(holder, g_name[i], &value);
        g_was_set[i] = false;
        if (g_live[i])
            log_line("restraint: '%s' exists, reads %d", CANDIDATES[i], value);
    }
}

bool restraint_blocks(void *holder)
{
    bool blocked = false;

    for (size_t i = 0; i < CANDIDATE_COUNT; ++i) {
        if (!g_live[i]) continue;

        bool value = false;
        if (!read_graph_bool(holder, g_name[i], &value)) continue;
        if (value) blocked = true;

        // One line per transition, not per update: the point is to learn which
        // name actually covers sitting, and a line per frame would bury it.
        if (value != g_was_set[i]) {
            log_line("restraint: '%s' -> %d", CANDIDATES[i], value);
            g_was_set[i] = value;
        }
    }

    return blocked;
}
