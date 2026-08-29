#include "restraint.h"
#include "game.h"
#include "log.h"

#include <stddef.h>
#include <stdint.h>

// ActorState begins at 0x0F0, where its vtable sits, and the two bitfield words
// follow together at 0x0F8. Located by dumping the neighbourhood rather than by
// trusting the CommonLibSF offset, which is eight bytes off on this build.
#define ACTOR_STATE_OFFSET 0xF8

// The Skyrim bit assignment does not survive here. Under it, walking, running
// and sprinting live at bits 6 to 8 and sitSleepState at 14 to 17, yet the word
// reads 0x30000000 and 0x10000000 in play, with everything below bit 28 clear
// the whole time. Whatever the fields are, they are packed at the top.
//
// So the guess is the narrowest one the evidence supports: the top nibble is a
// small enumeration that is non zero while the game owns the body. It is only a
// guess, and the watchdog below is what makes it safe to ship.
#define STATE_SHIFT 28
#define STATE_MASK  0xFu

// A wrong guess must not cost the mod. If the gate holds while the player is
// plainly moving for this many consecutive updates, roughly a few seconds, the
// hypothesis is declared wrong and never blocks again for the rest of the
// session. The previous candidate disabled the plugin for a whole playtest, and
// that must not be repeatable.
#define WATCHDOG_UPDATES 600

// Every change of the pair is logged, not just the high bits: the low ones have
// been clear in every sample so far, so there is nothing to drown out, and if
// sitting turns out to move them that is exactly what needs to be seen.
#define MAX_REPORTS 120

// Names from meshes/animtextdata/tables/anim_variables.xtbl, the game's own
// declaration of every graph variable. Logged beside the first few state
// changes so a replacement signal is already measured if the nibble fails.
static const char *const WITNESS_INT[] = {
    "bDisableFurnitureHeadtrack",
    "bIsFurnitureExit",
    "DisableAnimationDriven",
};

#define WITNESS_INT_COUNT (sizeof WITNESS_INT / sizeof WITNESS_INT[0])
#define MAX_WITNESS_REPORTS 12

static void *g_witness[WITNESS_INT_COUNT];
static void *g_code_driven = NULL;

static uint64_t g_previous = 0;
static bool     g_have_previous = false;
static int      g_reports = 0;
static int      g_held = 0;
static bool     g_abandoned = false;

static void report(void *player, uint32_t state1, uint32_t state2)
{
    log_line("restraint: actorState1=0x%08X actorState2=0x%08X top=%u",
             state1, state2, (state1 >> STATE_SHIFT) & STATE_MASK);

    if (g_reports >= MAX_WITNESS_REPORTS) return;

    void *holder = holder_of(player);

    if (!g_code_driven) g_code_driven = intern_string("IsUsingCodeDrivenRotation");
    bool driven = false;
    if (g_code_driven && read_graph_bool(holder, g_code_driven, &driven))
        log_line("  IsUsingCodeDrivenRotation=%d", driven);

    for (size_t i = 0; i < WITNESS_INT_COUNT; ++i) {
        if (!g_witness[i]) g_witness[i] = intern_string(WITNESS_INT[i]);
        int32_t value = 0;
        if (g_witness[i] && read_graph_int(holder, g_witness[i], &value))
            log_line("  %s=%d", WITNESS_INT[i], value);
    }
}

bool restraint_blocks(void *player)
{
    const uint32_t *words = (const uint32_t *)((const char *)player
                                               + ACTOR_STATE_OFFSET);
    const uint64_t pair = ((uint64_t)words[1] << 32) | words[0];

    if (g_reports < MAX_REPORTS && (!g_have_previous || pair != g_previous)) {
        report(player, words[0], words[1]);
        ++g_reports;
    }
    g_previous = pair;
    g_have_previous = true;

    if (g_abandoned) return false;

    if (((words[0] >> STATE_SHIFT) & STATE_MASK) == 0) {
        g_held = 0;
        return false;
    }

    if (++g_held > WATCHDOG_UPDATES) {
        log_line("restraint: held for %d moving updates, the top nibble is not"
                 " the sit state; yielding disabled for this session", g_held);
        g_abandoned = true;
        return false;
    }
    return true;
}
