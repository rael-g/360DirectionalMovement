#pragma once

#include <math.h>

#define SF360_TWO_PI 6.2831853f
#define SF360_PI     3.14159265f
#define SF360_RAD_TO_DEG 57.2958f

// Movement input is quantised to eighths of a turn.
#define SF360_OCTANTS 8

// maps an angle onto (-pi, pi]
static inline float wrap_signed(float a)
{
    while (a > SF360_PI) a -= SF360_TWO_PI;
    while (a < -SF360_PI) a += SF360_TWO_PI;
    return a;
}

// maps an angle onto [0, 2pi), the range the game stores
static inline float wrap_unsigned(float a)
{
    while (a < 0.0f) a += SF360_TWO_PI;
    while (a >= SF360_TWO_PI) a -= SF360_TWO_PI;
    return a;
}

// Reducing a heading to its octant separates a real direction change from the
// small drift our own rotation introduces.
static inline int octant_of(float turns)
{
    int o = (int)lroundf(turns * SF360_OCTANTS) % SF360_OCTANTS;
    return o < 0 ? o + SF360_OCTANTS : o;
}
