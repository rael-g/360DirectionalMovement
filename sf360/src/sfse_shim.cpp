// The only C++ in the plugin.
//
// SFSE hands out per-frame work through ITaskDelegate, an abstract class, so
// registering for it requires a C++ vtable. Keeping that requirement in one
// file lets the rest of the plugin be C while still compiling against the
// official SFSE headers, which stay the source of truth for the ABI.

#include <windows.h>

#include "PluginAPI.h"
#include "sfse_version.h"

extern "C" {
#include "log.h"
#include "plugin.h"
}

namespace {

// Runs on the main thread once per frame. Everything that touches a game object
// belongs here: calling game virtuals from a private thread would race against
// the destruction and recreation of those objects and would contend with the
// render thread for the string pool lock.
class FrameTask : public SFSETaskInterface::ITaskDelegate
{
public:
    void Run() override { sf360_update(); }
    void Destroy() override {}
};

FrameTask g_task;

// Bumped whenever the plugin ships; SFSE only reports it.
constexpr std::uint32_t kPluginVersion = 2;

}  // namespace

extern "C" {

// The declared runtime is a hard gate, and deliberately so. The struct offsets
// and vtable slots this plugin relies on are not version independent, so
// loading blind on an untested build would corrupt memory instead of failing.
__declspec(dllexport) SFSEPluginVersionData SFSEPlugin_Version =
{
    SFSEPluginVersionData::kVersion,

    kPluginVersion,
    "360 Directional Movement",
    "raelg",

    SFSEPluginVersionData::kAddressIndependence_AddressLibraryV2,
    SFSEPluginVersionData::kStructureIndependence_NoStructs,
    { RUNTIME_VERSION_1_16_244, 0 },

    0,
    0, 0,
};

__declspec(dllexport) bool SFSEPlugin_Load(const SFSEInterface* sfse)
{
    // Returning true even on failure lets the log explain the reason.
    if (!sf360_startup(sfse->sfseVersion, sfse->runtimeVersion)) return true;

    auto tasks = static_cast<SFSETaskInterface*>(
        sfse->QueryInterface(kInterface_Task));
    if (tasks && tasks->AddTaskPermanent) {
        tasks->AddTaskPermanent(&g_task);
        log_line("frame task registered (interface %u)", tasks->interfaceVersion);
    } else {
        log_line("failed: SFSE task interface unavailable");
    }
    return true;
}

}  // extern "C"

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
