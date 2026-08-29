#include "restraint.h"
#include "game.h"
#include "log.h"

#include <stddef.h>
#include <stdint.h>

// ActorState begins at 0x0F0, where its vtable sits, and the two bitfield words
// follow together at 0x0F8. Located by dumping the neighbourhood rather than by
// trusting the CommonLibSF offset, which is eight bytes off on this build.
#define ACTOR_STATE_OFFSET 0xF8

// The low byte holds four two bit fields, each reading 1 or 2 and never 0 or 3,
// which is how this game spells the movement flags Skyrim kept as single bits.
// They change on every step and would fill the log budget before anything
// interesting happened, so they are masked out of the change test.
#define MOVEMENT_BITS 0xFFu

// Only rotate while the graph says the rotation is code driven. The name is the
// game's own, from anim_variables.xtbl, and it reads 1 throughout normal
// walking, so a furniture animation taking the wheel should read 0.
//
// This replaces a guess at the actor state bits: the top nibble was 3 for the
// whole of a session, moving or not, so it is not the sit state.
#define YIELD_WHEN_NOT_CODE_DRIVEN 1

// A wrong guess must not cost the mod. If the gate holds while the player is
// plainly moving for this many consecutive updates, roughly a few seconds, the
// hypothesis is declared wrong and never blocks again for the rest of the
// session.
#define WATCHDOG_UPDATES 600

#define MAX_REPORTS 120

// Names from meshes/animtextdata/tables/anim_variables.xtbl, the game's own
// declaration of every graph variable.
static const char *const WITNESS_INT[] = {
    "bDisableFurnitureHeadtrack",
    "bIsFurnitureExit",
    "DisableAnimationDriven",
};

#define WITNESS_INT_COUNT (sizeof WITNESS_INT / sizeof WITNESS_INT[0])

static void *g_witness[WITNESS_INT_COUNT];
static void *g_code_driven = NULL;

static uint64_t g_previous = 0;
static bool     g_have_previous = false;
static int      g_reports = 0;
static int      g_held = 0;
static bool     g_abandoned = false;

static bool code_driven(void *holder, bool *out)
{
    if (!g_code_driven) g_code_driven = intern_string("IsUsingCodeDrivenRotation");
    return g_code_driven && read_graph_bool(holder, g_code_driven, out);
}

void restraint_observe(void *player, float speed)
{
    const uint32_t *words = (const uint32_t *)((const char *)player
                                               + ACTOR_STATE_OFFSET);
    const uint64_t pair = ((uint64_t)words[1] << 32) | (words[0] & ~MOVEMENT_BITS);

    if (g_reports >= MAX_REPORTS || (g_have_previous && pair == g_previous)) {
        g_previous = pair;
        g_have_previous = true;
        return;
    }
    g_previous = pair;
    g_have_previous = true;
    ++g_reports;

    void *holder = holder_of(player);

    // Speed is logged here because the gate sits behind the speed test. If
    // sitting turns out to happen below the threshold, the gate can never see
    // it, and only this line would say so.
    bool driven = false;
    const bool has_driven = code_driven(holder, &driven);
    log_line("restraint: actorState1=0x%08X actorState2=0x%08X speed=%.2f"
             " codeDriven=%s",
             words[0], words[1], speed, has_driven ? (driven ? "1" : "0") : "?");

    for (size_t i = 0; i < WITNESS_INT_COUNT; ++i) {
        if (!g_witness[i]) g_witness[i] = intern_string(WITNESS_INT[i]);
        int32_t value = 0;
        if (g_witness[i] && read_graph_int(holder, g_witness[i], &value) && value)
            log_line("  %s=%d", WITNESS_INT[i], value);
    }
}

bool restraint_blocks(void *player)
{
    if (g_abandoned) return false;

    bool driven = false;
    if (!code_driven(holder_of(player), &driven)) return false;

    if (driven == YIELD_WHEN_NOT_CODE_DRIVEN) {
        g_held = 0;
        return false;
    }

    if (++g_held > WATCHDOG_UPDATES) {
        log_line("restraint: held for %d moving updates, IsUsingCodeDrivenRotation"
                 " is not the signal; yielding disabled for this session", g_held);
        g_abandoned = true;
        return false;
    }
    return true;
}
