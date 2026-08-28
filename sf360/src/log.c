#include "log.h"
#include "paths.h"

#include <stdarg.h>
#include <stdio.h>
#include <wchar.h>

void log_line(const char *fmt, ...)
{
    static wchar_t path[SF360_PATH_MAX];
    static int resolved = 0;
    if (!resolved) {
        log_path(path, SF360_PATH_MAX);
        resolved = 1;
    }

    FILE *f = _wfopen(path, L"a");
    if (!f) return;

    va_list args;
    va_start(args, fmt);
    vfprintf(f, fmt, args);
    va_end(args);
    fputc('\n', f);
    fclose(f);
}
