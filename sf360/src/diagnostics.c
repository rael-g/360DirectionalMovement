#include "diagnostics.h"
#include "angles.h"
#include "log.h"
#include "rotator.h"

#include <stddef.h>

// The transition at the head of a movement and the long stretch that follows it
// answer different questions, so they are buffered differently. Every sample of
// the head shows how a direction change resolves. The tail is decimated and
// keeps only the most recent, because a movement that goes wrong and stays
// wrong is described by where it ended up, not by when it started.
#define HEAD_SAMPLES  24
#define TAIL_SAMPLES  48
#define SAMPLE_STRIDE 16

#define MAX_MOVEMENTS 24

struct record
{
    struct diagnostic_sample sample;
    long  calls;
    float dt;
    float driven;
};

static struct record g_head[HEAD_SAMPLES];
static struct record g_tail[TAIL_SAMPLES];

static size_t g_head_count = 0;
static size_t g_tail_count = 0;
static size_t g_tail_next = 0;
static size_t g_skipped = 0;

static size_t g_movement = 0;
static long   g_calls_at_last_sample = 0;

static struct record capture(const struct diagnostic_sample *sample)
{
    const long calls = rotator_hook_calls();
    const struct record r = {
        .sample = *sample,
        .calls = calls - g_calls_at_last_sample,
        .dt = rotator_last_dt(),
        .driven = rotator_driven_angle(),
    };
    g_calls_at_last_sample = calls;
    return r;
}

void diagnostics_record(const struct diagnostic_sample *sample)
{
    if (g_movement >= MAX_MOVEMENTS) return;

    if (g_head_count < HEAD_SAMPLES) {
        g_head[g_head_count++] = capture(sample);
        return;
    }

    if (++g_skipped < SAMPLE_STRIDE) return;
    g_skipped = 0;

    g_tail[g_tail_next] = capture(sample);
    g_tail_next = (g_tail_next + 1) % TAIL_SAMPLES;
    if (g_tail_count < TAIL_SAMPLES) ++g_tail_count;
}

static void write_record(const char *mark, size_t index, const struct record *r)
{
    const struct diagnostic_sample *s = &r->sample;
    log_line("  %s%2zu code=%d angle=%7.1f rel=%7.1f heading=%7.1f "
             "measured=%7.1f travel=%6.3f dir=%6.3f offset=%7.1f "
             "speed=%6.2f calls=%ld dt=%5.1fms driven=%7.1f jump=%d "
             "st1=%08X st2=%08X",
             mark, index, s->code,
             s->angle * SF360_RAD_TO_DEG, s->relative * SF360_RAD_TO_DEG,
             s->heading * SF360_RAD_TO_DEG, s->measured, s->travel,
             s->direction, s->offset * SF360_RAD_TO_DEG, s->speed,
             r->calls, r->dt * 1000.0f, r->driven * SF360_RAD_TO_DEG,
             s->jump, s->state1, s->state2);
}

void diagnostics_flush(void)
{
    if (!g_head_count || g_movement >= MAX_MOVEMENTS) {
        diagnostics_reset();
        return;
    }

    log_line("[movement %zu]", ++g_movement);
    for (size_t i = 0; i < g_head_count; ++i)
        write_record("", i, &g_head[i]);

    // Oldest first, which for a full ring starts at the next slot to be written.
    const size_t first = (g_tail_count < TAIL_SAMPLES) ? 0 : g_tail_next;
    for (size_t i = 0; i < g_tail_count; ++i)
        write_record("+", i, &g_tail[(first + i) % TAIL_SAMPLES]);

    diagnostics_reset();
}

void diagnostics_reset(void)
{
    g_head_count = 0;
    g_tail_count = 0;
    g_tail_next = 0;
    g_skipped = 0;
}
