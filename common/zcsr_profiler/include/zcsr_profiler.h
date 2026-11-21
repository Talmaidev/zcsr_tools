#ifndef ZCSR_PROFILER_H
#define ZCSR_PROFILER_H
/* zcsr_tools / zcsr_profiler — game-agnostic frame/counter profiler.
 * Fixed storage only: caller owns the profiler and the text buffer. */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define ZCSR_PROFILER_FRAME_CAP 120u
#define ZCSR_PROFILER_COUNTER_CAP 32u
#define ZCSR_PROFILER_NAME_CAP 32u

typedef struct {
    double   frame_ms;
    double   cpu_ms;
    double   gpu_ms;
    uint32_t draw_calls;
    uint64_t memory_bytes;
    uint64_t entity_count;
    uint64_t projectile_count;
} zcsr_profile_frame;

typedef struct {
    uint32_t sample_count;
    double   avg_frame_ms;
    double   min_frame_ms;
    double   max_frame_ms;
    double   p95_frame_ms;
    double   avg_cpu_ms;
    double   avg_gpu_ms;
    uint32_t last_draw_calls;
    uint64_t last_memory_bytes;
    uint64_t last_entity_count;
    uint64_t last_projectile_count;
} zcsr_profile_stats;

typedef struct {
    char   name[ZCSR_PROFILER_NAME_CAP];
    double value;
} zcsr_profile_counter;

typedef struct {
    uint32_t             frame_count;
    uint32_t             frame_head;
    zcsr_profile_frame   frames[ZCSR_PROFILER_FRAME_CAP];
    uint32_t             counter_count;
    zcsr_profile_counter counters[ZCSR_PROFILER_COUNTER_CAP];
} zcsr_profiler;

void     zcsr_profiler_init(zcsr_profiler* p);
void     zcsr_profiler_set_frame(zcsr_profiler* p, const zcsr_profile_frame* frame);
void     zcsr_profiler_add_counter(zcsr_profiler* p, const char* name, double value);
void     zcsr_profiler_clear_counters(zcsr_profiler* p);
bool     zcsr_profiler_get_counter(const zcsr_profiler* p, const char* name, double* out);
bool     zcsr_profiler_stats(const zcsr_profiler* p, zcsr_profile_stats* out);
/* Writes a compact HUD block into caller memory. Returns chars written (NUL excluded),
 * clamped to cap - 1 when truncated. cap == 0 returns 0. */
uint32_t zcsr_profiler_format_hud(const zcsr_profiler* p, char* out, uint32_t cap);

#endif /* ZCSR_PROFILER_H */
