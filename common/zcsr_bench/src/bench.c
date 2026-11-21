#include "zcsr_bench.h"

#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#  define WIN32_LEAN_AND_MEAN
#  include <windows.h>
#else
#  include <time.h>
#endif

static uint64_t default_now(void* user) {
    (void)user;
#if defined(_WIN32)
    {
        LARGE_INTEGER frequency, counter;
        if (!QueryPerformanceFrequency(&frequency) || frequency.QuadPart == 0) return 0u;
        if (!QueryPerformanceCounter(&counter)) return 0u;
        return (uint64_t)((double)counter.QuadPart * 1000000000.0 / (double)frequency.QuadPart);
    }
#else
    {
        struct timespec ts;
        if (timespec_get(&ts, TIME_UTC) == 0) return 0u;
        return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
    }
#endif
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

static void bench_finalize(zcsr_bench_result* r) {
    if (!r || r->sample_count == 0u) return;
    sort_doubles(r->samples, r->sample_count);
    r->min_ms = r->samples[0];
    r->max_ms = r->samples[r->sample_count - 1u];
    r->avg_ms = r->total_ms / (double)r->sample_count;
    r->p50_ms = r->samples[percentile_index(r->sample_count, 0.50)];
    r->p95_ms = r->samples[percentile_index(r->sample_count, 0.95)];
    r->p99_ms = r->samples[percentile_index(r->sample_count, 0.99)];
}

bool zcsr_bench_run_series(const zcsr_bench_series_config* cfg,
                           zcsr_bench_frame_fn step, void* user,
                           zcsr_bench_result* frame_result,
                           zcsr_bench_result* series_results) {
    zcsr_bench_now_fn now;
    void* clock_user;
    uint32_t i, j;
    uint32_t series_samples[ZCSR_BENCH_MAX_SERIES] = { 0u, 0u, 0u, 0u };

    if (!cfg || !step) return false;
    if (!frame_result && !series_results) return false;
    if (cfg->measure_frames == 0u || cfg->measure_frames > ZCSR_BENCH_MAX_SAMPLES) return false;
    if (cfg->series_count > ZCSR_BENCH_MAX_SERIES) return false;
    if (cfg->series_count > 0u && !series_results) return false;

    if (frame_result) memset(frame_result, 0, sizeof *frame_result);
    for (j = 0; j < cfg->series_count; ++j) memset(&series_results[j], 0, sizeof series_results[j]);

    now = cfg->now ? cfg->now : default_now;
    clock_user = cfg->now ? cfg->clock_user : NULL;

    for (i = 0; i < cfg->warmup_frames; ++i) {
        zcsr_bench_frame frame;
        memset(&frame, 0, sizeof frame);
        frame.frame_index = i;
        frame.measure = false;
        step(user, &frame);
    }

    for (i = 0; i < cfg->measure_frames; ++i) {
        zcsr_bench_frame frame;
        uint64_t t0, t1;
        double frame_ms;

        memset(&frame, 0, sizeof frame);
        frame.frame_index = cfg->warmup_frames + i;
        frame.measure = true;

        t0 = now(clock_user);
        step(user, &frame);
        t1 = now(clock_user);
        frame_ms = (t1 >= t0) ? (double)(t1 - t0) / 1000000.0 : 0.0;

        if (frame_result) {
            frame_result->samples[i] = frame_ms;
            frame_result->total_ms += frame_ms;
        }
        for (j = 0; j < cfg->series_count; ++j) {
            double sample = frame.series_ms[j];
            uint32_t slot;
            if (!frame.series_valid[j]) continue;
            if (sample < 0.0) sample = 0.0;
            slot = series_samples[j];
            if (slot >= ZCSR_BENCH_MAX_SAMPLES) continue;
            series_results[j].samples[slot] = sample;
            series_results[j].total_ms += sample;
            series_samples[j] = slot + 1u;
        }
    }

    if (frame_result) {
        frame_result->sample_count = cfg->measure_frames;
        bench_finalize(frame_result);
    }
    for (j = 0; j < cfg->series_count; ++j) {
        series_results[j].sample_count = series_samples[j];
        bench_finalize(&series_results[j]);
    }
    return true;
}

typedef struct {
    zcsr_bench_step_fn step;
    void* user;
} zcsr_bench_single_adapter;

static void bench_single_step(void* user, zcsr_bench_frame* frame) {
    zcsr_bench_single_adapter* adapter = (zcsr_bench_single_adapter*)user;
    (void)frame;
    adapter->step(adapter->user);
}

bool zcsr_bench_run(const zcsr_bench_config* cfg, zcsr_bench_step_fn step, void* user,
                    zcsr_bench_result* out) {
    zcsr_bench_series_config series_cfg;
    zcsr_bench_single_adapter adapter;
    if (!cfg || !step || !out) return false;

    series_cfg.warmup_frames = cfg->warmup_frames;
    series_cfg.measure_frames = cfg->measure_frames;
    series_cfg.now = cfg->now;
    series_cfg.clock_user = cfg->clock_user;
    series_cfg.series_count = 0u;

    adapter.step = step;
    adapter.user = user;
    return zcsr_bench_run_series(&series_cfg, bench_single_step, &adapter, out, NULL);
}

bool zcsr_bench_write_csv(const zcsr_bench_result* r, const char* path) {
    FILE* fp;
    if (!r || !path) return false;
    fp = fopen(path, "wb");
    if (!fp) return false;
    fprintf(fp, "metric,value\n");
    fprintf(fp, "samples,%u\n", r->sample_count);
    fprintf(fp, "total_ms,%.6f\n", r->total_ms);
    fprintf(fp, "avg_ms,%.6f\n", r->avg_ms);
    fprintf(fp, "min_ms,%.6f\n", r->min_ms);
    fprintf(fp, "max_ms,%.6f\n", r->max_ms);
    fprintf(fp, "p50_ms,%.6f\n", r->p50_ms);
    fprintf(fp, "p95_ms,%.6f\n", r->p95_ms);
    fprintf(fp, "p99_ms,%.6f\n", r->p99_ms);
    fprintf(fp, "frame,ms\n");
    for (uint32_t i = 0; i < r->sample_count; ++i) {
        fprintf(fp, "%u,%.6f\n", i, r->samples[i]);
    }
    fclose(fp);
    return true;
}

bool zcsr_bench_write_markdown(const zcsr_bench_result* r, const char* path) {
    FILE* fp;
    if (!r || !path) return false;
    fp = fopen(path, "wb");
    if (!fp) return false;
    fprintf(fp, "| metric | value |\n");
    fprintf(fp, "|---|---:|\n");
    fprintf(fp, "| samples | %u |\n", r->sample_count);
    fprintf(fp, "| total_ms | %.6f |\n", r->total_ms);
    fprintf(fp, "| avg_ms | %.6f |\n", r->avg_ms);
    fprintf(fp, "| min_ms | %.6f |\n", r->min_ms);
    fprintf(fp, "| max_ms | %.6f |\n", r->max_ms);
    fprintf(fp, "| p50_ms | %.6f |\n", r->p50_ms);
    fprintf(fp, "| p95_ms | %.6f |\n", r->p95_ms);
    fprintf(fp, "| p99_ms | %.6f |\n", r->p99_ms);
    fclose(fp);
    return true;
}
