#include "locomotion.h"
#include "config.h"
#include "game.h"

#include <stdint.h>

// Measured in play, in the probe builds, not taken from the variable table.
//
//   iSyncIdleLocomotion  0 standing, 1 for the whole time the character is
//                        moving on foot, jumps included.
//   iSyncSprintState     1 for the whole sprint.
//
// Jumping is deliberately not tested: iSyncIdleLocomotion stays at the on foot
// value through a jump, so jumps are admitted by saying nothing about them.
//
// The permissive value is named for each so the test below reads as a
// statement about the state rather than a comparison against a bare number.
#define LOCOMOTION_ON_FOOT 1
#define SPRINT_INACTIVE    0
// Off a ladder the variable reads -1, not 0: zero is one of the climb states.
#define LADDER_NONE        (-1)

static void *g_locomotion_var = NULL;
static void *g_sprint_var = NULL;
static void *g_ladder_var = NULL;
static void *g_gravity_var = NULL;
static void *g_zerog_spine_var = NULL;

// The weapon test does not come from the graph, which has no drawn or holstered
// state: bAimActive was tried as one and reads 1 with the weapon away. It comes
// from the ActorState bitfield instead, where the value was measured.

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

// For vetoes, where an unreadable variable means the subsystem behind it is not
// loaded and so cannot be the state being excluded. The opposite of reads(),
// which is used for the one condition that has to be positively true.
static bool is_zero(void *holder, void *name)
{
    int32_t value = 0;
    if (!read_graph_int(holder, name, &value)) return true;
    return value == 0;
}

bool locomotion_allowed(void *player)
{
    if (!g_locomotion_var) g_locomotion_var = intern_string("iSyncIdleLocomotion");
    if (!g_sprint_var) g_sprint_var = intern_string("iSyncSprintState");
    if (!g_ladder_var) g_ladder_var = intern_string("iLadderClimbState");
    if (!g_gravity_var) g_gravity_var = intern_string("iSyncGravity");
    if (!g_zerog_spine_var) g_zerog_spine_var = intern_string("bZeroGSpine");

    if (is_weapon_drawn(player)) return false;

    void *holder = holder_of(player);

    // Ladders, zero g and furniture are not tested for. They do not need to be:
    // whatever they set iSyncIdleLocomotion to, it is not the on foot value, so
    // they fall out here without ever having been named.
    if (!reads(holder, g_locomotion_var, LOCOMOTION_ON_FOOT)) return false;

    // Sprinting is on foot too, so it has to be subtracted back out: its turn
    // is the one users describe as too snappy.
    if (!reads(holder, g_sprint_var, SPRINT_INACTIVE)) return false;

    // A ladder keeps iSyncIdleLocomotion at the on foot value, so it has to be
    // named. iLadderClimbState is readable only while the ladder subsystem is
    // loaded, which is why earlier probes reported it as absent: a failed read
    // here means no ladder, not a wrong name.
    int32_t climb = LADDER_NONE;
    if (read_graph_int(holder, g_ladder_var, &climb) && climb != LADDER_NONE)
        return false;

    // Zero g leaves no trace in ActorState, so it has to come from the graph.
    // Both of these read zero on a planet, including a low gravity one: Mars
    // has fGravityScale at 0.38 with iSyncGravity still at zero, so this is
    // about being in free fall rather than about how strong gravity is.
    if (!is_zero(holder, g_gravity_var)) return false;
    if (!is_zero(holder, g_zerog_spine_var)) return false;

    return true;
}
