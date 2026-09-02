#pragma once

// Buffers the first updates of each movement and writes them out when the
// movement ends, so file access never happens inside the control path.

enum diagnostic_code
{
    DIAGNOSTIC_START = 1,     // first update of a movement
    DIAGNOSTIC_SETTLED = 3,   // offset already captured
    DIAGNOSTIC_SETTLING = 4,  // heading still stabilising
};

struct diagnostic_sample
{
    int   code;
    float angle;      // body heading, radians
    float relative;   // movement direction relative to the body, radians
    float heading;    // where the actor is actually going, radians
    float measured;   // heading derived from displacement, degrees
    float travel;     // displacement magnitude
    int   sneak;      // raw iIsInSneak, -1000 when unreadable
    int   jump;       // raw iSyncJumpState, -1000 when unreadable
    float direction;  // raw Direction graph variable
    float camera_yaw; // raw fCameraYaw graph variable
    float offset;     // captured body to camera offset, radians
    float speed;      // raw Speed graph variable
};

void diagnostics_record(const struct diagnostic_sample *sample);
void diagnostics_flush(void);
void diagnostics_reset(void);
