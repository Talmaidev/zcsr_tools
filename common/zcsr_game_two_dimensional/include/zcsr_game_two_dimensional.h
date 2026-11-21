#ifndef ZCSR_GAME_TWO_DIMENSIONAL_H
#define ZCSR_GAME_TWO_DIMENSIONAL_H
/* zcsr_tools / common / game two dimensional
 * 通用 2D 几何：点、格子、固定容量棋盘坐标与调用方存储的占用视图。
 * 这里不包含塔防、植物、僵尸、阳光或任何具体游戏语义。 */
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZCSR_GAME_TWO_DIMENSIONAL_MAXIMUM_ROWS 64u
#define ZCSR_GAME_TWO_DIMENSIONAL_MAXIMUM_COLUMNS 64u
#define ZCSR_GAME_TWO_DIMENSIONAL_EMPTY_KIND 0u

typedef struct {
    float x;
    float y;
} zcsr_game_two_dimensional_point;

typedef struct {
    uint32_t row;
    uint32_t column;
} zcsr_game_two_dimensional_cell;

/* 棋盘：rows/columns 是格子数量，origin 是左上角世界坐标，cell 是单格尺寸。
 * occupied_kind 由调用方提供，工具层不拥有内存；empty_kind 由调用方定义。
 * 这样 9×9 关卡只申请 81 个格子，不需要为每次输入换算背负 64×64 的固定占用表。 */
typedef struct {
    uint32_t rows;
    uint32_t columns;
    float origin_x;
    float origin_y;
    float cell_width;
    float cell_height;
    uint32_t* occupied_kind;
    uint32_t occupied_stride;
    uint32_t empty_kind;
} zcsr_game_two_dimensional_grid;

/* 初始化棋盘并设置几何；不分配占用内存。尺寸超过固定容量时按容量截断。 */
void zcsr_game_two_dimensional_grid_initialize(
    zcsr_game_two_dimensional_grid* grid,
    uint32_t rows,
    uint32_t columns,
    float origin_x,
    float origin_y,
    float cell_width,
    float cell_height);

/* 绑定调用方提供的占用数组，并按 empty_kind 清空 rows × stride。
 * stride 必须大于等于 columns；绑定失败时棋盘仍可用于纯坐标换算。 */
bool zcsr_game_two_dimensional_grid_bind_occupancy(
    zcsr_game_two_dimensional_grid* grid,
    uint32_t* occupied_kind,
    uint32_t occupied_stride,
    uint32_t empty_kind);

/* 格子是否在棋盘内。 */
bool zcsr_game_two_dimensional_grid_is_inside(
    const zcsr_game_two_dimensional_grid* grid,
    uint32_t row,
    uint32_t column);

/* 世界坐标落在哪一格；落在 origin 左侧/上方或棋盘外返回 false。 */
bool zcsr_game_two_dimensional_grid_world_to_cell(
    const zcsr_game_two_dimensional_grid* grid,
    float x,
    float y,
    zcsr_game_two_dimensional_cell* out_cell);

/* 取格子中心世界坐标。 */
bool zcsr_game_two_dimensional_grid_cell_to_world_center(
    const zcsr_game_two_dimensional_grid* grid,
    uint32_t row,
    uint32_t column,
    zcsr_game_two_dimensional_point* out_point);

/* 读取占用类型；未绑定存储或越界返回 empty_kind。 */
uint32_t zcsr_game_two_dimensional_grid_occupied_kind_at(
    const zcsr_game_two_dimensional_grid* grid,
    uint32_t row,
    uint32_t column);

/* 空格写入占用类型；未绑定、越界、类型等于 empty_kind、已被占用返回 false。 */
bool zcsr_game_two_dimensional_grid_try_occupy(
    zcsr_game_two_dimensional_grid* grid,
    uint32_t row,
    uint32_t column,
    uint32_t occupied_kind);

/* 清空占用；未绑定、越界、原本为空返回 false。 */
bool zcsr_game_two_dimensional_grid_release(
    zcsr_game_two_dimensional_grid* grid,
    uint32_t row,
    uint32_t column);

#ifdef __cplusplus
}
#endif

#endif /* ZCSR_GAME_TWO_DIMENSIONAL_H */
