#include "restraint.h"
#include "game.h"
#include "log.h"

#include <stddef.h>
#include <stdint.h>

// Actor inherits TESObjectREFR, then MagicTarget, then ActorState. ActorState
// carries a vtable and then two bitfield words.
//
// CommonLibSF puts ActorState at 0x0E8, which would place the first bitfield at
// 0x0F0. Reading there returned 0x44CBBFB8, the low half of a pointer sitting
// 0x330 below the player's own vtable, so 0x0F0 is where the subobject starts
// and the bitfields follow it.
#define ACTOR_STATE_OFFSET 0xF8

// Bit assignment is the one Bethesda has used since Skyrim: the low bits carry
// the movement flags, then flyState, then the sit and sleep state.
//
//   0-3 moving back/forward/right/left   6 walking   7 running   8 sprinting
//   9 sneaking   10 swimming   11-13 flyState   14-17 sitSleepState
//
// sitSleepState is zero only when the actor is free standing. Every other value
// is a step of sitting down, sitting, or standing up, and the whole range has
// to be yielded because the crooked angle is set during the entry animation,
// not once seated.
#define SIT_SLEEP_SHIFT 14
#define SIT_SLEEP_MASK  0xFu

// The highest field in the word is around bit 25, so anything set above that is
// not a state word at all. Reading the wrong address must not leave the plugin
// blocking forever: a wrong offset should cost the fix, not the whole mod.
#define IMPLAUSIBLE_SHIFT 26

// Only bits 11 and up are watched for reporting: the ones below change on every
// step taken and would bury everything else.
#define WATCHED_SHIFT 11
#define MAX_REPORTS   40

// Names taken from meshes/animtextdata/tables/anim_variables.xtbl, the game's
// own declaration of every graph variable. They are logged next to the state
// word so that if the word is wrong again, the replacement is already measured.
static const char *const WITNESS_INT[] = {
    "bDisableFurnitureHeadtrack",
    "bIsFurnitureExit",
    "DisableAnimationDriven",
    "bIsFirstPerson",
};

#define WITNESS_INT_COUNT (sizeof WITNESS_INT / sizeof WITNESS_INT[0])

static void *g_witness[WITNESS_INT_COUNT];
static void *g_code_driven = NULL;

static uint32_t g_previous = 0;
static bool     g_have_previous = false;
static int      g_reports = 0;
static bool     g_dumped = false;

// One pass over the neighbourhood, so the state word can be located from the
// log rather than from another round of guessing. A pointer prints as a pair of
// halves that read as an address; the state word does not.
static void dump_window(const void *player)
{
    for (size_t offset = 0xE0; offset <= 0x110; offset += 8) {
        const void *const *slot = (const void *const *)((const char *)player + offset);
        log_line("  +0x%03zX %016llX%s", offset, (unsigned long long)(uintptr_t)*slot,
                 inside_module(*slot) ? "  <- points into the game" : "");
    }
}

static void report(void *player, uint32_t state, bool trusted)
{
    log_line("restraint: actorState1=0x%08X sitSleep=%u fly=%u%s",
             state, (state >> SIT_SLEEP_SHIFT) & SIT_SLEEP_MASK,
             (state >> 11) & 0x7u, trusted ? "" : "  (implausible, ignored)");

    if (!g_dumped) {
        dump_window(player);
        g_dumped = true;
    }

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
    const uint32_t state = *(const uint32_t *)((const char *)player
                                               + ACTOR_STATE_OFFSET);
    const bool trusted = (state >> IMPLAUSIBLE_SHIFT) == 0;

    if (g_reports < MAX_REPORTS
        && (!g_have_previous
            || (state >> WATCHED_SHIFT) != (g_previous >> WATCHED_SHIFT))) {
        report(player, state, trusted);
        ++g_reports;
    }
    g_previous = state;
    g_have_previous = true;

    return trusted && ((state >> SIT_SLEEP_SHIFT) & SIT_SLEEP_MASK) != 0;
}
