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
static bool  g_snap_next = false;

static volatile long g_calls = 0;

// Chases the target along the shorter arc and returns the angle to write.
static float step_toward_target(void)
{
    if (g_snap_next) {
        g_current = g_target;
        g_snap_next = false;
        return g_current;
    }

    float delta = wrap_signed(g_target - g_current);
    if (g_config.max_step <= 0.0f) {
        g_current = g_target;
        return g_current;
    }
    if (delta > g_config.max_step) delta = g_config.max_step;
    if (delta < -g_config.max_step) delta = -g_config.max_step;
    g_current += delta;
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
    g_snap_next = true;
}

long rotator_hook_calls(void) { return g_calls; }

bool rotator_failed(void) { return g_failed; }
