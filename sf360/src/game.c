#include "game.h"
#include "angles.h"

typedef void (*get_entry_fn)(void **out, const char *text, bool case_sensitive);
typedef bool (*read_float_fn)(void *, void *const *, float *);
typedef bool (*read_int_fn)(void *, void *const *, int32_t *);
typedef bool (*read_bool_fn)(void *, void *const *, bool *);

void *intern_string(const char *text)
{
    static get_entry_fn func = NULL;
    static bool resolved = false;
    if (!resolved) {
        func = (get_entry_fn)address_resolve(ID_GET_ENTRY);
        resolved = true;
    }
    if (!func) return NULL;

    void *entry = NULL;
    func(&entry, text, false);  // BSFixedString comparison is case-insensitive
    return entry;
}

void *get_player(void)
{
    const uintptr_t addr = address_resolve(ID_PLAYER_SINGLETON);
    if (!addr) return NULL;

    void *p = *(void **)addr;
    if (!p) return NULL;
    if ((uintptr_t)p < MIN_VALID_POINTER) return NULL;
    if (!inside_module(*(void **)p)) return NULL;
    return p;
}

// The vtable is validated on every call: the holder belongs to an object the
// game may be rebuilding underneath us.
bool read_graph_float(void *holder, void *interned_name, float *out)
{
    void **vtable = *(void ***)holder;
    if (!inside_module(vtable) || !inside_module(vtable[SLOT_FLOAT])) return false;
    return ((read_float_fn)vtable[SLOT_FLOAT])(holder, &interned_name, out);
}

// Most of the graph's flags are declared Integer rather than Boolean, so this
// is not the rare case it looks like.
bool read_graph_int(void *holder, void *interned_name, int32_t *out)
{
    void **vtable = *(void ***)holder;
    if (!inside_module(vtable) || !inside_module(vtable[SLOT_INT])) return false;
    return ((read_int_fn)vtable[SLOT_INT])(holder, &interned_name, out);
}

bool read_graph_bool(void *holder, void *interned_name, bool *out)
{
    void **vtable = *(void ***)holder;
    if (!inside_module(vtable) || !inside_module(vtable[SLOT_BOOL])) return false;
    return ((read_bool_fn)vtable[SLOT_BOOL])(holder, &interned_name, out);
}

float get_angle_z(void *refr)
{
    return *(float *)((char *)refr + ANGLE_Z_OFFSET);
}

void write_angle_z(void *refr, float radians)
{
    *(float *)((char *)refr + ANGLE_Z_OFFSET) = wrap_unsigned(radians);
}

void get_planar_position(void *refr, float *x, float *y)
{
    const float *p = (const float *)((char *)refr + LOCATION_OFFSET);
    *x = p[0];
    *y = p[1];
}

void get_actor_state(void *refr, uint32_t *first, uint32_t *second)
{
    const uint32_t *words = (const uint32_t *)((char *)refr + ACTOR_STATE_OFFSET);
    *first = words[0];
    *second = words[1];
}

// The field walks through several values while the entry animation places the
// body and holds at the last of them for as long as the character stays
// seated. Only the standing value has a known meaning, so the test is against
// that one rather than against a named step.
bool is_sitting(void *refr)
{
    uint32_t first = 0, second = 0;
    get_actor_state(refr, &first, &second);
    return ((second >> SIT_STATE_SHIFT) & SIT_STATE_MASK) != SIT_STATE_STANDING;
}

bool is_weapon_drawn(void *refr)
{
    uint32_t first = 0, second = 0;
    get_actor_state(refr, &first, &second);
    return (second & WEAPON_STATE_MASK) != WEAPON_SHEATHED;
}
