#include <windows.h>
#include <cstdio>
#include <string>

// Prova de vida da toolchain: usa Win32 (windows.h), a CRT (cstdio) e a STL
// (std::string). Se os tres linkarem, o CommonLibSF tambem vai.
extern "C" __declspec(dllexport) const char* SFSEPlugin_Version()
{
    static std::string s = "toolchain ok";
    char buf[64];
    std::snprintf(buf, sizeof buf, " (%lu)", GetTickCount());
    s += buf;
    return s.c_str();
}

BOOL APIENTRY DllMain(HMODULE, DWORD, LPVOID) { return TRUE; }
