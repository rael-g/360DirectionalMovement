#include "diagnostics.h"
#include "angles.h"
#include "log.h"
#include "rotator.h"

#include <stddef.h>

#define MAX_SAMPLES   40
#define MAX_MOVEMENTS 8

static struct diagnostic_sample g_samples[MAX_SAMPLES];
static long   g_sample_calls[MAX_SAMPLES];
static float  g_sample_dt[MAX_SAMPLES];
static float  g_sample_driven[MAX_SAMPLES];
static size_t g_count = 0;
static size_t g_movement = 0;
static long   g_calls_at_last_sample = 0;

void diagnostics_record(const struct diagnostic_sample *sample)
{
    if (g_movement >= MAX_MOVEMENTS || g_count >= MAX_SAMPLES) return;

    const long calls = rotator_hook_calls();
    g_samples[g_count] = *sample;
    g_sample_calls[g_count] = calls - g_calls_at_last_sample;
    g_sample_dt[g_count] = rotator_last_dt();
    g_sample_driven[g_count] = rotator_driven_angle();
    g_calls_at_last_sample = calls;
    ++g_count;
}

void diagnostics_flush(void)
{
    if (!g_count || g_movement >= MAX_MOVEMENTS) {
        g_count = 0;
        return;
    }

    log_line("[movement %zu]", ++g_movement);
    for (size_t i = 0; i < g_count; ++i) {
        const struct diagnostic_sample *s = &g_samples[i];
        log_line("  %2zu code=%d angle=%7.1f rel=%7.1f heading=%7.1f "
                 "measured=%7.1f travel=%6.3f dir=%6.3f offset=%7.1f "
                 "speed=%6.2f calls=%ld dt=%5.1fms driven=%7.1f",
                 i, s->code,
                 s->angle * SF360_RAD_TO_DEG, s->relative * SF360_RAD_TO_DEG,
                 s->heading * SF360_RAD_TO_DEG, s->measured, s->travel,
                 s->direction, s->offset * SF360_RAD_TO_DEG, s->speed,
                 g_sample_calls[i], g_sample_dt[i] * 1000.0f,
                 g_sample_driven[i] * SF360_RAD_TO_DEG);
    }
    g_count = 0;
}

void diagnostics_reset(void) { g_count = 0; }
