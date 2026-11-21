#include "zcsr_tower_defense.h"

#include <string.h>

int32_t zcsr_tower_defense_lane_clamp(uint32_t lane_count, int32_t lane) {
    if (lane_count == 0u) return 0;
    if (lane < 0) return 0;
    if ((uint32_t)lane >= lane_count) return (int32_t)(lane_count - 1u);
    return lane;
}

void zcsr_tower_defense_wave_schedule_initialize(
    zcsr_tower_defense_wave_schedule* schedule) {
    if (!schedule) return;
    memset(schedule, 0, sizeof *schedule);
    schedule->complete = true;
}

bool zcsr_tower_defense_wave_schedule_add_wave(
    zcsr_tower_defense_wave_schedule* schedule,
    float delay_seconds,
    float interval_seconds,
    uint32_t unit_count,
    uint32_t unit_kind) {
    zcsr_tower_defense_wave* wave;

    if (!schedule) return false;
    if (schedule->wave_count >= ZCSR_TOWER_DEFENSE_MAXIMUM_WAVES) {
        return false;
    }
    if (delay_seconds < 0.0f || interval_seconds < 0.0f) return false;
    if (unit_count == 0u || unit_kind == 0u) {
        return false;
    }

    wave = &schedule->waves[schedule->wave_count];
    wave->delay_seconds = delay_seconds;
    wave->interval_seconds = interval_seconds;
    wave->unit_count = unit_count;
    wave->unit_kind = unit_kind;
    ++schedule->wave_count;
    schedule->complete = false;
    return true;
}

void zcsr_tower_defense_wave_schedule_reset(
    zcsr_tower_defense_wave_schedule* schedule) {
    if (!schedule) return;
    schedule->current_wave_index = 0u;
    schedule->spawned_in_current_wave = 0u;
    schedule->complete = schedule->wave_count == 0u;
    schedule->running = schedule->wave_count > 0u;
    schedule->time_until_next_spawn_seconds =
        schedule->running ? schedule->waves[0].delay_seconds : 0.0f;
}

bool zcsr_tower_defense_wave_schedule_step_seconds(
    zcsr_tower_defense_wave_schedule* schedule,
    float delta_seconds,
    zcsr_tower_defense_spawn* out_spawn) {
    if (!schedule || !out_spawn) return false;
    if (!schedule->running || schedule->complete) return false;
    if (delta_seconds < 0.0f) return false;

    schedule->time_until_next_spawn_seconds -= delta_seconds;

    while (schedule->time_until_next_spawn_seconds <= 0.0f) {
        zcsr_tower_defense_wave* wave =
            &schedule->waves[schedule->current_wave_index];

        if (schedule->spawned_in_current_wave < wave->unit_count) {
            out_spawn->wave_index = schedule->current_wave_index;
            out_spawn->unit_kind = wave->unit_kind;
            out_spawn->spawned_index = schedule->spawned_in_current_wave;
            ++schedule->spawned_in_current_wave;
            schedule->time_until_next_spawn_seconds += wave->interval_seconds;

            if (schedule->current_wave_index + 1u >= schedule->wave_count &&
                schedule->spawned_in_current_wave >= wave->unit_count) {
                schedule->complete = true;
                schedule->running = false;
            }
            return true;
        }

        if (schedule->current_wave_index + 1u >= schedule->wave_count) {
            schedule->complete = true;
            schedule->running = false;
            return false;
        }

        ++schedule->current_wave_index;
        schedule->spawned_in_current_wave = 0u;
        schedule->time_until_next_spawn_seconds +=
            schedule->waves[schedule->current_wave_index].delay_seconds;
    }

    return false;
}

bool zcsr_tower_defense_wave_schedule_is_complete(
    const zcsr_tower_defense_wave_schedule* schedule) {
    return schedule ? schedule->complete : true;
}
