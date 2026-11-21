#include "zcsr_profiler.h"

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

int main(void) {
    zcsr_profiler p;
    zcsr_profile_frame f;
    zcsr_profile_stats s;
    char hud[512];
    uint32_t n;
    int fails = 0;

    zcsr_profiler_init(&p);
    memset(&f, 0, sizeof f);
    f.frame_ms = 10.0; f.cpu_ms = 4.0; f.gpu_ms = 2.0;
    f.draw_calls = 7; f.memory_bytes = 1024; f.entity_count = 100; f.projectile_count = 20;
    zcsr_profiler_set_frame(&p, &f);
    f.frame_ms = 20.0; f.cpu_ms = 5.0; f.gpu_ms = 3.0;
    zcsr_profiler_set_frame(&p, &f);
    f.frame_ms = 30.0; f.cpu_ms = 6.0; f.gpu_ms = 4.0;
    zcsr_profiler_set_frame(&p, &f);

    fails += expect(zcsr_profiler_stats(&p, &s), "stats available");
    fails += expect(s.sample_count == 3, "sample count");
    fails += expect(near_enough(s.avg_frame_ms, 20.0), "avg frame");
    fails += expect(near_enough(s.min_frame_ms, 10.0), "min frame");
    fails += expect(near_enough(s.max_frame_ms, 30.0), "max frame");
    fails += expect(near_enough(s.p95_frame_ms, 30.0), "p95 frame");
    fails += expect(s.last_draw_calls == 7, "last draw calls");
    fails += expect(s.last_entity_count == 100, "last entity count");

    zcsr_profiler_add_counter(&p, "entities", 100.0);
    zcsr_profiler_add_counter(&p, "draw", 7.0);
    {
        double v = 0.0;
        fails += expect(zcsr_profiler_get_counter(&p, "entities", &v) && near_enough(v, 100.0),
                        "counter lookup");
    }

    n = zcsr_profiler_format_hud(&p, hud, (uint32_t)sizeof hud);
    fails += expect(n > 0 && strlen(hud) == n, "hud length");
    fails += expect(strstr(hud, "FPS") != NULL, "hud FPS");
    fails += expect(strstr(hud, "frame") != NULL, "hud frame");
    fails += expect(strstr(hud, "entities") != NULL, "hud counter");

    {
        char tiny[8];
        n = zcsr_profiler_format_hud(&p, tiny, (uint32_t)sizeof tiny);
        fails += expect(n > 0 && strlen(tiny) < sizeof tiny, "hud truncation safe");
    }

    for (int i = 0; i < 130; ++i) {
        memset(&f, 0, sizeof f);
        f.frame_ms = (double)i;
        zcsr_profiler_set_frame(&p, &f);
    }
    fails += expect(zcsr_profiler_stats(&p, &s), "stats after wrap");
    fails += expect(s.sample_count == ZCSR_PROFILER_FRAME_CAP, "ring capped at 120");

    if (!fails) printf("test_profiler: PASS\n");
    return fails ? 1 : 0;
}
