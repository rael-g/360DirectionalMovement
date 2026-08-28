#include <windows.h>
#include <cstdio>
int main() {
    HMODULE h = LoadLibraryA("hello.dll");
    if (!h) { std::printf("FALHOU LoadLibrary: %lu\n", GetLastError()); return 1; }
    auto f = (const char*(*)())GetProcAddress(h, "SFSEPlugin_Version");
    if (!f) { std::printf("FALHOU GetProcAddress\n"); return 2; }
    std::printf("RETORNOU: %s\n", f());
    return 0;
}
