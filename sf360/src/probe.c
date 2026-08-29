#include "probe.h"
#include "config.h"
#include "game.h"
#include "log.h"
#include "state_vars.h"

#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>

// Every name and type here comes from anim_variables.xtbl, the game's own
// declaration of the graph. Guessing a name costs a whole playtest: a name the
// graph does not know reads as zero forever and looks exactly like a state that
// is simply never entered.
enum var_type { VAR_INT, VAR_FLOAT, VAR_BOOL };

struct candidate
{
    const char    *name;
    enum var_type  type;
};

// The first round established that being declared in the table is not the same
// as being present in the player's graph: CurrentGraphState, iSyncIdleWalkRun,
// bCombatWalk, iSyncCoverStates and both ladder variables all read as absent,
// and iState and bIsInAir stayed at zero through jumps and sprints. Those are
// dropped rather than kept as dead columns.
static const struct candidate CANDIDATES[] = {
    // Confirmed in round one: iSyncSprintState went to 1 for the whole sprint
    // and iSyncJumpState cycled 0-1-2 once per jump.
    { "iSyncSprintState",       VAR_INT  },
    { "iPlayingSprintAnimation", VAR_INT },
    { "iSyncJumpState",         VAR_INT  },

    // The gap round one left: nothing readable separated standing from walking
    // from running. These are the remaining spellings the table offers.
    { "iSyncIdleLocomotion",    VAR_INT  },
    { "iSyncMoveDirection",     VAR_INT  },
    { "bPlayerMoveStartActive", VAR_INT  },
    { "iSyncStandingCrouching", VAR_INT  },
    { "bFreeMovement",          VAR_INT  },

    // Aiming, which round one could not answer because none of it was probed.
    // The Nexus request is to lock the body while aiming, not while merely
    // holding a weapon, so drawn and sighted have to be told apart.
    { "bAimActive",             VAR_BOOL },
    { "iIsSighted",             VAR_INT  },
    { "iSyncSighted",           VAR_INT  },
    { "iSightedRequested",      VAR_INT  },
    { "bNoAim",                 VAR_INT  },
    { "bIsMelee",               VAR_INT  },
    { "bInReloadState",         VAR_INT  },
};

#define CANDIDATE_COUNT (sizeof CANDIDATES / sizeof CANDIDATES[0])

// A variable the graph does not know is not the same as one reading zero, and
// telling them apart is the entire point of the probe.
#define UNREADABLE INT32_MIN

static void *g_interned[CANDIDATE_COUNT];
static int32_t g_last[CANDIDATE_COUNT];
static bool g_resolved = false;
static bool g_baseline_logged = false;

// Floats are compared as hundredths so a value that merely dithers in its low
// bits does not pass for a state change and flood the log.
#define FLOAT_QUANTUM 100.0f

static int32_t sample(void *holder, size_t i)
{
    if (!g_interned[i]) return UNREADABLE;

    if (CANDIDATES[i].type == VAR_INT) {
        int32_t v = 0;
        if (!read_graph_int(holder, g_interned[i], &v)) return UNREADABLE;
        return v;
    }

    // Reading a Boolean through the integer slot fails, so the declared type
    // has to be honoured rather than guessed at.
    if (CANDIDATES[i].type == VAR_BOOL) {
        bool v = false;
        if (!read_graph_bool(holder, g_interned[i], &v)) return UNREADABLE;
        return v ? 1 : 0;
    }

    float v = 0.0f;
    if (!read_graph_float(holder, g_interned[i], &v)) return UNREADABLE;
    return (int32_t)lroundf(v * FLOAT_QUANTUM);
}

// The animation graph has no drawn or holstered state, so the weapon has to be
// looked for outside it. ActorState is the candidate: its two bitfield words
// already carry the sit state this plugin reads, and on earlier Bethesda titles
// the weapon state sits in the first of them. Logging the raw words on every
// change lets drawing and holstering pick out their own bits.
void probe_actor_state(void *player)
{
    static uint32_t last_first = 0, last_second = 0;
    static bool seen = false;

    if (!g_config.probe_states) return;

    uint32_t first = 0, second = 0;
    get_actor_state(player, &first, &second);
    if (seen && first == last_first && second == last_second) return;

    log_line("actorstate: 1=0x%08X 2=0x%08X", first, second);
    last_first = first;
    last_second = second;
    seen = true;
}

// Guessing names one build at a time cost several sessions and was wrong more
// often than right, so this sweeps every state variable the game declares and
// reports whichever ones move. The state being hunted identifies itself instead
// of having to be named in advance.
#define X(name, type) { name, type },
static const struct candidate SCANNED[] = { SF360_STATE_VARS(X) };
#undef X

#define SCANNED_COUNT (sizeof SCANNED / sizeof SCANNED[0])

static void *g_scan_interned[SCANNED_COUNT];
static int32_t g_scan_last[SCANNED_COUNT];
static bool g_scan_resolved = false;

static int32_t sample_of(void *holder, const struct candidate *c, void *name)
{
    if (!name) return UNREADABLE;

    if (c->type == VAR_INT) {
        int32_t v = 0;
        if (!read_graph_int(holder, name, &v)) return UNREADABLE;
        return v;
    }

    bool v = false;
    if (!read_graph_bool(holder, name, &v)) return UNREADABLE;
    return v ? 1 : 0;
}

void probe_scan(void *holder)
{
    if (!g_config.scan_states) return;

    if (!g_scan_resolved) {
        for (size_t i = 0; i < SCANNED_COUNT; i++) {
            g_scan_interned[i] = intern_string(SCANNED[i].name);
            g_scan_last[i] = UNREADABLE;
        }
        g_scan_resolved = true;
        // Absent variables are never mentioned again, so the opening burst is
        // the roster of what this graph actually has.
        log_line("scan: %u variables", (unsigned)SCANNED_COUNT);
    }

    for (size_t i = 0; i < SCANNED_COUNT; i++) {
        const int32_t now = sample_of(holder, &SCANNED[i], g_scan_interned[i]);
        if (now == g_scan_last[i]) continue;
        g_scan_last[i] = now;
        if (now == UNREADABLE) continue;
        log_line("scan: %s=%d", SCANNED[i].name, now);
    }
}

void probe_update(void *holder)
{
    if (!g_config.probe_states) return;

    if (!g_resolved) {
        for (size_t i = 0; i < CANDIDATE_COUNT; i++) {
            g_interned[i] = intern_string(CANDIDATES[i].name);
            g_last[i] = UNREADABLE;
        }
        g_resolved = true;
    }

    int32_t now[CANDIDATE_COUNT];
    bool changed = false;
    for (size_t i = 0; i < CANDIDATE_COUNT; i++) {
        now[i] = sample(holder, i);
        if (now[i] != g_last[i]) changed = true;
    }

    // The first sample is logged even if nothing changed, so the transcript
    // opens with a full baseline to read the later rows against.
    if (!changed && g_baseline_logged) return;

    // Every variable on one line, changed or not, so the log reads as a table
    // with one row per state change instead of a stream that has to be
    // reassembled to find out what the state was at any given moment.
    // Recorded before formatting: a line long enough to truncate would
    // otherwise leave the tail of the set stale and report a change forever.
    for (size_t i = 0; i < CANDIDATE_COUNT; i++) g_last[i] = now[i];

    char line[1024];
    int used = 0;
    for (size_t i = 0; i < CANDIDATE_COUNT; i++) {
        int written;
        if (now[i] == UNREADABLE) {
            written = snprintf(line + used, sizeof line - used, "%s=? ",
                               CANDIDATES[i].name);
        } else if (CANDIDATES[i].type == VAR_FLOAT) {
            written = snprintf(line + used, sizeof line - used, "%s=%.2f ",
                               CANDIDATES[i].name,
                               (double)now[i] / FLOAT_QUANTUM);
        } else {
            written = snprintf(line + used, sizeof line - used, "%s=%d ",
                               CANDIDATES[i].name, now[i]);
        }
        if (written < 0 || written >= (int)(sizeof line - used)) break;
        used += written;
    }

    log_line("probe: %s", line);
    g_baseline_logged = true;
}
