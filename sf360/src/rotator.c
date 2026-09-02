#include "rotator.h"
#include "angles.h"
#include "config.h"
#include "game.h"
#include "log.h"

#include <windows.h>

typedef void (*update_graph_fn)(void *, const void *);

static update_graph_fn g_original_pre_update = NULL;
static update_graph_fn g_original_post_update = NULL;
static void *g_holder = NULL;
static void *g_player = NULL;
static bool  g_failed = true;

static bool  g_has_target = false;
static float g_target = 0.0f;

// The angle written to the actor, as opposed to the one asked for.
static float g_current = 0.0f;

// Set to take the next step straight to the target instead of easing it.
static bool  g_snap_next = false;

// Radians per second, carried between steps so a turn can accelerate.
static float g_velocity = 0.0f;

#define SETTLE_TIME_TO_FREQUENCY 2.0f
#define CRITICAL_DAMPING 2.0f

// Beyond this the integration below rings instead of settling.
#define STABLE_FREQUENCY_TIMES_FRAME 1.0f

static volatile long g_calls = 0;

// Longer than this was a load screen, not a frame, and is charged as one frame.
#define LONGEST_FRAME 0.1f

// The graph update carries no timestep reachable from here, and the hook does
// not fire at a fixed rate, so the interval is measured off the wall clock.
static double g_seconds_per_tick = 0.0;
static LARGE_INTEGER g_last_tick = { 0 };

static float elapsed_seconds(void)
{
    if (g_seconds_per_tick == 0.0) {
        LARGE_INTEGER frequency;
        // Turning at the wrong speed beats not turning at all.
        if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart == 0)
            return LONGEST_FRAME;
        g_seconds_per_tick = 1.0 / (double)frequency.QuadPart;
    }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const LONGLONG previous = g_last_tick.QuadPart;
    g_last_tick = now;

    if (previous == 0) return 0.0f;

    const float dt = (float)((double)(now.QuadPart - previous) * g_seconds_per_tick);
    if (dt <= 0.0f) return 0.0f;
    return dt < LONGEST_FRAME ? dt : LONGEST_FRAME;
}

static float g_last_dt = 0.0f;

// Chases the target along the shorter arc and returns the angle to write.
static float step_toward_target(void)
{
    const float dt = elapsed_seconds();
    if (dt > 0.0f) g_last_dt = dt;

    if (g_snap_next) {
        g_current = g_target;
        g_velocity = 0.0f;
        g_snap_next = false;
        return g_current;
    }

    // The hook can fire twice within one tick, and a step unscaled by an
    // interval would turn faster the more often that happens.
    if (dt <= 0.0f) return g_current;

    const float delta = wrap_signed(g_target - g_current);

    if (g_config.turn_smoothing > 0.0f) {
        // Critically damped: eased at both ends, arriving without overshoot.
        float omega = SETTLE_TIME_TO_FREQUENCY / g_config.turn_smoothing;
        if (omega * dt > STABLE_FREQUENCY_TIMES_FRAME)
            omega = STABLE_FREQUENCY_TIMES_FRAME / dt;
        g_velocity += (omega * omega * delta
                       - CRITICAL_DAMPING * omega * g_velocity) * dt;
    } else {
        g_velocity = delta / dt;
    }

    // The spring alone swings fast when the arc is wide.
    if (g_config.turn_rate > 0.0f) {
        const float cap = g_config.turn_rate * SF360_DEG_TO_RAD;
        if (g_velocity > cap) g_velocity = cap;
        if (g_velocity < -cap) g_velocity = -cap;
    }

    float step = g_velocity * dt;

    // Arriving a frame early beats passing the target and coming back.
    if ((delta >= 0.0f && step > delta) || (delta <= 0.0f && step < delta)) {
        step = delta;
        g_velocity = 0.0f;
    }

    g_current = wrap_unsigned(g_current + step);
    return g_current;
}

// The last instant before the graph consumes the actor state.
static void pre_update_hook(void *holder, const void *manager)
{
    // The original first: it is a game method and may have side effects.
    if (g_original_pre_update) g_original_pre_update(holder, manager);

    // The vtable belongs to the class, not the instance. Without this guard
    // any NPC sharing the PlayerCharacter vtable would be rotated too.
    if (holder != g_holder || !g_player) return;

    InterlockedIncrement(&g_calls);
    if (g_has_target) write_angle_z(g_player, step_toward_target());
}

// The first instant after the game has applied its own rotation, so this write
// has the final say. It repeats the value the step already produced; the point
// is the ordering, not a second step.
static void post_update_hook(void *holder, const void *manager)
{
    if (g_original_post_update) g_original_post_update(holder, manager);
    if (holder != g_holder || !g_player) return;
    if (!g_has_target) return;
    write_angle_z(g_player, g_current);
}

// Swaps a vtable entry. No trampoline and no Address Library entry is needed
// because the pointer comes from the live object.
static bool swap_slot(void **vtable, size_t slot, void *replacement,
                      update_graph_fn *saved)
{
    void *original = vtable[slot];
    if (!inside_module(original)) {
        log_line("hook: slot 0x%zX = %p is outside the module", slot, original);
        return false;
    }

    DWORD previous = 0;
    if (!VirtualProtect(&vtable[slot], sizeof(void *), PAGE_READWRITE, &previous)) {
        log_line("hook: VirtualProtect failed on slot 0x%zX (%lu)",
                 slot, GetLastError());
        return false;
    }

    *saved = (update_graph_fn)original;
    vtable[slot] = replacement;

    DWORD discarded = 0;
    VirtualProtect(&vtable[slot], sizeof(void *), previous, &discarded);

    log_line("hook installed: vtable=%p [0x%zX] original=%p -> %p",
             vtable, slot, original, replacement);
    return true;
}

bool rotator_install(void *player)
{
    rotator_clear_target();

    void *holder = holder_of(player);
    void **vtable = *(void ***)holder;
    if (!inside_module(vtable)) {
        log_line("hook: vtable %p is outside the module", vtable);
        g_failed = true;
        return false;
    }

    if (!swap_slot(vtable, SLOT_PRE_UPDATE, (void *)&pre_update_hook,
                   &g_original_pre_update)) {
        g_failed = true;
        return false;
    }

    // PostUpdate is optional: PreUpdate alone still rotates the body, it just
    // leaves a constant offset behind.
    if (g_config.hook_post)
        swap_slot(vtable, SLOT_POST_UPDATE, (void *)&post_update_hook,
                  &g_original_post_update);

    g_holder = holder;
    g_player = player;
    g_failed = false;
    return true;
}

void rotator_set_target(float radians)
{
    g_target = radians;
    g_has_target = true;
}

void rotator_clear_target(void) { g_has_target = false; }

void rotator_reset(float current_angle)
{
    g_current = current_angle;
    g_velocity = 0.0f;
    g_snap_next = g_config.snap_on_start;
    // The next step measures from now, not back across however long the body
    // spent standing still.
    g_last_tick.QuadPart = 0;
}

long rotator_hook_calls(void) { return g_calls; }

float rotator_last_dt(void) { return g_last_dt; }

float rotator_driven_angle(void) { return g_current; }

bool rotator_failed(void) { return g_failed; }
