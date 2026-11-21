#include "zcsr_game_two_dimensional.h"

#include <string.h>

static bool zcsr_game_two_dimensional_grid_is_usable(
    const zcsr_game_two_dimensional_grid* grid) {
    if (!grid) return false;
    if (grid->rows == 0u || grid->columns == 0u) return false;
    if (grid->rows > ZCSR_GAME_TWO_DIMENSIONAL_MAXIMUM_ROWS) return false;
    if (grid->columns > ZCSR_GAME_TWO_DIMENSIONAL_MAXIMUM_COLUMNS) return false;
    if (grid->cell_width <= 0.0f || grid->cell_height <= 0.0f) return false;
    return true;
}

void zcsr_game_two_dimensional_grid_initialize(
    zcsr_game_two_dimensional_grid* grid,
    uint32_t rows,
    uint32_t columns,
    float origin_x,
    float origin_y,
    float cell_width,
    float cell_height) {
    if (!grid) return;
    memset(grid, 0, sizeof *grid);
    if (rows > ZCSR_GAME_TWO_DIMENSIONAL_MAXIMUM_ROWS) {
        rows = ZCSR_GAME_TWO_DIMENSIONAL_MAXIMUM_ROWS;
    }
    if (columns > ZCSR_GAME_TWO_DIMENSIONAL_MAXIMUM_COLUMNS) {
        columns = ZCSR_GAME_TWO_DIMENSIONAL_MAXIMUM_COLUMNS;
    }
    grid->rows = rows;
    grid->columns = columns;
    grid->origin_x = origin_x;
    grid->origin_y = origin_y;
    grid->cell_width = cell_width;
    grid->cell_height = cell_height;
}

bool zcsr_game_two_dimensional_grid_bind_occupancy(
    zcsr_game_two_dimensional_grid* grid,
    uint32_t* occupied_kind,
    uint32_t occupied_stride,
    uint32_t empty_kind) {
    uint32_t row;
    uint32_t column;

    if (!grid || !occupied_kind) return false;
    if (grid->rows == 0u || grid->columns == 0u) return false;
    if (occupied_stride < grid->columns) return false;

    grid->occupied_kind = occupied_kind;
    grid->occupied_stride = occupied_stride;
    grid->empty_kind = empty_kind;
    for (row = 0u; row < grid->rows; ++row) {
        for (column = 0u; column < occupied_stride; ++column) {
            grid->occupied_kind[row * occupied_stride + column] = empty_kind;
        }
    }
    return true;
}

bool zcsr_game_two_dimensional_grid_is_inside(
    const zcsr_game_two_dimensional_grid* grid,
    uint32_t row,
    uint32_t column) {
    if (!grid) return false;
    return row < grid->rows && column < grid->columns;
}

bool zcsr_game_two_dimensional_grid_world_to_cell(
    const zcsr_game_two_dimensional_grid* grid,
    float x,
    float y,
    zcsr_game_two_dimensional_cell* out_cell) {
    uint32_t row;
    uint32_t column;

    if (!zcsr_game_two_dimensional_grid_is_usable(grid) || !out_cell) {
        return false;
    }
    if (x < grid->origin_x || y < grid->origin_y) return false;

    row = (uint32_t)((y - grid->origin_y) / grid->cell_height);
    column = (uint32_t)((x - grid->origin_x) / grid->cell_width);
    if (!zcsr_game_two_dimensional_grid_is_inside(grid, row, column)) {
        return false;
    }

    out_cell->row = row;
    out_cell->column = column;
    return true;
}

bool zcsr_game_two_dimensional_grid_cell_to_world_center(
    const zcsr_game_two_dimensional_grid* grid,
    uint32_t row,
    uint32_t column,
    zcsr_game_two_dimensional_point* out_point) {
    if (!zcsr_game_two_dimensional_grid_is_usable(grid) || !out_point) {
        return false;
    }
    if (!zcsr_game_two_dimensional_grid_is_inside(grid, row, column)) {
        return false;
    }

    out_point->x = grid->origin_x +
                   ((float)column + 0.5f) * grid->cell_width;
    out_point->y = grid->origin_y +
                   ((float)row + 0.5f) * grid->cell_height;
    return true;
}

uint32_t zcsr_game_two_dimensional_grid_occupied_kind_at(
    const zcsr_game_two_dimensional_grid* grid,
    uint32_t row,
    uint32_t column) {
    if (!zcsr_game_two_dimensional_grid_is_usable(grid) ||
        !grid->occupied_kind) {
        return grid ? grid->empty_kind : ZCSR_GAME_TWO_DIMENSIONAL_EMPTY_KIND;
    }
    if (!zcsr_game_two_dimensional_grid_is_inside(grid, row, column)) {
        return grid->empty_kind;
    }
    return grid->occupied_kind[row * grid->occupied_stride + column];
}

bool zcsr_game_two_dimensional_grid_try_occupy(
    zcsr_game_two_dimensional_grid* grid,
    uint32_t row,
    uint32_t column,
    uint32_t occupied_kind) {
    if (!zcsr_game_two_dimensional_grid_is_usable(grid) ||
        !grid->occupied_kind) {
        return false;
    }
    if (occupied_kind == grid->empty_kind) return false;
    if (!zcsr_game_two_dimensional_grid_is_inside(grid, row, column)) {
        return false;
    }
    if (grid->occupied_kind[row * grid->occupied_stride + column] !=
        grid->empty_kind) {
        return false;
    }
    grid->occupied_kind[row * grid->occupied_stride + column] = occupied_kind;
    return true;
}

bool zcsr_game_two_dimensional_grid_release(
    zcsr_game_two_dimensional_grid* grid,
    uint32_t row,
    uint32_t column) {
    if (!zcsr_game_two_dimensional_grid_is_usable(grid) ||
        !grid->occupied_kind) {
        return false;
    }
    if (!zcsr_game_two_dimensional_grid_is_inside(grid, row, column)) {
        return false;
    }
    if (grid->occupied_kind[row * grid->occupied_stride + column] ==
        grid->empty_kind) {
        return false;
    }
    grid->occupied_kind[row * grid->occupied_stride + column] =
        grid->empty_kind;
    return true;
}
