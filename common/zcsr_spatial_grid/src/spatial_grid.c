#include "zcsr_spatial_grid.h"

#include <stddef.h>
#include <string.h>

static uint32_t clamp_cell(float value, float origin, float cell_size, uint32_t count)
{
    float relative;

    if (!(value > origin)) {
        return 0u;
    }
    relative = (value - origin) / cell_size;
    if (relative >= (float)count) {
        return count - 1u;
    }
    return (uint32_t)relative;
}

static void begin_query(zcsr_spatial_grid* grid)
{
    if (grid->item_mark == NULL || grid->item_capacity == 0u) {
        return;
    }
    if (grid->query_stamp == 0u || grid->query_stamp == UINT32_MAX) {
        memset(grid->item_mark, 0, (size_t)grid->item_capacity * sizeof(uint32_t));
        grid->query_stamp = 1u;
    } else {
        grid->query_stamp += 1u;
    }
}

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
    uint32_t item_capacity)
{
    size_t cell_count;

    if (grid == NULL || cell_head == NULL) {
        return false;
    }
    if (columns == 0u || rows == 0u || cell_width <= 0.0f || cell_height <= 0.0f) {
        return false;
    }
    if (link_capacity > 0u && (link_item == NULL || link_next == NULL)) {
        return false;
    }

    cell_count = (size_t)columns * (size_t)rows;
    if (cell_count > (size_t)UINT32_MAX) {
        return false;
    }

    grid->columns = columns;
    grid->rows = rows;
    grid->min_x = min_x;
    grid->min_y = min_y;
    grid->cell_width = cell_width;
    grid->cell_height = cell_height;
    grid->cell_head = cell_head;
    grid->link_item = link_item;
    grid->link_next = link_next;
    grid->link_capacity = link_capacity;
    grid->link_count = 0u;
    grid->item_mark = item_mark;
    grid->item_capacity = item_capacity;
    grid->query_stamp = 0u;

    memset(cell_head, 0xFF, cell_count * sizeof(uint32_t));
    if (item_mark != NULL && item_capacity > 0u) {
        memset(item_mark, 0, (size_t)item_capacity * sizeof(uint32_t));
    }
    return true;
}

bool zcsr_spatial_grid_insert_aabb(
    zcsr_spatial_grid* grid,
    uint32_t item,
    float x,
    float y,
    float width,
    float height)
{
    uint32_t cx0;
    uint32_t cx1;
    uint32_t cy0;
    uint32_t cy1;
    uint64_t needed;
    uint32_t cx;
    uint32_t cy;

    if (grid == NULL || grid->cell_head == NULL) {
        return false;
    }
    if (grid->cell_width <= 0.0f || grid->cell_height <= 0.0f) {
        return false;
    }
    if (grid->item_capacity > 0u && item >= grid->item_capacity) {
        return false;
    }

    if (width < 0.0f) {
        width = 0.0f;
    }
    if (height < 0.0f) {
        height = 0.0f;
    }

    cx0 = clamp_cell(x, grid->min_x, grid->cell_width, grid->columns);
    cy0 = clamp_cell(y, grid->min_y, grid->cell_height, grid->rows);
    cx1 = clamp_cell(x + width, grid->min_x, grid->cell_width, grid->columns);
    cy1 = clamp_cell(y + height, grid->min_y, grid->cell_height, grid->rows);

    needed = ((uint64_t)cx1 - cx0 + 1u) * ((uint64_t)cy1 - cy0 + 1u);
    if (needed > (uint64_t)(grid->link_capacity - grid->link_count)) {
        return false;
    }

    for (cy = cy0; cy <= cy1; ++cy) {
        for (cx = cx0; cx <= cx1; ++cx) {
            uint32_t cell = cy * grid->columns + cx;
            uint32_t link = grid->link_count++;

            grid->link_item[link] = item;
            grid->link_next[link] = grid->cell_head[cell];
            grid->cell_head[cell] = link;
        }
    }
    return true;
}

static uint32_t query_cell_range(
    zcsr_spatial_grid* grid,
    uint32_t cx0,
    uint32_t cy0,
    uint32_t cx1,
    uint32_t cy1,
    zcsr_spatial_grid_visit_fn visit,
    void* user_data)
{
    uint32_t cx;
    uint32_t cy;
    uint32_t visited = 0u;

    for (cy = cy0; cy <= cy1; ++cy) {
        for (cx = cx0; cx <= cx1; ++cx) {
            uint32_t cell = cy * grid->columns + cx;
            uint32_t link = grid->cell_head[cell];
            while (link != ZCSR_SPATIAL_GRID_INVALID) {
                uint32_t item = grid->link_item[link];
                bool duplicate = false;

                if (grid->item_mark != NULL && item < grid->item_capacity) {
                    if (grid->item_mark[item] == grid->query_stamp) {
                        duplicate = true;
                    } else {
                        grid->item_mark[item] = grid->query_stamp;
                    }
                }
                if (!duplicate) {
                    ++visited;
                    if (visit != NULL) {
                        visit(item, user_data);
                    }
                }
                link = grid->link_next[link];
            }
        }
    }
    return visited;
}

uint32_t zcsr_spatial_grid_query_point(
    zcsr_spatial_grid* grid,
    float x,
    float y,
    zcsr_spatial_grid_visit_fn visit,
    void* user_data)
{
    uint32_t cx;
    uint32_t cy;

    if (grid == NULL || grid->cell_head == NULL) {
        return 0u;
    }
    cx = clamp_cell(x, grid->min_x, grid->cell_width, grid->columns);
    cy = clamp_cell(y, grid->min_y, grid->cell_height, grid->rows);

    begin_query(grid);
    return query_cell_range(grid, cx, cy, cx, cy, visit, user_data);
}

uint32_t zcsr_spatial_grid_query_aabb(
    zcsr_spatial_grid* grid,
    float x,
    float y,
    float width,
    float height,
    zcsr_spatial_grid_visit_fn visit,
    void* user_data)
{
    uint32_t cx0;
    uint32_t cx1;
    uint32_t cy0;
    uint32_t cy1;

    if (grid == NULL || grid->cell_head == NULL) {
        return 0u;
    }
    if (width < 0.0f) {
        width = 0.0f;
    }
    if (height < 0.0f) {
        height = 0.0f;
    }

    cx0 = clamp_cell(x, grid->min_x, grid->cell_width, grid->columns);
    cy0 = clamp_cell(y, grid->min_y, grid->cell_height, grid->rows);
    cx1 = clamp_cell(x + width, grid->min_x, grid->cell_width, grid->columns);
    cy1 = clamp_cell(y + height, grid->min_y, grid->cell_height, grid->rows);

    begin_query(grid);
    return query_cell_range(grid, cx0, cy0, cx1, cy1, visit, user_data);
}
