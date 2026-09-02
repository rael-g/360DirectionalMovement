#include "state_gate.h"
#include "config.h"
#include "game.h"
#include "log.h"

#include <stdint.h>

// Measured in play, not read from the game's variable table, which says nothing
// about what a value means.
//
//   iSyncIdleLocomotion  0 standing, 1 moving on foot, jumps and ladders too
//   iSyncSprintState     1 for the whole sprint
//   iLadderClimbState   -1 off a ladder, 0 upwards while climbing one
//   iSyncGravity         1 in free fall, 0 under any gravity however weak
#define LOCOMOTION_ON_FOOT 1
#define SPRINT_INACTIVE    0
#define LADDER_NONE        (-1)
#define ZERO_G_INACTIVE    0

static void *g_locomotion_var = NULL;
static void *g_sprint_var = NULL;
static void *g_ladder_var = NULL;
static void *g_gravity_var = NULL;
static void *g_zerog_spine_var = NULL;
static void *g_sneak_var = NULL;
static void *g_jump_var = NULL;

// The sit state comes from an offset a game update could move, so the raw words
// are logged once per binding.
static bool g_sit_logged = false;

// A variable is readable only while the graph declaring it is loaded, so
// unreadable means opposite things depending on which way the test runs. Hence
// two helpers rather than one with a flag.

// For a condition that must be positively true. Unreadable fails it, so a name
// lost to a game update stops the mod rather than leaving it on a stale
// assumption.
static bool confirmed(void *holder, void *name, int32_t expected)
{
    int32_t value = expected + 1;
    if (!read_graph_int(holder, name, &value)) return false;
    return value == expected;
}

// For a veto. Unreadable clears it: an unloaded subsystem cannot be the state
// the character is in.
static bool vetoed(void *holder, void *name, int32_t inactive)
{
    int32_t value = inactive;
    if (!read_graph_int(holder, name, &value)) return false;
    return value != inactive;
}

// For a veto whose resting value has not been measured. `iLadderClimbState`
// rests at -1, so testing against zero there disabled the mod everywhere; only
// a positive reading is taken as the state being active.
static bool vetoed_positive(void *holder, void *name)
{
    int32_t value = 0;
    if (!read_graph_int(holder, name, &value)) return false;
    return value > 0;
}

// Whether the jump signal ever reads anything at all is still unproven: the
// diagnostic window closed before the first jump of the session. One line the
// first time it moves settles it without another build.
static bool g_jump_logged = false;

bool state_gate_is_jumping(void *player)
{
    if (!g_jump_var) g_jump_var = intern_string("iSyncJumpState");

    void *holder = holder_of(player);
    int32_t value = 0;
    if (!read_graph_int(holder, g_jump_var, &value)) return false;

    if (value != 0 && !g_jump_logged) {
        log_line("jump: iSyncJumpState=%d", value);
        g_jump_logged = true;
    }
    return value > 0;
}

int state_gate_raw(void *player, int which)
{
    void *holder = holder_of(player);
    void *name = (which == 0) ? g_sneak_var : g_jump_var;
    int32_t value = -1000;
    if (!name || !read_graph_int(holder, name, &value)) return -1000;
    return value;
}

void state_gate_rebound(void)
{
    g_sit_logged = false;
    g_jump_logged = false;
}

bool state_gate_allows(void *player)
{
    if (!g_locomotion_var) g_locomotion_var = intern_string("iSyncIdleLocomotion");
    if (!g_sprint_var) g_sprint_var = intern_string("iSyncSprintState");
    if (!g_ladder_var) g_ladder_var = intern_string("iLadderClimbState");
    if (!g_gravity_var) g_gravity_var = intern_string("iSyncGravity");
    if (!g_zerog_spine_var) g_zerog_spine_var = intern_string("bZeroGSpine");
    if (!g_sneak_var) g_sneak_var = intern_string("iIsInSneak");
    if (!g_jump_var) g_jump_var = intern_string("iSyncJumpState");

    // From ActorState, because the graph has no drawn or holstered state.
    if (is_weapon_drawn(player)) return false;

    if (g_config.yield_when_sitting && is_sitting(player)) {
        if (!g_sit_logged) {
            uint32_t first = 0, second = 0;
            get_actor_state(player, &first, &second);
            log_line("sitting: actorState1=0x%08X actorState2=0x%08X",
                     first, second);
            g_sit_logged = true;
        }
        return false;
    }

    void *holder = holder_of(player);

    // The one positive condition, and it only separates moving on foot from
    // standing still and from furniture. Every veto below exists because it
    // admits a state it should not.
    if (!confirmed(holder, g_locomotion_var, LOCOMOTION_ON_FOOT)) return false;

    if (!g_config.allow_sprint
        && vetoed(holder, g_sprint_var, SPRINT_INACTIVE)) return false;

    if (g_config.yield_when_sneaking
        && vetoed_positive(holder, g_sneak_var)) return false;

    // Climbing: rotating the body here traps the character between decks.
    if (vetoed(holder, g_ladder_var, LADDER_NONE)) return false;

    // Zero g: rotating the body here spins the character on the spot.
    if (vetoed(holder, g_gravity_var, ZERO_G_INACTIVE)) return false;
    if (vetoed(holder, g_zerog_spine_var, ZERO_G_INACTIVE)) return false;

    return true;
}
