#include "probe.h"
#include "config.h"
#include "game.h"
#include "log.h"
#include "state_vars.h"

#include <stdbool.h>
#include <stdint.h>

// Only Integer and Boolean variables are swept. Floats carry continuous
// quantities that move every frame and would bury the transitions.
enum var_type { VAR_INT, VAR_BOOL };

// A variable the graph does not currently declare is not the same as one
// reading zero, and keeping them apart is the whole point: a name reads as
// unavailable while the graph that owns it is unloaded, not only when the name
// is wrong.
#define UNREADABLE INT32_MIN

struct candidate
{
    const char    *name;
    enum var_type  type;
};

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
