#ifndef ZCSR_SPATIAL_GRID_H
#define ZCSR_SPATIAL_GRID_H

/* zcsr_tools / common / spatial_grid
 * 通用均匀网格 broadphase：把 AABB 索引到网格，支持点/AABB 候选查询。
 * 只做空间候选，不判断碰撞、不选择目标、不计算伤害；所有存储由调用方提供。
 * 本模块不分配内存。 */

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define ZCSR_SPATIAL_GRID_INVALID UINT32_MAX

typedef void (*zcsr_spatial_grid_visit_fn)(uint32_t item, void* user_data);

typedef struct {
    uint32_t columns;
    uint32_t rows;
    float min_x;
    float min_y;
    float cell_width;
    float cell_height;

    uint32_t* cell_head;    /* columns * rows；每个格子的链表头 */
    uint32_t* link_item;    /* link_capacity；链表节点对应的 item */
    uint32_t* link_next;    /* link_capacity；下一个节点 */
    uint32_t link_capacity;
    uint32_t link_count;

    uint32_t* item_mark;    /* item_capacity；可选，用于跨格去重 */
    uint32_t item_capacity;
    uint32_t query_stamp;
} zcsr_spatial_grid;

/* 初始化网格。cell_head 长度必须为 columns * rows；init 会把它全部写成 INVALID。
 * item_mark 可以为 NULL；提供时长度必须为 item_capacity，用于查询去重。 */
bool zcsr_spatial_grid_init(
    zcsr_spatial_grid* grid,
    uint32_t columns,
    uint32_t rows,
    float min_x,
    float min_y,
    float cell_width,
    float cell_height,
    uint32_t* cell_head,
    uint32_t* link_item,
    uint32_t* link_next,
    uint32_t link_capacity,
    uint32_t* item_mark,
    uint32_t item_capacity);

/* 把一个 AABB 索引到所有覆盖的格子。item 是调用方的不透明索引。
 * 若 link 容量不足，返回 false 且不写入任何节点。 */
bool zcsr_spatial_grid_insert_aabb(
    zcsr_spatial_grid* grid,
    uint32_t item,
    float x,
    float y,
    float width,
    float height);

/* 查询与点或 AABB 覆盖格子相交的候选 item。
 * 返回访问的 item 数；同一 item 跨多个格子时，只有提供 item_mark 才会去重。
 * visit 可以为 NULL，此时只返回计数。 */
uint32_t zcsr_spatial_grid_query_point(
    zcsr_spatial_grid* grid,
    float x,
    float y,
    zcsr_spatial_grid_visit_fn visit,
    void* user_data);

uint32_t zcsr_spatial_grid_query_aabb(
    zcsr_spatial_grid* grid,
    float x,
    float y,
    float width,
    float height,
    zcsr_spatial_grid_visit_fn visit,
    void* user_data);

#ifdef __cplusplus
}
#endif

#endif /* ZCSR_SPATIAL_GRID_H */
