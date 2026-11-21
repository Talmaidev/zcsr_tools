#ifndef ZCSR_TOWER_DEFENSE_H
#define ZCSR_TOWER_DEFENSE_H
/* zcsr_tools / tower_defense：塔防大类独立模块。
 * 只保留塔防重复实现的波次调度与 lane 限制；棋盘几何和占用由
 * common/zcsr_game_two_dimensional 独立提供，调用方按需直接使用，
 * 本层不做二次包装，也不复制公共 2D 实现。 */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZCSR_TOWER_DEFENSE_MAXIMUM_WAVES 128u

/* 把调用方传入的 lane 限制在 [0, lane_count - 1]；lane_count 为 0 时返回 0。 */
int32_t zcsr_tower_defense_lane_clamp(uint32_t lane_count, int32_t lane);

typedef struct {
    float delay_seconds;    /* 进入本波后到第一只单位的等待 */
    float interval_seconds; /* 同波单位生成间隔 */
    uint32_t unit_count;    /* 本波单位总数 */
    uint32_t unit_kind;     /* 调用方定义的生成类型，0 表示无效 */
} zcsr_tower_defense_wave;

typedef struct {
    zcsr_tower_defense_wave waves[ZCSR_TOWER_DEFENSE_MAXIMUM_WAVES];
    uint32_t wave_count;
    uint32_t current_wave_index;
    uint32_t spawned_in_current_wave;
    float time_until_next_spawn_seconds;
    bool running;
    bool complete;
} zcsr_tower_defense_wave_schedule;

typedef struct {
    uint32_t wave_index;
    uint32_t unit_kind;
    uint32_t spawned_index;
} zcsr_tower_defense_spawn;

/* 初始化空波次表。 */
void zcsr_tower_defense_wave_schedule_initialize(
    zcsr_tower_defense_wave_schedule* schedule);

/* 追加一波；顺序固定，不能插队。参数非法或容量满返回 false。 */
bool zcsr_tower_defense_wave_schedule_add_wave(
    zcsr_tower_defense_wave_schedule* schedule,
    float delay_seconds,
    float interval_seconds,
    uint32_t unit_count,
    uint32_t unit_kind);

/* 从第一波重新开始；第一波 delay 作为起始等待。 */
void zcsr_tower_defense_wave_schedule_reset(
    zcsr_tower_defense_wave_schedule* schedule);

/* 推进时间，最多产生一个生成事件；返回 true 表示 out_spawn 有效。
 * 调用方每帧可循环调用直到返回 false，以处理大 dt 或 0 间隔。 */
bool zcsr_tower_defense_wave_schedule_step_seconds(
    zcsr_tower_defense_wave_schedule* schedule,
    float delta_seconds,
    zcsr_tower_defense_spawn* out_spawn);

/* 所有波次完成且最后一只已生成。 */
bool zcsr_tower_defense_wave_schedule_is_complete(
    const zcsr_tower_defense_wave_schedule* schedule);

#ifdef __cplusplus
}
#endif

#endif /* ZCSR_TOWER_DEFENSE_H */
