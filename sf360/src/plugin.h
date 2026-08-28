#pragma once

#include <stdbool.h>
#include <stdint.h>

// Entry points the SFSE shim calls. Everything below this line is C; the shim
// is the only translation unit that has to be C++, because SFSE hands out work
// through an abstract class.
bool sf360_startup(uint32_t sfse_version, uint32_t runtime_version);
void sf360_update(void);
