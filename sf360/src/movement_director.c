#include "movement_director.h"
#include "angles.h"
#include "config.h"
#include "diagnostics.h"
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
#include <windows.h>

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

// Beyond this the movement counts as a reversal rather than a turn.
#define REVERSAL_ANGLE 2.36f

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

// A candidate octant and how long it has held. An analogue stick crosses
// octant boundaries on the way to anywhere, so a change that does not survive
// this long is a boundary being brushed rather than a direction being asked
// for.
static int   g_pending_octant = NO_OCTANT;
static float g_pending_seconds = 0.0f;

// How long the current movement has been asked for. The rotator's clock cannot
// serve here: it only advances while there is a target, and the whole point is
// to measure the stretch before there is one.
static float g_start_seconds = 0.0f;
static LARGE_INTEGER g_start_tick = { 0 };
static double g_seconds_per_tick = 0.0;

// Longer than this was a load screen, not a frame.
#define LONGEST_FRAME 0.1f

static float since_last_frame(void)
{
    if (g_seconds_per_tick == 0.0) {
        LARGE_INTEGER frequency;
        if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart == 0)
            return LONGEST_FRAME;
        g_seconds_per_tick = 1.0 / (double)frequency.QuadPart;
    }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const LONGLONG previous = g_start_tick.QuadPart;
    g_start_tick = now;
    if (previous == 0) return 0.0f;

    const float dt = (float)((double)(now.QuadPart - previous) * g_seconds_per_tick);
    if (dt <= 0.0f) return 0.0f;
    return dt < LONGEST_FRAME ? dt : LONGEST_FRAME;
}

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
    // Without this, stopping and moving again along the same heading would
    // leave `Direction` unchanged, suppress the start branch entirely and carry
    // the previous movement's offset over.
    g_last_direction = NO_DIRECTION;
}

// The only reading of `Direction` taken as absolute truth. Afterwards it is
// contaminated: the reported direction rotates with the body, so feeding it
// back would have positive gain.
static void begin_movement(float angle, float relative, float camera_yaw,
                           float direction)
{
    g_moving = true;
    g_settled = false;
    g_last_octant = octant_of(direction);
    g_pending_octant = NO_OCTANT;
    g_pending_seconds = 0.0f;
    g_offset = wrap_signed((angle + relative) - camera_yaw);
    g_previous_heading = angle + relative;
    rotator_reset(angle);
}

static void maintain_offset(float direction, float heading, float camera_yaw)
{
    // A change of octant means the player asked for a new direction, so the
    // captured offset no longer describes intent. The replacement is taken on
    // this sample because the body has not turned yet, leaving the heading
    // still describing intent rather than our own rotation.
    //
    // Deliberately not gated on having settled: the game blends `Direction`
    // towards a new input an eighth of a turn at a time, so during a quick
    // change the heading never holds still and nothing would ever settle.
    if (g_config.recapture_on_switch) {
        const int octant = octant_of(direction);
        if (octant == g_last_octant) {
            g_pending_octant = NO_OCTANT;
            g_pending_seconds = 0.0f;
        } else {
            // The clock measures time spent away from the octant the offset was
            // taken in, not time spent in one candidate. Restarting it on every
            // new candidate meant a zig zag faster than the hold never committed
            // to anything, and the body stayed pointed the way it started.
            g_pending_octant = octant;
            g_pending_seconds += rotator_last_dt();

            // A reversal waits longer than a sideways change. The game answers
            // one with a brake and a step the other way, and that transition is
            // worth letting finish: turning across it is what makes the
            // character appear to duck partway through a half turn.
            const float turn_size = fabsf(wrap_signed(direction * SF360_TWO_PI));
            const float hold = (turn_size > REVERSAL_ANGLE)
                               ? g_config.reversal_hold : g_config.direction_hold;

            if (g_pending_seconds >= hold) {
                g_offset = wrap_signed(heading - camera_yaw);
                g_last_octant = octant;
                g_pending_octant = NO_OCTANT;
                g_pending_seconds = 0.0f;
                g_previous_heading = heading;
            }
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

    // Read once per frame whatever happens below, so the interval never carries
    // the time spent behind an early return.
    const float frame_seconds = since_last_frame();
    if (!g_moving) g_start_seconds += frame_seconds;

    // Above every gate, including the toggle: the states it exists to describe
    // are the ones the mod refuses to run in, so sampling below a gate would
    // record everything except what is being looked for.
    probe_actor_state(player);
    probe_scan(holder);

    // Switched off from the keyboard. Below accumulate_travel so the position
    // stays current: whatever happened while the mod was off must not be
    // charged to the first update after it comes back.
    //
    // The hooks stay installed and go inert on the cleared target, which avoids
    // restoring vtable slots that another plugin may have swapped since.
    if (!toggle_enabled()) {
        end_movement();
        return;
    }

    // Sitting, ladders, zero g, sprinting and a drawn weapon all leave here.
    // Above the speed test on purpose: the graph reports speed while seated, so
    // anything behind that test would read sitting as ordinary movement.
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

    // A jump keeps whatever heading it left the ground with. Recapturing here
    // would chase the arc, and standing down would let the game swing the body
    // round to face forward halfway through.
    const bool jumping = g_config.freeze_when_jumping
                         && state_gate_is_jumping(player);

    // Starting is not gated on a fresh sample. Walking in a straight line holds
    // `direction` still for hundreds of updates, so a movement ended by one
    // frame of a veto would stay dead until the player turned.
    //
    // It is gated on the movement lasting, though. Tapping a direction key does
    // not turn the character in the base game: one press is not enough to commit
    // to it. Turning on the first update made every tap swing the body round,
    // which is the transition players describe as missing.
    const bool starting = !g_moving && g_start_seconds >= g_config.start_hold;
    if (starting) begin_movement(angle, relative, camera_yaw, direction);
    if (!g_moving) return;

    uint32_t state1 = 0, state2 = 0;
    get_actor_state(player, &state1, &state2);

    if (fresh_sample) {
        if (!starting && !jumping)
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
            .jump = state_gate_raw(player, 1),
            .state1 = state1,
            .state2 = state2,
        };
        diagnostics_record(&sample);
    }

    // Outside the fresh sample gate: during a straight run `direction` is
    // constant, and recomputing the target only inside that gate would stop the
    // body from following the camera while the camera turns.
    if (!g_moving) return;

    // The rotator keeps the target it already has, so the body holds its
    // heading through the arc even if the camera swings.
    if (jumping) return;

    const float target = camera_yaw + g_offset;
    if (rotator_failed()) write_angle_z(player, target);
    else rotator_set_target(target);
}
