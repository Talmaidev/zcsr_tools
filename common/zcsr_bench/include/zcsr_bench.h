#ifndef ZCSR_BENCH_H
#define ZCSR_BENCH_H
/* zcsr_tools / zcsr_bench — generic benchmark runner.
 * The caller supplies the scenario step and clock; this module only measures and reports. */
#include <stdbool.h>
#include <stdint.h>

#define ZCSR_BENCH_MAX_SAMPLES 4096u
#define ZCSR_BENCH_MAX_SERIES 4u

typedef uint64_t (*zcsr_bench_now_fn)(void* user);
typedef void     (*zcsr_bench_step_fn)(void* user);

typedef struct {
    uint32_t          warmup_frames;
    uint32_t          measure_frames;
    zcsr_bench_now_fn now;        /* NULL = portable default clock */
    void*             clock_user;
} zcsr_bench_config;

typedef struct {
    uint32_t sample_count;
    double   samples[ZCSR_BENCH_MAX_SAMPLES];
    double   total_ms;
    double   avg_ms;
    double   min_ms;
    double   max_ms;
    double   p50_ms;
    double   p95_ms;
    double   p99_ms;
} zcsr_bench_result;

bool zcsr_bench_run(const zcsr_bench_config* cfg, zcsr_bench_step_fn step, void* user,
                    zcsr_bench_result* out);

/* Multi-series frame benchmark.
 * The runner measures one wall-clock frame per callback; the callback may fill
 * frame->series_ms[i] with its own per-frame values (CPU submit, GPU timer, ...)
 * and mark frame->series_valid[i] = true. Invalid series samples are skipped,
 * so a series result may have fewer samples than measure_frames.
 * Warmup frames are executed with frame->measure = false.
 * frame_result receives the wall-clock series; series_results receives exactly
 * cfg->series_count series (caller-owned array). Either output may be NULL, but
 * at least one must be non-NULL. */
typedef struct {
    uint32_t frame_index;
    bool     measure;
    bool     series_valid[ZCSR_BENCH_MAX_SERIES];
    double   series_ms[ZCSR_BENCH_MAX_SERIES];
} zcsr_bench_frame;

typedef void (*zcsr_bench_frame_fn)(void* user, zcsr_bench_frame* frame);

typedef struct {
    uint32_t          warmup_frames;
    uint32_t          measure_frames;
    zcsr_bench_now_fn now;        /* NULL = portable default clock */
    void*             clock_user;
    uint32_t          series_count; /* 0..ZCSR_BENCH_MAX_SERIES */
} zcsr_bench_series_config;

bool zcsr_bench_run_series(const zcsr_bench_series_config* cfg,
                           zcsr_bench_frame_fn step, void* user,
                           zcsr_bench_result* frame_result,
                           zcsr_bench_result* series_results);

bool zcsr_bench_write_csv(const zcsr_bench_result* r, const char* path);
bool zcsr_bench_write_markdown(const zcsr_bench_result* r, const char* path);

#endif /* ZCSR_BENCH_H */
