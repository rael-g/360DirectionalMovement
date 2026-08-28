#include <windows.h>
#include <cstdio>
#include <string>

// Toolchain smoke test: exercises Win32 (windows.h), the CRT (cstdio) and the
// STL (std::string). If all three link, the real plugin will link too.
extern "C" __declspec(dllexport) const char* SFSEPlugin_Version()
{
    static std::string s = "toolchain ok";
    char buffer[64];
    std::snprintf(buffer, sizeof buffer, " (%lu)", GetTickCount());
    s += buffer;
    return s.c_str();
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
