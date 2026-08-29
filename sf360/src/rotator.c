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

// Angle actually written. The target may jump; this chases it at a bounded
// rate, matching how fast the game turns the body on its own.
static float g_current = 0.0f;

// The first step of a movement goes straight to the target instead of being
// eased. That is not a shortcut, it is load bearing, and taking it out proved
// it: we rotate the body by writing its angle, which tells the animation graph
// nothing, so the legs keep running the old cycle while the body turns under
// them. An instant turn gives that mismatch no time to show. Easing a half turn
// over a third of a second puts it on screen, and it reads as the character
// ducking mid turn instead of braking and pivoting the way the game does when
// it rotates the actor itself.
static bool  g_snap_next = false;

// Radians per second. Kept between calls because it is what makes the body
// accelerate into a turn: a step computed from the remaining arc alone is at
// full speed on its very first frame, which is the part that still read as
// abrupt once the arrival had been smoothed.
static float g_velocity = 0.0f;

// Turns the configured settle time into the spring frequency that settles in
// about that long without overshooting.
#define CRITICAL_DAMPING_GAIN 2.0f

static volatile long g_calls = 0;

// A frame long enough to have been a load screen or an alt tab. Stepping by it
// would close the whole arc at once, which is the snap this module exists to
// avoid, so it is treated as one ordinary frame instead.
#define LONGEST_FRAME 0.1f

// The graph update carries no timestep this code can reach, and the hook does
// not fire at a fixed rate, so anything measured per call is really per frame
// and turns faster on a faster machine. This measures the wall clock instead.
static double g_seconds_per_tick = 0.0;
static LARGE_INTEGER g_last_tick = { 0 };

static float elapsed_seconds(void)
{
    if (g_seconds_per_tick == 0.0) {
        LARGE_INTEGER frequency;
        // Without a clock the step has nothing to scale by. Reporting the
        // longest accepted frame keeps the body turning, slowly and at the
        // wrong speed, rather than freezing it facing one way.
        if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart == 0)
            return LONGEST_FRAME;
        g_seconds_per_tick = 1.0 / (double)frequency.QuadPart;
    }

    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    const LONGLONG previous = g_last_tick.QuadPart;
    g_last_tick = now;

    // No previous sample, so there is no interval to report yet.
    if (previous == 0) return 0.0f;

    const float dt = (float)((double)(now.QuadPart - previous) * g_seconds_per_tick);
    if (dt <= 0.0f) return 0.0f;
    return dt < LONGEST_FRAME ? dt : LONGEST_FRAME;
}

// Chases the target along the shorter arc and returns the angle to write.
static float g_last_dt = 0.0f;

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

    // The hook fires more than once per frame. Without an interval there is no
    // step to take, and taking a full one anyway would turn several times as
    // fast on whichever machine calls it most often.
    if (dt <= 0.0f) return g_current;

    const float delta = wrap_signed(g_target - g_current);

    if (g_config.turn_smoothing > 0.0f) {
        // Critically damped: the pull towards the target is opposed by a drag
        // on the current speed, tuned so the body arrives without swinging
        // past. Both ends of the turn are eased, which is the part a step
        // proportional to the remaining arc cannot do.
        float omega = CRITICAL_DAMPING_GAIN / g_config.turn_smoothing;
        // Integrating a step this way is only stable while omega times the
        // frame stays under one. On a long frame the response is slowed rather
        // than allowed to oscillate.
        if (omega * dt > 1.0f) omega = 1.0f / dt;
        g_velocity += (omega * omega * delta - 2.0f * omega * g_velocity) * dt;
    } else {
        g_velocity = delta / dt;
    }

    // The easing alone still swings fast when the arc is wide, so a ceiling on
    // the speed keeps a half turn from happening in a couple of frames.
    if (g_config.turn_rate > 0.0f) {
        const float cap = g_config.turn_rate / SF360_RAD_TO_DEG;
        if (g_velocity > cap) g_velocity = cap;
        if (g_velocity < -cap) g_velocity = -cap;
    }

    float step = g_velocity * dt;

    // The speed ceiling can leave the body carrying more speed than the arc has
    // left to give, so the last step is trimmed. Passing the target and coming
    // back is a wobble, and it is worse than arriving a frame early.
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
    g_snap_next = true;
    // The previous step may have been long ago, and an interval measured back
    // to it would spend the whole gap in one step, which is the snap again
    // wearing a different hat. The next step measures from now instead.
    g_last_tick.QuadPart = 0;
}

long rotator_hook_calls(void) { return g_calls; }

float rotator_last_dt(void) { return g_last_dt; }

float rotator_driven_angle(void) { return g_current; }

bool rotator_failed(void) { return g_failed; }
