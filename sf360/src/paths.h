#pragma once

#include <wchar.h>

#define SF360_PATH_MAX 512

// Paths are wide end to end. Narrowing them would mean picking a code page,
// and the ANSI one cannot represent every path Windows accepts.
void sfse_folder(wchar_t *out, int capacity);
void log_path(wchar_t *out, int capacity);
void config_path(wchar_t *out, int capacity);
