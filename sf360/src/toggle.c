#include "toggle.h"
#include "config.h"
#include "log.h"

#include <windows.h>

static bool g_enabled = true;
static bool g_was_down = false;

// Key state is system wide, so without this a press meant for another window
// would toggle the mod while the game is not even in focus.
static bool game_has_focus(void)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &pid);
    return pid == GetCurrentProcessId();
}

void toggle_update(void)
{
    if (!g_config.toggle_key) return;

    const bool down = game_has_focus()
                      && (GetAsyncKeyState(g_config.toggle_key) & 0x8000) != 0;

    // Only the transition counts. Testing the key as held would flip the state
    // on every frame the finger stays down.
    if (down && !g_was_down) {
        g_enabled = !g_enabled;
        log_line("toggled %s", g_enabled ? "on" : "off");
    }
    g_was_down = down;
}

bool toggle_enabled(void) { return g_enabled; }
