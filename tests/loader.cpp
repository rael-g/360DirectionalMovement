#include <windows.h>
#include <cstdio>

// Loads the smoke test DLL and calls its exported function, confirming the
// cross compiled binary is a valid Windows module end to end.
int main()
{
    HMODULE module = LoadLibraryA("hello.dll");
    if (!module) {
        std::printf("LoadLibrary failed: %lu\n", GetLastError());
        return 1;
    }
    auto entry = reinterpret_cast<const char* (*)()>(
        GetProcAddress(module, "SFSEPlugin_Version"));
    if (!entry) {
        std::printf("GetProcAddress failed\n");
        return 2;
    }
    std::printf("returned: %s\n", entry());
    return 0;
}
