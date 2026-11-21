#include "zcsr_tower_defense.h"

#include <stdio.h>

static int expect(int condition, const char* label) {
    if (!condition) printf("FAIL: %s\n", label);
    return condition ? 0 : 1;
}

int main(void) {
    zcsr_tower_defense_wave_schedule schedule;
    zcsr_tower_defense_spawn spawn;
    int fails = 0;

    fails += expect(zcsr_tower_defense_lane_clamp(5u, -1) == 0,
                    "lane clamp low");
    fails += expect(zcsr_tower_defense_lane_clamp(5u, 2) == 2,
                    "lane clamp inside");
    fails += expect(zcsr_tower_defense_lane_clamp(5u, 8) == 4,
                    "lane clamp high");
    fails += expect(zcsr_tower_defense_lane_clamp(0u, 8) == 0,
                    "lane clamp zero lanes");

    zcsr_tower_defense_wave_schedule_initialize(&schedule);
    fails += expect(zcsr_tower_defense_wave_schedule_is_complete(&schedule),
                    "empty schedule complete");
    fails += expect(zcsr_tower_defense_wave_schedule_add_wave(
                        &schedule, 0.5f, 1.0f, 2u, 1u),
                    "add wave one");
    fails += expect(zcsr_tower_defense_wave_schedule_add_wave(
                        &schedule, 2.0f, 0.5f, 1u, 2u),
                    "add wave two");
    fails += expect(!zcsr_tower_defense_wave_schedule_add_wave(
                        &schedule, 0.0f, 0.0f, 0u, 3u),
                    "zero count rejected");
    fails += expect(!zcsr_tower_defense_wave_schedule_add_wave(
                        &schedule, -1.0f, 0.0f, 1u, 3u),
                    "negative delay rejected");

    zcsr_tower_defense_wave_schedule_reset(&schedule);
    fails += expect(!zcsr_tower_defense_wave_schedule_is_complete(&schedule),
                    "reset not complete");
    fails += expect(zcsr_tower_defense_wave_schedule_step_seconds(
                        &schedule, 0.5f, &spawn),
                    "first spawn at delay boundary");
    fails += expect(spawn.wave_index == 0u && spawn.unit_kind == 1u &&
                        spawn.spawned_index == 0u,
                    "first spawn payload");
    fails += expect(zcsr_tower_defense_wave_schedule_step_seconds(
                        &schedule, 1.0f, &spawn),
                    "second spawn");
    fails += expect(spawn.spawned_index == 1u, "second spawn index");
    fails += expect(!zcsr_tower_defense_wave_schedule_is_complete(&schedule),
                    "second spawn not complete");
    fails += expect(zcsr_tower_defense_wave_schedule_step_seconds(
                        &schedule, 3.0f, &spawn),
                    "second wave first spawn");
    fails += expect(spawn.wave_index == 1u && spawn.unit_kind == 2u &&
                        spawn.spawned_index == 0u,
                    "second wave payload");
    fails += expect(zcsr_tower_defense_wave_schedule_is_complete(&schedule),
                    "schedule complete after last spawn");
    fails += expect(!zcsr_tower_defense_wave_schedule_step_seconds(
                        &schedule, 0.0f, &spawn),
                    "no spawn after complete");

    if (fails == 0) printf("test_tower_defense: PASS\n");
    return fails ? 1 : 0;
}
