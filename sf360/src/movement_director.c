#include "movement_director.h"
#include "angles.h"
#include "config.h"
#include "diagnostics.h"
#include "game.h"
#include "layout_check.h"
#include "log.h"
#include "probe.h"
#include "rotator.h"
#include "toggle.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>

// A graph variable that could not be read stays distinguishable from one that
// legitimately reads zero.
#define UNAVAILABLE (-999.0f)

// Heading steps below this count as stable, about three degrees.
#define STABLE_HEADING 0.052f

// Squared displacement below which a measured heading is just noise.
#define MIN_TRAVEL_SQ 1e-6f

// `Direction` is a fraction of a turn and never negative, so this marks "no
// sample carried over from a previous movement".
#define NO_DIRECTION (-1.0f)

static void *g_speed_var = NULL;
static void *g_direction_var = NULL;
static void *g_camera_yaw_var = NULL;
static void *g_first_person_var = NULL;

static void *g_player = NULL;
static void *g_pending_player = NULL;

static bool  g_moving = false;
static bool  g_settled = false;
static float g_offset = 0.0f;
static float g_previous_heading = 0.0f;
static float g_settled_direction = 0.0f;
static float g_last_direction = NO_DIRECTION;

static float g_last_x = 0.0f, g_last_y = 0.0f;
static float g_travel_x = 0.0f, g_travel_y = 0.0f;
static float g_sampled_x = 0.0f, g_sampled_y = 0.0f;

// The sit state is read from a measured offset, so a game update could move it
// under us. Recording it once per load costs one line and turns "sitting broke
// again" into a question the log can answer.
static bool g_sit_logged = false;

static float read_or_unavailable(void *holder, void *name)
{
    float v = UNAVAILABLE;
    if (name) read_graph_float(holder, name, &v);
    return v;
}

static bool resolve_variables(void)
{
    if (!g_speed_var) g_speed_var = intern_string("Speed");
    if (!g_direction_var) g_direction_var = intern_string("Direction");
    // Named 'bIsFirstPerson' and declared Integer, not 'IsFirstPerson' and
    // Boolean. The Skyrim spelling reads nothing, so first person was never
    // actually detected before.
    if (!g_first_person_var) g_first_person_var = intern_string("bIsFirstPerson");
    if (!g_camera_yaw_var) g_camera_yaw_var = intern_string("fCameraYaw");
    return g_speed_var && g_direction_var;
}

// Loading a save rebuilds the player object, possibly with a different vtable,
// while this module's state survives.
//
// The object also appears before its animation graph is ready, so binding only
// commits once the layout check passes. Committing earlier would spend the one
// attempt during the load and leave the plugin inert for the whole session.
static bool rebind(void *player)
{
    const bool first_attempt = (player != g_pending_player);
    g_pending_player = player;

    if (!validate_layout(player, g_speed_var, g_direction_var, first_attempt))
        return false;

    g_player = player;
    g_moving = false;
    g_settled = false;
    g_last_direction = NO_DIRECTION;
    diagnostics_reset();
    g_sit_logged = false;
    rotator_install(player);
    return true;
}

static void end_movement(void)
{
    if (g_moving) diagnostics_flush();
    g_moving = false;
    rotator_clear_target();
    // Without this, stopping and moving again along the same heading would
    // leave `Direction` unchanged, suppress the start branch entirely and carry
    // the previous movement's offset over.
    g_last_direction = NO_DIRECTION;
}

// The only reading of `Direction` taken as absolute truth. Afterwards it is
// contaminated: the reported direction rotates with the body, so feeding it
// back would have positive gain.
static void begin_movement(float angle, float relative, float camera_yaw)
{
    g_moving = true;
    g_settled = false;
    g_offset = wrap_signed((angle + relative) - camera_yaw);
    g_previous_heading = angle + relative;
    rotator_reset(angle);
}

static void maintain_offset(float direction, float heading, float camera_yaw)
{
    // `Direction` returns to roughly zero once the body faces the movement.
    // Leaving that octant means the player changed direction, so the captured
    // offset no longer describes intent.
    //
    // The new offset is taken here and now, for the same reason the start of a
    // movement takes one: on the first sample after the input changes the body
    // has not turned yet, so the heading still describes intent rather than our
    // own rotation. Deferring to the settle test instead would never capture
    // while the player keeps turning, because the heading never stops moving.
    if (g_settled && g_config.recapture_on_switch) {
        if (octant_of(direction) != octant_of(g_settled_direction)) {
            g_offset = wrap_signed(heading - camera_yaw);
            g_settled_direction = direction;
            g_previous_heading = heading;
            return;
        }
    }

    // `Direction` ramps up from zero at the start of a movement. Capturing on
    // the first frame would lock the movement onto a wrong offset, so the
    // capture waits until the heading stops changing, then happens once.
    if (!g_settled && g_config.recapture_on_settle) {
        if (fabsf(wrap_signed(heading - g_previous_heading)) < STABLE_HEADING) {
            g_offset = wrap_signed(heading - camera_yaw);
            g_settled = true;
            g_settled_direction = direction;
        }
        g_previous_heading = heading;
    }
}

// Position does not change on the same frame the graph updates, so the delta is
// accumulated and consumed when a new sample arrives.
static void accumulate_travel(void *player)
{
    float x = 0.0f, y = 0.0f;
    get_planar_position(player, &x, &y);
    g_travel_x += x - g_last_x;
    g_travel_y += y - g_last_y;
    g_last_x = x;
    g_last_y = y;
}

static float measured_heading(void)
{
    const float travel_sq = g_sampled_x * g_sampled_x + g_sampled_y * g_sampled_y;
    if (travel_sq <= MIN_TRAVEL_SQ) return UNAVAILABLE;
    return atan2f(g_sampled_x, g_sampled_y) * SF360_RAD_TO_DEG;
}

void movement_director_update(void)
{
    toggle_update();

    void *player = get_player();
    if (!player) return;

    // Interning first: the layout check reads graph variables.
    if (!resolve_variables()) return;
    if (player != g_player && !rebind(player)) return;

    void *holder = holder_of(player);
    // Kept above every early return so the position stays current: coming out of
    // a chair with a stale sample would charge the whole displacement of the
    // animation to the first update of the next movement.
    accumulate_travel(player);

    // Above every gate, including the toggle: the states it exists to describe
    // are the ones the mod refuses to run in, so sampling below a gate would
    // record everything except what is being looked for.
    probe_update(holder);

    // Switched off from the keyboard. Below accumulate_travel for the same
    // reason as the sitting gate: the position has to stay current so whatever
    // happened while the mod was off is not charged to the first update after
    // it comes back.
    //
    // The hooks stay installed and go inert on the cleared target, which avoids
    // restoring vtable slots that another plugin may have swapped since.
    if (!toggle_enabled()) {
        end_movement();
        return;
    }

    // Sitting down, entering a cockpit, lying down: the game turns the body to
    // match the furniture, and writing our own angle over it swings the camera
    // through the animation and leaves the character seated crooked.
    //
    // Tested before speed, not after. The graph reports speed for as long as
    // the character stays seated, so anything behind the speed test would see
    // sitting as ordinary movement.
    if (g_config.yield_when_sitting && is_sitting(player)) {
        if (!g_sit_logged) {
            uint32_t first = 0, second = 0;
            get_actor_state(player, &first, &second);
            log_line("sitting: actorState1=0x%08X actorState2=0x%08X",
                     first, second);
            g_sit_logged = true;
        }
        end_movement();
        return;
    }

    float speed = 0.0f, direction = 0.0f;
    if (!read_graph_float(holder, g_speed_var, &speed)) return;
    if (!read_graph_float(holder, g_direction_var, &direction)) return;

    int32_t first_person = 0;
    read_graph_int(holder, g_first_person_var, &first_person);

    if (speed <= g_config.min_speed || first_person) {
        end_movement();
        return;
    }

    // `Direction` is the movement direction relative to the body, in turns. It
    // is exact from the first update of a movement, which is why it drives the
    // control instead of a heading derived from position, which needs two
    // samples above a minimum displacement.
    const float relative = wrap_signed(direction * SF360_TWO_PI);
    const float angle = get_angle_z(player);

    // The task runs several times per graph update while `direction` only
    // changes with the update. Acting again on the same value would be repeated
    // open loop correction.
    const bool fresh_sample = (direction != g_last_direction);
    g_last_direction = direction;
    if (fresh_sample) {
        g_sampled_x = g_travel_x;
        g_sampled_y = g_travel_y;
        g_travel_x = 0.0f;
        g_travel_y = 0.0f;
    }

    // The camera heading is the only intent signal our own writes cannot
    // contaminate: rotating the body does not move it.
    float camera_yaw = 0.0f;
    if (!g_camera_yaw_var) return;
    if (!read_graph_float(holder, g_camera_yaw_var, &camera_yaw)) return;

    if (fresh_sample) {
        const bool starting = !g_moving;
        if (starting) begin_movement(angle, relative, camera_yaw);
        else maintain_offset(direction, angle + relative, camera_yaw);

        const struct diagnostic_sample sample = {
            .code = starting ? DIAGNOSTIC_START
                             : (g_settled ? DIAGNOSTIC_SETTLED
                                          : DIAGNOSTIC_SETTLING),
            .angle = angle,
            .relative = relative,
            .heading = angle + relative,
            .measured = measured_heading(),
            .travel = sqrtf(g_sampled_x * g_sampled_x + g_sampled_y * g_sampled_y),
            .direction = read_or_unavailable(holder, g_direction_var),
            .camera_yaw = read_or_unavailable(holder, g_camera_yaw_var),
            .offset = g_offset,
            .speed = read_or_unavailable(holder, g_speed_var),
        };
        diagnostics_record(&sample);
    }

    // Outside the fresh sample gate: during a straight run `direction` is
    // constant, and recomputing the target only inside that gate would stop the
    // body from following the camera while the camera turns.
    if (!g_moving) return;

    const float target = camera_yaw + g_offset;
    if (rotator_failed()) write_angle_z(player, target);
    else rotator_set_target(target);
}
