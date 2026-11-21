#include "zcsr_bench.h"

#include <stdio.h>
#include <string.h>

static int near_enough(double a, double b) {
    double d = a - b;
    if (d < 0.0) d = -d;
    return d <= 1e-9;
}

static int expect(int cond, const char* label) {
    if (!cond) printf("FAIL: %s\n", label);
    return cond ? 0 : 1;
}

typedef struct {
    uint64_t now_ns;
    uint32_t frame;
    uint64_t durations_ms[8];
} fake_clock;

static uint64_t fake_now(void* user) {
    return ((fake_clock*)user)->now_ns;
}

static void fake_step(void* user) {
    fake_clock* f = (fake_clock*)user;
    uint32_t i = f->frame < 8 ? f->frame : 7;
    f->now_ns += f->durations_ms[i] * 1000000ull;
    ++f->frame;
}

static void fake_frame_step(void* user, zcsr_bench_frame* frame) {
    fake_clock* f = (fake_clock*)user;
    uint32_t i = f->frame < 8 ? f->frame : 7;
    double duration = (double)f->durations_ms[i];
    f->now_ns += f->durations_ms[i] * 1000000ull;
    frame->series_valid[0] = true;
    frame->series_valid[1] = true;
    frame->series_ms[0] = duration;
    frame->series_ms[1] = duration * 0.5;
    ++f->frame;
}

int main(void) {
    fake_clock clock = { 0, 0, { 1, 1, 10, 20, 30, 40, 0, 0 } };
    zcsr_bench_config cfg;
    zcsr_bench_result r;
    zcsr_bench_series_config scfg;
    zcsr_bench_result frame_r;
    zcsr_bench_result series_r[2];
    FILE* fp;
    char buf[4096];
    int fails = 0;

    memset(&cfg, 0, sizeof cfg);
    cfg.warmup_frames = 2;
    cfg.measure_frames = 4;
    cfg.now = fake_now;
    cfg.clock_user = &clock;

    fails += expect(zcsr_bench_run(&cfg, fake_step, &clock, &r), "bench run");
    fails += expect(r.sample_count == 4, "sample count 4");
    fails += expect(near_enough(r.avg_ms, 25.0), "avg 25");
    fails += expect(near_enough(r.min_ms, 10.0), "min 10");
    fails += expect(near_enough(r.max_ms, 40.0), "max 40");
    fails += expect(near_enough(r.p50_ms, 30.0), "p50 30");
    fails += expect(near_enough(r.p95_ms, 40.0), "p95 40");
    fails += expect(near_enough(r.p99_ms, 40.0), "p99 40");
    fails += expect(near_enough(r.total_ms, 100.0), "total 100");

    clock.frame = 0;
    clock.now_ns = 0;
    memset(&scfg, 0, sizeof scfg);
    scfg.warmup_frames = 2;
    scfg.measure_frames = 4;
    scfg.now = fake_now;
    scfg.clock_user = &clock;
    scfg.series_count = 2;
    fails += expect(zcsr_bench_run_series(&scfg, fake_frame_step, &clock,
                                          &frame_r, series_r), "series run");
    fails += expect(frame_r.sample_count == 4, "series frame count");
    fails += expect(near_enough(frame_r.avg_ms, 25.0), "series frame avg");
    fails += expect(near_enough(series_r[0].avg_ms, 25.0), "series 0 avg");
    fails += expect(near_enough(series_r[1].avg_ms, 12.5), "series 1 avg");
    fails += expect(near_enough(series_r[0].p50_ms, 30.0), "series 0 p50");
    fails += expect(near_enough(series_r[1].p50_ms, 15.0), "series 1 p50");
    scfg.series_count = ZCSR_BENCH_MAX_SERIES + 1u;
    fails += expect(!zcsr_bench_run_series(&scfg, fake_frame_step, &clock,
                                           &frame_r, series_r), "series count reject");

    fails += expect(zcsr_bench_write_csv(&r, "bench_test.csv"), "csv write");
    fp = fopen("bench_test.csv", "rb");
    fails += expect(fp != NULL, "csv open");
    if (fp) {
        size_t n = fread(buf, 1, sizeof buf - 1, fp);
        buf[n] = '\0';
        fclose(fp);
        fails += expect(strstr(buf, "samples,4") != NULL, "csv samples row");
        fails += expect(strstr(buf, "p95_ms,40.000000") != NULL, "csv p95 row");
    }
    remove("bench_test.csv");

    fails += expect(zcsr_bench_write_markdown(&r, "bench_test.md"), "md write");
    fp = fopen("bench_test.md", "rb");
    fails += expect(fp != NULL, "md open");
    if (fp) {
        size_t n = fread(buf, 1, sizeof buf - 1, fp);
        buf[n] = '\0';
        fclose(fp);
        fails += expect(strstr(buf, "p95_ms") != NULL, "md p95 label");
        fails += expect(strstr(buf, "| 40.000000 |") != NULL, "md p95 value");
    }
    remove("bench_test.md");

    if (!fails) printf("test_bench: PASS\n");
    return fails ? 1 : 0;
}
