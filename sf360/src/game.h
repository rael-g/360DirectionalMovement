#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "address_library.h"

// Layout of the game's objects, as depended on below. Every offset and slot
// here is a fact about one build of the executable, so a game update is
// expected to invalidate them; layout_check exists to notice when it does.
//
// ActorState is the one entry not shared with CommonLibSF, whose Actor + 0xE8
// holds a pointer on this build. The bitfield words start at 0xF8.

#define ID_PLAYER_SINGLETON 922868u
#define ID_GET_ENTRY        1186742u

#define HOLDER_OFFSET   0x60
#define SLOT_FLOAT      0x12
#define SLOT_INT        0x13
#define SLOT_BOOL       0x14
#define SLOT_PRE_UPDATE 0x15
#define SLOT_POST_UPDATE 0x16

// TESObjectREFR::data lives at +0x80; OBJ_REFR starts with NiPoint3 angle,
// whose z component is at +8, followed by location.
#define ANGLE_Z_OFFSET  (0x80 + 8)
#define LOCATION_OFFSET (0x80 + 0x0C)

// ActorState's two bitfield words, actorState1 then actorState2. The sit and
// sleep state is a two bit field in the second of them.
#define ACTOR_STATE_OFFSET 0xF8
#define SIT_STATE_SHIFT    11
#define SIT_STATE_MASK     0x3u
#define SIT_STATE_STANDING 0u

// The weapon state shares the second word, in its low three bits. It walks
// sheathed, drawing, drawn, sheathing, and only the sheathed value counts as
// put away: treating the transitional values as drawn keeps the mod from
// switching on halfway through the animation.
#define WEAPON_STATE_MASK  0x7u
#define WEAPON_SHEATHED    0u

// Addresses below this are never valid user space pointers on Win64.
#define MIN_VALID_POINTER 0x10000

// Returns the player only when its vtable is sane. Loading a save destroys and
// rebuilds the object, and calling a virtual during that window would jump to
// an arbitrary address.
void *get_player(void);

// Graph variables are looked up by interned BSFixedString, not by raw text.
void *intern_string(const char *text);

static inline void *holder_of(void *refr)
{
    return (char *)refr + HOLDER_OFFSET;
}

bool read_graph_float(void *holder, void *interned_name, float *out);
bool read_graph_int(void *holder, void *interned_name, int32_t *out);
bool read_graph_bool(void *holder, void *interned_name, bool *out);

float get_angle_z(void *refr);
void  write_angle_z(void *refr, float radians);
void  get_planar_position(void *refr, float *x, float *y);

// True from the first frame of sitting down until the actor stands up again.
bool is_sitting(void *refr);

// True from the first frame of drawing until the weapon is fully put away.
bool is_weapon_drawn(void *refr);

// The raw pair behind is_sitting, for the one log line that records it.
void get_actor_state(void *refr, uint32_t *first, uint32_t *second);
