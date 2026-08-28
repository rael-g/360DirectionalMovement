#pragma once

#include <stdbool.h>
#include <stdint.h>

// Loads a version database and resolves IDs against the running module.
// Address Library keeps an ID stable across game builds, which is what makes
// the lookups this plugin needs survive a game update.
bool address_library_load(const wchar_t *path);

uintptr_t module_base(void);
uintptr_t address_resolve(uint32_t id);

// True when the pointer lies inside the game executable's image.
bool inside_module(const void *p);
