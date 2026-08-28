#include "address_library.h"
#include "log.h"

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>

// V5 header: fileVersion i32, gameVersion u32[4], name char[64],
// pointerSize i32, dataFormat i32, offsetCount i32. Then a flat u32 array
// indexed by ID itself; zero means the ID does not exist in this build.
#define FORMAT_V5          5
#define VERSION_FIELD_COUNT 4
#define NAME_FIELD_SIZE    64

static const size_t header_size_v5 =
    sizeof(int32_t)
    + VERSION_FIELD_COUNT * sizeof(uint32_t)
    + NAME_FIELD_SIZE
    + 3 * sizeof(int32_t);

static uint32_t *g_offsets = NULL;
static size_t    g_offset_count = 0;

bool address_library_load(const wchar_t *path)
{
    FILE *f = _wfopen(path, L"rb");
    if (!f) return false;

    int32_t format = 0;
    fread(&format, sizeof format, 1, f);
    if (format != FORMAT_V5) {
        log_line("versionlib: format %d is not supported (expected %d)",
                 format, FORMAT_V5);
        fclose(f);
        return false;
    }

    uint32_t version[VERSION_FIELD_COUNT] = { 0 };
    char     name[NAME_FIELD_SIZE] = { 0 };
    int32_t  pointer_size = 0, data_format = 0, count = 0;
    fread(version, sizeof version[0], VERSION_FIELD_COUNT, f);
    fread(name, 1, NAME_FIELD_SIZE, f);
    fread(&pointer_size, sizeof pointer_size, 1, f);
    fread(&data_format, sizeof data_format, 1, f);
    fread(&count, sizeof count, 1, f);

    if (count <= 0) {
        fclose(f);
        return false;
    }

    free(g_offsets);
    g_offset_count = (size_t)count;
    g_offsets = malloc(g_offset_count * sizeof *g_offsets);
    if (!g_offsets) {
        g_offset_count = 0;
        fclose(f);
        return false;
    }

    fseek(f, (long)header_size_v5, SEEK_SET);
    const size_t read = fread(g_offsets, sizeof *g_offsets, g_offset_count, f);
    fclose(f);

    log_line("versionlib: %u.%u.%u.%u '%s', %d entries, %zu read",
             version[0], version[1], version[2], version[3], name, count, read);
    return read == g_offset_count;
}

uintptr_t module_base(void)
{
    static uintptr_t base = 0;
    if (!base) base = (uintptr_t)GetModuleHandleW(NULL);
    return base;
}

uintptr_t address_resolve(uint32_t id)
{
    if (id >= g_offset_count || !g_offsets[id]) return 0;
    return module_base() + g_offsets[id];
}

bool inside_module(const void *p)
{
    static uintptr_t begin = 0, end = 0;
    if (!end) {
        const uintptr_t base = module_base();
        const IMAGE_DOS_HEADER *dos = (const IMAGE_DOS_HEADER *)base;
        const IMAGE_NT_HEADERS64 *nt =
            (const IMAGE_NT_HEADERS64 *)(base + dos->e_lfanew);
        begin = base;
        end = base + nt->OptionalHeader.SizeOfImage;
    }
    const uintptr_t v = (uintptr_t)p;
    return v >= begin && v < end;
}
