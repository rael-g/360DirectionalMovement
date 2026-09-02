#include "movement_director.h"
#include "angles.h"
#include "config.h"
#include "diagnostics.h"
#include "frame_clock.h"
#include "game.h"
#include "layout_check.h"
#include "log.h"
#include "probe.h"
#include "rotator.h"
#include "state_gate.h"
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
static float g_last_direction = NO_DIRECTION;

// Octant the direction was in when the offset was last taken. A change of
// octant separates a new direction from the drift our own rotation feeds back.
#define NO_OCTANT (-1)
static int g_last_octant = NO_OCTANT;

// How long the current movement has been asked for. The rotator's clock cannot
// serve here: it only advances while there is a target, and the whole point is
// to measure the stretch before there is one.
static float g_start_seconds = 0.0f;
static struct frame_clock g_clock = { 0 };

static float g_last_x = 0.0f, g_last_y = 0.0f;
static float g_travel_x = 0.0f, g_travel_y = 0.0f;
static float g_sampled_x = 0.0f, g_sampled_y = 0.0f;

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
    // Declared Integer under this exact spelling. Any other reads nothing.
    if (!g_first_person_var) g_first_person_var = intern_string("bIsFirstPerson");
    if (!g_camera_yaw_var) g_camera_yaw_var = intern_string("fCameraYaw");
    return g_speed_var && g_direction_var;
}

// The player object appears before its animation graph is ready, so binding
// commits only once the layout check passes.
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
    state_gate_rebound();
    rotator_install(player);
    return true;
}

static void end_movement(void)
{
    if (g_moving) diagnostics_flush();
    g_moving = false;
    g_start_seconds = 0.0f;
    rotator_clear_target();
    // A movement resumed along the same heading must still read as fresh.
    g_last_direction = NO_DIRECTION;
}

static void begin_movement(float angle, float relative, float camera_yaw,
                           float direction)
{
    g_moving = true;
    g_settled = false;
    g_offset = wrap_signed((angle + relative) - camera_yaw);
    g_last_octant = octant_of(direction);
    g_previous_heading = angle + relative;
    rotator_reset(angle);
}

static void maintain_offset(float direction, float heading, float camera_yaw)
{
    // A change of octant means a new direction was asked for, so the captured
    // offset no longer describes intent. The replacement is taken on this
    // sample, while the body has not turned yet and the heading still carries
    // intent rather than our own rotation.
    if (g_config.recapture_on_switch) {
        const int octant = octant_of(direction);
        if (octant != g_last_octant) {
            g_offset = wrap_signed(heading - camera_yaw);
            g_last_octant = octant;
            g_previous_heading = heading;
            return;
        }
    }

    // `Direction` ramps up from zero, so the capture waits for the heading to
    // hold still and then happens once.
    if (!g_settled && g_config.recapture_on_settle) {
        if (fabsf(wrap_signed(heading - g_previous_heading)) < STABLE_HEADING) {
            g_offset = wrap_signed(heading - camera_yaw);
            g_settled = true;
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
    // Above every early return: a stale position charges the whole gap to the
    // first update of the next movement.
    accumulate_travel(player);

    // Read once per frame whatever happens below, so the interval never carries
    // the time spent behind an early return.
    const float frame_seconds = frame_clock_step(&g_clock);
    if (!g_moving) g_start_seconds += frame_seconds;

    // Above every gate, including the toggle: the states worth sampling are the
    // ones the gates reject.
    probe_actor_state(player);
    probe_scan(holder);

    // The hooks stay installed and go inert on the cleared target, which avoids
    // restoring vtable slots another plugin may have swapped since.
    if (!toggle_enabled()) {
        end_movement();
        return;
    }

    // Sitting, ladders, zero g, sprinting and a drawn weapon all leave here.
    // Above the speed test: the graph reports speed while seated.
    if (g_config.state_allowlist && !state_gate_allows(player)) {
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

    // `Direction` is the movement direction relative to the body, in turns, and
    // is exact from the first update of a movement.
    const float relative = wrap_signed(direction * SF360_TWO_PI);
    const float angle = get_angle_z(player);

    // The task runs several times per graph update while `direction` only
    // changes with the update.
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

    // Not gated on a fresh sample: a straight line holds `direction` still for
    // hundreds of updates. Gated on the movement lasting, since a tapped
    // direction key is not a commitment to face that way.
    const bool starting = !g_moving && g_start_seconds >= g_config.start_hold;
    if (starting) begin_movement(angle, relative, camera_yaw, direction);
    if (!g_moving) return;

    uint32_t state1 = 0, state2 = 0;
    get_actor_state(player, &state1, &state2);

    if (fresh_sample) {
        if (!starting)
            maintain_offset(direction, angle + relative, camera_yaw);

        const struct diagnostic_sample sample = {
            .code = starting ? DIAGNOSTIC_START
                             : (g_settled ? DIAGNOSTIC_SETTLED
                                          : DIAGNOSTIC_SETTLING),
            .angle = angle,
            .relative = relative,
            .heading = wrap_signed(angle + relative),
            .measured = measured_heading(),
            .travel = sqrtf(g_sampled_x * g_sampled_x + g_sampled_y * g_sampled_y),
            .direction = read_or_unavailable(holder, g_direction_var),
            .camera_yaw = read_or_unavailable(holder, g_camera_yaw_var),
            .offset = g_offset,
            .speed = read_or_unavailable(holder, g_speed_var),
            .jump = state_gate_jump_state(player),
            .state1 = state1,
            .state2 = state2,
        };
        diagnostics_record(&sample);
    }

    // Outside the fresh sample gate, so the body keeps following the camera
    // during a straight run, where `direction` is constant.
    if (!g_moving) return;

    const float target = camera_yaw + g_offset;
    if (rotator_failed()) write_angle_z(player, target);
    else rotator_set_target(target);
}
