#include "restraint.h"
#include "game.h"
#include "log.h"

#include <stddef.h>
#include <stdint.h>

// Actor inherits TESObjectREFR at 0x000, MagicTarget at 0x0D0 and ActorState at
// 0x0E8. ActorState carries a vtable and then two bitfield words, so the first
// of them sits at 0x0F0. Layout from CommonLibSF, which the plugin reads but
// does not link against.
#define ACTOR_STATE_OFFSET 0xF0

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

// Only bits 11 and up are watched for reporting: the ones below change on every
// step taken and would bury everything else.
#define WATCHED_SHIFT 11
#define MAX_REPORTS   40

// Names taken from meshes/animtextdata/tables/anim_variables.xtbl, the game's
// own declaration of every graph variable. Guessing Skyrim names produced
// eighteen misses in a row; these are the real ones, with the real types.
//
// None of them is known to mark sitting yet. They are read and logged next to
// the actor state so that one playtest either confirms the state word or names
// the variable that should replace it.
static const char *const WITNESS_INT[] = {
    "bDisableFurnitureHeadtrack",
    "bIsFurnitureExit",
    "DisableAllowRotation",
    "DisableAnimationDriven",
    "bIsFirstPerson",
};

#define WITNESS_INT_COUNT (sizeof WITNESS_INT / sizeof WITNESS_INT[0])

static void *g_witness[WITNESS_INT_COUNT];
static void *g_code_driven = NULL;

static uint32_t g_previous = 0;
static bool     g_have_previous = false;
static int      g_reports = 0;

static void report(void *player, uint32_t state)
{
    log_line("restraint: actorState1=0x%08X sitSleep=%u fly=%u",
             state, (state >> SIT_SLEEP_SHIFT) & SIT_SLEEP_MASK,
             (state >> 11) & 0x7u);

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

    if (g_reports < MAX_REPORTS
        && (!g_have_previous
            || (state >> WATCHED_SHIFT) != (g_previous >> WATCHED_SHIFT))) {
        report(player, state);
        ++g_reports;
    }
    g_previous = state;
    g_have_previous = true;

    return ((state >> SIT_SLEEP_SHIFT) & SIT_SLEEP_MASK) != 0;
}
