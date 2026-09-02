#include "plugin.h"
#include "address_library.h"
#include "config.h"
#include "game.h"
#include "log.h"
#include "movement_director.h"
#include "paths.h"

#include <windows.h>
#include <wchar.h>

// Macro only header, so it is usable from C and stays the source of truth for
// how a packed version integer is laid out.
#include "sfse_version.h"

// The Address Library ships one database per game build, named after that
// build. Deriving the name from the running version means a game update only
// needs an updated database, not a new plugin binary.
static void database_path(wchar_t *out, int capacity, uint32_t runtime_version)
{
    wchar_t exe[SF360_PATH_MAX] = { 0 };
    GetModuleFileNameW(NULL, exe, SF360_PATH_MAX);

    wchar_t *last = wcsrchr(exe, L'\\');
    if (last) *last = L'\0';

    swprintf(out, capacity,
             L"%ls\\Data\\SFSE\\Plugins\\versionlib-%u-%u-%u-0.bin",
             exe,
             GET_EXE_VERSION_MAJOR(runtime_version),
             GET_EXE_VERSION_MINOR(runtime_version),
             GET_EXE_VERSION_BUILD(runtime_version));
}

bool sf360_startup(uint32_t sfse_version, uint32_t runtime_version)
{
    config_load();

    log_line("--- 360 Directional Movement " SF360_VERSION " ---");
    log_line("config: hookPost=%d recaptureOnSettle=%d recaptureOnSwitch=%d "
             "turnRate=%.1f turnSmoothing=%.3f "
             "startHold=%.3f snapOnStart=%d minSpeed=%.3f yieldWhenSitting=%d "
             "yieldWhenSneaking=%d allowSprint=%d "
             "toggleKey=%d probeStates=%d scanStates=%d stateAllowlist=%d",
             g_config.hook_post, g_config.recapture_on_settle,
             g_config.recapture_on_switch, g_config.turn_rate,
             g_config.turn_smoothing,
             g_config.start_hold,
             g_config.snap_on_start,
             g_config.min_speed,
             g_config.yield_when_sitting, g_config.yield_when_sneaking,
             g_config.allow_sprint,
             g_config.toggle_key, g_config.probe_states,
             g_config.scan_states, g_config.state_allowlist);
    log_line("sfse %u.%u.%u | runtime %u.%u.%u",
             GET_EXE_VERSION_MAJOR(sfse_version),
             GET_EXE_VERSION_MINOR(sfse_version),
             GET_EXE_VERSION_BUILD(sfse_version),
             GET_EXE_VERSION_MAJOR(runtime_version),
             GET_EXE_VERSION_MINOR(runtime_version),
             GET_EXE_VERSION_BUILD(runtime_version));

    wchar_t database[SF360_PATH_MAX];
    database_path(database, SF360_PATH_MAX, runtime_version);

    if (!address_library_load(database)) {
        log_line("failed to load %ls", database);
        return false;
    }

    log_line("player singleton = %p | GetEntry = %p",
             (void *)address_resolve(ID_PLAYER_SINGLETON),
             (void *)address_resolve(ID_GET_ENTRY));
    return true;
}

void sf360_update(void) { movement_director_update(); }
