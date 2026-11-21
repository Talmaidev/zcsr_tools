#include "zcsr_profiler.h"

#include <float.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static void copy_name(char* dst, size_t cap, const char* src) {
    size_t i = 0;
    if (!dst || cap == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    for (; i + 1u < cap && src[i] != '\0'; ++i) dst[i] = src[i];
    dst[i] = '\0';
}

static uint32_t percentile_index(uint32_t n, double p) {
    uint32_t i;
    if (n <= 1u) return 0u;
    i = (uint32_t)((double)(n - 1u) * p + 0.5);
    return i < n ? i : n - 1u;
}

static void sort_doubles(double* v, uint32_t n) {
    for (uint32_t i = 1u; i < n; ++i) {
        double x = v[i];
        uint32_t j = i;
        while (j > 0u && v[j - 1u] > x) {
            v[j] = v[j - 1u];
            --j;
        }
        v[j] = x;
    }
}

static void append_text(char* out, uint32_t cap, uint32_t* used, const char* fmt, ...) {
    va_list ap;
    int n;
    if (!out || !used || *used >= cap) return;
    va_start(ap, fmt);
    n = vsnprintf(out + *used, cap - *used, fmt, ap);
    va_end(ap);
    if (n < 0) {
        out[*used] = '\0';
        return;
    }
    if ((uint32_t)n >= cap - *used) {
        *used = cap - 1u;
        out[cap - 1u] = '\0';
    } else {
        *used += (uint32_t)n;
    }
}

void zcsr_profiler_init(zcsr_profiler* p) {
    if (!p) return;
    memset(p, 0, sizeof *p);
}

void zcsr_profiler_set_frame(zcsr_profiler* p, const zcsr_profile_frame* frame) {
    if (!p || !frame) return;
    p->frames[p->frame_head] = *frame;
    p->frame_head = (p->frame_head + 1u) % ZCSR_PROFILER_FRAME_CAP;
    if (p->frame_count < ZCSR_PROFILER_FRAME_CAP) ++p->frame_count;
}

void zcsr_profiler_add_counter(zcsr_profiler* p, const char* name, double value) {
    if (!p || !name) return;
    for (uint32_t i = 0; i < p->counter_count; ++i) {
        if (strcmp(p->counters[i].name, name) == 0) {
            p->counters[i].value = value;
            return;
        }
    }
    if (p->counter_count < ZCSR_PROFILER_COUNTER_CAP) {
        zcsr_profile_counter* c = &p->counters[p->counter_count++];
        copy_name(c->name, sizeof c->name, name);
        c->value = value;
    }
}

void zcsr_profiler_clear_counters(zcsr_profiler* p) {
    if (!p) return;
    p->counter_count = 0;
}

bool zcsr_profiler_get_counter(const zcsr_profiler* p, const char* name, double* out) {
    if (!p || !name || !out) return false;
    for (uint32_t i = 0; i < p->counter_count; ++i) {
        if (strcmp(p->counters[i].name, name) == 0) {
            *out = p->counters[i].value;
            return true;
        }
    }
    return false;
}

bool zcsr_profiler_stats(const zcsr_profiler* p, zcsr_profile_stats* out) {
    double values[ZCSR_PROFILER_FRAME_CAP];
    double sum_frame = 0.0, sum_cpu = 0.0, sum_gpu = 0.0;
    uint32_t n;
    if (!p || !out) return false;
    memset(out, 0, sizeof *out);
    n = p->frame_count;
    if (n > ZCSR_PROFILER_FRAME_CAP) n = ZCSR_PROFILER_FRAME_CAP;
    out->sample_count = n;
    if (n == 0u) return true;

    out->min_frame_ms = DBL_MAX;
    out->max_frame_ms = -DBL_MAX;
    for (uint32_t i = 0; i < n; ++i) {
        const zcsr_profile_frame* f = &p->frames[i];
        values[i] = f->frame_ms;
        sum_frame += f->frame_ms;
        sum_cpu += f->cpu_ms;
        sum_gpu += f->gpu_ms;
        if (f->frame_ms < out->min_frame_ms) out->min_frame_ms = f->frame_ms;
        if (f->frame_ms > out->max_frame_ms) out->max_frame_ms = f->frame_ms;
    }
    sort_doubles(values, n);
    out->avg_frame_ms = sum_frame / (double)n;
    out->avg_cpu_ms = sum_cpu / (double)n;
    out->avg_gpu_ms = sum_gpu / (double)n;
    out->p95_frame_ms = values[percentile_index(n, 0.95)];

    {
        uint32_t last = (p->frame_head + ZCSR_PROFILER_FRAME_CAP - 1u) % ZCSR_PROFILER_FRAME_CAP;
        const zcsr_profile_frame* f = &p->frames[last];
        out->last_draw_calls = f->draw_calls;
        out->last_memory_bytes = f->memory_bytes;
        out->last_entity_count = f->entity_count;
        out->last_projectile_count = f->projectile_count;
    }
    return true;
}

uint32_t zcsr_profiler_format_hud(const zcsr_profiler* p, char* out, uint32_t cap) {
    zcsr_profile_stats s;
    uint32_t used = 0u;
    double fps;
    if (!out || cap == 0u) return 0u;
    out[0] = '\0';
    if (!p) return 0u;
    if (!zcsr_profiler_stats(p, &s)) return 0u;

    fps = s.avg_frame_ms > 0.0 ? 1000.0 / s.avg_frame_ms : 0.0;
    append_text(out, cap, &used,
                "FPS %.1f | frame %.2f ms | CPU %.2f ms | GPU %.2f ms\n",
                fps, s.avg_frame_ms, s.avg_cpu_ms, s.avg_gpu_ms);
    append_text(out, cap, &used,
                "draw %u | mem %.1f MB | entities %llu | projectiles %llu\n",
                s.last_draw_calls,
                (double)s.last_memory_bytes / (1024.0 * 1024.0),
                (unsigned long long)s.last_entity_count,
                (unsigned long long)s.last_projectile_count);

    for (uint32_t i = 0; i < p->counter_count; ++i) {
        append_text(out, cap, &used, "%s=%.2f%s",
                    p->counters[i].name,
                    p->counters[i].value,
                    (i + 1u < p->counter_count) ? " | " : "\n");
    }
    if (used == 0u) out[0] = '\0';
    return used;
}
