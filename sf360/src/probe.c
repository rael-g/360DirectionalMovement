#include "probe.h"
#include "config.h"
#include "game.h"
#include "log.h"

#include <stdbool.h>
#include <stdint.h>
#include <math.h>
#include <stdio.h>

// Every name and type here comes from anim_variables.xtbl, the game's own
// declaration of the graph. Guessing a name costs a whole playtest: a name the
// graph does not know reads as zero forever and looks exactly like a state that
// is simply never entered.
enum var_type { VAR_INT, VAR_FLOAT };

struct candidate
{
    const char    *name;
    enum var_type  type;
};

static const struct candidate CANDIDATES[] = {
    // Whole graph state. If either of these turns out to be a single enum that
    // separates locomotion from ladders and zero g, the allowlist collapses
    // into one comparison and none of the rest are needed.
    { "CurrentGraphState",           VAR_FLOAT },
    { "iState",                      VAR_INT   },

    // On foot locomotion, the states the allowlist is meant to admit.
    { "iSyncIdleWalkRun",            VAR_INT   },
    { "iPlayingSprintAnimation",     VAR_INT   },
    { "iSyncSprintState",            VAR_INT   },
    { "bCombatWalk",                 VAR_INT   },

    { "bIsInAir",                    VAR_INT   },
    { "iSyncJumpState",              VAR_INT   },

    // The reported breakages.
    { "iLadderClimbState",           VAR_INT   },
    { "iPlayerLadderClimbAnimation", VAR_INT   },
    { "bZeroGSpine",                 VAR_INT   },
    { "iSyncGravity",                VAR_INT   },
    { "iSyncGravDash",               VAR_INT   },
    { "fGravityScale",               VAR_FLOAT },

    { "iSyncSwimState",              VAR_INT   },
    { "iSyncCoverStates",            VAR_INT   },
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

    float v = 0.0f;
    if (!read_graph_float(holder, g_interned[i], &v)) return UNREADABLE;
    return (int32_t)lroundf(v * FLOAT_QUANTUM);
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
