#include "locomotion.h"
#include "config.h"
#include "game.h"
#include "log.h"

#include <stdint.h>

// Measured in play, in the probe builds, not taken from the variable table.
//
//   iSyncIdleLocomotion  0 standing, 1 for the whole time the character is
//                        moving on foot, jumps included.
//   iSyncSprintState     1 for the whole sprint.
//   iSyncJumpState       0 on the ground, 1 then 2 through a jump, and 3 once
//                        on a longer one.
//
// The permissive value is named for each so the test below reads as a
// statement about the state rather than a comparison against a bare number.
#define LOCOMOTION_ON_FOOT 1
#define SPRINT_INACTIVE    0
#define JUMP_GROUNDED      0

static void *g_locomotion_var = NULL;
static void *g_sprint_var = NULL;
static void *g_jump_var = NULL;
static void *g_weapon_var = NULL;

// bAimActive is the weapon test on inference, not on measurement: it was the
// only thing that tracked the weapon being drawn and holstered in the probe
// session, and everything else the variable table offers on the subject is
// transient. Each flip is logged so the first minute of play settles it.
static int g_weapon_logged = -1;

// An allowlist inverts the failure mode: a name that goes missing after a game
// update stops the mod instead of letting it run somewhere it breaks. That is
// only true if a failed read is treated as "not allowed", which is what the
// initialiser here buys.
static bool reads(void *holder, void *name, int32_t expected)
{
    int32_t value = expected + 1;
    if (!read_graph_int(holder, name, &value)) return false;
    return value == expected;
}

bool locomotion_allowed(void *holder)
{
    if (!g_locomotion_var) g_locomotion_var = intern_string("iSyncIdleLocomotion");
    if (!g_sprint_var) g_sprint_var = intern_string("iSyncSprintState");
    if (!g_jump_var) g_jump_var = intern_string("iSyncJumpState");
    if (!g_weapon_var) g_weapon_var = intern_string("bAimActive");

    // Ladders, zero g and furniture are not tested for. They do not need to be:
    // whatever they set iSyncIdleLocomotion to, it is not the on foot value, so
    // they fall out here without ever having been named.
    if (!reads(holder, g_locomotion_var, LOCOMOTION_ON_FOOT)) return false;

    // Sprinting and jumping are on foot too, so they have to be subtracted back
    // out. Both are still unsolved: the sprint turn is the one users call too
    // snappy, and the jump has a slide.
    if (!reads(holder, g_sprint_var, SPRINT_INACTIVE)) return false;
    if (!reads(holder, g_jump_var, JUMP_GROUNDED)) return false;

    // Declared Boolean rather than Integer, so it needs the boolean slot.
    bool weapon_out = true;
    if (!read_graph_bool(holder, g_weapon_var, &weapon_out)) return false;

    if (g_weapon_logged != (int)weapon_out) {
        log_line("weapon: bAimActive=%d", (int)weapon_out);
        g_weapon_logged = (int)weapon_out;
    }

    return !weapon_out;
}
