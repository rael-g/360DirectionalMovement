#include "config.h"
#include "log.h"
#include "paths.h"

#include <stdio.h>
#include <string.h>
#include <windows.h>

struct config g_config = {
    .hook_post = true,
    .recapture_on_settle = true,
    .recapture_on_switch = true,
    .max_step = 0.5f,
    .min_speed = 0.5f,
    .yield_when_sitting = true,
    // F11. F5 and F9 are the game's quicksave and quickload, and F12 is the
    // Steam screenshot, so this is the nearest key that is free by default.
    .toggle_key = VK_F11,
    .probe_states = false,
};

void config_load(void)
{
    wchar_t path[SF360_PATH_MAX];
    config_path(path, SF360_PATH_MAX);

    FILE *f = _wfopen(path, L"r");
    if (!f) return;

    char line[256];
    while (fgets(line, sizeof line, f)) {
        if (line[0] == ';' || line[0] == '\n') continue;

        char key[64] = { 0 };
        float value = 0.0f;
        if (sscanf(line, "%63[^=]=%f", key, &value) != 2) continue;
        const bool on = value != 0.0f;

        if (!strcmp(key, "hookPost")) g_config.hook_post = on;
        else if (!strcmp(key, "recaptureOnSettle")) g_config.recapture_on_settle = on;
        else if (!strcmp(key, "recaptureOnSwitch")) g_config.recapture_on_switch = on;
        else if (!strcmp(key, "maxStep")) g_config.max_step = value;
        else if (!strcmp(key, "minSpeed")) g_config.min_speed = value;
        else if (!strcmp(key, "yieldWhenSitting")) g_config.yield_when_sitting = on;
        // A virtual key code, not a flag, so the whole value is kept.
        else if (!strcmp(key, "toggleKey")) g_config.toggle_key = (int)value;
        else if (!strcmp(key, "probeStates")) g_config.probe_states = on;
        // Silently ignoring a key means editing it appears to do nothing, and
        // the reader concludes the behaviour it names is irrelevant.
        else log_line("config: ignoring unknown key '%s'", key);
    }
    fclose(f);
}
